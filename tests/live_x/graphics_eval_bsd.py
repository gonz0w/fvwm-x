#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License, version 2 or later.
"""Base-system ps observation for the owned FreeBSD/OpenBSD fixture modules."""
from dataclasses import dataclass
from datetime import datetime
import os
import re
import selectors
import subprocess
import time


MAX_PS_BYTES = 64 * 1024
MAX_CANDIDATES = 16
QUERY_SECONDS = 1
PS_FIELDS = ('pid', 'ppid', 'pgid', 'uid', 'state', 'lstart', 'ucomm')
MONTHS = 'Jan Feb Mar Apr May Jun Jul Aug Sep Oct Nov Dec'.split()
WEEKDAYS = 'Mon Tue Wed Thu Fri Sat Sun'.split()


class ObservationUnavailable(ValueError):
    """A bounded ps reply cannot establish an owned module observation."""


def _require(condition, reason):
    if not condition:
        raise ObservationUnavailable(reason)


def _ps(arguments, deadline):
    _require(time.monotonic() < deadline, 'ps observation deadline')
    child = subprocess.Popen(['/bin/ps', *arguments], stdin=subprocess.DEVNULL,
        stdout=subprocess.PIPE, stderr=subprocess.DEVNULL,
        env={'LC_ALL': 'C', 'TZ': 'UTC'})
    data = bytearray()
    try:
        with selectors.DefaultSelector() as selector:
            selector.register(child.stdout, selectors.EVENT_READ)
            while True:
                remaining = deadline - time.monotonic()
                _require(remaining > 0 and selector.select(remaining),
                    'ps observation deadline')
                block = os.read(child.stdout.fileno(), 8192)
                if not block:
                    break
                _require(len(data) + len(block) <= MAX_PS_BYTES, 'ps output byte bound')
                data.extend(block)
        remaining = deadline - time.monotonic()
        _require(remaining > 0 and child.wait(timeout=remaining) == 0,
            'ps failed or process disappeared')
        return data.decode('ascii')
    finally:
        # The only signal goes to this observer's own ps subprocess.
        try:
            if child.poll() is None:
                child.kill()
            child.wait(timeout=.25)
        finally:
            child.stdout.close()


def _lines(reply):
    _require(reply.endswith('\n') and len(reply) <= MAX_PS_BYTES and
        all(char in ' \t\n' or 33 <= ord(char) <= 126 for char in reply),
        'malformed or truncated ps output')
    lines = reply[:-1].split('\n')
    _require(all(line.strip() for line in lines), 'blank ps row')
    return lines


def _uint(value):
    _require(re.fullmatch(r'[0-9]{1,10}', value), 'nonnumeric ps field')
    return int(value)


def _candidates(reply, parent):
    result, seen = [], set()
    for line in _lines(reply):
        fields = line.split()
        _require(len(fields) == 2, 'malformed numeric ps discovery row')
        pid, ppid = map(_uint, fields)
        _require(pid not in seen, 'ambiguous ps discovery PID')
        seen.add(pid)
        if pid > 0 and pid != parent and ppid == parent:
            result.append(pid)
            _require(len(result) <= MAX_CANDIDATES, 'owned candidate count bound')
    return result


def _start(fields):
    weekday, month, day, stamp, year = fields
    _require(weekday in WEEKDAYS and month in MONTHS and
        re.fullmatch(r'[0-9]{1,2}', day) and
        re.fullmatch(r'[0-9]{2}:[0-9]{2}:[0-9]{2}', stamp) and
        re.fullmatch(r'[0-9]{4}', year), 'unsupported ps start time')
    hour, minute, second = map(int, stamp.split(':'))
    date = datetime(int(year), MONTHS.index(month) + 1, int(day), hour, minute, second)
    _require(WEEKDAYS[date.weekday()] == weekday, 'inconsistent ps start date')
    return ' '.join(fields)


def _module_argv(command, path, alias, ucomm):
    arguments = command.split()
    if not arguments or arguments[0] != path:
        return None
    if len(arguments) == 8 and arguments[-1] == f'({ucomm})':
        arguments.pop()
    # fvwm/module_list.c supplies argv[0], two decimal pipe descriptors, the
    # read filename or none, two hexadecimal window/context values, and alias.
    _require(len(arguments) == 7 and
        all(re.fullmatch(r'[0-9]{1,10}', value) for value in arguments[1:3]) and
        (arguments[3] == 'none' or re.fullmatch(r'/[A-Za-z0-9_./-]+', arguments[3])) and
        all(re.fullmatch(r'[0-9a-f]{1,16}', value) for value in arguments[4:6]),
        'incomplete or unexpected module invocation')
    return tuple(arguments) if arguments[6] == alias else None


@dataclass(frozen=True)
class BsdModuleIdentity:
    pid: int
    ppid: int
    pgid: int
    euid: int
    lstart: str
    ucomm: str
    path: str
    alias: str
    argv: tuple[str, ...]


class BsdModuleObserver:
    """Observe direct children of the fixture's live, session-owning core."""

    def __init__(self, core_popen):
        self.core = core_popen
        self.euid = os.geteuid()
        self._bound = {}

    def _live(self, deadline):
        _require(time.monotonic() < deadline, 'ps observation deadline')
        _require(self.core.poll() is None, 'owned core exited')

    def _read(self, pid, deadline, measure=False):
        self._live(deadline)
        names = PS_FIELDS + (('time', 'rss') if measure else ()) + ('command',)
        arguments = ['-ww', '-p', str(pid)]
        for name in names:
            arguments += ['-o', name + '=']
        lines = _lines(_ps(arguments, deadline))
        self._live(deadline)
        _require(len(lines) == 1, 'ambiguous ps detail row')
        count = 14 if measure else 12
        fields = lines[0].split(None, count - 1)
        _require(len(fields) == count, 'truncated ps detail row')
        observed_pid, ppid, pgid, euid = map(_uint, fields[:4])
        # HarnessRuntime starts the core in its own session, with PGID == PID.
        _require((observed_pid, ppid, pgid, euid) ==
            (pid, self.core.pid, self.core.pid, self.euid), 'foreign or stale ps row')
        _require(fields[4][0] in 'DILRSTW', 'retired or unsupported process state')
        ucomm = fields[10]
        _require(re.fullmatch(r'[A-Za-z0-9_.-]{1,64}', ucomm), 'unsupported ps ucomm')
        identity = dict(pid=pid, ppid=ppid, pgid=pgid, euid=euid,
            lstart=_start(fields[5:10]), ucomm=ucomm)
        return identity, fields[-1], fields[11:13] if measure else ()

    def bind(self, path, alias):
        """Bind one exact displayed invocation or return None."""
        deadline = time.monotonic() + QUERY_SECONDS
        try:
            path = os.fspath(path)
            _require(isinstance(path, str) and len(path) <= 1024 and
                re.fullmatch(r'/[A-Za-z0-9_./-]+', path) and
                isinstance(alias, str) and re.fullmatch(r'[A-Za-z0-9_-]{1,128}', alias),
                'unsupported fixture invocation')
            self._live(deadline)
            children = _candidates(_ps(['-ax', '-o', 'pid=', '-o', 'ppid='], deadline),
                self.core.pid)
            matches = []
            for pid in children:
                fields, command, _ = self._read(pid, deadline)
                argv = _module_argv(command, path, alias, fields['ucomm'])
                if argv is not None:
                    matches.append(BsdModuleIdentity(**fields, path=path, alias=alias, argv=argv))
            self._live(deadline)
            _require(len(matches) == 1, 'missing or ambiguous module invocation')
            identity = matches[0]
            _require(identity.pid in self._bound or len(self._bound) < MAX_CANDIDATES,
                'bound identity count limit')
            self._bound[identity.pid] = identity
            return identity
        except (OSError, ValueError, TypeError, subprocess.TimeoutExpired):
            return None

    def sample(self, identity):
        """Recheck the binding and report the displayed CPU/RSS precision."""
        pid = getattr(identity, 'pid', None)
        sample = dict(status='unavailable', pid=pid, cpu_centiseconds=None,
            cpu_text=None, cpu_resolution_centiseconds=None, rss_kib=None,
            high_water_kib=None, wakeups=None)
        try:
            _require(isinstance(identity, BsdModuleIdentity) and
                self._bound.get(pid) == identity, 'module was not bound by this observer')
            fields, command, counters = self._read(pid,
                time.monotonic() + QUERY_SECONDS, measure=True)
            argv = _module_argv(command, identity.path, identity.alias, fields['ucomm'])
            _require(argv is not None and BsdModuleIdentity(**fields,
                path=identity.path, alias=identity.alias, argv=argv) == identity,
                'bound module identity changed')
            cpu_text, rss = counters
            cpu = re.fullmatch(r'([0-9]{1,10}):([0-5][0-9])\.([0-9]{2})', cpu_text)
            _require(cpu is not None, 'unsupported cumulative ps CPU format')
            minutes, seconds, centiseconds = map(int, cpu.groups())
            rss_kib = _uint(rss)
            sample.update(status='collected', cpu_text=cpu_text,
                cpu_centiseconds=6000 * minutes + 100 * seconds + centiseconds,
                cpu_resolution_centiseconds=1, rss_kib=rss_kib)
        except (OSError, ValueError, TypeError, subprocess.TimeoutExpired) as error:
            sample['reason'] = str(error)
        return sample
