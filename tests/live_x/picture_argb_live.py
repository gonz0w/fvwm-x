#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
"""Compare bounded conversion with legacy rendering on actual X visuals."""
import argparse
from pathlib import Path
import shutil
import tempfile
import time

from live_x_harness import HarnessError, HarnessRuntime, start_display_server


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--probe', required=True)
    parser.add_argument('--display')
    parser.add_argument('--artifacts-dir', type=Path, required=True)
    args = parser.parse_args()
    args.artifacts_dir.mkdir(parents=True, exist_ok=True)
    artifacts = Path(tempfile.mkdtemp(prefix='run-', dir=args.artifacts_dir))
    profiles = ((None, False),) if args.display else (
        (24, False), (8, False), (8, True))
    for depth, strict in profiles:
        with HarnessRuntime(prefix='fvwm-picture-argb-') as runtime:
            try:
                display = args.display
                if display is None:
                    _, display = start_display_server(
                        runtime, 'Xvfb',
                        ['Xvfb', '-screen', '0', f'800x600x{depth}'],
                        'xserver.log', time.monotonic() + 15)
                probe = runtime.start(
                    'picture-argb', [args.probe] + (['--strict'] if strict else []),
                    'probe.log',
                    runtime.environment({'DISPLAY': display}))
                if probe.wait_until_exit(time.monotonic() + 60) != 0:
                    raise HarnessError('bounded picture conversion failed')
                print(probe.log_path.read_text())
            finally:
                for log in runtime.root.glob('*.log'):
                    shutil.copyfile(log, artifacts / f'{depth}-{strict}-{log.name}')


if __name__ == '__main__':
    main()
