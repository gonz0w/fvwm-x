#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or any later version.
"""Run the actual clipboard child against an independent public-pattern owner."""
import argparse
from pathlib import Path
import shutil
import time

from live_x_harness import HarnessError, HarnessRuntime, start_display_server


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--probe', type=Path, required=True)
    parser.add_argument('--capture', type=Path, required=True)
    parser.add_argument('--failure-capture', type=Path)
    parser.add_argument('--go-probe', type=Path)
    parser.add_argument('--deadline-capture', type=Path)
    parser.add_argument('--artifacts-dir', type=Path, required=True)
    args = parser.parse_args()
    args.artifacts_dir.mkdir(parents=True, exist_ok=True)
    with HarnessRuntime(prefix='fvwm-clipboard-') as runtime:
        try:
            _, display = start_display_server(runtime, 'Xvfb',
                ['Xvfb', '-screen', '0', '800x600x24'], 'xserver.log',
                time.monotonic() + 15)
            environment = runtime.environment({'DISPLAY': display,
                'FVWM_TEST_PRIVATE_X': '1'})
            jobs = [('capture', [str(args.probe), str(args.capture)])]
            if args.failure_capture:
                jobs.append(('cleanup-failure', [str(args.probe),
                    str(args.failure_capture), '--cleanup-failure']))
            if args.deadline_capture:
                jobs.append(('deadline', [str(args.probe),
                    str(args.deadline_capture), '--deadline']))
            if args.go_probe:
                jobs.append(('go-native', [str(args.go_probe),
                    '-test.run=^TestNativeChildMetadata$', '-test.v']))
            environment['FVWM_TEST_NATIVE_CLIPBOARD'] = str(args.capture)
            for name, argv in jobs:
                process = runtime.start(name, argv, name + '.log', environment)
                if process.wait_until_exit(time.monotonic() + 30) != 0:
                    raise HarnessError(name + ' clipboard checkpoint failed')
                print(process.log_path.read_text())
        finally:
            for log in runtime.root.glob('*.log'):
                shutil.copy2(log, args.artifacts_dir / log.name)


if __name__ == '__main__':
    main()
