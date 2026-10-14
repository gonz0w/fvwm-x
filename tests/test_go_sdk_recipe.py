#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or any later version.
"""Check SDK recipe refusal before downloading, extracting or replacing data."""
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class SdkRecipeTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix='fvwm-go-sdk-')
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        tools = self.root / 'tools'
        tools.mkdir()
        uname = tools / 'uname'
        uname.write_text('#!' + sys.executable + '\nimport os,sys\n'
                         'print(os.environ.get("SDK_TEST_SYSTEM", "OpenBSD") '
                         'if sys.argv[1] == "-s" else "arm64")\n')
        uname.chmod(0o755)
        self.env = dict(os.environ, PATH=str(tools) + os.pathsep + os.environ['PATH'])
        self.archive = self.root / 'sdk.tgz'
        self.archive.write_bytes(b'not the authenticated compiler')

    def run_recipe(self, archive=None):
        result = subprocess.run(['sh', str(ROOT / 'dev-docs/ci/bsd-go-sdk.sh'),
                                 str(archive or self.archive)], cwd=self.root,
                                env=self.env, capture_output=True, text=True, timeout=10)
        self.assertNotEqual(result.returncode, 0)
        self.assertFalse((self.root / 'builddir-bsd/go-sdk-environment.sh').exists())
        self.assertFalse((self.root / 'builddir-bsd-go-sdk/go').exists())
        return result

    def test_wrong_archive_refused_before_extraction(self):
        self.run_recipe()
        self.assertEqual(self.archive.read_bytes(), b'not the authenticated compiler')

    def test_symlink_archive_refused(self):
        link = self.root / 'alias.tgz'
        link.symlink_to(self.archive)
        self.run_recipe(link)

    def test_existing_sdk_preserved(self):
        sdk = self.root / 'builddir-bsd-go-sdk'
        sdk.mkdir()
        marker = sdk / 'owner'
        marker.write_text('existing compiler')
        result = self.run_recipe()
        self.assertIn('Refuse to overwrite', result.stderr)
        self.assertEqual(marker.read_text(), 'existing compiler')

    def test_unsupported_platform_refused(self):
        self.env['SDK_TEST_SYSTEM'] = 'OtherOS'
        self.run_recipe()
        self.assertFalse((self.root / 'builddir-bsd-go-sdk').exists())


if __name__ == '__main__':
    unittest.main()
