#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
"""Run real core client identity and construction/retirement membership with private module endpoints."""
import argparse
import json
from pathlib import Path
import shutil
import time

from live_x_harness import HarnessRuntime, start_display_server


def run(args):
    expected = {
        'client-context-and-alias-identity', 'construction-interval-fallback',
        'retirement-and-reused-xid-fallback',
        'transient-root-self-and-alias-guards',
        'indexed-deep-and-wide-parent-selection'}
    args.artifacts_dir.mkdir(parents=True, exist_ok=True)
    with HarnessRuntime(prefix='fvwm-client-identity-') as runtime:
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
