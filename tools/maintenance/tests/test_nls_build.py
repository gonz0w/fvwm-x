# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or any later version.
"""Real Meson checks of the owning NLS detection/catalog build blocks."""
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from common import ROOT


@unittest.skipUnless(shutil.which('meson'), 'native Meson is required')
class NlsBuildTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='fvwm nls ')
        self.root = Path(self.temp.name)
        self.source = self.root / 'source'
        self.source.mkdir()
        text = (ROOT / 'meson.build').read_text()
        start = text.index('# BSD ports install gettext')
        end = text.index('\nlibpng = dependency(', start)
        detection = text[start:end]
        start = text.index('if nls_enabled\n    subdir(\'po\')')
        end = text.index("subdir('libs')", start)
        catalogs = text[start:end]
        self.assertIn("'NLS': nls_enabled,", text)
        (self.source / 'meson.build').write_text(
            "project('fvwm3', 'c', version: '1.0', "
            "meson_version: '>=1.5.1')\n"
            "cc = meson.get_compiler('c')\n"
            "host = host_machine.system()\n"
            "conf = configuration_data()\n"
            "all_found_deps = []\n" + detection + '\n' + catalogs +
            "configure_file(output: 'config.h', configuration: conf)\n"
            "summary({'NLS': nls_enabled}, bool_yn: true)\n")
        (self.source / 'meson.options').write_text(
            "option('nls', type: 'feature', value: 'auto')\n")
        po = self.source / 'po'
        po.mkdir()
        for name in ['meson.build', 'LINGUAS', 'POTFILES', 'fvwm3.pot']:
            shutil.copyfile(ROOT / 'po' / name, po / name)
        for path in (ROOT / 'po').glob('*.po'):
            shutil.copyfile(path, po / path.name)
        self.before = self.fingerprints()

    def tearDown(self):
        self.temp.cleanup()

    def fingerprints(self):
        return {p.relative_to(self.source).as_posix():
                hashlib.sha256(p.read_bytes()).hexdigest()
                for p in self.source.rglob('*') if p.is_file()}

    def run_checked(self, argv):
        result = subprocess.run(argv, capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        return result

    def configure(self, mode, missing=False):
        build = self.root / 'build'
        argv = [shutil.which('meson'), 'setup', str(build), str(self.source),
                '--prefix=/usr/local', '-Dnls=' + mode]
        env = os.environ.copy()
        if missing:
            # Native-file program overrides can name future executables.
            # Exercise real lookup with compiler/build tools but no msgfmt.
            tools = self.root / 'tools'
            tools.mkdir()
            for name in ('ninja', 'pkg-config', 'cc', 'gcc', 'clang', 'ld',
                         'ld.lld', 'as', 'ar', 'strip', 'python3', 'python',
                         'sh', 'uname', 'env'):
                program = shutil.which(name)
                if program:
                    (tools / name).symlink_to(program)
            env['PATH'] = str(tools)
            self.assertIsNone(shutil.which('msgfmt', path=env['PATH']))
        return build, subprocess.run(argv, capture_output=True, text=True,
                                     env=env)

    def check_disabled(self, mode, missing=False):
        build, result = self.configure(mode, missing)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertRegex(result.stdout, r'NLS\s*:\s*NO')
        self.assertNotIn('#define HAVE_NLS 1',
                         (build / 'config.h').read_text())
        targets = self.run_checked(['meson', 'introspect', str(build),
                                    '--targets'])
        self.assertEqual(json.loads(targets.stdout), [])
        self.run_checked(['meson', 'compile', '-C', str(build)])
        stage = self.root / 'stage'
        self.run_checked(['meson', 'install', '-C', str(build),
                          '--no-rebuild', '--destdir', str(stage)])
        self.assertEqual(list(stage.rglob('*.mo')), [])
        self.assertEqual(self.fingerprints(), self.before)

    def test_disabled_creates_no_catalog_targets(self):
        self.check_disabled('disabled')

    def test_disabled_does_not_require_msgfmt(self):
        self.check_disabled('disabled', missing=True)

    def test_auto_without_msgfmt_disables_nls(self):
        self.check_disabled('auto', missing=True)

    def test_enabled_without_msgfmt_refuses_configuration(self):
        _, result = self.configure('enabled', missing=True)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('msgfmt', result.stdout + result.stderr)
        self.assertEqual(self.fingerprints(), self.before)

    def test_enabled_regenerates_and_installs_all_catalogs(self):
        build, result = self.configure('enabled')
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertRegex(result.stdout, r'NLS\s*:\s*YES')
        self.assertIn('#define HAVE_NLS 1', (build / 'config.h').read_text())
        self.run_checked(['meson', 'compile', '-C', str(build)])
        stage = self.root / 'stage'
        self.run_checked(['meson', 'install', '-C', str(build),
                          '--no-rebuild', '--destdir', str(stage)])
        languages = (self.source / 'po/LINGUAS').read_text().split()
        self.assertEqual(len(list(stage.rglob('*.mo'))), len(languages))
        for language in languages:
            direct = self.root / (language + '.mo')
            self.run_checked(['msgfmt', '--check', '-o', str(direct),
                              str(self.source / 'po' / (language + '.po'))])
            installed = stage / 'usr/local/share/locale' / language
            installed = installed / 'LC_MESSAGES/fvwm3.mo'
            self.assertEqual(installed.read_bytes(), direct.read_bytes())
        self.assertEqual(self.fingerprints(), self.before)


if __name__ == '__main__':
    unittest.main()
