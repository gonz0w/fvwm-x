#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or any later version.
"""Check PNG source identity, transitive loader refusal and owned deadlines."""
import hashlib
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import time
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools/maintenance'))
import ci_png


class PngTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='fvwm-ci-png-')
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)

    def test_source_refuses_changed_release_without_output(self):
        source = self.root / 'source.xz'
        source.write_bytes(b'changed release')
        output = self.root / 'source.tar'
        with self.assertRaises(ValueError):
            ci_png.prepare_source(source, output)
        self.assertFalse(output.exists())

    def test_guest_refuses_changed_payload_before_extraction(self):
        source = self.root / 'source.tar'
        source.write_bytes(b'changed tar')
        output = self.root / 'extract'
        output.mkdir()
        with self.assertRaises(ValueError):
            ci_png.extract_source(source, output)
        self.assertEqual(list(output.iterdir()), [])

    def test_loader_accepts_owned_aliases(self):
        prefix = self.root / 'prefix'
        prefix.mkdir()
        library = prefix / 'libpng16.so.18.2'
        library.write_bytes(b'patched PNG')
        alias = prefix / 'libpng.so.18.2'
        alias.symlink_to(library.name)
        rows = ci_png.check_loader('\tlibpng.so.18.2 => ' + str(alias), prefix)
        self.assertEqual(rows, {str(alias): hashlib.sha256(library.read_bytes()).hexdigest()})

    def test_loader_refuses_transitive_vendor_even_with_patched_direct(self):
        prefix = self.root / 'prefix'
        prefix.mkdir()
        library = prefix / 'libpng16.so.16'
        library.write_bytes(b'patched PNG')
        vendor = self.root / 'libpng.so.18.2'
        vendor.write_bytes(b'unverified vendor PNG')
        text = str(library) + '\n' + str(vendor)
        with self.assertRaises(ValueError):
            ci_png.check_loader(text, prefix)

    def test_loader_refuses_missing_png_and_escaping_alias(self):
        with self.assertRaises(ValueError):
            ci_png.check_loader('no PNG consumer', self.root)
        prefix = self.root / 'prefix'
        prefix.mkdir()
        outside = self.root / 'libpng.so'
        outside.write_bytes(b'outside')
        alias = prefix / 'libpng.so'
        alias.symlink_to(outside)
        with self.assertRaises(ValueError):
            ci_png.check_loader(str(alias), prefix)

    def test_deadline_reaps_owned_process_group_and_retains_log(self):
        log = self.root / 'timeout.log'
        pid = self.root / 'pid'
        code = ('import pathlib,time,os; pathlib.Path(' + repr(str(pid)) +
                ').write_text(str(os.getpid())); print("started", flush=True); time.sleep(30)')
        start = time.monotonic()
        with self.assertRaises(TimeoutError):
            ci_png.command([sys.executable, '-c', code], log, timeout=10)
        self.assertLess(time.monotonic() - start, 20)
        self.assertIn('started', log.read_text())
        with self.assertRaises(ProcessLookupError):
            os.kill(int(pid.read_text()), 0)

    def test_failed_command_preserves_output_and_refuses_reuse(self):
        log = self.root / 'failure.log'
        with self.assertRaises(RuntimeError):
            ci_png.command([sys.executable, '-c', 'print("failed"); exit(23)'], log)
        self.assertIn('failed', log.read_text())
        with self.assertRaises(FileExistsError):
            ci_png.command([sys.executable, '-c', 'exit(0)'], log)


if __name__ == '__main__':
    unittest.main()
