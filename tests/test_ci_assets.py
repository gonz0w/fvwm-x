#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or any later version.
"""Exercise CI asset publication with fresh, stale and unsafe cache inputs."""
import hashlib
import importlib.util
import io
import json
import os
from pathlib import Path
import stat
import subprocess
import sys
import tempfile
import time
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools/maintenance'))
spec = importlib.util.spec_from_file_location('ci_assets',
    ROOT / 'tools/maintenance/ci_assets.py')
assets = importlib.util.module_from_spec(spec)
spec.loader.exec_module(assets)


class AssetTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='fvwm-ci-assets-')
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.cache = self.root / 'cache'
        self.cache.mkdir(mode=0o700)
        self.output = self.root / 'output'
        self.body = b'pinned CI fixture\n'
        self.asset = {'name': 'anyvm.py', 'size': len(self.body),
            'sha256': hashlib.sha256(self.body).hexdigest(),
            'url': 'https://github.com/anyvm-org/anyvm/releases/download/v0.7.2/anyvm.py'}
        self.cached = self.cache / self.asset['sha256']

    def seed(self, body=None):
        self.cached.write_bytes(self.body if body is None else body)
        self.cached.chmod(0o600)

    def materialize(self, offline=True, timeout=10):
        return assets.materialize([self.asset], 'lock-hash', 'test',
            self.cache, self.output, offline, timeout=timeout)

    def assert_refused(self):
        with self.assertRaises((ValueError, OSError)):
            self.materialize()
        self.assertFalse(self.output.exists())

    def test_lock_selections(self):
        for profile in [*assets.PROFILES, 'all', 'png-source']:
            with self.subTest(profile=profile):
                selected, digest = assets.load_assets(assets.PINS, profile)
                expected = 1 if profile == 'png-source' else 13 if profile == 'all' else 4
                self.assertEqual(len(selected), expected)
                if profile == 'png-source':
                    self.assertEqual(selected, [assets.PNG_SOURCE])
                self.assertEqual(len(digest), 64)
                self.assertEqual(len({item['name'] for item in selected}), len(selected))

    def test_lock_refuses_inconsistent_records(self):
        original = json.loads(assets.PINS.read_text())
        mutations = [
            ('url', 'http://example.invalid/anyvm.py'),
            ('url', original['assets'][0]['url'] + '?other'),
            ('name', '../anyvm.py'), ('size', True), ('size', 0),
            ('size', assets.MAX_ASSET + 1), ('download_sha256', 'xyz'),
            ('publisher_digest', 'sha256:' + '0' * 64),
            ('repository', 'another/repository'), ('release', 'latest'),
            ('verified_download', False),
        ]
        for field, value in mutations:
            with self.subTest(field=field, value=value):
                lock = json.loads(json.dumps(original))
                lock['assets'][0][field] = value
                path = self.root / 'lock.json'
                path.write_text(json.dumps(lock))
                with self.assertRaises(ValueError):
                    assets.load_assets(path, 'all')
        for mode in ('missing', 'duplicate', 'oversized'):
            with self.subTest(mode=mode):
                lock = json.loads(json.dumps(original))
                if mode == 'missing':
                    lock['assets'].pop()
                elif mode == 'duplicate':
                    lock['assets'].append(lock['assets'][0])
                path.write_text(' ' * (assets.CHUNK + 1) if mode == 'oversized'
                                else json.dumps(lock))
                with self.assertRaises(ValueError):
                    assets.load_assets(path, 'all')

    def test_verified_cache_is_independent_private_output(self):
        self.seed()
        with patch.object(assets, 'download', side_effect=AssertionError('network')):
            rows = self.materialize()
        out = self.output / self.asset['name']
        self.assertEqual(out.read_bytes(), self.body)
        self.assertNotEqual(out.stat().st_ino, self.cached.stat().st_ino)
        self.assertEqual(stat.S_IMODE(self.output.stat().st_mode), 0o700)
        self.assertEqual(stat.S_IMODE(out.stat().st_mode), 0o600)
        self.assertEqual(rows[0]['origin'], 'verified-cache')
        self.assertEqual(json.loads((self.output / 'manifest.json').read_text())
                         ['lock_sha256'], 'lock-hash')
        out.write_bytes(b'consumer changes')
        self.assertEqual(self.cached.read_bytes(), self.body)

    def test_same_size_cache_corruption_is_not_trusted(self):
        self.seed(b'x' * len(self.body))
        self.assert_refused()
        self.assertEqual(self.cached.read_bytes(), b'x' * len(self.body))

    def test_wrong_size_cache_is_preserved_and_refused(self):
        self.seed(self.body[:-1])
        self.assert_refused()
        self.assertTrue(self.cached.exists())

    def test_symlink_cache_entry_is_refused(self):
        target = self.root / 'target'
        target.write_bytes(self.body)
        self.cached.symlink_to(target)
        self.assert_refused()
        self.assertEqual(target.read_bytes(), self.body)

    def test_fifo_cache_entry_does_not_wait(self):
        os.mkfifo(self.cached, 0o600)
        start = time.monotonic()
        self.assert_refused()
        self.assertLess(time.monotonic() - start, 2)

    def test_directory_cache_entry_is_refused(self):
        self.cached.mkdir(mode=0o700)
        self.assert_refused()

    def test_hardlinked_cache_entry_is_refused(self):
        self.seed()
        os.link(self.cached, self.root / 'another-name')
        self.assert_refused()

    def test_shared_cache_permissions_are_refused(self):
        self.seed()
        self.cached.chmod(0o640)
        self.assert_refused()
        self.cached.chmod(0o600)
        self.cache.chmod(0o755)
        self.assert_refused()

    def test_symlink_cache_directory_is_refused(self):
        self.cache = self.root / 'cache-link'
        self.cache.symlink_to(self.root / 'cache', target_is_directory=True)
        self.assert_refused()

    def test_missing_offline_input_keeps_existing_cache(self):
        self.assert_refused()
        self.assertEqual(list(self.cache.iterdir()), [])

    def test_existing_output_is_preserved(self):
        self.seed()
        self.output.mkdir()
        (self.output / 'keep').write_text('existing evidence')
        with self.assertRaises(ValueError):
            self.materialize()
        self.assertEqual((self.output / 'keep').read_text(), 'existing evidence')

    def test_multi_asset_failure_does_not_publish_partial_output(self):
        self.seed()
        missing = dict(self.asset, name='missing', sha256='0' * 64)
        with self.assertRaises(ValueError):
            assets.materialize([self.asset, missing], 'hash', 'test',
                self.cache, self.output, offline=True)
        self.assertFalse(self.output.exists())
        self.assertEqual(self.cached.read_bytes(), self.body)

    def test_short_oversize_and_wrong_hash_streams_are_refused(self):
        for body in (self.body[:-1], self.body + b'x', b'x' * len(self.body)):
            with self.subTest(body=body):
                out = self.root / 'partial'
                with self.assertRaises(ValueError):
                    assets.checked_copy(io.BytesIO(body), out, self.asset)
                out.unlink()

    def test_deadline_range(self):
        for value in (0, -1, 301, True, 1.5):
            with self.subTest(value=value), self.assertRaises(ValueError):
                assets.materialize([self.asset], 'hash', 'test', self.cache,
                    self.output, timeout=value)

    def worker_environment(self, body=None, delay=0):
        # Exercise the real acquisition subprocess without network or TLS
        # infrastructure. sitecustomize only alters this test child's urllib.
        site = self.root / 'sitecustomize.py'
        site.write_text('import io, time, urllib.request\n'
            'class Response(io.BytesIO):\n'
            '    def geturl(self): return "https://github.com/fixture"\n'
            'def fixture(url, timeout):\n'
            f'    time.sleep({delay!r})\n'
            f'    return Response({self.body if body is None else body!r})\n'
            'class Opener:\n'
            '    open = staticmethod(fixture)\n'
            'urllib.request.build_opener = lambda *args: Opener()\n')
        return patch.dict(os.environ, PYTHONPATH=str(self.root))

    def test_real_worker_publishes_download_and_reuses_cache(self):
        with self.worker_environment():
            rows = self.materialize(offline=False)
        self.assertEqual(rows[0]['origin'], 'verified-download')
        self.assertEqual(self.cached.read_bytes(), self.body)
        self.assertEqual(self.cached.stat().st_nlink, 1)
        first = self.output
        self.output = self.root / 'second'
        with patch.object(assets, 'download', side_effect=AssertionError('network')):
            self.assertEqual(self.materialize()[0]['origin'], 'verified-cache')
        self.assertNotEqual((first / self.asset['name']).stat().st_ino,
                            self.cached.stat().st_ino)

    def test_real_worker_refuses_mismatch_and_removes_partial_stage(self):
        with self.worker_environment(body=b'x' * len(self.body)):
            with self.assertRaises(ValueError):
                self.materialize(offline=False)
        self.assertFalse(self.output.exists())
        self.assertEqual(list(self.cache.iterdir()), [])
        self.assertFalse(list(self.root.glob('.fvwm-maint-*')))

    def test_real_worker_total_deadline(self):
        start = time.monotonic()
        with self.worker_environment(delay=10):
            with self.assertRaises(subprocess.TimeoutExpired):
                self.materialize(offline=False, timeout=1)
        self.assertLess(time.monotonic() - start, 5)
        self.assertFalse(self.output.exists())
        self.assertEqual(list(self.cache.iterdir()), [])

    def test_concurrent_identical_cache_publication(self):
        original = assets.download
        def competing_download(asset, destination, timeout):
            original(asset, destination, timeout)
            self.seed()
        with self.worker_environment(), patch.object(assets, 'download',
                side_effect=competing_download):
            self.materialize(offline=False)
        self.assertEqual(self.cached.read_bytes(), self.body)
        self.assertEqual(self.cached.stat().st_nlink, 1)

    def test_concurrent_corrupt_cache_publication_is_refused(self):
        original = assets.download
        def competing_download(asset, destination, timeout):
            original(asset, destination, timeout)
            self.seed(b'x' * len(self.body))
        with self.worker_environment(), patch.object(assets, 'download',
                side_effect=competing_download):
            with self.assertRaises(ValueError):
                self.materialize(offline=False)
        self.assertFalse(self.output.exists())
        self.assertEqual(self.cached.read_bytes(), b'x' * len(self.body))

    def test_https_redirect_refuses_downgrade_before_request(self):
        with self.assertRaises(ValueError):
            assets.HTTPSRedirects().redirect_request(None, None, 302,
                'redirect', {}, 'http://example.invalid/asset')

    def test_worker_refuses_unlocked_url_without_opening_it(self):
        with self.worker_environment(delay=10):
            result = subprocess.run([sys.executable, str(Path(assets.__file__)),
                '--download-worker', 'file:///not-an-asset',
                str(self.asset['size']), self.asset['sha256'],
                str(self.root / 'unexpected')], capture_output=True, timeout=2)
        self.assertNotEqual(result.returncode, 0)
        self.assertFalse((self.root / 'unexpected').exists())


if __name__ == '__main__':
    unittest.main()
