#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License, version 2 or later.
"""Observe paired GC/Xft clipping and subsequent unclipped drawing on real X."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import tempfile
import time

from live_x_harness import HarnessRuntime, start_display_server


def run(args):
    base = args.artifacts_dir.resolve()
    base.mkdir(parents=True, exist_ok=True)
    out = Path(tempfile.mkdtemp(prefix='run-', dir=base))
    evidence = dict(status='running', core_only=args.core_only,
        render_disabled=args.no_render, jobs=[],
        probe_sha256=hashlib.sha256(args.probe.read_bytes()).hexdigest(),
        execution_profile=args.execution_profile)
    with HarnessRuntime(prefix='fvwm-text-clip-') as runtime:
        try:
            display = args.display
            if display is None:
                command = ['Xvfb', '-screen', '0', '800x600x24', '-noreset']
                if args.no_render:
                    command += ['-extension', 'RENDER']
                _, display = start_display_server(runtime, 'Xvfb', command,
                    'xserver.log', time.monotonic() + 10)
            for mode in ('core',) if args.core_only else ('core', 'xft'):
                probe = runtime.start('clip-' + mode,
                    [str(args.probe)] + (['--xft'] if mode == 'xft' else []),
                    'clip-' + mode + '.log',
                    runtime.environment({'DISPLAY': display}))
                status = probe.wait_until_exit(time.monotonic() + 15)
                assert status == 0, (mode, status, probe.log_path.read_text())
                result = json.loads(probe.log_path.read_text())
                expected = ('xft',) if mode == 'xft' else ('fontset', 'core-font')
                assert result['backend'] in expected and result['x_errors'] == 0
                result['requested_family'] = mode
                evidence['jobs'].append(result)
            evidence['status'] = 'passed'
        except BaseException as error:
            evidence.update(status='failed; retained', error=repr(error))
            raise
        finally:
            failures = []
            for process in reversed(runtime.processes):
                try:
                    process.close(time.monotonic() + 2)
                except BaseException as error:
                    failures.append(repr(error))
            if failures:
                evidence.update(status='failed; retained',
                    cleanup_failures=failures)
            else:
                evidence['cleanup'] = 'all owned children closed'
            try:
                for log in runtime.root.glob('*.log'):
                    shutil.copyfile(log, out / log.name)
            finally:
                (out / 'evidence.json').write_text(
                    json.dumps(evidence, indent=2) + '\n')
            if failures and 'error' not in evidence:
                raise RuntimeError('owned text-clip cleanup failed')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__, allow_abbrev=False)
    parser.add_argument('--probe', type=Path, required=True)
    parser.add_argument('--artifacts-dir', type=Path, required=True)
    parser.add_argument('--display')
    parser.add_argument('--core-only', action='store_true')
    parser.add_argument('--no-render', action='store_true')
    parser.add_argument('--execution-profile', default='unspecified',
        choices=('unspecified', 'native-cpu', 'emulated-cpu'))
    args = parser.parse_args()
    if args.no_render and (not args.core_only or args.display):
        parser.error('--no-render requires owned Xvfb and --core-only')
    run(args)
