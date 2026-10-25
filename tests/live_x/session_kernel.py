#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
"""Optional accounting using existing delegated cgroup-v2 and tracefs parents.

No mounts, controller changes, privilege changes or global tracing writes occur.
The caller owns stopping its session before close(); this helper never kills or
moves tasks. Scheduler counts are tracepoint occurrences, not timer/IRQ counts.
"""

import math
import os
from pathlib import Path
import platform
import re
import time
import uuid


EVENTS = ("sched_process_fork", "sched_process_exit", "sched_wakeup",
          "sched_wakeup_new")
TRACE_LIMIT = 32 * 1024 * 1024
_SMALL_LIMIT = 64 * 1024
_RECORD = re.compile(
    r"^\s*.+-\d+\s+(?:\(\s*\d+\)\s+)?\[\d+\]\s+\S+\s+"
    r"(?P<time>\d+\.\d+):\s+(?P<event>\w+):\s+(?P<fields>.*)$")
_HEADER = re.compile(r"entries-in-buffer/entries-written:\s*(\d+)/(\d+)")
_CPU_REQUIRED = {"usage_usec", "user_usec", "system_usec"}
_LOSS_KEYS = {"overrun", "commit overrun", "dropped events"}


def _read(path, limit=_SMALL_LIMIT):
    with path.open("rb") as stream:
        data = stream.read(limit + 1)
    if len(data) > limit:
        raise ValueError(f"{path.name}: exceeds {limit}-byte limit")
    return data.decode("utf-8", errors="strict")


def _write(path, value):
    # Never create a missing control file or follow a replaced control symlink.
    fd = os.open(path, os.O_WRONLY | os.O_TRUNC | os.O_NOFOLLOW)
    try:
        data = str(value).encode("ascii")
        if os.write(fd, data) != len(data):
            raise OSError(f"short control write: {path}")
    finally:
        os.close(fd)


def _new_directory(parent):
    child = parent / f"fvwm-session-{os.getpid()}-{uuid.uuid4().hex}"
    child.mkdir(mode=0o700)
    return child


def _identity(path):
    stat = path.lstat()
    if not path.is_dir() or path.is_symlink():
        raise ValueError(f"not an owned directory: {path}")
    return stat.st_dev, stat.st_ino


def _number(text):
    value = text.strip()
    if not re.fullmatch(r"[0-9]+", value):
        raise ValueError("expected nonnegative integer")
    return int(value)


def _cpu_stat(text):
    result = {}
    for line in text.splitlines():
        fields = line.split()
        if len(fields) != 2 or fields[0] in result:
            raise ValueError("malformed or duplicate cpu.stat entry")
        result[fields[0]] = _number(fields[1])
    if not _CPU_REQUIRED <= result.keys():
        raise ValueError("missing required cpu.stat counters")
    return result


def _delta(before, after):
    if before is None or after is None:
        return None
    if before.keys() != after.keys():
        raise ValueError("cpu.stat counter set changed")
    if any(after[key] < value for key, value in before.items()):
        raise ValueError("cpu.stat counter regressed")
    return {key: after[key] - value for key, value in before.items()}


def _wait_empty(path):
    """Allow a bounded child drain without signalling or moving tasks."""
    deadline = time.monotonic() + 2.0
    for attempt in range(21):
        members = _read(path / "cgroup.procs").strip()
        if not members:
            return
        remaining = deadline - time.monotonic()
        if remaining <= 0 or attempt == 20:
            break
        time.sleep(min(0.1, remaining))
    raise ValueError("owned cgroup is not empty after bounded 2-second drain; "
                     "refusing final totals/removal")


def _loss_stats(text):
    result = {}
    for line in text.splitlines():
        if ":" not in line:
            raise ValueError("malformed per-CPU trace stats")
        key, value = (part.strip() for part in line.split(":", 1))
        if key in result:
            raise ValueError("duplicate per-CPU trace stat")
        # Timestamp and byte statistics are metadata; only loss fields count.
        result[key] = _number(value) if key in _LOSS_KEYS else value
    if not _LOSS_KEYS <= result.keys():
        raise ValueError("missing per-CPU trace loss stats")
    return result


def _event_pid(fields, key):
    values = re.findall(r"(?:^|\s)" + key + r"=(\S+)", fields)
    if len(values) != 1:
        raise ValueError(f"missing or ambiguous {key}")
    value = _number(values[0])
    if value == 0:
        raise ValueError(f"zero {key}")
    return value


def _trace_counts(raw, root_pid, parent_pid, boundaries, end):
    records = []
    births = []
    entries = None
    previous = -1.0
    if raw and not raw.endswith("\n"):
        raise ValueError("unterminated trace; possible truncated record")
    for line in raw.splitlines():
        if not line.strip():
            continue
        if line.lstrip().startswith("#"):
            match = _HEADER.search(line)
            if match:
                if entries is not None:
                    raise ValueError("duplicate trace buffer entry header")
                entries = int(match[1])
                if entries != int(match[2]):
                    raise ValueError("trace entries were overwritten")
            continue
        match = _RECORD.fullmatch(line)
        if match is None or match["event"] not in EVENTS:
            raise ValueError("unparsed scheduler trace record")
        timestamp = float(match["time"])
        if not math.isfinite(timestamp) or timestamp < previous:
            raise ValueError("scheduler trace timestamps regressed")
        previous = timestamp
        name, fields = match["event"], match["fields"]
        pid = _event_pid(fields, "pid")
        child = _event_pid(fields, "child_pid") if name == EVENTS[0] else None
        if name == EVENTS[0] and pid == parent_pid and child == root_pid:
            births.append(len(records))
        records.append((timestamp, name, pid, child))
    if entries is None:
        raise ValueError("missing trace buffer entry header")
    if len(records) != entries:
        raise ValueError("trace record count differs from buffer entry header")
    if len(births) != 1:
        raise ValueError("missing or ambiguous session root birth")
    if not boundaries or end < boundaries[-1]["monotonic_seconds"]:
        raise ValueError("invalid phase boundaries")
    phases = []
    for index, phase in enumerate(boundaries):
        phase_end = (boundaries[index + 1]["monotonic_seconds"]
                     if index + 1 < len(boundaries) else end)
        phases.append({"name": phase["name"],
                       "start_monotonic_seconds": phase["monotonic_seconds"],
                       "end_monotonic_seconds": phase_end,
                       "wakeup_count": 0, "wakeup_new_count": 0})
    totals = {"wakeup_count": 0, "wakeup_new_count": 0}
    # Each fork starts a new PID generation. Exit removes only that generation;
    # descendants keep their membership across parent death and reparenting.
    active = {}
    for index, (timestamp, name, pid, child) in enumerate(records):
        if not boundaries[0]["monotonic_seconds"] <= timestamp <= end:
            raise ValueError("trace timestamp outside monotonic observation")
        if name == EVENTS[0]:
            if child in active:
                raise ValueError("fork reused an active PID without an exit")
            active[child] = index == births[0] or active.get(pid, False)
        elif name == EVENTS[1]:
            active.pop(pid, None)
        elif active.get(pid, False):
            key = "wakeup_new_count" if name == EVENTS[3] else "wakeup_count"
            totals[key] += 1
            for phase in phases:
                before_end = (timestamp < phase["end_monotonic_seconds"] or
                              (phase is phases[-1] and timestamp == end))
                if (phase["start_monotonic_seconds"] <= timestamp and
                        before_end):
                    phase[key] += 1
                    break
    return dict(totals, phase_counts=phases)


class KernelObservation:
    """One launch observation. prepare() must precede the caller's Popen.

    Explicit parent paths opt in independently. Failed optional facilities keep
    their reason and null values while the session can still run. A wrapped
    cgroup launch fails with status 125 if joining the prepared group fails.
    """

    def __init__(self, cgroup_parent=None, trace_parent=None,
                 clock=time.monotonic):
        self.cgroup_parent = (Path(cgroup_parent)
                              if cgroup_parent is not None else None)
        self.trace_parent = (Path(trace_parent)
                            if trace_parent is not None else None)
        self.clock = clock
        self._owned = {}
        self._prepared = False
        self._finished = False
        self._boundaries = []
        self._cleanup_errors = []
        self.report = {
            "schema_version": 1,
            "kernel": {"system": platform.system(),
                       "release": platform.release()},
            "cgroup": {"requested": cgroup_parent is not None,
                       "available": False,
                       "reason": "not requested", "complete": False,
                       "path": None, "snapshots": [],
                       "cpu_delta": None, "charged_memory_peak_bytes": None,
                       "memory_semantics": (
                           "cgroup charge includes kernel/cache; not RSS/PSS"),
                       "peak_semantics": (
                           "cumulative since fresh group creation; never reset"),
                       "admission": "shell joins immediately before exec"},
            "trace": {"requested": trace_parent is not None, "available": False,
                      "reason": "not requested", "complete": False,
                      "path": None, "root_pid": None, "parent_pid": None,
                      "wakeup_count": None,
                      "wakeup_new_count": None, "phase_counts": None,
                      "per_cpu_stats": {}, "metadata": {},
                      "semantics": (
                          "selected-task sched tracepoint occurrences; "
                          "not IRQs/timers")},
            "raw_trace": None, "cleanup_errors": self._cleanup_errors,
        }

    def _create(self, kind, parent):
        if not parent.is_dir():
            raise ValueError(f"{kind} parent is not an existing directory")
        path = _new_directory(parent)
        self._owned[kind] = (path, _identity(path))
        self.report[kind]["path"] = str(path)
        return path

    def ownership(self):
        """Snapshot creation identities still held by this observer."""
        return {kind: {"path": str(path), "device": identity[0],
                       "inode": identity[1]}
                for kind, (path, identity) in self._owned.items()}

    def _path(self, kind):
        path, identity = self._owned[kind]
        if _identity(path) != identity:
            raise ValueError(f"{kind} owned directory identity changed")
        return path

    def prepare(self):
        if self._prepared:
            raise ValueError("observation already prepared")
        self._prepared = True
        start = self.clock()
        self._boundaries.append({"name": "launch", "monotonic_seconds": start})
        if self.cgroup_parent is not None:
            group = self.report["cgroup"]
            try:
                control = self.cgroup_parent / "cgroup.subtree_control"
                enabled = _read(control).split()
                if not {"cpu", "memory"} <= set(enabled):
                    raise ValueError("cpu and memory controllers are not "
                                     "already enabled")
                path = self._create("cgroup", self.cgroup_parent)
                snapshot = self._snapshot("launch", start)
                if snapshot["errors"]:
                    raise ValueError("; ".join(snapshot["errors"]))
                if (_read(path / "cgroup.procs").strip() or
                        any(snapshot["cpu_stat"].values()) or
                        snapshot["memory_current_bytes"] != 0 or
                        snapshot["memory_peak_bytes"] != 0):
                    raise ValueError("fresh cgroup is not empty with zero "
                                     "CPU/memory peak")
                group.update(available=True, reason=None)
            except (OSError, ValueError) as error:
                group["reason"] = str(error)
        if self.trace_parent is not None:
            trace = self.report["trace"]
            try:
                path = self._create("trace", self.trace_parent)
                _write(path / "tracing_on", "0")
                _write(path / "events/enable", "0")
                _write(path / "trace_clock", "mono")
                _write(path / "buffer_size_kb", "4096")
                _write(path / "trace", "")
                formats = {}
                for name in EVENTS:
                    event = path / "events/sched" / name
                    formats[name] = _read(event / "format")
                    _write(event / "enable", "1")
                selected_clock = _read(path / "trace_clock").strip()
                if (selected_clock != "mono" and
                        "[mono]" not in selected_clock.split()):
                    raise ValueError("private trace did not select mono clock")
                trace["metadata"] = {
                    "trace_clock": selected_clock,
                    "buffer_size_kb_per_cpu": (
                        _read(path / "buffer_size_kb").strip()),
                    "raw_trace_limit_bytes": TRACE_LIMIT,
                    "event_formats": formats}
                _write(path / "tracing_on", "1")
                trace.update(available=True, reason=None)
            except (OSError, ValueError) as error:
                trace["reason"] = str(error)
        return self

    def command(self, argv):
        if not self._prepared or self._finished:
            raise ValueError("command requires a prepared unfinished "
                             "observation")
        if not argv or not all(isinstance(arg, str) and "\0" not in arg
                               for arg in argv):
            raise ValueError("command requires nonempty string argv")
        if not self.report["cgroup"]["available"]:
            return list(argv)
        path = self._path("cgroup") / "cgroup.procs"
        return ["/bin/sh", "-c",
                'printf \'%s\\n\' "$$" > "$1" || exit 125; shift; exec "$@"',
                "fvwm-session-cgroup", str(path), *argv]

    def attach(self, root_pid, parent_pid):
        if not self._prepared or self._finished:
            raise ValueError("attach requires a prepared unfinished "
                             "observation")
        trace = self.report["trace"]
        if trace["root_pid"] is not None:
            raise ValueError("session already attached")
        if any(type(pid) is not int or pid <= 0
               for pid in (root_pid, parent_pid)):
            raise ValueError("attach requires positive PIDs")
        trace.update(root_pid=root_pid, parent_pid=parent_pid)

    def _snapshot(self, name, timestamp):
        group = self.report["cgroup"]
        result = {"name": name, "monotonic_seconds": timestamp,
                  "cpu_stat": None,
                  "cpu_delta": None, "memory_current_bytes": None,
                  "memory_peak_bytes": None, "errors": []}
        try:
            path = self._path("cgroup")
        except (OSError, ValueError) as error:
            result["errors"].append(str(error))
            group["snapshots"].append(result)
            return result
        fields = (("cpu.stat", "cpu_stat", _cpu_stat),
                  ("memory.current", "memory_current_bytes", _number),
                  ("memory.peak", "memory_peak_bytes", _number))
        for filename, key, parser in fields:
            try:
                result[key] = parser(_read(path / filename))
            except (OSError, ValueError) as error:
                result["errors"].append(f"{filename}: {error}")
        previous = group["snapshots"][-1] if group["snapshots"] else None
        if (result["memory_current_bytes"] is not None and
                result["memory_peak_bytes"] is not None and
                result["memory_current_bytes"] > result["memory_peak_bytes"]):
            result["errors"].append("memory.current exceeds memory.peak")
        if previous:
            try:
                result["cpu_delta"] = _delta(previous["cpu_stat"],
                                             result["cpu_stat"])
                if (previous["memory_peak_bytes"] is not None and
                        result["memory_peak_bytes"] is not None and
                        result["memory_peak_bytes"] <
                        previous["memory_peak_bytes"]):
                    raise ValueError("memory.peak regressed")
            except ValueError as error:
                result["errors"].append(str(error))
        group["snapshots"].append(result)
        return result

    def mark_phase(self, name):
        if not self._prepared or self._finished:
            raise ValueError("phase requires a prepared unfinished observation")
        if not isinstance(name, str) or not name or any(
                phase["name"] == name for phase in self._boundaries):
            raise ValueError("phase requires a unique nonempty name")
        timestamp = self.clock()
        if timestamp < self._boundaries[-1]["monotonic_seconds"]:
            raise ValueError("phase clock regressed")
        self._boundaries.append({"name": name, "monotonic_seconds": timestamp})
        if self.report["cgroup"]["available"]:
            self._snapshot(name, timestamp)

    def finish(self):
        if self._finished:
            return self.report
        if not self._prepared:
            raise ValueError("finish requires prepare")
        self._finished = True
        group, trace = self.report["cgroup"], self.report["trace"]
        drain_error = None
        if group["available"]:
            try:
                _wait_empty(self._path("cgroup"))
            except (OSError, ValueError) as error:
                drain_error = str(error)
        stopped = False
        if trace["available"]:
            try:
                _write(self._path("trace") / "tracing_on", "0")
                stopped = True
            except (OSError, ValueError) as error:
                trace["reason"] = f"could not stop private trace: {error}"
        end = self.clock()
        self.report["boundaries"] = list(self._boundaries)
        self.report["end_monotonic_seconds"] = end
        if group["available"]:
            try:
                self._snapshot("finish", end)
                errors = [error for item in group["snapshots"]
                          for error in item["errors"]]
                if drain_error:
                    errors.append(drain_error)
                if errors:
                    raise ValueError("; ".join(errors))
                first, last = group["snapshots"][0], group["snapshots"][-1]
                group["cpu_delta"] = _delta(first["cpu_stat"], last["cpu_stat"])
                group["charged_memory_peak_bytes"] = last["memory_peak_bytes"]
                group["complete"] = True
            except (OSError, ValueError) as error:
                group["reason"] = str(error)
        if stopped:
            try:
                path = self._path("trace")
                # Preserve raw evidence even if loss stats refuse counts.
                with (path / "trace").open("rb") as stream:
                    raw = stream.read(TRACE_LIMIT + 1)
                self.report["raw_trace"] = raw[:TRACE_LIMIT].decode(
                    "utf-8", errors="replace")
                if len(raw) > TRACE_LIMIT:
                    raise ValueError("raw trace exceeds byte limit; truncated")
                raw = raw.decode("utf-8", errors="strict")
                stats_paths = sorted((path / "per_cpu").glob("cpu*/stats"))
                cpu_paths = sorted((path / "per_cpu").glob("cpu*"))
                if not cpu_paths or len(stats_paths) != len(cpu_paths):
                    raise ValueError("missing per-CPU trace stats")
                for stats in stats_paths:
                    trace["per_cpu_stats"][stats.parent.name] = (
                        _loss_stats(_read(stats)))
                if any(stats[key] for stats in trace["per_cpu_stats"].values()
                       for key in _LOSS_KEYS):
                    raise ValueError("trace buffer overrun or dropped events")
                if trace["root_pid"] is None:
                    raise ValueError("session root was not attached")
                trace.update(_trace_counts(raw, trace["root_pid"],
                                           trace["parent_pid"],
                                           self._boundaries, end))
                trace.update(complete=True, reason=None)
            except (OSError, ValueError) as error:
                trace["reason"] = str(error)
        return self.report

    def close(self):
        """Stop/remove only owned children; return refusal/error details.

        A nonempty group is retained for the caller to stop/reap its tasks and
        retry. No recursive deletion, task migration or parent removal is used.
        """
        self._cleanup_errors.clear()
        for kind in ("trace", "cgroup"):
            if kind not in self._owned:
                continue
            try:
                path = self._path(kind)
                if kind == "trace":
                    _write(path / "tracing_on", "0")
                else:
                    _wait_empty(path)
                path.rmdir()
                del self._owned[kind]
            except (OSError, ValueError) as error:
                self._cleanup_errors.append(f"{kind}: {error}")
        return self._cleanup_errors
