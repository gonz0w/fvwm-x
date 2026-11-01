#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or (at your option) any later
# version.
"""Two actual screen managers: synthetic clears, ordered loss and isolation.

This fixture is authored for the next shared manager checkpoint. A peer naming
the core's window in XSetSelectionOwner still owns that selection itself; it
cannot supply a genuine foreign-role clear to the core. Linked production
controls cover those negative identity cases instead.
"""
import argparse
import ctypes as C
import json
from pathlib import Path
import shutil
import time

from geometry_transition_live import Client
from live_x_harness import HarnessRuntime, start_display_server, wait_for_predicate
from mfl_stream_live import Connection
from selection_live import SelectionNotify


class Clear(C.Structure):
    _fields_ = [('type', C.c_int), ('serial', C.c_ulong),
        ('send_event', C.c_int), ('display', C.c_void_p),
        ('window', C.c_ulong), ('selection', C.c_ulong), ('time', C.c_ulong)]


class Event(C.Union):
    _fields_ = [('clear', Clear), ('selection', SelectionNotify),
        ('pad', C.c_long * 24)]


class Screen:
    def __init__(self, runtime, args, display, index):
        self.peer = self.core = self.link = None
        try:
            self.initialize(runtime, args, display, index)
        except BaseException:
            self.close()
            raise

    def initialize(self, runtime, args, display, index):
        self.peer = Client(display)
        self.x, self.d = self.peer.x, self.peer.display
        p, w, i = C.c_void_p, C.c_ulong, C.c_int
        for name, arguments, result in (
                ('XScreenCount', [p], i),
                ('XFlush', [p], i),
                ('XInternAtom', [p, C.c_char_p, i], w),
                ('XGetSelectionOwner', [p, w], w),
                ('XSetSelectionOwner', [p, w, w, w], i),
                ('XConvertSelection', [p, w, w, w, w, w], i),
                ('XSendEvent', [p, w, i, C.c_long, C.POINTER(Event)], i),
                ('XCheckTypedWindowEvent', [p, w, i, C.POINTER(Event)], i),
                ('XGetWindowProperty', [p, w, w, C.c_long, C.c_long, i, w,
                    C.POINTER(w), C.POINTER(i), C.POINTER(w), C.POINTER(w),
                    C.POINTER(p)], i)):
            getattr(self.x, name).argtypes = arguments
            getattr(self.x, name).restype = result
        assert self.x.XScreenCount(self.d) == 2
        self.selection = self.atom(f'WM_S{index}')
        self.target = self.atom('TIMESTAMP')
        self.property = self.atom('manager-ownership-result')
        self.window = self.x.XCreateSimpleWindow(self.d, self.peer.root,
            0, 0, 1, 1, 0, 0, 0)
        self.root = runtime.root / f'screen-{index}'
        self.root.mkdir(mode=0o700)
        self.core = runtime.start(f'core-{index}', [str(args.fvwm),
            '-d', display, '-v', '-o', str(self.root / 'debug.log'),
            '-c', f'ModulePath {args.mfl.parent}',
            '-c', 'AddToFunc StartFunction I Module FvwmMFL'],
            f'core-{index}.log', runtime.environment({
                'FVWMMFL_SOCKET_PATH': str(self.root)}))
        endpoint = self.root / f"fvwm_mfl_{display.replace(':', '_')}.sock"
        wait_for_predicate('screen manager startup', endpoint.exists,
            time.monotonic() + 12, process=self.core)
        self.link = Connection(endpoint)
        self.link.record()
        self.link.send(b'set echo\n')
        self.link.echo(f'screen-{index}-ready')
        self.owner = self.x.XGetSelectionOwner(self.d, self.selection)
        assert self.owner and self.owner != self.window

    def atom(self, name):
        return self.x.XInternAtom(self.d, name.encode(), False)

    def query(self):
        self.x.XConvertSelection(self.d, self.selection, self.target,
            self.property, self.window, 0)
        self.x.XFlush(self.d)

    def metadata(self):
        self.query()
        event = Event()
        wait_for_predicate('manager timestamp after preceding X events', lambda:
            self.x.XCheckTypedWindowEvent(self.d, self.window, 31,
                C.byref(event)), time.monotonic() + 3, process=self.core)
        reply = event.selection
        assert (reply.requestor, reply.selection, reply.target, reply.time,
            reply.property) == (self.window, self.selection, self.target, 0,
                self.property)
        actual, count, after = C.c_ulong(), C.c_ulong(), C.c_ulong()
        form, data = C.c_int(), C.c_void_p()
        assert self.x.XGetWindowProperty(self.d, self.window, self.property,
            0, 2, False, 0, C.byref(actual), C.byref(form), C.byref(count),
            C.byref(after), C.byref(data)) == 0
        try:
            assert (actual.value, form.value, count.value, after.value) == (
                self.atom('INTEGER'), 32, 1, 0)
            assert C.cast(data, C.POINTER(C.c_ulong))[0] != 0
        finally:
            if data:
                self.x.XFree(data)

    def synthetic_clear(self):
        event = Event()
        event.clear = Clear(type=29, display=self.d, window=self.owner,
            selection=self.selection, time=0)
        assert self.x.XSendEvent(self.d, self.owner, False, 0, C.byref(event))
        # Same sending connection: the following conversion is queued after
        # the clear. An independent module-pipe echo would not prove this.
        self.metadata()
        assert self.x.XGetSelectionOwner(self.d, self.selection) == self.owner

    def take_over(self):
        # A conversion precedes the real ownership loss on this connection.
        # Its reply may complete or be cancelled; it must not delay shutdown.
        self.query()
        self.x.XSetSelectionOwner(self.d, self.selection, self.window, 0)
        self.x.XSync(self.d, False)
        assert self.x.XGetSelectionOwner(self.d, self.selection) == self.window
        assert self.core.wait_until_exit(time.monotonic() + 5) == 0
        assert self.x.XGetSelectionOwner(self.d, self.selection) == self.window

    def close(self):
        if self.core is not None:
            self.core.close(time.monotonic() + 3)
            self.core = None
        if self.link is not None:
            self.link.close()
            self.link = None
        if self.peer is not None:
            self.peer.close()
            self.peer = None


def run(args):
    args.artifacts_dir.mkdir(parents=True, exist_ok=True)
    evidence = {'status': 'started', 'scope': 'two owned Xvfb screens'}
    with HarnessRuntime(prefix='fvwm-manager-ownership-') as runtime:
        screens = []
        try:
            display = args.display
            if display is None:
                _, display = start_display_server(runtime, 'Xvfb',
                    ['Xvfb', '-screen', '0', '800x600x24', '-screen', '1',
                        '640x480x24', '-noreset'],
                    'xserver.log', time.monotonic() + 10)
            assert '.' not in display.rsplit(':', 1)[-1]
            for index in range(2):
                screens.append(Screen(runtime, args, f'{display}.{index}', index))
            assert screens[0].owner != screens[1].owner
            for screen in screens:
                screen.synthetic_clear()
            evidence['synthetic-exact-clear-survival'] = 'passed on both screens'
            screens[0].take_over()
            screens[1].metadata()
            assert screens[1].core.poll() is None
            evidence['other-screen-remains-responsive-after-loss'] = 'passed'
            screens[1].take_over()
            for screen in screens:
                assert screen.x.XGetSelectionOwner(screen.d,
                    screen.selection) == screen.window
            evidence['genuine-loss-preserves-both-successors'] = 'passed'
            evidence['status'] = 'passed'
        except BaseException as error:
            evidence.update(status='failed', error=repr(error))
            raise
        finally:
            for screen in reversed(screens):
                screen.close()
            for source in runtime.root.rglob('*.log'):
                name = str(source.relative_to(runtime.root)).replace('/', '-')
                shutil.copy2(source, args.artifacts_dir / name)
            (args.artifacts_dir / 'evidence.json').write_text(
                json.dumps(evidence, indent=2) + '\n')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(allow_abbrev=False)
    parser.add_argument('--fvwm', type=Path, required=True)
    parser.add_argument('--mfl', type=Path, required=True)
    parser.add_argument('--artifacts-dir', type=Path, required=True)
    parser.add_argument('--display')
    args = parser.parse_args()
    args.fvwm, args.mfl = args.fvwm.resolve(), args.mfl.resolve()
    run(args)
