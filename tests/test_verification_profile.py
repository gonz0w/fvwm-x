#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
"""Check actual verification entry-point arguments without a system install."""
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class VerificationProfileTests(unittest.TestCase):
    def check_profile(self, platform):
        with tempfile.TemporaryDirectory(prefix='fvwm-profile-') as directory:
            root = Path(directory)
            commands = root / 'commands'
            commands.mkdir()
            fake = commands / 'driver'
            fake.write_text('#!'+sys.executable+'\n'+'''
import json, os, pathlib, sys
name=pathlib.Path(sys.argv[0]).name
args=sys.argv[1:]
if name=='uname':
    print('8.0' if args == ['-r'] else os.environ['PROFILE_PLATFORM'])
if name=='sysctl': print('OpenBSD 8.0 (GENERIC.MP) #1')
if name=='meson':
    with open(os.environ['PROFILE_LOG'], 'a') as log:
        log.write(json.dumps({'args':args, 'pkg_config_path':os.environ.get('PKG_CONFIG_PATH')})+'\\n')
    if args[0]=='setup':
        path=pathlib.Path(args[1])/'meson-private';path.mkdir(parents=True, exist_ok=True)
        (path/'coredata.dat').touch()
    if args[0]=='install':
        path=pathlib.Path(args[args.index('--destdir')+1])/'usr/local'
        (path/'bin').mkdir(parents=True, exist_ok=True)
        program=path/'bin/fvwm3';program.write_text('#!/bin/sh\\nexit 0\\n');program.chmod(0o755)
        (path/'share/xsessions').mkdir(parents=True, exist_ok=True)
        (path/'share/xsessions/fvwm3.desktop').touch()
''')
            fake.chmod(0o755)
            for name in ['uname', 'meson', 'cc', 'readelf', 'file', 'python3', 'sysctl']:
                (commands / name).symlink_to(fake.name)
            # BSD prepends its standard program directories. Invoke the copied
            # entry point with those directories absent from this fixture's
            # prefix so the recording Meson is selected on every host.
            script = ROOT / 'tests' / ('run_on_linux.sh' if platform == 'Linux'
                                      else 'run_on_bsd.sh')
            text = script.read_text().replace(
                'PATH="/usr/local/bin:/usr/X11R6/bin:$PATH"', 'PATH="$PATH"')
            local_script = root / 'entry.sh'
            local_script.write_text(text)
            # The public wrappers delegate beside themselves. Keep that real
            # helper with the copied entry point rather than testing a missing
            # file or replacing the shared setup implementation.
            (root / 'run_native_gate.sh').write_bytes(
                (ROOT / 'tests/run_native_gate.sh').read_bytes())
            env = os.environ.copy()
            env.update(PATH=str(commands)+':'+os.environ['PATH'],
                       PROFILE_PLATFORM=platform, PROFILE_LOG=str(root/'calls'))
            env.pop('FVWM_PNG_SECURITY_BACKPORT', None)
            env.pop('CC', None)
            for attempt in range(2):
                env['PKG_CONFIG_PATH'] = '/verified-prefix/pkgconfig'
                if attempt:
                    env['FVWM_PNG_SECURITY_BACKPORT'] = 'true'
                subprocess.run(['sh', str(local_script), str(root/'build')],
                               env=env, check=True, timeout=15,
                               stdout=subprocess.DEVNULL)
            calls = [json.loads(line) for line in (root/'calls').read_text().splitlines()]
            setup = [row for row in calls if row['args'][0] == 'setup']
            self.assertEqual(len(setup), 2)
            for index, row in enumerate(setup):
                args = row['args']
                self.assertIn('-Dpng-security-backport='+str(bool(index)).lower(), args)
                path = row['pkg_config_path']
                self.assertTrue(path.startswith('/verified-prefix/pkgconfig'))
                self.assertIn('-Dpkg_config_path='+path, args)
            self.assertIn('--reconfigure', setup[1]['args'])
            self.assertIn('--clearcache', setup[1]['args'])

    def test_linux(self):
        self.check_profile('Linux')

    def test_bsd(self):
        self.check_profile('OpenBSD')


if __name__ == '__main__':
    unittest.main()
