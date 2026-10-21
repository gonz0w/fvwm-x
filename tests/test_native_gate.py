#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
"""Check native gate invocation, staging, profile and failure contracts."""
import json
import os
from pathlib import Path
import subprocess
import shutil
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class NativeGateTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="fvwm-native-gate-")
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.bin = self.root / "bin"
        self.bin.mkdir()
        self.log = self.root / "calls.jsonl"
        self.build = self.root / "build with spaces"
        self.env = dict(os.environ, PATH=str(self.bin)+os.pathsep+os.environ['PATH'],
            NATIVE_GATE_TEST_LOG=str(self.log), NATIVE_GATE_TEST_OS="Linux",
            PKG_CONFIG_PATH="/private/test-libs", FVWM_PNG_SECURITY_BACKPORT="true")
        stub = "#!"+sys.executable+"\n"+'''
import json, os, pathlib, sys
name = pathlib.Path(sys.argv[0]).name
args = sys.argv[1:]
with open(os.environ['NATIVE_GATE_TEST_LOG'], 'a') as out:
    out.write(json.dumps([name, args])+"\\n")
if name == 'go':
    with open(os.environ['NATIVE_GATE_TEST_LOG'], 'a') as out:
        out.write(json.dumps(['offline-env', [os.environ.get(key) for key in
            ('GOTOOLCHAIN', 'GOPROXY', 'GOSUMDB')]])+"\\n")
if name == 'uname':
    print(os.environ.get('NATIVE_GATE_TEST_RELEASE', '8.0') if args == ['-r']
          else os.environ['NATIVE_GATE_TEST_OS'])
if name == 'sysctl':
    print(os.environ.get('NATIVE_GATE_TEST_KERNEL',
                         'OpenBSD 8.0 (GENERIC.MP) #1'))
if name == 'python3':
    if args == ['--version']:
        print('Python contract stub')
    sys.exit(0)
if name == 'meson':
    if args and args[0] == os.environ.get('NATIVE_GATE_TEST_FAIL'):
        sys.exit(23)
    if args and args[0] == 'setup':
        pathlib.Path(args[1]).mkdir(parents=True, exist_ok=True)
    if args and args[0] == 'install':
        stage = pathlib.Path(args[args.index('--destdir')+1]) / 'usr/local'
        program = stage / 'bin/fvwm3'
        program.parent.mkdir(parents=True)
        program.write_text('#!/bin/sh\\nexit 0\\n')
        program.chmod(0o755)
        session = stage / 'share/xsessions/fvwm3.desktop'
        session.parent.mkdir(parents=True)
        session.touch()
        for client in ('FvwmPrompt', 'FvwmCommand', 'fvwm-menu-xlock', 'fvwm-menu-directory', 'fvwm-convert-2.6', 'fvwm-menu-desktop'):
            exe = stage / 'bin' / client
            exe.write_text('#!/bin/sh\\nexit 0\\n')
            exe.chmod(0o755)
'''
        for name in ('uname', 'meson', 'cc', 'readelf', 'file', 'go',
                     'python3', 'sysctl'):
            path = self.bin / name
            path.write_text(stub)
            path.chmod(0o755)

    def run_gate(self, entry='run_native_gate.sh', flags=(), succeeds=True):
        result = subprocess.run(['sh', str(ROOT / 'tests' / entry),
            *flags, str(self.build)], env=self.env, cwd=ROOT, text=True,
            capture_output=True)
        if succeeds:
            self.assertEqual(result.returncode, 0, result.stderr)
        else:
            self.assertNotEqual(result.returncode, 0)
        return [json.loads(line) for line in self.log.read_text().splitlines()]

    def setup_args(self, calls):
        return next(args for name, args in calls if name == 'meson' and
            args and args[0] == 'setup')

    def test_openbsd_refuses_other_releases_before_build(self):
        self.env['NATIVE_GATE_TEST_OS'] = 'OpenBSD'
        for version in ('7.9', '8.0-current', '8.1'):
            with self.subTest(version=version):
                self.env['NATIVE_GATE_TEST_RELEASE'] = version
                calls = self.run_gate(succeeds=False)
                self.assertFalse(any(name == 'meson' for name, _ in calls))
                self.log.unlink()

    def test_openbsd_refuses_snapshot_kernel_with_release_uname(self):
        self.env['NATIVE_GATE_TEST_OS'] = 'OpenBSD'
        self.env['NATIVE_GATE_TEST_RELEASE'] = '8.0'
        for kernel in ('OpenBSD 8.0-current (GENERIC.MP) #1',
                       'OpenBSD 8.0-stable (GENERIC.MP) #1', ''):
            with self.subTest(kernel=kernel):
                self.env['NATIVE_GATE_TEST_KERNEL'] = kernel
                calls = self.run_gate(succeeds=False)
                self.assertFalse(any(name == 'meson' for name, _ in calls))
                self.log.unlink()

    def test_platform_wrappers_and_staging(self):
        for system, entry in [('Linux', 'run_on_linux.sh'),
                ('FreeBSD', 'run_on_bsd.sh'), ('OpenBSD', 'run_on_bsd.sh')]:
            with self.subTest(system=system):
                self.env['NATIVE_GATE_TEST_OS'] = system
                calls = self.run_gate(entry)
                args = self.setup_args(calls)
                self.assertIn('--buildtype=debugoptimized', args)
                self.assertIn('-Dlive-x-tests=true', args)
                self.assertIn('-Dpng-security-backport=true', args)
                self.assertIn('-Dgolang=disabled', args)
                pkg = next(arg for arg in args if arg.startswith('-Dpkg_config_path='))
                self.assertIn('/private/test-libs', pkg)
                if system != 'Linux':
                    self.assertIn('/usr/X11R6/lib/pkgconfig', pkg)
                self.assertTrue((self.build / 'install-stage/usr/local/bin/fvwm3').exists())
                self.log.unlink()
                shutil.rmtree(self.build)

    def test_go_profile_is_offline_and_stages_only_clients(self):
        calls = self.run_gate(flags=('--go-clients',))
        args = self.setup_args(calls)
        self.assertIn('-Dgolang=enabled', args)
        compile_args = next(args for name, args in calls if name == 'meson'
            and args[0] == 'compile')
        self.assertIn('fvwm3:executable', compile_args)
        self.assertNotIn('fvwm3', compile_args)
        self.assertIn('fvwm-menu-desktop', compile_args)
        self.assertIn('FvwmForm', compile_args)
        self.assertTrue(all(args == ['local', 'off', 'off']
            for name, args in calls if name == 'offline-env'))
        tests = next(args for name, args in calls if name == 'meson'
            and args[0] == 'test')
        for name in ('prompt-transport', 'prompt-client', 'command-client',
                'prompt-protocol-xvfb', 'command-client-xvfb',
                'xlock-menu-contracts', 'xlock-menu', 'xlock-menu-xvfb',
                'directory-menu-contracts', 'directory-menu', 'directory-menu-xvfb',
                'config-converter-contracts', 'config-converter',
                'script-com-example', 'script-com-example-xvfb',
                'desktop-menu', 'desktop-menu-xvfb'):
            self.assertIn(name, tests)
        install = next(args for name, args in calls if name == 'meson'
            and args[0] == 'install')
        self.assertIn('prompt-client,command-client,xlock-menu,directory-menu,config-converter,script-com-example,desktop-menu', install)
        self.assertTrue(any(name == 'python3' and 'tests/test_xdg_desktop.py'
            in args and '--provider' in args for name, args in calls))
        self.assertTrue(any(name == 'go' and '-race' in args
            for name, args in calls))

    def test_sanitizer_profile_does_not_stage(self):
        calls = self.run_gate(flags=('--sanitizers',))
        args = self.setup_args(calls)
        self.assertIn('--buildtype=debug', args)
        self.assertIn('-Db_sanitize=address,undefined', args)
        self.assertFalse(any(name == 'meson' and args[0] == 'install'
            for name, args in calls))

    def test_reconfigure_retains_cache_contract(self):
        private = self.build / 'meson-private'
        private.mkdir(parents=True)
        (private / 'coredata.dat').touch()
        args = self.setup_args(self.run_gate())
        self.assertIn('--reconfigure', args)
        self.assertIn('--clearcache', args)

    def test_failed_build_or_test_never_stages(self):
        for step in ('setup', 'compile', 'test'):
            with self.subTest(step=step):
                self.env['NATIVE_GATE_TEST_FAIL'] = step
                calls = self.run_gate(succeeds=False)
                self.assertFalse(any(name == 'meson' and args[0] == 'install'
                    for name, args in calls))
                self.log.unlink()

    def test_wrong_platform_refuses_before_setup(self):
        for system, entry, flags in [('Darwin', 'run_native_gate.sh', ()),
                ('OpenBSD', 'run_on_linux.sh', ()),
                ('Linux', 'run_on_bsd.sh', ()),
                ('FreeBSD', 'run_native_gate.sh', ('--sanitizers',))]:
            with self.subTest(system=system, entry=entry):
                self.env['NATIVE_GATE_TEST_OS'] = system
                calls = self.run_gate(entry, flags, succeeds=False)
                self.assertFalse(any(name == 'meson' for name, _ in calls))
                self.log.unlink()


if __name__ == '__main__':
    unittest.main()
