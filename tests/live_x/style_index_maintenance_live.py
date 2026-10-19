#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
"""Check incremental style index ownership."""
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
    with HarnessRuntime(prefix='fvwm-style-index-maintenance-') as runtime:
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
            baseline = 'FVWM_STYLE_INDEX_BASELINE' in os.environ
            expected = {'style index maintenance'}
            for mode in range(2):
                for family, count in [('append', 16), ('delete', 16),
                                      ('simplify-0', 1), ('simplify-1', 1),
                                      ('simplify-2', 1)]:
                    expected.add(f'index-{family} collision={mode} '
                                 f'rebuilds={count if baseline else 0}')
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
