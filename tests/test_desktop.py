#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
"""Desktop policy, persistent state, and external-input regression coverage."""

import datetime as dt
import importlib.util
import json
import fcntl
import os
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('desktop', ROOT / 'default-config/desktop.py')
desktop = importlib.util.module_from_spec(spec)
spec.loader.exec_module(desktop)


class DesktopPolicy(unittest.TestCase):
    def test_day_night_boundaries_and_midnight(self):
        settings = desktop.DEFAULTS.copy()
        for time, expected in [('06:59', 'dark'), ('07:00', 'light'),
                               ('18:59', 'light'), ('19:00', 'dark'), ('00:00', 'dark')]:
            now = dt.datetime.strptime(time, '%H:%M')
            self.assertEqual(desktop.appearance(settings, now), expected)
        settings.update(day='19:00', night='07:00')
        self.assertEqual(desktop.appearance(settings, dt.datetime(2026, 1, 1, 23)), 'light')
        self.assertEqual(desktop.appearance(settings, dt.datetime(2026, 1, 1, 8)), 'dark')
        settings['appearance'] = 'dark'
        self.assertEqual(desktop.appearance(settings, dt.datetime(2026, 1, 1, 23)), 'dark')

    def test_untrusted_state_is_data_only(self):
        settings = desktop.validate_settings({'palette':'slate; Exec evil', 'appearance':'broken',
                                              'day':'24:00', 'night':'-1', 'motion':'false',
                                              'transparency':False, 'extra':'ignored'})
        expected = dict(desktop.DEFAULTS, transparency=False)
        self.assertEqual(settings, expected)
        for value in ('7:00', '24:00', '10:60', '00:0', '20:00\nExec evil'):
            with self.assertRaises(ValueError):
                desktop.minute(value)
        settings = desktop.validate_settings({'day':'08:00', 'night':'08:00'})
        self.assertNotEqual(settings['day'], settings['night'])

    def test_atomic_state_is_private_and_preserves_other_files(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            local = root / 'local.config'
            local.write_text('user configuration')
            state = root / 'desktop-state.json'
            desktop.save_settings(state, dict(desktop.DEFAULTS, palette='grove'))
            self.assertEqual(json.loads(state.read_text())['palette'], 'grove')
            self.assertEqual(state.stat().st_mode & 0o777, 0o600)
            self.assertEqual(local.read_text(), 'user configuration')
            self.assertEqual([p.name for p in root.glob('.desktop-*')], ['.desktop-state.lock'])

    def test_menu_titles_cannot_execute_or_expand(self):
        value = 'a" $[w.id] %icon% &x\\\nExec evil'
        cleaned = desktop.clean_label(value)
        self.assertFalse(any(c in cleaned for c in '\\"$%&\n'))
        self.assertLessEqual(len(desktop.clean_label('x' * 200)), 100)

    def test_exec_field_codes_are_argv_not_shell(self):
        command = desktop.desktop_exec('app "hello world" %f %F %u %U %i %c %k %%',
                                       'A; touch /tmp/evil', '/tmp/a.desktop', 'an-icon')
        self.assertEqual(command, ['app', 'hello world', '--icon', 'an-icon',
                                  'A; touch /tmp/evil', '/tmp/a.desktop', '%'])
        for value in ('app --file=%f', 'app %Z', '%f'):
            with self.assertRaises(ValueError):
                desktop.desktop_exec(value, 'Name', '/tmp/a')

    def test_user_hidden_entry_masks_system_application(self):
        with tempfile.TemporaryDirectory() as directory:
            user, system = Path(directory) / 'user', Path(directory) / 'system'
            user.mkdir()
            system.mkdir()
            (system / 'app.desktop').write_text('[Desktop Entry]\nType=Application\nName=System\nExec=/bin/sh\n')
            (user / 'app.desktop').write_text('[Desktop Entry]\nType=Application\nHidden=true\n')
            (system / 'safe.desktop').write_text('[Desktop Entry]\nType=Application\nName=Quoted;Name\nExec=/bin/sh "literal; argument"\n')
            (system / 'bad.desktop').write_text('[Desktop Entry]\nType=Application\nName=Bad\nExec=/bin/sh %Z\n')
            apps = desktop.discover_apps([user, system])
            self.assertEqual(len(apps), 1)
            self.assertEqual(apps[0]['argv'], ['/bin/sh', 'literal; argument'])

    def test_palettes_have_readable_text_and_selection(self):
        palettes = json.loads((ROOT / 'default-config/palettes.json').read_text())
        def luminance(color):
            channels = [int(color[i:i + 2], 16) / 255 for i in (1, 3, 5)]
            linear = [c / 12.92 if c <= .04045 else ((c + .055) / 1.055) ** 2.4 for c in channels]
            return sum(c * w for c, w in zip(linear, (.2126, .7152, .0722)))
        def contrast(first, second):
            a, b = sorted((luminance(first), luminance(second)))
            return (b + .05) / (a + .05)
        self.assertEqual(set(palettes), set(desktop.PALETTE_NAMES))
        for variants in palettes.values():
            self.assertEqual(set(variants), {'light', 'dark'})
            for p in variants.values():
                self.assertGreaterEqual(contrast(p['text'], p['surface']), 4.5)
                self.assertGreaterEqual(contrast(p['muted'], p['surface']), 4.5)
                self.assertGreaterEqual(contrast(p['on_accent'], p['accent']), 4.5)

    def test_bsd_native_counter_formats(self):
        for system, counters in (('FreeBSD', '38 0 80 1 11386'),
                                  ('OpenBSD', '180,0,147,13,6,3954')):
            values = {'kern.cp_time':counters, 'hw.physmem':'1073741824',
                      'hw.pagesize':'4096', 'vm.stats.vm.v_free_count':'1024',
                      'hw.acpi.battery.life':'75'}
            def output(argv):
                if argv[0] == 'sysctl':
                    return values.get(argv[-1], '')
                if argv[0] == 'apm':
                    return '3' if argv[-1] == '-b' else '75'
                return {'vmstat':'  1024 pages free',
                        'ifconfig':'em0: flags=UP\n\tstatus: active\n'}.get(argv[0], '')
            with patch.object(desktop, 'command_output', side_effect=output):
                status = desktop.collect_status(system)
            self.assertEqual(status['cpu'], (sum(map(int, counters.replace(',', ' ').split())),
                                             int(counters.replace(',', ' ').split()[-1])))
            self.assertEqual(status['memory'], 1073741824 - 1024 * 4096)
            self.assertEqual(status['battery'], 75)
            self.assertEqual(status['network'], 'em0')
        with patch.object(desktop, 'command_output', side_effect=lambda argv:
                          '4' if argv == ['apm', '-b'] else '0'):
            self.assertIsNone(desktop.collect_status('OpenBSD')['battery'])

    def test_wallpaper_retention_does_not_replace_saved_surface(self):
        commands = []
        with tempfile.TemporaryDirectory() as directory:
            instance = desktop.Desktop(commands.append, ROOT / 'default-config',
                                       Path(directory) / 'state.json')
            try:
                instance.theme()
                commands.clear()
                instance.change_settings(dict(palette='dune', keep_wallpaper=True))
                self.assertFalse(any(c.startswith('Colorset 20') for c in commands))
                self.assertTrue(any('#d0a17e' in c or '#87583e' in c for c in commands))
                retained = instance.settings['wallpaper'].copy()
            finally:
                instance.pool.shutdown()
            restarted = desktop.Desktop(commands.append, ROOT / 'default-config',
                                        Path(directory) / 'state.json')
            try:
                commands.clear()
                restarted.theme()
                self.assertEqual(restarted.settings['wallpaper'], retained)
                self.assertTrue(any('slate-' in c for c in commands if 'AspectPixmap' in c))
            finally:
                restarted.pool.shutdown()


class SettingsRegistry(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.path = Path(self.directory.name) / 'state.json'
        self.commands = []
        self.instance = desktop.Desktop(self.commands.append, ROOT / 'default-config', self.path)

    def tearDown(self):
        self.instance.pool.shutdown()
        self.directory.cleanup()

    def test_registry_drives_types_defaults_and_scopes(self):
        for key, item in desktop.SETTINGS.items():
            self.assertEqual(item['default'], desktop.DEFAULTS[key])
            self.assertEqual(desktop.setting_value(key, item['default']), item['default'])
            for bad in (None, [], {}, 1.5):
                with self.assertRaises(ValueError):
                    desktop.setting_value(key, bad)
        for value in ('00:00', '23:59'):
            self.assertEqual(desktop.setting_value('day', value), value)
        for value in ('24:00', '00:60', '7:00', '12:01\n'):
            with self.assertRaises(ValueError):
                desktop.setting_value('day', value)
        for value in (0, 1, 'false'):
            with self.assertRaises(ValueError):
                desktop.setting_value('motion', value)
        self.assertEqual(desktop.setting_scope('appearance'), ['appearance'])
        self.assertEqual(set(desktop.setting_scope('group:schedule')), {'day', 'night'})
        for scope in ('wallpaper', 'missing', '', 'group:missing'):
            with self.assertRaises(ValueError):
                desktop.setting_scope(scope)

    def test_change_reset_reload_and_legacy_migration(self):
        self.path.write_text(json.dumps(dict(desktop.DEFAULTS, palette='ink')))
        self.path.chmod(0o600)
        self.instance.reload_settings()
        self.instance.change_settings({'motion': True, 'day': '08:00', 'night': '20:00'})
        self.assertEqual(json.loads(self.path.read_text())['schema_version'], 1)
        self.instance.reset_settings('group:schedule')
        self.assertEqual(self.instance.saved['day'], '07:00')
        self.assertEqual(self.instance.saved['night'], '19:00')
        self.assertTrue(self.instance.saved['motion'])
        self.assertEqual(self.instance.saved['palette'], 'ink')
        self.instance.reset_settings('motion')
        self.assertFalse(self.instance.saved['motion'])
        self.assertEqual(desktop.load_settings(self.path)[0], self.instance.saved)

    def test_failed_write_preserves_memory_bytes_and_commands(self):
        self.instance.change_settings({'palette': 'grove'})
        previous = self.path.read_bytes(), self.instance.saved.copy(), self.instance.settings.copy()
        self.commands.clear()
        with patch.object(desktop.os, 'replace', side_effect=OSError('injected replacement failure')):
            with self.assertRaises(OSError):
                self.instance.change_settings({'palette': 'ink'})
        self.assertEqual((self.path.read_bytes(), self.instance.saved, self.instance.settings), previous)
        self.assertEqual(self.commands, [])
        self.assertEqual([p.name for p in self.path.parent.glob('.desktop-*')], ['.desktop-state.lock'])
        self.instance.change_settings({'palette': 'ink'})
        self.assertEqual(self.instance.saved['palette'], 'ink')

    def test_fsync_failure_is_before_commit(self):
        with patch.object(desktop.os, 'fsync', side_effect=OSError('injected sync failure')):
            with self.assertRaises(OSError):
                self.instance.change_settings({'palette': 'ink'})
        self.assertFalse(self.path.exists())
        self.assertEqual(self.instance.saved['palette'], 'slate')
        self.assertEqual(self.commands, [])

    def test_stale_writer_must_reload_and_busy_writer_never_waits(self):
        self.instance.change_settings({'palette': 'grove'})
        desktop.save_settings(self.path, dict(self.instance.saved, palette='dune'))
        with self.assertRaisesRegex(ValueError, 'externally'):
            self.instance.change_settings({'palette': 'ink'})
        self.assertEqual(self.instance.saved['palette'], 'grove')
        self.instance.reload_settings()
        self.instance.change_settings({'palette': 'ink'})
        with (self.path.parent / '.desktop-state.lock').open('r+') as lock:
            fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
            with self.assertRaises(BlockingIOError):
                self.instance.change_settings({'palette': 'grove'})
        self.assertEqual(self.instance.saved['palette'], 'ink')

    def test_file_and_parent_policy_refuse_unsafe_paths_without_blocking(self):
        target = self.path.parent / 'other'
        target.write_text('keep')
        self.path.symlink_to(target)
        with self.assertRaises(OSError):
            desktop.save_settings(self.path, desktop.DEFAULTS)
        self.assertEqual(target.read_text(), 'keep')
        self.path.unlink()
        os.mkfifo(self.path, 0o600)
        with self.assertRaises(ValueError):
            desktop.load_settings(self.path)
        self.path.unlink()
        desktop.save_settings(self.path, desktop.DEFAULTS)
        self.path.chmod(0o644)
        with self.assertRaises(ValueError):
            desktop.load_settings(self.path)
        self.path.chmod(0o600)
        os.link(self.path, target.parent / 'alias')
        with self.assertRaises(ValueError):
            desktop.load_settings(self.path)
        self.path.unlink()
        self.path.parent.chmod(0o777)
        try:
            with self.assertRaises(ValueError):
                desktop.save_settings(self.path, desktop.DEFAULTS)
        finally:
            self.path.parent.chmod(0o700)

    def test_bounded_versioned_data_and_duplicate_refusal(self):
        self.path.touch(mode=0o600)
        for data in ('x' * 8193, '{"palette":"ink","palette":"dune"}', '[]',
                     '{"schema_version":2}', '{"schema_version":true}',
                     '{"schema_version":1}', '[' * 2000):
            self.path.write_text(data)
            with self.assertRaises(ValueError):
                desktop.load_settings(self.path)
            self.assertEqual(self.path.read_text(), data)

    def test_overrides_are_separate_and_not_overwritten_by_reset(self):
        for line in ('*FvwmDesktop: palette grove', '*FvwmDesktoppalette grove',
                     '*fvwmdesktopPalette grove'):
            self.assertEqual(desktop.desktop_configuration(line), ('palette', 'grove'))
        self.assertIsNone(desktop.desktop_configuration('*Other: palette grove'))
        self.instance.configure_setting('palette', 'grove')
        self.instance.finish_configuration()
        self.instance.change_settings({'palette': 'ink'})
        item = self.instance.query_settings('palette')['palette']
        self.assertEqual((item['saved'], item['effective'], item['source']),
                         ('ink', 'grove', 'module-configuration'))
        self.assertFalse(item['preview'])
        self.instance.reset_settings('palette')
        self.assertEqual(self.instance.saved['palette'], 'slate')
        self.assertEqual(self.instance.settings['palette'], 'grove')
        self.assertEqual(json.loads(self.path.read_text())['palette'], 'slate')

    def test_invalid_changes_refuse_without_effects(self):
        for changes in ({'day': '19:00'}, {'unknown': 'ink'}, {'palette': 'ink; Exec evil'},
                        {'motion': 1}, {'wallpaper': {'palette': 'ink', 'appearance': 'dark'}}):
            with self.assertRaises(ValueError):
                self.instance.change_settings(changes)
        self.assertFalse(self.path.exists())
        self.assertEqual(self.commands, [])

    def test_native_consumer_and_query_are_registry_driven(self):
        self.instance.theme()
        self.commands.clear()
        self.instance.handle('query-settings test_1 all')
        self.assertEqual(self.commands.pop(), 'Echo desktop-settings-test_1 done')
        query = {line.split(' ', 3)[2]: json.loads(line.split(' ', 3)[3])
                 for line in self.commands}
        self.assertEqual(set(query), set(desktop.SETTINGS))
        self.assertTrue(all(len(line.encode()) < 1000 for line in self.commands))
        self.commands.clear()
        self.instance.settings_menu('group:effects')
        commands = '\n'.join(self.commands)
        self.assertIn('reset group:effects', commands)
        self.assertIn('set motion true', commands)
        self.assertNotIn('SlateSetting_palette', commands)
        self.assertIn('AddToMenu SlateSetting_motion Greyed "Saved: false"', commands)
        self.assertNotIn('" Greyed', commands)

    def test_invalid_startup_data_is_preserved_until_repaired_reload(self):
        self.instance.pool.shutdown()
        self.path.write_text('{"schema_version":99}')
        self.path.chmod(0o600)
        self.instance = desktop.Desktop(self.commands.append, ROOT / 'default-config', self.path)
        self.instance.theme()
        with self.assertRaises(ValueError):
            self.instance.reset_settings('all')
        self.assertEqual(json.loads(self.path.read_text())['schema_version'], 99)
        self.path.unlink()
        self.instance.reload_settings()
        self.instance.change_settings({'palette': 'ink'})
        self.assertEqual(self.instance.saved['palette'], 'ink')

    def editor_values(self, **changes):
        editor = self.instance.settings_editor
        values = dict(self.instance.saved, **changes)
        return editor['token'] + ' ' + ' '.join(str(values[key]).lower() for key in editor['keys'])

    def test_editor_applies_several_fields_with_one_publication(self):
        self.instance.edit_settings()
        generation = self.instance.settings_generation
        with patch.object(desktop, 'save_settings', wraps=desktop.save_settings) as save:
            self.instance.apply_settings_editor(self.editor_values(
                palette='grove', motion=True, day='08:00', night='21:00'))
        self.assertEqual(save.call_count, 1)
        self.assertEqual(self.instance.saved['palette'], 'grove')
        self.assertEqual(self.instance.saved['night'], '21:00')
        self.assertTrue(self.instance.saved['motion'])
        self.assertIsNone(self.instance.settings_editor)
        self.assertNotEqual(generation, self.instance.settings_generation)
        self.assertEqual(desktop.load_settings(self.path)[0], self.instance.saved)

    def test_editor_bad_group_or_failed_write_preserves_draft_and_state(self):
        self.instance.change_settings({'palette': 'dune'})
        self.instance.edit_settings('group:schedule')
        editor = self.instance.settings_editor.copy()
        before = self.path.read_bytes(), self.instance.saved.copy(), self.instance.settings.copy()
        self.commands.clear()
        with self.assertRaises(ValueError):
            self.instance.apply_settings_editor(self.editor_values(day='08:00', night='08:00'))
        with patch.object(desktop.os, 'replace', side_effect=OSError('injected failure')):
            with self.assertRaises(OSError):
                self.instance.apply_settings_editor(self.editor_values(day='08:00', night='21:00'))
        self.assertEqual((self.path.read_bytes(), self.instance.saved, self.instance.settings), before)
        self.assertEqual(self.instance.settings_editor, editor)
        self.assertEqual(self.commands, [])
        self.instance.apply_settings_editor(self.editor_values(day='08:00', night='21:00'))
        self.assertEqual(self.instance.saved['palette'], 'dune')

    def test_editor_defaults_are_scoped_unsaved_and_cancelable(self):
        self.instance.change_settings({'palette': 'grove', 'motion': True})
        self.instance.edit_settings('group:effects')
        token = self.instance.settings_editor['token']
        before = self.path.read_bytes()
        self.commands.clear()
        self.instance.handle('defaults-settings ' + token)
        self.assertEqual(self.instance.settings_editor['keys'], ['transparency', 'motion'])
        self.assertNotEqual(self.instance.settings_editor['token'], token)
        self.assertIn('*SlateSettingsForm: Choice motion_false false on "Off"', self.commands)
        self.assertFalse(any('Choice palette_' in line for line in self.commands))
        self.assertEqual(self.path.read_bytes(), before)
        self.instance.handle('cancel-settings ' + self.instance.settings_editor['token'])
        self.assertIsNone(self.instance.settings_editor)
        self.assertEqual(self.path.read_bytes(), before)

    def test_editor_replacement_change_reload_and_restart_reject_stale_callbacks(self):
        self.instance.edit_settings()
        first = self.editor_values(palette='ink')
        self.instance.edit_settings()
        with self.assertRaisesRegex(ValueError, 'stale'):
            self.instance.apply_settings_editor(first)
        second = self.editor_values(palette='ink')
        self.instance.change_settings({'palette': 'grove'})
        with self.assertRaisesRegex(ValueError, 'stale'):
            self.instance.apply_settings_editor(second)
        self.instance.edit_settings()
        third = self.editor_values(palette='ink')
        self.instance.reload_settings()
        with self.assertRaisesRegex(ValueError, 'stale'):
            self.instance.apply_settings_editor(third)
        self.instance.edit_settings()
        fourth = self.editor_values(palette='ink')
        self.instance.pool.shutdown()
        self.instance = desktop.Desktop(self.commands.append, ROOT / 'default-config', self.path)
        with self.assertRaisesRegex(ValueError, 'stale'):
            self.instance.apply_settings_editor(fourth)
        self.assertEqual(self.instance.saved['palette'], 'grove')

    def test_editor_external_write_cannot_be_overwritten(self):
        self.instance.change_settings({'palette': 'slate'})
        self.instance.edit_settings()
        argument = self.editor_values(palette='ink')
        desktop.save_settings(self.path, dict(self.instance.saved, palette='dune'))
        with self.assertRaisesRegex(ValueError, 'externally'):
            self.instance.apply_settings_editor(argument)
        self.assertEqual(desktop.load_settings(self.path)[0]['palette'], 'dune')
        self.assertEqual(self.instance.settings['palette'], 'slate')
        self.assertIsNotNone(self.instance.settings_editor)

    def test_editor_deadline_and_unsupported_scopes(self):
        for scope in ('unknown', 'wallpaper', 'group:missing'):
            with self.assertRaises(ValueError):
                self.instance.edit_settings(scope)
        with patch.object(desktop.time, 'monotonic', return_value=100):
            self.instance.edit_settings()
        argument = self.editor_values(palette='ink')
        with patch.object(desktop.time, 'monotonic', return_value=100 + desktop.EDITOR_SECONDS):
            with self.assertRaisesRegex(ValueError, 'expired'):
                self.instance.apply_settings_editor(argument)
        self.assertIsNone(self.instance.settings_editor)
        self.assertFalse(self.path.exists())
        with patch.object(desktop.time, 'monotonic', return_value=100):
            self.instance.edit_settings()
        with patch.object(desktop.time, 'monotonic', return_value=100 + desktop.EDITOR_SECONDS):
            self.instance.tick()
        self.assertIsNone(self.instance.settings_editor)
        self.assertFalse(self.path.exists())

    def test_editor_override_is_visible_without_changing_personal_configuration(self):
        self.instance.configure_setting('palette', 'grove')
        self.instance.finish_configuration()
        self.instance.edit_settings('palette')
        self.assertIn('*SlateSettingsForm: Text "Effective: grove (module configuration override)"',
                      self.commands)
        self.instance.apply_settings_editor(self.editor_values(palette='ink'))
        self.assertEqual(self.instance.saved['palette'], 'ink')
        self.assertEqual(self.instance.settings['palette'], 'grove')
        self.assertEqual(self.instance.overrides, {'palette': 'grove'})

    def test_editor_values_are_complete_and_queries_expose_snapshot_identity(self):
        self.instance.edit_settings()
        token = self.instance.settings_editor['token']
        for argument in (token, token + ' ink', self.editor_values() + ' extra',
                         self.editor_values(palette='$(Exec evil)')):
            with self.assertRaises(ValueError):
                self.instance.apply_settings_editor(argument)
        self.assertFalse(self.path.exists())
        item = self.instance.query_settings('palette')['palette']
        self.assertEqual(item['generation'], self.instance.settings_generation)
        self.assertFalse(item['preview'])


if __name__ == '__main__':
    unittest.main()
