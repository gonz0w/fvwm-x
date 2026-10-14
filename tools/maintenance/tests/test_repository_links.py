# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or any later version.
"""Offline documentation link checks and current/history scope regressions."""
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from repository_links import check_markdown_links, documentation_paths, markdown_links


class RepositoryLinkTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='fvwm doc links ')
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)

    def write(self, name, text=''):
        path = self.root / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text)
        return path

    def test_relative_files_images_directories_and_fragments(self):
        self.write('code/file.c', 'source')
        self.write('images/logo.png', 'fixture')
        page = self.write('dev-docs/page.md', '[source](../code/file.c#L12)\n'
                          '![image](../images/logo.png) [directory](../code)\n'
                          '[section](#local-section)\n')
        self.assertEqual(check_markdown_links(self.root, [page]), [])

    def test_missing_target_reports_origin_and_literal_link(self):
        page = self.write('README.md', '[missing](dev-docs/missing.md#section)')
        errors = check_markdown_links(self.root, [page])
        self.assertEqual(len(errors), 1)
        self.assertIn('README.md: dev-docs/missing.md#section', errors[0])

    def test_angle_targets_percent_encoding_and_titles(self):
        self.write('space name.md')
        self.write('hash#name.md')
        page = self.write('README.md', '[angle](<space name.md> "a title")\n'
                          '[escaped](space%20name.md \'another title\')\n'
                          '[hash](hash%23name.md)\n')
        self.assertEqual(check_markdown_links(self.root, [page]), [])

    def test_balanced_and_escaped_parenthesis_targets(self):
        self.write('name(a).md')
        page = self.write('README.md', '[balanced](name(a).md)\n'
                          r'[escaped](name\(a\).md)' + '\n')
        self.assertEqual(list(markdown_links(page.read_text())), ['name(a).md', 'name(a).md'])
        self.assertEqual(check_markdown_links(self.root, [page]), [])

    def test_code_examples_do_not_create_false_missing_links(self):
        text = ('```sh\n[missing](not-a-file)\n```\n'
                '~~~python\n[missing](also-not-a-file)\n~~~\n'
                '    [missing](indented-example)\n'
                '`[missing](inline-example)`\n'
                '``[missing](double-tick-example)``\n')
        page = self.write('README.md', text)
        self.assertEqual(list(markdown_links(text)), [])
        self.assertEqual(check_markdown_links(self.root, [page]), [])

    def test_long_fence_is_not_closed_by_shorter_marker(self):
        text = '````text\n```\n[missing](example)\n````\n[real](real.md)\n'
        self.assertEqual(list(markdown_links(text)), ['real.md'])

    def test_remote_and_uri_links_are_not_fetched(self):
        page = self.write('README.md', '[remote](https://invalid.example/page)\n'
                          '[contact](mailto:owner@example.invalid)\n'
                          '[reference](codex://review?pr=fixture)\n'
                          '[network](//invalid.example/page)\n')
        self.assertEqual(check_markdown_links(self.root, [page]), [])

    def test_external_parent_and_absolute_targets_are_refused(self):
        for target in ('../outside.md', '/outside.md', '%2e%2e/outside.md'):
            with self.subTest(target=target):
                page = self.write('README.md', '[outside](' + target + ')')
                errors = check_markdown_links(self.root, [page])
                self.assertEqual(len(errors), 1)
                self.assertIn('escapes repository', errors[0])

    def test_symlink_target_cannot_escape_repository(self):
        (self.root / 'outside').symlink_to(self.root.parent / 'not-owned')
        page = self.write('README.md', '[link](outside)')
        errors = check_markdown_links(self.root, [page])
        self.assertEqual(len(errors), 1)
        self.assertIn('escapes repository', errors[0])

    def test_linked_document_input_is_not_read(self):
        target = self.write('original.md', '[missing](target)')
        link = self.root / 'README.md'
        link.symlink_to(target)
        errors = check_markdown_links(self.root, [link])
        self.assertEqual(len(errors), 1)
        self.assertIn('unreadable documentation', errors[0])

    def test_invalid_encoded_target_is_diagnosed(self):
        page = self.write('README.md', '[invalid](%00bad)')
        errors = check_markdown_links(self.root, [page])
        self.assertEqual(len(errors), 1)
        self.assertIn('invalid local target', errors[0])

    def test_current_guides_and_history_index_are_checked(self):
        self.write('README.md')
        self.write('dev-docs/decisions/contract.md')
        self.write('dev-docs/verification/report.md')
        self.write('tools/maintenance/README.md')
        self.write('.github/PULL_REQUEST_TEMPLATE.md')
        self.write('dev-docs/history/README.md')
        self.write('dev-docs/history/planning/old.md', '[old](deleted-input)')
        self.write('bin/vendor/README.md', '[vendor](upstream-reference)')
        names = [path.relative_to(self.root).as_posix() for path in documentation_paths(self.root)]
        self.assertEqual(len(names), 6)
        self.assertNotIn('dev-docs/history/planning/old.md', names)
        self.assertNotIn('bin/vendor/README.md', names)
        self.assertEqual(check_markdown_links(self.root), [])

    def test_unreadable_input_has_an_explicit_error(self):
        errors = check_markdown_links(self.root, [self.root / 'missing.md'])
        self.assertEqual(len(errors), 1)
        self.assertIn('unreadable documentation', errors[0])

    def test_relative_input_paths_are_based_on_selected_repository(self):
        self.write('dev-docs/page.md', '[source](../source.c)')
        self.write('source.c')
        self.assertEqual(check_markdown_links(self.root, [Path('dev-docs/page.md')]), [])

    def test_cyclic_target_is_diagnosed_without_aborting_other_checks(self):
        (self.root / 'cycle').symlink_to('cycle')
        page = self.write('README.md', '[cycle](cycle)\n[missing](missing.md)')
        errors = check_markdown_links(self.root, [page])
        self.assertEqual(len(errors), 2)
        self.assertIn('cycle', errors[0])
        self.assertIn('missing local target', errors[1])

    def test_unfinished_inline_syntax_is_not_claimed_as_a_link(self):
        self.assertEqual(list(markdown_links('[text](unfinished')), [])
        self.assertEqual(list(markdown_links('[text](<unfinished')), [])


if __name__ == '__main__':
    unittest.main()
