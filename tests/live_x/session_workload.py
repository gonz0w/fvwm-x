#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or any later version.
"""Finite natural-allocation activity for the installed session observation."""
from contextlib import contextmanager
import math
from pathlib import Path
import time

from mfl_request_live import frame, reply


ADDITIONAL_APPLICATIONS = 512
DIRECTORY_ENTRIES = 96
ROUNDS = 8
# The existing worker caps discovery at 512 even with 513 fixture input files.
DISCOVERED_APPLICATIONS = 512
OPERATION_LIMIT = 16
DEADLINE_SECONDS = 10
MAX_ATTEMPTS = 500
POLL_SECONDS = .02
SEARCH_COMMAND = 'SendToModule FvwmDesktop search Workload application'
FILES_COMMAND = 'SendToModule FvwmDesktop files'


def prepare_workload(applications: Path, home: Path):
    """Add only deterministic fixtures beneath the caller's disposable roots."""
    if not applications.is_dir() or not home.is_dir():
        raise NotADirectoryError('Workload requires prepared fixture roots')
    app_paths = [applications / ('workload-%03d.desktop' % index)
                 for index in range(ADDITIONAL_APPLICATIONS)]
    directory_paths = [home / ('workload-directory-%03d' % index)
                       for index in range(DIRECTORY_ENTRIES)]
    # Preflight collisions so a reused root is never silently overwritten.
    for path in app_paths + directory_paths:
        if path.exists() or path.is_symlink():
            raise FileExistsError(path)
    for index, path in enumerate(app_paths):
        with path.open('x', encoding='utf-8') as stream:
            stream.write('[Desktop Entry]\nType=Application\n'
                'Name=Workload application %03d\nExec=/bin/true\n' % index)
    # Direct children make the supported HOME files command visit this load.
    for path in directory_paths:
        path.mkdir(mode=0o700)
    return dict(additional_applications=ADDITIONAL_APPLICATIONS,
                application_desktop_files=ADDITIONAL_APPLICATIONS + 1,
                expected_discovered_applications=DISCOVERED_APPLICATIONS,
                private_directory_entries=DIRECTORY_ENTRIES, rounds=ROUNDS,
                operation_limit=OPERATION_LIMIT,
                deadline_seconds=DEADLINE_SECONDS, max_attempts=MAX_ATTEMPTS,
                directory_prefix='workload-directory-',
                search_command=SEARCH_COMMAND, files_command=FILES_COMMAND)


def _identity(value):
    assert (isinstance(value, str) and len(value) == 32 and
            all(character in '0123456789abcdef' for character in value)), value
    return value


@contextmanager
def _reply_deadline(connection, deadline):
    remaining = deadline - time.monotonic()
    if remaining <= 0:
        raise TimeoutError('Workload reply deadline')
    previous = connection.socket.gettimeout()
    connection.socket.settimeout(min(previous, remaining)
                                 if previous is not None else remaining)
    try:
        yield
    finally:
        connection.socket.settimeout(previous)


def _operation(result, provider, operation_id=None):
    assert result['owner'] == 'FvwmDesktop', result
    assert result['provider'] == provider, result
    operation = result['operation']
    identity = _identity(operation['id'])
    if operation_id is not None:
        assert identity == operation_id, result
    # Operation generations are independent of the preferences generation and
    # can change between snapshots. Correlate the stable provider and ID.
    _identity(operation['generation'])
    assert operation['target'] == 'applications', result
    assert operation['cancel_requested'] is False, result
    assert operation['state'] in ('accepted', 'running', 'completed',
                                   'failed', 'cancelled'), result
    if operation['state'] in ('failed', 'cancelled'):
        raise AssertionError('Workload operation terminated: %r' % operation)
    if operation['state'] == 'completed':
        assert operation['application'] == 'submitted-to-fvwm', operation
        assert operation['error'] is None, operation
        entries = operation['result']['entries']
        assert (type(entries) is int and
                entries == DISCOVERED_APPLICATIONS), operation
    return operation


def run_workload(connection, process, show_menu_callback,
                 input_sample_callback):
    """Run eight rounds and return JSON receipts.

    The menu callback receives (command, label) and must observe and dismiss an
    actual menu grab. The input callback receives the last operation snapshot
    and returns milliseconds; a completed snapshot makes no overlap claim.
    Unexpected replies, transport errors and callback failures propagate.
    """
    serial = 0
    def send(method='query', resource='operations', **fields):
        nonlocal serial
        assert process.poll() is None, 'Session exited during fixed workload'
        serial += 1
        token = 'session-workload-%d' % serial
        connection.send(frame(token, method, resource=resource, **fields))
        return token

    def check_deadline(deadline, label):
        if time.monotonic() >= deadline:
            raise TimeoutError('Workload deadline: ' + label)

    token = send(resource='provider')
    provider = reply(connection, token)
    assert provider['owner'] == 'FvwmDesktop', provider
    assert provider['lifecycle'] == 'ready', provider
    identity = _identity(provider['provider'])
    generation = _identity(provider['generation'])
    assert provider['operation_limit'] == OPERATION_LIMIT, provider
    assert 'applications' in provider['operation_targets'], provider
    rounds, seen_ids = [], set()
    for index in range(ROUNDS):
        deadline = time.monotonic() + DEADLINE_SECONDS
        for attempt in range(1, MAX_ATTEMPTS + 1):
            check_deadline(deadline, 'applications admission')
            token = send('action', operation='start', target='applications',
                         provider=identity, generation=generation)
            # Only an explicit startup busy refusal is retryable. All other
            # errors and malformed request/reply correlations fail immediately.
            with _reply_deadline(connection, deadline):
                envelope = connection.record()
            assert set(envelope) == {'reply'}, envelope
            value = envelope['reply']
            assert value['version'] == 1 and value['id'] == token, value
            check_deadline(deadline, 'applications admission')
            if value['status'] == 'ok':
                operation = _operation(value['result'], identity)
                assert operation['state'] == 'accepted', operation
                break
            assert value['status'] == 'error', value
            assert (index == 0 and
                    value['error']['code'] == 'operation_busy'), value
            time.sleep(POLL_SECONDS)
        else:
            raise TimeoutError('Workload applications admission attempt limit')
        operation_id = operation['id']
        assert operation_id not in seen_ids, operation
        seen_ids.add(operation_id)
        assert len(seen_ids) <= OPERATION_LIMIT
        item = dict(round=index + 1, start_attempts=attempt,
                    states_observed=['accepted'])
        deadline = time.monotonic() + DEADLINE_SECONDS
        for poll in range(1, MAX_ATTEMPTS + 1):
            check_deadline(deadline, 'applications completion')
            token = send(provider=identity, target=operation_id)
            with _reply_deadline(connection, deadline):
                result = reply(connection, token)
            operation = _operation(result, identity, operation_id)
            check_deadline(deadline, 'applications completion')
            state = operation['state']
            if item['states_observed'][-1] != state:
                assert not (state == 'accepted' and 'running' in
                            item['states_observed']), operation
                item['states_observed'].append(state)
            if poll == 1:
                milliseconds = input_sample_callback(dict(operation))
                assert (type(milliseconds) in (int, float) and
                        math.isfinite(milliseconds) and
                        milliseconds >= 0), milliseconds
                item['input'] = dict(operation_state_before_input=state,
                    operation_id=operation_id,
                    operation_generation=operation['generation'],
                    milliseconds=milliseconds)
            if state == 'completed':
                item['completed'] = operation
                item['polls'] = poll
                break
            time.sleep(POLL_SECONDS)
        else:
            raise TimeoutError('Workload applications completion attempt limit')
        label = 'workload-%02d-' % (index + 1)
        item['search_menu'] = show_menu_callback(SEARCH_COMMAND,
                                               label + 'search')
        item['files_menu'] = show_menu_callback(FILES_COMMAND, label + 'files')
        rounds.append(item)
    return dict(provider=identity, preferences_generation=generation,
                operation_limit=OPERATION_LIMIT, rounds=rounds)
