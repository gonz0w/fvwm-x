#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or any later version.
"""Check unmanaged correction refusal, retained managed work and retry."""
import argparse
import json
import os
from pathlib import Path
import shutil
import time
from live_x_harness import HarnessRuntime, start_display_server


def run(args):
    args.artifacts_dir.mkdir(parents=True, exist_ok=True)
    results = {}
    for two in (False, True):
        for refuse in (False, True):
            environment = {}
            if two:
                environment['FVWM_OVERRIDE_TWO'] = '1'
            if refuse:
                environment['FVWM_OVERRIDE_REFUSE'] = '1'
            label = f'managed-{1 + int(two)}-refuse-{int(refuse)}'
            out = args.artifacts_dir / label
            out.mkdir(exist_ok=True)
            with HarnessRuntime(prefix='fvwm-override-storage-') as runtime:
                try:
                    display = args.display
                    if display is None:
                        _, display = start_display_server(runtime,
                            'Xvfb', ['Xvfb', '-screen', '0',
                                '800x600x24', '-noreset'],
                            'xserver.log', time.monotonic() + 10)
                    core = runtime.start('core', [str(args.fvwm),
                        '-d', display, '-f', '/dev/null'], 'core.log',
                        environment=dict(os.environ, **environment))
                    assert core.wait_until_exit(time.monotonic() + 20) == 0
                    text = core.log_path.read_text()
                    expected = {'ordinary-override-raise: passed'}
                    if refuse:
                        expected = {
                            'override-storage-refusal-preserves-managed-and-server-order: passed',
                            'override-storage-refusal-retry: passed'}
                    if two:
                        expected.add('completed-managed-raise-retained: passed')
                    observed = {line for line in text.splitlines()
                        if line.endswith(': passed')}
                    assert observed == expected, text
                    for failure in ('X error', 'AddressSanitizer',
                        'runtime error:'):
                        assert failure not in text, text
                    results[label] = 'passed'
                finally:
                    for path in runtime.root.glob('*.log'):
                        shutil.copy2(path, out / path.name)
    assert len(results) == 4
    (args.artifacts_dir / 'evidence.json').write_text(
        json.dumps(results, indent=2) + '\n')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(allow_abbrev=False)
    for name in ('fvwm', 'artifacts-dir'):
        parser.add_argument('--' + name, type=Path, required=True)
    parser.add_argument('--display')
    run(parser.parse_args())
