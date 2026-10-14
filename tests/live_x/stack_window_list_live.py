#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or any later version.
"""Check initialized restack IDs, icon counts and detached record membership."""
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
    for new_window in (False, True):
        for icons in range(3):
            for suppressed in (False, True):
                for lower in (False, True):
                    for detached in ((False, True) if new_window else (False,)):
                        environment = {'FVWM_COUNT_ICONS': str(icons)}
                        for label, enabled in [('NEW', new_window),
                            ('SUPPRESSED', suppressed), ('LOWER', lower),
                            ('DETACHED', detached)]:
                            if enabled:
                                environment['FVWM_COUNT_' + label] = '1'
                        label = ('new' if new_window else 'layer') + (
                            f'-{icons}-suppressed-{int(suppressed)}'
                            f'-lower-{int(lower)}-detached-{int(detached)}')
                        out = args.artifacts_dir / label
                        out.mkdir(exist_ok=True)
                        with HarnessRuntime(prefix='fvwm-stack-list-') as runtime:
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
                                assert core.wait_until_exit(
                                    time.monotonic() + 20) == 0
                                text = core.log_path.read_text()
                                assert 'initialized-restack-and-server-ring-order: passed' in text
                                for failure in ('X error', 'AddressSanitizer',
                                    'runtime error:'):
                                    assert failure not in text, text
                                results[label] = 'passed'
                            finally:
                                for path in runtime.root.glob('*.log'):
                                    shutil.copy2(path, out / path.name)
    assert len(results) == 36
    (args.artifacts_dir / 'evidence.json').write_text(
        json.dumps(results, indent=2) + '\n')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(allow_abbrev=False)
    for name in ('fvwm', 'artifacts-dir'):
        parser.add_argument('--' + name, type=Path, required=True)
    parser.add_argument('--display')
    run(parser.parse_args())
