#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or any later version.
"""Opt-in independent FileManager1 native bridge fault checks, without UI.

Run the actual enabled helper against a fixture-owned dbus-daemon. Independent
installed dbus-send processes make real ass method calls. This fixture stands
in for the private Go parent only: injected reply bytes are recorded explicitly
and never claimed as browser admission or completed filesystem work. No browser,
provider, X display, fake bus implementation or service activation is supplied.

The two-second native admission timer is observed with a 750 ms scheduling
allowance, not asserted as a hard real-time bound. UNIX-FD injection is unavailable
with dbus-send; native FD policy requires its separate client/unit qualification.
"""
import argparse
import array
import fcntl
import hashlib
import json
import os
import platform
from pathlib import Path
import re
import select
import shutil
import subprocess
import sys
import termios
import time

from live_x_harness import HarnessRuntime, ManagedProcess, wait_for_predicate

NAME = 'org.freedesktop.FileManager1'
PATH = '/org/freedesktop/FileManager1'
SCHEDULING_ALLOWANCE = 0.75


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def write_json(path, value):
    temporary = path.with_suffix('.tmp')
    temporary.write_text(json.dumps(value, indent=2) + '\n')
    temporary.replace(path)


class Client:
    """One independent real bus call, with an owned process and finite Wait."""
    def __init__(self, run, label, method, arguments=(), destination=NAME,
            path=PATH):
        self.run = run
        self.record = dict(label=label, started=time.monotonic())
        argv = [str(run.args.dbus_send), '--bus=' + run.address,
            '--type=method_call', '--print-reply', '--reply-timeout=5000',
            '--dest=' + destination, path, method, *arguments]
        self.process = run.runtime.start(label, argv, label + '.client.log', run.env)
        self.record.update(argv=argv, pid=self.process.process.pid,
            log=label + '.client.log')
        run.evidence['calls'].append(self.record)

    def pending(self, seconds=0.12):
        deadline = time.monotonic() + seconds
        while time.monotonic() < deadline:
            assert self.process.poll() is None, 'call resolved before matching reply: ' + str(self.record)
            select.select([], [], [], max(0, min(0.02, deadline - time.monotonic())))
        self.record.setdefault('pending_observations', []).append(seconds)

    def finish(self, success=False, error=None, timeout=6):
        status = self.process.wait_until_exit(time.monotonic() + timeout)
        text = self.process.log_path.read_text()
        self.record.update(returncode=status,
            elapsed=time.monotonic() - self.record['started'])
        assert (status == 0) == success, (self.record, text)
        if success:
            lines = text.strip().splitlines()
            assert len(lines) == 1 and lines[0].startswith('method return '), text
        else:
            assert text.strip() and 'method return ' not in text, text
            if error:
                assert error in text, (error, text)
        self.record['status'] = 'passed'
        return text


class Bridge:
    """Own native pipes and child identity; read/write only bounded frames."""
    def __init__(self, run, label):
        self.run, self.label = run, label
        self.pending = bytearray()
        self.record = dict(label=label, frames=[], injected=[], started=time.monotonic())
        run.evidence['bridges'].append(self.record)
        errors = (run.runtime.root / (label + '.stderr')).open('wb')
        argv = [str(run.args.helper), 'filemanager']
        try:
            process = subprocess.Popen(argv, stdin=subprocess.PIPE,
                stdout=subprocess.PIPE, stderr=errors, env=run.env,
                start_new_session=True)
        except BaseException:
            errors.close()
            raise
        self.process = ManagedProcess(label, argv,
            run.runtime.root / (label + '.stderr'), process, errors)
        run.runtime.processes.append(self.process)
        self.input, self.output = process.stdin, process.stdout
        os.set_blocking(self.input.fileno(), False)
        os.set_blocking(self.output.fileno(), False)
        self.raw = (run.runtime.root / (label + '.producer')).open('wb')
        self.injected = (run.runtime.root / (label + '.consumer')).open('wb')
        self.bytes_read = 0
        self.record.update(pid=process.pid, argv=argv,
            producer_log=label + '.producer', consumer_log=label + '.consumer')
        run.bridges.append(self)
        ready = self.frame()
        assert set(ready) == {'version', 'kind', 'owner'}, ready
        assert ready['kind'] == 'ready' and ready['owner'].startswith(':'), ready
        assert run.owner() == ready['owner']
        self.last_id = 0

    def frame(self, timeout=5):
        deadline = time.monotonic() + timeout
        while b'\n' not in self.pending:
            remaining = deadline - time.monotonic()
            assert remaining > 0, 'native frame deadline: ' + self.label
            if not select.select([self.output], [], [], min(0.05, remaining))[0]:
                assert self.process.poll() is None, 'native exited before complete frame'
                continue
            data = os.read(self.output.fileno(), 65536)
            assert data, 'native producer EOF before complete frame'
            self.bytes_read += len(data)
            assert self.bytes_read <= 512 * 1024, 'fixture producer log bound'
            self.raw.write(data); self.raw.flush()
            self.pending.extend(data)
            assert len(self.pending) <= 65536, 'native frame bound'
        line, remainder = self.pending.split(b'\n', 1)
        self.pending = bytearray(remainder)
        value = json.loads(line)
        assert value['version'] == 1, value
        self.record['frames'].append(dict(received=time.monotonic(), value=value))
        return value

    def request(self, client, method, uris, startup='bridge-fixture'):
        value = self.frame(timeout=2)
        assert set(value) == {'version', 'kind', 'id', 'method', 'uris', 'startup_id'}, value
        assert value['kind'] == 'request' and value['method'] == method, value
        assert value['uris'] == uris and value['startup_id'] == startup, value
        number = int(value['id'])
        assert str(number) == value['id'] and number > self.last_id
        self.last_id = number
        client.record['native_id'] = value['id']
        return value['id']

    def send(self, data, fault=None):
        assert len(data) <= 514, 'bounded intentional parent injection'
        self.injected.write(data); self.injected.flush()
        self.record['injected'].append(dict(bytes=len(data),
            sha256=hashlib.sha256(data).hexdigest(), fault=fault,
            attempted=time.monotonic()))
        pending = memoryview(data)
        deadline = time.monotonic() + 0.5
        while pending:
            assert time.monotonic() < deadline, 'parent write deadline'
            try:
                count = os.write(self.input.fileno(), pending)
                assert count > 0
                pending = pending[count:]
            except BlockingIOError:
                select.select([], [self.input], [], 0.02)

    def reply(self, id_, status='accepted'):
        return (json.dumps(dict(version=1, id=id_, status=status,
            detail='controlled-parent-' + status), separators=(',', ':')) + '\n').encode()

    def retire(self, client=None, deadline=None, bus_alive=True):
        result = self.process.wait_until_exit(deadline or time.monotonic() + 2)
        self.record.update(returncode=result, retired_at=time.monotonic(),
            elapsed=time.monotonic() - self.record['started'], retired=True)
        assert result != 0, 'retired helper unexpectedly returned success'
        if client:
            client.finish()
        if bus_alive:
            assert not self.run.has_owner(), 'retired bridge retained bus name'

    def close(self):
        for stream in (self.input, self.output, self.raw, self.injected):
            if stream and not stream.closed:
                stream.close()
        self.process.close(time.monotonic() + 3)


class Run:
    def __init__(self, args, runtime, evidence):
        self.args, self.runtime, self.evidence = args, runtime, evidence
        self.bridges, self.serial = [], 0
        busdir = runtime.root / 'bus'
        busdir.mkdir(mode=0o700)
        self.bus = runtime.start('private-bus', [str(args.dbus_daemon),
            '--session', '--nofork', '--nopidfile', '--print-address=1',
            '--address=unix:path=' + str(busdir / 'socket')], 'private-bus.log')
        self.address = wait_for_predicate('owned bridge fixture bus', lambda:
            (lambda data: data.splitlines()[0] if data.startswith('unix:')
                else None)(self.bus.log_path.read_text()), time.monotonic() + 8,
            process=self.bus).split(',guid=', 1)[0]
        self.env = runtime.environment({'DBUS_SESSION_BUS_ADDRESS': self.address})
        evidence['bus_address'] = self.address

    def query(self, method):
        self.serial += 1
        call = Client(self, 'owner-%03d' % self.serial,
            'org.freedesktop.DBus.' + method, ['string:' + NAME],
            'org.freedesktop.DBus', '/org/freedesktop/DBus')
        result = call.process.wait_until_exit(time.monotonic() + 3)
        assert result == 0
        text = call.process.log_path.read_text()
        assert text.startswith('method return '), text
        call.record.update(returncode=result, status='passed')
        return text

    def owner(self):
        text = self.query('GetNameOwner')
        values = re.findall(r'^\s*string "(:[^"]+)"$', text, re.M)
        assert len(values) == 1, text
        return values[0]

    def has_owner(self):
        text = self.query('NameHasOwner')
        assert ('boolean true' in text) != ('boolean false' in text), text
        return 'boolean true' in text

    def call(self, label, method='ShowFolders', uris=None):
        uris = ['file:///tmp/bridge-fixture'] if uris is None else uris
        assert all(',' not in value for value in uris)
        return Client(self, label, NAME + '.' + method,
            ['array:string:' + ','.join(uris), 'string:bridge-fixture'])


def checks(run):
    cases = run.evidence['cases']
    uris = ['file:///tmp/bridge-fixture']
    bridge = Bridge(run, 'status-mapping')
    for index, (method, status, error) in enumerate([
            ('ShowFolders', 'accepted', None), ('ShowItems', 'accepted', None),
            ('ShowItemProperties', 'accepted', None),
            ('ShowFolders', 'invalid', 'org.freedesktop.DBus.Error.InvalidArgs'),
            ('ShowItems', 'refused', 'org.freedesktop.DBus.Error.LimitsExceeded'),
            ('ShowItemProperties', 'unknown', 'org.freedesktop.DBus.Error.Failed')]):
        client = run.call('status-%d' % index, method)
        id_ = bridge.request(client, method, uris)
        bridge.send(bridge.reply(id_, status))
        client.finish(success=status == 'accepted', error=error)
    cases['three_methods_empty_returns_and_status_mapping'] = 'passed'

    client = run.call('stale-then-matching')
    id_ = bridge.request(client, 'ShowFolders', uris)
    bridge.send(bridge.reply(str(int(id_) - 1)), fault='stale prior ID')
    client.pending()
    bridge.send(bridge.reply(str(int(id_) + 1)), fault='unsubmitted future ID')
    client.pending()
    reply = bridge.reply(id_)
    midpoint = len(reply) // 2
    bridge.send(reply[:midpoint], fault='partial matching reply, no newline')
    client.pending()
    bridge.send(reply[midpoint:])
    client.finish(success=True)
    cases['stale_future_ids_ignored_and_partial_reply_completed'] = 'passed'

    first = run.call('pending-first')
    id_ = bridge.request(first, 'ShowFolders', uris)
    second = run.call('pending-second')
    second.finish(error='admission_busy')
    first.pending()
    assert not bridge.pending and not select.select([bridge.output], [], [], 0)[0], \
        'busy request unexpectedly forwarded to parent'
    bridge.send(bridge.reply(id_))
    first.finish(success=True)
    cases['second_request_busy_first_preserved'] = 'passed'
    bridge.close()
    assert not run.has_owner()

    for label, data in [('malformed-json', b'{\n'),
            ('duplicate-field', b'{"version":1,"id":"1","id":"2","status":"accepted","detail":"x"}\n'),
            ('oversize-parent-frame', b'x' * 513)]:
        bridge = Bridge(run, label)
        client = run.call(label + '-call')
        bridge.request(client, 'ShowFolders', uris)
        bridge.send(data, fault=label)
        bridge.retire(client)
        cases[label + '_retires_without_success'] = 'passed'
        bridge.close()

    bridge = Bridge(run, 'backpressure')
    if not hasattr(fcntl, 'F_SETPIPE_SZ') or not hasattr(termios, 'FIONREAD'):
        cases['partial_native_output_backpressure'] = dict(status='unavailable',
            reason='Linux pipe sizing/occupancy observation unavailable')
    else:
        size = fcntl.fcntl(bridge.output.fileno(), fcntl.F_SETPIPE_SZ, 4096)
        large = ['file:///tmp/' + 'a' * 12000]
        assert size < len(large[0]), 'pipe must force a partial native write'
        client = run.call('backpressure-call', uris=large)
        occupancy = array.array('i', [0])
        def full():
            fcntl.ioctl(bridge.output.fileno(), termios.FIONREAD, occupancy, True)
            return occupancy[0] == size
        wait_for_predicate('actual full native producer pipe', full,
            time.monotonic() + 1, process=bridge.process)
        client.pending(0.15)
        fcntl.ioctl(bridge.output.fileno(), termios.FIONREAD, occupancy, True)
        assert occupancy[0] == size
        bridge.record['backpressure'] = dict(pipe_bytes=size,
            observed_unread_bytes=occupancy[0], uri_bytes=len(large[0]))
        id_ = bridge.request(client, 'ShowFolders', large)
        bridge.send(bridge.reply(id_))
        client.finish(success=True)
        cases['partial_native_output_backpressure'] = 'passed'
    bridge.close()
    assert not run.has_owner()

    bridge = Bridge(run, 'no-parent-reply')
    client = run.call('deadline-call')
    id_ = bridge.request(client, 'ShowFolders', uris)
    observed = time.monotonic()
    bridge.retire(client, deadline=observed + 2 + SCHEDULING_ALLOWANCE)
    elapsed = bridge.record['retired_at'] - observed
    assert elapsed <= 2 + SCHEDULING_ALLOWANCE
    late = bridge.reply(id_)
    try:
        bridge.send(late, fault='late reply after admission deadline')
    except BrokenPipeError:
        bridge.record['late_reply_pipe_closed'] = True
    assert not run.has_owner()
    cases['no_reply_deadline_and_late_reply'] = dict(status='passed',
        native_deadline_seconds=2, scheduling_allowance=SCHEDULING_ALLOWANCE,
        observed_seconds_from_complete_frame=elapsed)
    bridge.close()

    for label, close_input in [('parent-eof', True), ('producer-reader-closed', False)]:
        bridge = Bridge(run, label)
        client = run.call(label + '-call')
        bridge.request(client, 'ShowFolders', uris)
        if close_input:
            bridge.input.close()
        else:
            bridge.output.close()
        bridge.retire(client)
        cases[label + '_retires'] = 'passed'
        bridge.close()

    bridge = Bridge(run, 'bus-loss')
    client = run.call('bus-loss-call')
    bridge.request(client, 'ShowFolders', uris)
    run.bus.close(time.monotonic() + 3)
    bridge.retire(client, bus_alive=False)
    cases['actual_private_bus_loss_retires'] = 'passed'
    bridge.close()


def main():
    parser = argparse.ArgumentParser(description=__doc__, allow_abbrev=False)
    parser.add_argument('--helper', type=Path, required=True)
    parser.add_argument('--artifacts-dir', type=Path, required=True)
    parser.add_argument('--dbus-send', type=Path)
    parser.add_argument('--dbus-daemon', type=Path)
    parser.add_argument('--execution-profile', choices=('native-linux',
        'emulated-linux', 'unclassified-linux'), default='unclassified-linux')
    args = parser.parse_args()
    if sys.platform != 'linux':
        parser.error('run only in the owning Linux executor; no candidate execution on macOS')
    for name, tool in [('helper', None), ('dbus_send', 'dbus-send'),
            ('dbus_daemon', 'dbus-daemon')]:
        value = getattr(args, name) or (shutil.which(tool) if tool else None)
        if not value:
            parser.error('unavailable required independent tool: ' + str(tool))
        path = Path(value).resolve()
        if not path.is_file() or not os.access(path, os.X_OK):
            parser.error('unavailable executable: ' + str(path))
        setattr(args, name, path)
    out = args.artifacts_dir.resolve()
    out.mkdir(parents=True, exist_ok=False)
    source = Path(__file__).resolve().parents[2]
    inputs = ['tests/live_x/filemanager_bridge_live.py',
        'tests/live_x/live_x_harness.py', 'bin/file_manager_bus.c',
        'bin/file_manager_policy.c', 'bin/file_manager_policy.h']
    evidence = dict(status='running', execution_profile=args.execution_profile,
        executor=platform.uname()._asdict(),
        cases={}, calls=[], bridges=[], cleanup_failures=[],
        source_sha256={name: digest(source / name) for name in inputs},
        binaries={name: dict(path=str(getattr(args, name)),
            sha256=digest(getattr(args, name))) for name in
            ('helper', 'dbus_send', 'dbus_daemon')},
        limits=dict(unix_fd_injection=dict(status='unavailable',
            reason='dbus-send cannot attach UNIX file descriptors'),
            browser_admission_ui_and_filesystem_work='not run; raw native bridge only',
            hard_real_time_or_rss_bound='not established',
            native_cpu_hardware_acceptance=False))
    runtime, run = HarnessRuntime(prefix='fvwm-filemanager-bridge-'), None
    try:
        run = Run(args, runtime, evidence)
        checks(run)
        evidence['status'] = 'incomplete' if any(isinstance(value, dict) and
            value.get('status') == 'unavailable' for value in evidence['cases'].values()) else 'passed'
    except BaseException as failure:
        evidence.update(status='failed', failure=repr(failure))
        raise
    finally:
        def clean(label, operation):
            try:
                operation()
            except BaseException as failure:
                evidence['status'] = 'failed'
                evidence['cleanup_failures'].append(dict(step=label, error=repr(failure)))
        if run:
            for bridge in reversed(run.bridges):
                clean('bridge ' + bridge.label, bridge.close)
        for process in reversed(runtime.processes):
            clean('process ' + process.name,
                lambda p=process: p.close(time.monotonic() + 3))
        evidence['owned_processes'] = [dict(name=p.name, argv=p.argv,
            pid=p.process.pid, returncode=p.poll()) for p in runtime.processes]
        for path in runtime.root.iterdir():
            if path.is_file():
                clean('copy ' + path.name, lambda p=path: shutil.copy2(p, out / p.name))
        clean('runtime', runtime.close)
        write_json(out / 'result.json', evidence)
    print(json.dumps(dict(status=evidence['status'], artifacts=str(out))))
    return 0 if evidence['status'] == 'passed' else 2


if __name__ == '__main__':
    raise SystemExit(main())
