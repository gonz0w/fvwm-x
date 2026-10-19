#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
"""Check colorset invalidation and sticky context work in the actual core."""
import argparse
import json
from pathlib import Path
import shutil
import time

from live_x_harness import HarnessRuntime, start_display_server


def run(args):
    out = args.artifacts_dir.resolve()
    out.mkdir(parents=True, exist_ok=True)
    with HarnessRuntime(prefix='fvwm-style-update-work-') as runtime:
        try:
            display = args.display
            if display is None:
                _, display = start_display_server(
                    runtime, 'Xvfb', ['Xvfb', '-screen', '0', '800x600x24',
                                     '-noreset'], 'server.log',
                    time.monotonic() + 15)
            process = runtime.start(
                'core', [str(args.fvwm), '-d', display, '-f', '/dev/null'],
                'core.log', runtime.environment())
            assert process.wait_until_exit(time.monotonic() + 30) == 0
            text = process.log_path.read_text()
            expected = {
                'all-normal-highlight-border-references',
                'unrelated-border-colorset-keeps-style-clean',
                'partial-zero-and-single-border-colorset-references',
                'unused-colorset-does-not-schedule-global-pass',
                'nonsticky-update-has-no-command-context',
                'sticky-update-keeps-owned-command-context',
                'icon-sticky-update-keeps-context-and-icon-state',
                'unrelated-colorset-preserves-pending-style-update',
                'decor-reference-and-pending-work-preserved',
                'ordinary-colorset-references-still-update',
                'implicit-zero-colors-preserve-legacy-border-refresh',
                'style update work',
            }
            assert {line[:-8] for line in text.splitlines()
                    if line.endswith(': passed')} == expected
            (out / 'evidence.json').write_text(
                json.dumps({'observations': text.splitlines()}, indent=2) +
                '\n')
        finally:
            for log in runtime.root.glob('*.log'):
                shutil.copy2(log, out / log.name)


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--fvwm', type=Path, required=True)
    parser.add_argument('--artifacts-dir', type=Path, required=True)
    parser.add_argument('--display')
    run(parser.parse_args())
