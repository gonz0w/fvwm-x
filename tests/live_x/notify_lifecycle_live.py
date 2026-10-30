#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License, version 2 or later.
"""Owned notification startup, nonreading-peer and teardown observations.

This is source for a lead-run Linux nested-X/private-bus gate, not a passing
result. Exact module children retain pidfds before any signal or retirement
observation. A missing-bus module uses two owned module pipes and an actual
Popen Wait; the stalled-bus case is a real core-launched module. A nonreading
peer does no bus work while another peer and the core make progress. External
broker buffers, kernel-blocked exit and physical/platform acceptance are not
inferred from this finite workload.
"""
import argparse
import json
import os
from pathlib import Path
import platform
import select
import shutil
import signal
import socket
import subprocess
import time
import traceback

from live_x_harness import HarnessRuntime, start_display_server, wait_for_predicate
from notify_live import Native, digest
from session_bus_lifetime_live import OwnedProcess, children
from session_recovery_live import identity

CASES = ('missing-bus', 'stalled-startup', 'slow-reader', 'churn', 'teardown',
    'idle-term')


class Lifecycle:
    def __init__(self, native):
        self.native = native
        self.handles = []
        self.module = self.capture()

    def capture(self):
        native = self.native
        parent = native.process.process.pid
        expected = native.options.notify.stat()

        def discover():
            matches = []
            for pid in children(parent):
                try:
                    actual = Path('/proc/%d/exe' % pid).stat()
                    if (actual.st_dev, actual.st_ino) == (expected.st_dev,
                            expected.st_ino):
                        matches.append(pid)
                except FileNotFoundError:
                    pass
            assert len(matches) <= 1, ('multiple notification module children', matches)
            return matches[0] if matches else None
        pid = wait_for_predicate('one exact notification child', discover,
            time.monotonic() + 3, process=native.process)
        handle = OwnedProcess(pid, parent, native.options.notify)
        self.handles.append(handle)
        return handle

    def no_owner(self):
        return 'error' in self.native.first.request('owner')

    def start(self):
        native = self.native
        native.command('Module FvwmNotify')
        owner = wait_for_predicate('explicit notification owner',
            lambda: native.first.request('owner').get('owner'),
            time.monotonic() + 5, process=native.process)
        self.module = self.capture()
        native.command('SendToModule FvwmNotify DND 0')
        wait_for_predicate('native notifications enabled',
            lambda: native.status()['dnd'] == 0,
            time.monotonic() + 3, process=native.process)
        return owner

    def stop(self):
        native = self.native
        self.module.send(signal.SIGTERM)
        retired = self.module.exited(seconds=3)
        wait_for_predicate('module exit releases notification name', self.no_owner,
            time.monotonic() + 3, process=native.process)
        wait_for_predicate('module exit retires popup and center windows',
            lambda: not native.popups() and not native.named('FVWM-X notifications'),
            time.monotonic() + 3, process=native.process)
        native.control.echo('notify-owned-module-retired-core-live')
        return retired

    def memory(self):
        current = self.module.sample()
        values = {}
        for line in Path('/proc/%d/smaps_rollup' % current['pid']).read_text().splitlines():
            if ':' not in line:
                continue
            key, value = line.split(':', 1)
            if key in ('Rss', 'Pss'):
                values[key] = int(value.split()[0])
        return {'identity': current, 'KiB': values}

    def close(self):
        receipts = []
        # Every signal is tied to the original pidfd and verified executable.
        # Failed startup checks do not leave a module stuck in authentication.
        errors = []
        for handle in self.handles:
            try:
                if handle.alive():
                    handle.send(signal.SIGKILL)
                    receipts.append(handle.exited(seconds=3))
            except BaseException:
                errors.append(traceback.format_exc())
            finally:
                handle.close()
        if errors:
            raise AssertionError({'exact_handle_cleanup_errors': errors})
        return receipts


def missing_bus(lifecycle, evidence):
    native = lifecycle.native
    lifecycle.stop()
    incoming, from_fixture = os.pipe()
    to_fixture, outgoing = os.pipe()
    process = None
    started = time.monotonic()
    receipt = {'harness': 'direct actual module with owned module pipes',
        'stderr_log': 'missing-bus-module.log', 'expected_exit': 1}
    evidence['cases']['missing-bus'] = receipt
    try:
        absent = native.home / 'absent-bus'
        assert not absent.exists()
        environment = dict(native.env, DBUS_SESSION_BUS_ADDRESS='unix:path=' + str(absent))
        with (native.out / 'missing-bus-module.log').open('wb') as log:
            process = subprocess.Popen([str(native.options.notify), str(outgoing),
                str(incoming), 'none', '0', '0'], pass_fds=(outgoing, incoming),
                stdin=subprocess.DEVNULL, stdout=log, stderr=subprocess.STDOUT,
                env=environment, start_new_session=True)
            os.close(outgoing)
            outgoing = -1
            os.close(incoming)
            incoming = -1
            receipt['pid'] = process.pid
            try:
                receipt['initial'] = identity(process.pid)
            except FileNotFoundError:
                receipt['initial'] = 'exited before /proc observation; Popen owns Wait'
            native.control.echo('core-usable-during-missing-notification-bus')
            status = process.wait(timeout=3)
            receipt.update(wait_observed=True, exit=status,
                seconds=time.monotonic() - started)
        os.set_blocking(to_fixture, False)
        ready = os.read(to_fixture, 4096)
        assert status == 1, ('missing-bus expected ordinary exit 1', status)
        assert b'NOP FINISHED STARTUP' in ready, 'missing real startup response'
        assert lifecycle.no_owner(), 'missing-bus attempt created an owner'
        assert not native.popups()
        receipt.update(wait_observed=True, exit=status,
            seconds=time.monotonic() - started, startup_pipe_hex=ready.hex(),
            core_echo='passed', owner_absent=True)
    finally:
        if process is not None and process.poll() is None:
            process.kill()
            process.wait(timeout=3)
        for descriptor in (incoming, from_fixture, to_fixture, outgoing):
            if descriptor >= 0:
                os.close(descriptor)
    evidence['cases']['missing-bus'] = receipt
    lifecycle.start()


def stalled_startup(lifecycle, evidence):
    native = lifecycle.native
    lifecycle.stop()
    directory = native.home / 'stalled-bus'
    directory.mkdir(mode=0o700)
    path = directory / 'socket'
    listener = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    accepted = None
    handle = None
    started = time.monotonic()
    receipt = {'address': 'unix:path=' + str(path), 'module_startup_seconds': 5,
        'reaping_attested_by_observer': False,
        'exit_signal_status': 'not available to nonparent pidfd observer'}
    try:
        listener.bind(str(path))
        listener.listen(1)
        listener.settimeout(2)
        native.command('SetEnv DBUS_SESSION_BUS_ADDRESS ' + receipt['address'])
        # The real core consumes the module's startup acknowledgment. Its Echo
        # barrier cannot pass a synchronous module whose acknowledgment stalls.
        native.command('ModuleSynchronous FvwmNotify')
        handle = lifecycle.capture()
        accepted, _ = listener.accept()
        accepted.setblocking(False)
        data = bytearray()
        deadline = time.monotonic() + 2
        while b'AUTH' not in data:
            assert time.monotonic() < deadline, 'no actual authentication attempt'
            if select.select([accepted], [], [], max(0, deadline - time.monotonic()))[0]:
                block = accepted.recv(1024)
                assert block, 'authentication socket closed before witness'
                data.extend(block)
                assert len(data) <= 4096
        receipt['accepted_at_seconds'] = time.monotonic() - started
        receipt['authentication_bytes_hex'] = bytes(data).hex()
        assert handle.alive(), 'module exited before blocked-startup witness'
        native.control.echo('core-live-while-module-authentication-stalled')
        receipt['core_echo_while_blocked'] = 'passed'
        # Keep the accepted socket open and deliberately send no authentication
        # response. Thus socket EOF cannot manufacture the timer's exit result.
        receipt['retired'] = handle.exited(seconds=8)
        elapsed = time.monotonic() - started
        assert 4 <= elapsed < 10, ('unexpected startup timer observation', elapsed)
        assert lifecycle.no_owner()
        native.control.echo('core-live-after-module-startup-deadline')
        receipt.update(seconds=elapsed, owner_absent=True,
            peer_closed_before_module_exit=False, core_echo_after='passed')
    finally:
        # Restore only this owned core's child environment before a later case.
        native.command('SetEnv DBUS_SESSION_BUS_ADDRESS ' + native.address)
        if handle is not None and handle.alive():
            handle.send(signal.SIGKILL)
            handle.exited(seconds=3)
        if accepted is not None:
            accepted.close()
        listener.close()
        if path.exists():
            path.unlink()
    evidence['cases']['stalled-startup'] = receipt
    lifecycle.start()


def slow_reader(lifecycle, evidence):
    native = lifecycle.native
    native.command('SendToModule FvwmNotify DismissAll')
    wait_for_predicate('empty before slow-reader case',
        lambda: native.status()['active'] == 0,
        time.monotonic() + 3, process=native.process)
    slow = native.peer('nonreading-producer')
    unique = slow.request('identity')['unique']
    before = lifecycle.memory()
    bursts, drains = [], []
    for _ in range(4):
        result = slow.request('burst', count=256)
        assert result.get('queued') == 256 and not result.get('replies_popped'), result
        assert result['outgoing_bytes'] == 0, result
        bursts.append(result)
        drained = slow.request('drain', count=256)
        assert 'error' not in drained, drained
        assert drained['notification_replies_drained'] == 256, drained
        assert drained['remaining'] == 0, drained
        drains.append(drained)
    assert sum(item['method_returns'] for item in drains) == 32, drains
    assert sum(item['method_errors'] for item in drains) == 992, drains
    # Every burst's Notify replies have now been explicitly drained, keeping
    # the peer's unchanged receive cap from obstructing this protocol barrier.
    # The barrier starts a separate nonreading interval for closure signals;
    # it also prevents DismissAll racing this producer's pending admissions.
    barrier = slow.request('capabilities')
    assert set(barrier['capabilities']) == {'body', 'actions'}, barrier
    wait_for_predicate('bounded full active table',
        lambda: native.status()['active'] == 32,
        time.monotonic() + 5, process=native.process)
    # From here until explicit peer close the producer waits only on stdin; it
    # performs no D-Bus read/pop call while closure signals accumulate for it.
    assert set(native.first.request('capabilities')['capabilities']) == {'body', 'actions'}
    native.command('SendToModule FvwmNotify DismissAll')
    wait_for_predicate('slow-reader records dismissed without reading peer',
        lambda: native.status()['active'] == 0,
        time.monotonic() + 5, process=native.process)
    identifier = native.first.notice(summary='Another peer still makes progress')
    native.popup(identifier)
    assert 'error' not in native.first.request('close', id=identifier)
    native.first.signal('closed', identifier, reason=3)
    after = lifecycle.memory()
    assert after['KiB']['Rss'] - before['KiB']['Rss'] < 8192, (before, after)
    assert lifecycle.module.alive()
    slow.close()
    assert slow.process.returncode == 0, slow.process.returncode
    native.peers.remove(slow)
    native.control.echo('core-live-after-nonreading-peer')
    evidence['cases']['slow-reader'] = {'unique': unique, 'bursts': bursts,
        'drains': drains,
        'queued_calls': 1024, 'active_cap_observed': 32,
        'other_peer_capabilities_notify_close': 'passed', 'core_echo': 'passed',
        'explicit_notification_reply_pops': 1024, 'sender_barrier': barrier,
        'burst_reply_phase': 'each fixed burst drained before the next burst',
        'nonreading_closure_interval': 'no D-Bus calls after barrier until close',
        'peer_exit': slow.process.returncode,
        'before': before, 'after': after,
        'limit': 'Finite workload and module RSS only; no external broker memory bound.'}


def churn(lifecycle, evidence):
    native = lifecycle.native
    records = []
    for index in range(4):
        peer = native.peer('churn-' + str(index))
        unique = peer.request('identity')['unique']
        identifier = peer.notice(summary='Departed producer ' + str(index),
            actions=['departed', 'Unavailable application'])
        assert peer.notice(replaces=identifier,
            summary='Replaced before disconnect ' + str(index),
            actions=['departed', 'Unavailable application']) == identifier
        peer.close()
        assert peer.process.returncode == 0
        native.peers.remove(peer)
        records.append({'unique': unique, 'id': identifier, 'peer_exit': 0})
    fresh = native.peer('reconnected-producer')
    fresh_unique = fresh.request('identity')['unique']
    assert fresh_unique not in {record['unique'] for record in records}
    assert native.status()['active'] == len(records)
    native.command('SendToModule FvwmNotify Focus')
    center = wait_for_predicate('departed-producer action center',
        lambda: native.named('FVWM-X notifications'), time.monotonic() + 3,
        process=native.process)
    wait_for_predicate('departed-producer explicit center focus',
        lambda: native.focus() == center, time.monotonic() + 3,
        process=native.process)
    assert native.property(center) == [records[0]['id']]
    native.press('Return')
    wait_for_predicate('departed callback retires only selected record',
        lambda: native.status()['active'] == len(records) - 1,
        time.monotonic() + 3, process=native.process)
    assert not fresh.request('signals')['signals'], 'callback rebound to a new connection'
    for record in records:
        result = native.first.request('close', id=record['id'])
        if record is records[0]:
            assert 'error' in result, result
        else:
            assert 'error' not in result, result
        assert 'error' in native.first.request('close', id=record['id'])
    wait_for_predicate('churn records fully retired',
        lambda: native.status()['active'] == 0,
        time.monotonic() + 3, process=native.process)
    native.press('Escape')
    identifier = fresh.notice(summary='A live producer retains its own callback',
        actions=['live', 'Live action'])
    native.popup(identifier)
    native.command('SendToModule FvwmNotify Focus')
    center = wait_for_predicate('live-producer action center',
        lambda: native.named('FVWM-X notifications'), time.monotonic() + 3,
        process=native.process)
    wait_for_predicate('live-producer center focus', lambda: native.focus() == center,
        time.monotonic() + 3, process=native.process)
    native.press('Return')
    fresh.signal('action', identifier, key='live')
    fresh.signal('closed', identifier, reason=2)
    native.press('Escape')
    evidence['cases']['churn'] = {'departed': records, 'fresh_unique': fresh_unique,
        'absent_owner_action_retired': records[0]['id'],
        'reconnected_peer_received_stale_callback': False,
        'live_action_id': identifier, 'live_action': 'passed'}


def teardown(lifecycle, evidence):
    native = lifecycle.native
    old_owner = native.first.request('owner')['owner']
    identifiers = [native.first.notice(summary='Pending teardown ' + str(index),
        actions=['old', 'Old action']) for index in range(3)]
    native.command('SendToModule FvwmNotify Focus')
    center = wait_for_predicate('center before owned teardown',
        lambda: native.named('FVWM-X notifications'), time.monotonic() + 3,
        process=native.process)
    retired = lifecycle.stop()
    owner = lifecycle.start()
    assert owner != old_owner
    state = native.status()
    assert state['active'] == state['history'] == 0, state
    fresh = native.peer('post-restart-producer')
    assert not fresh.request('signals')['signals']
    new_id = fresh.notice(summary='Fresh server owner has fresh state')
    native.popup(new_id)
    for old in identifiers:
        if old != new_id:
            assert 'error' in fresh.request('close', id=old)
    assert 'error' not in fresh.request('close', id=new_id)
    fresh.signal('closed', new_id, reason=3)
    evidence['cases']['teardown'] = {'old_owner': old_owner, 'new_owner': owner,
        'old_ids': identifiers, 'old_center': center, 'retired': retired,
        'new_id': new_id, 'empty_restart_state': state,
        'numeric_id_reuse_is_scoped_to_new_server_owner': new_id in identifiers,
        'new_connection_stale_signals': False}


def idle_term(lifecycle, evidence):
    native = lifecycle.native
    native.command('SendToModule FvwmNotify DismissAll')
    wait_for_predicate('idle before TERM', lambda: native.status()['active'] == 0,
        time.monotonic() + 3, process=native.process)
    started = time.monotonic()
    retired = lifecycle.stop()
    elapsed = time.monotonic() - started
    assert elapsed < 3, elapsed
    evidence['cases']['idle-term'] = {'retired': retired, 'seconds': elapsed,
        'core_echo': 'passed', 'owner_absent': True}


def run(options):
    assert platform.system() == 'Linux' and hasattr(os, 'pidfd_open')
    assert hasattr(signal, 'pidfd_send_signal')
    out = options.artifacts_dir.resolve()
    out.mkdir(parents=True, exist_ok=True)
    runtime = HarnessRuntime(prefix='fvwm-notify-lifecycle-')
    native = lifecycle = None
    evidence = {'status': 'running', 'cases': {}, 'limits': [
        'Linux nested X/private bus only, not complete platform or physical acceptance.',
        'pidfd retirement is process-exit observation, not a claim of parent waitpid.',
        'A requested startup termination timer cannot preempt uninterruptible kernel work.',
        'Finite nonreading-peer workload does not bound external broker buffers.']}
    try:
        if options.display is None:
            _, options.display = start_display_server(runtime, 'Xvfb',
                ['Xvfb', '-screen', '0', '1440x1000x24', '-noreset'],
                'notify-lifecycle-xserver.log', time.monotonic() + 10)
        native = Native(runtime, options, out)
        lifecycle = Lifecycle(native)
        evidence['inputs'] = {name: {'path': str(getattr(options, name)),
            'sha256': digest(getattr(options, name))} for name in
            ('fvwm', 'notify', 'client', 'mfl')}
        evidence['core'] = identity(native.process.process.pid)
        evidence['initial_module'] = lifecycle.module.initial
        functions = {'missing-bus': missing_bus, 'stalled-startup': stalled_startup,
            'slow-reader': slow_reader, 'churn': churn, 'teardown': teardown,
            'idle-term': idle_term}
        for case in options.cases:
            functions[case](lifecycle, evidence)
        evidence['status'] = 'passed'
    except BaseException as failure:
        evidence.update(status='failed', error=str(failure), traceback=traceback.format_exc())
        raise
    finally:
        cleanup_errors = []
        try:
            if lifecycle:
                evidence['exact_handle_cleanup'] = lifecycle.close()
        except BaseException:
            cleanup_errors.append(traceback.format_exc())
        try:
            if native:
                for index, peer in enumerate(native.peers):
                    (out / ('wire-%02d.json' % index)).write_text(
                        json.dumps(peer.records, indent=2) + '\n')
                native.close()
                if native.errors.unexpected:
                    evidence.update(status='failed', cleanup_x_errors=native.errors.unexpected)
        except BaseException:
            cleanup_errors.append(traceback.format_exc())
        try:
            for process in runtime.processes:
                if process.log_path.exists():
                    shutil.copyfile(process.log_path, out / process.log_path.name)
        except BaseException:
            cleanup_errors.append(traceback.format_exc())
        try:
            runtime.close()
        except BaseException:
            cleanup_errors.append(traceback.format_exc())
        if cleanup_errors:
            evidence.update(status='failed', cleanup_errors=cleanup_errors)
        (out / 'result.json').write_text(json.dumps(evidence, indent=2) + '\n')
    assert evidence['status'] == 'passed', evidence


def main():
    parser = argparse.ArgumentParser(allow_abbrev=False)
    parser.add_argument('--build', type=Path, required=True)
    for name in ('fvwm', 'notify', 'client', 'mfl', 'capture', 'desktop'):
        parser.add_argument('--' + name, type=Path)
    parser.add_argument('--display')
    parser.add_argument('--cases', nargs='+', choices=CASES, default=list(CASES))
    parser.add_argument('--artifacts-dir', type=Path, required=True)
    options = parser.parse_args()
    defaults = {'fvwm': options.build / 'fvwm3',
        'notify': options.build / 'modules/FvwmNotify/FvwmNotify',
        'client': options.build / 'tests/notify-client',
        'mfl': options.build / 'modules/FvwmMFL/FvwmMFL',
        'capture': options.build / 'bin/fvwm-desktop-capture'}
    for name, value in defaults.items():
        setattr(options, name, (getattr(options, name) or value).resolve())
        assert getattr(options, name).is_file(), getattr(options, name)
    run(options)


if __name__ == '__main__':
    main()
