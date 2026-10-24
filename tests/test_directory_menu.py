#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or any later version.
"""Check directory CLI/menu parity and owned remembered navigation."""
import argparse
import json
import os
import re
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def run(binary):
    count = 0
    with tempfile.TemporaryDirectory(prefix='fvd-', dir='/tmp') as temporary:
        root = Path(temporary)
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

        def invoke(args=()):
            return subprocess.run([str(binary)] + ['--dir', str(directory)] + list(args), env=env,
                capture_output=True, timeout=9)

        reference = json.loads((ROOT / 'tests/fixtures/directory-cli.json').read_text())
        for case in reference['cases']:
            args = [v.replace('<ROOT>', str(root)) for v in case['args']]
            if args == ['--item=%n:%D:%d']:
                for p in directory.iterdir():
                    if not p.is_symlink():
                        os.utime(p, (1700000000, 1700000000))
            result = invoke(args)
            def normalized(data):
                return data.replace(str(binary).encode(), b'TOOL').replace(
                    str(root).encode(), b'<ROOT>').decode('utf-8', 'surrogateescape').replace(
                    '(integer number expected)', '(number expected)')
            expected = dict(case)
            # Only directory st_size is filesystem-specific. Preserve the
            # frozen layout while checking its value against independent stat.
            if args in (['--item=%n:%s:%t:%T'], ['--item=%8n:%-8s']):
                for name in ('folder', 'linked-folder'):
                    old = str(reference['provenance']['directory_size'])
                    size = str((directory / name).stat().st_size)
                    if args == ['--item=%8n:%-8s']:
                        old, size = old.ljust(8), size.ljust(8)
                    expected['stdout'] = expected['stdout'].replace(
                        name + ':' + old, name + ':' + size)
            stdout = normalized(result.stdout)
            if args and re.fullmatch(r'--order=-?[0-9]+', args[0]) and abs(int(args[0][8:])) < 4:
                order = int(args[0][8:])
                names = [v for v in os.listdir(directory) if not v.startswith('.')]
                if abs(order) in (2, 3):
                    names.sort(key=lambda v: (directory / v).is_dir(),
                               reverse=abs(order) == 2)
                if order < 0:
                    names.reverse()
                rows = [line for line in stdout.splitlines() if
                        re.match(r'^\+ ".*" (Popup|Exec) ', line)][1:]
                # Check native enumeration/stable partition independently;
                # compare the frozen command/layout rows as an unordered set.
                def label(name):
                    return name.replace('\\', '\\\\').replace('"', '\\"').replace('$', '$$')
                assert len(rows) == len(names), (args, rows, names)
                assert all(row.startswith('+ "' + label(name) + '" ')
                           for row, name in zip(rows, names)), (args, rows, names)
                stdout = '\n'.join(sorted(stdout.splitlines()))
                expected['stdout'] = '\n'.join(sorted(expected['stdout'].splitlines()))
            observed = (result.returncode, stdout, normalized(result.stderr))
            wanted = (expected['status'], expected['stdout'], expected['stderr'])
            assert observed == wanted, (args, observed, wanted)
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
