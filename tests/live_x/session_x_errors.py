#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
"""Keep session observer window-retirement races inside owned X cleanup.

Xlib's error handler is process-global. This fixture owns it through display
closure and records unexpected errors for assertion after runtime cleanup. This
handles protocol errors only; XIO/socket loss can still terminate the process.
"""
import ctypes as C


class XError(C.Structure):
    _fields_ = [('type', C.c_int), ('display', C.c_void_p),
                ('resource', C.c_ulong), ('serial', C.c_ulong),
                ('code', C.c_ubyte), ('request', C.c_ubyte),
                ('minor', C.c_ubyte)]


def _scalar(value):
    return int(getattr(value, 'value', value) or 0)


class SessionXErrors:
    def __init__(self, client, evidence):
        self.client = client
        self.evidence = evidence
        self.unexpected = []
        self.expected_count = 0
        self.evidence['observer_unexpected_x_errors'] = self.unexpected
        self.evidence['observer_retired_windows'] = 0
        self._borrowed = None
        self._installed = False
        self._closed = False
        self._previous = None
        self._handler_type = C.CFUNCTYPE(C.c_int, C.c_void_p,
                                        C.POINTER(XError))
        # Xlib retains only a function pointer; own the Python callback until
        # after the display closes and the previous handler is restored.
        self._callback = self._handler_type(self._on_error)

    def _on_error(self, display, error):
        try:
            event = error.contents
            borrowed = self._borrowed
            if (borrowed is not None and
                    _scalar(display) == _scalar(self.client.display) and
                    _scalar(event.display) == _scalar(self.client.display) and
                    event.code == 3 and event.request in (20, 15, 3) and
                    event.request == borrowed['request'] and
                    event.resource == borrowed['window'] and
                    event.resource != _scalar(self.client.root) and
                    event.resource != 0):
                borrowed['retired'] = True
                self.expected_count += 1
                self.evidence['observer_retired_windows'] = self.expected_count
            else:
                self.unexpected.append({
                    'display': _scalar(event.display),
                    'handler_display': _scalar(display),
                    'type': int(event.type), 'resource': int(event.resource),
                    'serial': int(event.serial), 'code': int(event.code),
                    'request': int(event.request), 'minor': int(event.minor)})
        except BaseException as failure:
            self.unexpected.append({'handler_failure': type(failure).__name__,
                                    'message': str(failure)})
        # Never raise across ctypes or chain Xlib's fatal default.
        return 0

    def install(self):
        if self._closed:
            raise RuntimeError('session display is already closed')
        if self._installed:
            return self
        self.client.x.XSetErrorHandler.argtypes = [self._handler_type]
        self.client.x.XSetErrorHandler.restype = C.c_void_p
        self._previous = self.client.x.XSetErrorHandler(self._callback)
        self._installed = True
        return self

    def read(self, window, request, operation):
        if not self._installed or self._closed:
            raise RuntimeError('session X error handler is not installed')
        if self._borrowed is not None:
            raise RuntimeError('session window reads cannot nest')
        # Pending errors belong to earlier work, even for this same window.
        self.client.x.XSync(self.client.display, 0)
        borrowed = {'window': _scalar(window), 'request': _scalar(request),
                    'retired': False}
        self._borrowed = borrowed
        try:
            result = operation()
        finally:
            try:
                self.client.x.XSync(self.client.display, 0)
            finally:
                self._borrowed = None
        return None if borrowed['retired'] else result

    def close_display(self):
        if self._closed:
            return
        if not self._installed:
            raise RuntimeError('session X error handler is not installed')
        try:
            try:
                self.client.x.XSync(self.client.display, 0)
            finally:
                self.client.close()
        finally:
            try:
                self.client.x.XSetErrorHandler.argtypes = [C.c_void_p]
                self.client.x.XSetErrorHandler(self._previous)
            finally:
                self._installed = False
                self._closed = True
