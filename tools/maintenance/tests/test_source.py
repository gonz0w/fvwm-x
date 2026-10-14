# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or any later version.
"""Source-tool policy and real Git publication/failure regressions."""
import hashlib
import io
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tarfile
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import source


class SourcePolicyTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='fvwm source policy ')
        self.root = Path(self.temp.name)

    def tearDown(self):
        self.temp.cleanup()

    def go_fixture(self, text):
        path = self.root / 'bin/FvwmPrompt/go.mod'
        path.parent.mkdir(parents=True)
        path.write_text(text)
        return path

    def fake_git(self, root, *args):
        if args[0] == 'ls-files':
            return b'large file\0link\0deleted\0'
        if args[0] == 'rev-parse':
            return b'a' * 40 + b'\n'
        return b''

    def test_hashing_bounds_reads_and_preserves_exact_bytes(self):
        data = bytes(range(256)) * 5000
        class BoundedStream(io.BytesIO):
            def read(self, size=-1):
                if size < 0 or size > 128 * 1024:
                    raise AssertionError('unbounded source read')
                return super().read(size)
        class Input:
            def open(self, mode):
                return BoundedStream(data)
        self.assertEqual(source.file_identity(Input()),
                         {'bytes': len(data), 'sha256': hashlib.sha256(data).hexdigest()})

    def test_inventory_streams_regular_files_and_never_follows_link_targets(self):
        data = b'large payload\0' * 100000
        (self.root / 'large file').write_bytes(data)
        (self.root / 'link').symlink_to('/absent target')
        with patch('source.git', side_effect=self.fake_git), \
                patch.object(Path, 'read_bytes', side_effect=AssertionError('whole-file read')):
            entries = {item['path']: item for item in source.inventory(self.root)['files']}
        self.assertEqual(entries['large file']['sha256'], hashlib.sha256(data).hexdigest())
        self.assertEqual(entries['link']['sha256'], hashlib.sha256(b'/absent target').hexdigest())
        self.assertEqual(entries['deleted']['state'], 'deleted')

    def test_missing_pkg_config_is_explicit_and_inventory_survives(self):
        with patch('source.git', side_effect=self.fake_git), \
                patch('source.subprocess.run', side_effect=FileNotFoundError('pkg-config')):
            result = source.inventory(self.root, dependencies=True)
        self.assertFalse(result['pkg_config_available'])
        self.assertEqual(set(result['pkg_config']), set(source.DEPENDENCIES))
        self.assertTrue(all(value is None for value in result['pkg_config'].values()))

    def test_pkg_config_package_absence_differs_from_missing_executable(self):
        with patch('source.git', side_effect=self.fake_git), \
                patch('source.subprocess.run', return_value=subprocess.CompletedProcess([], 1, '', 'missing')):
            result = source.inventory(self.root, dependencies=True)
        self.assertTrue(result['pkg_config_available'])
        self.assertTrue(all(value is None for value in result['pkg_config'].values()))

    def test_go_directives_keep_provenance_and_local_replacement_as_data(self):
        path = self.go_fixture('module fixture\ngo 1.26\ntoolchain go1.26.2\n'
                               'require ( // declarations\n\texample.org/a v1.2.3 // indirect\n)\n'
                               'replace example.org/a => "../local modules/a"\n'
                               'exclude (\n example.org/b v2.0.0\n)\n')
        result = source.go_metadata(self.root, 'bin/FvwmPrompt/go.mod')
        directives = result['directives']
        self.assertEqual([item['kind'] for item in directives],
                         ['module', 'go', 'toolchain', 'require', 'replace', 'exclude'])
        self.assertEqual([item['line'] for item in directives], [1, 2, 3, 5, 7, 9])
        self.assertEqual(directives[4]['text'], 'example.org/a => "../local modules/a"')
        self.assertEqual(result['sha256'], hashlib.sha256(path.read_bytes()).hexdigest())
        self.assertIn('not a resolved module graph', result['note'])

    def test_go_companion_identities_and_legacy_requirement_map(self):
        path = self.go_fixture('module fixture\ngo 1.26\nrequire example.org/a v1.2.3\n')
        (path.parent / 'go.sum').write_text('checksum fixture\n')
        (path.parent / 'vendor').mkdir()
        (path.parent / 'vendor/modules.txt').write_text('# example.org/a v1.2.3\n')
        result = source.maintenance_metadata(self.root)
        self.assertEqual(result['vendored']['go_modules'], {'example.org/a': 'v1.2.3'})
        companions = result['go_manifest']['companions']
        self.assertEqual([item['path'] for item in companions],
                         ['bin/FvwmPrompt/go.sum', 'bin/FvwmPrompt/vendor/modules.txt'])
        for item in companions:
            self.assertEqual(item['sha256'], hashlib.sha256((self.root / item['path']).read_bytes()).hexdigest())

    def test_missing_go_manifest_or_companions_is_an_observation(self):
        self.assertNotIn('go_manifest', source.maintenance_metadata(self.root))
        self.go_fixture('module fixture\n')
        self.assertEqual(source.go_metadata(self.root, 'bin/FvwmPrompt/go.mod')['companions'], [])

    def test_unfinished_go_block_is_recorded_without_asserting_validation(self):
        self.go_fixture('require (\n example.org/a v1.2.3\n')
        self.assertEqual(source.go_metadata(self.root, 'bin/FvwmPrompt/go.mod')['unfinished_block'], 'require')

    def test_linked_go_companion_refused(self):
        path = self.go_fixture('module fixture\n')
        (path.parent / 'go.sum').symlink_to(path)
        with self.assertRaises(ValueError):
            source.go_metadata(self.root, 'bin/FvwmPrompt/go.mod')

    def test_git_archive_failure_does_not_publish_partial_output(self):
        output = self.root / 'output'
        with patch('source.git', side_effect=self.fake_git), \
                patch('source.subprocess.run', side_effect=subprocess.CalledProcessError(7, ['git', 'archive'])):
            with self.assertRaises(subprocess.CalledProcessError):
                source.archive(self.root, output)
        self.assertFalse(output.exists())
        self.assertEqual(list(self.root.glob('.fvwm-maint-*')), [])

    def test_existing_destination_is_preserved_before_git_archive_runs(self):
        output = self.root / 'output'
        output.mkdir()
        (output / 'evidence').write_text('retain')
        with patch('source.git', side_effect=self.fake_git), patch('source.subprocess.run') as run:
            with self.assertRaises(ValueError):
                source.archive(self.root, output)
            run.assert_not_called()
        self.assertEqual((output / 'evidence').read_text(), 'retain')

    def test_negative_epoch_is_rejected_by_programmatic_api(self):
        with self.assertRaises(ValueError):
            source.archive(self.root, self.root / 'output', epoch=-1)
        self.assertFalse((self.root / 'output').exists())


@unittest.skipUnless(shutil.which('git'), 'real Git controls require Git')
class SourceGitTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='fvwm source git ')
        self.root = Path(self.temp.name) / 'checkout'
        self.root.mkdir()
        subprocess.run(['git', 'init', '-q', str(self.root)], check=True)
        self.data = bytes(range(256)) * 20000
        (self.root / 'large file').write_bytes(self.data)
        (self.root / 'executable').write_text('script\n')
        (self.root / 'executable').chmod(0o755)
        (self.root / 'link').symlink_to('large file')
        (self.root / '.current-version').write_text('test\n')
        subprocess.run(['git', '-C', str(self.root), 'add', '.'], check=True)
        subprocess.run(['git', '-C', str(self.root), '-c', 'commit.gpgsign=false',
                        '-c', 'user.name=Fixture', '-c', 'user.email=fixture@example.invalid',
                        'commit', '-qm', 'fixture'], check=True)

    def tearDown(self):
        self.temp.cleanup()

    def test_archive_streams_large_payload_and_manifest_hash_matches(self):
        output = Path(self.temp.name) / 'archive'
        with patch.object(Path, 'read_bytes', side_effect=AssertionError('whole-file read')):
            source.archive(self.root, output, epoch=123)
        manifest = json.loads((output / 'archive.json').read_text())
        target = output / 'source.tar.gz'
        self.assertEqual(manifest['sha256'], hashlib.sha256(target.read_bytes()).hexdigest())
        self.assertEqual(manifest['bytes'], target.stat().st_size)
        with tarfile.open(target) as archive:
            self.assertEqual(archive.extractfile('large file').read(), self.data)
            self.assertEqual(archive.getmember('executable').mode, 0o755)
            self.assertEqual(archive.getmember('link').linkname, 'large file')
            self.assertTrue(all(item.mtime == 123 for item in archive.getmembers()))

    def test_dirty_development_archive_contains_commit_not_worktree(self):
        (self.root / 'large file').write_text('uncommitted change')
        with self.assertRaises(ValueError):
            source.archive(self.root, Path(self.temp.name) / 'refused')
        output = Path(self.temp.name) / 'development'
        source.archive(self.root, output, allow_dirty=True)
        self.assertTrue(json.loads((output / 'archive.json').read_text())['development_only'])
        with tarfile.open(output / 'source.tar.gz') as archive:
            self.assertEqual(archive.extractfile('large file').read(), self.data)

    def test_invalid_commit_does_not_create_output(self):
        output = Path(self.temp.name) / 'output'
        with self.assertRaises(subprocess.CalledProcessError):
            source.archive(self.root, output, ref='no-such-ref')
        self.assertFalse(output.exists())


if __name__ == '__main__':
    unittest.main()
