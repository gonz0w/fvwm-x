#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License, version 2 or later.
"""Observe fixed power providers and retained fake lockers on real nested X.

Every native power/locker executable is an owned fixture. login1 is exported
only on a private fixture bus. No real power, authentication or secure lock is
performed. Use one coherent optional-enabled wrapper/helper prefix: the wrapper
uses its build-configured helper path, which this script cannot override.
Source authoring is not execution evidence. Each invocation needs a fresh OUT.
"""
import argparse
import ctypes as C
import hashlib
import json
import os
from pathlib import Path
import platform
import shutil
import subprocess
import sys
import time

from live_x_harness import HarnessRuntime, start_display_server, wait_for_predicate
from mfl_request_live import frame, reply
from session_services_live import PROFILES, Session, dismiss_panel


def save(path, value):
    path.write_text(json.dumps(value, indent=2) + '\n')


def rows(path):
    return [json.loads(line) for line in path.read_text().splitlines()] if path.exists() else []


class NativeTools:
    """A PATH containing only fixed, harmless fixture executables."""
    def __init__(self, root, artifacts):
        self.root = root
        root.mkdir(mode=0o700)
        self.bin = root / 'bin'
        self.bin.mkdir(mode=0o700)
        # Retain the release sentinel and event log if bounded cleanup fails.
        # Removing a temporary runtime must never make a fake locker miss its
        # release and linger until the fixture's emergency deadline.
        self.events = artifacts / 'native-events-live.jsonl'
        self.release = artifacts / 'release-locker'
        self.locker_exits = []
        self.mode = root / 'native-mode'
        self.mode.write_text('normal\n')
        script = ('#!' + sys.executable + '\n' +
            'import json, os, sys, time\nfrom pathlib import Path\n' +
            'root = Path(' + repr(str(root)) + ')\n' +
            'events = Path(' + repr(str(self.events)) + ')\n' +
            'release = Path(' + repr(str(self.release)) + ')\n' +
            'name = Path(sys.argv[0]).name\n' +
            'birth = None\n' +
            'try: birth = Path("/proc/self/stat").read_text().rsplit(") ", 1)[1].split()[19]\n' +
            'except (OSError, IndexError): pass\n' +
            'def record(event):\n' +
            '    fd = os.open(events, os.O_WRONLY | os.O_CREAT | os.O_APPEND, 0o600)\n' +
            '    try: os.write(fd, (json.dumps(dict(tool=name, event=event, pid=os.getpid(), birth=birth, argv=sys.argv[1:])) + "\\n").encode())\n' +
            '    finally: os.close(fd)\n' +
            'record("started")\n' +
            'if name in ("xlock", "slock"):\n' +
            '    deadline = time.monotonic() + 120\n' +
            '    while not release.exists():\n' +
            '        if time.monotonic() >= deadline:\n' +
            '            record("fixture-deadline")\n' +
            '            sys.exit(9)\n' +
            '        time.sleep(.02)\n' +
            '    record("released")\n' +
            '    sys.exit(0)\n' +
            'mode = (root / "native-mode").read_text().strip()\n' +
            'if mode == "stall": time.sleep(10)\n' +
            'sys.exit(1 if mode == "deny" else 0)\n')
        for name in ['systemctl', 'shutdown', 'xlock', 'slock']:
            path = self.bin / name
            path.write_text(script)
            path.chmod(0o700)

    def environment(self, address=None):
        environment = {'PATH': str(self.bin),
            # Empty explicit address prevents any conventional system-bus use.
            'DBUS_SYSTEM_BUS_ADDRESS': address if address else ''}
        return environment

    def starts(self, *tools):
        return [row for row in rows(self.events)
                if row['event'] == 'started' and row['tool'] in tools]

    def powers(self):
        return self.starts('systemctl', 'shutdown')

    def release_locker(self):
        self.release.write_text('release owned fake locker\n')

    def release_and_observe_exit(self):
        self.release_locker()
        def exited():
            events = rows(self.events)
            observations = []
            for child in self.starts('xlock', 'slock'):
                pid, birth = child['pid'], child.get('birth')
                released = any(event['event'] == 'released' and
                    event['pid'] == pid and event.get('birth') == birth for event in events)
                state = 'running'
                if Path('/proc').is_dir() and birth is not None:
                    try:
                        fields = Path('/proc/%d/stat' % pid).read_text().rsplit(') ', 1)[1].split()
                        if fields[19] != birth:
                            state = 'original-process-exited-pid-reused'
                        elif fields[0] == 'Z':
                            state = 'exited-zombie'
                    except FileNotFoundError:
                        state = 'exited-absent'
                else:
                    try:
                        os.kill(pid, 0)
                    except ProcessLookupError:
                        state = 'exited-absent'
                observations.append({'pid': pid, 'birth': birth,
                    'released_event': released, 'process_state': state,
                    'reaping_attested': False})
            self.locker_exits = observations
            return all(item['released_event'] and item['process_state'] != 'running'
                for item in observations)
        wait_for_predicate('owned fake locker exit after persistent release', exited,
            time.monotonic() + 3)
        return self.locker_exits


class PrivatePower:
    def __init__(self, runtime, options, out):
        self.runtime, self.options, self.out = runtime, options, out
        self.root = runtime.root / 'private-power'
        self.root.mkdir(mode=0o700)
        self.address = 'unix:path=' + str(self.root / 'bus')
        self.env = runtime.environment({'DBUS_SYSTEM_BUS_ADDRESS': self.address,
            'DBUS_SESSION_BUS_ADDRESS': self.address})
        self.daemon = shutil.which('dbus-daemon')
        assert self.daemon, 'dbus-daemon is required for optional provider qualification'
        self.bus_serial = 0
        self.start_bus()
        self.process, self.logs, self.serial = None, [], 0
        self.scenario = self.root / 'scenario'
        self.scenario.write_text('yes yes yes normal\n')

    def start_bus(self):
        self.bus_serial += 1
        self.bus = self.runtime.start('private-power-bus-' + str(self.bus_serial),
            [self.daemon, '--session', '--nofork', '--nopidfile', '--address=' + self.address],
            'private-power-bus-%02d.log' % self.bus_serial, self.env)
        wait_for_predicate('owned private power bus socket',
            (self.root / 'bus').exists, time.monotonic() + 5, process=self.bus)

    def start(self, permissions=('yes', 'yes', 'yes'), mode='normal', replace=False):
        if self.process and not replace:
            self.process.close(time.monotonic() + 2)
        self.serial += 1
        temporary = self.root / 'next-scenario'
        temporary.write_text(' '.join((*permissions, mode)) + '\n')
        temporary.replace(self.scenario)
        log = self.out / ('provider-%02d.log' % self.serial)
        argv = [str(self.options.provider), '--private-bus', self.address,
            str(self.scenario), str(log)]
        if replace:
            argv.append('--replace')
        previous = self.process
        self.process = self.runtime.start('power-fixture-' + str(self.serial),
            argv, 'power-fixture-%02d.log' % self.serial, self.env)
        self.logs.append(log)
        wait_for_predicate('fixed private power provider ready',
            lambda: log.exists() and 'ready\n' in log.read_text(),
            time.monotonic() + 5, process=self.process)
        if previous and replace:
            previous.close(time.monotonic() + 2)
        return log

    def calls(self, log=None):
        sources = [log] if log else self.logs
        return [line for source in sources if source.exists()
            for line in source.read_text().splitlines()
            if line.startswith(('PowerOff ', 'Reboot '))]

    def method_calls(self, method):
        return [line for source in self.logs if source.exists()
            for line in source.read_text().splitlines()
            if line.startswith(method + ' ') or line == 'rejected method=' + method]

    def helper(self, *arguments):
        started = time.monotonic()
        result = subprocess.run([str(self.options.helper), *arguments],
            env=self.env, capture_output=True, text=True, timeout=5)
        record = {'argv': list(arguments), 'seconds': time.monotonic() - started,
            'returncode': result.returncode, 'stdout': result.stdout,
            'stderr': result.stderr}
        save(self.out / ('helper-%03d.json' % len(list(self.out.glob('helper-*.json')))), record)
        assert result.returncode == 0, record
        assert len(result.stdout.encode()) <= 16384, record
        value = json.loads(result.stdout)
        assert value['version'] == 1 and value['kind'] == 'power', value
        assert [c['action'] for c in value['capabilities']] == ['poweroff', 'reboot', 'suspend'], value
        return value


def terminal(s, identifier):
    return wait_for_predicate('provider operation terminal outcome',
        lambda: (record if (record := s.operation_state(identifier))['state']
            in ['completed', 'failed', 'cancelled'] else None),
        time.monotonic() + 10, process=s.process)


def operation(s, target, connection=None, wait=True):
    """Retry refused admission only; never repeat an admitted mutation."""
    deadline = time.monotonic() + 8
    peer = connection or s.rpc
    while True:
        settings = s.request(resource='settings', scope='all')
        s.serial += 1
        token = 'provider-start-' + str(s.serial)
        peer.send(frame(token, method='action', resource='operations',
            operation='start', provider=s.provider,
            generation=settings['generation'], target=target))
        packet = peer.record()['reply']
        assert packet['id'] == token, packet
        if packet['status'] == 'ok':
            identifier = packet['result']['operation']['id']
            record = {'admission': 'accepted', 'id': identifier, 'target': target}
            receipt = s.out / ('operation-' + token + '.json')
            save(receipt, record)
            if wait:
                record['operation'] = terminal(s, identifier)
                save(receipt, record)
            return record
        code = packet['error']['code']
        if code not in ['operation_busy', 'stale_generation']:
            record = {'admission': 'refused', 'code': code, 'target': target}
            save(s.out / ('operation-' + token + '.json'), record)
            return record
        assert time.monotonic() < deadline, packet
        time.sleep(.05)


def outcome(record):
    return record.get('operation', {}).get('result', {}).get('outcome')


def reprobe(s):
    record = operation(s, 'power-reprobe')
    assert record['admission'] == 'accepted', record
    observed = s.status()
    assert observed['schema_version'] == 2, observed
    assert len(observed['power']['generation']) == 32, observed
    assert not observed['locker']['secure_ready'], observed
    return observed


def stop_session(s):
    if s.process.poll() is None:
        # Quit closes the connection; a command barrier after it is invalid.
        s.control.send(b'Quit\n')
        assert s.process.wait_until_exit(time.monotonic() + 8) == 0


def record_case(s, name, callback):
    record = {'status': 'running'}
    s.evidence['cases'][name] = record
    try:
        record['observations'] = callback()
        record['status'] = 'passed'
    except BaseException as error:
        record.update(status='failed', type=type(error).__name__, message=str(error))
        raise
    finally:
        save(s.out / ('case-' + name + '.json'), record)


def native_cases(s, tools):
    def fixed_requests():
        first = reprobe(s)
        assert first['power']['provider'] == 'portable'
        assert first['power']['state'] == 'ready', first
        assert [c['permission'] for c in first['power']['capabilities']] == ['unknown', 'unknown', 'na']
        records = {}
        for action in ['poweroff', 'reboot']:
            reprobe(s)
            before = len(tools.powers())
            records[action] = operation(s, action)
            assert outcome(records[action]) == 'requested', records[action]
            assert len(tools.powers()) == before + 1, tools.powers()
            assert tools.powers()[-1]['tool'] == 'systemctl', tools.powers()
            assert tools.powers()[-1]['argv'] == [action], tools.powers()
        return records
    record_case(s, 'native-fixed-requests', fixed_requests)

    def native_failure():
        tools.mode.write_text('deny\n')
        reprobe(s)
        before = len(tools.powers())
        failed = operation(s, 'poweroff')
        assert outcome(failed) == 'outcome-unconfirmed', failed
        assert len(tools.powers()) == before + 1
        s.control.echo('native-failure-no-replay-observed')
        time.sleep(.3)
        assert len(tools.powers()) == before + 1
        tools.mode.write_text('normal\n')
        return {'request': failed, 'calls': tools.powers(), 'observation_seconds': .3}
    record_case(s, 'native-failure-no-replay', native_failure)

    def shutdown_fallback():
        (tools.bin / 'systemctl').unlink()
        reprobe(s)
        records = {}
        for action, flag in [('poweroff', '-p'), ('reboot', '-r')]:
            reprobe(s)
            records[action] = operation(s, action)
            assert outcome(records[action]) == 'requested', records[action]
            assert tools.powers()[-1]['tool'] == 'shutdown'
            assert tools.powers()[-1]['argv'] == [flag, 'now']
        (tools.bin / 'shutdown').unlink()
        absent = reprobe(s)
        before = len(tools.powers())
        refused = operation(s, 'poweroff')
        assert absent['power']['state'] == 'unavailable', absent
        assert refused['admission'] == 'refused', refused
        assert len(tools.powers()) == before
        return {'fallback': records, 'absent': absent, 'refused': refused}
    record_case(s, 'native-fallback-and-absence', shutdown_fallback)


def reconnect_desktop(s):
    old = s.provider
    s.control.send(b'KillModule FvwmDesktop\nModule FvwmDesktop\n')
    wait_for_predicate('replacement Desktop provider',
        lambda: (observed if (observed := s.ready()) and observed['provider'] != old else None),
        time.monotonic() + 12, process=s.process)
    s.provider = s.settings['provider']
    wait_for_predicate('parent observed after Desktop restart', s.owner_ready,
        time.monotonic() + 10, process=s.process)
    return old, s.provider


def restart_core(s):
    endpoint = s.home / ('fvwm_mfl_' + s.display.replace(':', '_') + '.sock')
    s.control.send(b'Restart\n')
    s.control.expect_closed()
    for peer in [s.control, s.rpc]:
        peer.close()
    def connect():
        peers = []
        try:
            for _ in range(2):
                peers.append(s.connect(endpoint))
            peers[0].send(b'set echo\n')
            peers[0].echo('provider-after-core-restart')
            return peers
        except (OSError, AssertionError):
            for peer in peers:
                peer.close()
            return None
    s.control, s.rpc = wait_for_predicate('real core Restart reconnect', connect,
        time.monotonic() + 15, process=s.process)
    wait_for_predicate('Desktop ready after core Restart', s.ready,
        time.monotonic() + 15, process=s.process)
    s.provider = s.settings['provider']
    wait_for_predicate('parent observed after core Restart', s.owner_ready,
        time.monotonic() + 10, process=s.process)


def locker_cases(s, tools):
    def retained():
        initial = reprobe(s)
        generation = initial['session_generation']
        endpoint = s.home / ('fvwm_mfl_' + s.display.replace(':', '_') + '.sock')
        requester = s.connect(endpoint)
        # A public operation outlives this requester's socket. Closing after
        # admission does not become authority to terminate the active locker.
        admitted = operation(s, 'lock', requester, wait=False)
        requester.close()
        assert admitted['admission'] == 'accepted', admitted
        admitted['operation'] = terminal(s, admitted['id'])
        assert outcome(admitted) == 'started', admitted
        wait_for_predicate('fake locker foreground child',
            lambda: tools.starts('xlock'), time.monotonic() + 5, process=s.process)
        witness = tools.starts('xlock')[0]
        pid = witness['pid']
        os.kill(pid, 0)
        results = {'started': admitted, 'locker_pid': pid}
        for phase in ['requester-disconnect', 'desktop-restart', 'core-restart']:
            if phase == 'desktop-restart':
                results['desktop_providers'] = reconnect_desktop(s)
            elif phase == 'core-restart':
                restart_core(s)
            observed = s.status()
            assert observed['session_generation'] == generation, observed
            assert observed['locker']['state'] in ['starting', 'running'], observed
            assert not observed['locker']['secure_ready'], observed
            duplicate = operation(s, 'lock')
            if duplicate['admission'] == 'accepted':
                # Public operation admission precedes the parent's duplicate
                # refusal. A terminal busy result is valid only with the same
                # still-running locker and no newly started fixture child.
                record = duplicate['operation']
                locker = record.get('result', {}).get('locker', {})
                assert record['state'] == 'failed' and outcome(duplicate) in ['duplicate', 'busy'], duplicate
                assert locker.get('state') in ['starting', 'running'], duplicate
                assert locker.get('generation') == observed['locker']['generation'], duplicate
                assert locker.get('secure_ready') is False, duplicate
            else:
                assert duplicate['code'] in ['operation_busy', 'operation_unavailable'], duplicate
            assert len(tools.starts('xlock', 'slock')) == 1, rows(tools.events)
            os.kill(pid, 0)
            results[phase] = {'status': observed, 'duplicate': duplicate}
        before = len(tools.powers())
        refused = operation(s, 'suspend')
        assert refused['admission'] == 'refused' or outcome(refused) in ['unavailable', 'secure-readiness-unconfirmed'], refused
        assert len(tools.powers()) == before
        results['suspend'] = refused
        stop_session(s)
        os.kill(pid, 0)
        results['survives_wrapper_exit'] = True
        tools.release_locker()
        wait_for_predicate('only fixture release ends fake locker',
            lambda: any(row['event'] == 'released' and row['pid'] == pid
                for row in rows(tools.events)), time.monotonic() + 5)
        results['secure_readiness_claim'] = False
        return results
    record_case(s, 'retained-locker-and-suspend-refusal', retained)


def login_cases(s, tools, bus):
    def absence():
        direct = bus.helper('power-probe')
        assert direct['available'] and direct['state'] == 'unavailable', direct
        observed = reprobe(s)
        assert observed['power']['provider'] == 'login1', observed
        assert observed['power']['state'] == 'unavailable', observed
        assert not tools.powers()
        return {'helper': direct, 'parent': observed}
    record_case(s, 'login1-absent', absence)

    def permissions():
        results = {}
        for permission in ['no', 'challenge', 'na', 'inhibited', 'inhibitor-blocked',
                           'challenge-inhibitor-blocked', 'unknown']:
            log = bus.start((permission, 'yes', 'yes'))
            direct = bus.helper('power-probe')
            observed = reprobe(s)
            assert observed['power']['state'] == 'ready', observed
            capabilities = observed['power']['capabilities']
            assert [c['permission'] for c in capabilities] == [permission, 'yes', 'yes'], observed
            refusal = operation(s, 'poweroff')
            assert refusal['admission'] == 'refused', refusal
            guarded = bus.helper('power-request', 'poweroff', direct['server_owner'], direct['bus_id'])
            expected = {'no': 'denied', 'challenge': 'authentication-required',
                'na': 'unavailable', 'unknown': 'failed'}.get(permission, 'inhibited')
            assert guarded['state'] == expected, guarded
            assert not bus.calls(log), bus.calls(log)
            assert not tools.powers(), tools.powers()
            results[permission] = {'helper': guarded, 'public': observed, 'refusal': refusal}
        return results
    record_case(s, 'login1-independent-permissions', permissions)

    def guarded_requests():
        log = bus.start()
        direct = bus.helper('power-probe')
        initial = reprobe(s)
        # Direct helper probes verify both hidden transport guards. Public
        # generation checks are exercised separately without exposing tuples.
        for owner, identity in [(direct['server_owner'], '0' * 32),
                                (':999999.999999', direct['bus_id'])]:
            stale = bus.helper('power-request', 'poweroff', owner, identity)
            assert stale['state'] == 'stale', stale
        assert not bus.calls(log)
        replacement = bus.start(replace=True)
        stale = bus.helper('power-request', 'poweroff', direct['server_owner'], direct['bus_id'])
        assert stale['state'] == 'stale', stale
        refreshed = reprobe(s)
        assert refreshed['power']['generation'] != initial['power']['generation']
        assert refreshed['generation'] != initial['generation']
        s.serial += 1
        token = 'stale-provider-generation-' + str(s.serial)
        s.rpc.send(frame(token, method='action', resource='operations',
            operation='start', provider=s.provider, generation=initial['generation'],
            target='poweroff'))
        assert reply(s.rpc, token, status='error') == 'stale_generation'
        assert not bus.calls(replacement)
        requests = {}
        for action in ['poweroff', 'reboot']:
            reprobe(s)
            before = len(bus.calls(replacement))
            requests[action] = operation(s, action)
            assert outcome(requests[action]) == 'requested', requests[action]
            assert len(bus.calls(replacement)) == before + 1, bus.calls(replacement)
            assert 'interactive=false' in bus.calls(replacement)[-1]
        before = len(bus.calls())
        assert not bus.method_calls('Suspend'), bus.method_calls('Suspend')
        suspend = operation(s, 'suspend')
        assert suspend['admission'] == 'refused' or outcome(suspend) in ['unavailable', 'secure-readiness-unconfirmed'], suspend
        assert len(bus.calls()) == before
        assert not bus.method_calls('Suspend'), bus.method_calls('Suspend')
        assert not tools.powers()
        return {'old': direct, 'replacement': refreshed, 'requests': requests, 'suspend': suspend}
    record_case(s, 'login1-guards-replacement-and-requests', guarded_requests)

    def failures():
        results = {}
        inventory = {item['id']: item for item in s.status()['items']
            if item['id'] in ['bus', 'notifications']}
        for mode, expected in [('deny', 'denied'), ('challenge', 'authentication-required'),
                               ('stall-request', 'outcome-unconfirmed'),
                               ('loss', 'outcome-unconfirmed')]:
            log = bus.start(mode=mode)
            reprobe(s)
            before = len(bus.calls(log))
            record = operation(s, 'poweroff')
            assert outcome(record) == expected, record
            assert len(bus.calls(log)) == before + 1, bus.calls(log)
            time.sleep(.3)
            assert len(bus.calls(log)) == before + 1
            assert not tools.powers()
            current = {item['id']: item for item in s.status()['items']
                if item['id'] in ['bus', 'notifications']}
            assert current == inventory, {'before': inventory, 'after': current}
            results[mode] = record
        bus.start(mode='stall')
        started = time.monotonic()
        stalled = reprobe(s)
        elapsed = time.monotonic() - started
        assert elapsed < 7, elapsed
        assert all(c['permission'] == 'unknown' for c in stalled['power']['capabilities']), stalled
        s.control.echo('desktop-usable-after-bounded-provider-stall')
        bus.start()
        recovered = reprobe(s)
        assert recovered['power']['state'] == 'ready', recovered
        assert recovered['power']['capabilities'][0]['permission'] == 'yes'
        assert not tools.powers()
        results['stall'] = {'seconds': elapsed, 'observed': stalled, 'recovered': recovered}
        return results
    record_case(s, 'login1-denial-loss-stall-recovery', failures)

    def policy_handshake():
        s.change({'session_power': 'portable'})
        wait_for_predicate('live portable policy handshake',
            lambda: s.status()['active']['session_power'] == 'portable',
            time.monotonic() + 8, process=s.process)
        native = reprobe(s)
        assert native['power']['provider'] == 'portable' and native['power']['state'] == 'ready'
        s.change({'session_power': 'login1'})
        wait_for_predicate('live login1 policy handshake',
            lambda: s.status()['active']['session_power'] == 'login1',
            time.monotonic() + 8, process=s.process)
        login = reprobe(s)
        assert login['power']['provider'] == 'login1' and login['power']['state'] == 'ready'
        assert not tools.powers()
        return {'portable': native, 'login1': login}
    record_case(s, 'live-shared-power-policy', policy_handshake)

    def bus_loss_and_reconnect():
        bus.start()
        original = bus.helper('power-probe')
        before = reprobe(s)
        inventory = {item['id']: item for item in before['items']
            if item['id'] in ['bus', 'notifications']}
        bus.bus.close(time.monotonic() + 2)
        lost = reprobe(s)
        assert lost['power']['state'] in ['unavailable', 'failed'], lost
        refused = operation(s, 'poweroff')
        assert refused['admission'] == 'refused', refused
        assert not tools.powers()
        current = {item['id']: item for item in lost['items']
            if item['id'] in ['bus', 'notifications']}
        assert current == inventory, {'before': inventory, 'after': current}
        # Only this fixture's stopped daemon ever owned this private pathname.
        if (bus.root / 'bus').exists():
            (bus.root / 'bus').unlink()
        bus.start_bus()
        bus.start()
        replacement = bus.helper('power-probe')
        assert replacement['bus_id'] != original['bus_id']
        guarded = bus.helper('power-request', 'poweroff',
            replacement['server_owner'], original['bus_id'])
        assert guarded['state'] == 'stale', guarded
        recovered = reprobe(s)
        assert recovered['power']['state'] == 'ready', recovered
        assert recovered['power']['generation'] != before['power']['generation']
        assert not tools.powers()
        return {'lost': lost, 'refusal': refused, 'replacement_guard': guarded,
            'recovered': recovered, 'independent_inventory': inventory}
    record_case(s, 'private-system-bus-loss-and-reconnect', bus_loss_and_reconnect)
    assert not bus.method_calls('Suspend'), bus.method_calls('Suspend')
    s.evidence['suspend_provider_method_calls'] = bus.method_calls('Suspend')


def click_confirmation(s, window):
    buttons = []
    for child in s.children(window):
        attrs = s.attrs(child)
        if attrs and attrs.map_state == 2 and attrs.all_event_masks & (1 << 2):
            buttons.append((attrs.y, attrs.x, child, attrs))
    # The actual confirmation has only Cancel and the action. Identify its
    # right-hand native button, rather than constructing a private callback.
    assert len(buttons) == 2, [(row[0], row[1]) for row in buttons]
    _, _, child, attrs = sorted(buttons)[-1]
    x, y, descendant = C.c_int(), C.c_int(), C.c_ulong()
    assert s.x.XTranslateCoordinates(s.d, child, s.client.root,
        attrs.width // 2, attrs.height // 2, C.byref(x), C.byref(y), C.byref(descendant))
    s.pointer.motion(x.value, y.value)
    current, ancestry = s.client.root, []
    for _ in range(8):
        root, below = C.c_ulong(), C.c_ulong()
        root_x, root_y, local_x, local_y = C.c_int(), C.c_int(), C.c_int(), C.c_int()
        mask = C.c_uint()
        assert s.x.XQueryPointer(s.d, current, C.byref(root), C.byref(below),
            C.byref(root_x), C.byref(root_y), C.byref(local_x),
            C.byref(local_y), C.byref(mask))
        ancestry.append(current)
        if current == child:
            assert 0 <= local_x.value < attrs.width
            assert 0 <= local_y.value < attrs.height
            break
        assert below.value, {'expected_button': child, 'pointer_ancestry': ancestry}
        current = below.value
    else:
        raise AssertionError('pointer did not resolve to native confirmation button')
    pressed = time.monotonic()
    try:
        s.pointer.button(True)
        # Realistic input duration lets Form handle ButtonPress and install its
        # release-mask grab. This is input timing, never service readiness.
        time.sleep(.05)
    finally:
        held = time.monotonic() - pressed
        s.pointer.button(False)
        s.evidence.setdefault('confirmation_clicks', []).append({
            'button': child, 'pointer_ancestry': ancestry,
            'root_position': [x.value, y.value],
            'minimum_hold_seconds': .05, 'actual_hold_seconds': held,
            'service_or_readiness_witness': False})
    wait_for_predicate('native confirmation action closed Form',
        lambda: s.named('Session') is None,
        time.monotonic() + 5, process=s.process)


def ui_cases(s, tools):
    def consumers():
        reprobe(s)
        s.command('SendToModule FvwmDesktop session-panel')
        panel = s.form('Session and services')
        s.capture('session-providers.png')
        s.key(panel, 'F8')
        wait_for_predicate('native power probe remains ready',
            lambda: s.status()['power']['state'] == 'ready',
            time.monotonic() + 8, process=s.process)
        panel = s.form('Session and services')
        s.key(panel, 'F2')
        editor = s.form('Session settings')
        s.capture('session-four-settings.png')
        s.key(editor, 'Escape')
        dismiss_panel(s)
        s.command('SendToModule FvwmDesktop confirm poweroff')
        confirmation = s.form('Session')
        s.capture('poweroff-confirmation.png')
        s.key(confirmation, 'Escape')
        assert not tools.powers()
        # The normal FVWM menu is a separate consumer; it owns an X grab until
        # Escape. Do not send a command-stream barrier while that grab is live.
        s.control.send(b'Popup SlateSession\n')
        wait_for_predicate('native Session menu grab', s.pointer.grabbed,
            time.monotonic() + 5, process=s.process)
        s.capture('session-menu.png')
        code = s.x.XKeysymToKeycode(s.d, s.x.XStringToKeysym(b'Escape'))
        s.pointer.x.XTestFakeKeyEvent.argtypes = [C.c_void_p, C.c_uint, C.c_int, C.c_ulong]
        s.pointer.x.XTestFakeKeyEvent.restype = C.c_int
        for pressed in [1, 0]:
            assert s.pointer.x.XTestFakeKeyEvent(s.d, code, pressed, 0)
        s.x.XSync(s.d, 0)
        wait_for_predicate('Session menu grab released', lambda: not s.pointer.grabbed(),
            time.monotonic() + 5, process=s.process)
        s.control.echo('native-session-menu-closed')
        return {'settings_count': 15, 'session_fields': 4,
            'confirmation_cancel_calls': len(tools.powers())}
    record_case(s, 'native-provider-consumers', consumers)

    def stale_confirmation():
        reprobe(s)
        s.command('SendToModule FvwmDesktop confirm poweroff')
        confirmation = s.form('Session')
        before = len(tools.powers())
        settings = s.request(resource='settings', scope='all')
        s.change({'motion': not settings['settings']['motion']['saved']})
        retirement = 'dismissed after shared generation change'
        if s.named('Session'):
            click_confirmation(s, confirmation)
            retirement = 'native submit refused after shared generation change'
        s.control.echo('stale-native-confirmation-observed')
        assert len(tools.powers()) == before
        reprobe(s)
        s.command('SendToModule FvwmDesktop confirm poweroff')
        s.form('Session')
        old = s.status()['power']['generation']
        observed = reprobe(s)
        assert observed['power']['generation'] != old
        wait_for_predicate('provider reprobe retires native confirmation',
            lambda: s.named('Session') is None,
            time.monotonic() + 5, process=s.process)
        assert len(tools.powers()) == before
        s.command('SendToModule FvwmDesktop confirm poweroff')
        s.form('Session')
        s.change({'session_power': 'login1'})
        wait_for_predicate('policy change retires native confirmation',
            lambda: s.named('Session') is None,
            time.monotonic() + 5, process=s.process)
        assert len(tools.powers()) == before
        s.change({'session_power': 'portable'})
        wait_for_predicate('portable policy restored after confirmation test',
            lambda: s.status()['active']['session_power'] == 'portable',
            time.monotonic() + 8, process=s.process)
        return {'native_calls': before, 'changed_shared_setting': 'motion',
            'settings_generation_guard': retirement,
            'power_generation_guard': 'native confirmation retired',
            'policy_guard': 'native confirmation retired'}
    record_case(s, 'native-confirmation-stale-cas', stale_confirmation)

    def confirmed_request():
        reprobe(s)
        s.command('SendToModule FvwmDesktop confirm poweroff')
        confirmation = s.form('Session')
        before = len(tools.powers())
        click_confirmation(s, confirmation)
        wait_for_predicate('native confirmed fake power request',
            lambda: len(tools.powers()) == before + 1,
            time.monotonic() + 8, process=s.process)
        observed = wait_for_predicate('parent request acknowledgment',
            lambda: (value if (value := s.status())['power']['state'] == 'requested' else None),
            time.monotonic() + 8, process=s.process)
        assert tools.powers()[-1]['tool'] == 'systemctl'
        assert tools.powers()[-1]['argv'] == ['poweroff']
        return {'status': observed, 'tool': tools.powers()[-1], 'completion_observed': False}
    record_case(s, 'native-confirmation-submit', confirmed_request)


def run_group(options, out, name, callback, login=False, profile=None):
    destination = out / name
    destination.mkdir()
    result = {'status': 'running'}
    session = tools = None
    runtime = HarnessRuntime(prefix='fvwm-pwr-')
    def cleanup(label, action):
        try:
            action()
        except BaseException as error:
            result['status'] = 'failed'
            result.setdefault('cleanup_failures', []).append({'action': label,
                'type': type(error).__name__, 'message': str(error)})
    try:
        _, options.display = start_display_server(runtime, 'Xvfb',
            ['Xvfb', '-screen', '0', '1440x1000x24', '-noreset'],
            'xserver.log', time.monotonic() + 10)
        tools = NativeTools(runtime.root / 'native-tools', destination)
        bus = PrivatePower(runtime, options, destination) if login else None
        environment = tools.environment(bus.address if bus else None)
        session = Session(runtime, options, destination, profile or name,
            power='login1' if login else 'portable', extra_env=environment)
        session.evidence['provider_fixture'] = True
        session.evidence['wrapper_pid'] = session.process.process.pid
        session.evidence['native_path_only'] = str(tools.bin)
        if login:
            session.evidence['private_bus'] = {'address': bus.address,
                'daemon': bus.daemon,
                'daemon_sha256': hashlib.sha256(Path(bus.daemon).read_bytes()).hexdigest(),
                'argv': bus.bus.argv, 'pid': bus.bus.process.pid}
            callback(session, tools, bus)
        else:
            callback(session, tools)
        stop_session(session)
        session.evidence['status'] = 'passed'
        result['status'] = 'passed'
    except BaseException as error:
        result.update(status='failed', type=type(error).__name__, message=str(error))
        if session:
            session.evidence.update(result)
    finally:
        if tools:
            cleanup('release owned fake locker', tools.release_locker)
        if session:
            cleanup('close session observer', session.close)
            cleanup('reap owned session', lambda: session.process.close(time.monotonic() + 3))
        if tools:
            cleanup('observe owned fake locker exit', tools.release_and_observe_exit)
            cleanup('preserve owned fake locker exit observation', lambda:
                save(destination / 'locker-cleanup.json', {
                    'release_file': str(tools.release), 'children': tools.locker_exits}))
        if session:
            cleanup('preserve session evidence', lambda: save(destination / 'evidence.json', session.evidence))
        if tools and tools.events.exists():
            cleanup('preserve native fixture events', lambda:
                shutil.copyfile(tools.events, destination / 'native-events.jsonl'))
        for path in runtime.root.glob('*/configuration.fvwm'):
            cleanup('preserve configuration', lambda path=path:
                shutil.copyfile(path, destination / 'configuration.fvwm'))
        for path in runtime.root.glob('*.log'):
            cleanup('preserve child log', lambda path=path:
                shutil.copyfile(path, destination / path.name))
        cleanup('close owned runtime', runtime.close)
        save(destination / 'group-result.json', result)
    return result


def run(options):
    out = options.artifacts_dir.resolve()
    out.mkdir(parents=True, exist_ok=False)
    evidence = {'status': 'running', 'groups': {}, 'limits': [
        'Nested Xvfb only; no physical authentication, real power or secure-lock claim.',
        'The supplied wrapper must already select the coherent built/installed optional helper.',
        'Provider fixture has finite 30-second/256-message life; each scenario starts a fresh owner.',
        'Notification IDs use the separate session-services optional consumer qualification.',
        'Native-CPU/sanitizer and full platform/release qualification are separate.'],
        'executor': {'system': platform.system(), 'release': platform.release(),
            'machine': platform.machine(), 'cpu_execution': 'not inferred from uname'},
        'argv': sys.argv[1:]}
    paths = [options.session, options.fvwm, options.mfl, options.desktop,
        options.form, options.capture, options.helper, options.provider,
        Path(__file__).resolve(), Path(__file__).with_name('session_services_live.py')]
    try:
        evidence['inputs'] = {str(path): hashlib.sha256(path.read_bytes()).hexdigest() for path in paths}
        evidence['selectors'] = {'providers_only': options.providers_only,
            'ui_only': options.ui_only, 'provider_cases': options.provider_cases,
            'profiles': options.profiles}
        if not options.ui_only:
            for name, callback in [('native', native_cases), ('locker', locker_cases), ('login1', login_cases)]:
                if name in options.provider_cases:
                    evidence['groups'][name] = run_group(options, out, name,
                        callback, login=name == 'login1')
                    save(out / 'evidence.json', evidence)
        if not options.providers_only:
            for profile in options.profiles:
                evidence['groups'][profile] = run_group(options, out, profile, ui_cases, profile=profile)
                save(out / 'evidence.json', evidence)
        evidence['status'] = ('passed' if evidence['groups'] and
            all(group['status'] == 'passed' for group in evidence['groups'].values()) else 'failed')
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
    parser.add_argument('--provider', type=Path, required=True)
    parser.add_argument('--helper', type=Path)
    for name in ['session', 'fvwm', 'mfl', 'desktop', 'form', 'capture', 'data-root', 'modules-dir']:
        parser.add_argument('--' + name, type=Path)
    parser.add_argument('--profiles', nargs='+', choices=PROFILES, default=PROFILES)
    selectors = parser.add_mutually_exclusive_group()
    selectors.add_argument('--providers-only', action='store_true')
    selectors.add_argument('--ui-only', action='store_true')
    parser.add_argument('--provider-cases', nargs='+', choices=['native', 'locker', 'login1'],
        default=['native', 'locker', 'login1'])
    options = parser.parse_args()
    options.build = options.build.resolve()
    source = Path(__file__).resolve().parents[2]
    defaults = {'session': options.build / 'bin/FvwmPrompt/fvwm-session',
        'fvwm': options.build / 'fvwm3', 'mfl': options.build / 'modules/FvwmMFL/FvwmMFL',
        'desktop': options.build / 'bin/FvwmPrompt/FvwmDesktop',
        'form': options.build / 'modules/FvwmForm/FvwmForm',
        'capture': options.build / 'bin/fvwm-desktop-capture',
        'helper': options.build / 'bin/fvwm-session-bus',
        'data_root': source / 'default-config'}
    for name, default in defaults.items():
        setattr(options, name, (getattr(options, name) or default).resolve())
    options.provider = options.provider.resolve()
    if options.modules_dir:
        options.modules_dir = options.modules_dir.resolve()
    return options


if __name__ == '__main__':
    run(arguments())
