#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
"""Run real core startup deadline/lifetime policy and finite real module continuation."""
import argparse
import json
from pathlib import Path
import shutil
import time

from live_x_harness import HarnessRuntime, start_display_server


def run(args):
    expected = {
        'disconnection-retires-producer',
        'input-error-retires-producer',
        'real-explicit-timeout-asynchronous-continuation',
        'activity-keeps-total-deadline',
        'buffered-escape-ends-barrier',
        'clock-failure-keeps-module-live',
        'clock-regression-keeps-module-live',
        'command-retirement-pin',
        'complete-frame-at-deadline-resumes',
        'custom-expect-and-inline-command',
        'default-response-prefix',
        'explicit-positive-override',
        'final-command-retirement-pin',
        'final-frame-at-deadline-resumes',
        'interrupted-input-resumes-after-timeout',
        'interrupted-output-keeps-deadline',
        'interrupted-wait-bounded',
        'invalid-timeout-refused-before-launch',
        'nested-startup-preserves-parent-deadline',
        'omitted-timeout-captures-module-timeout',
        'ordinary-x-event-remains-queued',
        'partial-input-resumes-after-timeout',
        'partial-output-keeps-deadline',
        'real-timeout-then-asynchronous-continuation',
        'startup-output-retains-pass-budget',
        'terminal-select-error-ends-barrier',
        'termination-ends-barrier',
        'thirty-second-default-finite-waits',
        'x-readiness-escape-real-clock',
    }
    args.artifacts_dir.mkdir(parents=True, exist_ok=True)
    with HarnessRuntime(prefix='fvwm-module-startup-') as runtime:
        try:
            display = args.display
            if display is None:
                _, display = start_display_server(runtime, 'Xvfb',
                    ['Xvfb', '-screen', '0', '800x600x24', '-noreset'],
                    'xserver.log', time.monotonic() + 10)
            core = runtime.start('core', [str(args.fvwm), '-d', display,
                '-f', '/dev/null'], 'core.log', environment=runtime.environment({
                    'FVWM_STARTUP_PRODUCER': str(args.producer), 'DISPLAY': display}))
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
    for name in ('fvwm', 'producer', 'artifacts-dir'):
        parser.add_argument('--' + name, type=Path, required=True)
    parser.add_argument('--display')
    run(parser.parse_args())
