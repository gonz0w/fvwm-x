# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or any later version.
"""Exercise the production dependency block with real Xlib development files."""
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[3]


@unittest.skipUnless(shutil.which('meson') and shutil.which('pkg-config'),
                     'native Meson/pkg-config are unavailable')
class XlibRequirementTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix='fvwm-xlib-')
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        source = self.root / 'source'
        source.mkdir()
        text = (ROOT / 'meson.build').read_text()
        block = text.split('# Dependencies.\n', 1)[1]
        block = block.split('# End Xlib XKB requirement.', 1)[0]
        (source / 'meson.build').write_text(
            "project('xlib-requirement', 'c', meson_version: '>=1.5.1')\n"
            "cc = meson.get_compiler('c')\n"
            "conf = configuration_data()\n" + block +
            "\nconfigure_file(output: 'config.h', configuration: conf)\n")
        self.calls = self.root / 'calls.jsonl'
        wrapper = self.root / 'pkg-config'
        wrapper.write_text(
            '#!' + sys.executable + '\n'
            'import json, os, sys\n'
            'with open(' + repr(str(self.calls)) + ', "a") as stream:\n'
            '    stream.write(json.dumps(sys.argv[1:]) + "\\n")\n'
            'if any("xkbcommon" in arg for arg in sys.argv[1:]):\n'
            '    sys.exit(1)\n'
            'os.execv(' + repr(shutil.which('pkg-config')) +
            ', ["pkg-config"] + sys.argv[1:])\n')
        wrapper.chmod(0o755)
        native = self.root / 'native.ini'
        # Meson native-file strings use single quotes, not shell quoting.
        self.assertNotIn("'", str(wrapper))
        native.write_text("[binaries]\npkg-config = '" + str(wrapper) + "'\n")

    def configure(self, *options):
        return subprocess.run(
            ['meson', 'setup', str(self.root / 'build'),
             str(self.root / 'source'), '--native-file',
             str(self.root / 'native.ini'), *options],
            capture_output=True, text=True, timeout=120)

    def test_real_xlib_with_libxkbcommon_unavailable(self):
        result = self.configure()
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        calls = [json.loads(row) for row in self.calls.read_text().splitlines()]
        self.assertTrue(calls)
        self.assertFalse(any('xkbcommon' in arg for row in calls for arg in row))
        config = (self.root / 'build/config.h').read_text()
        self.assertIn('#define HAVE_X11_XKBLIB_H 1', config)

    def test_missing_xkb_header_is_a_configuration_error(self):
        header = self.root / 'include/X11/XKBlib.h'
        header.parent.mkdir(parents=True)
        header.write_text('#error XKB header deliberately unavailable\n')
        result = self.configure('-Dc_args=-I' + str(self.root / 'include'))
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('Xlib development files must provide', result.stdout)

    def test_missing_xkb_symbol_is_a_configuration_error(self):
        result = self.configure(
            '-Dc_args=-DXkbKeycodeToKeysym=fvwm_unavailable_xkb_symbol')
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('Xlib development files must provide', result.stdout)


if __name__ == '__main__':
    unittest.main()
