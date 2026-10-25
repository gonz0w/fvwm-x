#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or any later version.
"""Fixed natural session activity and asynchronous receipts, without X."""
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parent / 'live_x'))
import session_workload as workload


PROVIDER = 'a' * 32
GENERATION = 'b' * 32


class Connection:
    """Respond to real request frames; never replace the helper's RPC code."""
    def __init__(self, states=('running', 'completed'), busy=0):
        self.requests = []
        self.states = states
        self.busy = busy
        self.started = 0
        self.queried = 0
        self.change = lambda value: value
        self.socket = Socket()

    def send(self, data):
        self.requests.append(json.loads(data.removeprefix(b'request ')))

    def record(self):
        request = self.requests[-1]
        if request['resource'] == 'provider':
            result = dict(owner='FvwmDesktop', provider=PROVIDER,
                          generation=GENERATION, lifecycle='ready',
                          operation_targets=['applications', 'status'],
                          operation_limit=16)
        elif request['method'] == 'action':
            if self.busy:
                self.busy -= 1
                return dict(reply=dict(version=1, id=request['id'],
                    status='error', error=dict(code='operation_busy')))
            self.started += 1
            self.queried = 0
            result = self.operation('accepted')
        else:
            state = self.states[min(self.queried, len(self.states) - 1)]
            self.queried += 1
            result = self.operation(state)
        return self.change(dict(reply=dict(version=1, id=request['id'],
                                           status='ok', result=result)))

    def operation(self, state):
        return dict(owner='FvwmDesktop', provider=PROVIDER, operation=dict(
            id='%032x' % self.started, generation='c' * 32,
            target='applications', state=state, cancel_requested=False,
            application=('submitted-to-fvwm' if state == 'completed'
                         else 'not-submitted'),
            result=dict(entries=512) if state == 'completed' else None,
            error=None))


class Process:
    def poll(self):
        return None


class Socket:
    def __init__(self):
        self.timeout = 5

    def gettimeout(self):
        return self.timeout

    def settimeout(self, value):
        self.timeout = value


class SessionWorkloadTests(unittest.TestCase):
    def run_workload(self, connection):
        self.menus, self.inputs = [], []
        def menu(command, label):
            self.menus.append((command, label))
            return dict(grabbed=True, dismissed=True)
        def sample(operation):
            self.inputs.append(dict(operation))
            return 2.5
        with patch.object(workload.time, 'sleep'):
            return workload.run_workload(connection, Process(), menu, sample)

    def test_fixture_has_fixed_bounds_and_preserves_existing_files(self):
        with tempfile.TemporaryDirectory() as temporary:
            home = Path(temporary)
            applications = home / 'applications'
            applications.mkdir()
            original = applications / 'fixture.desktop'
            original.write_text('existing fixture\n')
            manifest = workload.prepare_workload(applications, home)
            self.assertEqual(original.read_text(), 'existing fixture\n')
            self.assertEqual(len(list(applications.iterdir())), 513)
            self.assertEqual(manifest['additional_applications'], 512)
            self.assertEqual(manifest['application_desktop_files'], 513)
            self.assertEqual(manifest['expected_discovered_applications'], 512)
            self.assertEqual(manifest['private_directory_entries'], 96)
            self.assertEqual(manifest['rounds'], 8)
            directories = list(home.glob('workload-directory-*'))
            self.assertEqual(len(directories), 96)
            self.assertTrue(all(item.is_dir() and not list(item.iterdir())
                                for item in directories))
            for index in range(512):
                self.assertEqual((applications /
                    ('workload-%03d.desktop' % index)).read_text(),
                    '[Desktop Entry]\nType=Application\n'
                    'Name=Workload application %03d\nExec=/bin/true\n' % index)

    def test_fixture_refuses_collision_before_creating_anything(self):
        with tempfile.TemporaryDirectory() as temporary:
            home = Path(temporary)
            applications = home / 'applications'
            applications.mkdir()
            collision = home / 'workload-directory-095'
            collision.mkdir()
            with self.assertRaises(FileExistsError):
                workload.prepare_workload(applications, home)
            self.assertEqual(list(applications.iterdir()), [])
            self.assertEqual(sorted(p.name for p in home.iterdir()),
                             ['applications', collision.name])

    def test_eight_rounds_correlate_operations_menus_and_input_receipts(self):
        connection = Connection()
        receipt = self.run_workload(connection)
        self.assertEqual(connection.started, 8)
        self.assertEqual(len(receipt['rounds']), 8)
        self.assertEqual(len(self.menus), 16)
        self.assertEqual(len(self.inputs), 8)
        self.assertLess(connection.started, receipt['operation_limit'])
        for index, item in enumerate(receipt['rounds']):
            self.assertEqual(item['input']['operation_state_before_input'],
                             'running')
            self.assertEqual(item['input']['milliseconds'], 2.5)
            self.assertEqual(item['completed']['result'], dict(entries=512))
            self.assertEqual(item['completed']['id'], '%032x' % (index + 1))
            self.assertEqual(item['states_observed'], ['accepted', 'running',
                                                       'completed'])
        self.assertTrue(all(command.startswith('SendToModule FvwmDesktop ')
                            for command, _ in self.menus))
        self.assertEqual(len({r['id'] for r in connection.requests}),
                         len(connection.requests))
        self.assertEqual(connection.socket.gettimeout(), 5)

    def test_explicit_busy_is_retried_without_admitting_extra_work(self):
        connection = Connection(busy=2)
        receipt = self.run_workload(connection)
        self.assertEqual(connection.started, 8)
        self.assertEqual(receipt['rounds'][0]['start_attempts'], 3)

    def test_completion_before_input_is_recorded_honestly(self):
        receipt = self.run_workload(Connection(states=('completed',)))
        self.assertTrue(all(item['input']['operation_state_before_input'] ==
                            'completed' for item in receipt['rounds']))

    def test_failed_and_cancelled_operations_stop_without_menu_or_retry(self):
        for state in ('failed', 'cancelled'):
            with self.subTest(state=state):
                connection = Connection(states=(state,))
                with self.assertRaisesRegex(AssertionError, state):
                    self.run_workload(connection)
                self.assertEqual(connection.started, 1)
                self.assertEqual(self.menus, [])

    def test_wrong_identity_reply_and_malformed_completion_are_rejected(self):
        for field, value in (('provider', 'd' * 32), ('id', 'e' * 32),
                             ('generation', 'bad'), ('entries', 513),
                             ('entries', True), ('reply_id', 'wrong'),
                             ('application', 'not-submitted')):
            with self.subTest(field=field):
                connection = Connection(states=('completed',))
                def change(envelope):
                    reply = envelope['reply']
                    result = reply['result']
                    if field == 'reply_id':
                        reply['id'] = value
                    elif 'operation' in result:
                        operation = result['operation']
                        if field == 'provider':
                            result[field] = value
                        elif field == 'entries' and operation['result']:
                            operation['result'][field] = value
                        elif field != 'entries' and (
                                field != 'id' or
                                operation['state'] != 'accepted'):
                            operation[field] = value
                    return envelope
                connection.change = change
                with self.assertRaises(AssertionError):
                    self.run_workload(connection)
                self.assertEqual(self.menus, [])

    def test_unexpected_admission_errors_are_never_retried(self):
        connection = Connection()
        original = connection.record
        def record():
            if connection.requests[-1]['method'] == 'action':
                return dict(reply=dict(version=1,
                    id=connection.requests[-1]['id'], status='error',
                    error=dict(code='stale_generation')))
            return original()
        with patch.object(connection, 'record', side_effect=record):
            with self.assertRaisesRegex(AssertionError, 'stale_generation'):
                self.run_workload(connection)
        self.assertEqual(len(connection.requests), 2)

    def test_busy_refusal_at_deadline_does_not_start_another_attempt(self):
        connection = Connection(busy=2)
        with patch.object(workload.time, 'monotonic',
                          side_effect=(0, 0, 0, 10)):
            with self.assertRaises(TimeoutError):
                self.run_workload(connection)
        self.assertEqual(len(connection.requests), 2)
        self.assertEqual(connection.socket.gettimeout(), 5)

    def test_busy_and_pending_attempts_have_frozen_hard_limits(self):
        for connection in (Connection(busy=10000),
                           Connection(states=('running',))):
            with self.subTest(busy=connection.busy):
                with patch.object(workload.time, 'monotonic', return_value=0):
                    with self.assertRaises(TimeoutError):
                        self.run_workload(connection)
                self.assertLessEqual(len(connection.requests),
                                     workload.MAX_ATTEMPTS + 2)

    def test_unexpected_transport_errors_propagate(self):
        connection = Connection()
        with patch.object(connection, 'record', side_effect=OSError('broken')):
            with self.assertRaisesRegex(OSError, 'broken'):
                self.run_workload(connection)


if __name__ == '__main__':
    unittest.main()
