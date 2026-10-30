#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License, version 2 or later.
"""Observe owned optional-bus lifetime with a coherent installed session/core.

Linux /proc and pidfds are required. All signals target owned fixture handles;
no process-name search, system bus, real power or user authentication is exercised.
Use --cases to repeat only affected groups. --expect-disabled selects one scoped
consumer check with the actual installed disabled helper. The ordinary invocation
requires optional support; an unavailable adapter is a failure, not a pass.
The build-configured helper path cannot be overridden by --helper: that argument
identifies the actual installed binary for identity and independent probe checks.
No UI changes, screenshot matrix, universal orphan prevention or reaping of
grandchildren by this observer is claimed. Source authoring is not execution.
"""
import argparse
from copy import deepcopy
import hashlib
import json
import os
from pathlib import Path
import platform
import re
import select
import shutil
import signal
import stat
import subprocess
import sys
import time

from geometry_transition_live import Client
from live_x_harness import HarnessRuntime, start_display_server, wait_for_predicate
from session_providers_live import NativeTools, operation, outcome, reprobe
from session_recovery_live import (OwnedCore, append, app_alive, descriptors,
    identity, rows, save, signal_wrapper)
from session_services_live import Session


ENABLED_CASES = ['parent-loss', 'normal-stop', 'stalled-stop', 'borrowed', 'off',
    'no-ack', 'nonleak', 'daemon-loss', 'guardian-loss']
CASES = ENABLED_CASES + ['disabled']
PRIVATE_ENV = ['FVWM_SESSION_READY_FD', 'FVWM_SESSION_LEASE_FD',
    'FVWM_SESSION_BUS_LIFETIME_FD']


def quote(values):
    # Paths are owned fixture inputs, but reject FVWM command separators too.
    assert all(not any(char in str(value) for char in ['\n', '\r', '"'])
        for value in values), values
    return ' '.join('"' + str(value) + '"' for value in values)


def witness(report, release=None, event='executed'):
    """One ordinary X client; persistent release lives outside runtime cleanup."""
    client = Client(os.environ['DISPLAY']) if release else None
    if client:
        client.create()
    append(report, {'event': event, **identity(os.getpid()),
        'window': client.window if client else None,
        'descriptors': descriptors(os.getpid()),
        'environment': {key: os.environ.get(key) for key in PRIVATE_ENV}})
    if not client:
        return
    try:
        deadline = time.monotonic() + 120
        while not release.exists():
            assert time.monotonic() < deadline, 'owned fixture emergency deadline'
            client.x.XSync(client.display, 0)
            time.sleep(.02)
    finally:
        client.close()
        append(report, {'event': 'released', **identity(os.getpid())})


def children(pid):
    # Linux exposes children per task, not per process. A Go parent can launch
    # on any runtime OS thread, so the thread-group leader alone is incomplete.
    # Enumerate only this known parent's tasks; OwnedProcess still verifies the
    # process PPID, birth, executable and pidfd before adopting a child handle.
    result = set()
    for task in Path('/proc/%d/task' % pid).iterdir():
        try:
            result.update(int(value) for value in (task / 'children').read_text().split())
        except FileNotFoundError:
            pass  # A runtime task can end during enumeration.
    return sorted(result)


def argv(pid):
    return Path('/proc/%d/cmdline' % pid).read_bytes().rstrip(b'\0').decode().split('\0')


class OwnedProcess:
    """Capture a known direct child before any reparenting, then retain pidfd."""
    def __init__(self, pid, parent, executable):
        self.fd = None
        before = identity(pid)
        fd = os.pidfd_open(pid)
        try:
            after = identity(pid)
            assert all(before[key] == after[key] for key in ['pid', 'birth', 'ppid']), (before, after)
            assert before['ppid'] == parent, before
            actual, expected = Path('/proc/%d/exe' % pid).stat(), executable.stat()
            assert (actual.st_dev, actual.st_ino) == (expected.st_dev, expected.st_ino)
            assert not select.select([fd], [], [], 0)[0], 'owned child already exited'
            self.initial, self.executable, self.arguments = before, str(executable), argv(pid)
            self.fd = fd
        except BaseException:
            os.close(fd)
            raise

    def alive(self):
        return not select.select([self.fd], [], [], 0)[0]

    def sample(self):
        assert self.alive(), self.initial
        current = identity(self.initial['pid'])
        assert current['birth'] == self.initial['birth'], current
        actual, expected = Path('/proc/%d/exe' % current['pid']).stat(), Path(self.executable).stat()
        assert (actual.st_dev, actual.st_ino) == (expected.st_dev, expected.st_ino)
        return current

    def send(self, requested):
        self.sample()
        signal.pidfd_send_signal(self.fd, requested)

    def exited(self, seconds=5):
        wait_for_predicate('exact owned process exit', lambda: not self.alive(),
            time.monotonic() + seconds)
        return {'identity': self.initial, 'pidfd_exit_observed': True,
            'reaping_attested_by_observer': False}

    def close(self):
        if self.fd is not None:
            os.close(self.fd)
            self.fd = None


def item(session, name='bus'):
    return next(value for value in session.status()['items'] if value['id'] == name)


class LifetimeRuntime(HarnessRuntime):
    """Extend this fixture's existing autostart witness before wrapper launch."""
    def __init__(self, helper, out):
        super().__init__(prefix='fvwm-bus-life-')
        self.helper, self.out = helper, out

    def start(self, name, arguments, log_name, environment=None, pass_fds=()):
        if Path(arguments[0]).name == 'fvwm-session':
            recorder = Path(environment['HOME']) / 'record-environment.py'
            assert recorder.is_file(), recorder
            recorder.write_text(recorder.read_text() +
                '\nimport subprocess, sys\n' +
                'sys.path.insert(0, ' + repr(str(Path(__file__).resolve().parent)) + ')\n' +
                'from session_bus_lifetime_live import witness\n' +
                'witness(Path(' + repr(str(self.out / 'autostart-descriptors.jsonl')) +
                '), event="autostart")\n' +
                'probe = subprocess.run([' + repr(str(self.helper)) + ', "probe"], ' +
                'env=dict(os.environ, DBUS_SYSTEM_BUS_ADDRESS=""), ' +
                'capture_output=True, text=True, timeout=4)\n' +
                'Path(' + repr(str(self.out / 'autostart-authentication.json')) +
                ').write_text(json.dumps(dict(returncode=probe.returncode, ' +
                'stdout=probe.stdout, stderr=probe.stderr)) + "\\n")\n')
        return super().start(name, arguments, log_name, environment, pass_fds)


class Observation:
    def __init__(self, runtime, options, out, name, display, bus='start', address=None):
        self.runtime, self.options, self.out = runtime, options, out
        self.session = self.core = self.guardian = self.daemon = None
        self.expected_guardian_loss = False
        self.apps, self.handles = [], []
        self.release = out / 'release-applications'
        self.application_log = out / 'independent-applications.jsonl'
        self.tools = NativeTools(runtime.root / 'native-tools', out)
        # NativeTools otherwise deliberately exposes only harmless fake tools.
        # This one additional executable is the resolved actual session daemon.
        (self.tools.bin / 'dbus-daemon').symlink_to(options.daemon)
        options.display = display
        if name == 'no-ack':
            # Keep the installed/default config byte-for-byte except one bounded
            # native startup barrier. Real MFL/public control remains usable.
            root = runtime.root / 'stalled-data'
            root.mkdir()
            for entry in options.data_root.iterdir():
                if entry.name != 'config':
                    (root / entry.name).symlink_to(entry)
            body = (options.data_root / 'config').read_text()
            (root / 'config').write_text(body +
                '\nAddToFunc StartFunction I Module FvwmMFL\n' +
                '+ I Wait BusLifetimeNeverReady\n')
            options.data_root = root
        self.session = Session(runtime, options, out, name, bus=bus,
            address=address, extra_env=self.tools.environment())
        self.core = OwnedCore(self.session.process, options.fvwm)
        self.module = self.session.home / 'modules' / 'BusLifetimeModule'
        self.module.write_text('#!' + sys.executable + '\nimport sys\n' +
            'sys.path.insert(0, ' + repr(str(Path(__file__).resolve().parent)) + ')\n' +
            'from pathlib import Path\nfrom session_bus_lifetime_live import witness\n' +
            'witness(Path(sys.argv[6]), event="module")\n')
        self.module.chmod(0o700)
        self.initial_status = self.session.status()
        save(out / 'initial-status.json', self.initial_status)

    def capture_owned(self, autostart=True):
        parent = self.session.process.process.pid
        def find_guardian():
            for pid in children(parent):
                try:
                    arguments = argv(pid)
                    if len(arguments) == 3 and arguments[1] == 'bus-guardian':
                        candidate = OwnedProcess(pid, parent, self.options.helper)
                        assert Path(arguments[2]).resolve() == self.options.daemon
                        self.guardian = candidate
                        self.handles.append(candidate)
                        return candidate
                except FileNotFoundError:
                    continue  # A short-lived probe can finish during enumeration.
        wait_for_predicate('known wrapper direct guardian child', find_guardian,
            time.monotonic() + 5, process=self.session.process)
        def find_daemon():
            for pid in children(self.guardian.initial['pid']):
                try:
                    candidate = OwnedProcess(pid, self.guardian.initial['pid'],
                        self.options.daemon)
                except FileNotFoundError:
                    continue
                self.daemon = candidate
                self.handles.append(candidate)
                return candidate
        wait_for_predicate('guardian actual direct daemon child', find_daemon,
            time.monotonic() + 5, process=self.session.process)
        observed = item(self.session)
        assert observed['state'] == 'ready' and observed['ownership'] == 'owned', observed
        if autostart:
            wait_for_predicate('actual autostart address witness',
                self.session.environment_marker.exists, time.monotonic() + 8,
                process=self.session.process)
            self.address = json.loads(self.session.environment_marker.read_text())['bus']
        else:
            environment = Path('/proc/%d/environ' % self.core.initial['pid']).read_bytes()
            values = [entry.split(b'=', 1)[1].decode() for entry in environment.split(b'\0')
                if entry.startswith(b'DBUS_SESSION_BUS_ADDRESS=')]
            assert len(values) == 1, 'missing native core private bus address'
            self.address = values[0]
        assert self.address and self.address.startswith('unix:'), self.address
        authenticated = self.probe()
        assert authenticated['state'] == 'ready' and authenticated['available'], authenticated
        autostart_auth = None
        if autostart:
            receipt = self.out / 'autostart-authentication.json'
            wait_for_predicate('actual authentication inside autostart application', receipt.exists,
                time.monotonic() + 6, process=self.session.process)
            autostart_auth = json.loads(receipt.read_text())
            assert autostart_auth['returncode'] == 0, autostart_auth
            reply = json.loads(autostart_auth['stdout'])
            assert reply['available'] and reply['state'] == 'ready', reply
            assert reply['bus_id'] == authenticated['bus_id'], (reply, authenticated)
            no_leak(rows(self.out / 'autostart-descriptors.jsonl')[0], self.pipe())
        text = self.session.process.log_path.read_text()
        if autostart:
            assert text.count('autostart scan started') == 1, text
            assert text.index('core startup acknowledged') < text.index('autostart scan started')
        record = {'guardian': self.guardian.initial, 'guardian_argv': self.guardian.arguments,
            'daemon': self.daemon.initial, 'daemon_argv': self.daemon.arguments,
            'public_bus': observed, 'authenticated_probe': authenticated,
            'autostart_authenticated_probe': autostart_auth,
            'address': self.address, 'autostart_scans': int(autostart), 'pipe': self.pipe()}
        save(self.out / 'owned-start.json', record)
        return record

    def probe(self, expect_ready=True):
        completed = subprocess.run([str(self.options.helper), 'probe'],
            env=dict(self.session.env, DBUS_SESSION_BUS_ADDRESS=self.address,
                DBUS_SYSTEM_BUS_ADDRESS=''), capture_output=True, text=True, timeout=4)
        record = {'argv': [str(self.options.helper), 'probe'],
            'returncode': completed.returncode, 'stdout': completed.stdout,
            'stderr': completed.stderr}
        save(self.out / ('probe-%03d.json' % len(list(self.out.glob('probe-*.json')))), record)
        assert completed.returncode == 0, record
        result = json.loads(completed.stdout)
        assert (result['state'] == 'ready') == expect_ready, result
        return result

    def pipe(self):
        self.core.sample()
        pid = self.core.initial['pid']
        path = Path('/proc/%d/fd/5' % pid)
        info = path.stat()
        assert stat.S_ISFIFO(info.st_mode), info
        data = Path('/proc/%d/fdinfo/5' % pid).read_text()
        flags = int(re.search(r'flags:\s*([0-7]+)', data).group(1), 8)
        assert flags & os.O_CLOEXEC and flags & os.O_ACCMODE in [os.O_WRONLY, os.O_RDWR], data
        key = {'device': info.st_dev, 'inode': info.st_ino}
        guardian_read = Path('/proc/%d/fd/3' % self.guardian.initial['pid']).stat()
        assert (guardian_read.st_dev, guardian_read.st_ino) == (info.st_dev, info.st_ino)
        parent_fds = descriptors(self.session.process.process.pid) if self.session.process.poll() is None else []
        if self.session.process.poll() is None:
            assert any((fd['device'], fd['inode']) == (info.st_dev, info.st_ino)
                for fd in parent_fds), parent_fds
        assert not any((fd['device'], fd['inode']) == (info.st_dev, info.st_ino)
            for fd in descriptors(self.daemon.initial['pid'])), 'daemon retained writer/read pipe'
        environment = Path('/proc/%d/environ' % pid).read_bytes().split(b'\0')
        # /proc/environ can retain original exec bytes after unsetenv; actual
        # child reports below, rather than this initial image, attest removal.
        return {**key, 'core_fd': 5, 'core_close_on_exec': True,
            'guardian_read_fd': 3, 'parent_descriptors': parent_fds,
            'proc_initial_environment_may_retain_marker':
                any(value.startswith(b'FVWM_SESSION_BUS_LIFETIME_FD=') for value in environment)}

    def no_extra_actor(self):
        path = Path('/proc/%d/fd/5' % self.core.initial['pid'])
        # An ordinary module pipe/socket may reuse 5. Its number alone is not
        # evidence of a leaked lifetime writer when no guardian exists.
        environment = Path('/proc/%d/environ' % self.core.initial['pid']).read_bytes().split(b'\0')
        assert not any(value.startswith(b'FVWM_SESSION_BUS_LIFETIME_FD=') for value in environment)
        actors = []
        for pid in children(self.session.process.process.pid):
            try:
                arguments = argv(pid)
            except FileNotFoundError:
                continue
            assert 'bus-guardian' not in arguments, arguments
            actors.append({'identity': identity(pid), 'argv': arguments})
        return {'wrapper_direct_children': actors, 'lifetime_environment_absent': True,
            'core_fd5_observation': os.readlink(path) if path.exists() else None,
            'limit': 'descriptor 5 may be reused for ordinary core work'}

    def application(self):
        self.session.control.send(('Exec exec ' + quote([sys.executable,
            Path(__file__).resolve(), '--_child', self.application_log, self.release]) + '\n').encode())
        wait_for_predicate('independent executed X client', lambda: rows(self.application_log),
            time.monotonic() + 5, process=self.session.process)
        row = rows(self.application_log)[0]
        self.apps.append(row)
        return row

    def stop(self):
        self.session.control.send(b'Quit\n')
        self.core.exited()
        assert self.session.process.wait_until_exit(time.monotonic() + 9) == 0

    def retirement(self, expect_receipt=True):
        result = {'daemon': self.daemon.exited(), 'guardian': self.guardian.exited(),
            'observer_reaped_daemon': False}
        if expect_receipt:
            text = self.session.process.log_path.read_text()
            result['parent_cleanup_diagnostics'] = [line for line in text.splitlines()
                if 'owned session bus cleanup:' in line]
            assert 'daemon Wait confirmed status=' in text, text
            assert 'guardian Wait observed status=' in text, text
        return result

    def cleanup(self):
        errors = []
        def attempt(label, action):
            try:
                action()
            except BaseException as error:
                errors.append({'action': label, 'type': type(error).__name__, 'message': str(error)})
        self.release.write_text('release only owned independent fixture applications\n')
        attempt('release and observe admitted fake lockers', self.tools.release_and_observe_exit)
        save(self.out / 'locker-cleanup.json', self.tools.locker_exits)
        if self.core:
            attempt('close exact owned core', self.core.close)
        if self.session:
            process = self.session.process
            if process.poll() is None:
                attempt('request exact owned wrapper stop', lambda: signal_wrapper(process, signal.SIGTERM))
                attempt('observe wrapper stop', lambda: process.wait_until_exit(time.monotonic() + 9))
            attempt('close X observers', self.session.close)
            save(self.out / 'native-session-observer.json', self.session.evidence)
        if self.daemon and self.daemon.alive():
            # A deliberately killed guardian cannot clean its orphan. Only this
            # previously captured actual daemon handle is an authorized target.
            if self.expected_guardian_loss:
                save(self.out / 'expected-guardian-loss-cleanup.json', {
                    'guardian': self.guardian.initial,
                    'guardian_exit_observed': not self.guardian.alive(),
                    'actual_daemon': self.daemon.initial,
                    'daemon_exit_observed': not self.daemon.alive(),
                    'natural_retirement_expected': False,
                    'reason': 'fixture deliberately killed actual guardian; retire only captured daemon pidfd'})
            else:
                attempt('observe natural owned daemon retirement', lambda: self.daemon.exited(4))
            if self.daemon.alive():
                attempt('terminate captured fixture daemon', lambda: self.daemon.send(signal.SIGTERM))
                attempt('observe captured fixture daemon exit', lambda: self.daemon.exited(3))
        if self.guardian:
            attempt('observe guardian actual exit without killing it', self.guardian.exited)
        observations = []
        def app_exits():
            observations.clear()
            events = rows(self.application_log)
            for start in self.apps:
                released = any(row['event'] == 'released' and row['pid'] == start['pid']
                    and row['birth'] == start['birth'] for row in events)
                try:
                    now = identity(start['pid'])
                    exited = now['birth'] != start['birth'] or now['state'] in ['Z', 'X']
                except FileNotFoundError:
                    exited = True
                observations.append({'pid': start['pid'], 'birth': start['birth'],
                    'released': released, 'exit_observed': exited, 'reaping_attested': False})
            return all(row['released'] and row['exit_observed'] for row in observations)
        attempt('observe independent fixture applications before runtime close', lambda:
            wait_for_predicate('released owned applications', app_exits, time.monotonic() + 4))
        save(self.out / 'application-cleanup.json', observations)
        for handle in self.handles:
            handle.close()
        return errors


def no_leak(record, pipe):
    assert all(value is None for value in record['environment'].values()), record
    assert not any((fd['device'], fd['inode']) == (pipe['device'], pipe['inode'])
        for fd in record['descriptors']), record


def resident_snapshot(observation):
    """Three small samples, scoped to the known parent/guardian/daemon trio."""
    processes = {'parent': observation.session.process.process.pid,
        'guardian': observation.guardian.initial['pid'],
        'daemon': observation.daemon.initial['pid']}
    births = {name: identity(pid)['birth'] for name, pid in processes.items()}
    samples = []
    for index in range(3):
        sample = {'monotonic': time.monotonic(), 'processes': {}}
        for name, pid in processes.items():
            current = identity(pid)
            assert current['birth'] == births[name], current
            root = Path('/proc/%d' % pid)
            fields = (root / 'stat').read_text().rsplit(') ', 1)[1].split()
            record = {'identity': current, 'utime_ticks': int(fields[11]),
                'stime_ticks': int(fields[12]), 'wchan': (root / 'wchan').read_text().strip()}
            for filename in ['smaps_rollup', 'status']:
                try:
                    raw = (root / filename).read_text()
                    (observation.out / ('resident-%d-%s-%s.txt' % (index, name, filename))).write_text(raw)
                    selected = ['Rss', 'Pss'] if filename == 'smaps_rollup' else [
                        'voluntary_ctxt_switches', 'nonvoluntary_ctxt_switches']
                    for key in selected:
                        match = re.search(r'^' + key + r':\s+(\d+)', raw, re.M)
                        record[key] = int(match.group(1)) if match else None
                except OSError as error:
                    record[filename + '_unavailable'] = str(error)
            sample['processes'][name] = record
        samples.append(sample)
        if index < 2:
            time.sleep(.1)
    deltas = {}
    for name in processes:
        first, last = [sample['processes'][name] for sample in [samples[0], samples[-1]]]
        deltas[name] = {key: last[key] - first[key] for key in ['utime_ticks', 'stime_ticks',
            'voluntary_ctxt_switches', 'nonvoluntary_ctxt_switches']
            if first.get(key) is not None and last.get(key) is not None}
    result = {'scope': 'three known owned parent/guardian/daemon processes only',
        'samples': samples, 'deltas': deltas, 'clock_ticks_per_second': os.sysconf('SC_CLK_TCK'),
        'limits': ['Three 100ms-spaced samples, not a benchmark or unsampled peak.',
            'RSS/PSS are nonsimultaneous proc samples; no desktop memory total.',
            'Context switches approximate activity, not scheduler wakeup counts.',
            'No CPU threshold or native-CPU qualification is inferred.']}
    save(observation.out / 'resident-observation.json', result)
    return result


def parent_loss(o, result):
    result['owned_start'] = o.capture_owned()
    result['resident'] = resident_snapshot(o)
    app = o.application()
    pipe = o.pipe()
    no_leak(app, pipe)
    signal_wrapper(o.session.process, signal.SIGKILL)
    assert o.session.process.wait_until_exit(time.monotonic() + 3) == -signal.SIGKILL
    o.session.control.echo('core-and-private-bus-after-parent-loss')
    o.probe()
    app_alive(app)
    before = o.core.sample()
    assert o.pipe()['inode'] == pipe['inode']
    o.session.control.send(b'Restart\n')
    o.session.control.expect_closed()
    o.session.control.close()
    endpoint = o.session.home / ('fvwm_mfl_' + o.session.display.replace(':', '_') + '.sock')
    def reconnect():
        try:
            peer = o.session.connect(endpoint)
            peer.send(b'set echo\n')
            peer.echo('fresh-connection-after-native-restart')
            return peer
        except (OSError, AssertionError):
            return None
    o.session.control = wait_for_predicate('native same-PID Restart new MFL',
        reconnect, time.monotonic() + 12)
    after = o.core.sample()
    assert (before['pid'], before['birth']) == (after['pid'], after['birth'])
    assert o.pipe()['inode'] == pipe['inode']
    o.probe()
    o.session.control.send(b'Quit\n')
    o.core.exited()
    result.update(wrapper_status=-signal.SIGKILL, before_restart=before,
        after_restart=after, retained_pipe=pipe, independent_app_alive=app_alive(app),
        retirement=o.retirement(expect_receipt=False),
        limits=['Parent died before cleanup receipts: process exits observed; no daemon reaping claim.'])


def stop_case(o, result, stalled=False):
    result['owned_start'] = o.capture_owned()
    app = o.application()
    reprobe(o.session)
    admitted = operation(o.session, 'lock')
    assert admitted['admission'] == 'accepted' and outcome(admitted) == 'started', admitted
    wait_for_predicate('admitted owned fake locker', lambda: o.tools.starts('xlock'),
        time.monotonic() + 5, process=o.session.process)
    locker = o.tools.starts('xlock')[0]
    probe = o.session.create('independent client survives owned bus retirement', deletable=False)
    if stalled:
        o.core.send(signal.SIGSTOP)
        wait_for_predicate('stalled exact owned core', lambda: o.core.sample()['state'] in ['T', 't'],
            time.monotonic() + 2)
    started = time.monotonic()
    signal_wrapper(o.session.process, signal.SIGTERM)
    if stalled:
        wait_for_predicate('repeat stop interval', lambda: time.monotonic() - started >= 1.5,
            time.monotonic() + 2, process=o.session.process)
        signal_wrapper(o.session.process, signal.SIGTERM)
        signal_wrapper(o.session.process, signal.SIGINT)
    wait_for_predicate('exact owned core exit before bus retirement', lambda: not o.core.alive(),
        started + 7)
    core_elapsed = time.monotonic() - started
    code = o.session.process.wait_until_exit(started + 10)
    elapsed = time.monotonic() - started
    o.core.exited()
    text = o.session.process.log_path.read_text()
    assert text.count('WM stop requested; cooperative grace started') == 1, text
    if stalled:
        assert code == 128 + signal.SIGKILL, code
        assert 4.8 <= core_elapsed < 6.35, core_elapsed
        assert text.count('WM stop grace expired; termination requested') == 1, text
    else:
        assert code in [0, 128 + signal.SIGTERM], code
    o.session.pump()
    assert probe['live'] and probe['messages'] == 0 and o.session.attrs(probe['window'])
    result.update(wrapper_status=code, seconds_including_bus_retirement=elapsed,
        seconds_to_core_exit=core_elapsed,
        retirement=o.retirement(), independent_app_alive=app_alive(app),
        locker_admission=admitted, locker_alive=app_alive(locker),
        independent_window=deepcopy(probe), secure_readiness_claim=False)


def borrowed(o, result, borrowed_process):
    observed = item(o.session)
    assert observed['state'] == 'ready' and observed['ownership'] == 'borrowed', observed
    result['no_extra_actor'] = o.no_extra_actor()
    signal_wrapper(o.session.process, signal.SIGKILL)
    assert o.session.process.wait_until_exit(time.monotonic() + 3) == -signal.SIGKILL
    o.session.control.echo('core-usable-with-borrowed-bus-after-parent-loss')
    o.probe()
    o.session.control.send(b'Quit\n')
    o.core.exited()
    assert borrowed_process.poll() is None
    result.update(public_bus=observed, borrowed_bus_alive_after_parent_and_core=True,
        authenticated_after_core_exit=o.probe())


def no_ack(o, result):
    result['owned_start_without_ack'] = o.capture_owned(autostart=False)
    text = lambda: o.session.process.log_path.read_text()
    wait_for_predicate('native missing startup acknowledgment deadline',
        lambda: 'autostart unavailable: core startup acknowledgment:' in text(),
        time.monotonic() + 18, process=o.session.process)
    assert 'core startup acknowledged' not in text() and 'autostart scan started' not in text()
    assert not o.session.environment_marker.exists()
    observed = item(o.session)
    assert observed['state'] == 'ready' and observed['ownership'] == 'owned', observed
    o.session.control.echo('native-core-usable-with-unconfirmed-startup')
    o.stop()
    result.update(autostart_scans=0, actual_core_usable=True,
        retirement=o.retirement(),
        legacy_RL_native_case='unavailable: this coherent core emits RLB; old/refusing RL is a Go unit case')


def nonleak(o, result):
    result['owned_start'] = o.capture_owned()
    pipe = o.pipe()
    app = o.application()
    no_leak(app, pipe)
    module_report = o.out / 'module.jsonl'
    o.session.control.send(('Module BusLifetimeModule ' + str(module_report) + '\n').encode())
    wait_for_predicate('native module execution report', lambda: rows(module_report),
        time.monotonic() + 5, process=o.session.process)
    no_leak(rows(module_report)[0], pipe)
    replacement = o.out / 'replacement.jsonl'
    o.session.control.send(('Restart ' + quote([sys.executable, Path(__file__).resolve(),
        '--_exec', replacement]) + '\n').encode())
    wait_for_predicate('explicit replacement exec report', lambda: rows(replacement),
        time.monotonic() + 6)
    no_leak(rows(replacement)[0], pipe)
    o.core.exited()
    assert o.session.process.wait_until_exit(time.monotonic() + 9) == 0
    result.update(application=app, module=rows(module_report)[0],
        custom_replacement=rows(replacement)[0], lifetime_pipe=pipe,
        retirement=o.retirement(), independent_app_alive=app_alive(app))


def loss(o, result, guardian=False):
    result['owned_start'] = o.capture_owned()
    previous = o.session.status()
    target = o.guardian if guardian else o.daemon
    # Target was captured through exact known parent/child executable identity.
    target.send(signal.SIGKILL if guardian else signal.SIGTERM)
    target.exited()
    o.expected_guardian_loss = guardian
    refreshes = []
    def terminal():
        # The public query reads FvwmDesktop's last observed snapshot. Request
        # an actual correlated owner status before checking that cached view;
        # repeatedly querying the cache cannot witness asynchronous bus loss.
        identifier = o.session.operation('session-status')
        completed = o.session.wait_operation(identifier, 'completed', timeout=3)
        observed = o.session.status()
        refreshes.append({'operation': completed, 'status': observed})
        save(o.out / 'terminal-status-refreshes.json', refreshes)
        bus = next(value for value in observed['items'] if value['id'] == 'bus')
        return observed if bus['state'] in ['failed', 'unavailable'] else None
    observed = wait_for_predicate('terminal owned bus/guardian loss public state', terminal,
        time.monotonic() + 6, process=o.session.process)
    snapshots = [observed]
    for _ in range(2):
        identifier = o.session.operation('session-reprobe')
        o.session.wait_operation(identifier, 'failed')
        snapshots.append(o.session.status())
    for snapshot in snapshots:
        bus = next(value for value in snapshot['items'] if value['id'] == 'bus')
        notification = next(value for value in snapshot['items'] if value['id'] == 'notifications')
        assert bus['state'] in ['failed', 'unavailable'] and notification['state'] != 'ready', snapshot
        assert bus['attempts'] == next(value for value in previous['items'] if value['id'] == 'bus')['attempts']
    text = o.session.process.log_path.read_text()
    assert text.count('autostart scan started') == 1, text
    o.session.control.echo('core-usable-after-terminal-owned-bus-loss')
    result.update(target=target.initial, before=previous, after_probes=snapshots,
        terminal_status_refreshes=refreshes,
        autostart_scans=1, later_probe_revival=False, automatic_replacement=False,
        guardian_failure_orphan_limit=guardian)
    o.stop()
    if guardian:
        # Guardian SIGKILL is an explicit uncertainty. The fixture owns a pidfd
        # to the original daemon and retires only that child after recording it.
        result['actual_daemon_after_guardian_loss'] = (
            o.daemon.sample() if o.daemon.alive() else {'exit_observed': True})
        text = o.session.process.log_path.read_text()
        result['parent_cleanup_diagnostics'] = [line for line in text.splitlines()
            if 'owned session bus cleanup:' in line]
        assert 'daemon Wait confirmed status=' not in text, text
        assert 'guardian Wait observed status=' in text, text
        if o.daemon.alive():
            o.daemon.send(signal.SIGTERM)
        result['fixture_daemon_retirement'] = o.daemon.exited()
    else:
        result['retirement'] = o.retirement()


def persist(runtime, out):
    for path in runtime.root.rglob('*'):
        if path.is_file() and not path.is_symlink() and path.suffix in [
                '.log', '.json', '.jsonl', '.fvwm', '.desktop']:
            destination = out / 'runtime' / path.relative_to(runtime.root)
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(path, destination)


def group(options, destination, name):
    out = destination / name
    out.mkdir()
    # Every selected case has its own options, HOME, X server and service owner.
    options = deepcopy(options)
    runtime = LifetimeRuntime(options.helper, out)
    result, observation = {'status': 'running'}, None
    try:
        _, display = start_display_server(runtime, 'Xvfb',
            ['Xvfb', '-screen', '0', '1440x1000x24', '-noreset'],
            'xserver.log', time.monotonic() + 10)
        bus, address, borrowed_process = 'start', None, None
        if name == 'borrowed':
            path = runtime.root / 'borrowed-bus'
            address = 'unix:path=' + str(path)
            borrowed_process = runtime.start('borrowed-bus', [str(options.daemon),
                '--session', '--nofork', '--nopidfile', '--address=' + address],
                'borrowed-bus.log', runtime.environment({'DBUS_SYSTEM_BUS_ADDRESS': ''}))
            wait_for_predicate('owned fixture borrowed bus socket', path.exists,
                time.monotonic() + 5, process=borrowed_process)
            bus = 'reuse'
        elif name == 'off':
            bus = 'off'
        observation = Observation(runtime, options, out, name, display, bus, address)
        if address:
            observation.address = address
        if options.expect_disabled:
            assert name == 'disabled', name
            observed = item(observation.session)
            assert observed['state'] == 'unavailable' and observed['ownership'] == 'none', observed
            result.update(public_bus=observed, no_extra_actor=observation.no_extra_actor(),
                disabled_optional_adapter=True,
                installed_helper_sha256=hashlib.sha256(options.helper.read_bytes()).hexdigest(),
                scope='same coherent wrapper/core with actual installed optional helper disabled; not full install/profile qualification')
            observation.stop()
        elif name == 'parent-loss':
            parent_loss(observation, result)
        elif name in ['normal-stop', 'stalled-stop']:
            stop_case(observation, result, name == 'stalled-stop')
        elif name == 'borrowed':
            borrowed(observation, result, borrowed_process)
        elif name == 'off':
            observed = item(observation.session)
            assert observed['state'] == 'stopped' and observed['ownership'] == 'none', observed
            result.update(public_bus=observed, no_extra_actor=observation.no_extra_actor())
            observation.stop()
        elif name == 'no-ack':
            no_ack(observation, result)
        elif name == 'nonleak':
            nonleak(observation, result)
        else:
            loss(observation, result, name == 'guardian-loss')
        result['status'] = 'passed'
    except BaseException as error:
        result.update(status='failed', failure={'type': type(error).__name__, 'message': str(error)})
    finally:
        if observation:
            errors = observation.cleanup()
            if errors:
                result.update(status='failed', cleanup_failures=errors)
        # ManagedProcess.close uses group signals. Retire exact owned wrapper
        # handles first, including a constructor failure, before generic cleanup.
        for process in list(runtime.processes):
            if process.name == 'Xvfb' or process.poll() is not None:
                continue
            try:
                if observation is None and Path(process.argv[0]).name == 'fvwm-session':
                    # A failed Session constructor may already own a diagnosed
                    # live core. Capture it while its known parent still lives.
                    try:
                        core = OwnedCore(process, options.fvwm)
                        core.close()
                    except BaseException as error:
                        result['status'] = 'failed'
                        result.setdefault('cleanup_failures', []).append({
                            'action': 'capture/close core after constructor failure',
                            'type': type(error).__name__, 'message': str(error)})
                process.process.terminate()
                try:
                    process.wait_until_exit(time.monotonic() + 9)
                except BaseException:
                    process.process.kill()
                    process.wait_until_exit(time.monotonic() + 2)
            except BaseException as error:
                result['status'] = 'failed'
                result.setdefault('cleanup_failures', []).append({'action': 'exact direct child cleanup',
                    'type': type(error).__name__, 'message': str(error)})
            if process.poll() is None:
                # Do not let the generic harness turn an unconfirmed exact
                # cleanup into a process-group broadcast to independent apps.
                runtime.processes.remove(process)
                process._log_file.close()
                result['status'] = 'failed'
                result.setdefault('cleanup_failures', []).append({
                    'action': 'preserve unresolved exact child; skip group cleanup',
                    'pid': process.process.pid, 'argv': process.argv})
        persist(runtime, out)
        try:
            runtime.close()
        except BaseException as error:
            result.update(status='failed', runtime_cleanup_failure=str(error))
        save(out / 'evidence.json', result)
    return result


def run(options):
    out = options.artifacts_dir.resolve()
    out.mkdir(parents=True, exist_ok=False)
    evidence = {'status': 'running', 'execution_profile': options.execution_profile,
        'uname': list(platform.uname()), 'argv': sys.argv, 'groups': {},
        'limits': ['Native-CPU, real power/user authentication/secure locking and release acceptance remain separate.',
            'Parent-loss and guardian-failure observations do not attest orphan reaping.',
            'Legacy RL/refusing ACK and full guardian malformed/W/U/permission failures belong to C/Go units.',
            'Native terminal-loss group observes post-loss probes; full in-flight probe races remain Go unit coverage.',
            'No private service is ever installed on or queried through the system bus.',
            'No replacement/adoption after terminal owned loss is accepted.']}
    try:
        assert platform.system() == 'Linux' and hasattr(os, 'pidfd_open')
        assert hasattr(signal, 'pidfd_send_signal'), 'pidfd signalling is required'
        paths = [options.session, options.fvwm, options.helper, options.mfl,
            options.desktop, options.form, options.daemon, Path(__file__).resolve()]
        paths += [Path(__file__).with_name(name) for name in [
            'session_services_live.py', 'session_providers_live.py', 'session_recovery_live.py',
            'geometry_transition_live.py', 'live_x_harness.py', 'mfl_request_live.py',
            'mfl_stream_live.py']]
        evidence['inputs'] = {str(path): hashlib.sha256(path.read_bytes()).hexdigest() for path in paths}
        for name in options.cases:
            evidence['groups'][name] = group(options, out, name)
            save(out / 'evidence.json', evidence)
        evidence['status'] = 'passed' if all(value['status'] == 'passed'
            for value in evidence['groups'].values()) else 'failed'
    except BaseException as error:
        evidence.update(status='failed', failure={'type': type(error).__name__, 'message': str(error)})
        raise
    finally:
        save(out / 'evidence.json', evidence)
    assert evidence['status'] == 'passed', evidence


def arguments():
    parser = argparse.ArgumentParser(description=__doc__, allow_abbrev=False)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--artifacts-dir', type=Path, required=True)
    parser.add_argument('--execution-profile', required=True)
    parser.add_argument('--cases', nargs='+', choices=CASES, default=ENABLED_CASES)
    parser.add_argument('--expect-disabled', action='store_true',
        help='Scoped installed disabled-helper consumer: use --cases disabled')
    for name in ['session', 'fvwm', 'helper', 'mfl', 'desktop', 'form', 'data-root', 'modules-dir']:
        parser.add_argument('--' + name, type=Path)
    options = parser.parse_args()
    options.build = options.build.resolve()
    source = Path(__file__).resolve().parents[2]
    defaults = {'session': options.build / 'bin/FvwmPrompt/fvwm-session',
        'fvwm': options.build / 'fvwm3', 'helper': options.build / 'bin/fvwm-session-bus',
        'mfl': options.build / 'modules/FvwmMFL/FvwmMFL',
        'desktop': options.build / 'bin/FvwmPrompt/FvwmDesktop',
        'form': options.build / 'modules/FvwmForm/FvwmForm',
        'data_root': source / 'default-config'}
    for name, default in defaults.items():
        setattr(options, name, (getattr(options, name) or default).resolve())
    if options.modules_dir:
        options.modules_dir = options.modules_dir.resolve()
    daemon = shutil.which('dbus-daemon')
    assert daemon, 'actual installed dbus-daemon is required'
    options.daemon = Path(daemon).resolve()
    options.cases = list(dict.fromkeys(options.cases))
    if options.expect_disabled:
        assert options.cases == ['disabled'], '--expect-disabled requires --cases disabled'
    else:
        assert 'disabled' not in options.cases, '--cases disabled requires --expect-disabled'
    return options


if __name__ == '__main__':
    if len(sys.argv) == 4 and sys.argv[1] == '--_child':
        witness(Path(sys.argv[2]), Path(sys.argv[3]), 'started')
    elif len(sys.argv) == 3 and sys.argv[1] == '--_exec':
        witness(Path(sys.argv[2]), event='replacement')
    else:
        run(arguments())
