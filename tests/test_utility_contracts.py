#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or any later version.
"""Capture the installed Perl utility contracts before their Go ports.

These are parity fixtures, not a claim that every legacy output is desirable.
Only --record explicitly changes the reference file. No generated actions run.
"""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
REFERENCE = ROOT / 'tests/fixtures/utility-contracts.json'


def observe(xlock_binary=None, xlock_version=None, directory_binary=None, directory_version=None,
            converter_binary=None):
    perl = shutil.which('perl')
    assert perl
    observations = {}
    with tempfile.TemporaryDirectory(prefix='fvwm-utility-') as temporary:
        root = Path(temporary)
        tools = root / 'tools'
        tools.mkdir()
        for name in ('fvwm-menu-directory', 'fvwm-menu-xlock', 'fvwm-convert-2.6'):
            data = (ROOT / 'bin' / (name + '.in')).read_text()
            (tools / name).write_text(data.replace('@PERL@', perl).replace('@VERSION@', 'fixture-version'))
        if xlock_binary:
            shutil.copy2(xlock_binary, tools / 'fvwm-menu-xlock')
        if directory_binary:
            shutil.copy2(directory_binary, tools / 'fvwm-menu-directory')
        if converter_binary:
            shutil.copy2(converter_binary, tools / 'fvwm-convert-2.6')
        files = root / 'files'
        files.mkdir()
        (files / 'Folder').mkdir()
        (files / 'Å_folder').mkdir()
        for name in ('alpha', '.hidden', 'quote"file', "apostrophe'file", 'cash$&file', 'café', 'program'):
            (files / name).write_text('fixture\n')
        (files / 'program').chmod(0o755)
        (files / 'linked-folder').symlink_to('Folder')
        (files / 'linked-file').symlink_to('alpha')
        home = root / 'home'
        home.mkdir()
        config = root / 'config'
        config.mkdir()
        env = dict(os.environ, HOME=str(home), FVWM_USERDIR=str(config),
                   SHELL='/bin/sh', EDITOR='fixture-editor', LC_ALL='C',
                   PATH=str(tools) + os.pathsep + os.environ.get('PATH', ''))

        def run(label, name, args, success=True):
            candidate = ((xlock_binary and name == 'fvwm-menu-xlock') or
                         (directory_binary and name == 'fvwm-menu-directory') or
                         (converter_binary and name == 'fvwm-convert-2.6'))
            command = [str(tools / name)] if candidate else [perl, str(tools / name)]
            result = subprocess.run([*command, *args], cwd=root,
                                    env=env, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                    timeout=10)
            assert (result.returncode == 0) == success, (label, result.stderr)
            def normalize(data):
                # Store bytes faithfully; surrogate escapes are not Unicode claims.
                return data.decode('utf-8', 'surrogateescape').replace(str(root), '<ROOT>')
            if xlock_binary and name == 'fvwm-menu-xlock':
                if label.endswith('-version'):
                    assert result.stdout == (xlock_version + '\n').encode()
                    result.stdout = b'fixture-version\n'
                elif label.endswith('-help'):
                    result.stdout = result.stdout.replace(
                        b'A small utility which builds xlock menu for fvwm.',
                        b'A small perl script which builds xlock menu for fvwm.', 1)
            if directory_binary and name == 'fvwm-menu-directory':
                if label.endswith('-version'):
                    assert result.stdout == (directory_version + '\n').encode()
                    result.stdout = b'fixture-version\n'
                elif label.endswith('-help'):
                    result.stdout = result.stdout.replace(
                        b'A utility that builds directory listing for fvwm.',
                        b'A perl script that builds directory listing for fvwm.', 1)
            observations[label] = {'status': result.returncode,
                                   'stdout': normalize(result.stdout), 'stderr': normalize(result.stderr)}
            return result

        for name in ('fvwm-menu-directory', 'fvwm-menu-xlock'):
            run(name + '-version', name, ['--version'])
            run(name + '-help', name, ['--help'])
            run(name + '-invalid', name, ['--not-an-option'], False)
        common = ['--dir=' + str(files), '--title=Fixture', '--special-dirs=0',
                  '--exec-title=-', '--exec-file=^fixture-editor']
        run('directory-default', 'fvwm-menu-directory', common)
        run('directory-hidden-reverse-icons', 'fvwm-menu-directory', common +
            ['--all', '--order=-5', '--wm-icons', '--item=%n [%T]', '--links'])
        run('directory-width', 'fvwm-menu-directory', common + ['--item=%*8n'])
        assert not (config / '.fvwm-menu-directory.dir').exists()

        xlock = tools / 'xlock'
        xlock.write_text('#!' + sys.executable + '\n'
                         'import json,os,sys\n'
                         'open(os.environ["XLOCK_ARGS"],"w").write(json.dumps(sys.argv[1:]))\n'
                         'print("where mode is one of:")\n'
                         'print("  alpha Shows Alpha pattern")\n'
                         'print("  beta Shows Beta pattern")\n'
                         'print("  blank Shows Blank screen")\n'
                         'print("  random Random mode")\n')
        xlock.chmod(0o755)
        env['XLOCK_ARGS'] = str(root / 'xlock-args.json')
        run('xlock-default', 'fvwm-menu-xlock', [])
        run('xlock-format-special-first', 'fvwm-menu-xlock',
            ['--name=FixtureMenu', '--title=Fixture', '--item=%n: %D',
             '--wm-icons', '--special-first', '--', '-nolock', '-inwindow'])
        assert json.loads((root / 'xlock-args.json').read_text()) == ['-display', 'NONE', '-help']

        original = (b'# retained comment\nStyle * HilightFore white\n'
                    b'AddToFunc InitFunction\n+ I Echo initialize\n'
                    b'Next [XTerm] Raise\n# UTF-8 caf\xc3\xa9\n')
        source = root / 'old config'
        source.write_bytes(original)
        destination = root / 'converted config'
        run('convert-config', 'fvwm-convert-2.6', [str(source), str(destination)])
        observations['converted-bytes'] = destination.read_bytes().decode('utf-8', 'surrogateescape')
        before = destination.read_bytes()
        run('convert-refuse-overwrite', 'fvwm-convert-2.6', [str(source), str(destination)], False)
        assert source.read_bytes() == original and destination.read_bytes() == before
        run('convert-missing', 'fvwm-convert-2.6', [str(root / 'absent')], False)
        assert not (root / 'absent.converted').exists()
    return observations


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--record', action='store_true')
    parser.add_argument('--xlock', type=Path)
    parser.add_argument('--xlock-version', default='fixture-version')
    parser.add_argument('--directory', type=Path)
    parser.add_argument('--directory-version', default='fixture-version')
    parser.add_argument('--converter', type=Path)
    args = parser.parse_args()
    assert not (args.record and (args.xlock or args.directory or args.converter)), 'Candidate runs cannot replace the legacy baseline'
    observed = observe(args.xlock.resolve() if args.xlock else None, args.xlock_version,
                       args.directory.resolve() if args.directory else None, args.directory_version,
                       args.converter.resolve() if args.converter else None)
    if args.record:
        REFERENCE.parent.mkdir(exist_ok=True)
        REFERENCE.write_text(json.dumps(observed, ensure_ascii=True, indent=2) + '\n')
    else:
        expected = json.loads(REFERENCE.read_text())
        assert observed == expected, [key for key in observed if observed[key] != expected.get(key)]
    print(json.dumps({'utility_contracts': len(observed), 'status': 'passed'}))
