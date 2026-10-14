#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
"""Run real core synchronous deadline/lifetime policy with private module endpoints."""
import argparse
import json
from pathlib import Path
import shutil
import time

from live_x_harness import HarnessRuntime, start_display_server


def run(args):
    expected = {
        'timely-unlock', 'idle-and-interrupted-wait',
        'partial-write-deadline', 'interrupted-write-deadline',
        'blocked-write-deadline', 'interrupted-read-deadline',
        'partial-read-deadline', 'output-and-unlock-share-deadline',
        'disconnected-input-retires-producer',
        'readiness-error-retires-producer',
        'activity-does-not-refresh-deadline',
        'nested-deadline-preserves-parent', 'command-retirement-pin',
        'final-command-retirement-pin', 'termination-cancels-wait',
        'clock-failure-retires-producer', 'clock-regression-retires-producer',
        'activity-total-deadline-real-clock'}
    args.artifacts_dir.mkdir(parents=True, exist_ok=True)
    with HarnessRuntime(prefix='fvwm-module-sync-') as runtime:
        try:
            display = args.display
            if display is None:
                _, display = start_display_server(runtime, 'Xvfb',
                    ['Xvfb', '-screen', '0', '800x600x24', '-noreset'],
                    'xserver.log', time.monotonic() + 10)
            core = runtime.start('core', [str(args.fvwm), '-d', display,
                '-f', '/dev/null'], 'core.log')
            assert core.wait_until_exit(time.monotonic() + 20) == 0
            text = core.log_path.read_text()
            passed = {line.removesuffix(': passed') for line in text.splitlines()
                if line.endswith(': passed')}
            assert passed == expected, text
            for failure in ('X error', 'AddressSanitizer', 'runtime error:'):
                assert failure not in text, text
            (args.artifacts_dir / 'evidence.json').write_text(json.dumps(
                {name: 'passed' for name in sorted(passed)}, indent=2) + '\n')
        finally:
            for path in runtime.root.glob('*.log'):
                shutil.copy2(path, args.artifacts_dir / path.name)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(allow_abbrev=False)
    for name in ('fvwm', 'artifacts-dir'):
        parser.add_argument('--' + name, type=Path, required=True)
    parser.add_argument('--display')
    run(parser.parse_args())
