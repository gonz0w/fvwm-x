#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
"""Bounded test-only Linux proc sampling; no X or production instrumentation.

CPU belongs to a process stat, memory to a process, and context switches to
individual tasks. All deltas require matching start identities. Sampling misses
unseen short-lived children, and RSS/PSS sums are sampled accounting, not physical
memory or an unsampled whole-session peak. Context switches are not wakeups.
"""
import copy
import math
import os
from pathlib import Path
import stat
import threading
import time


def parse_stat(value):
    """Read stat fields after the final comm parenthesis, including spaced comm."""
    left, right = value.find(' ('), value.rfind(')')
    if left < 1 or right <= left:
        raise ValueError('malformed stat comm')
    try:
        pid = int(value[:left])
        fields = value[right + 1:].split()
        if len(fields) < 22 or len(fields[0]) != 1:
            raise ValueError('truncated stat')
        parent, user, system, start = (int(fields[i]) for i in (1, 11, 12, 19))
        if pid <= 0 or min(parent, user, system, start) < 0:
            raise ValueError('negative stat counter or invalid PID')
    except (IndexError, ValueError) as error:
        raise ValueError('invalid stat fields') from error
    return {'pid': pid, 'ppid': parent, 'start_ticks': start,
            'cpu_ticks': user + system}


def _read(path, maximum=65536):
    # A malformed fixture FIFO must not wedge the owned measurement thread.
    flags = os.O_RDONLY | os.O_NONBLOCK | getattr(os, 'O_NOFOLLOW', 0)
    descriptor = os.open(path, flags)
    try:
        if not stat.S_ISREG(os.fstat(descriptor).st_mode):
            raise ValueError('not a regular proc record')
        data = bytearray()
        while len(data) <= maximum:
            part = os.read(descriptor, min(8192, maximum + 1 - len(data)))
            if not part:
                break
            data.extend(part)
        if len(data) > maximum:
            raise ValueError('record byte limit')
        return data.decode('utf-8', errors='strict')
    finally:
        os.close(descriptor)


def _status(path):
    fields = {}
    for line in _read(path).splitlines():
        if ':' not in line:
            raise ValueError('malformed status line')
        name, value = line.split(':', 1)
        if name in fields:
            raise ValueError('duplicate status field')
        fields[name] = value.strip()
    return fields


def _counter(fields, name, unit=None):
    parts = fields[name].split()
    if len(parts) != (2 if unit else 1) or (unit and parts[1] != unit):
        raise ValueError('invalid ' + name)
    result = int(parts[0])
    if result < 0:
        raise ValueError('negative ' + name)
    return result


def _ids(path, maximum, preferred=()):
    """Bound directory work as well as retained records; order is deterministic."""
    found = set(preferred)
    limited = False
    with os.scandir(path) as entries:
        for count, entry in enumerate(entries):
            if count >= 16384:
                limited = True
                break
            if entry.name.isascii() and entry.name.isdecimal():
                value = int(entry.name)
                if value in found:
                    continue
                if len(found) >= maximum:
                    limited = True
                    break
                found.add(value)
    ordered = list(dict.fromkeys(preferred))
    ordered.extend(sorted(found.difference(ordered)))
    return ordered[:maximum], limited or len(ordered) > maximum


def _process_key(record):
    return record['pid'], record['start_ticks']


def _task_map(records):
    return {(p['pid'], p['start_ticks'], t['tid'], t['start_ticks']): t
            for p in records for t in p['tasks']}


def _interval(before, after, ticks_per_second):
    seconds = after['at'] - before['at']
    old = {_process_key(p): p for p in before['processes']}
    new = {_process_key(p): p for p in after['processes']}
    old_tasks, new_tasks = _task_map(before['processes']), _task_map(after['processes'])
    shared, shared_tasks = old.keys() & new.keys(), old_tasks.keys() & new_tasks.keys()
    result = {'from': before['at'], 'to': after['at'], 'seconds': seconds,
              'phase': after['phase'] if before['phase'] == after['phase'] else None,
              'matched_processes': len(shared),
              'fresh_processes': len(new.keys() - old.keys()),
              'vanished_processes': len(old.keys() - new.keys()),
              'matched_tasks': len(shared_tasks),
              'fresh_tasks': len(new_tasks.keys() - old_tasks.keys()),
              'vanished_tasks': len(old_tasks.keys() - new_tasks.keys()),
              'cpu_regressions': 0, 'switch_regressions': 0,
              'unmatched_cpu_processes': 0, 'unmatched_switch_tasks': 0,
              'cpu_ticks': None, 'cpu_ms': None, 'cpu_percent': None,
              'context_switches': None, 'status': 'complete'}
    cpu, switches = [], []
    for key in shared:
        delta = new[key]['cpu_ticks'] - old[key]['cpu_ticks']
        if delta < 0:
            result['cpu_regressions'] += 1
        else:
            cpu.append(delta)
    for key in shared_tasks:
        values = [records[key]['context_switches']
                  for records in (old_tasks, new_tasks)]
        if None in values:
            result['unmatched_switch_tasks'] += 1
        elif values[1] < values[0]:
            result['switch_regressions'] += 1
        else:
            switches.append(values[1] - values[0])
    result['unmatched_cpu_processes'] = (
        result['fresh_processes'] + result['vanished_processes'])
    result['unmatched_switch_tasks'] += result['fresh_tasks'] + result['vanished_tasks']
    if seconds <= 0:
        result['status'] = 'invalid_time'
        return result
    if cpu:
        result['cpu_ticks'] = sum(cpu)
        result['cpu_ms'] = sum(cpu) * 1000 / ticks_per_second
        result['cpu_percent'] = result['cpu_ms'] / (seconds * 10)
    if switches:
        result['context_switches'] = sum(switches)
    if (before['status'] != 'complete' or after['status'] != 'complete' or
            not cpu or not switches or any(result[name] for name in
            ('unmatched_cpu_processes', 'unmatched_switch_tasks',
             'cpu_regressions', 'switch_regressions'))):
        result['status'] = 'partial'
    return result


class SessionObserver:
    """One owner starts/stops a non-daemon sampler and consumes its JSON report.

    The injected clock and proc root permit deterministic fixture sampling via
    _sample(). Public phase marks take an immediate baseline under the same lock.
    All filesystem reads are bounded/nonblocking; stop cancels Event.wait and
    joins the owner thread. A join failure is an explicit error, never a detached
    measurement daemon.
    """
    MAX_PROCESSES = 256
    MAX_TASKS = 4096
    MAX_SECONDS = 300.
    PHASES = ('startup', 'idle', 'activity', 'shutdown')

    def __init__(self, root_pid, launched_at, proc_root=Path('/proc'),
                 interval=.05, max_samples=2048, clock=time.monotonic):
        if (not isinstance(root_pid, int) or root_pid <= 0 or
                not math.isfinite(launched_at) or not math.isfinite(interval) or
                interval <= 0 or not isinstance(max_samples, int) or
                max_samples <= 0 or max_samples > 2048):
            raise ValueError('invalid observer bounds or launch identity')
        self.root_pid = root_pid
        self.launched_at = launched_at
        self.proc_root = Path(proc_root)
        self.interval = interval
        self.max_samples = max_samples
        self.clock = clock
        self.ticks_per_second = os.sysconf('SC_CLK_TCK')
        self._lock = threading.RLock()
        self._cancel = threading.Event()
        self._thread = None
        self._snapshots = []
        self._known = set()
        self._root_identity = None
        self._phase = 'startup'
        self._marks = [{'name': 'startup', 'at': launched_at}]
        self._limits = set()
        self._lifecycle = 'new'
        self._error = None
        self._ended_at = None

    def start(self):
        with self._lock:
            if self._lifecycle != 'new':
                raise RuntimeError('observer can only start once')
            self._lifecycle = 'running'
            self._thread = threading.Thread(target=self._run,
                                            name='session-proc-observer', daemon=False)
            try:
                self._thread.start()
            except BaseException as error:
                self._lifecycle = 'failed'
                self._error = str(error)
                raise
        return self

    def _run(self):
        # A real wall-clock bound also protects a stalled injected test clock.
        deadline = time.monotonic() + min(
            self.MAX_SECONDS, max(1., self.interval * self.max_samples))
        try:
            while not self._cancel.is_set():
                self._sample()
                if len(self._snapshots) >= self.max_samples:
                    with self._lock:
                        self._limits.add('sample_limit')
                    break
                if time.monotonic() >= deadline:
                    with self._lock:
                        self._limits.add('time_limit')
                    break
                if self._cancel.wait(min(self.interval,
                                         max(0., deadline - time.monotonic()))):
                    break
        except BaseException as error:
            with self._lock:
                self._error = f'{type(error).__name__}: {error}'
                self._lifecycle = 'failed'
            self._cancel.set()
        finally:
            with self._lock:
                if self._lifecycle == 'running' and self._limits:
                    self._lifecycle = 'exhausted'

    def mark_phase(self, name):
        if name not in self.PHASES:
            raise ValueError('unknown observation phase')
        with self._lock:
            if self._ended_at is not None:
                raise RuntimeError('observer already stopped')
            if name == self._phase:
                return
            if any(mark['name'] == name for mark in self._marks):
                raise ValueError('observation phase cannot be revisited')
            now = self.clock()
            if not math.isfinite(now):
                raise ValueError('nonfinite observation clock')
            self._phase = name
            self._marks.append({'name': name, 'at': now})
            self._sample()

    def stop(self):
        self._cancel.set()
        thread = self._thread
        if thread is not None and thread.ident is not None:
            thread.join(timeout=2.)
            if thread.is_alive():
                with self._lock:
                    self._lifecycle = 'failed'
                    self._error = 'observer thread did not join within two seconds'
                raise RuntimeError(self._error)
        with self._lock:
            if self._ended_at is not None:
                return
            if self._lifecycle != 'failed':
                try:
                    self._sample()
                    self._lifecycle = 'stopped'
                except Exception as error:
                    self._lifecycle = 'failed'
                    self._error = f'{type(error).__name__}: {error}'
            now = self.clock()
            self._ended_at = now if math.isfinite(now) else self.launched_at

    def _sample(self):
        with self._lock:
            if len(self._snapshots) >= self.max_samples:
                self._limits.add('sample_limit')
                return
            at = self.clock()
            if not math.isfinite(at):
                raise ValueError('nonfinite observation clock')
            issues = []
            preferred = [self.root_pid] + sorted({pid for pid, _ in self._known
                                                if pid != self.root_pid})
            try:
                ids, limited = _ids(self.proc_root, self.MAX_PROCESSES, preferred)
                if limited:
                    issues.append('process_scan_limit')
            except (OSError, ValueError) as error:
                ids = preferred[:self.MAX_PROCESSES]
                issues.append('proc_directory: ' + str(error))
            candidates = {}
            for pid in ids:
                try:
                    record = parse_stat(_read(self.proc_root / str(pid) / 'stat'))
                    if record['pid'] != pid:
                        raise ValueError('PID mismatch')
                    candidates[pid] = record
                except (OSError, ValueError, UnicodeError) as error:
                    issues.append(f'{pid}/stat: {error}')
            root = candidates.get(self.root_pid)
            if self._root_identity is None and root is not None:
                self._root_identity = _process_key(root)
            root_valid = root is not None and _process_key(root) == self._root_identity
            if not root_valid:
                issues.append('root_identity_lost')
            selected = {pid for pid, record in candidates.items()
                        if _process_key(record) in self._known}
            if root_valid:
                selected.add(self.root_pid)
            while True:
                more = {pid for pid, record in candidates.items()
                        if record['ppid'] in selected and
                        candidates[record['ppid']]['start_ticks'] <=
                        record['start_ticks']}
                if more <= selected:
                    break
                selected |= more
            records, task_budget = [], self.MAX_TASKS
            for pid in sorted(selected):
                record = dict(candidates[pid])
                folder = self.proc_root / str(pid)
                record.update(rss_kib=None, hwm_kib=None, pss_kib=None, tasks=[])
                try:
                    fields = _status(folder / 'status')
                    record['rss_kib'] = _counter(fields, 'VmRSS', 'kB')
                    record['hwm_kib'] = _counter(fields, 'VmHWM', 'kB')
                except (OSError, ValueError, KeyError, UnicodeError) as error:
                    issues.append(f'{pid}/status: {error}')
                try:
                    # smaps_rollup starts with a mapping header, unlike status.
                    lines = _read(folder / 'smaps_rollup').splitlines()
                    pss = [line.split(':', 1)[1].strip() for line in lines
                           if line.startswith('Pss:')]
                    if len(pss) != 1:
                        raise ValueError('missing/duplicate Pss')
                    record['pss_kib'] = _counter({'Pss': pss[0]}, 'Pss', 'kB')
                except (OSError, ValueError, KeyError, UnicodeError) as error:
                    issues.append(f'{pid}/smaps_rollup: {error}')
                try:
                    tids, limited = _ids(folder / 'task', task_budget)
                    if limited:
                        issues.append('task_scan_limit')
                    for tid in tids:
                        task_budget -= 1
                        task_folder = folder / 'task' / str(tid)
                        try:
                            task_stat = parse_stat(_read(task_folder / 'stat'))
                            if task_stat['pid'] != tid:
                                raise ValueError('TID mismatch')
                            task = {'tid': tid, 'start_ticks': task_stat['start_ticks'],
                                    'context_switches': None}
                            try:
                                fields = _status(task_folder / 'status')
                                task['context_switches'] = (
                                    _counter(fields, 'voluntary_ctxt_switches') +
                                    _counter(fields, 'nonvoluntary_ctxt_switches'))
                            except (OSError, ValueError, KeyError,
                                    UnicodeError) as error:
                                issues.append(f'{pid}/task/{tid}/status: {error}')
                            end = parse_stat(_read(task_folder / 'stat'))
                            if (end['pid'] != tid or
                                    end['start_ticks'] != task['start_ticks']):
                                raise ValueError('task identity changed during sample')
                            record['tasks'].append(task)
                        except (OSError, ValueError, UnicodeError) as error:
                            issues.append(f'{pid}/task/{tid}/stat: {error}')
                    if not tids:
                        issues.append(f'{pid}/task: no readable tasks')
                except (OSError, ValueError) as error:
                    issues.append(f'{pid}/task: {error}')
                records.append(record)
            # Validate every observed process after collecting the poll. A
            # child's initial PPID is insufficient authority when that parent
            # vanishes or is reused during collection. Commit new identities
            # only through an ancestry chain whose records survived this pass.
            stable, final_parents = {}, {}
            for record in records:
                pid = record['pid']
                try:
                    end = parse_stat(_read(self.proc_root / str(pid) / 'stat'))
                    if _process_key(end) != _process_key(record):
                        raise ValueError('process identity changed during sample')
                    stable[pid] = record
                    final_parents[pid] = end['ppid']
                except (OSError, ValueError, UnicodeError) as error:
                    issues.append(f'{pid}/stat final: {error}')
            accepted = {pid for pid, record in stable.items()
                        if _process_key(record) in self._known}
            if root_valid and self.root_pid in stable:
                accepted.add(self.root_pid)
            retained_count = len(self._known) + sum(
                _process_key(stable[pid]) not in self._known for pid in accepted)
            while True:
                added = set()
                for pid, record in stable.items():
                    if pid in accepted or record['ppid'] not in accepted:
                        continue
                    if final_parents[pid] != record['ppid']:
                        continue
                    if retained_count >= self.MAX_PROCESSES:
                        issues.append('retained_identity_limit')
                        continue
                    added.add(pid)
                    retained_count += 1
                if not added:
                    break
                accepted |= added
            for pid in stable.keys() - accepted:
                issues.append(f'{pid}/ancestry: parent identity not retained')
            records = [stable[pid] for pid in sorted(accepted)]
            self._known.update(_process_key(record) for record in records)
            rss = [p['rss_kib'] for p in records if p['rss_kib'] is not None]
            pss = [p['pss_kib'] for p in records if p['pss_kib'] is not None]
            snapshot = {'at': at, 'phase': self._phase, 'processes': records,
                        'status': 'partial' if issues or not records else 'complete',
                        'issues': issues,
                        'rss_sum_kib': (sum(rss) if records and
                                        len(rss) == len(records) else None),
                        'known_rss_sum_kib': sum(rss) if rss else None,
                        'rss_missing_processes': len(records) - len(rss),
                        'pss_sum_kib': (sum(pss) if records and
                                        len(pss) == len(records) else None),
                        'known_pss_sum_kib': sum(pss) if pss else None,
                        'pss_missing_processes': len(records) - len(pss)}
            self._snapshots.append(snapshot)

    def report(self):
        with self._lock:
            snapshots = copy.deepcopy(self._snapshots)
            intervals = [_interval(a, b, self.ticks_per_second)
                         for a, b in zip(snapshots, snapshots[1:])]
            end = self._ended_at if self._ended_at is not None else self.clock()
            phases = {}
            for index, mark in enumerate(self._marks):
                finish = (self._marks[index + 1]['at']
                          if index + 1 < len(self._marks) else end)
                samples = [s for s in snapshots if s['phase'] == mark['name']]
                spans = [i for i in intervals
                         if i['phase'] == mark['name'] and i['seconds'] > 0]
                cpu_spans = [i for i in spans if i['cpu_ms'] is not None]
                switch_spans = [i for i in spans if i['context_switches'] is not None]
                cpu_seconds = sum(i['seconds'] for i in cpu_spans)
                cpu_ms = sum(i['cpu_ms'] for i in cpu_spans) if cpu_spans else None
                def peak(field):
                    values = [s[field] for s in samples if s[field] is not None]
                    return max(values) if values else None
                phases[mark['name']] = {
                    'duration_seconds': finish - mark['at'],
                    'sample_count': len(samples),
                    'partial_samples': sum(s['status'] != 'complete' for s in samples),
                    'sampled_rss_peak_kib': peak('rss_sum_kib'),
                    'sampled_pss_peak_kib': peak('pss_sum_kib'),
                    'retained_rss_kib': samples[-1]['rss_sum_kib'] if samples else None,
                    'retained_pss_kib': samples[-1]['pss_sum_kib'] if samples else None,
                    'observed_cpu_seconds': cpu_seconds, 'cpu_ms': cpu_ms,
                    'cpu_percent': cpu_ms / (cpu_seconds * 10) if cpu_seconds else None,
                    'context_switches': (sum(i['context_switches']
                                             for i in switch_spans)
                                         if switch_spans else None),
                    'partial_intervals': sum(i['status'] != 'complete' for i in spans),
                    'invalid_time_intervals': sum(i['status'] == 'invalid_time'
                                                  for i in intervals
                                                  if i['phase'] == mark['name'])}
            gaps = [i['seconds'] for i in intervals if i['seconds'] > 0]
            return {'schema_version': 1, 'root_pid': self.root_pid,
                    'launched_at': self.launched_at,
                    'ticks_per_second': self.ticks_per_second,
                    'interval_seconds': self.interval, 'lifecycle': self._lifecycle,
                    'error': self._error, 'limits_reached': sorted(self._limits),
                    'first_sample_delay_seconds': (
                        snapshots[0]['at'] - self.launched_at
                        if snapshots else None),
                    'max_sample_gap_seconds': max(gaps) if gaps else None,
                    'snapshots': snapshots, 'intervals': intervals, 'phases': phases,
                    'phase_boundary_seconds': sum(i['seconds'] for i in intervals
                                                  if i['phase'] is None and
                                                  i['seconds'] > 0),
                    'phase_marks': copy.deepcopy(self._marks), 'wakeups': None,
                    'limitations': ['Unseen ephemeral descendants are not measured.',
                                    'Root identity starts at first readable stat.',
                                    'RSS/PSS peaks cover samples only; RSS may '
                                    'double-count shared pages.',
                                    'Per-process HWM is raw; it is never '
                                    'summed as a session peak.',
                                    'Context switches are not wakeups; '
                                    'no scheduler tracing is used.',
                                    'Proc syscall latency cannot be preempted; '
                                    'a join timeout raises an owner error.']}
