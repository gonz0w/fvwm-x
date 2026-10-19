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
            index_work = []
            subject_work = []
            cursor_work = []
            escaped_work = []
            for reference, collision in ((False, False), (True, False),
                                         (False, True)):
                for size in (64, 256):
                    label = f'index-{size}-{reference}-{collision}'
                    environment = {
                        'FVWM_STYLE_PROFILE_MODE': 'index',
                        'FVWM_STYLE_PROFILE_SIZE': str(size),
                        'FVWM_STYLE_PROFILE_WINDOWS': '1'}
                    if reference:
                        environment['FVWM_STYLE_PROFILE_REFERENCE'] = '1'
                    if collision:
                        environment['FVWM_STYLE_INDEX_COLLISION'] = '1'
                    process = runtime.start(label,
                        [str(args.fvwm), '-d', display, '-f', '/dev/null'],
                        label + '.log', runtime.environment(environment))
                    assert process.wait_until_exit(time.monotonic() + 45) == 0
                    work = [json.loads(line.removeprefix('STYLE_INDEX_WORK '))
                            for line in process.log_path.read_text().splitlines()
                            if line.startswith('STYLE_INDEX_WORK ')]
                    assert len(work) == 12
                    assert [(r['case'], r['target']) for r in work] == [
                        (kind, target) for kind in
                        ('window-id', 'explicit', 'compatibility')
                        for target in range(4)]
                    for r in work:
                        assert r['reference'] == reference
                        assert r['collision'] == collision
                        assert r['rules'] == size and r['lookups'] == 32
                        if reference or collision or (size == 64 and
                                                     r['case'] == 'explicit'):
                            assert r['visits'] == 32 * (size + 1)
                        else:
                            assert 64 <= r['visits'] < 32 * (size + 1)
                    subject = [json.loads(line.removeprefix('STYLE_SUBJECT_WORK '))
                               for line in process.log_path.read_text().splitlines()
                               if line.startswith('STYLE_SUBJECT_WORK ')]
                    assert len(subject) == 1
                    assert subject[0] == {'long_name_bytes': 16384,
                        'domain_transitions': 3, 'reference': reference,
                        'collision': collision}
                    subject_work.extend(subject)
                    cursor = [json.loads(line.removeprefix('STYLE_CURSOR_WORK '))
                              for line in process.log_path.read_text().splitlines()
                              if line.startswith('STYLE_CURSOR_WORK ')]
                    assert [r['case'] for r in cursor] == [
                        'fixed-miss', 'fixed-hit', 'question-hit', 'question-miss',
                        'star-depth', 'middle-miss', 'empty-tail']
                    for r in cursor:
                        assert not r['baseline']
                        assert r['reference'] == reference
                        assert r['collision'] == collision
                        assert r['calls'] == int(reference)
                        assert r['hit'] == (r['case'] in (
                            'fixed-hit', 'question-hit', 'star-depth'))
                    cursor_work.extend(cursor)
                    escaped = [json.loads(line.removeprefix('STYLE_ESCAPED_WORK '))
                               for line in process.log_path.read_text().splitlines()
                               if line.startswith('STYLE_ESCAPED_WORK ')]
                    assert [r['case'] for r in escaped] == [
                        'fixed-hit', 'fixed-miss', 'star-hit', 'star-miss',
                        'cursor-hit', 'cursor-miss', 'empty-tail',
                        'trailing-escape', 'star-depth']
                    for r in escaped:
                        assert not r['baseline']
                        assert r['reference'] == reference
                        assert r['collision'] == collision
                        assert r['calls'] == int(reference)
                        assert r['hit'] == (r['case'] in (
                            'fixed-hit', 'star-hit', 'cursor-hit', 'star-depth'))
                    escaped_work.extend(escaped)
                    index_work.extend(work)
            repeated = []
            overlap_work = []
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
                overlaps = [json.loads(line.removeprefix('STYLE_OVERLAP_WORK '))
                            for line in process.log_path.read_text().splitlines()
                            if line.startswith('STYLE_OVERLAP_WORK ')]
                assert [(r['reference'], r['case']) for r in overlaps] == [
                    (reference, case) for reference in (False, True)
                    for case in ('length', 'position', 'literal-member',
                                 'budget-member', 'escaped-length',
                                 'escaped-position', 'escaped-member',
                                 'anchor-head', 'anchor-tail', 'zero-slack')]
                for r in overlaps:
                    assert not r['baseline'] and r['collision'] == collision
                    assert r['rules'] == (3 if r['reference'] or
                                         r['case'] == 'budget-member' else 2)
                overlap_work.extend(overlaps)
            process = runtime.start('fields',
                [str(args.fvwm), '-d', display, '-f', '/dev/null'],
                'fields.log', runtime.environment({
                    'FVWM_STYLE_PROFILE_MODE': 'fields',
                    'FVWM_STYLE_PROFILE_SIZE': '64',
                    'FVWM_STYLE_PROFILE_WINDOWS': '1'}))
            assert process.wait_until_exit(time.monotonic() + 45) == 0
            fields = [json.loads(line.removeprefix('STYLE_FIELD_WORK '))
                      for line in process.log_path.read_text().splitlines()
                      if line.startswith('STYLE_FIELD_WORK ')]
            assert fields == [
                {'case': 'literal-first', 'lookups': 32,
                 'lengths': 0, 'prefixes': 0},
                {'case': 'cached-length', 'lookups': 32,
                 'lengths': 32, 'prefixes': 0}]
            (out / 'evidence.json').write_text(
                json.dumps({'workloads': rows, 'matching_work': matching,
                            'wildcard_work': wildcard,
                            'simplify_work': repeated, 'index_work': index_work,
                            'subject_work': subject_work,
                            'cursor_work': cursor_work, 'overlap_work': overlap_work,
                            'escaped_work': escaped_work, 'field_work': fields},
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
