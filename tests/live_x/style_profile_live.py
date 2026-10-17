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
    matching = []
    wildcard = []
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
            if args.simplify_only:
                workloads = []
            for reference in (False, True):
                for mode, size, windows in workloads:
                    label = f'{mode}-{size}-{windows}-{reference}'
                    environment = {
                        'FVWM_STYLE_PROFILE_MODE': mode,
                        'FVWM_STYLE_PROFILE_SIZE': str(size),
                        'FVWM_STYLE_PROFILE_WINDOWS': str(windows)}
                    if reference:
                        environment['FVWM_STYLE_PROFILE_REFERENCE'] = '1'
                    process = runtime.start(
                        label, [str(args.fvwm), '-d', display,
                                '-f', '/dev/null'], label + '.log',
                        runtime.environment(environment))
                    assert process.wait_until_exit(time.monotonic() + 45) == 0
                    lines = [line.removeprefix('STYLE_PROFILE ') for line in
                             process.log_path.read_text().splitlines()
                             if line.startswith('STYLE_PROFILE ')]
                    assert len(lines) == 1
                    row = json.loads(lines[0])
                    assert (row['reference'], row['mode'], row['rules'],
                            row['windows']) == (reference, mode, size, windows)
                    rows.append(row)
                    work = [json.loads(line.removeprefix('STYLE_MATCH_WORK '))
                            for line in process.log_path.read_text().splitlines()
                            if line.startswith('STYLE_MATCH_WORK ')]
                    if mode == 'lookup' and size == 64:
                        assert [r['case'] for r in work] == [
                            'prefix', 'duplicate', 'window-id']
                        for r in work:
                            if reference:
                                expected = (64 if r['case'] == 'window-id'
                                            else 192)
                            else:
                                expected = 0
                            assert r['reference'] == reference
                            assert r['rules'] == 64 and r['calls'] == expected
                        matching.extend(work)
                    else:
                        assert not work
                    specialized = [json.loads(line.removeprefix(
                        'STYLE_WILDCARD_WORK '))
                        for line in process.log_path.read_text().splitlines()
                        if line.startswith('STYLE_WILDCARD_WORK ')]
                    if mode == 'lookup' and size == 64 and not reference:
                        assert len(specialized) == 6
                        assert [(r['case'], r['previous']) for r in specialized] == [
                            (case, previous)
                            for case in ('minimum', 'suffix', 'single-star')
                            for previous in (False, True)]
                        for r in specialized:
                            assert r['rules'] == 64
                            assert r['calls'] == (64 if r['previous'] else 0)
                        wildcard.extend(specialized)
                    else:
                        assert not specialized
            repeated = []
            for collision in (False, True):
                label = f'repeat-{collision}'
                environment = {
                    'FVWM_STYLE_PROFILE_MODE': 'repeat',
                    'FVWM_STYLE_PROFILE_SIZE': '64',
                    'FVWM_STYLE_PROFILE_WINDOWS': '1'}
                if collision:
                    environment['FVWM_STYLE_HASH_COLLISION'] = '1'
                process = runtime.start(label, [str(args.fvwm), '-d', display,
                    '-f', '/dev/null'], label + '.log',
                    runtime.environment(environment))
                assert process.wait_until_exit(time.monotonic() + 45) == 0
                work = [json.loads(line.removeprefix('STYLE_SIMPLIFY_WORK '))
                        for line in process.log_path.read_text().splitlines()
                        if line.startswith('STYLE_SIMPLIFY_WORK ')]
                assert [r['case'] for r in work] == [
                    'fingerprint', 'prefix-cutoff', 'disjoint']
                assert all(r['hash_collision'] == collision for r in work)
                repeated.extend(work)
            (out / 'evidence.json').write_text(
                json.dumps({'workloads': rows, 'matching_work': matching,
                            'wildcard_work': wildcard,
                            'simplify_work': repeated},
                           indent=2) + '\n')
        finally:
            for log in runtime.root.glob('*.log'):
                shutil.copy2(log, out / log.name)


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--fvwm', type=Path, required=True)
    parser.add_argument('--artifacts-dir', type=Path, required=True)
    parser.add_argument('--display')
    parser.add_argument('--simplify-only', action='store_true')
    run(parser.parse_args())
