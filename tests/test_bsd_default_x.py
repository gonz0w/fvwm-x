#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or any later version.
"""Verify the default-X driver's ownership and authentication preconditions."""
import struct
import subprocess
import unittest
from unittest.mock import patch

from run_bsd_default_x import authority_record, preflight, query_ready
from mfl_stream_live import Connection


class DefaultXTests(unittest.TestCase):
    def test_readiness_query_timeout_and_status(self):
        with patch('run_bsd_default_x.subprocess.run', side_effect=
                   subprocess.TimeoutExpired('xrandr', 2)) as query:
            self.assertIsNone(query_ready(':197', {'DISPLAY': ':197'}))
            self.assertEqual(query.call_args.kwargs['timeout'], 2)
        for status, expected in [(1, None), (0, 'ready')]:
            with patch('run_bsd_default_x.subprocess.run', return_value=
                       subprocess.CompletedProcess([], status, stdout='ready')):
                self.assertEqual(query_ready(':197', {}), expected)

    def test_authority_wire_record(self):
        cookie = bytes(range(16))
        record = authority_record(197, cookie)
        self.assertEqual(record[:2], struct.pack('!H', 65535))
        fields = []
        offset = 2
        while offset < len(record):
            length, = struct.unpack('!H', record[offset:offset+2])
            offset += 2
            fields.append(record[offset:offset+length])
            offset += length
        self.assertEqual(fields, [b'', b'197', b'MIT-MAGIC-COOKIE-1', cookie])

    def test_listener_readiness_failure_closes_socket(self):
        with patch('mfl_stream_live.socket.socket') as factory:
            factory.return_value.connect.side_effect = ConnectionRefusedError()
            with self.assertRaises(ConnectionRefusedError):
                Connection('/private/unready-test-endpoint')
            factory.return_value.close.assert_called_once()

    def test_wrong_platform_or_missing_vm_ack_never_inventories(self):
        with patch('run_bsd_default_x.subprocess.run') as inventory:
            for system, disposable in [('Linux', True), ('FreeBSD', False),
                                       ('OpenBSD', False)]:
                with self.assertRaises(RuntimeError):
                    preflight(system, disposable, 197)
            inventory.assert_not_called()

    def test_invalid_display_never_inventories(self):
        with patch('run_bsd_default_x.subprocess.run') as inventory:
            for number in (0, -1, 65536):
                with self.assertRaises(RuntimeError):
                    preflight('FreeBSD', True, number)
            inventory.assert_not_called()

    def test_running_server_or_inventory_failure_is_preserved(self):
        for status in (0, 2):
            with patch('run_bsd_default_x.subprocess.run', return_value=
                       subprocess.CompletedProcess([], status)):
                with self.assertRaises(RuntimeError):
                    preflight('FreeBSD', True, 197)

    def test_existing_socket_or_lock_is_preserved(self):
        with patch('run_bsd_default_x.subprocess.run', return_value=
                   subprocess.CompletedProcess([], 1)):
            for occupied in ((True,), (False, True)):
                with patch('run_bsd_default_x.Path.exists',
                           side_effect=occupied):
                    with self.assertRaises(RuntimeError):
                        preflight('OpenBSD', True, 197)

    def test_empty_vm_is_ready(self):
        with patch('run_bsd_default_x.subprocess.run', return_value=
                   subprocess.CompletedProcess([], 1)), patch(
                       'run_bsd_default_x.Path.exists', return_value=False):
            preflight('FreeBSD', True, 197)
            preflight('OpenBSD', True, 197)


if __name__ == '__main__':
    unittest.main()
