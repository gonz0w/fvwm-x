#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
"""Exercise production temporary X error handlers against a private server."""
import argparse
from pathlib import Path
import shutil
import time

from live_x_harness import HarnessError, HarnessRuntime, start_display_server


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--probe', required=True)
    parser.add_argument('--artifacts-dir', type=Path, required=True)
    args = parser.parse_args()
    args.artifacts_dir.mkdir(parents=True, exist_ok=True)
    with HarnessRuntime(prefix='fvwm-x-error-') as runtime:
        try:
            _, display = start_display_server(runtime, 'Xvfb',
                ['Xvfb', '-screen', '0', '800x600x24'], 'xserver.log',
                time.monotonic() + 15)
            probe = runtime.start('x-error-handler', [args.probe], 'probe.log',
                                  runtime.environment({'DISPLAY':display}))
            if probe.wait_until_exit(time.monotonic() + 15) != 0:
                raise HarnessError('X error-handler probe failed')
            print(probe.log_path.read_text())
        finally:
            for log in runtime.root.glob('*.log'):
                shutil.copy2(log, args.artifacts_dir / log.name)


if __name__ == '__main__':
    main()
