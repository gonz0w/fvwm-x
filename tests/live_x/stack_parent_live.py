#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
"""Check parent continuation order, finite stack use and storage fallback."""
import argparse
import json
import os
from pathlib import Path
import shutil
import time
from live_x_harness import HarnessRuntime, start_display_server


def run(args):
    cases = [('cycle-parity', {},
        'cyclic-redirection-matches-direct-target-and-transient-rules: passed')]
    for operation in ('raise', 'lower', 'restack'):
        flags = {} if operation == 'raise' else {
            'FVWM_PARENT_' + operation.upper(): '1'}
        for constrained in (False, True):
            environment = dict(flags, FVWM_PARENT_DEPTH='1')
            if constrained:
                environment['FVWM_DEPTH_SMALL_STACK'] = '1'
            cases.append((operation + ('-small' if constrained else '-default'),
                environment, 'finite-deep-parent-operation: passed'))
        for refuse in (False, True):
            environment = dict(flags, FVWM_PARENT_STORAGE='1')
            if refuse:
                environment['FVWM_PARENT_FAIL'] = '1'
            cases.append((operation + ('-refusal' if refuse else '-ancestry'),
                environment, operation + ('-invoked-fallback' if refuse else
                    '-ancestry') + '-exact-ring-and-server-order: passed'))
    args.artifacts_dir.mkdir(parents=True, exist_ok=True)
    results = {}
    for label, environment, expected in cases:
        out = args.artifacts_dir / label
        out.mkdir(exist_ok=True)
        with HarnessRuntime(prefix='fvwm-stack-parent-') as runtime:
            try:
                display = args.display
                if display is None:
                    _, display = start_display_server(runtime, 'Xvfb',
                        ['Xvfb', '-screen', '0', '800x600x24', '-noreset'],
                        'xserver.log', time.monotonic() + 10)
                core = runtime.start('core', [str(args.fvwm), '-d', display,
                    '-f', '/dev/null'], 'core.log',
                    environment=dict(os.environ, **environment))
                assert core.wait_until_exit(time.monotonic() + 25) == 0
                text = core.log_path.read_text()
                assert expected in text, text
                for failure in ('X error', 'AddressSanitizer', 'runtime error:'):
                    assert failure not in text, text
                results[label] = 'passed'
            finally:
                for path in runtime.root.glob('*.log'):
                    shutil.copy2(path, out / path.name)
    (args.artifacts_dir / 'evidence.json').write_text(
        json.dumps(results, indent=2) + '\n')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(allow_abbrev=False)
    for name in ('fvwm', 'artifacts-dir'):
        parser.add_argument('--' + name, type=Path, required=True)
    parser.add_argument('--display')
    run(parser.parse_args())
