#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or any later version.
"""Exercise the coordinated stdlib XDG utility through its installed CLI.

Predecessor observations remain in the dated XDG port verification receipts.
This gate checks normative behavior and records intentional improvements without replacing them.
"""
import argparse
import base64
import json
import os
import re
from pathlib import Path
import subprocess
import tempfile
import unittest
from xml.sax.saxutils import escape

PROVIDER = None
FORM = '*FvwmForm-XDGMenu-ConfigDefault: '


class DesktopTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix='fvwm-xdg-cli-')
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.user = self.root / 'user'
        self.apps = self.root / 'data/applications'
        self.menus = self.root / 'config/menus'
        for directory in (self.user, self.apps, self.menus):
            directory.mkdir(parents=True, mode=0o700)
        self.env = dict(os.environ, HOME=str(self.root), FVWM_USERDIR=str(self.user),
                        XDG_CONFIG_HOME=str(self.menus.parent),
                        XDG_CONFIG_DIRS=str(self.root/'absent-config'),
                        XDG_DATA_HOME=str(self.apps.parent),
                        XDG_DATA_DIRS=str(self.root/'absent-data'),
                        XDG_MENU_PREFIX='', LC_ALL='C', LANGUAGE='')
        self.env.pop('FVWM_XDG_MENU_TIMEOUT', None)
        self.desktop('a', 'Alpha & 100%', 'fixture-app "two words" %%')
        self.menu = self.write_menu('applications', '<DefaultAppDirs/><Include><All/></Include>')

    def desktop(self, name, title, command='fixture-app', extra=''):
        (self.apps/(name+'.desktop')).write_text('[Desktop Entry]\nType=Application\nName='+title+'\nExec='+command+'\n'+extra)

    def write_menu(self, name, body):
        path = self.menus/(name+'.menu')
        path.write_text('<Menu><Name>Applications</Name>'+body+'</Menu>')
        return path

    def run_cli(self, *args, status=0, env=None):
        completed = subprocess.run([str(PROVIDER), *args], env=env or self.env,
                                   capture_output=True, timeout=30)
        self.assertEqual(completed.returncode, status, completed.stderr.decode(errors='replace'))
        return completed.stdout.decode(), completed.stderr.decode()

    def records(self):
        result = []
        for path in self.user.glob('.XDGMenu-actions-*'):
            if path.suffix != '.lock':
                self.assertEqual(path.stat().st_mode & 0o777, 0o600)
                result.extend(json.loads(path.read_text())['Records'].values())
        return result

    def test_discovery_lists_without_publication(self):
        out, _ = self.run_cli('--get-menus', 'all')
        self.assertEqual(out, str(self.menu)+'\n')
        self.assertEqual(list(self.user.iterdir()), [])

    def test_literal_application_arguments(self):
        out, _ = self.run_cli('--include-items', 'none')
        self.assertIn('+ "Alpha && 100%%" Exec exec fvwm-menu-desktop --launch ', out)
        self.assertEqual(self.records()[0]['Argv'], ['fixture-app', 'two words', '%'])
        self.assertNotIn('fixture-app "two words"', out)

    def test_short_options_and_toggle_order(self):
        out, _ = self.run_cli('-wvtCustom', '-s32', '--without-titles', '--include-items', 'none')
        self.assertIn('AddToMenu "Custom"', out)
        self.assertNotIn(' Title\n', out)
        self.run_cli('--enable-style', '--xterm', 'ignored', '--include-items', 'none')

    def test_visibility_after_allocation(self):
        self.desktop('hidden', 'Hidden', extra='Hidden=true\n')
        self.desktop('only', 'Only', extra='OnlyShowIn=FVWM;\n')
        self.desktop('not', 'Not shown', extra='NotShowIn=FVWM;\n')
        self.desktop('missing', 'No program', extra='TryExec=missing-owned-fixture\n')
        out, _ = self.run_cli('--include-items', 'none')
        self.assertIn('+ "Only"', out)
        for label in ('Hidden', 'Not shown', 'No program'):
            self.assertNotIn('+ "'+label+'"', out)
        self.assertEqual(len(self.records()), 2)

    def test_merge_move_delete_and_inline(self):
        self.desktop('b', 'Beta')
        merged = self.write_menu('merged', '<DefaultAppDirs/><Menu><Name>Old</Name><Include><Filename>b.desktop</Filename></Include></Menu>')
        self.menu.write_text('<Menu><Name>Applications</Name><MergeFile>'+escape(str(merged))+'</MergeFile><Move><Old>Old</Old><New>New</New></Move><Menu><Name>Deleted</Name><Deleted/></Menu><Layout><Menuname inline="true" inline_header="false">New</Menuname></Layout></Menu>')
        out, _ = self.run_cli('--set-menus', str(self.menu), '--include-items', 'none')
        self.assertIn('+ "Beta"', out)
        self.assertNotIn('Popup "New"', out)
        self.assertNotIn('Deleted', out)
        self.assertEqual(self.records()[0]['Argv'], ['fixture-app'])

    def test_user_appdir_override_and_hidden_mask(self):
        override = self.root/'overrides'
        override.mkdir()
        (override/'a.desktop').write_text('[Desktop Entry]\nHidden=true\n')
        self.menu.write_text('<Menu><Name>Applications</Name><DefaultAppDirs/><AppDir>'+escape(str(override))+'</AppDir><Include><All/></Include></Menu>')
        out, _ = self.run_cli('--set-menus', str(self.menu), '--include-items', 'none')
        self.assertNotIn('Alpha', out)
        self.assertEqual(self.records(), [])

    def test_empty_menu_and_insert_dynamic(self):
        empty = self.write_menu('empty', '')
        out, _ = self.run_cli('--set-menus', str(empty), '--include-items', 'none')
        self.assertIn('AddToMenu "XDGMenu"', out)
        out, _ = self.run_cli('--set-menus', str(self.menu), '--insert-in-menu', 'Root', '--dynamic')
        self.assertNotIn('DestroyMenu "Root"', out)
        self.assertIn('AddToMenu "Root"', out)

    def test_settings_data_and_private_output(self):
        pref = self.user/'.FvwmForm-XDGMenu-Config'
        pref.write_text(FORM+'FutureField keep\n'+FORM+'Title Before\n')
        fields = [('Title', 'Saved "quoted" $[literal]'), ('Path', str(self.user/'export')), ('IncludeNone', 'on')]
        args = ['--form-action', 'save']
        for key, value in fields:
            encode = lambda text: base64.urlsafe_b64encode(text.encode()).decode().rstrip('=')
            args += ['--field', encode(key)+'='+encode(value)]
        self.run_cli(*args)
        self.assertEqual(pref.stat().st_mode & 0o777, 0o600)
        self.assertIn('FutureField keep', pref.read_text())
        out, _ = self.run_cli('--include-items', 'none')
        self.assertIn('Saved \\"quoted\\" $$[literal]', out)
        self.run_cli(*[arg if arg!='save' else 'export' for arg in args])
        self.assertEqual((self.user/'export').stat().st_mode & 0o777, 0o600)
        self.assertIn('AddToMenu', (self.user/'export').read_text())

    def test_native_form_uses_encoded_local_arguments(self):
        out, _ = self.run_cli('--configure')
        self.assertIn('Module FvwmForm FvwmForm-XDGMenu-Config', out)
        self.assertIn('Choice MENapplications on on', out)
        self.assertIn('$(Title:Base64)', out)
        self.assertIn('$(FormJob:Token)', out)
        self.assertIn('CommandAppend', out)
        self.assertNotIn('UseData', out)
        self.assertNotIn('$(Title)', out)
        for line in out.splitlines():
            self.assertLess(len(line.encode()), 1000)

    def test_completed_result_consumed_once(self):
        token = '1'*48
        self.run_cli('--form-action', 'save', '--result-token', token)
        self.assertTrue((self.user/('.XDGMenu-result-'+token)).exists())
        out, _ = self.run_cli('--form-result', token)
        self.assertEqual(out, 'Nop\n')
        self.assertFalse((self.user/('.XDGMenu-result-'+token)).exists())
        self.run_cli('--form-result', token, status=1)

    def test_previous_actions_survive_regeneration(self):
        self.run_cli('--include-items', 'none')
        self.desktop('a', 'Replaced', 'new-fixture-app')
        self.run_cli('--include-items', 'none')
        self.assertEqual({r['Argv'][0] for r in self.records()}, {'fixture-app', 'new-fixture-app'})

    def test_failed_settings_keep_previous_file(self):
        pref=self.user/'.FvwmForm-XDGMenu-Config'
        pref.write_text(FORM+'Title Kept\n')
        encode=lambda s:base64.urlsafe_b64encode(s.encode()).decode().rstrip('=')
        self.run_cli('--form-action','save','--field',encode('Title')+'='+encode('two\nlines'),status=1)
        self.assertEqual(pref.read_text(),FORM+'Title Kept\n')

    def test_error_menu_and_regular_source_refusal(self):
        self.menu.unlink()
        out, _ = self.run_cli('--menu-error', '--title', 'ErrorMenu', status=1)
        self.assertIn('DestroyMenu "ErrorMenu"', out)
        self.assertIn('Desktop menu generation failed', out)
        os.mkfifo(self.menu)
        out, _ = self.run_cli('--set-menus', str(self.menu), status=1)
        self.assertEqual(out, '')

    def test_help_avoids_saved_data_and_deadline_is_finite(self):
        (self.user/'.FvwmForm-XDGMenu-Config').write_bytes(b'\0')
        out, _ = self.run_cli('-h')
        self.assertIn('Usage:', out)
        self.run_cli('--timeout', '0s', status=2)
        self.run_cli('--timeout', '6m', status=2)

    def test_core_read_limit_matches_producer(self):
        source=Path(__file__).resolve().parents[1]
        native=int(re.search(r'^#define MAX_READ_ITEMS\s+(\d+)',(source/'libs/defaults.h').read_text(),re.M)[1])
        go=int(re.search(r'^const maxReadCommands = (\d+)',(source/'bin/FvwmPrompt/cmd/fvwmmenudesktop/main.go').read_text(),re.M)[1])
        self.assertEqual(native,go)


if __name__ == '__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--provider',type=Path,required=True)
    args=parser.parse_args()
    PROVIDER=args.provider.resolve()
    result=unittest.TextTestRunner(verbosity=2).run(unittest.defaultTestLoader.loadTestsFromTestCase(DesktopTests))
    raise SystemExit(0 if result.wasSuccessful() else 1)
