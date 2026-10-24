#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
"""Palette contrast checks independent of the retired Python controller."""

import json
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]


class DesktopAssets(unittest.TestCase):
    def test_palettes_have_readable_text_and_selection(self):
        palettes = json.loads((ROOT / 'default-config/palettes.json').read_text())
        def luminance(color):
            channels = [int(color[i:i + 2], 16) / 255 for i in (1, 3, 5)]
            linear = [c / 12.92 if c <= .04045 else ((c + .055) / 1.055) ** 2.4 for c in channels]
            return sum(c * w for c, w in zip(linear, (.2126, .7152, .0722)))
        def contrast(first, second):
            a, b = sorted((luminance(first), luminance(second)))
            return (b + .05) / (a + .05)
        self.assertEqual(set(palettes), {'slate', 'dune', 'grove', 'ink'})
        for variants in palettes.values():
            self.assertEqual(set(variants), {'light', 'dark'})
            for p in variants.values():
                self.assertGreaterEqual(contrast(p['text'], p['surface']), 4.5)
                self.assertGreaterEqual(contrast(p['muted'], p['surface']), 4.5)
                self.assertGreaterEqual(contrast(p['on_accent'], p['accent']), 4.5)


if __name__ == '__main__':
    unittest.main()
