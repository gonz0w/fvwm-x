#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or any later version.
"""Verify command client delivery/order against the actual WM and MFL."""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import time

from live_x_harness import HarnessRuntime, start_display_server, wait_for_predicate
from mfl_stream_live import Connection


def run(args):
    args.artifacts_dir.mkdir(parents=True, exist_ok=True)
    evidence = {}
    with HarnessRuntime(prefix='fc-live-') as runtime:
        try:
            display = args.display
            if display is None:
                _, display = start_display_server(runtime, 'Xvfb',
                    ['Xvfb', '-screen', '0', '800x600x24', '-noreset'],
                    'xserver.log', time.monotonic() + 10)
            env = runtime.environment({'FVWMMFL_SOCKET_PATH': str(runtime.root),
                                      'FVWM_USERDIR': str(runtime.root), 'DISPLAY': display})
            core = runtime.start('fvwm', [str(args.fvwm), '-d', display,
                '-c', 'ModulePath ' + str(args.mfl.parent),
                '-c', 'AddToFunc StartFunction I Module FvwmMFL'], 'fvwm.log', env)
            endpoint = runtime.root / ('fvwm_mfl_' + display.replace(':', '_') + '.sock')
            wait_for_predicate('MFL endpoint', endpoint.exists, time.monotonic() + 10, process=core)
            observer = Connection(endpoint)
            try:
                assert 'connection_profile' in observer.record()
                observer.send(b'set echo\n')
                observer.echo('observer-ready')
                for label, argv, incoming, expected, direct in [
                    ('explicit', ['-f', str(endpoint), 'Echo', 'command-explicit'], None, ['command-explicit'], False),
                    ('environment', ['Echo', 'command-environment'], None, ['command-environment'], True),
                    ('discovery', ['Echo', 'command-discovery'], None, ['command-discovery'], False),
                    ('stdin-order', ['-c', 'ignored'], b'Echo command-first\r\nEcho command-second\n',
                     ['command-first', 'command-second'], True),
                    ('feedback', ['-i', '1', 'Echo', 'command-feedback'], None, ['command-feedback'], True),
                ]:
                    client_env = dict(env)
                    if direct:
                        client_env['FVWMMFL_SOCKET'] = str(endpoint)
                    else:
                        client_env.pop('FVWMMFL_SOCKET', None)
                    result = subprocess.run([str(args.command), *argv], input=incoming,
                        capture_output=True, env=client_env, timeout=12)
                    assert result.returncode == 0, (label, result.stderr)
                    for message in expected:
                        observer.expect_echo(message)
                    if label == 'feedback':
                        assert 'connection_profile' in json.loads(result.stdout)
                    evidence[label] = 'passed'
                result = subprocess.run([str(args.command), 'Echo bad\nEcho hidden'],
                    capture_output=True, env=dict(env, FVWMMFL_SOCKET=str(endpoint)), timeout=12)
                assert result.returncode == 1 and b'command must be one line' in result.stderr
                observer.echo('healthy-after-refusal')
                evidence['refusal-core-remains-responsive'] = 'passed'
                assert core.poll() is None
            finally:
                observer.close()
        finally:
            for path in runtime.root.glob('*.log'):
                shutil.copy2(path, args.artifacts_dir / path.name)
            (args.artifacts_dir / 'results.json').write_text(json.dumps(evidence, indent=2) + '\n')
    print(json.dumps(evidence, sort_keys=True))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('fvwm', 'mfl', 'command', 'artifacts-dir'):
        parser.add_argument('--' + name, type=Path, required=True)
    parser.add_argument('--display')
    options = parser.parse_args()
    for name, value in vars(options).items():
        if isinstance(value, Path):
            setattr(options, name, value.resolve())
    run(options)
