#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or any later version.
"""Compare conversion bytes and check private, non-overwriting publication.

Configuration text is data in these tests. No converted commands are executed.
"""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def check(binary):
    count = 0
    with tempfile.TemporaryDirectory(prefix='fvwm-converter-') as temporary:
        root = Path(temporary)
        perl = shutil.which('perl')
        assert perl
        legacy = root / 'legacy'
        legacy.write_bytes((ROOT / 'bin/fvwm-convert-2.6.in').read_bytes().replace(
            b'@PERL@', os.fsencode(perl)))
        (root / 'home/.fvwm').mkdir(parents=True)
        (root / 'user').mkdir()
        env = dict(os.environ, HOME=str(root / 'home'),
                   FVWM_USERDIR=str(root / 'user'), LC_ALL='C')

        def run(args, old=False, directory=None):
            command = [perl, str(legacy)] if old else [str(binary)]
            return subprocess.run([*command, *args], cwd=directory or root,
                                  env=env, capture_output=True, timeout=15)

        for args in ([], ['-h'], ['--help'], ['--he'], ['-H'], ['-help'],
                     ['--HELP'], ['--unknown'], ['--help=yes'],
                     ['a', 'b', 'c'], ['a', '--help'], ['--', '-h', 'a', 'b'],
                     ['--he=x'], ['--fol=x'], ['---help'], ['-hf']):
            old, new = run(args, True), run(args)
            assert (new.returncode, new.stdout, new.stderr) == (
                old.returncode, old.stdout, old.stderr), (args, old, new)
            count += 1

        samples = [
            b'', b'\n', b'# retained\nUnknownCommand caf\xc3\xa9\n',
            b'Style * NoTitle, NoPPosition, NoBorder\n',
            b'Style "XTerm" NoTitle, NoPPosition\n',
            b'STYLE * noTitle, NoFoo, NoFoo, noPposition\n',
            b' Style * NoTitle\nStyle broken\n',
            b'*FvwmTheme: Colorset 2 fg black\n  *FvwmTheme: ignored\n',
            b'Module FvwmTheme\nModuleSynchronous FvwmTheme\n',
            b'ModulePath /one:/two\n WindowShadeSteps 12p\nWindowShadeAnimate invalid\n',
            b'WindowFont "xft:Sans 12"\nIconFont fixed\n',
            b'EdgeResistance 250 10\nEdgeResistance 12\n',
            b'Key Tab A M Next [XTerm] Raise\nMouse 1 R A Next (CurrentDesk XTerm) Raise\n',
            b'Key x A M Next [*] Raise\nMouse 1 R A Prev [*] Raise\n',
            b'SnapAttraction 10 Screen\n SnapGrid 10 20\n',
            b'PixmapPath /one\nIconPath /two\nColorLimit 20\nGnomeButton 1 2\nGnomeShowDesks all\n',
            b'Desk 0 2\n HilightColorset 2\nHideGeometryWindow\n',
            b'AddToFunc InitFunction\n+ I Echo initial\n+ "I" Next [XTerm] Raise\n',
            b'AddToFunc InitFunction I Echo inline\n+ I Echo following\n',
            b'AddToFunc StartFunction\n+ I Echo start\nAddToFunc RestartFunction\n+ I Echo restart\n',
            b'AddToFunc StartFunction I Echo inline\nAddToFunc RestartFunction I Echo inline\n',
            b'AddToFunc InitFunction\n+ I Echo initial\nAddToFunc Other\n+ I Next [XTerm] Raise\n',
            b'+ I Next (CurrentDesk XTerm) Raise\n+ "C" Pick [XTerm] Raise\n',
            b'Style * NoTitle, \\\nNoBorder\n# after\n',
            b'# continuation \\\nsecond \\\nthird\n# after\n',
            b'Unknown "back\\slash"\nUnknown ends\\inside\n',
            b'# CRLF\r\nWindowFont fixed\r\n# invalid \xff\xfe\n',
            b'# ' + b'x' * 70000 + b'\nWindowFont fixed\n',
            b'Key x A M next [XTerm] Raise\nMouse 1 R A next (CurrentDesk XTerm) Raise\n',
            b'AddToFunc F I next [XTerm] Raise\n+ I next (A B) Raise\n',
            b'Style * NoTitle, NoFooNoBar, NoPPosition, NoPPosition\n',
            b'UnknownMouse next [XTerm] Raise\nComment iconpath word\n',
            b'\tAddToFunc InitFunction\n\t+ I Echo initial\n\tAddToFunc F\n\t+ I Echo other\n',
        ]
        for index, sample in enumerate(samples):
            source = root / ('source ' + str(index))
            source.write_bytes(sample)
            old_dest, new_dest = root / 'old output', root / 'new output'
            old = run([str(source), str(old_dest)], True)
            new = run([str(source), str(new_dest)])
            assert old.returncode == new.returncode == 0, (index, old, new)
            assert old.stdout == new.stdout and old.stderr == new.stderr, (index, old, new)
            assert old_dest.read_bytes() == new_dest.read_bytes(), (
                index, old_dest.read_bytes(), new_dest.read_bytes())
            assert source.read_bytes() == sample
            assert new_dest.stat().st_mode & 0o777 == 0o600
            old_dest.unlink()
            new_dest.unlink()
            count += 1

        # Resolve Read only through the existing literal search sequence.
        for name in ('local', 'user/secondary', 'home/.fvwm/third'):
            (root / name).write_bytes(b'WindowFont fixed\n')
        source = root / 'reads'
        source.write_bytes(b'Read local\nRead secondary\nRead third\nRead absent\nRead "local"\n')
        for flags in ([], ['-f'], ['--follow-read'], ['--fol'], ['-F']):
            old_dest, new_dest = root / 'old output', root / 'new output'
            old = run([*flags, str(source), str(old_dest)], True)
            recorded = {p.name: p.read_bytes() for p in root.glob('*.converted')}
            for p in root.glob('*.converted'):
                p.unlink()
            new = run([*flags, str(source), str(new_dest)])
            assert (new.returncode, new.stdout, new.stderr) == (
                old.returncode, old.stdout, old.stderr), (flags, old, new)
            assert old_dest.read_bytes() == new_dest.read_bytes()
            assert {p.name: p.read_bytes() for p in root.glob('*.converted')} == recorded
            for p in root.glob('*.converted'):
                p.unlink()
            old_dest.unlink()
            new_dest.unlink()
            count += 1

        source.write_bytes(b'# file\n')
        default = root / 'reads.converted'
        assert run([str(source)]).returncode == 0
        assert default.read_bytes() == b'# file'
        default.unlink()
        count += 1

        for kind in ('regular', 'symlink', 'dangling', 'hardlink', 'directory', 'fifo'):
            destination = root / 'occupied'
            sentinel = root / 'sentinel'
            sentinel.write_bytes(b'preserved')
            if kind == 'regular':
                destination.write_bytes(b'preserved')
            elif kind in ('symlink', 'dangling'):
                destination.symlink_to(sentinel if kind == 'symlink' else root / 'absent')
            elif kind == 'hardlink':
                os.link(sentinel, destination)
            elif kind == 'directory':
                destination.mkdir()
            else:
                os.mkfifo(destination)
            before = destination.lstat()
            result = run([str(source), str(destination)])
            assert result.returncode == 255 and b'exists' in result.stderr, result
            assert destination.lstat().st_ino == before.st_ino
            assert sentinel.read_bytes() == b'preserved'
            if kind == 'directory':
                destination.rmdir()
            else:
                destination.unlink()
            count += 1

        destination = root / 'output'
        for data, expected in ((b'# unfinished \\', b'unterminated continuation'),
                               (b'x' * (1024 * 1024 + 1), b'logical line')):
            source.write_bytes(data)
            result = run([str(source), str(destination)])
            assert result.returncode != 0 and expected in result.stderr, result
            assert not destination.exists()
            count += 1
        source.unlink()
        with source.open('wb') as out:
            out.truncate(128 * 1024 * 1024 + 1)
        result = run([str(source), str(destination)])
        assert result.returncode != 0 and b'128 MiB' in result.stderr
        assert not destination.exists()
        count += 1

        source.unlink()
        os.mkfifo(source)
        result = run([str(source), str(destination)])
        assert result.returncode != 0 and b'regular file' in result.stderr
        source.unlink()
        count += 1

        # Publication races must produce one complete output and one refusal.
        source.write_bytes(b'WindowFont fixed\n' * 1000)
        processes = [subprocess.Popen([str(binary), str(source), str(destination)],
                     cwd=root, env=env, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
                     for _ in range(2)]
        observations = [p.communicate(timeout=15) for p in processes]
        assert sorted(p.returncode for p in processes) == [0, 255], observations
        assert destination.read_bytes() == (b'Style * Font fixed\n' * 1000).rstrip(b'\n')
        destination.unlink()
        count += 1

        alias = root / 'parent alias'
        alias.symlink_to(root, target_is_directory=True)
        assert run([str(source), str(alias / 'output')]).returncode == 0
        assert destination.stat().st_mode & 0o777 == 0o600
        destination.unlink()
        count += 1
        assert not list(root.glob('.fvwm-convert-2.6-*'))
        source.write_bytes(b'Read absent\n' * 4097)
        result = run([str(source), str(destination)])
        assert result.returncode != 0 and b'Read references' in result.stderr
        assert not destination.exists()
        count += 1
        # Source aliases are read without changing the target or following
        # arbitrary file types. Parent aliases only affect the output directory.
        source.write_bytes(b'WindowFont fixed\n')
        source_alias = root / 'source alias'
        source_alias.symlink_to(source)
        assert run([str(source_alias), str(destination)]).returncode == 0
        assert source.read_bytes() == b'WindowFont fixed\n'
        destination.unlink()
        count += 1
    return count


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--converter', type=Path, required=True)
    args = parser.parse_args()
    print(json.dumps({'config_converter_cases': check(args.converter.resolve()),
                      'status': 'passed'}))
