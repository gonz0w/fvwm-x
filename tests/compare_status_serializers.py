#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the Free
# Software Foundation; either version 2 of the License, or any later version.
"""Finite LIB-06 observation; production adoption requires its own gate."""
import argparse
from collections import defaultdict, deque
import hashlib
import json
import math
from pathlib import Path
from pathlib import PurePosixPath
import statistics
import subprocess
import sys
import time
from types import SimpleNamespace


class CapturedRuns:
    """Check exported native observations without executing their binaries."""

    def __init__(self, root, identities):
        self.root = root.resolve(strict=True)
        raw = self.read('capture.json', 2 * 1024 * 1024)
        manifest = json.loads(raw)
        assert manifest['schema'] == 1
        assert set(manifest['artifacts']) == set(identities)
        self.labels = {}
        for label, identity in identities.items():
            expected = {k: identity[k] for k in ('sha256', 'bytes')}
            assert manifest['artifacts'][label] == expected
            self.labels[identity['path']] = label
        self.runs = defaultdict(deque)
        self.sequence = deque()
        assert 0 < len(manifest['runs']) <= 2048
        for row in manifest['runs']:
            assert row['backend'] in identities
            args = row['args']
            assert type(args) is list and 0 < len(args) <= 12
            assert all(type(x) is str and len(x) <= 64 for x in args)
            key = (row['backend'], *args)
            self.runs[key].append(row)
            self.sequence.append(key)
        self.manifest_sha256 = hashlib.sha256(raw).hexdigest()

    def read(self, name, limit):
        path = PurePosixPath(name)
        assert not path.is_absolute() and '..' not in path.parts
        assert str(path) == name
        file = self.root / name
        assert file.is_file() and not file.is_symlink()
        assert file.parent.resolve().is_relative_to(self.root)
        assert file.stat().st_size <= limit
        with file.open('rb') as stream:
            raw = stream.read(limit + 1)
        assert len(raw) <= limit
        return raw

    def run(self, binary, args):
        key = (self.labels[str(binary)], *map(str, args))
        assert self.sequence and self.sequence.popleft() == key, (
            'captured invocation order differs', key)
        assert self.runs[key], ('missing captured invocation', key)
        row = self.runs[key].popleft()
        data = {}
        for name, limit in [('stdout', 2 * 1024 * 1024), ('stderr', 65536),
                            ('metrics', 4096)]:
            expected = row[name]
            assert type(expected['bytes']) is int and expected['bytes'] >= 0
            raw = self.read(expected['path'], limit)
            assert len(raw) == expected['bytes']
            assert hashlib.sha256(raw).hexdigest() == expected['sha256']
            data[name] = raw
        assert type(row['exit']) is int and row['exit'] in (0, 2, 3)
        metrics = json.loads(data.pop('metrics'))
        assert metrics['wait_confirmed'] is True
        assert metrics['timed_out'] is False
        assert type(metrics['cancel_signal']) is int
        assert metrics['cancel_signal'] == 0
        assert type(metrics['child_wait']) is int
        assert metrics['child_wait'] == row['exit']
        wall = metrics['supervised_wall_ns']
        assert type(wall) is int and 0 <= wall < 30_000_000_000
        assert type(metrics['poll_ns']) is int
        assert metrics['poll_ns'] == 1000000
        # Inclusive fork/exec/wait includes requested 1 ms polling cadence and
        # scheduler delay; it is not isolated loader/startup timing.
        return SimpleNamespace(returncode=row['exit'], wall_ns=wall, **data)

    def consumed(self):
        assert not self.sequence, 'unconsumed captured chronology'
        assert not any(self.runs.values()), 'unconsumed captured invocations'


captured_runs = None


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
    if captured_runs is None:
        result = subprocess.run(argv, capture_output=True, timeout=30,
                                check=False)
        result.wall_ns = time.monotonic_ns() - started
    else:
        result = captured_runs.run(binary, args)
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
    global captured_runs
    p = argparse.ArgumentParser(allow_abbrev=False)
    p.add_argument('--baseline', type=Path, required=True)
    p.add_argument('--candidate', type=Path, required=True)
    p.add_argument('--output', type=Path, required=True)
    p.add_argument('--repetitions', type=int, default=3, choices=range(1, 6))
    p.add_argument('--iterations', type=int, default=20, choices=range(1, 1001))
    p.add_argument('--capture-dir', type=Path,
                   help='validate exported observations; execute no binary')
    a = p.parse_args()
    binaries = {k: v.resolve(strict=True) for k, v in
                [('baseline', a.baseline), ('candidate', a.candidate)]}
    assert all(v.is_file() for v in binaries.values())
    identities = {k: {'path': str(v), 'bytes': v.stat().st_size,
                     'sha256': hashlib.sha256(v.read_bytes()).hexdigest()}
                  for k, v in binaries.items()}
    if a.capture_dir:
        captured_runs = CapturedRuns(a.capture_dir, identities)
    # Fail closed on existing output; retain the first failed attempt intact.
    a.output.mkdir(mode=0o700)
    result = {'status': 'running', 'binaries': identities, 'cases': {},
              'timing_scope': 'synthetic construction/write/disposal only',
              'production_parity': 'pending actual producers/MFL',
              'six_target_comparison': 'pending; this receipt is one profile',
              'tracked_peak_scope': 'requested bytes, including copy-realloc '
              'overlap; excludes headers, allocator overhead and workspace'}
    if captured_runs is not None:
        result['capture_manifest_sha256'] = captured_runs.manifest_sha256
        result['execution_mode'] = 'captured native observations; no replay'
        result['process_wall_scope'] = ('native fork/exec/wait, including '
                                        '1 ms watchdog polling')
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
        if captured_runs is not None:
            captured_runs.consumed()
        result['status'] = ('observations complete; differences retained; '
                            'no adoption')
    except BaseException as e:
        result.update(status='failed', error=repr(e))
        raise
    finally:
        save()


if __name__ == '__main__':
    main()
