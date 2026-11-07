#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License, version 2 or later.
"""Checks for the single-architecture installed Debian package snapshot."""
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from debian_package_costs import compare_versions, parse_snapshot
from package_costs import compare


def row(name, version='1.0', size='1', depends='', predepends='', provides='',
        status='install ok installed', architecture='arm64', multiarch=''):
    return '\t'.join((name, version, size, depends, predepends, provides,
        status, architecture, multiarch)) + '\n'


class DebianPackageCosts(unittest.TestCase):
    def test_version_ordering(self):
        ordered = [
            ('1.0~rc1', '1.0'), ('1.0', '1.0-1'), ('1.0-1', '1.0-2'),
            ('1.0-2', '1.0-10'), ('1.0a', '1.0+'), ('1.0+git1', '1.0.1'),
            ('2.0', '1:1.0'), ('1:9.0', '2:1.0'),
            ('1.0~~', '1.0~'), ('1.0~rc9', '1.0~rc10'),
            ('1.0-1ubuntu1', '1.0-1ubuntu1.1'),
        ]
        for older, newer in ordered:
            with self.subTest(older=older, newer=newer):
                self.assertEqual(compare_versions(older, newer), -1)
                self.assertEqual(compare_versions(newer, older), 1)
        for left, right in [('1.0', '1.0-0'), ('01:1.00', '1:1.0'),
                ('1.001', '1.1'), ('1.0-000', '1.0')]:
            with self.subTest(left=left, right=right):
                self.assertEqual(compare_versions(left, right), 0)

    def test_closure_alternatives_exact_owner_and_predepends(self):
        text = (row('root', depends='missing | liba (>= 1.2), virtual',
                    predepends='base') +
            row('liba:arm64', version='1.3', size='2', depends='base',
                multiarch='same') +
            row('virtual', size='3') +
            row('provider', size='20', provides='virtual') +
            row('base', size='4', architecture='all'))
        snapshot = parse_snapshot(text, 'arm64')
        packages = snapshot['packages']
        self.assertEqual(packages['root']['dependencies'],
            {'liba:arm64': '1.3', 'virtual': '1.0', 'base': '1.0'})
        result = compare(packages, ['base'], ['root'])
        self.assertEqual(result['added_packages'],
            ['liba:arm64', 'root', 'virtual'])
        self.assertEqual(result['added_flat_bytes'], 6 * 1024)
        resolutions = snapshot['resolutions']['root']
        self.assertEqual(resolutions[0]['alternative'], 'liba (>= 1.2)')
        self.assertEqual(resolutions[0]['package'], 'liba:arm64')
        self.assertEqual(resolutions[1]['via'], 'package')
        self.assertEqual(resolutions[2]['field'], 'Pre-Depends')

    def test_first_satisfied_alternative_is_recorded(self):
        text = row('root', depends='zlib | alib') + row('zlib') + row('alib')
        snapshot = parse_snapshot(text, 'arm64')
        self.assertEqual(snapshot['packages']['root']['dependencies'],
            {'zlib': '1.0'})
        self.assertEqual(snapshot['resolutions']['root'][0]['alternative'],
            'zlib')
        with self.assertRaisesRegex(ValueError, 'foreign dependency'):
            parse_snapshot(row('root', depends='zlib | alib:amd64') +
                row('zlib') + row('alib'), 'arm64')

    def test_versioned_provider_and_all_relations(self):
        text = (row('root', depends='abi (>= 2), abi (<< 3), abi (>> 1), '
                    'abi (<= 2), abi (= 2)') +
            row('implementation', version='9.0', provides='abi (= 1), '
                'abi (= 2)'))
        snapshot = parse_snapshot(text, 'arm64')
        self.assertEqual(snapshot['packages']['root']['dependencies'],
            {'implementation': '9.0'})
        for resolution in snapshot['resolutions']['root']:
            self.assertEqual(resolution['via'], 'Provides')
        self.assertEqual(snapshot['resolutions']['root'][0]['provided_version'],
            '2')

    def test_unversioned_provider_does_not_satisfy_versioned_requirement(self):
        with self.assertRaisesRegex(ValueError, 'unsatisfied dependency'):
            parse_snapshot(row('root', depends='abi (>= 1)') +
                row('implementation', provides='abi'), 'arm64')

    def test_ambiguous_providers_refused_but_exact_package_preferred(self):
        text = (row('root', depends='virtual') +
            row('one', provides='virtual') + row('two', provides='virtual'))
        with self.assertRaisesRegex(ValueError, 'ambiguous provider'):
            parse_snapshot(text, 'arm64')
        snapshot = parse_snapshot(text + row('virtual'), 'arm64')
        self.assertEqual(snapshot['packages']['root']['dependencies'],
            {'virtual': '1.0'})

    def test_architecture_qualifiers_require_explicit_metadata(self):
        text = row('root', depends='interpreter:any, lib:native, lib:arm64')
        text += row('interpreter', multiarch='allowed')
        text += row('lib:arm64', multiarch='same')
        snapshot = parse_snapshot(text, 'arm64')
        self.assertEqual(snapshot['packages']['root']['dependencies'],
            {'interpreter': '1.0', 'lib:arm64': '1.0'})
        with self.assertRaisesRegex(ValueError, 'unsatisfied dependency'):
            parse_snapshot(row('root', depends='interpreter:any') +
                row('interpreter'), 'arm64')
        for text in (row('root', depends='lib:amd64') + row('lib'),
                row('foreign', architecture='amd64'),
                row('lib:amd64', architecture='arm64'),
                row('lib', multiarch='unknown')):
            with self.subTest(text=text), self.assertRaises(ValueError):
                parse_snapshot(text, 'arm64')

    def test_missing_or_bad_data_refuses_even_unrelated_roots(self):
        invalid = [
            '', row('root', depends='absent'), row('root') + row('root'),
            row('root', size='-1'), row('root', size='1.5'),
            row('root', size='01'), row('root', status='deinstall ok config-files'),
            row('root', depends='base [arm64]') + row('base'),
            row('root', depends='base <!nocheck>') + row('base'),
            row('root', depends='base |') + row('base'),
            row('root', depends='base (>=)') + row('base'),
            row('root', provides='abi (>= 1)'),
            row('root', provides='abi:any'),
            row('root', version='not-a-version'),
            row('root', architecture=''),
            row('root').rstrip('\n'), row('root').replace('\tarm64\t', '\t'),
            row('root').replace('root', 'røot', 1),
            row('root').replace('\n', '\r\n'),
            row('root', depends='base\x7f') + row('base'),
            row('base:all', architecture='all'),
            row('base', architecture='all', multiarch='same'),
        ]
        for text in invalid:
            with self.subTest(text=text), self.assertRaises(ValueError):
                parse_snapshot(text, 'arm64')
        for version in ('', 'a1', '1:', 'x:1', '1-foo_bar', '1\n', None):
            with self.subTest(version=version), self.assertRaises(ValueError):
                compare_versions(version, '1')

    def test_bounded_input_and_relation_work(self):
        for text in ('x' * (16 * 1024 * 1024 + 1),
                row('root', depends='a' * 65537),
                ''.join(row('p' + str(i)) for i in range(4097)),
                row('root', depends=', '.join(['base'] * 32769)) +
                    row('base')):
            with self.assertRaises(ValueError):
                parse_snapshot(text, 'arm64')
        with self.assertRaises(ValueError):
            parse_snapshot(row('root', size=str(1 << 63)), 'arm64')


if __name__ == '__main__':
    unittest.main()
