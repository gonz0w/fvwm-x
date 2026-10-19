#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
"""Check temporary selector ownership and rejected style commands."""
import argparse
import json
import os
from pathlib import Path
import shutil
import time

from live_x_harness import HarnessRuntime, start_display_server


def run(args):
    out = args.artifacts_dir.resolve()
    out.mkdir(parents=True, exist_ok=True)
    with HarnessRuntime(prefix='fvwm-style-command-owner-') as runtime:
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
            status = process.wait_until_exit(time.monotonic() + 30)
            (out / 'status.json').write_text(json.dumps({'status': status}))
            assert status == 0, status
            text = process.log_path.read_text()
            expected = {
                'use-style-copy-survives-source-removal',
                'use-style-explicit-selector-release',
                'use-style-miss-preserves-following-options',
                'destroy-style-found-and-missing-selector-release',
                'destroy-style-invalidates-and-applies-fallback',
                'refused-style-commands-do-not-schedule-pass',
                'refused-command-preserves-pending-style-update',
                'style command ownership',
            }
            if 'FVWM_STYLE_COMMAND_BASELINE' not in os.environ:
                expected.add('missing-selector-command-engine-refusal')
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
