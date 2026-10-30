#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License, version 2 or later.
"""Opt-in Linux notification cost observations on an owned X/private bus.

This lead-run fixture records observations, not a performance budget or a
passing platform result. Reuse the real notification wire peer and retain exact
pidfds for the module, its private broker and the core. Keep their memory values
separate: RSS includes shared mappings and must not be added across processes.
The startup sample is first-ready, not a continuously observed startup peak.
"""
import argparse
from collections import Counter
import hashlib
import json
import os
from pathlib import Path
import platform
import shutil
import signal
import sys
import time
import traceback

from live_x_harness import HarnessRuntime, start_display_server, wait_for_predicate
from notify_lifecycle_live import Lifecycle, OwnedProcess
from notify_live import Native, digest

FIELDS = {'app': 128, 'summary': 256, 'body': 2048,
    'action_key': 64, 'action_label': 128}
ACTIVE_COUNT = 32
HISTORY_COUNT = 16
IDLE_SECONDS = 1.0
SETTLE_SECONDS = .25


def file_identity(path):
    path = Path(path).resolve(strict=True)
    information = path.stat()
    return {'path': str(path), 'sha256': digest(path),
        'bytes': information.st_size, 'device': information.st_dev,
        'inode': information.st_ino}


def source_inputs(options):
    source = Path(__file__).resolve().parents[2]
    names = ['tests/live_x/notify_cost_live.py', 'tests/live_x/notify_live.py',
        'tests/live_x/notify_lifecycle_live.py',
        'tests/live_x/session_bus_lifetime_live.py',
        'tests/live_x/session_recovery_live.py', 'tests/live_x/live_x_harness.py',
        'tests/notify_client.c', 'modules/FvwmNotify/notify.c',
        'modules/FvwmNotify/notify_bus.c', 'modules/FvwmNotify/notify_model.c',
        'modules/FvwmNotify/notify_model.h',
        'modules/FvwmNotify/notify_startup.c',
        'modules/FvwmNotify/notify_startup.h']
    return {'source_revision': options.source_revision,
        'revision_authority': 'caller declaration; file hashes observed here',
        'sources': {name: file_identity(source / name) for name in names},
        'binaries': {name: file_identity(getattr(options, name))
            for name in ('fvwm', 'notify', 'mfl', 'client')},
        'python': dict(file_identity(sys.executable), version=sys.version)}


def process_sample(handle, origin):
    started = time.monotonic()
    current = handle.sample()
    directory = Path('/proc/%d' % current['pid'])
    fields = (directory / 'stat').read_text().rsplit(') ', 1)[1].split()
    assert fields[19] == current['birth'], current
    memory = {}
    for line in (directory / 'smaps_rollup').read_text().splitlines():
        key, separator, text = line.partition(':')
        if separator and key in ('Rss', 'Pss', 'Private_Clean',
                'Private_Dirty', 'Private_Hugetlb'):
            words = text.split()
            assert len(words) == 2 and words[1] == 'kB', line
            memory[key] = int(words[0])
    assert all(key in memory for key in ('Rss', 'Pss', 'Private_Clean',
        'Private_Dirty')), memory
    high_water = None
    for line in (directory / 'status').read_text().splitlines():
        if line.startswith('VmHWM:'):
            words = line.split()
            assert len(words) == 3 and words[2] == 'kB', line
            high_water = int(words[1])
    assert high_water is not None, 'kernel VmHWM unavailable'
    mappings = set()
    for line in (directory / 'maps').read_text().splitlines():
        parts = line.split(None, 5)
        if len(parts) == 6 and parts[5].startswith('/'):
            mappings.add(parts[5])
    # Revalidate birth/executable after the sequential /proc reads. These are
    # observations over a recorded interval, not an atomic process snapshot.
    assert handle.sample()['birth'] == current['birth'], current
    return {'identity': current, 'seconds_from_ready': started - origin,
        'sample_seconds': time.monotonic() - started,
        'cpu_ticks': {'user': int(fields[11]), 'system': int(fields[12])},
        'memory_KiB': memory, 'observed_kernel_VmHWM_KiB': high_water,
        'private_KiB': sum(memory.get(key, 0) for key in
            ('Private_Clean', 'Private_Dirty', 'Private_Hugetlb')),
        'loaded_file_mapping_paths': sorted(mappings)}


def snapshot(evidence, name, handles, origin, state):
    sample = {'phase': name, 'state_before_sample': state,
        'processes': {name: process_sample(handle, origin)
            for name, handle in handles.items()}}
    evidence['samples'].append(sample)
    return sample


def idle_interval(evidence, name, before, handles, origin, state):
    started = time.monotonic()
    # Deliberately no X, MFL or D-Bus polling in this interval. Read /proc only
    # in the endpoint snapshots, and do not assert a CPU timing threshold.
    time.sleep(IDLE_SECONDS)
    ended = time.monotonic()
    after = snapshot(evidence, name, handles, origin, state)
    deltas = {}
    for process in handles:
        left = before['processes'][process]
        right = after['processes'][process]
        assert left['identity']['birth'] == right['identity']['birth']
        delta = {key: right['cpu_ticks'][key] - left['cpu_ticks'][key]
            for key in ('user', 'system')}
        assert all(value >= 0 for value in delta.values()), delta
        deltas[process] = {'cpu_ticks': delta,
            'endpoint_sample_start_interval_seconds':
                right['seconds_from_ready'] - left['seconds_from_ready']}
    evidence['idle_intervals'].append({'phase': name,
        'requested_sleep_seconds': IDLE_SECONDS,
        'observed_sleep_seconds': ended - started,
        'start_seconds_from_ready': started - origin,
        'end_seconds_from_ready': ended - origin,
        'protocol_or_X_polling_during_sleep': False,
        'processes': deltas})
    return after


def payload(index):
    actions = []
    for number in range(4):
        actions.extend([str(number) + 'k' * (FIELDS['action_key'] - 1),
            str(number) + 'L' * (FIELDS['action_label'] - 1)])
    prefix = 'Notice %02d ' % index
    return {'app': 'A' * FIELDS['app'],
        'summary': prefix + 'S' * (FIELDS['summary'] - len(prefix)),
        'body': 'B' * FIELDS['body'], 'actions': actions,
        'timeout': 0, 'urgency': 1}


def state(native, active, history, limit):
    def expected():
        result = native.status()
        return result if (result['active'], result['history'],
            result['history_limit'], result['pending']) == (
                active, history, limit, 0) else None
    return wait_for_predicate('cost workload state %d/%d/%d' %
        (active, history, limit), expected, time.monotonic() + 5,
        process=native.process)


def observe(native, lifecycle, handles, evidence):
    origin = time.monotonic()
    first = snapshot(evidence, 'first-ready', handles, origin,
        state(native, 0, 0, HISTORY_COUNT))
    idle_interval(evidence, 'settled-idle', first, handles, origin,
        first['state_before_sample'])
    identifiers = []
    requests = [payload(index) for index in range(ACTIVE_COUNT)]
    encoded = json.dumps(requests, ensure_ascii=False, sort_keys=True).encode()
    evidence['workload'] = {'active_count': ACTIVE_COUNT,
        'admitted_text_field_bytes': FIELDS, 'actions_per_record': 4,
        'history_count_after_dismiss': HISTORY_COUNT,
        'payload_encoding': 'ASCII, hence UTF-8 bytes equal character counts',
        'payloads_json_sha256': hashlib.sha256(encoded).hexdigest(),
        'payloads_json_bytes': len(encoded), 'payloads': requests,
        'target_wire_calls': {'notify': ACTIVE_COUNT + 1, 'close': 1},
        'dismiss_all_commands': 1, 'history_limit_changes': [0],
        'automatic_expiration': False,
        'owner_field': 'bus-assigned unique name; not a caller-sized field'}
    for request in requests:
        identifiers.append(native.first.notice(**request))
    assert len(set(identifiers)) == ACTIVE_COUNT, identifiers
    current = state(native, ACTIVE_COUNT, 0, HISTORY_COUNT)
    native.popup(identifiers[0])
    time.sleep(SETTLE_SECONDS)
    evidence['workload']['admitted_ids'] = identifiers
    snapshot(evidence, '32-maximal-records-four-actions', handles, origin, current)
    native.command('SendToModule FvwmNotify DismissAll')
    current = state(native, 0, HISTORY_COUNT, HISTORY_COUNT)
    wait_for_predicate('cost dismissal retires all popups',
        lambda: not native.popups(), time.monotonic() + 5,
        process=native.process)
    time.sleep(SETTLE_SECONDS)
    snapshot(evidence, '16-history-after-dismissal', handles, origin, current)
    native.command('*FvwmNotify History 0\nSendToModule FvwmNotify Reload')
    state(native, 0, 0, 0)
    recovered = native.first.notice(summary='Cost observation recovery')
    native.popup(recovered)
    assert 'error' not in native.first.request('close', id=recovered)
    native.first.signal('closed', recovered, reason=3)
    current = state(native, 0, 0, 0)
    native.absent(recovered)
    time.sleep(SETTLE_SECONDS)
    recovered_sample = snapshot(evidence, 'post-clear-and-recovery', handles,
        origin, current)
    idle_interval(evidence, 'post-recovery-idle', recovered_sample, handles,
        origin, current)
    evidence['workload']['recovered_id'] = recovered
    evidence['observation_seconds_excluding_startup'] = time.monotonic() - origin
    assert lifecycle.module.alive()


def run(options):
    assert platform.system() == 'Linux' and hasattr(os, 'pidfd_open')
    assert hasattr(signal, 'pidfd_send_signal')
    out = options.artifacts_dir.resolve()
    out.mkdir(parents=True, exist_ok=True)
    evidence = {'status': 'running', 'samples': [], 'idle_intervals': [],
        'success_means': 'workload/oracles completed; no cost budget asserted',
        'inputs': source_inputs(options), 'clock_ticks_per_second':
            os.sysconf('SC_CLK_TCK'),
        'executor': {'uname': list(os.uname()),
            'execution_profile': options.execution_profile,
            'cpu_execution': 'caller declaration; not inferred from uname'},
        'limits': [
            'First-ready startup sample is not continuous sampling or a startup peak.',
            'VmHWM is the observed kernel field, not an independently measured peak.',
            'Sequential /proc samples are not simultaneous or atomic.',
            'RSS/PSS/private values stay per process; no cross-process RSS total.',
            'Private KiB sums Private_Clean, Private_Dirty and Private_Hugetlb only.',
            'Owned private broker contribution is not a whole-desktop budget.',
            'Loaded file mappings are observed paths, not a complete package/license inventory.',
            'Finite workload/idle ticks do not establish an absolute performance guarantee.',
            'Linux /proc/pidfd observation does not qualify BSD or physical/native CPU execution.']}
    runtime = HarnessRuntime(prefix='fvwm-notify-cost-')
    native = lifecycle = None
    observers = []
    started = time.monotonic()
    try:
        options.display_origin = ('caller-owned' if options.display is not None
            else 'fixture-owned-Xvfb')
        if options.display is None:
            server, options.display = start_display_server(runtime, 'Xvfb',
                ['Xvfb', '-screen', '0', '1440x1000x24', '-noreset'],
                'notify-cost-xserver.log', time.monotonic() + 10)
            evidence['inputs']['Xvfb'] = file_identity(
                Path('/proc/%d/exe' % server.process.pid))
        evidence['display'] = {'name': options.display,
            'ownership': options.display_origin}
        native = Native(runtime, options, out)
        evidence['native_ready_seconds_including_fixture_startup'] = (
            time.monotonic() - started)
        lifecycle = Lifecycle(native)
        core = OwnedProcess(native.process.process.pid, os.getpid(), options.fvwm)
        observers.append(core)
        broker_path = Path('/proc/%d/exe' % native.bus.process.pid).resolve(strict=True)
        broker = OwnedProcess(native.bus.process.pid, os.getpid(), broker_path)
        observers.append(broker)
        evidence['inputs']['private_broker'] = file_identity(broker_path)
        evidence['inputs']['configuration'] = file_identity(native.config)
        handles = {'module': lifecycle.module, 'private_broker': broker, 'core': core}
        evidence['scope'] = {'processes': {name: {
                'identity': handle.initial, 'executable': handle.executable,
                'arguments': handle.arguments} for name, handle in handles.items()},
            'private_bus_address': native.address,
            'independent_wire_peer_count': len(native.peers),
            'excluded': ['X server', 'wire peers', 'MFL', 'fixture Python',
                'other desktop/session processes']}
        observe(native, lifecycle, handles, evidence)
        evidence['module_retirement'] = lifecycle.stop()
        evidence['status'] = 'passed'
    except BaseException as failure:
        evidence.update(status='failed', error=str(failure),
            traceback=traceback.format_exc())
        raise
    finally:
        errors = []
        try:
            if lifecycle:
                evidence['exact_handle_cleanup'] = lifecycle.close()
        except BaseException:
            errors.append(traceback.format_exc())
        for observer in observers:
            try:
                observer.close()
            except BaseException:
                errors.append(traceback.format_exc())
        try:
            if native:
                evidence['actual_client_verbs'] = []
                for index, peer in enumerate(native.peers):
                    evidence['actual_client_verbs'].append(dict(Counter(
                        item['request']['verb'] for item in peer.records)))
                    (out / ('wire-%02d.json' % index)).write_text(
                        json.dumps(peer.records, indent=2) + '\n')
                native.close()
                if native.errors.unexpected:
                    evidence.update(status='failed',
                        cleanup_x_errors=native.errors.unexpected)
        except BaseException:
            errors.append(traceback.format_exc())
        try:
            for process in runtime.processes:
                if process.log_path.exists():
                    shutil.copyfile(process.log_path, out / process.log_path.name)
        except BaseException:
            errors.append(traceback.format_exc())
        try:
            runtime.close()
        except BaseException:
            errors.append(traceback.format_exc())
        if errors:
            evidence.update(status='failed', cleanup_errors=errors)
        (out / 'result.json').write_text(json.dumps(evidence, indent=2) + '\n')
    assert evidence['status'] == 'passed', evidence


def main():
    parser = argparse.ArgumentParser(allow_abbrev=False)
    parser.add_argument('--build', type=Path, required=True)
    for name in ('fvwm', 'notify', 'client', 'mfl', 'capture'):
        parser.add_argument('--' + name, type=Path)
    parser.add_argument('--display')
    parser.add_argument('--execution-profile', default='unspecified',
        choices=('unspecified', 'native-cpu', 'emulated-cpu'))
    parser.add_argument('--source-revision', default='unspecified')
    parser.add_argument('--artifacts-dir', type=Path, required=True)
    options = parser.parse_args()
    options.desktop = None
    defaults = {'fvwm': options.build / 'fvwm3',
        'notify': options.build / 'modules/FvwmNotify/FvwmNotify',
        'client': options.build / 'tests/notify-client',
        'mfl': options.build / 'modules/FvwmMFL/FvwmMFL',
        'capture': options.build / 'bin/fvwm-desktop-capture'}
    for name, value in defaults.items():
        setattr(options, name, (getattr(options, name) or value).resolve())
        if name != 'capture':
            assert getattr(options, name).is_file(), getattr(options, name)
    run(options)


if __name__ == '__main__':
    main()
