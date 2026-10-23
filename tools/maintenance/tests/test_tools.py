# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or any later version.
"""Failure, data-preservation, and current-source contracts for new tooling."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tarfile
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import build
from common import ROOT, fresh_output, source_file
import docs
import generate
import source
import translations


class ToolTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='fvwm tooling space ')
        self.root = Path(self.temp.name)

    def tearDown(self):
        self.temp.cleanup()

    def executable(self, name, body):
        path = self.root / name
        path.write_text('#!' + sys.executable + '\n' + body)
        path.chmod(0o755)
        return str(path)

    def build_args(self, **changes):
        args = argparse.Namespace(source=ROOT, builddir=self.root / 'build dir',
                                  meson='meson', reconfigure=False, wipe=False, define=[],
                                  prefix=None, jobs=2, test=False, install=False)
        vars(args).update(changes)
        return args

    def test_build_preserves_argv_and_has_no_default_install_or_wipe(self):
        command = build.plan(self.build_args(prefix='/space prefix', define=['foo=a b']))
        self.assertIn('-Dfoo=a b', command[0])
        self.assertIn('/space prefix', command[0])
        self.assertEqual([item[1] for item in command], ['setup', 'compile'])
        self.assertNotIn('--wipe', command[0])

    def test_build_reconfigure_preserves_existing_data(self):
        args = self.build_args(define=['bidi=disabled'])
        private = args.builddir / 'meson-private'
        private.mkdir(parents=True)
        (private / 'coredata.dat').touch()
        marker = args.builddir / 'my-results'
        marker.write_text('evidence')
        self.assertIn('--reconfigure', build.plan(args)[0])
        self.assertEqual(marker.read_text(), 'evidence')
        self.assertEqual(len(build.plan(self.build_args())), 1)

    def test_build_refuses_non_meson_data_and_source(self):
        (self.root / 'evidence').touch()
        with self.assertRaises(ValueError):
            build.plan(self.build_args(builddir=self.root))
        with self.assertRaises(ValueError):
            build.plan(self.build_args(builddir=ROOT))

    def test_legacy_build_options_preserve_order_and_explicit_wipe(self):
        argv = build.legacy_arguments([
            '-d', '-p', '/prefix with spaces', '-Dprefix=/last prefix',
            '-Dmandoc=false', '-b', 'build with spaces', '-fvi', '--jobs', '2'])
        self.assertEqual(argv, [
            '--builddir', 'compile', '-D', 'mandoc=true', '-D', 'htmldoc=true',
            '-D', 'prefix=/prefix with spaces', '-D', 'prefix=/last prefix',
            '-D', 'mandoc=false', '--builddir', 'build with spaces',
            '--wipe', '--verbose', '--install', '--jobs', '2'])
        args = self.build_args(wipe=True)
        private = args.builddir / 'meson-private'
        private.mkdir(parents=True)
        (private / 'coredata.dat').touch()
        self.assertIn('--wipe', build.plan(args)[0])
        self.assertEqual(build.legacy_arguments([]), ['--builddir', 'compile'])
        self.assertIsNone(build.legacy_arguments(['-h']))

    def test_legacy_build_dry_run_preserves_paths_and_creates_no_output(self):
        result = subprocess.run([
            sys.executable, str(ROOT / 'tools/maintenance/build.py'), '--legacy',
            '-b', str(self.root / 'legacy build'), '-p', '/prefix with spaces',
            '-Dbidi=disabled', '--dry-run'], capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("'-Dprefix=/prefix with spaces'", result.stdout)
        self.assertFalse((self.root / 'legacy build').exists())
        self.assertFalse(list(self.root.glob('*.log')))

    def test_verbose_failure_is_propagated_and_compile_not_run(self):
        calls = self.root / 'calls'
        fake = self.executable('fake meson',
            'import pathlib,sys\n'
            f'with pathlib.Path({str(calls)!r}).open("a") as stream: stream.write(sys.argv[1]+"\\n")\n'
            'sys.stdout.buffer.write(b"\\xff failure without newline")\n'
            'sys.exit(37)\n')
        log = self.root / 'log'
        result = subprocess.run([sys.executable, str(ROOT / 'tools/maintenance/build.py'),
                                 '--meson', fake, '--builddir', str(self.root / 'new'),
                                 '--log', str(log), '--verbose'], capture_output=True)
        self.assertEqual(result.returncode, 37)
        self.assertEqual(calls.read_text(), 'setup\n')
        self.assertIn(b'\xff failure without newline', log.read_bytes())
        self.assertIn(b'\xff failure without newline', result.stdout)

    def test_build_entrypoint_propagates_verbose_failure(self):
        calls = self.root / 'legacy calls'
        fake = self.executable('legacy fake meson',
            'import pathlib,sys\n'
            f'pathlib.Path({str(calls)!r}).write_text(sys.argv[1])\n'
            'sys.exit(37)\n')
        result = subprocess.run([
            str(ROOT / 'build'), '-b', str(self.root / 'entrypoint build'),
            '--meson', fake, '--log', str(self.root / 'entrypoint log'), '-v'],
            capture_output=True)
        self.assertEqual(result.returncode, 37, result.stderr)
        self.assertEqual(calls.read_text(), 'setup')

    def test_failed_generation_does_not_publish_and_preserves_existing_output(self):
        output = self.root / 'output'
        with self.assertRaises(RuntimeError):
            with fresh_output(output) as stage:
                (stage / 'partial').touch()
                raise RuntimeError('renderer failed')
        self.assertFalse(output.exists())
        output.mkdir()
        (output / 'evidence').write_text('keep')
        with self.assertRaises(ValueError):
            with fresh_output(output):
                self.fail('should refuse existing output')
        self.assertEqual((output / 'evidence').read_text(), 'keep')
        self.assertFalse(list(self.root.glob('.fvwm-maint-*')))

    def test_outputs_refuse_runtime_and_build_source_components(self):
        for name in ('doc', 'fvwm', 'libs', 'modules', 'perllib', 'po', '.github'):
            with self.assertRaises(ValueError):
                with fresh_output(ROOT / name / 'never-create-maint-output', ROOT):
                    self.fail('source component must be protected')

    def test_build_dry_run_is_read_only_and_rejects_bad_jobs(self):
        command = [sys.executable, str(ROOT / 'tools/maintenance/build.py'),
                   '--builddir', str(self.root / 'dry run'), '--dry-run']
        result = subprocess.run(command, capture_output=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertFalse((self.root / 'dry run').exists())
        self.assertFalse(list(self.root.glob('*.log')))
        result = subprocess.run(command + ['--jobs', '0'], capture_output=True)
        self.assertEqual(result.returncode, 2)

    def test_source_symlink_and_traversal_refused(self):
        (self.root / 'link').symlink_to(ROOT / 'README.md')
        for name in ('link', '../README.md'):
            with self.assertRaises(ValueError):
                source_file(self.root, name)

    def test_manual_manifest_matches_meson_and_includes_desktop(self):
        manifest = json.loads((ROOT / 'tools/maintenance/manuals.json').read_text())
        self.assertEqual(docs.manual_manifest(), manifest)
        meson = (ROOT / 'doc/meson.build').read_text()
        self.assertIn("input: '../tools/maintenance/manuals.json'", meson)
        for group in ('pages', 'sections', 'split_pages'):
            names = subprocess.check_output([
                sys.executable, str(ROOT / 'tools/maintenance/docs.py'),
                '--list', group], text=True).splitlines()
            self.assertEqual(names, manifest[group])
        for name in manifest['pages'] + manifest['split_pages']:
            self.assertTrue((ROOT / 'doc' / (name + '.adoc')).is_file())
        self.assertIn('FvwmDesktop', manifest['pages'])

    def test_sections_reject_corruption_and_match_legacy_extraction(self):
        text = (ROOT / 'doc/fvwm3_manpage_source.adoc').read_text()
        for name in ('commands', 'menus', 'styles'):
            old = subprocess.check_output(['sh', str(ROOT / 'meson-scripts/generate_ad_file.sh'),
                                           name, str(ROOT / 'doc/fvwm3_manpage_source.adoc')], text=True)
            self.assertEqual(docs.section(text, name), old)
        for text in ("// BEGIN 'x'\n", "// END 'x'\n// BEGIN 'x'\n", "// BEGIN 'x'\n// BEGIN 'x'\n// END 'x'\n"):
            with self.assertRaises(ValueError):
                docs.section(text, 'x')

    def test_docs_both_modes_include_utilities_and_report_failure(self):
        fake = self.executable('fake asciidoctor',
            'import pathlib,sys\n'
            'pathlib.Path(sys.argv[sys.argv.index("-o")+1]).write_text("rendered")\n')
        output = self.root / 'docs'
        docs.render(ROOT, output, ['html', 'man'], False, fake)
        self.assertTrue((output / 'html/FvwmDesktop.html').is_file())
        self.assertTrue((output / 'man/fvwm3styles.1').is_file())
        self.assertTrue((output / 'man/FvwmPrompt.1').exists())
        self.assertTrue((output / 'man/fvwm-file-browser.1').exists())
        self.assertIn('xref:FvwmPrompt', (output / 'sources/index.adoc').read_text())
        docs.render(ROOT, self.root / 'prompt', ['man'], True, fake)
        self.assertTrue((self.root / 'prompt/man/FvwmPrompt.1').exists())
        self.assertTrue((self.root / 'prompt/man/fvwm-file-browser.1').exists())
        bad = self.executable('bad renderer', 'import sys\nsys.exit(3)\n')
        with self.assertRaises(subprocess.CalledProcessError):
            docs.render(ROOT, self.root / 'failed', ['man'], False, bad)
        self.assertFalse((self.root / 'failed').exists())

    def test_website_index_preserves_title_links_and_source_version(self):
        body = '<div class="sect1"><p><a href="fvwm3.html">manual</a></p></div>\n'
        result = docs.website_fragment(body, 'index', 'v1&2')
        self.assertIn('title: Fvwm3 Manual Pages', result)
        self.assertIn('<strong>v1&amp;2</strong>', result)
        self.assertIn('href="fvwm3/"', result)
        self.assertNotIn('<p>', result)
        self.assertNotIn('class="sect1"', result)
        page = docs.website_fragment('<div class="literalblock">x</div>', 'fvwm3', 'v')
        self.assertIn('literalblock highlight', page)
        self.assertIn('permalink: /Man/fvwm3/index.html', page)

    def test_flat_website_preserves_existing_data_and_source(self):
        fake = self.executable('website renderer',
            'import pathlib,sys\n'
            'pathlib.Path(sys.argv[sys.argv.index("-o")+1]).write_text("body")\n')
        main = ROOT / 'doc/fvwm3_manpage_source.adoc'
        original = main.read_bytes()
        output = self.root / 'Man'
        docs.website_directory(ROOT, output, True, fake)
        self.assertTrue((output / 'FvwmPrompt.html').is_file())
        self.assertTrue((output / 'FvwmDesktop.html').is_file())
        self.assertFalse((output / 'sources').exists())
        self.assertEqual(main.read_bytes(), original)
        (output / 'user-page.html').write_text('keep')
        with self.assertRaises(ValueError):
            docs.website_directory(ROOT, output, True, fake)
        self.assertEqual((output / 'user-page.html').read_text(), 'keep')

    def test_protocol_generation_matches_known_wire_values(self):
        values = generate.constants((ROOT / 'libs/Module.h').read_text())
        self.assertEqual(values['MX_VISIBLE_ICON_NAME'], 0x80000002)
        self.assertEqual(values['MX_REPLY'], 0x80000400)
        self.assertEqual(values['HEADER_SIZE'], 4)
        self.assertEqual(values['RESPONSE_UNLOCK'], 'NOP UNLOCK')
        self.assertEqual(values['MX_PROPERTY_CHANGE_BACKGROUND'], 1)
        for expression in ('__import__("os").system("false")', '1 << 100000', '-1', '1 / 0'):
            with self.assertRaises(ValueError):
                generate.integer(expression, {})

    @unittest.skipUnless(shutil.which('perl'), 'Perl optional for developer comparison')
    def test_metadata_is_valid_perl_deterministic_and_uses_current_version(self):
        for name in ('first', 'second'):
            generate.metadata(ROOT, self.root / name, 0)
        for name in ('Constants.pm', 'Commands.pm'):
            first = self.root / 'first' / name
            self.assertEqual(first.read_bytes(), (self.root / 'second' / name).read_bytes())
            result = subprocess.run(['perl', '-c', str(first)], capture_output=True)
            self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("$VERS = '" + (ROOT / '.current-version').read_text().strip() + "';",
                      (self.root / 'first/Commands.pm').read_text())
        entries = json.loads((self.root / 'first/commands.json').read_text())
        by_name = {entry['name']: entry for entry in entries}
        self.assertIn('#', by_name)
        self.assertIn('*', by_name)
        self.assertEqual(by_name['Move']['window'], 1)
        self.assertEqual(by_name['Move']['cursor'], 'MOVE')
        for name in ('Constants.pm', 'Commands.pm'):
            text = (self.root / 'first' / name).read_text()
            self.assertIn('use strict;', text)
            self.assertIn('use warnings;', text)
            self.assertIn('=head1 NAME', text)
            self.assertEqual((self.root / 'first' / name).read_bytes(),
                             (ROOT / 'perllib/FVWM' / name).read_bytes())

    def test_metadata_refuses_incomplete_extended_mask(self):
        source_root = self.root / 'source'
        (source_root / 'libs').mkdir(parents=True)
        header = (ROOT / 'libs/Module.h').read_text()
        (source_root / 'libs/Module.h').write_text(
            header.replace('0x000007fe', '0x0000001f'))
        with self.assertRaisesRegex(ValueError, 'named events'):
            generate.metadata(source_root, self.root / 'output', 0)
        self.assertFalse((self.root / 'output').exists())

    def test_generator_version_mismatch_does_not_generate(self):
        with patch('generate.subprocess.check_output', return_value='bison 2.3\n'):
            with self.assertRaises(ValueError):
                generate.parsers(ROOT, self.root / 'parser', 'bison', 'flex', '3.8.2', '2.6.4')
        self.assertFalse((self.root / 'parser').exists())

    def parser_generator(self, arguments, *, cwd, **kwargs):
        stage = Path(cwd)
        if arguments[0] == 'bison':
            (stage / 'script.c').write_text('#include "config.h"\n')
            (stage / 'script.h').write_text('#define TOKEN 258\n')
        else:
            (stage / 'scanner.c').write_text('#include "config.h"\n')

    def test_parser_generation_records_exact_inputs_and_outputs(self):
        output = self.root / 'parser'
        with patch('generate.subprocess.check_output',
                   side_effect=['bison 3.8.2\n', 'flex 2.6.4\n']), \
             patch('generate.subprocess.run', side_effect=self.parser_generator):
            generate.parsers(ROOT, output, 'bison', 'flex', '3.8.2', '2.6.4')
        record = json.loads((output / 'generators.json').read_text())
        for section, names in [('input_sha256', ('script.y', 'scanner.l')),
                               ('output_sha256', ('script.c', 'script.h', 'scanner.c'))]:
            self.assertEqual(set(record[section]), set(names))
            for name in names:
                self.assertEqual(record[section][name],
                                 hashlib.sha256((output / name).read_bytes()).hexdigest())

    def test_parser_header_order_failure_does_not_publish(self):
        def misplaced_header(arguments, *, cwd, **kwargs):
            self.parser_generator(arguments, cwd=cwd, **kwargs)
            if arguments[0] == 'flex':
                (Path(cwd) / 'scanner.c').write_text(
                    '#include <stdio.h>\n#include "config.h"\n')

        output = self.root / 'parser'
        with patch('generate.subprocess.check_output',
                   side_effect=['bison 3.8.2\n', 'flex 2.6.4\n']), \
             patch('generate.subprocess.run', side_effect=misplaced_header):
            with self.assertRaisesRegex(ValueError, 'scanner.c must include config.h first'):
                generate.parsers(ROOT, output, 'bison', 'flex', '3.8.2', '2.6.4')
        self.assertFalse(output.exists())
        self.assertFalse(any(self.root.glob('.fvwm-maint-*')))

    def test_parser_wrapper_preserves_arguments_and_status(self):
        record = self.root / 'arguments.json'
        self.executable('python3',
            'import json,sys\n'
            f'open({str(record)!r}, "w").write(json.dumps(sys.argv[1:]))\n'
            'sys.exit(7)\n')
        output = self.root / 'candidate with spaces'
        environment = os.environ.copy()
        environment['PATH'] = str(self.root) + os.pathsep + environment['PATH']
        result = subprocess.run(['sh', str(ROOT / 'modules/FvwmScript/fvwmscript-syntax.sh'),
                                 '--output', str(output)], env=environment,
                                capture_output=True)
        self.assertEqual(result.returncode, 7, result.stderr)
        self.assertEqual(json.loads(record.read_text()),
                         [str(ROOT / 'modules/FvwmScript/../../tools/maintenance/generate.py'),
                          'parsers', '--output', str(output)])
        self.assertFalse(output.exists())

    def test_translation_literals_locations_and_bad_escapes(self):
        text = 'one\n$[gt.Hello \\"there\\"]\nLocaleTitle {Héllo} WindowLocaleTitle {World}'
        self.assertEqual(translations.messages(text), [(2, 'Hello "there"'), (3, 'Héllo'), (3, 'World')])
        with self.assertRaises(ValueError):
            translations.messages(r'$[gt.unknown\q]')
        self.assertGreater(len(translations.potfiles(ROOT)), 10)
        with self.assertRaises(ValueError):
            translations.maintain(ROOT, self.root / 'po', 'build', '../../bad')

    def test_translation_failure_keeps_source_and_no_output(self):
        po = ROOT / 'po/fr.po'
        digest = hashlib.sha256(po.read_bytes()).hexdigest()
        with patch('translations.subprocess.run', side_effect=subprocess.CalledProcessError(2, 'msgfmt')):
            with self.assertRaises(subprocess.CalledProcessError):
                translations.maintain(ROOT, self.root / 'po', 'build', 'fr')
        self.assertFalse((self.root / 'po').exists())
        self.assertEqual(hashlib.sha256(po.read_bytes()).hexdigest(), digest)

    def git_fixture(self):
        subprocess.run(['git', 'init', '-q', str(self.root)], check=True)
        (self.root / 'source name.c').write_text('source\n')
        (self.root / 'source name.c').chmod(0o755)
        (self.root / '.current-version').write_text('test\n')
        (self.root / 'dev-docs').mkdir()
        (self.root / 'dev-docs/evidence').write_text('keep\n')
        (self.root / 'link').symlink_to('source name.c')
        subprocess.run(['git', '-C', str(self.root), 'add', '.'], check=True)
        subprocess.run(['git', '-C', str(self.root), '-c', 'user.name=Fixture',
                        '-c', 'user.email=fixture@example.invalid', 'commit', '-qm', 'fixture'], check=True)

    def test_inventory_handles_spaces_links_and_deleted_files(self):
        self.git_fixture()
        (self.root / 'source name.c').unlink()
        (self.root / 'new file').write_text('new')
        files = {item['path']: item for item in source.inventory(self.root, True)['files']}
        self.assertEqual(files['source name.c']['state'], 'deleted')
        self.assertEqual(files['link']['kind'], 'symlink')
        self.assertIn('new file', files)

    def test_archive_is_deterministic_does_not_delete_and_refuses_dirty(self):
        self.git_fixture()
        # Output outside the fixture keeps it clean between archive calls.
        with tempfile.TemporaryDirectory() as out:
            for name, mask in (('first', '0002'), ('second', '0077')):
                subprocess.run(['git', '-C', str(self.root), 'config', 'tar.umask', mask], check=True)
                source.archive(self.root, Path(out) / name)
            first = Path(out) / 'first/source.tar.gz'
            self.assertEqual(first.read_bytes(), (Path(out) / 'second/source.tar.gz').read_bytes())
            with tarfile.open(first) as archive:
                self.assertIn('.current-version', archive.getnames())
                self.assertIn('source name.c', archive.getnames())
                self.assertEqual(archive.getmember('source name.c').mode, 0o755)
                self.assertNotIn('dev-docs/evidence', archive.getnames())
            self.assertTrue((self.root / 'dev-docs/evidence').is_file())
            (self.root / 'new file').touch()
            with self.assertRaises(ValueError):
                source.archive(self.root, Path(out) / 'dirty')

    def test_dependency_and_workflow_inventory_records_sources_without_a_security_verdict(self):
        (self.root / 'libs').mkdir()
        (self.root / 'libs/cJSON.h').write_text(
            '#define CJSON_VERSION_MAJOR 2\n#define CJSON_VERSION_MINOR 0\n#define CJSON_VERSION_PATCH 1\n')
        (self.root / 'bin/FvwmPrompt').mkdir(parents=True)
        (self.root / 'bin/FvwmPrompt/go.mod').write_text(
            'module fixture\ngo 1.21\nrequire golang.org/x/text v0.9.0\n')
        (self.root / '.github/workflows').mkdir(parents=True)
        (self.root / '.github/workflows/fixture.yml').write_text(
            'steps:\n  - uses: actions/checkout@v5\n  - uses: owner/action@' + 'a' * 40 + '\n')
        report = source.maintenance_metadata(self.root)
        self.assertEqual(report['vendored']['cJSON'], '2.0.1')
        self.assertEqual(report['vendored']['go_modules']['golang.org/x/text'], 'v0.9.0')
        self.assertEqual([item['full_commit_pin'] for item in report['workflow_references']], [False, True])
        for reference in report['workflow_references']:
            self.assertGreater(reference['line'], 0)
            self.assertIn('Text reference only', reference['note'])

    def test_archive_rejects_traversal_and_unsafe_symlinks(self):
        for name in ('../escape', '/absolute'):
            with self.assertRaises(ValueError):
                source.safe_member(tarfile.TarInfo(name))
        link = tarfile.TarInfo('safe/link')
        link.type = tarfile.SYMTYPE
        link.linkname = '../../escape'
        with self.assertRaises(ValueError):
            source.safe_member(link)


if __name__ == '__main__':
    unittest.main()
