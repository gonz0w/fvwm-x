#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or any later version.
from pathlib import Path
import tempfile
import unittest

from content_memory_control import (check_enforcement, check_lock_refusal,
                                    read_rows)


def metrics(code=0, signal=0, bus=7):
    return dict(wait_confirmed=True, timed_out=False, cancel_signal=0,
                supervised_wall_ns=1000000, poll_ns=1000000,
                child_wait=code, child_signal=signal, sigsegv=11, sigbus=bus)


def observation(size=4096):
    return dict(candidate='base-os', page=4096, run=0, owner_epoch=1,
                bytes=size, written_bytes=max(size - 1, 0), mapped_model=12288,
                locked_model=4096, preparation=0, errno=0, injected=0,
                lock_attempted=1, lock_errno=0, region_kind=1,
                producer_closed=1, reader_acquired=1, accessed=1,
                wipe_observed=1,
                cleanup=1, accounting=1, prepare_ns=1, finish_ns=1)


def ready(control=1, bus=7):
    return dict(candidate='base-os', ready=True, control=control,
                sigsegv=11, sigbus=bus, uid=1000, euid=1000,
                unprivileged_lock_profile=True, dumpguard=2,
                region_kind=1, page=4096, memlock_soft=8388608,
                memlock_hard=8388608)


def positive(os_name='Linux'):
    prepared = ready()
    prepared['dumpguard'] = 2 if os_name == 'Linux' else 1
    return check_enforcement('rw-control', 0, metrics(),
                             [prepared, observation()], os_name)


def lock_rows():
    rows = [dict(candidate='base-os', lock_profile=True, lock_limit_zero=True,
                 uid=1000, euid=1000, capabilities_checked=1,
                 ipc_lock_capability=False, dumpguard=2)]
    for i, size in enumerate((0, 1, 4095, 4096, 4097, 65536)):
        row = observation(size)
        row.update(owner_epoch=i + 1, written_bytes=0, preparation=2,
                   errno=12, lock_errno=12, producer_closed=0,
                   reader_acquired=0, accessed=0)
        rows.append(row)
    return rows + [dict(candidate='base-os', summary=True)]


class Controls(unittest.TestCase):
    def test_positive_and_native_signals(self):
        self.assertTrue(positive()['explicit_cleanup'])
        for mode, control in [('noaccess-write', 2), ('readonly-write', 3)]:
            for signal, bus in [(11, 7), (10, 10)]:
                result = check_enforcement(
                    mode, 128 + signal,
                    metrics(128 + signal, signal, bus),
                    [ready(control, bus)], 'Linux')
                self.assertEqual(result['outcome'], 'observed')
                self.assertFalse(result['explicit_cleanup'])

    def test_exit_139_is_not_a_signal(self):
        with self.assertRaises(ValueError):
            check_enforcement(
                'noaccess-write', 139, metrics(139), [ready(2)], 'Linux')
        with self.assertRaises(ValueError):
            check_enforcement(
                'noaccess-write', 137, metrics(137, 9), [ready(2)], 'Linux')

    def test_readiness_and_cleanup_required(self):
        for rows in ([], [ready(3)], [ready(2), ready(2)]):
            with self.assertRaises(ValueError):
                check_enforcement(
                    'noaccess-write', 139, metrics(139, 11), rows, 'Linux')
        for field in ('cleanup', 'accounting', 'wipe_observed', 'accessed'):
            row = observation()
            row[field] = 0
            with self.assertRaises(ValueError):
                check_enforcement(
                    'rw-control', 0, metrics(), [ready(), row], 'Linux')

    def test_wait_deadline_and_typed_metrics(self):
        for field, value in [('timed_out', True), ('cancel_signal', 2),
                             ('wait_confirmed', False),
                             ('supervised_wall_ns', 5000000000),
                             ('child_wait', 0), ('poll_ns', True)]:
            meta = metrics(139, 11)
            meta[field] = value
            with self.assertRaises(ValueError):
                check_enforcement(
                    'noaccess-write', 139, meta, [ready(2)], 'Linux')

    def test_dump_guard_matches_native_profile(self):
        for value in (0, 1, True, '2'):
            row = ready() | dict(dumpguard=value)
            with self.assertRaises(ValueError):
                check_enforcement('rw-control', 0, metrics(),
                                  [row, observation()], 'Linux')
        with self.assertRaises(ValueError):
            check_enforcement('rw-control', 0, metrics(),
                              [ready(), observation()], 'FreeBSD')

    def test_regional_mechanism_is_not_process_guard(self):
        for value in (0, 2, 3, True):
            row = ready() | dict(region_kind=value)
            with self.assertRaises(ValueError):
                check_enforcement('rw-control', 0, metrics(),
                                  [row, observation()], 'Linux')

    def test_configured_sodium_mapping_is_profile_specific(self):
        prepared = ready() | dict(candidate='sodium-1.0.22', dumpguard=1,
                                  region_kind=3)
        row = observation() | dict(candidate='sodium-1.0.22', region_kind=3)
        result = check_enforcement('rw-control', 0, metrics(),
                                   [prepared, row], 'OpenBSD')
        self.assertEqual(result['region_kind'], 3)
        with self.assertRaises(ValueError):
            check_enforcement('rw-control', 0, metrics(),
                              [prepared, row], 'FreeBSD')
        prepared['candidate'] = row['candidate'] = 'base-os'
        with self.assertRaises(ValueError):
            check_enforcement('rw-control', 0, metrics(),
                              [prepared, row], 'OpenBSD')

    def test_unavailable_never_passes_after_readiness(self):
        self.assertEqual(check_enforcement('noaccess-write', 77,
                         metrics(77), [], 'Linux')['outcome'], 'unavailable')
        row = observation()
        row.update(preparation=3, written_bytes=0, producer_closed=0,
                   reader_acquired=0, accessed=0)
        self.assertEqual(check_enforcement('rw-control', 77, metrics(77),
                         [row], 'Linux')['outcome'], 'unavailable')
        with self.assertRaises(ValueError):
            check_enforcement(
                'noaccess-write', 77, metrics(77), [ready(2)], 'Linux')

    def test_genuine_lock_refusal(self):
        result = check_lock_refusal(
            0, metrics(), lock_rows(), positive(), 'Linux')
        self.assertEqual(result['outcome'], 'observed')
        rows = lock_rows()
        rows[0]['capabilities_checked'] = 0
        rows[0]['dumpguard'] = 1
        self.assertEqual(check_lock_refusal(
                         0, metrics(), rows, positive('OpenBSD'),
                         'OpenBSD')['outcome'], 'observed')

    def test_lock_prerequisites_and_actual_call(self):
        for field, value in [('euid', 0), ('ipc_lock_capability', True),
                             ('capabilities_checked', 0)]:
            rows = lock_rows()
            rows[0][field] = value
            with self.assertRaises(ValueError):
                check_lock_refusal(0, metrics(), rows, positive(), 'Linux')
        for field, value in [('lock_attempted', 0), ('lock_errno', 0),
                             ('injected', 2), ('written_bytes', 1)]:
            rows = lock_rows()
            rows[2][field] = value
            with self.assertRaises(ValueError):
                check_lock_refusal(0, metrics(), rows, positive(), 'Linux')
        with self.assertRaises(ValueError):
            check_lock_refusal(0, metrics(), lock_rows(), {}, 'Linux')

    def test_lock_positive_profile_must_match(self):
        root_positive = positive()
        for changes in ({'uid': 0, 'euid': 0,
                         'unprivileged_lock_profile': False},
                        {'uid': 1001}, {'euid': 1001},
                        {'uid': True}, {'memlock_soft': 0},
                        {'unprivileged_lock_profile': False}):
            mismatch = root_positive | changes
            with self.assertRaises(ValueError):
                check_lock_refusal(0, metrics(), lock_rows(), mismatch, 'Linux')

        for field in ('uid', 'euid', 'unprivileged_lock_profile'):
            missing = positive()
            del missing[field]
            with self.assertRaises(ValueError):
                check_lock_refusal(0, metrics(), lock_rows(), missing, 'Linux')
        root_ready = ready() | dict(uid=0, euid=0,
                                    unprivileged_lock_profile=False)
        root = check_enforcement('rw-control', 0, metrics(),
                                 [root_ready, observation()], 'Linux')
        self.assertEqual(root['outcome'], 'observed')

    def test_early_refusal_is_inconclusive(self):
        rows = lock_rows()
        rows[1].update(preparation=3, lock_attempted=0, lock_errno=0)
        self.assertEqual(check_lock_refusal(
                         77, metrics(77), rows, positive(),
                         'Linux')['outcome'], 'unavailable')
        with self.assertRaises(ValueError):
            check_lock_refusal(0, metrics(), rows, positive(), 'Linux')
        with self.assertRaises(ValueError):
            check_lock_refusal(
                77, metrics(77), lock_rows(), positive(), 'Linux')

    def test_benchmark_rows_have_an_explicit_bound(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'capture'
            path.write_bytes(b'{}\n' * 9)
            self.assertEqual(len(read_rows(path, 4096, max_rows=9)), 9)
            for bound in (0, 129, True):
                with self.assertRaises(ValueError):
                    read_rows(path, 4096, max_rows=bound)

    def test_bounded_strict_capture(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'capture'
            for raw in (b'{"a":1,"a":2}\n', b'{"x":NaN}\n', b'[]\n',
                        b'\xff', b'\n', b'{}\n' * 9):
                path.write_bytes(raw)
                with self.assertRaises((ValueError, UnicodeError)):
                    read_rows(path, 4096)
            path.write_bytes(b'{}\n')
            self.assertEqual(read_rows(path, 4096), [{}])
            with self.assertRaises(ValueError):
                read_rows(path, 1)
            link = Path(directory) / 'link'
            link.symlink_to(path)
            with self.assertRaises(ValueError):
                read_rows(link, 4096)


if __name__ == '__main__':
    unittest.main()
