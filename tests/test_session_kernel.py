#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
"""Pure file/trace fixtures for optional session kernel accounting."""

from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parent / "live_x"))
import session_kernel
from session_kernel import KernelObservation


def event(name, fields, timestamp=1.0):
    return f" worker-9 [001] .... {timestamp:.6f}: {name}: {fields}\n"


def fork(parent, child, timestamp=1.0):
    return event("sched_process_fork",
                 f"comm=worker pid={parent} child_comm=child child_pid={child}",
                 timestamp)


def wake(pid, timestamp=2.0, new=False):
    return event("sched_wakeup_new" if new else "sched_wakeup",
                 f"comm=child pid={pid} prio=120 target_cpu=001", timestamp)


class KernelFixture(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)
        self.cgroup = self.root / "cgroup"
        self.trace = self.root / "instances"
        self.cgroup.mkdir()
        self.trace.mkdir()
        (self.cgroup / "cgroup.subtree_control").write_text("cpu memory pids\n")
        self.clock = iter([0.0, 1.5, 3.0, 4.0, 5.0]).__next__
        original = session_kernel._new_directory

        def create(parent):
            child = original(parent)
            if parent == self.cgroup:
                self.put(child, "cgroup.procs", "")
                self.put(child, "cpu.stat", "usage_usec 0\nuser_usec 0\nsystem_usec 0\n")
                self.put(child, "memory.current", "0\n")
                self.put(child, "memory.peak", "0\n")
            else:
                for name in ("tracing_on", "trace_clock", "buffer_size_kb", "trace",
                             "events/enable"):
                    self.put(child, name, "0\n")
                for name in session_kernel.EVENTS:
                    self.put(child, f"events/sched/{name}/enable", "0\n")
                    self.put(child, f"events/sched/{name}/format", "name: " + name)
                self.put(child, "per_cpu/cpu0/stats",
                         "entries: 0\noverrun: 0\ncommit overrun: 0\ndropped events: 0\n")
            return child

        self.patcher = patch.object(session_kernel, "_new_directory", create)
        self.patcher.start()
        self.addCleanup(self.patcher.stop)

    @staticmethod
    def put(root, name, contents):
        path = root / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(contents)

    def observation(self, cgroup=True, trace=True):
        observer = KernelObservation(self.cgroup if cgroup else None,
                                     self.trace if trace else None,
                                     iter([0.0, 1.5, 3.0, 4.0, 5.0]).__next__)
        observer.prepare()
        observer.attach(100, 50)
        return observer

    def fill_trace(self, observer, body):
        path = Path(observer.report["trace"]["path"])
        count = len(body.splitlines())
        self.put(path, "trace",
                 f"# entries-in-buffer/entries-written: {count}/{count} #P:1\n" + body)
        return path

    def test_counter_deltas_and_cumulative_peak(self):
        observer = self.observation(trace=False)
        path = Path(observer.report["cgroup"]["path"])
        self.put(path, "cpu.stat", "usage_usec 10\nuser_usec 7\nsystem_usec 3\n")
        self.put(path, "memory.current", "512\n")
        self.put(path, "memory.peak", "1024\n")
        observer.mark_phase("ready")
        self.put(path, "cpu.stat", "usage_usec 30\nuser_usec 19\nsystem_usec 11\n")
        self.put(path, "memory.current", "128\n")
        observer.mark_phase("idle_end")
        report = observer.finish()["cgroup"]
        self.assertEqual(report["cpu_delta"]["usage_usec"], 30)
        self.assertEqual(report["snapshots"][2]["cpu_delta"]["usage_usec"], 20)
        self.assertEqual(report["charged_memory_peak_bytes"], 1024)
        self.assertEqual(report["snapshots"][2]["memory_peak_bytes"], 1024)

    def test_missing_malformed_and_regressing_counters_are_unknown(self):
        for contents in ("usage_usec bad\n", "usage_usec 1\nusage_usec 2\n", ""):
            with self.subTest(contents=contents):
                observer = self.observation(trace=False)
                path = Path(observer.report["cgroup"]["path"])
                self.put(path, "cpu.stat", contents)
                result = observer.finish()["cgroup"]
                self.assertIsNone(result["cpu_delta"])
                self.assertTrue(result["reason"])
        observer = self.observation(trace=False)
        path = Path(observer.report["cgroup"]["path"])
        (path / "memory.current").unlink()
        result = observer.finish()["cgroup"]
        self.assertIsNone(result["snapshots"][-1]["memory_current_bytes"])
        self.assertIsNone(result["charged_memory_peak_bytes"])

    def test_regressing_counters_and_peak_are_unknown(self):
        for filename, first, second in (
                ("cpu.stat", "usage_usec 20\nuser_usec 10\nsystem_usec 10\n",
                 "usage_usec 10\nuser_usec 5\nsystem_usec 5\n"),
                ("memory.peak", "200\n", "100\n")):
            with self.subTest(filename=filename):
                observer = self.observation(trace=False)
                path = Path(observer.report["cgroup"]["path"])
                self.put(path, filename, first)
                observer.mark_phase("ready")
                self.put(path, filename, second)
                result = observer.finish()["cgroup"]
                self.assertIsNone(result["cpu_delta"])
                self.assertIsNone(result["charged_memory_peak_bytes"])
                self.assertIn("regressed", result["reason"])

    def test_nonzero_fresh_group_is_refused(self):
        original = session_kernel._new_directory

        def charged(parent):
            child = original(parent)
            if parent == self.cgroup:
                self.put(child, "memory.peak", "1\n")
            return child

        with patch.object(session_kernel, "_new_directory", charged):
            observer = self.observation(trace=False)
        self.assertFalse(observer.report["cgroup"]["available"])
        self.assertIn("zero", observer.report["cgroup"]["reason"])

    def test_nonfresh_cgroup_and_unavailable_parent_do_not_wrap(self):
        self.put(self.cgroup, "cgroup.subtree_control", "cpu\n")
        observer = self.observation(trace=False)
        self.assertFalse(observer.report["cgroup"]["available"])
        self.assertEqual(observer.command(["program", "argument"]), ["program", "argument"])
        self.assertEqual(list(self.cgroup.iterdir()), [self.cgroup / "cgroup.subtree_control"])

    def test_command_exec_preserves_pid_and_argv(self):
        observer = self.observation(trace=False)
        command = observer.command(["/x y", "$(false)", "a'b"])
        self.assertEqual(command[:2], ["/bin/sh", "-c"])
        self.assertIn('"$$"', command[2])
        self.assertIn('exec "$@"', command[2])
        self.assertEqual(command[-3:], ["/x y", "$(false)", "a'b"])

    def test_ancestry_threads_exit_and_pid_reuse(self):
        observer = self.observation(cgroup=False)
        observer.mark_phase("ready")
        body = fork(50, 100) + wake(100, 1.1, new=True)
        body += fork(100, 101, 1.2) + fork(101, 102, 1.3)
        body += wake(101, 2.0) + wake(102, 2.1)
        body += event("sched_process_exit", "comm=child pid=101 prio=120", 2.2)
        body += fork(9, 101, 2.3) + wake(101, 2.4) + wake(102, 2.5)
        body += fork(101, 103, 2.6) + wake(103, 2.7)
        self.fill_trace(observer, body)
        result = observer.finish()["trace"]
        self.assertTrue(result["complete"], result["reason"])
        self.assertEqual(result["wakeup_count"], 3)
        self.assertEqual(result["wakeup_new_count"], 1)
        self.assertEqual(result["phase_counts"][0]["wakeup_new_count"], 1)
        self.assertEqual(result["phase_counts"][1]["wakeup_count"], 3)

    def test_orphaned_descendant_and_reused_root_are_distinguished(self):
        observer = self.observation(cgroup=False)
        observer.mark_phase("ready")
        body = fork(50, 100) + fork(100, 101, 1.1)
        body += event("sched_process_exit", "comm=child pid=100 prio=120", 1.2)
        body += fork(9, 100, 1.3) + fork(100, 102, 1.4)
        body += wake(100, 1.5) + wake(102, 1.6) + wake(101, 1.7)
        self.fill_trace(observer, body)
        result = observer.finish()["trace"]
        self.assertTrue(result["complete"], result["reason"])
        self.assertEqual(result["wakeup_count"], 1)

    def test_bad_trace_refuses_complete_counts(self):
        variants = {
            "missing birth": wake(100),
            "ambiguous birth": fork(50, 100) + fork(50, 100),
            "unparsed event": fork(50, 100) + "garbage\n",
            "malformed pid": fork(50, 100) + event("sched_wakeup", "comm=x pid=no"),
            "lost event": fork(50, 100) + "CPU:0 [LOST 4 EVENTS]\n",
            "unexpected event": fork(50, 100) + event("sched_switch", "prev_pid=100"),
            "out of order cpu": fork(50, 100, 2) + wake(100, 1),
            "missing exit": fork(50, 100) + fork(100, 101) + fork(9, 101),
            "partial final line": fork(50, 100).rstrip("\n"),
            "wrong clock epoch": fork(50, 100, 1000000.0),
        }
        for label, body in variants.items():
            with self.subTest(label=label):
                observer = self.observation(cgroup=False)
                self.fill_trace(observer, body)
                result = observer.finish()["trace"]
                self.assertFalse(result["complete"])
                self.assertIsNone(result["wakeup_count"])
                self.assertTrue(result["reason"])

    def test_loss_stats_missing_stats_and_truncation_refuse_counts(self):
        for key in ("overrun", "commit overrun", "dropped events", "missing", "truncated"):
            with self.subTest(key=key):
                observer = self.observation(cgroup=False)
                path = self.fill_trace(observer, fork(50, 100) + wake(100))
                if key == "missing":
                    (path / "per_cpu/cpu0/stats").unlink()
                elif key == "truncated":
                    self.put(path, "trace", "x" * (session_kernel.TRACE_LIMIT + 1))
                else:
                    self.put(path, "per_cpu/cpu0/stats",
                             "".join(f"{name}: {int(name == key)}\n"
                                     for name in ("overrun", "commit overrun", "dropped events")))
                result = observer.finish()["trace"]
                self.assertFalse(result["complete"])
                self.assertIsNone(result["wakeup_new_count"])

    def test_header_overwrite_and_boundary_truncation_are_refused(self):
        for header in ("3/3", "2/3"):
            observer = self.observation(cgroup=False)
            path = self.fill_trace(observer, fork(50, 100) + wake(100))
            self.put(path, "trace", f"# entries-in-buffer/entries-written: {header} #P:1\n" +
                     fork(50, 100) + wake(100))
            result = observer.finish()["trace"]
            self.assertFalse(result["complete"])
            self.assertIsNone(result["wakeup_count"])

    def test_fresh_trace_only_and_cleanup_refuses_busy_group(self):
        self.put(self.root, "tracing_on", "global unchanged")
        observer = self.observation()
        group = Path(observer.report["cgroup"]["path"])
        trace = self.fill_trace(observer, fork(50, 100))
        self.put(group, "cgroup.procs", "100\n")
        with patch.object(session_kernel.time, "sleep", lambda delay: None):
            errors = observer.close()
        self.assertEqual((trace / "tracing_on").read_text(), "0")
        self.assertTrue(group.exists())
        self.assertTrue(any("not empty" in error for error in errors))
        self.assertEqual((self.root / "tracing_on").read_text(), "global unchanged")
        self.assertTrue(self.trace.exists())
        self.assertTrue(self.cgroup.exists())

    def test_finish_waits_for_exiting_members_before_final_counters(self):
        observer = self.observation(trace=False)
        path = Path(observer.report["cgroup"]["path"])
        self.put(path, "cgroup.procs", "100\n")
        sleeps = []

        def drain(delay):
            sleeps.append(delay)
            self.put(path, "cgroup.procs", "")
            self.put(path, "cpu.stat", "usage_usec 30\nuser_usec 20\nsystem_usec 10\n")

        with patch.object(session_kernel.time, "sleep", drain):
            report = observer.finish()["cgroup"]
        self.assertEqual(len(sleeps), 1)
        self.assertTrue(report["complete"])
        self.assertEqual(report["cpu_delta"]["usage_usec"], 30)

    def test_busy_finish_is_unknown_and_close_can_retry(self):
        observer = self.observation(trace=False)
        path = Path(observer.report["cgroup"]["path"])
        self.put(path, "cgroup.procs", "100\n")
        with patch.object(session_kernel.time, "sleep", lambda delay: None):
            report = observer.finish()["cgroup"]
            first = list(observer.close())
            self.assertEqual(observer.close(), first)
        self.assertFalse(report["complete"])
        self.assertIsNone(report["cpu_delta"])
        self.assertIn("not empty", report["reason"])
        self.put(path, "cgroup.procs", "")
        with patch.object(Path, "rmdir", lambda child: None):
            self.assertEqual(observer.close(), [])

    def test_cleanup_refuses_replaced_directory(self):
        observer = self.observation(trace=False)
        path = Path(observer.report["cgroup"]["path"])
        path.rename(path.with_name(path.name + "-old"))
        path.mkdir()
        self.put(path, "cgroup.procs", "")
        errors = observer.close()
        self.assertTrue(any("identity changed" in error for error in errors))
        self.assertTrue(path.exists())

    def test_successful_cleanup_removes_only_owned_empty_children(self):
        observer = self.observation()
        owned = {Path(observer.report[kind]["path"]) for kind in ("cgroup", "trace")}
        removals = []

        def remove(path):
            removals.append(path)

        with patch.object(Path, "rmdir", remove):
            self.assertEqual(observer.close(), [])
            self.assertEqual(observer.close(), [])
        self.assertEqual(set(removals), owned)
        self.assertEqual(len(removals), 2)

    def test_missing_private_control_is_unavailable_and_never_created(self):
        original = session_kernel._new_directory

        def missing_clock(parent):
            child = original(parent)
            if parent == self.trace:
                (child / "trace_clock").unlink()
            return child

        with patch.object(session_kernel, "_new_directory", missing_clock):
            observer = self.observation(cgroup=False)
        result = observer.finish()["trace"]
        self.assertFalse(result["available"])
        self.assertIsNone(result["wakeup_count"])
        self.assertFalse((Path(result["path"]) / "trace_clock").exists())

    def test_disabled_observation_has_unknowns_and_finish_is_idempotent(self):
        observer = KernelObservation(clock=self.clock)
        observer.prepare()
        observer.mark_phase("ready")
        result = observer.finish()
        self.assertIsNone(result["trace"]["wakeup_count"])
        self.assertIsNone(result["cgroup"]["charged_memory_peak_bytes"])
        self.assertIs(observer.finish(), result)


class OwnershipJournalTests(unittest.TestCase):
    def test_creation_identity_survives_removal_and_is_not_mutable_by_caller(self):
        with tempfile.TemporaryDirectory() as directory:
            observation = KernelObservation()
            path = observation._create('cgroup', Path(directory))
            created = observation.ownership()
            expected = dict(created['cgroup'])
            path.rmdir()
            created['cgroup']['inode'] = -1
            self.assertEqual(observation.ownership()['cgroup'], expected)


if __name__ == "__main__":
    unittest.main()
