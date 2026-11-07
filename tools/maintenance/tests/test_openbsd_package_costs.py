#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License, version 2 or later.
import copy
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from openbsd_package_costs import parse_snapshot
from package_costs import compare


def row(name, size, requiring=None, required_by=None, declarations=''):
    return dict(contents='@name ' + name + '\n' + declarations,
        size=str(size) + '\n', requiring=requiring, required_by=required_by)


class OpenBSDPackageCosts(unittest.TestCase):
    def setUp(self):
        self.snapshot = {
            'native-1.0': row('native-1.0', 10, 'common-2.0\n', declarations=
                '@depend devel/common:common-*:common-2.0\n'),
            'candidate-1.0': row('candidate-1.0', 20, 'common-2.0\nextra-3.0\n'),
            'common-2.0': row('common-2.0', 100, required_by=
                'native-1.0\ncandidate-1.0\n'),
            'extra-3.0': row('extra-3.0', 30, required_by='candidate-1.0\n')}

    def test_shared_closure(self):
        packages = parse_snapshot(self.snapshot)
        result = compare(packages, ['native-1.0'], ['candidate-1.0'])
        self.assertEqual(result['baseline_flat_bytes'], 110)
        self.assertEqual(result['candidate_flat_bytes'], 150)
        self.assertEqual(result['net_flat_bytes'], 40)

    def test_empty_package_is_explicit_zero(self):
        self.assertEqual(parse_snapshot({'empty-1.0': row('empty-1.0', 0)})[
            'empty-1.0']['flat_bytes'], 0)

    def test_packing_list_is_data(self):
        self.snapshot['extra-3.0']['contents'] += '@exec impossible-command\n'
        self.assertIn('extra-3.0', parse_snapshot(self.snapshot))

    def test_missing_or_wrong_identity_and_size(self):
        for key, value in [('contents', '@name other-1.0\n'), ('size', ''),
                ('size', '-1\n'), ('size', True), ('size', '10 KB\n')]:
            with self.subTest(key=key, value=value):
                snapshot = copy.deepcopy(self.snapshot)
                snapshot['native-1.0'][key] = value
                with self.assertRaises(ValueError):
                    parse_snapshot(snapshot)

    def test_missing_dependency_and_reciprocity(self):
        for field, value in [('requiring', 'missing-1.0\n'),
                ('requiring', None), ('required_by', 'common-2.0\n')]:
            with self.subTest(field=field, value=value):
                snapshot = copy.deepcopy(self.snapshot)
                snapshot['native-1.0'][field] = value
                with self.assertRaises(ValueError):
                    parse_snapshot(snapshot)

    def test_duplicate_and_malformed_edges(self):
        for value in ['common-2.0\ncommon-2.0\n', '../common-2.0\n', '\n']:
            snapshot = copy.deepcopy(self.snapshot)
            snapshot['native-1.0']['requiring'] = value
            with self.assertRaises(ValueError):
                parse_snapshot(snapshot)

    def test_bounds_and_control_characters(self):
        for contents in ['@name native-1.0\n' + 'x' * (2 * 1024 * 1024),
                '@name native-1.0\r\n', '@name native-1.0\n\x00']:
            snapshot = copy.deepcopy(self.snapshot)
            snapshot['native-1.0']['contents'] = contents
            with self.assertRaises(ValueError):
                parse_snapshot(snapshot)


if __name__ == '__main__':
    unittest.main()
