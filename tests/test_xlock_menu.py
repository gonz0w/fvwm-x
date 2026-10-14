#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or any later version.
"""Compare xlock formatting/options and exercise the bounded direct help probe."""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import time

ROOT = Path(__file__).resolve().parents[1]


def run(binary):
    count = 0
    with tempfile.TemporaryDirectory(prefix='fvwm-xlock-') as temporary:
        root = Path(temporary)
        legacy = root / 'legacy'
        legacy.write_text((ROOT / 'bin/fvwm-menu-xlock.in').read_text().replace(
            '@PERL@', shutil.which('perl')).replace('@VERSION@', 'fixture-version'))
        producer = root / 'xlock'
        env = dict(os.environ, PATH=str(root) + os.pathsep + os.environ['PATH'],
                   LC_ALL='C', PROBE_ARGS=str(root / 'args.json'))

        def script(body):
            producer.write_text('#!' + sys.executable + '\n' +
                'import os,sys,json,time\n' +
                'open(os.environ["PROBE_ARGS"],"w").write(json.dumps(sys.argv[1:]))\n' + body)
            producer.chmod(0o755)

        def invoke(args=(), old=False, custom_env=None):
            return subprocess.run(([shutil.which('perl'), str(legacy)] if old else
                [str(binary)]) + list(args), env=custom_env or env,
                capture_output=True, timeout=9)

        script('print("where mode is one of:")\n'
               'print("  alpha Shows café pattern")\n'
               'print("  blank Shows Blank screen")\n'
               'print("  random Random mode")\n')
        cases = [[], ['--special-first'], ['--wm-icons'], ['--icon-item=0', '--wm-icons'],
            ['-n', 'Menu', '-t', 'Title', '--icon-title=title.xpm', '--icon-item=item.xpm'],
            ['--NAME=Menu', '--TI=Title', '--it=%D'], ['--name', '--help'],
            ['--item', '-literal', 'argument', '--', '-nolock', '-inwindow'],
            ['--i'], ['--icon'], ['--unknown'], ['--wm-icons=x'], ['--item']]
        cases += [['--item=' + fmt] for fmt in ('%n:%d:%D', '%8n:%-8D', '%*5n',
            '%*-5n', '%8*4n:%-8*-4n', '%0n:%*0D:%*3D:%*99D', '%q %% %n', r'%n\t%D')]
        for args in cases:
            old, new = invoke(args, old=True), invoke(args)
            assert (new.returncode, new.stdout, new.stderr.replace(str(binary).encode(), b'TOOL')) == (
                old.returncode, old.stdout, old.stderr.replace(str(legacy).encode(), b'TOOL')), (args, old, new)
            count += 1
        assert json.loads((root / 'args.json').read_text()) == ['-display', 'NONE', '-help']

        script('print("where mode is one of:", file=sys.stderr)\n'
               'print("  alpha Shows Alpha", file=sys.stderr)\nsys.exit(2)\n')
        result = invoke()
        assert result.returncode == 0 and b'Exec xlock -mode alpha' in result.stdout
        count += 1
        script('print("where mode is one of:")\n'
               'print("  alpha Shows \\"quoted\\" \\\\path")\n'
               'print("  bad;mode Refuse")\n')
        result = invoke()
        assert result.returncode == 0 and b'\\"quoted\\" \\\\path' in result.stdout and b'bad;mode' not in result.stdout
        count += 1
        script('print("different help format")\n')
        result = invoke()
        assert result.returncode == 0 and b'Exec xlock' not in result.stdout
        count += 1
        script('sys.stdout.write("x" * (1024 * 1024 + 1))\n')
        result = invoke()
        assert result.returncode == 1 and not result.stdout and b'exceeds 1 MiB' in result.stderr
        count += 1
        # exec replaces the producer so the deadline owns the process directly.
        script('os.execl("/bin/sleep", "sleep", "30")\n')
        began = time.monotonic()
        result = invoke()
        elapsed = time.monotonic() - began
        assert result.returncode == 1 and not result.stdout and b'deadline exceeded' in result.stderr and elapsed < 7, (result, elapsed)
        count += 1
        producer.unlink()
        result = invoke(custom_env=dict(env, PATH=str(root)))
        assert result.returncode == 1 and not result.stdout and b'could not read xlock help' in result.stderr
        count += 1
    print(json.dumps({'xlock_cases': count, 'status': 'passed'}))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--xlock', type=Path, required=True)
    run(parser.parse_args().xlock.resolve())
