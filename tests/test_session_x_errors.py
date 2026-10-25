#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
"""Fake-X contracts for narrowly borrowed session observer window reads."""
import ctypes as C
import gc
import json
from pathlib import Path
import sys
import unittest
import weakref

sys.path.insert(0, str(Path(__file__).resolve().parent / 'live_x'))
from session_x_errors import SessionXErrors, XError


class Function:
    def __init__(self, function):
        self.function = function

    def __call__(self, *args):
        return self.function(*args)


class FakeX:
    def __init__(self):
        self.events = []
        self.pending = []
        self.callback_ref = None
        self.previous = 1234
        self.XSetErrorHandler = Function(self.set_handler)
        self.XSync = Function(self.sync)

    def set_handler(self, handler):
        if callable(handler):
            self.events.append('install')
            # Xlib keeps a function pointer, not a Python ownership reference.
            self.callback_ref = weakref.ref(handler)
        else:
            self.events.append(('restore', handler))
            self.callback_ref = None
        return self.previous

    def error(self, *, display=100, handler_display=100, window=20,
              request=20, code=3):
        error = XError(type=0, display=display, resource=window, serial=7,
                       code=code, request=request, minor=0)
        callback = self.callback_ref()
        self.events.append(('error', code, request, window))
        return callback(handler_display, C.pointer(error))

    def sync(self, display, discard):
        self.events.append('sync')
        pending, self.pending = self.pending, []
        for error in pending:
            self.error(**error)


class FakeClient:
    def __init__(self):
        self.x = FakeX()
        self.display = 100
        self.root = 1
        self.on_close = None

    def close(self):
        self.x.events.append('close')
        if self.on_close:
            self.on_close()


class SessionXErrorTests(unittest.TestCase):
    def setUp(self):
        self.client = FakeClient()
        self.evidence = {}
        self.errors = SessionXErrors(self.client, self.evidence)
        self.errors.install()
        self.addCleanup(self.errors.close_display)

    def test_exact_retired_read_discarded_for_each_supported_request(self):
        for request in (20, 15, 3):
            def operation():
                self.client.x.pending.append({'request': request})
                return 'stale result'
            self.assertIsNone(self.errors.read(20, request, operation))
        self.assertEqual(self.errors.expected_count, 3)
        self.assertEqual(self.errors.unexpected, [])
        self.assertEqual(self.evidence['observer_retired_windows'], 3)

    def test_successful_read_returns_operation_result(self):
        marker = object()
        self.assertIs(self.errors.read(20, 20, lambda: marker), marker)
        self.assertEqual(self.errors.expected_count, 0)

    def test_expected_predicate_excludes_other_errors(self):
        variants = ({'display': 101}, {'handler_display': 101},
                    {'window': 21}, {'window': 1}, {'window': 0},
                    {'request': 15}, {'request': 2}, {'code': 8})
        for event in variants:
            with self.subTest(event=event):
                def operation():
                    self.assertEqual(self.client.x.error(**event), 0)
                    return 'result'
                self.assertEqual(self.errors.read(20, 20, operation), 'result')
        self.assertEqual(len(self.errors.unexpected), len(variants))
        self.assertEqual(self.errors.expected_count, 0)
        json.dumps(self.errors.unexpected, allow_nan=False)
        for record in self.errors.unexpected:
            self.assertTrue(all(isinstance(v, int) for v in record.values()))

    def test_matching_unsupported_request_stays_unexpected(self):
        result = self.errors.read(20, 2,
                                  lambda: self.client.x.error(request=2))
        self.assertEqual(result, 0)
        self.assertEqual(self.errors.expected_count, 0)
        self.assertEqual(len(self.errors.unexpected), 1)

    def test_callback_internal_failure_is_recorded_without_raising(self):
        self.errors._borrowed = {'window': 20, 'request': 20,
                                 'retired': False}
        self.client.display = object()
        self.assertEqual(self.client.x.error(), 0)
        self.assertIn('handler_failure', self.errors.unexpected[-1])
        self.errors._borrowed = None
        self.client.display = 100

    def test_root_borrow_never_expected(self):
        def operation():
            self.client.x.error(window=1)
            return 'root result'
        self.assertEqual(self.errors.read(1, 20, operation), 'root result')
        self.assertEqual(len(self.errors.unexpected), 1)
        self.assertEqual(self.errors.expected_count, 0)

    def test_outside_borrow_is_unexpected_and_callback_returns_zero(self):
        self.assertEqual(self.client.x.error(), 0)
        self.assertEqual(len(self.errors.unexpected), 1)
        self.assertEqual(self.errors.expected_count, 0)

    def test_old_pending_error_is_flushed_before_new_borrow(self):
        self.client.x.pending.append({})
        result = self.errors.read(20, 20, lambda: 'fresh result')
        self.assertEqual(result, 'fresh result')
        self.assertEqual(self.errors.expected_count, 0)
        self.assertEqual(len(self.errors.unexpected), 1)
        self.assertEqual(self.client.x.events[1:3],
                         ['sync', ('error', 3, 20, 20)])

    def test_post_read_sync_precedes_borrow_removal(self):
        self.errors.read(20, 20,
                         lambda: self.client.x.pending.append({}))
        self.assertEqual(self.errors.expected_count, 1)
        self.client.x.error()
        self.assertEqual(len(self.errors.unexpected), 1)

    def test_raising_operation_still_flushes_and_removes_borrow(self):
        def operation():
            self.client.x.pending.append({})
            raise ValueError('fixture operation')
        with self.assertRaisesRegex(ValueError, 'fixture operation'):
            self.errors.read(20, 20, operation)
        self.assertEqual(self.errors.expected_count, 1)
        self.client.x.error()
        self.assertEqual(len(self.errors.unexpected), 1)

    def test_callback_is_retained_through_gc_and_display_close(self):
        gc.collect()
        self.assertIsNotNone(self.client.x.callback_ref())
        self.client.x.pending.append({})
        self.client.on_close = lambda: self.client.x.error(request=3)
        self.errors.close_display()
        self.assertEqual(len(self.errors.unexpected), 2)
        self.assertEqual(self.errors.expected_count, 0)
        self.assertEqual(self.client.x.events[-3:],
                         ['close', ('error', 3, 3, 20), ('restore', 1234)])
        self.errors.close_display()
        self.assertEqual(self.client.x.events.count('close'), 1)

    def test_failed_close_still_restores_previous_handler(self):
        def fail_close():
            self.client.x.error(code=8)
            raise RuntimeError('fixture close')
        self.client.on_close = fail_close
        with self.assertRaisesRegex(RuntimeError, 'fixture close'):
            self.errors.close_display()
        self.assertEqual(self.client.x.events[-1], ('restore', 1234))
        self.assertEqual(len(self.errors.unexpected), 1)

    def test_failed_final_sync_still_closes_before_restoring(self):
        def fail_sync(*args):
            self.client.x.events.append('failed sync')
            raise RuntimeError('fixture sync')
        self.client.x.XSync = Function(fail_sync)
        with self.assertRaisesRegex(RuntimeError, 'fixture sync'):
            self.errors.close_display()
        self.assertEqual(self.client.x.events[-2:],
                         ['close', ('restore', 1234)])


if __name__ == '__main__':
    unittest.main()
