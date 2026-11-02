#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or any later version.
"""Pure captured-observation oracle cases; no native payload execution."""
import unittest

from content_payload_control import check_control
from test_content_memory_control import metrics


def identity(control=1, os_name='Linux'):
    return dict(probe='native-content-payload', control=control, uid=65534,
                euid=65534, eligible=True, dumpguard=2 if os_name == 'Linux'
                else 1, page=4096, memlock_soft=65536, memlock_hard=65536)


def ready(control=1, os_name='Linux'):
    return identity(control, os_name) | dict(
        ready=True, live=True, closed=control != 3, leases=int(control == 3),
        work=0, produced=3, mapped_model=12288, locked_model=4096,
        sigsegv=11, sigbus=7)


def done(control=1, os_name='Linux'):
    return identity(control, os_name) | dict(
        done=True, clean=True, lock_refused=control == 4, items=0, work=0,
        leases=0, mapped_model=0, locked_model=0)


def positive(os_name='Linux'):
    return check_control('rw-control', 0, metrics(),
                         [ready(os_name=os_name), done(os_name=os_name)],
                         os_name)


def negative(os_name='Linux'):
    observed = dict(probe='native-content-payload', negative=True,
                    operation=6, errno=12, mapped_before=12288,
                    locked_before=4096, closed_before=True, items_before=1,
                    work_before=0, leases_before=0, produced_before=0)
    completed = done(4, os_name) | dict(memlock_soft=0, memlock_hard=0)
    return [observed, completed]


class PayloadControls(unittest.TestCase):
    def test_native_control_paths(self):
        for os_name in ('Linux', 'FreeBSD', 'OpenBSD'):
            observed = positive(os_name)
            self.assertTrue(observed['explicit_cleanup'])
            locked = check_control('zero-lock-limit', 0, metrics(),
                                   negative(os_name), os_name, observed)
            self.assertEqual(locked['outcome'], 'observed')
            for mode, control in [('noaccess-write', 2),
                                  ('readonly-write', 3)]:
                for signal in (7, 11):
                    result = check_control(
                        mode, 128 + signal, metrics(128 + signal, signal),
                        [ready(control, os_name)], os_name)
                    self.assertFalse(result['explicit_cleanup'])

    def test_numeric_exit_and_unrelated_signal_refused(self):
        for code, signal in [(139, 0), (137, 9), (0, 0)]:
            with self.assertRaises(ValueError):
                check_control('noaccess-write', code, metrics(code, signal),
                              [ready(2)], 'Linux')

    def test_exact_scopes_and_completion_required(self):
        for field, value in [('live', False), ('closed', False), ('leases', 1),
                             ('work', 1), ('produced', 2),
                             ('mapped_model', 4096), ('locked_model', 0),
                             ('sigsegv', True), ('dumpguard', 1),
                             ('page', 4095), ('eligible', 1)]:
            with self.subTest(field=field), self.assertRaises(ValueError):
                check_control('rw-control', 0, metrics(),
                              [ready() | {field: value}, done()], 'Linux')
        for field, value in [('clean', False), ('items', 1), ('leases', True),
                             ('mapped_model', 12288), ('lock_refused', True),
                             ('uid', 65533)]:
            with self.subTest(field=field), self.assertRaises(ValueError):
                check_control('rw-control', 0, metrics(),
                              [ready(), done() | {field: value}], 'Linux')
        with self.assertRaises(ValueError):
            check_control('readonly-write', 139, metrics(139, 11),
                          [ready(3) | dict(closed=True)], 'Linux')

    def test_matched_unprivileged_positive_required(self):
        receipt = positive()
        bad = [None]
        bad += [receipt | {field: value} for field, value in [
            ('uid', 0), ('euid', 0), ('uid', 65533), ('eligible', False),
            ('os', 'FreeBSD'), ('explicit_cleanup', False),
            ('dumpguard', 1), ('memlock_soft', 0), ('memlock_hard', 1),
            ('page', 4095), ('page', 8192)]]
        for invalid in bad:
            with self.subTest(receipt=invalid), self.assertRaises(ValueError):
                check_control('zero-lock-limit', 0, metrics(), negative(),
                              'Linux', invalid)

    def test_refusal_and_cleanup_not_inferred_from_exit(self):
        for index, field, value in [
                (0, 'errno', 0), (0, 'operation', 5), (0, 'negative', False),
                (0, 'closed_before', False), (0, 'mapped_before', 0),
                (0, 'locked_before', 0), (0, 'items_before', True),
                (0, 'work_before', 1), (0, 'leases_before', 1),
                (0, 'produced_before', 3), (1, 'eligible', False),
                (1, 'memlock_soft', 1), (1, 'memlock_hard', 1),
                (1, 'lock_refused', False), (1, 'clean', False),
                (1, 'locked_model', 4096)]:
            rows = negative()
            rows[index][field] = value
            with self.subTest(field=field), self.assertRaises(ValueError):
                check_control('zero-lock-limit', 0, metrics(), rows,
                              'Linux', positive())

    def test_unavailable_and_wait_limits(self):
        self.assertEqual(check_control('rw-control', 77, metrics(77), [],
                                       'Linux')['outcome'], 'unavailable')
        for rows in [[ready()], [done()]]:
            with self.assertRaises(ValueError):
                check_control('rw-control', 77, metrics(77), rows, 'Linux')
        for field, value in [('timed_out', True), ('cancel_signal', 2),
                             ('wait_confirmed', False),
                             ('supervised_wall_ns', 5000000000),
                             ('child_wait', 1), ('poll_ns', True)]:
            with self.subTest(field=field), self.assertRaises(ValueError):
                check_control('rw-control', 0, metrics() | {field: value},
                              [ready(), done()], 'Linux')


if __name__ == '__main__':
    unittest.main()
