#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
"""Synthetic Linux proc and timeline contracts for session observations."""
import json
import os
from pathlib import Path
import shutil
import sys
import tempfile
import time
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parent / 'live_x'))
from session_observer import SessionObserver, parse_stat


class Timeline:
    def __init__(self):
        self.now = 10.0

    def __call__(self):
        return self.now


def stat(pid, parent=1, start=100, cpu=0, name='name (with) spaces'):
    fields = ['S', str(parent)] + ['0'] * 48
    fields[11], fields[12], fields[19] = str(cpu), '0', str(start)
    return f'{pid} ({name}) ' + ' '.join(fields) + '\n'


def process(root, pid=10, parent=1, start=100, cpu=0, rss=50, hwm=80,
            pss=30, tasks=((10, 100, 3),)):
    folder = root / str(pid)
    folder.mkdir(exist_ok=True)
    (folder / 'stat').write_text(stat(pid, parent, start, cpu))
    (folder / 'status').write_text(
        f'Name:\tfixture\nVmRSS:\t{rss} kB\nVmHWM:\t{hwm} kB\n'
        'voluntary_ctxt_switches:\t9999\nnonvoluntary_ctxt_switches:\t9\n')
    if pss is not None:
        (folder / 'smaps_rollup').write_text(f'Pss:\t{pss} kB\n')
    else:
        (folder / 'smaps_rollup').unlink(missing_ok=True)
    task_root = folder / 'task'
    if task_root.exists():
        shutil.rmtree(task_root)
    task_root.mkdir()
    for tid, task_start, switches in tasks:
        task = task_root / str(tid)
        task.mkdir()
        (task / 'stat').write_text(stat(tid, parent, task_start, 999))
        (task / 'status').write_text(
            f'voluntary_ctxt_switches:\t{switches}\n'
            'nonvoluntary_ctxt_switches:\t2\n')


class ObserverFixtures(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)
        self.clock = Timeline()
        self.observer = SessionObserver(10, 9.9, proc_root=self.root,
                                        clock=self.clock)

    def sample(self, advance=0):
        self.clock.now += advance
        self.observer._sample()
        return self.observer.report()['snapshots'][-1]

    def test_stat_comm_parentheses_and_spaces(self):
        record = parse_stat(stat(10, 4, 123, 42))
        self.assertEqual(record, {'pid': 10, 'ppid': 4,
                                  'start_ticks': 123, 'cpu_ticks': 42})

    def test_stat_refuses_malformed_truncated_and_negative(self):
        for value in ('', '10 malformed', stat(10).rsplit(' ', 40)[0],
                      stat(10, start=-1), stat(10, cpu=-1)):
            with self.subTest(value=value), self.assertRaises(ValueError):
                parse_stat(value)

    def test_process_cpu_and_memory_once_all_thread_switches(self):
        process(self.root, cpu=20, tasks=((10, 100, 3), (11, 102, 7)))
        self.sample()
        process(self.root, cpu=32, tasks=((10, 100, 8), (11, 102, 16)))
        sample = self.sample(2)
        self.assertEqual(sample['rss_sum_kib'], 50)
        self.assertEqual(sample['pss_sum_kib'], 30)
        interval = self.observer.report()['intervals'][-1]
        self.assertEqual(interval['cpu_ticks'], 12)
        self.assertEqual(interval['context_switches'], 14)
        self.assertEqual(interval['matched_tasks'], 2)
        self.assertEqual(interval['seconds'], 2)
        self.assertAlmostEqual(interval['cpu_ms'],
                               12000 / os.sysconf('SC_CLK_TCK'))
        json.dumps(self.observer.report(), allow_nan=False)

    def test_descendant_survives_reparenting_and_pid_reuse_is_not_adopted(self):
        process(self.root)
        process(self.root, pid=20, parent=10, start=200, tasks=((20, 200, 0),))
        self.assertEqual(len(self.sample()['processes']), 2)
        process(self.root, pid=20, parent=1, start=200, tasks=((20, 200, 0),))
        self.assertEqual(len(self.sample(1)['processes']), 2)
        process(self.root, pid=20, parent=1, start=300, tasks=((20, 300, 0),))
        self.assertEqual(len(self.sample(1)['processes']), 1)
        interval = self.observer.report()['intervals'][-1]
        self.assertEqual(interval['vanished_processes'], 1)

    def test_root_reuse_cannot_adopt_unrelated_process_or_children(self):
        process(self.root)
        self.sample()
        process(self.root, start=999)
        process(self.root, pid=30, parent=10, start=1000,
                tasks=((30, 1000, 0),))
        sample = self.sample(1)
        self.assertEqual(sample['processes'], [])
        self.assertIn('root_identity_lost', sample['issues'])
        self.assertIsNone(sample['rss_sum_kib'])

    def test_new_and_vanished_tasks_do_not_invent_counter_deltas(self):
        process(self.root, tasks=((10, 100, 10), (11, 200, 20)))
        self.sample()
        process(self.root, tasks=((10, 100, 12), (11, 201, 900), (12, 202, 50)))
        self.sample(1)
        interval = self.observer.report()['intervals'][-1]
        self.assertEqual(interval['context_switches'], 2)
        self.assertEqual(interval['fresh_tasks'], 2)
        self.assertEqual(interval['vanished_tasks'], 1)

    def test_counter_regression_is_invalid_not_negative(self):
        process(self.root, cpu=10, tasks=((10, 100, 30),))
        self.sample()
        process(self.root, cpu=1, tasks=((10, 100, 2),))
        self.sample(1)
        interval = self.observer.report()['intervals'][-1]
        self.assertIsNone(interval['cpu_ticks'])
        self.assertIsNone(interval['context_switches'])
        self.assertEqual(interval['cpu_regressions'], 1)
        self.assertEqual(interval['switch_regressions'], 1)
        self.assertEqual(interval['status'], 'partial')

    def test_unknown_pss_is_null_with_known_partial_sum(self):
        process(self.root)
        process(self.root, pid=20, parent=10, pss=None,
                tasks=((20, 100, 0),))
        sample = self.sample()
        self.assertIsNone(sample['pss_sum_kib'])
        self.assertEqual(sample['known_pss_sum_kib'], 30)
        self.assertEqual(sample['pss_missing_processes'], 1)
        self.assertEqual(sample['status'], 'partial')

    def test_unknown_rss_is_not_a_complete_sum(self):
        process(self.root)
        process(self.root, pid=20, parent=10, tasks=((20, 100, 0),))
        (self.root / '20/status').unlink()
        sample = self.sample()
        self.assertIsNone(sample['rss_sum_kib'])
        self.assertEqual(sample['known_rss_sum_kib'], 50)
        self.assertEqual(sample['rss_missing_processes'], 1)

    def test_identity_change_during_read_discards_mixed_process_record(self):
        process(self.root)
        from session_observer import _read
        calls = 0

        def racing_read(path, maximum=65536):
            nonlocal calls
            if path == self.root / '10/stat':
                calls += 1
                if calls == 2:
                    return stat(10, start=999)
            return _read(path, maximum)

        with patch('session_observer._read', side_effect=racing_read):
            sample = self.sample()
        self.assertEqual(sample['processes'], [])
        self.assertTrue(any('identity changed' in i for i in sample['issues']))

    def test_changed_parent_cannot_adopt_or_retain_unknown_child(self):
        from session_observer import _read

        for parent_pid, disappear in ((10, False), (10, True), (20, False)):
            with self.subTest(parent_pid=parent_pid, disappear=disappear):
                observer = SessionObserver(10, 9.9, proc_root=self.root,
                                           clock=self.clock)
                process(self.root)
                if parent_pid == 20:
                    process(self.root, pid=20, parent=10, start=200,
                            tasks=((20, 200, 0),))
                    process(self.root, pid=40, parent=20, start=400,
                            tasks=((40, 400, 0),))
                    observer._sample()
                    # A previously known child keeps its identity even when
                    # reparented away from the now-invalid ancestor.
                    process(self.root, pid=40, parent=1, start=400,
                            tasks=((40, 400, 0),))
                process(self.root, pid=30, parent=parent_pid, start=1000,
                        tasks=((30, 1000, 0),))
                process(self.root, pid=50, parent=30, start=1100,
                        tasks=((50, 1100, 0),))
                calls = 0

                def racing_read(path, maximum=65536):
                    nonlocal calls
                    if path == self.root / str(parent_pid) / 'stat':
                        calls += 1
                        if calls == 2:
                            if disappear:
                                raise FileNotFoundError('parent vanished')
                            return stat(parent_pid, start=999)
                    return _read(path, maximum)

                with patch('session_observer._read', side_effect=racing_read):
                    observer._sample()
                records = observer.report()['snapshots'][-1]['processes']
                self.assertNotIn(30, [record['pid'] for record in records])
                self.assertNotIn((30, 1000), observer._known)
                self.assertNotIn(50, [record['pid'] for record in records])
                self.assertNotIn((50, 1100), observer._known)
                if parent_pid == 20:
                    self.assertIn(40, [record['pid'] for record in records])
                process(self.root, pid=30, parent=1, start=1000,
                        tasks=((30, 1000, 0),))
                observer._sample()
                records = observer.report()['snapshots'][-1]['processes']
                self.assertNotIn(30, [record['pid'] for record in records])
                # Clear this timeline before the next parent variant.
                for folder in self.root.iterdir():
                    shutil.rmtree(folder)

    def test_vanishing_task_and_denied_pss_are_explicit(self):
        process(self.root)
        from session_observer import _read

        def unavailable_read(path, maximum=65536):
            if path.name == 'smaps_rollup':
                raise PermissionError('fixture denied')
            if path == self.root / '10/task/10/stat':
                raise FileNotFoundError('fixture vanished')
            return _read(path, maximum)

        with patch('session_observer._read', side_effect=unavailable_read):
            sample = self.sample()
        self.assertIsNone(sample['pss_sum_kib'])
        self.assertEqual(sample['processes'][0]['tasks'], [])
        self.assertEqual(sample['status'], 'partial')
        self.assertTrue(any('denied' in i for i in sample['issues']))
        self.assertTrue(any('vanished' in i for i in sample['issues']))

    def test_missing_thread_status_remains_visible_partial_record(self):
        process(self.root, tasks=((10, 100, 3), (11, 101, 7)))
        (self.root / '10/task/11/status').unlink()
        sample = self.sample()
        self.assertEqual(len(sample['processes'][0]['tasks']), 2)
        self.assertIsNone(sample['processes'][0]['tasks'][1]['context_switches'])
        self.assertEqual(sample['status'], 'partial')

    def test_truncated_or_malformed_process_never_silently_disappears(self):
        process(self.root)
        self.sample()
        (self.root / '10/stat').write_text('10 (broken) S 1')
        sample = self.sample(1)
        self.assertEqual(sample['status'], 'partial')
        self.assertTrue(any('stat' in issue for issue in sample['issues']))
        interval = self.observer.report()['intervals'][-1]
        self.assertEqual(interval['vanished_processes'], 1)

    def test_sample_time_gaps_and_phase_memory_are_observed(self):
        process(self.root, rss=80, hwm=900)
        self.sample()
        self.observer.mark_phase('idle')
        process(self.root, rss=60, hwm=1000, cpu=10)
        self.sample(3)
        self.observer.mark_phase('activity')
        process(self.root, rss=90, cpu=20)
        self.sample(1)
        report = self.observer.report()
        self.assertAlmostEqual(report['first_sample_delay_seconds'], .1)
        self.assertEqual(report['max_sample_gap_seconds'], 3)
        self.assertEqual(report['phases']['idle']['sampled_rss_peak_kib'], 80)
        self.assertEqual(report['phases']['idle']['retained_rss_kib'], 60)
        self.assertEqual(report['phases']['idle']['duration_seconds'], 3)
        self.assertEqual(report['phases']['idle']['observed_cpu_seconds'], 3)
        expected_ms = 10000 / os.sysconf('SC_CLK_TCK')
        self.assertEqual(report['phases']['idle']['cpu_ms'], expected_ms)
        self.assertAlmostEqual(report['phases']['idle']['cpu_percent'],
                               expected_ms / 30)
        self.assertEqual(report['phases']['activity']['sampled_rss_peak_kib'], 90)
        self.assertNotIn('hwm_sum_kib', report['phases']['idle'])

    def test_nonpositive_time_interval_is_invalid(self):
        process(self.root)
        self.sample()
        self.sample()
        interval = self.observer.report()['intervals'][-1]
        self.assertEqual(interval['status'], 'invalid_time')
        self.assertIsNone(interval['cpu_ms'])

    def test_read_bound_and_fifo_do_not_block(self):
        process(self.root)
        (self.root / '10/status').write_bytes(b'x' * 65537)
        (self.root / '10/smaps_rollup').unlink()
        os.mkfifo(self.root / '10/smaps_rollup')
        before = time.monotonic()
        sample = self.sample()
        self.assertLess(time.monotonic() - before, .5)
        self.assertEqual(sample['status'], 'partial')
        self.assertIsNone(sample['processes'][0]['rss_kib'])

    def test_process_and_task_caps_report_partial(self):
        process(self.root)
        self.observer.MAX_PROCESSES = 2
        self.observer.MAX_TASKS = 1
        process(self.root, pid=20, parent=10, tasks=((20, 100, 0),))
        process(self.root, pid=30, parent=10, tasks=((30, 100, 0),))
        sample = self.sample()
        self.assertLessEqual(len(sample['processes']), 2)
        self.assertLessEqual(sum(len(p['tasks']) for p in sample['processes']), 1)
        self.assertEqual(sample['status'], 'partial')
        self.assertTrue(any('limit' in issue for issue in sample['issues']))

    def test_sample_cap_is_finite_and_visible(self):
        self.observer = SessionObserver(10, 9.9, proc_root=self.root,
                                        clock=self.clock, max_samples=2)
        process(self.root)
        for _ in range(5):
            self.sample(.1)
        self.assertEqual(len(self.observer.report()['snapshots']), 2)
        self.assertIn('sample_limit', self.observer.report()['limits_reached'])

    def test_stop_joins_owned_thread_and_is_idempotent(self):
        process(self.root)
        observer = SessionObserver(10, time.monotonic(), proc_root=self.root,
                                   interval=30, max_samples=2)
        observer.start()
        self.addCleanup(observer.stop)
        before = time.monotonic()
        observer.stop()
        observer.stop()
        self.assertLess(time.monotonic() - before, 1)
        self.assertFalse(observer._thread.is_alive())
        self.assertFalse(observer._thread.daemon)
        self.assertEqual(observer.report()['lifecycle'], 'stopped')

    def test_background_failure_is_explicit_and_joined(self):
        process(self.root)
        with patch.object(self.observer, '_sample',
                          side_effect=RuntimeError('fixture')):
            self.observer.start()
            self.observer._thread.join(timeout=1)
            self.observer.stop()
        report = self.observer.report()
        self.assertEqual(report['lifecycle'], 'failed')
        self.assertIn('fixture', report['error'])
        self.assertFalse(self.observer._thread.is_alive())

    def test_background_sample_limit_ends_without_stop(self):
        process(self.root)
        observer = SessionObserver(10, time.monotonic(), proc_root=self.root,
                                   interval=.001, max_samples=2)
        observer.start()
        self.addCleanup(observer.stop)
        observer._thread.join(timeout=1)
        self.assertFalse(observer._thread.is_alive())
        self.assertIn('sample_limit', observer.report()['limits_reached'])
        self.assertEqual(observer.report()['lifecycle'], 'exhausted')

    def test_final_sample_failure_is_reported_after_join(self):
        process(self.root)
        with patch.object(self.observer, '_sample',
                          side_effect=RuntimeError('final fixture')):
            self.observer.stop()
        self.assertEqual(self.observer.report()['lifecycle'], 'failed')
        self.assertIn('final fixture', self.observer.report()['error'])

    def test_invalid_arguments_and_phases(self):
        for args in ({'interval': 0}, {'max_samples': 0}, {'interval': float('nan')}):
            with self.subTest(args=args), self.assertRaises(ValueError):
                SessionObserver(10, 1, **args)
        with self.assertRaises(ValueError):
            self.observer.mark_phase('unknown')


if __name__ == '__main__':
    unittest.main()
