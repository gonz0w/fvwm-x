#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or any later version.
"""Check PNG source identity, transitive loader refusal and owned deadlines."""
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import time
import unittest
from unittest.mock import patch

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

    def consumer_fixture(self):
        output = self.root / 'bootstrap'
        prefix = output / 'prefix'
        library = prefix / 'lib/libpng16.so.18.2'
        library.parent.mkdir(parents=True)
        library.write_bytes(b'qualified PNG fixture')
        (output / 'receipt.json').write_text(json.dumps({'installed': {
            'lib/libpng16.so.18.2': ci_png.digest(library)}}))
        directory = self.root / 'build'
        for name in ['fvwm3', 'bin/fvwm-image-svg-worker',
                'install-stage/usr/local/bin/fvwm3',
                'install-stage/usr/local/libexec/fvwm-image-svg-worker']:
            path = directory / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(name.encode())
        def loader(argv, log, **kwargs):
            self.assertNotIn('LD_BIND_NOW', kwargs['env'])
            self.assertEqual(argv[0], 'ldd')
            log.write_text('libpng => ' + str(library))
        return output, directory, library, loader

    def test_build_and_install_proofs_coexist_without_overwriting(self):
        output, directory, library, loader = self.consumer_fixture()
        with patch.object(ci_png, 'command', side_effect=loader), patch.dict(
                os.environ, LD_BIND_NOW='1'):
            ci_png.verify(output, directory, build_only=True)
            built = directory / 'png-verify-built'
            before = {p.name: p.read_bytes() for p in built.iterdir()}
            ci_png.verify(output, directory)
            installed = directory / 'png-verify-installed'
            self.assertEqual(len(json.loads(
                (built / 'png-consumers.json').read_text())), 2)
            self.assertEqual(len(json.loads(
                (installed / 'png-consumers.json').read_text())), 4)
            self.assertEqual({p.name: p.read_bytes() for p in built.iterdir()}, before)
            with self.assertRaises(FileExistsError):
                ci_png.verify(output, directory, build_only=True)
            self.assertEqual({p.name: p.read_bytes() for p in built.iterdir()}, before)
            repeated = directory / 'second-installed-observation'
            ci_png.verify(output, directory, evidence_directory=repeated)
            self.assertEqual((installed / 'png-consumers.json').read_bytes(),
                             (repeated / 'png-consumers.json').read_bytes())

    def test_changed_consumer_library_keeps_failed_log_without_success_receipt(self):
        output, directory, library, loader = self.consumer_fixture()
        library.write_bytes(b'changed PNG')
        with patch.object(ci_png, 'command', side_effect=loader):
            with self.assertRaises(ValueError):
                ci_png.verify(output, directory)
        evidence = directory / 'png-verify-installed'
        self.assertTrue((evidence / 'png-loader-0.log').is_file())
        self.assertFalse((evidence / 'png-consumers.json').exists())

    def test_missing_stage_does_not_claim_installed_qualification(self):
        output, directory, library, loader = self.consumer_fixture()
        (directory / 'install-stage/usr/local/bin/fvwm3').unlink()
        with patch.object(ci_png, 'command', side_effect=loader) as run:
            with self.assertRaises(ValueError):
                ci_png.verify(output, directory)
            run.assert_not_called()
        self.assertFalse((directory / 'png-verify-installed/png-consumers.json').exists())

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
