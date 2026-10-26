#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or any later version.
"""Embedded command discovery is generated from the actual core table."""
import importlib.util
import json
import sys
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools/maintenance'))
spec = importlib.util.spec_from_file_location('fvwm_generator',
    ROOT / 'tools/maintenance/generate.py')
generator = importlib.util.module_from_spec(spec)
spec.loader.exec_module(generator)


class CommandCatalog(unittest.TestCase):
    def test_embedded_catalog_matches_core_authority(self):
        expected = generator.commands((ROOT / 'fvwm/functable.c').read_text())
        actual = json.loads((ROOT / 'bin/FvwmPrompt/internal/commandmeta/commands.json').read_text())
        self.assertEqual(actual, expected)
        self.assertGreater(len(actual), 100)
        self.assertEqual(len({entry['name'] for entry in actual}), len(actual))


if __name__ == '__main__':
    unittest.main()
