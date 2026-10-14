#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
"""Exercise deferred picture ownership with a synthetic typed-output worker."""
import argparse
from pathlib import Path
import shutil
import tempfile
import time

from live_x_harness import HarnessError, HarnessRuntime, start_display_server


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--probe', required=True)
    parser.add_argument('--worker', required=True)
    parser.add_argument('--display')
    parser.add_argument('--artifacts-dir', type=Path, required=True)
    args = parser.parse_args()
    args.artifacts_dir.mkdir(parents=True, exist_ok=True)
    artifacts = Path(tempfile.mkdtemp(prefix='run-', dir=args.artifacts_dir))
    for depth in ((None,) if args.display else (24, 8)):
        with HarnessRuntime(prefix='fvwm-picture-async-') as runtime:
            try:
                display = args.display
                if display is None:
                    _, display = start_display_server(runtime, 'Xvfb',
                        ['Xvfb', '-screen', '0', f'800x600x{depth}'],
                        'xserver.log', time.monotonic() + 15)
                probe = runtime.start('picture-async',
                    [args.probe, args.worker], 'probe.log',
                    runtime.environment({'DISPLAY': display}))
                if probe.wait_until_exit(time.monotonic() + 45) != 0:
                    raise HarnessError('picture ownership probe failed')
                print(probe.log_path.read_text())
            finally:
                for log in runtime.root.glob('*.log'):
                    shutil.copyfile(log, artifacts / f'{depth}-{log.name}')


if __name__ == '__main__':
    main()
