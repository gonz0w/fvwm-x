#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the Free
# Software Foundation; either version 2 of the License, or any later version.
"""Exported observations are data; validating them must execute no binary."""
import hashlib
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest

spec = importlib.util.spec_from_file_location('serializer_comparison',
    Path(__file__).with_name('compare_status_serializers.py'))
comparison = importlib.util.module_from_spec(spec)
spec.loader.exec_module(comparison)


class CaptureTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.identities = {k: {'path': '/non-executable/' + k,
                              'sha256': 'a' * 64, 'bytes': 7}
                           for k in ('baseline', 'candidate')}
        self.row = {'backend': 'baseline', 'args': ['emit', 'ordinary'],
                    'exit': 0}
        self.metrics = {'wait_confirmed': True, 'timed_out': False,
                        'child_wait': 0, 'cancel_signal': 0,
                        'supervised_wall_ns': 100000, 'poll_ns': 1000000}
        for key, raw in [('stdout', b'{}\n'), ('stderr', b'')]:
            (self.root / key).write_bytes(raw)
            self.row[key] = {'path': key, 'bytes': len(raw),
                            'sha256': hashlib.sha256(raw).hexdigest()}
        self.manifest = {'schema': 1, 'artifacts': {
            k: {f: v[f] for f in ('sha256', 'bytes')}
            for k, v in self.identities.items()}, 'runs': [self.row]}

    def load(self):
        raw = json.dumps(self.metrics).encode()
        (self.root / 'metrics').write_bytes(raw)
        for row in self.manifest['runs']:
            row['metrics'] = {'path': 'metrics', 'bytes': len(raw),
                              'sha256': hashlib.sha256(raw).hexdigest()}
        (self.root / 'capture.json').write_text(json.dumps(self.manifest))
        return comparison.CapturedRuns(self.root, self.identities)

    def test_fifo_and_complete_consumption_without_execution(self):
        self.manifest['runs'].append(dict(self.row))
        captured = self.load()
        for _ in range(2):
            observed = captured.run(Path('/non-executable/baseline'),
                                    ['emit', 'ordinary'])
            self.assertEqual(observed.stdout, b'{}\n')
            self.assertEqual(observed.wall_ns, 100000)
        captured.consumed()
        with self.assertRaises(AssertionError):
            captured.run(Path('/non-executable/baseline'),
                         ['emit', 'ordinary'])

    def test_unconsumed_invocation_refuses(self):
        with self.assertRaises(AssertionError):
            self.load().consumed()

    def test_cross_backend_chronology_refuses(self):
        row = dict(self.row)
        row['backend'] = 'candidate'
        self.manifest['runs'].append(row)
        with self.assertRaises(AssertionError):
            self.load().run(Path('/non-executable/candidate'),
                            ['emit', 'ordinary'])

    def test_wrong_artifact_refuses_before_observation(self):
        self.manifest['artifacts']['baseline']['sha256'] = 'b' * 64
        with self.assertRaises(AssertionError):
            self.load()

    def test_changed_export_and_invalid_exit_refuse(self):
        for field, value in [('exit', True), ('exit', 124)]:
            with self.subTest(field=field, value=value):
                self.row[field] = value
                with self.assertRaises(AssertionError):
                    self.load().run(Path('/non-executable/baseline'),
                                    ['emit', 'ordinary'])
        self.row['exit'] = 0
        (self.root / 'stdout').write_bytes(b'changed')
        with self.assertRaises(AssertionError):
            self.load().run(Path('/non-executable/baseline'),
                            ['emit', 'ordinary'])

    def test_outside_and_symlink_exports_refuse(self):
        for name in ('../stdout', '/stdout', 'sub/../stdout'):
            with self.subTest(name=name):
                self.row['stdout']['path'] = name
                with self.assertRaises(AssertionError):
                    self.load().run(Path('/non-executable/baseline'),
                                    ['emit', 'ordinary'])
        self.row['stdout']['path'] = 'link'
        (self.root / 'link').symlink_to(self.root / 'stdout')
        with self.assertRaises(AssertionError):
            self.load().run(Path('/non-executable/baseline'),
                            ['emit', 'ordinary'])

    def test_manifest_size_bound(self):
        (self.root / 'capture.json').write_bytes(b' ' * (2 * 1024 * 1024 + 1))
        with self.assertRaises(AssertionError):
            comparison.CapturedRuns(self.root, self.identities)

    def test_allowed_child_exit_cannot_hide_watchdog_failure(self):
        for field, value in [('timed_out', True), ('cancel_signal', 15),
                             ('wait_confirmed', False), ('child_wait', 3),
                             ('supervised_wall_ns', 30_000_000_000)]:
            with self.subTest(field=field):
                original = self.metrics[field]
                self.metrics[field] = value
                with self.assertRaises(AssertionError):
                    self.load().run(Path('/non-executable/baseline'),
                                    ['emit', 'ordinary'])
                self.metrics[field] = original


if __name__ == '__main__':
    unittest.main()
