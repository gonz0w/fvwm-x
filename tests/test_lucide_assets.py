#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License, version 2 or later.
"""Verify the approved subset and reversible derivation without network access."""
import configparser
import hashlib
import json
from pathlib import Path
import unittest
import xml.etree.ElementTree as ET

SOURCE = Path(__file__).resolve().parents[1]
THEME = SOURCE / 'default-config/icons/FVWM-X-Lucide'
NAMES = set('folder folder-open file file-text image clipboard copy scissors '
    'clipboard-paste search refresh-cw arrow-left arrow-right arrow-up settings '
    'trash'.split())
COMMIT = '4b203ec9b6002fd74fd5dadc8612220e64289305'
LICENSE_SHA = 'b495047bd93a9b06913511076f504daba17d5bbeb3e0650f3bb53a4220329c57'


class LucideAssets(unittest.TestCase):
    def test_source_parity_and_complete_notice(self):
        manifest = json.loads((THEME / 'provenance.json').read_text())
        self.assertEqual(manifest['upstream_commit'], COMMIT)
        self.assertEqual(set(manifest['icons']), NAMES)
        self.assertEqual({p.stem for p in THEME.rglob('*.svg')}, NAMES)
        self.assertEqual(hashlib.sha256((THEME / 'LICENSE').read_bytes()).hexdigest(),
            LICENSE_SHA)
        for name, row in manifest['icons'].items():
            with self.subTest(name=name):
                raw = (THEME / 'scalable/actions' / (name + '.svg')).read_bytes()
                self.assertEqual(hashlib.sha256(raw).hexdigest(), row['derived_sha256'])
                self.assertEqual(raw.count(b'stroke="#000000"'), 1)
                original = raw.replace(b'stroke="#000000"', b'stroke="currentColor"')
                self.assertEqual(hashlib.sha256(original).hexdigest(), row['upstream_sha256'])
                self.assertEqual(row['source_url'], 'https://raw.githubusercontent.com/'
                    'lucide-icons/lucide/' + COMMIT + '/icons/' + name + '.svg')
                svg = ET.fromstring(raw)
                self.assertEqual(svg.attrib['viewBox'], '0 0 24 24')
                self.assertEqual(svg.attrib['fill'], 'none')
                self.assertEqual(svg.attrib['stroke-width'], '2')
                self.assertEqual(svg.attrib['stroke-linecap'], 'round')
                self.assertEqual(svg.attrib['stroke-linejoin'], 'round')

    def test_scalable_index_and_content_budget(self):
        index = configparser.ConfigParser(interpolation=None)
        index.read(THEME / 'index.theme')
        self.assertEqual(index['Icon Theme']['Directories'], 'scalable/actions')
        directory = index['scalable/actions']
        self.assertEqual(directory['Type'], 'Scalable')
        self.assertEqual(int(directory['Size']), 24)
        self.assertLessEqual(int(directory['MinSize']), 8)
        self.assertGreaterEqual(int(directory['MaxSize']), 96)
        files = [p for p in THEME.rglob('*') if p.is_file()]
        # Include the configured example's source bytes conservatively. Actual
        # prefix-dependent installed bytes are measured at the shared checkpoint.
        total = sum(p.stat().st_size for p in files)
        total += (SOURCE / 'default-config/lucide-menus.fvwm.in').stat().st_size
        self.assertLessEqual(total, 32768)


if __name__ == '__main__':
    unittest.main()
