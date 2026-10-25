# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or any later version.
"""Exercise the production POSIX signal requirement with native Meson."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[3]
DIAGNOSTIC = 'FVWM-X requires POSIX sigaction and sigemptyset'


@unittest.skipUnless(shutil.which('meson'), 'native Meson is unavailable')
class SignalRequirementTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix='fvwm-signals-')
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        source = self.root / 'source'
        source.mkdir()
        text = (ROOT / 'meson.build').read_text()
        block = text.split('# POSIX signal installation requirement.\n', 1)[1]
        block = block.split('# End POSIX signal installation requirement.', 1)[0]
        (source / 'meson.build').write_text(
            "project('signal-requirement', 'c', meson_version: '>=1.5.1',\n"
            "    default_options: ['c_std=c11'])\n"
            "cc = meson.get_compiler('c')\n"
            "syslibs = []\n" + block)

    def configure(self, *options):
        return subprocess.run(
            ['meson', 'setup', str(self.root / 'build'),
             str(self.root / 'source'), *options],
            capture_output=True, text=True, timeout=120)

    def assert_refused(self, result):
        self.assertNotEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn(DIAGNOSTIC, result.stdout + result.stderr)

    def test_declared_linkable_posix_signals(self):
        result = self.configure()
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_missing_signal_header_is_a_configuration_error(self):
        header = self.root / 'include/signal.h'
        header.parent.mkdir()
        header.write_text('#error signal header deliberately unavailable\n')
        self.assert_refused(self.configure(
            '-Dc_args=-I' + str(header.parent)))

    def test_missing_declarations_are_a_configuration_error(self):
        header = self.root / 'include/signal.h'
        header.parent.mkdir()
        # Supply the types and constants used by the probe, but no functions.
        # Parenthesized calls must reject this even without -Werror.
        header.write_text(
            'typedef struct { int unused; } sigset_t;\n'
            'struct sigaction {\n'
            '    void (*sa_handler)(int);\n'
            '    sigset_t sa_mask;\n'
            '};\n'
            '#define SIG_DFL ((void (*)(int))0)\n'
            '#define SIGTERM 15\n')
        self.assert_refused(self.configure(
            '-Dc_args=-I' + str(header.parent)))

    def test_missing_sigaction_symbol_is_a_configuration_error(self):
        self.assert_refused(self.configure(
            '-Dc_args=-Dsigaction=fvwm_unavailable_sigaction'))

    def test_missing_sigemptyset_symbol_is_a_configuration_error(self):
        self.assert_refused(self.configure(
            '-Dc_args=-Dsigemptyset=fvwm_unavailable_sigemptyset'))


if __name__ == '__main__':
    unittest.main()
