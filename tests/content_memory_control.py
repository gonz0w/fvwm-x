#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or any later version.
"""Validate captured private memory controls; never execute a candidate."""
import argparse
import json
import os
import stat
from pathlib import Path

CONTROLS = {'rw-control': 1, 'noaccess-write': 2, 'readonly-write': 3}
CANDIDATES = {'base-os', 'sodium-1.0.22'}


def require(condition, message):
    if not condition:
        raise ValueError(message)


def integer(value, low=0, high=(1 << 64) - 1):
    return type(value) is int and low <= value <= high


def unique_pairs(pairs):
    result = {}
    for key, value in pairs:
        require(key not in result, 'duplicate JSON field')
        result[key] = value
    return result


def reject_constant(value):
    raise ValueError('nonfinite JSON: ' + value)


def read_rows(path, limit):
    require(path.is_file() and not path.is_symlink(),
            'regular capture required')
    descriptor = os.open(path, os.O_RDONLY | os.O_NOFOLLOW |
                         os.O_NONBLOCK | os.O_CLOEXEC)
    try:
        info = os.fstat(descriptor)
        require(stat.S_ISREG(info.st_mode) and info.st_size <= limit,
                'regular bounded capture required')
        with os.fdopen(descriptor, 'rb') as stream:
            descriptor = -1
            raw = stream.read(limit + 1)
    finally:
        if descriptor >= 0:
            os.close(descriptor)
    require(len(raw) <= limit, 'capture grew')
    rows = []
    for line in raw.decode('utf-8', errors='strict').splitlines():
        require(bool(line), 'empty capture line')
        row = json.loads(line, object_pairs_hook=unique_pairs,
                         parse_constant=reject_constant)
        require(isinstance(row, dict), 'object capture required')
        rows.append(row)
    require(len(rows) <= 8, 'too many records')
    return rows


def checked_wait(metrics, exit_code):
    require(type(metrics.get('wait_confirmed')) is bool and
            metrics['wait_confirmed'], 'unconfirmed child wait')
    require(metrics.get('timed_out') is False, 'deadline expired')
    require(integer(metrics.get('cancel_signal')) and
            metrics['cancel_signal'] == 0, 'cancelled observation')
    require(integer(metrics.get('supervised_wall_ns'), high=4999999999),
            'five-second observation bound')
    require(integer(metrics.get('poll_ns')) and metrics['poll_ns'] == 1000000,
            'unexpected supervisor cadence')
    require(integer(exit_code, high=255) and
            integer(metrics.get('child_wait'), high=255) and
            metrics['child_wait'] == exit_code, 'wrapper/child disagreement')
    require(integer(metrics.get('child_signal'), high=127), 'invalid signal')
    for name in ('sigsegv', 'sigbus'):
        require(integer(metrics.get(name), 1, 127), 'native signal unavailable')


def preparation(row, candidate, page):
    require(row.get('candidate') == candidate, 'candidate identity changed')
    require(row.get('page') == page and integer(page, 1, 65536),
            'page identity changed')
    for name in ('run', 'owner_epoch', 'bytes', 'written_bytes',
                 'mapped_model', 'locked_model', 'preparation', 'errno',
                 'injected', 'lock_attempted', 'lock_errno', 'accessed',
                 'wipe_observed', 'cleanup', 'accounting', 'prepare_ns',
                 'finish_ns'):
        require(integer(row.get(name)), 'missing numeric observation: ' + name)
    require(row['preparation'] <= 6 and row['injected'] <= 5,
            'unknown preparation/fault')
    for name in ('lock_attempted', 'accessed', 'wipe_observed',
                 'cleanup', 'accounting'):
        require(row[name] <= 1, 'invalid observation flag')
    require(row['written_bytes'] <= row['bytes'], 'unwritten length published')
    require(row['cleanup'] == 1 and row['accounting'] == 1,
            'cleanup/accounting incomplete')


def check_enforcement(control, exit_code, metrics, rows):
    require(control in CONTROLS, 'unknown control')
    checked_wait(metrics, exit_code)
    if exit_code == 77:
        require(metrics['child_signal'] == 0 and
                not any('ready' in row for row in rows),
                'unavailable after readiness or signal')
        require(len(rows) <= 1, 'unexpected unavailable observations')
        if rows:
            row = rows[0]
            candidate, page = row.get('candidate'), row.get('page')
            require(candidate in CANDIDATES, 'unknown unavailable candidate')
            preparation(row, candidate, page)
            require(row['preparation'] != 0 and row['written_bytes'] == 0 and
                    row['accessed'] == 0, 'unavailable after producer access')
        return {'outcome': 'unavailable', 'control': control}
    expected_count = 2 if control == 'rw-control' else 1
    require(len(rows) == expected_count, 'readiness/observation count')
    ready = rows[0]
    candidate = ready.get('candidate')
    require(candidate in CANDIDATES and ready.get('ready') is True and
            integer(ready.get('control')) and
            ready['control'] == CONTROLS[control], 'wrong readiness')
    require(ready.get('sigsegv') == metrics['sigsegv'] and
            ready.get('sigbus') == metrics['sigbus'], 'native signal mismatch')
    if control == 'rw-control':
        require(exit_code == 0 and metrics['child_signal'] == 0,
                'positive access control did not complete')
        row = rows[1]
        page = row.get('page')
        preparation(row, candidate, page)
        require(row['preparation'] == 0 and row['injected'] == 0 and
                row['bytes'] == page and row['written_bytes'] == page - 1 and
                row['lock_attempted'] == 1 and row['lock_errno'] == 0 and
                row['accessed'] == 1 and row['wipe_observed'] == 1,
                'positive preparation/access/wipe incomplete')
        return {'outcome': 'observed', 'control': control,
                'candidate': candidate, 'page': page, 'explicit_cleanup': True}
    signal = metrics['child_signal']
    require(signal in (metrics['sigsegv'], metrics['sigbus']) and
            exit_code == 128 + signal, 'expected access signal not observed')
    return {'outcome': 'observed', 'control': control, 'candidate': candidate,
            'explicit_cleanup': False, 'child_signal': signal}


def lock_header(header, candidate, os_name):
    require(header.get('candidate') == candidate and
            header.get('lock_profile') is True and
            header.get('lock_limit_zero') is True and
            integer(header.get('uid'), 1) and integer(header.get('euid'), 1) and
            type(header.get('capabilities_checked')) is int and
            header['capabilities_checked'] == int(os_name == 'Linux') and
            header.get('ipc_lock_capability') is False,
            'unprivileged lock profile not established')


def check_lock_refusal(exit_code, metrics, rows, positive, os_name):
    checked_wait(metrics, exit_code)
    require(os_name in ('Linux', 'FreeBSD', 'OpenBSD'),
            'unknown native profile')
    require(positive.get('outcome') == 'observed' and
            positive.get('control') == 'rw-control' and
            positive.get('candidate') in CANDIDATES and
            integer(positive.get('page'), 1, 65536),
            'matching positive control required')
    require(metrics['child_signal'] == 0, 'lock control signalled')
    candidate, page = positive['candidate'], positive['page']
    if exit_code == 77 and not rows:
        return {'outcome': 'unavailable', 'control': 'zero-lock-limit'}
    require(len(rows) in (1, 8), 'lock observation count')
    lock_header(rows[0], candidate, os_name)
    if exit_code == 77 and len(rows) == 1:
        return {'outcome': 'unavailable', 'control': 'zero-lock-limit'}
    require(exit_code in (0, 77) and len(rows) == 8,
            'lock control did not complete')
    observations, summary = rows[1:-1], rows[-1]
    sizes = (0, 1, page - 1, page, page + 1, 65536)
    all_refused = True
    for ordinal, (row, size) in enumerate(zip(observations, sizes)):
        preparation(row, candidate, page)
        require(row['run'] == 0 and row['owner_epoch'] == ordinal + 1 and
                row['bytes'] == size and row['written_bytes'] == 0 and
                row['injected'] == 0 and row['accessed'] == 0,
                'unexpected lock trial or producer bytes')
        refused = (row['preparation'] == 2 and row['lock_attempted'] == 1 and
                   row['lock_errno'] != 0 and row['wipe_observed'] == 1)
        all_refused = all_refused and refused
        if not refused:
            require(exit_code == 77 and row['preparation'] != 0 and
                    not row['lock_attempted'] and not row['lock_errno'],
                    'genuine checked lock refusal not observed')
    require(summary.get('summary') is True and
            summary.get('candidate') == candidate, 'missing summary')
    require((exit_code == 0) == all_refused, 'inconsistent lock outcome')
    return {'outcome': 'observed' if all_refused else 'unavailable',
            'control': 'zero-lock-limit', 'candidate': candidate, 'page': page,
            'explicit_cleanup': True}


def main():
    parser = argparse.ArgumentParser(allow_abbrev=False)
    parser.add_argument('--control', choices=(*CONTROLS, 'zero-lock-limit'),
                        required=True)
    parser.add_argument('--exit', type=int, required=True)
    parser.add_argument('--metrics', type=Path, required=True)
    parser.add_argument('--stdout', type=Path, required=True)
    parser.add_argument('--positive', type=Path)
    parser.add_argument('--os', choices=('Linux', 'FreeBSD', 'OpenBSD'))
    args = parser.parse_args()
    metrics = read_rows(args.metrics, 4096)
    require(len(metrics) == 1, 'one supervisor receipt required')
    rows = read_rows(args.stdout, 32768)
    if args.control == 'zero-lock-limit':
        require(args.positive is not None and args.os is not None,
                'positive control and native OS required')
        positive = read_rows(args.positive, 4096)
        require(len(positive) == 1, 'one positive-control receipt required')
        result = check_lock_refusal(args.exit, metrics[0], rows,
                                    positive[0], args.os)
    else:
        result = check_enforcement(args.control, args.exit, metrics[0], rows)
    print(json.dumps(result, sort_keys=True))


if __name__ == '__main__':
    main()
