# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or any later version.
"""The maintained language list and complete multi-catalog publication."""
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from common import ROOT
import translations


class TranslationTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='fvwm catalogs ')
        self.root = Path(self.temp.name)
        self.source = self.root / 'source'
        po = self.source / 'po'
        po.mkdir(parents=True)
        (po / 'LINGUAS').write_text('# shared list\nfr ka # comment\n')
        for language in ('fr', 'ka'):
            (po / f'{language}.po').write_text('original ' + language)
        (po / 'fvwm3.pot').write_text('template')

    def tearDown(self):
        self.temp.cleanup()

    def test_discovery_uses_meson_list_and_preserves_order(self):
        self.assertEqual(translations.languages(self.source), ['fr', 'ka'])
        self.assertIn('ka', translations.languages(ROOT))
        result = subprocess.run([
            sys.executable, str(ROOT / 'tools/maintenance/translations.py'),
            'languages', '--source', str(self.source)],
            capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(result.stdout.splitlines(), ['fr', 'ka'])

    def test_discovery_requires_complete_unique_catalog_list(self):
        for content in ('fr fr', '# no catalogs', 'fr missing'):
            (self.source / 'po/LINGUAS').write_text(content)
            with self.assertRaises(ValueError):
                translations.languages(self.source)

    def test_all_build_publishes_complete_catalogs_and_records_selection(self):
        commands = []

        def compile_catalog(command, **kwargs):
            commands.append(command)
            Path(command[command.index('-o') + 1]).write_bytes(b'catalog')

        output = self.root / 'catalogs'
        with patch('translations.subprocess.run', side_effect=compile_catalog):
            translations.maintain(self.source, output, 'build',
                                  use_fuzzy=True, all_languages=True)
        self.assertEqual(sorted(p.name for p in output.glob('*.gmo')),
                         ['fvwm3.fr.gmo', 'fvwm3.ka.gmo'])
        self.assertTrue(all('--use-fuzzy' in command for command in commands))
        record = json.loads((output / 'operation.json').read_text())
        self.assertEqual(record['languages'], ['fr', 'ka'])

    def test_all_update_keeps_sources_and_discards_partial_failure(self):
        originals = {p.name: hashlib.sha256(p.read_bytes()).hexdigest()
                     for p in (self.source / 'po').glob('*.po')}
        calls = []

        def update_catalog(command, **kwargs):
            calls.append(command)
            self.assertIn('--no-fuzzy-matching', command)
            Path(command[command.index('-o') + 1]).write_text('candidate')
            if len(calls) == 2:
                raise subprocess.CalledProcessError(2, command)

        output = self.root / 'updated'
        with patch('translations.subprocess.run', side_effect=update_catalog):
            with self.assertRaises(subprocess.CalledProcessError):
                translations.maintain(self.source, output, 'update',
                                      all_languages=True)
        self.assertEqual(len(calls), 2)
        self.assertFalse(output.exists())
        self.assertFalse(list(self.root.glob('.fvwm-maint-*')))
        self.assertEqual(originals, {
            p.name: hashlib.sha256(p.read_bytes()).hexdigest()
            for p in (self.source / 'po').glob('*.po')})

    def test_selection_errors_do_not_create_output(self):
        output = self.root / 'output'
        for action, language, all_languages in (
                ('build', None, False), ('pot', None, True),
                ('init', None, True), ('build', 'fr', True)):
            with self.assertRaises(ValueError):
                translations.maintain(self.source, output, action, language,
                                      all_languages=all_languages)
            self.assertFalse(output.exists())


if __name__ == '__main__':
    unittest.main()
