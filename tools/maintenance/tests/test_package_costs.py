#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License, version 2 or later.
"""Pure checks for nominal package dependency closure comparisons."""
from copy import deepcopy
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from package_costs import compare


def package(size, dependencies=None, version='1.0'):
    return dict(version=version, flat_bytes=size, dependencies=dependencies or {})


class PackageCosts(unittest.TestCase):
    def test_transitive_shared_closure_and_removed_packages(self):
        packages = {
            'base': package(10, {'shared': '1.0', 'old': '1.0'}),
            'candidate': package(20, {'shared': '1.0', 'new': '1.0'}),
            'shared': package(30, {'leaf': '1.0'}),
            'leaf': package(40), 'old': package(50), 'new': package(60),
        }
        original = deepcopy(packages)
        result = compare(packages, ['base', 'shared', 'base'], ['candidate'])
        self.assertEqual(result, {
            'size_scope': 'nominal package flat sizes',
            'baseline_packages': ['base', 'leaf', 'old', 'shared'],
            'candidate_packages': ['candidate', 'leaf', 'new', 'shared'],
            'added_packages': ['candidate', 'new'],
            'removed_packages': ['base', 'old'],
            'baseline_flat_bytes': 130, 'candidate_flat_bytes': 150,
            'added_flat_bytes': 80, 'removed_flat_bytes': 60,
            'net_flat_bytes': 20,
        })
        self.assertEqual(packages, original)

    def test_cycle_and_existing_dependency_have_zero_increment(self):
        packages = {'a': package(10, {'b': '1.0'}),
            'b': package(20, {'a': '1.0'})}
        result = compare(packages, ['a'], ['b', 'a'])
        self.assertEqual(result['baseline_packages'], ['a', 'b'])
        self.assertEqual(result['candidate_flat_bytes'], 30)
        self.assertEqual(result['added_packages'], [])
        self.assertEqual(result['removed_packages'], [])
        self.assertEqual(result['net_flat_bytes'], 0)

    def test_empty_roots_and_zero_byte_package(self):
        self.assertEqual(compare({}, [], [])['baseline_flat_bytes'], 0)
        result = compare({'empty': package(0)}, (), {'empty'})
        self.assertEqual(result['added_packages'], ['empty'])
        self.assertEqual(result['added_flat_bytes'], 0)
        self.assertEqual(compare({'empty': package(0)}, frozenset({'empty'}),
            [])['removed_packages'], ['empty'])

    def test_missing_or_mismatched_dependency_refuses_complete_cost(self):
        for packages in (
                {'a': package(1, {'missing': '1.0'})},
                {'a': package(1, {'b': '2.0'}), 'b': package(2)},
                {'unused': package(1, {'missing': '1.0'})}):
            with self.subTest(packages=packages), self.assertRaises(ValueError):
                compare(packages, [], [])
        with self.assertRaises(ValueError):
            compare({'a': package(1)}, ['missing'], ['a'])

    def test_invalid_package_metadata(self):
        invalid = [None, [], {'bad name': package(1)},
            {'a': None}, {'a': {}}, {'a': package(True)}, {'a': package(-1)},
            {'a': package(1.5)}, {'a': package(1, version='')},
            {'a': package(1, version='1.0\n')},
            {'a': package(1, version=1)},
            {'a': dict(version='1.0', flat_bytes=1, dependencies=[])},
            {'a': package(1, {'bad/name': '1.0'})},
            {'a': package(1, {'a': None})}]
        for packages in invalid:
            with self.subTest(packages=packages), self.assertRaises(ValueError):
                compare(packages, [], [])

    def test_root_types_and_names(self):
        packages = {'a': package(1)}
        for roots in ('a', None, {'a': '1.0'}, [None], ['bad name'],
                ['a'] * 4097):
            with self.subTest(roots=roots), self.assertRaises(ValueError):
                compare(packages, [], roots)

    def test_graph_size_and_edge_limits(self):
        packages = {f'p{i}': package(1) for i in range(4097)}
        with self.assertRaises(ValueError):
            compare(packages, [], [])
        packages = {f'p{i}': package(1) for i in range(182)}
        dependencies = {name: '1.0' for name in packages}
        for row in packages.values():
            row['dependencies'] = dependencies
        with self.assertRaises(ValueError):
            compare(packages, [], [])


if __name__ == '__main__':
    unittest.main()
