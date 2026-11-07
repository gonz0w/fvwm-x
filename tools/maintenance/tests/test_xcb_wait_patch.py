#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License, version 2 or later.
"""Private XCB patch admission and matching-source application checks."""
import hashlib
import importlib.util
import os
from pathlib import Path
import tarfile
import tempfile
import unittest

FIXTURE = Path(__file__).resolve().parents[3] / 'tests/fixtures/xcb-wait'
SPEC = importlib.util.spec_from_file_location('xcb_wait_patch',
    FIXTURE / 'apply_private.py')
patcher = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(patcher)
ARCHIVE_SHA256 = '1cb65df8543a69ec0555ac696123ee386321dfac1964a3da39976c9a05ad724d'


class Patch(unittest.TestCase):
    def test_anchor_missing_or_duplicated_is_rejected(self):
        for text in ('nothing', 'before before'):
            with self.subTest(text=text), self.assertRaises(ValueError):
                patcher.replace_once(text, 'before', 'after')

    def test_mismatched_source_is_not_modified(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary).resolve()
            (root / 'src').mkdir()
            path = root / 'src/xcb_in.c'
            path.write_text('different source\n')
            with self.assertRaises(ValueError):
                patcher.patch(root)
            self.assertEqual(path.read_text(), 'different source\n')
            self.assertEqual(sorted(p.name for p in (root / 'src').iterdir()),
                ['xcb_in.c'])

    def test_source_symlink_is_rejected(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary).resolve()
            (root / 'src').mkdir()
            target = root / 'other.c'
            target.write_text('owned test\n')
            (root / 'src/xcb_in.c').symlink_to(target)
            with self.assertRaises(ValueError):
                patcher.patch(root)
            self.assertEqual(target.read_text(), 'owned test\n')

    @unittest.skipUnless(os.environ.get('XCB_WAIT_SOURCE_ARCHIVE'),
        'matching source archive supplied by the selected qualification job')
    def test_exact_source_patch_and_reapplication(self):
        archive = Path(os.environ['XCB_WAIT_SOURCE_ARCHIVE'])
        self.assertLessEqual(archive.stat().st_size, 1048576)
        self.assertEqual(hashlib.sha256(archive.read_bytes()).hexdigest(),
            ARCHIVE_SHA256)
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary).resolve()
            (root / 'src').mkdir()
            with tarfile.open(archive) as source:
                for name in patcher.PINS:
                    member = source.getmember('libxcb-1.15/' + name)
                    self.assertTrue(member.isfile())
                    self.assertLessEqual(member.size, 262144)
                    (root / name).write_bytes(source.extractfile(member).read())
            result = patcher.patch(root)
            self.assertEqual(set(result), set(patcher.PINS) |
                {'src/fvwm_xcb_wait.c', 'src/fvwm_xcb_wait.h'})
            text = (root / 'src/xcb_in.c').read_text()
            self.assertEqual(text.count('fvwm_xcb_wait_reply_begin(c, request);'), 1)
            self.assertEqual(text.count('fvwm_xcb_wait_reply_wait(c, request);'), 1)
            self.assertEqual(text.count('fvwm_xcb_wait_partial(c);'), 1)
            self.assertEqual(text.count('fvwm_xcb_wait_reply_end(c, request, ret != 0);'), 1)
            self.assertIn('int ret = read_block(c, (char *) buf + done, len - done);', text)
            self.assertIn('fvwm_xcb_wait.lo',
                (root / 'src/Makefile.in').read_text())
            with self.assertRaises(ValueError):
                patcher.patch(root)
            self.assertEqual(result, {name: hashlib.sha256(
                (root / name).read_bytes()).hexdigest() for name in result})


if __name__ == '__main__':
    unittest.main()
