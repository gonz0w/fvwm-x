# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or any later version.
"""Incremental option policy and real Meson compile/test/install ordering."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import build


class BuildReusePolicyTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='fvwm build reuse ')
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.source = self.root / 'source'
        self.source.mkdir()
        self.builddir = self.root / 'build'
        private = self.builddir / 'meson-private'
        private.mkdir(parents=True)
        (private / 'coredata.dat').touch()
        self.options = self.builddir / 'meson-info/intro-buildoptions.json'
        self.options.parent.mkdir()
        self.info = self.options.parent / 'meson-info.json'
        self.info.write_text(json.dumps({'directories': {
            'source': str(self.source), 'build': str(self.builddir)}, 'error': False}))
        self.write_options()

    def args(self, **changes):
        args = argparse.Namespace(source=self.source, builddir=self.builddir,
                                  meson='meson', reconfigure=False, wipe=False,
                                  define=[], prefix=None, jobs=2, test=False,
                                  install=False)
        vars(args).update(changes)
        return args

    def write_options(self):
        self.options.write_text(json.dumps([
            {'name': 'flag', 'value': True},
            {'name': 'count', 'value': 4},
            {'name': 'mode', 'value': 'disabled'},
            {'name': 'prefix', 'value': '/prefix with spaces'},
            {'name': 'items', 'value': ['a', 'b']},
            {'name': 'flag', 'value': False, 'subproject': 'child'},
        ]))

    def test_identical_scalars_skip_setup_and_keep_requested_parallelism(self):
        commands = build.plan(self.args(define=['flag=true', 'count=4', 'mode=disabled'], jobs=3))
        self.assertEqual([argv[1] for argv in commands], ['compile'])
        self.assertEqual(commands[0][-2:], ['-j', '3'])

    def test_changed_unknown_and_array_options_use_ordinary_setup(self):
        for definition in ('flag=false', 'count=5', 'unknown=true', "items=['a', 'b']"):
            with self.subTest(definition=definition):
                commands = build.plan(self.args(define=[definition]))
                self.assertEqual(commands[0][1], 'setup')
                self.assertIn('--reconfigure', commands[0])
                self.assertIn('-D' + definition, commands[0])
                self.assertNotIn('--wipe', commands[0])

    def test_final_definition_and_prefix_order_is_preserved(self):
        args = self.args(define=['flag=false', 'flag=true', 'prefix=/earlier'],
                         prefix='/prefix with spaces')
        self.assertEqual([argv[1] for argv in build.plan(args)], ['compile'])
        args.prefix = '/different'
        setup = build.plan(args)[0]
        self.assertEqual(setup[-2:], ['--prefix', '/different'])

    def test_subproject_options_do_not_collide_with_parent(self):
        self.assertEqual([argv[1] for argv in build.plan(self.args(define=['child:flag=false']))], ['compile'])
        self.assertEqual(build.plan(self.args(define=['child:flag=true']))[0][1], 'setup')

    def test_missing_malformed_or_unrecognized_metadata_retains_setup(self):
        for text in ('{', 'null', '[{}]', '{"wrong": []}'):
            with self.subTest(text=text):
                self.options.write_text(text)
                self.assertEqual(build.plan(self.args(define=['flag=true']))[0][1], 'setup')
        self.options.unlink()
        self.assertEqual(build.plan(self.args(define=['flag=true']))[0][1], 'setup')

    def test_invalid_definition_reaches_meson_instead_of_being_ignored(self):
        for definition in ('flag', '=value'):
            with self.subTest(definition=definition):
                commands = build.plan(self.args(define=[definition]))
                self.assertEqual(commands[0][1], 'setup')
                self.assertIn('-D' + definition, commands[0])

    def test_explicit_reconfigure_and_wipe_are_never_skipped(self):
        for changes in ({'reconfigure': True}, {'wipe': True}):
            with self.subTest(changes=changes):
                setup = build.plan(self.args(define=['flag=true'], **changes))[0]
                self.assertEqual(setup[1], 'setup')
                self.assertIn('--reconfigure', setup)
                self.assertEqual('--wipe' in setup, bool(changes.get('wipe')))

    def test_foreign_or_failed_build_metadata_never_skips_setup(self):
        for info in ({'directories': {'source': str(self.root / 'other source'),
                                      'build': str(self.builddir)}},
                     {'directories': {'source': str(self.source),
                                      'build': str(self.root / 'other build')}},
                     {'directories': {'source': str(self.source),
                                      'build': str(self.builddir)}, 'error': True},
                     None):
            with self.subTest(info=info):
                self.info.write_text(json.dumps(info))
                self.assertEqual(build.plan(self.args(define=['flag=true']))[0][1], 'setup')
        self.info.unlink()
        self.assertEqual(build.plan(self.args(define=['flag=true']))[0][1], 'setup')

    def test_new_build_always_configures_before_compile(self):
        shutil.rmtree(self.builddir)
        self.assertEqual([argv[1] for argv in build.plan(self.args(define=['flag=true']))], ['setup', 'compile'])

    def test_compile_is_the_build_barrier_for_test_and_install(self):
        commands = build.plan(self.args(test=True, install=True))
        self.assertEqual([argv[1] for argv in commands], ['compile', 'test', 'install'])
        self.assertNotIn('--no-rebuild', commands[1])
        self.assertIn('--no-rebuild', commands[2])


@unittest.skipUnless(shutil.which('meson') and shutil.which('cc'),
                     'real Meson/compiler integration requires native tools')
class BuildReuseMesonTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='fvwm meson reuse ')
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.source = self.root / 'source with spaces'
        self.source.mkdir()
        self.builddir = self.root / 'build with spaces'
        self.prefix = self.root / 'install with spaces'
        self.log = self.root / 'commands.log'
        (self.source / 'meson.build').write_text(
            "project('reuse-fixture', 'c', default_options: ['c_std=c11'])\n"
            "program = executable('sample', 'sample.c', install: true)\n"
            "test('sample', program)\n"
            "test_only = executable('test-only', 'sample.c', build_by_default: false)\n"
            "test('test-only', test_only)\n")
        (self.source / 'meson.options').write_text(
            "option('flag', type: 'boolean', value: true)\n")
        self.program = self.source / 'sample.c'
        self.program.write_text('int main(void) { return 0; }\n')
        self.command = [sys.executable, str(Path(build.__file__)),
                        '--source', str(self.source), '--builddir', str(self.builddir),
                        '--prefix', str(self.prefix), '-D', 'flag=true', '--jobs', '2',
                        '--log', str(self.log), '--test', '--install']

    def run_wrapper(self, flags=(), success=True):
        result = subprocess.run(self.command + list(flags), capture_output=True, text=True)
        if success:
            self.assertEqual(result.returncode, 0, self.log.read_text() if self.log.exists() else result.stderr)
        else:
            self.assertNotEqual(result.returncode, 0)
        return result

    def setup_count(self):
        return sum(line.startswith('$ ') and ' setup ' in line
                   for line in self.log.read_text().splitlines())

    def test_reuse_changed_options_and_meson_regeneration(self):
        self.run_wrapper()
        installed = self.prefix / 'bin/sample'
        self.assertTrue((self.builddir / 'test-only').is_file())
        self.assertFalse((self.prefix / 'bin/test-only').exists())
        initial = hashlib.sha256(installed.read_bytes()).hexdigest()
        self.run_wrapper()
        self.assertEqual(self.setup_count(), 1)
        self.assertEqual(hashlib.sha256(installed.read_bytes()).hexdigest(), initial)
        # Ninja must still regenerate changed build descriptions on a reused
        # tree even though explicit option values themselves are unchanged.
        (self.source / 'meson.build').write_text((self.source / 'meson.build').read_text() + "message('regeneration observed')\n")
        self.run_wrapper()
        self.assertEqual(self.setup_count(), 1)
        self.assertIn('regeneration observed', self.log.read_text())
        self.run_wrapper(flags=('-D', 'flag=false'))
        self.assertEqual(self.setup_count(), 2)
        options = json.loads((self.builddir / 'meson-info/intro-buildoptions.json').read_text())
        self.assertFalse(next(entry['value'] for entry in options if entry['name'] == 'flag'))
        self.run_wrapper(flags=('-D', 'flag=false', '--reconfigure'))
        self.assertEqual(self.setup_count(), 3)

    def test_failed_test_preserves_previously_installed_binary(self):
        self.run_wrapper()
        installed = self.prefix / 'bin/sample'
        previous = hashlib.sha256(installed.read_bytes()).hexdigest()
        self.program.write_text('int main(void) { return 1; }\n')
        self.run_wrapper(success=False)
        self.assertEqual(hashlib.sha256(installed.read_bytes()).hexdigest(), previous)
        self.assertNotEqual(hashlib.sha256((self.builddir / 'sample').read_bytes()).hexdigest(), previous)
        commands = [line for line in self.log.read_text().splitlines() if line.startswith('$ ')]
        self.assertIn(' test ', commands[-1])
        self.assertNotIn('--no-rebuild', commands[-1])


if __name__ == '__main__':
    unittest.main()
