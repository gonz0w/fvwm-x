#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
"""Measure actual-core style work; never use elapsed ratios as acceptance."""
import argparse
import json
from pathlib import Path
import shutil
import time

from live_x_harness import HarnessRuntime, start_display_server


def run(args):
    out = args.artifacts_dir.resolve()
    out.mkdir(parents=True, exist_ok=True)
    rows = []
    with HarnessRuntime(prefix='fvwm-style-profile-') as runtime:
        try:
            display = args.display
            if display is None:
                _, display = start_display_server(
                    runtime, 'Xvfb', ['Xvfb', '-screen', '0', '800x600x24',
                                     '-noreset'], 'server.log',
                    time.monotonic() + 15)
            # Each workload gets fresh core state. Counts are assertions;
            # instrumented elapsed/grab intervals are observations only.
            workloads = [(mode, size, 1) for mode in ('simplify', 'lookup')
                         for size in (64, 128, 256)]
            # Vary rules and windows independently around (256, 8).
            workloads += [('update', size, 8) for size in (64, 128, 256)]
            workloads += [('update', 256, windows) for windows in (16, 32)]
            for mode, size, windows in workloads:
                label = f'{mode}-{size}-{windows}'
                process = runtime.start(
                    label, [str(args.fvwm), '-d', display,
                            '-f', '/dev/null'], label + '.log',
                    runtime.environment({
                        'FVWM_STYLE_PROFILE_MODE': mode,
                        'FVWM_STYLE_PROFILE_SIZE': str(size),
                        'FVWM_STYLE_PROFILE_WINDOWS': str(windows)}))
                assert process.wait_until_exit(time.monotonic() + 45) == 0
                lines = [line.removeprefix('STYLE_PROFILE ') for line in
                         process.log_path.read_text().splitlines()
                         if line.startswith('STYLE_PROFILE ')]
                assert len(lines) == 1
                row = json.loads(lines[0])
                assert (row['mode'], row['rules'], row['windows']) == (
                    mode, size, windows)
                rows.append(row)
            (out / 'evidence.json').write_text(
                json.dumps({'workloads': rows}, indent=2) + '\n')
        finally:
            for log in runtime.root.glob('*.log'):
                shutil.copy2(log, out / log.name)


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--fvwm', type=Path, required=True)
    parser.add_argument('--artifacts-dir', type=Path, required=True)
    parser.add_argument('--display')
    run(parser.parse_args())
