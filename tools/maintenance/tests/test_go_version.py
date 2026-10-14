# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or any later version.
"""Exercise compiler patch boundaries and the actual Meson detection block."""
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / 'tools/maintenance'))
import go_version


class GoVersionTests(unittest.TestCase):
    def test_patched_stable_versions(self):
        for value in ('go1.26.9', 'go1.26.10', 'go1.27.2', 'go1.27.3',
                      'go1.28.0', 'go2.0.0'):
            with self.subTest(value=value):
                self.assertEqual(go_version.supported_version(value), value[2:])

    def test_old_and_unpatched_versions(self):
        for value in ('go1.21.13', 'go1.22.2', 'go1.25.13', 'go1.26.0',
                      'go1.26.8', 'go1.27.0', 'go1.27.1'):
            with self.subTest(value=value), self.assertRaises(ValueError):
                go_version.supported_version(value)

    def test_only_complete_stable_output(self):
        for value in ('', 'devel go1.28-abcd', 'go1.27rc1', 'go1.27.2rc1',
                      'go1.27', 'go01.27.2', 'go1.027.2', 'go1.27.02',
                      'go1.27.2 linux/amd64', 'go1.27.2\ngo1.28.0',
                      'go１.27.2'):
            with self.subTest(value=value), self.assertRaises(ValueError):
                go_version.supported_version(value)
        self.assertEqual(go_version.supported_version('go1.27.2\n'), '1.27.2')

    def test_cli_failure_and_normalized_success(self):
        script = ROOT / 'tools/maintenance/go_version.py'
        result = subprocess.run([sys.executable, str(script), 'go1.27.0'],
                                capture_output=True, text=True, timeout=10)
        self.assertEqual(result.returncode, 1)
        self.assertEqual(result.stdout, '')
        self.assertIn('-Dgolang=disabled', result.stderr)
        self.assertIn('1.27.2', result.stderr)
        result = subprocess.run([sys.executable, str(script), 'go1.27.2'],
                                capture_output=True, text=True, timeout=10)
        self.assertEqual(result.returncode, 0)
        self.assertEqual(result.stdout, '1.27.2\n')


@unittest.skipUnless(shutil.which('meson'), 'native Meson is unavailable')
class GoMesonTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix='fvwm-go-policy-')
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.source = self.root / 'source'
        self.source.mkdir()
        self.tools = self.root / 'tools'
        self.tools.mkdir()
        self.calls = self.root / 'calls.json'
        # The fixture executes the production block, not a hand-copied policy.
        text = (ROOT / 'meson.build').read_text()
        block = text.split('# Optional Go compiler policy;', 1)[1]
        block = block.split('# End optional Go compiler policy.', 1)[0]
        block = '# Optional Go compiler policy;' + block
        (self.source / 'meson.build').write_text(
            "project('go-policy', meson_version: '>=1.5.1')\n"
            "py = import('python').find_installation('python3')\n" + block)
        (self.source / 'meson.options').write_text(
            "option('golang', type: 'feature', value: 'auto')\n")
        target = self.source / 'tools/maintenance'
        target.mkdir(parents=True)
        shutil.copy2(ROOT / 'tools/maintenance/go_version.py', target)
        fake = self.tools / 'go'
        fake.write_text('#!' + sys.executable + '\n'
                        'import json, os, pathlib, sys\n'
                        'pathlib.Path(os.environ["GO_POLICY_CALLS"]).write_text('
                        'json.dumps({"argv":sys.argv[1:],'
                        '"toolchain":os.environ.get("GOTOOLCHAIN")}))\n'
                        'print(os.environ["GO_POLICY_VERSION"])\n')
        fake.chmod(0o755)

    def configure(self, version, feature='enabled'):
        env = dict(os.environ, PATH=str(self.tools) + os.pathsep + os.environ['PATH'],
                   GO_POLICY_VERSION=version, GO_POLICY_CALLS=str(self.calls),
                   GOTOOLCHAIN='auto')
        return subprocess.run(['meson', 'setup', str(self.root / 'build'),
                               str(self.source), '-Dgolang=' + feature],
                              env=env, capture_output=True, text=True, timeout=120)

    def test_patched_compiler_uses_selected_program_without_downloading(self):
        result = self.configure('go1.27.2')
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual(json.loads(self.calls.read_text()),
                         {'argv': ['env', 'GOVERSION'], 'toolchain': 'local'})

    def test_unpatched_compiler_fails_with_upgrade_or_disable_diagnostic(self):
        result = self.configure('go1.27.0')
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('-Dgolang=disabled', result.stdout + result.stderr)

    def test_disabled_go_never_invokes_compiler(self):
        result = self.configure('devel unsupported', feature='disabled')
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertFalse(self.calls.exists())

    def test_auto_feature_refuses_unpatched_present_compiler(self):
        result = self.configure('go1.26.8', feature='auto')
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('1.26.9', result.stdout + result.stderr)


if __name__ == '__main__':
    unittest.main()
