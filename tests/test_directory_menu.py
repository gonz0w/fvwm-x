#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or any later version.
"""Check directory CLI/menu parity and owned remembered navigation."""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def run(binary):
    count = 0
    with tempfile.TemporaryDirectory(prefix='fvwm-directory-') as temporary:
        root = Path(temporary)
        legacy = root / 'legacy'
        legacy.write_text((ROOT / 'bin/fvwm-menu-directory.in').read_text().replace(
            '@PERL@', shutil.which('perl')).replace('@VERSION@', 'fixture-version'))
        directory = root / 'listing'
        directory.mkdir()
        (directory / 'folder').mkdir()
        (directory / 'linked-folder').symlink_to('folder')
        for name in ('zeta', 'alpha', '.hidden', 'café', 'quote" dollar$', 'app'):
            p = directory / name
            p.write_bytes(b'contents\n')
            p.chmod(0o700 if name == 'app' else 0o600)
            os.utime(p, (1700000000, 1700000000))
        os.mkfifo(directory / 'pipe', 0o600)
        env = dict(os.environ, HOME=str(root), FVWM_USERDIR=str(root),
                   SHELL='/bin/sh', EDITOR='vi', TZ='UTC', LC_ALL='C')

        def invoke(args=(), old=False):
            return subprocess.run(([shutil.which('perl'), str(legacy)] if old else
                [str(binary)]) + ['--dir', str(directory)] + list(args), env=env,
                capture_output=True, timeout=9)

        def normalized(result, executable):
            return result.returncode, result.stdout, result.stderr.replace(
                str(executable).encode(), b'TOOL').replace(
                    b'(integer number expected)', b'(number expected)')

        cases = [[], ['--ALL', '--no-all'], ['--all'], ['--links'],
            ['--wm-icons'], ['--icon-file=file', '--icon-app=app', '--icon-dir=dir'],
            ['--title', '', '--command-file=Nop'], ['--command-title', '', '--memory-for-speed'],
            ['--special-dirs'], ['--special-dirs=3,1,2-'], ['--check-subdirs'],
            ['--menu-style=Custom', '--func-name=CustomFunction'],
            ['--exec-title=^pwd', '--exec-file=-', '--exec-app=^echo'],
            ['--xterm=terminal -e', '--exec-app'], ['--command-app=Echo app %f'],
            ['--title=%d (%p)'], ['--name=Unused'], ['--i'], ['--icon'],
            ['--unknown'], ['--all=yes'], ['--order=oops'], ['--item'],
            ['--DIR', str(directory) + '///', '--title', '%d:%p'],
            ['--DIR', str(directory) + '///', '--special-dirs'],
        ]
        cases += [['--order=' + str(order)] for order in range(-6, 7)]
        # Only regular files have fixed dates; test metadata in a separate fixture below.
        cases += [['--item=' + fmt] for fmt in ('%n:%s:%t:%T', '%8n:%-8s',
            '%*5n', '%*-5n', '%8*4n:%-8*-4n', '%0n:%*0n:%*3n:%*99n',
            '%q %% %n', r'%n\t%T', '%f')]
        for args in cases:
            old, new = invoke(args, old=True), invoke(args)
            assert normalized(new, binary) == normalized(old, legacy), (args, old, new)
            count += 1
        for p in directory.iterdir():
            if not p.is_symlink():
                os.utime(p, (1700000000, 1700000000))
        old, new = invoke(['--item=%n:%D:%d'], old=True), invoke(['--item=%n:%D:%d'])
        assert normalized(new, binary) == normalized(old, legacy), (old, new)
        count += 1
        state = root / '.fvwm-menu-directory.dir'
        result = subprocess.run([str(binary), '--fvwm-remember', str(root),
            str(directory), 'Browser'], env=env, capture_output=True, timeout=5)
        assert result.returncode == 0 and result.stdout == b'Menu "Browser" WarpTitle\n'
        assert state.read_text() == str(directory) + '\n' and state.stat().st_mode & 0o777 == 0o600
        result = invoke(['--reuse', '--name=Browser', '--dir=/missing'])
        assert result.returncode == 0 and b'--fvwm-remember' in result.stdout and str(directory).encode() in result.stdout
        assert b'echo ' not in result.stdout and b' >' not in result.stdout
        count += 1
        state.unlink()
        state.symlink_to('nonexistent')
        result = invoke(['--reuse'])
        assert result.returncode == 1 and not result.stdout
        count += 1
        state.unlink()
        os.mkfifo(state, 0o600)
        result = invoke(['--reuse'])
        assert result.returncode == 1 and not result.stdout
        count += 1
        (directory / 'line\nbreak').write_text('display only')
        result = invoke()
        assert result.returncode == 0 and b'+ "line break" Nop\n' in result.stdout
        count += 1
    print(json.dumps({'directory_cases': count, 'status': 'passed'}))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--directory', type=Path, required=True)
    run(parser.parse_args().directory.resolve())
