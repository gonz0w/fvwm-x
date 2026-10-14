#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
"""Desktop policy, persistent state, and external-input regression coverage."""

import datetime as dt
import importlib.util
import json
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
            self.assertFalse(list(root.glob('.desktop-*')))

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
                instance.settings.update(palette='dune', keep_wallpaper=True)
                instance.theme()
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


if __name__ == '__main__':
    unittest.main()
