#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the Free
# Software Foundation; either version 2 of the License, or any later version.
"""Finite LIB-06 observation; production adoption requires its own gate."""
import argparse
import hashlib
import json
import math
from pathlib import Path
import statistics
import subprocess
import sys
import time


class Pairs(list):
    """Preserve object order and duplicates separately from arrays."""


def reject_constant(value):
    raise ValueError(f'non-standard JSON constant: {value}')


def decode(raw):
    return json.loads(raw.decode('utf-8', errors='strict'),
                      object_pairs_hook=Pairs, parse_constant=reject_constant)


def expected(case, mutated=False):
    titles = {'ordinary': 'quote"\\\n\t\x01 café', 'unicode': 'café 😀',
              'legacy': '\ufffdcut\ufffd\ufffd'}
    def record(title):
        return Pairs([('window_name', Pairs([
            ('window', '0x1234'), ('name', title)]))])
    if case in titles:
        return record(titles[case])
    if case == 'legacy-keys':
        return Pairs([('\ufffd', 1), ('\ufffd', 2)])
    if case == 'numbers':
        return [0, 0, -2147483648, 2147483647, .25, sys.float_info.min,
                sys.float_info.max, -sys.float_info.max, None, None,
                18446744073709551615]
    if case == 'stress':
        return [record(('y' if mutated else 'x') + 'x' * 255)
                for _ in range(256)]
    if case == 'large':
        return ('y' if mutated else 'x') + 'x' * 65535
    cap = 256 * 1024 if case.startswith('queue-') else 1024 * 1024
    # Encoded quotes add two bytes; the at-cap case is refused by transport.
    return 'x' * (cap - (3 if case.endswith('below') else 2))


def equivalent(actual, want):
    if isinstance(want, Pairs):
        return (isinstance(actual, Pairs) and len(actual) == len(want)
                and all(ak == wk and equivalent(av, wv)
                        for (ak, av), (wk, wv) in zip(actual, want)))
    if isinstance(want, list):
        return (type(actual) is list and len(actual) == len(want)
                and all(equivalent(a, w) for a, w in zip(actual, want)))
    if isinstance(want, float):
        return (type(actual) in (float, int) and math.isfinite(actual)
                and math.isclose(actual, want,
                                 rel_tol=4 * sys.float_info.epsilon,
                                 abs_tol=0))
    if type(want) is int:
        return (type(actual) in (int, float) and actual == want
                and (want != 18446744073709551615 or type(actual) is int))
    return actual == want and type(actual) is type(want)


def run(binary, *args):
    argv = [str(binary), *map(str, args)]
    started = time.monotonic_ns()
    result = subprocess.run(argv, capture_output=True, timeout=30, check=False)
    result.wall_ns = time.monotonic_ns() - started
    assert len(result.stdout) <= 2 * 1024 * 1024, 'unbounded probe output'
    assert len(result.stderr) <= 65536, 'unbounded diagnostic output'
    assert not result.stderr, (argv, result.stderr.decode(errors='replace'))
    assert result.returncode in (0, 2, 3), (argv, result.returncode)
    return result


def check_output(binary, mode, case, mutated=False):
    q = run(binary, 'emit', case, *mode)
    if q.returncode == 3:
        assert not q.stdout
        return {'result': 'refused', 'semantic_parity': False}
    assert q.returncode == 0, (mode, case, q.returncode)
    actual = decode(q.stdout)
    assert equivalent(actual, expected(case, mutated)), (mode, case,
                                                       'partial/wrong JSON')
    # Preserve lexical evidence; differences never become a byte-parity pass.
    return {'result': 'complete', 'semantic_parity': True,
            'encoded_sha256': hashlib.sha256(q.stdout[:-1]).hexdigest(),
            'encoded_bytes': len(q.stdout) - 1,
            'output': (q.stdout.decode('utf-8')
                       if len(q.stdout) < 4096 else None)}


def main():
    p = argparse.ArgumentParser(allow_abbrev=False)
    p.add_argument('--baseline', type=Path, required=True)
    p.add_argument('--candidate', type=Path, required=True)
    p.add_argument('--output', type=Path, required=True)
    p.add_argument('--repetitions', type=int, default=3, choices=range(1, 6))
    p.add_argument('--iterations', type=int, default=20, choices=range(1, 101))
    a = p.parse_args()
    binaries = {k: v.resolve(strict=True) for k, v in
                [('baseline', a.baseline), ('candidate', a.candidate)]}
    assert all(v.is_file() for v in binaries.values())
    identities = {k: {'path': str(v), 'bytes': v.stat().st_size,
                     'sha256': hashlib.sha256(v.read_bytes()).hexdigest()}
                  for k, v in binaries.items()}
    # Fail closed on existing output; retain the first failed attempt intact.
    a.output.mkdir(mode=0o700)
    result = {'status': 'running', 'binaries': identities, 'cases': {},
              'timing_scope': 'synthetic construction/write/disposal only',
              'production_parity': 'pending actual producers/MFL',
              'six_target_comparison': 'pending; this receipt is one profile',
              'tracked_peak_scope': 'requested bytes, including copy-realloc '
              'overlap; excludes headers, allocator overhead and workspace'}
    def save():
        (a.output / 'result.json').write_text(
            json.dumps(result, indent=2) + '\n')
    save()
    cases = ['ordinary', 'unicode', 'legacy', 'legacy-keys', 'numbers',
             'large', 'stress', 'queue-below', 'queue-at', 'status-below',
             'status-at']
    try:
        for label, mode in [('baseline', []), ('candidate', []),
                            ('candidate-borrowed', ['--borrowed'])]:
            key = 'baseline' if label == 'baseline' else 'candidate'
            binary = binaries[key]
            rows = result['cases'][label] = {}
            for case in cases:
                row = rows[case] = check_output(binary, mode, case)
                assert label != 'baseline' or row['result'] == 'complete'
                stats = run(binary, 'measure', case, *mode)
                assert stats.returncode == 0
                tracked = json.loads(stats.stdout)
                assert tracked['live_final'] == 0
                assert tracked['complete'] == int(row['result'] == 'complete')
                row['tracked'] = tracked
                if case in ('queue-below', 'queue-at', 'status-below',
                            'status-at'):
                    which = 'mfl' if case.startswith('queue-') else 'status'
                    assert tracked[which + '_empty_admitted'] == (
                        case.endswith('below'))
                assert tracked['mfl_128_queued_admitted'] == (
                    tracked['complete'] == 1
                    and tracked['encoded_bytes'] < 256 * 1024 - 128)
                if label == 'baseline' and case == 'legacy':
                    assert row['output'] == (
                        '{"window_name":{"window":"0x1234","name":'
                        '"\\ufffdcut\\ufffd\\ufffd"}}\n')
                if label == 'baseline' and case == 'legacy-keys':
                    assert row['output'] == '{"\\ufffd":1,"\\ufffd":2}\n'
                if case in ('large', 'stress'):
                    row['input_lifetime_control'] = check_output(
                        binary, [*mode, '--mutate-input'], case,
                        mutated=label.endswith('borrowed'))
                # Sweep every observed ordinal on small records. Success after
                # an optional allocation failure must still be complete/parity.
                if case in ('ordinary', 'legacy', 'legacy-keys', 'numbers'):
                    count = tracked['attempts_last']
                    assert 0 < count <= 512, 'failure sweep scope exceeded'
                    row['failures'] = []
                    for ordinal in range(count):
                        args = [*mode, '--fail-at', ordinal]
                        q = run(binary, 'measure', case, *args)
                        assert q.returncode == 0
                        obs = json.loads(q.stdout)
                        assert obs['live_final'] == 0
                        emitted = check_output(binary, args, case)
                        assert obs['complete'] == int(
                            emitted['result'] == 'complete')
                        row['failures'].append({'ordinal': ordinal,
                                                'outcome': obs['outcome'],
                                                'complete': obs['complete']})
                row['budget_refusals'] = []
                for budget in (1, 64):
                    q = run(binary, 'measure', case, *mode, '--budget', budget)
                    assert q.returncode == 0
                    obs = json.loads(q.stdout)
                    assert obs['live_final'] == 0 and obs['complete'] == 0
                    row['budget_refusals'].append(obs)
                save()
        # Alternate order per repetition. Native allocation has no tracking
        # hooks, fail injection, borrowed-input invalidation or output capture.
        for case in ('ordinary', 'large', 'stress'):
            samples = {k: [] for k in result['cases']}
            for repetition in range(a.repetitions):
                order = list(samples)
                if repetition % 2:
                    order.reverse()
                for label in order:
                    key = ('baseline' if label == 'baseline' else
                           'candidate')
                    binary = binaries[key]
                    mode = ['--borrowed'] if label.endswith('borrowed') else []
                    q = run(binary, 'measure', case, *mode, '--native',
                            '--repeat', a.iterations)
                    assert q.returncode == 0
                    obs = json.loads(q.stdout)
                    assert obs['complete'] == a.iterations
                    assert obs['allocator'] == 'native'
                    assert all(obs[k] is None for k in
                               ('attempts_last', 'requested_total',
                                'requested_peak'))
                    obs['process_wall_ns'] = q.wall_ns
                    samples[label].append(obs)
            for label, rows in samples.items():
                result['cases'][label][case]['native_samples'] = rows
                for field in ('elapsed_ns', 'cpu_ns'):
                    values = [r[field] / a.iterations for r in rows]
                    result['cases'][label][case][field + '_per_iteration'] = {
                        'median': statistics.median(values),
                        'range': [min(values), max(values)]}
            save()
        for case in cases:
            base = result['cases']['baseline'][case]
            for label in ('candidate', 'candidate-borrowed'):
                row = result['cases'][label][case]
                row['byte_parity'] = (
                    row.get('encoded_sha256') is not None
                    and row['encoded_sha256'] == base.get('encoded_sha256'))
        for k, f in binaries.items():
            assert hashlib.sha256(f.read_bytes()).hexdigest() == (
                identities[k]['sha256'])
        result['status'] = ('observations complete; differences retained; '
                            'no adoption')
    except BaseException as e:
        result.update(status='failed', error=repr(e))
        raise
    finally:
        save()


if __name__ == '__main__':
    main()
