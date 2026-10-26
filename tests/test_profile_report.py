#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
"""Normalized empirical reports never substitute missing data or accept a build."""
import copy
from contextlib import redirect_stderr, redirect_stdout
import importlib.util
import io
import json
from pathlib import Path
import tempfile
import unittest

SCRIPT = Path(__file__).resolve().parents[1] / 'tools/maintenance/profile_report.py'
SPEC = importlib.util.spec_from_file_location('profile_report', SCRIPT)
profile_report = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(profile_report)


def receipt(values=(10, 12, 11)):
    return {
        'schema_version': 1,
        'context': {
            'profile': 'complete', 'workload': 'session',
            'workload_version': '1', 'config': 'config-manifest-sha256',
            'source_scope': 'desktop', 'artifact_scope': 'installed-tree',
            'platform': 'platform-manifest-sha256',
            'compiler': 'compiler-manifest-sha256',
            'instrumentation': 'sampling-manifest-sha256',
            'effective_features': {'svg': 'enabled', 'nls': 'disabled',
                                   'service': 'unavailable'},
        },
        'source_revision': 'revision-a', 'artifact_identity': 'artifact-a',
        'metrics': {'rss': {'unit': 'KiB', 'scope': 'all-session-residents',
                            'coverage': 'complete', 'quantum': 1}},
        'runs': [{'id': 'run-' + str(index), 'metrics': {'rss': value}}
                 for index, value in enumerate(values)],
    }


class ProfileReportTests(unittest.TestCase):
    def test_envelope_range_and_quantum(self):
        result = profile_report.report(receipt())
        self.assertEqual(result['candidate']['metrics']['rss'][
            'proposed_envelope'], 14)
        flat = profile_report.report(receipt((0, 0, 0)))
        self.assertEqual(flat['candidate']['metrics']['rss'][
            'proposed_envelope'], 1)
        self.assertIn('no statistical confidence', result['policy'])
        self.assertIn('no automatic acceptance', result['policy'])

    def test_frozen_baseline_allows_equal_and_different_revisions(self):
        baseline = receipt()
        candidate = receipt((13, 14, 12))
        candidate['source_revision'] = 'revision-b'
        candidate['artifact_identity'] = 'artifact-b'
        result = profile_report.report(candidate, baseline)
        self.assertEqual(result['comparison']['rss'], 'within_proposed_envelope')
        self.assertEqual(result['baseline']['metrics']['rss'][
            'proposed_envelope'], 14)
        self.assertEqual(result['candidate']['source_revision'], 'revision-b')
        self.assertEqual(result['baseline']['artifact_identity'], 'artifact-a')
        candidate['runs'][0]['metrics']['rss'] = 15
        self.assertEqual(profile_report.report(candidate, baseline)[
            'comparison']['rss'], 'above_proposed_envelope')

    def test_every_context_identity_must_match(self):
        for key in receipt()['context']:
            with self.subTest(key=key):
                candidate = receipt()
                if key == 'effective_features':
                    candidate['context'][key]['svg'] = 'disabled'
                else:
                    candidate['context'][key] += '-changed'
                with self.assertRaisesRegex(ValueError, 'context mismatch'):
                    profile_report.report(candidate, receipt())

    def test_metric_set_unit_scope_coverage_and_quantum_must_match(self):
        for key, value in [('unit', 'bytes'), ('scope', 'core-only'),
                           ('coverage', 'partial'), ('quantum', 2)]:
            with self.subTest(key=key):
                candidate = receipt()
                candidate['metrics']['rss'][key] = value
                with self.assertRaisesRegex(ValueError, 'metric mismatch'):
                    profile_report.report(candidate, receipt())
        candidate = receipt()
        candidate['metrics']['cpu'] = copy.deepcopy(candidate['metrics']['rss'])
        with self.assertRaisesRegex(ValueError, 'metric mismatch'):
            profile_report.report(candidate, receipt())

    def test_missing_null_and_partial_never_make_an_envelope(self):
        for missing in ('absent', 'null', 'partial', 'unavailable'):
            with self.subTest(missing=missing):
                candidate = receipt()
                if missing == 'absent':
                    del candidate['runs'][0]['metrics']['rss']
                elif missing == 'null':
                    candidate['runs'][0]['metrics']['rss'] = None
                else:
                    candidate['metrics']['rss']['coverage'] = missing
                result = profile_report.report(candidate, copy.deepcopy(candidate))
                metric = result['candidate']['metrics']['rss']
                self.assertIsNone(metric['proposed_envelope'])
                self.assertEqual(result['comparison']['rss'], 'unavailable')

    def test_three_distinct_run_ids_required(self):
        candidate = receipt((1, 2))
        with self.assertRaisesRegex(ValueError, '3.*runs'):
            profile_report.report(candidate)
        candidate = receipt()
        candidate['runs'][1]['id'] = candidate['runs'][0]['id']
        with self.assertRaisesRegex(ValueError, 'duplicate run'):
            profile_report.report(candidate)

    def test_bad_numbers_and_unknown_metrics_are_rejected(self):
        for value in (True, -1, float('inf'), float('nan'), '1', [], {}):
            with self.subTest(value=value):
                candidate = receipt()
                candidate['runs'][0]['metrics']['rss'] = value
                with self.assertRaises(ValueError):
                    profile_report.report(candidate)
                candidate = receipt()
                candidate['metrics']['rss']['quantum'] = value
                with self.assertRaises(ValueError):
                    profile_report.report(candidate)
        candidate = receipt()
        candidate['runs'][0]['metrics']['unknown'] = 1
        with self.assertRaisesRegex(ValueError, 'unknown metric'):
            profile_report.report(candidate)

    def test_malformed_schema_and_auto_feature_are_rejected(self):
        candidates = [None, [], {}, receipt()]
        candidates[-1]['context']['effective_features']['svg'] = 'auto'
        for candidate in candidates:
            with self.subTest(candidate=candidate):
                with self.assertRaises(ValueError):
                    profile_report.report(candidate)
        candidate = receipt()
        candidate['schema_version'] = True
        with self.assertRaises(ValueError):
            profile_report.report(candidate)
        candidate = receipt()
        candidate['runs'][0]['extra'] = 1
        with self.assertRaises(ValueError):
            profile_report.report(candidate)

    def test_overflow_and_duplicate_json_members_are_rejected(self):
        with self.assertRaisesRegex(ValueError, 'finite'):
            profile_report.report(receipt((1e308, 0, 1e308)))
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'receipt.json'
            path.write_text('{"schema_version":1,"schema_version":1}')
            with self.assertRaisesRegex(ValueError, 'duplicate JSON'):
                profile_report.load_receipt(path)
            path.write_text(json.dumps(receipt()))
            self.assertEqual(profile_report.load_receipt(path), receipt())
            path.write_bytes(b' ' * (profile_report.MAX_BYTES + 1))
            with self.assertRaisesRegex(ValueError, 'too large'):
                profile_report.load_receipt(path)

    def test_cli_reports_without_writing_inputs_and_refuses_mismatch(self):
        with tempfile.TemporaryDirectory() as directory:
            candidate = Path(directory) / 'candidate.json'
            baseline = Path(directory) / 'baseline.json'
            original = json.dumps(receipt())
            candidate.write_text(original)
            baseline.write_text(original)
            output = io.StringIO()
            with redirect_stdout(output):
                status = profile_report.main(
                    [str(candidate), '--baseline', str(baseline)])
            self.assertEqual(status, 0)
            self.assertEqual(json.loads(output.getvalue())['comparison']['rss'],
                             'within_proposed_envelope')
            self.assertEqual(candidate.read_text(), original)
            self.assertEqual(baseline.read_text(), original)
            changed = receipt()
            changed['context']['platform'] = 'different-kernel-manifest'
            candidate.write_text(json.dumps(changed))
            output, errors = io.StringIO(), io.StringIO()
            with redirect_stdout(output), redirect_stderr(errors):
                status = profile_report.main(
                    [str(candidate), '--baseline', str(baseline)])
            self.assertEqual(status, 2)
            self.assertEqual(output.getvalue(), '')
            self.assertIn('context mismatch', errors.getvalue())


if __name__ == '__main__':
    unittest.main()
