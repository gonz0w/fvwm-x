#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the Free
# Software Foundation; either version 2, or any later version.
"""Validate captured production-owner controls; never execute an exported ELF."""
import argparse
import json
from pathlib import Path

from content_memory_control import checked_wait, integer, read_rows, require

CONTROLS = {'rw-control': 1, 'noaccess-write': 2, 'readonly-write': 3,
            'zero-lock-limit': 4}
PROBE = 'native-content-payload'
LOCK_OPERATION = 6


def identity(row, control, os_name):
    require(row.get('probe') == PROBE and integer(row.get('control')) and
            row['control'] == CONTROLS[control], 'wrong probe/control')
    for field in ('uid', 'euid', 'memlock_soft', 'memlock_hard'):
        require(integer(row.get(field)), 'missing native identity: ' + field)
    require(row['memlock_soft'] <= row['memlock_hard'], 'invalid lock limits')
    require(type(row.get('eligible')) is bool, 'missing eligibility')
    require(not row['eligible'] or (row['uid'] > 0 and row['euid'] > 0),
            'root cannot establish strict lock profile')
    require(integer(row.get('dumpguard')) and
            row['dumpguard'] == (2 if os_name == 'Linux' else 1),
            'native process guard mismatch')
    page = row.get('page')
    require(integer(page, 1, 65536) and not page & (page - 1),
            'native page unavailable')
    return {name: row[name] for name in ('uid', 'euid', 'eligible',
            'dumpguard', 'page', 'memlock_soft', 'memlock_hard')}


def completed(row, control, os_name, negative):
    info = identity(row, control, os_name)
    require(row.get('done') is True and row.get('clean') is True and
            row.get('lock_refused') is negative, 'completion unavailable')
    for field in ('items', 'work', 'leases', 'mapped_model', 'locked_model'):
        require(integer(row.get(field)) and row[field] == 0,
                'remaining cleanup charge: ' + field)
    return info


def check_control(control, exit_code, metrics, rows, os_name, positive=None):
    require(control in CONTROLS, 'unknown control')
    require(os_name in ('Linux', 'FreeBSD', 'OpenBSD'), 'unknown native OS')
    checked_wait(metrics, exit_code)
    if exit_code == 77:
        require(metrics['child_signal'] == 0 and not rows,
                'unavailable after an observation or signal')
        return {'outcome': 'unavailable', 'control': control}
    if control == 'zero-lock-limit':
        require(exit_code == 0 and metrics['child_signal'] == 0,
                'lock control did not complete')
        require(positive and positive.get('outcome') == 'observed' and
                positive.get('control') == 'rw-control' and
                positive.get('os') == os_name and
                positive.get('explicit_cleanup') is True and
                positive.get('eligible') is True and
                integer(positive.get('uid'), 1) and
                integer(positive.get('euid'), 1) and
                integer(positive.get('page'), 1, 65536) and
                integer(positive.get('memlock_soft'), positive['page']),
                'matching unprivileged positive required')
        identity(dict(positive, probe=PROBE, control=CONTROLS['rw-control']),
                 'rw-control', os_name)
        require(len(rows) == 2, 'lock observation count')
        negative, done = rows
        info = completed(done, control, os_name, True)
        require(info['eligible'] is True and info['memlock_soft'] == 0 and
                info['memlock_hard'] == 0, 'verified zero limit unavailable')
        for field in ('uid', 'euid', 'page', 'dumpguard'):
            require(info[field] == positive[field], 'profile mismatch: ' + field)
        require(negative.get('probe') == PROBE and
                negative.get('negative') is True and
                integer(negative.get('operation')) and
                negative['operation'] == LOCK_OPERATION and
                integer(negative.get('errno'), 1), 'genuine lock refusal absent')
        page = info['page']
        for field, value in [('mapped_before', 3 * page),
                             ('locked_before', page), ('items_before', 1),
                             ('work_before', 0), ('leases_before', 0),
                             ('produced_before', 0)]:
            require(integer(negative.get(field)) and negative[field] == value,
                    'unexpected negative observation: ' + field)
        require(negative.get('closed_before') is True,
                'refused allocation not closed')
        return {'outcome': 'observed', 'control': control, 'os': os_name,
                'explicit_cleanup': True, **info}
    count = 2 if control == 'rw-control' else 1
    require(len(rows) == count, 'readiness/completion count')
    ready = rows[0]
    info = identity(ready, control, os_name)
    require(ready.get('ready') is True and ready.get('live') is True and
            integer(ready.get('work')) and ready['work'] == 0 and
            integer(ready.get('produced')) and ready['produced'] == 3,
            'producer/publication evidence absent')
    for field in ('sigsegv', 'sigbus'):
        require(integer(ready.get(field), 1, 127) and
                ready[field] == metrics[field], 'native signals mismatch')
    readonly = control == 'readonly-write'
    require(ready.get('closed') is (not readonly) and
            integer(ready.get('leases')) and
            ready['leases'] == int(readonly), 'wrong final access scope')
    for field, value in [('mapped_model', 3 * info['page']),
                         ('locked_model', info['page'])]:
        require(integer(ready.get(field)) and ready[field] == value,
                'reservation model mismatch')
    if control == 'rw-control':
        require(exit_code == 0 and metrics['child_signal'] == 0,
                'positive control did not complete')
        require(completed(rows[1], control, os_name, False) == info,
                'positive context changed')
    else:
        native_signal = metrics['child_signal']
        require(native_signal in (metrics['sigsegv'], metrics['sigbus']) and
                exit_code == 128 + native_signal, 'actual access fault absent')
    return {'outcome': 'observed', 'control': control, 'os': os_name,
            'explicit_cleanup': control == 'rw-control', **info}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--control', choices=CONTROLS, required=True)
    parser.add_argument('--os', choices=['Linux', 'FreeBSD', 'OpenBSD'],
                        required=True)
    parser.add_argument('--exit-code', type=int, required=True)
    parser.add_argument('--stdout', type=Path, required=True)
    parser.add_argument('--metrics', type=Path, required=True)
    parser.add_argument('--positive', type=Path)
    args = parser.parse_args()
    metrics = read_rows(args.metrics, 4096)
    require(len(metrics) == 1, 'supervisor observation count')
    positive = None
    if args.positive:
        values = read_rows(args.positive, 4096)
        require(len(values) == 1, 'positive receipt count')
        positive = values[0]
    result = check_control(args.control, args.exit_code, metrics[0],
                           read_rows(args.stdout, 16384), args.os, positive)
    print(json.dumps(result, sort_keys=True))


if __name__ == '__main__':
    main()
