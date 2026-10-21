#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or (at your option) any later
# version.
"""Independent manager-selection peers exercise startup handoff and refusal."""
import argparse
import ctypes as C
import json
from pathlib import Path
import shutil
import signal
import time

from geometry_transition_live import Client
from live_x_harness import HarnessRuntime, start_display_server, wait_for_predicate
from mfl_stream_live import Connection


class Message(C.Structure):
    _fields_ = [('type', C.c_int), ('serial', C.c_ulong),
        ('send_event', C.c_int), ('display', C.c_void_p),
        ('window', C.c_ulong), ('message_type', C.c_ulong),
        ('format', C.c_int), ('data', C.c_long * 5)]


class Event(C.Union):
    _fields_ = [('message', Message), ('pad', C.c_long * 24)]


def run_case(args, label):
    out = args.artifacts_dir / label
    out.mkdir(parents=True, exist_ok=True)
    evidence = {'case': label, 'status': 'running'}
    with HarnessRuntime(prefix='fvwm-manager-handoff-') as runtime:
        peers = []
        connection = None
        try:
            display = args.display
            if display is None:
                _, display = start_display_server(runtime, 'Xvfb',
                    ['Xvfb', '-screen', '0', '800x600x24', '-noreset'],
                    'xserver.log', time.monotonic() + 10)
            peer = Client(display)
            peers.append(peer)
            x, d = peer.x, peer.display
            x.XInternAtom.argtypes = [C.c_void_p, C.c_char_p, C.c_int]
            x.XInternAtom.restype = C.c_ulong
            x.XDefaultScreen.argtypes = [C.c_void_p]
            x.XDefaultScreen.restype = C.c_int
            x.XGetSelectionOwner.argtypes = [C.c_void_p, C.c_ulong]
            x.XGetSelectionOwner.restype = C.c_ulong
            x.XSetSelectionOwner.argtypes = [C.c_void_p, C.c_ulong,
                C.c_ulong, C.c_ulong]
            # This binding has a full native XEvent-sized destination.
            x.XCheckTypedWindowEvent.argtypes = [C.c_void_p, C.c_ulong,
                C.c_int, C.POINTER(Event)]
            selection = x.XInternAtom(d,
                f'WM_S{x.XDefaultScreen(d)}'.encode(), 0)
            manager = x.XInternAtom(d, b'MANAGER', 0)
            old = None
            if label != 'announcement':
                old = x.XCreateSimpleWindow(d, peer.root, 1, 1,
                    1, 1, 0, 0, 0)
                x.XSetSelectionOwner(d, selection, old, 0)
            mask = 1 << 17  # StructureNotify: observe the MANAGER message.
            if old:
                mask |= 1 << 20  # Simulate the previous WM's redirect lease.
            x.XSelectInput(d, peer.root, mask)
            x.XSync(d, 0)
            assert x.XGetSelectionOwner(d, selection) == (old or 0)
            config = runtime.root / 'config'
            config.write_text(f'ModulePath {args.mfl.parent}\n'
                'AddToFunc StartFunction I Module FvwmMFL\n')
            argv = [str(args.fvwm), '-d', display, '-f', str(config),
                '-v', '-o', str(runtime.root / 'debug.log')]
            if label not in ['announcement', 'refused']:
                argv.append('--replace')
            core = runtime.start('core', argv, 'core.log', runtime.environment({
                'FVWMMFL_SOCKET_PATH': str(runtime.root)}))
            endpoint = runtime.root / f"fvwm_mfl_{display.replace(':', '_')}.sock"

            def alive(window):
                root, parent, children, count = (C.c_ulong(), C.c_ulong(),
                    C.POINTER(C.c_ulong)(), C.c_uint())
                assert x.XQueryTree(d, peer.root, C.byref(root),
                    C.byref(parent), C.byref(children), C.byref(count))
                try:
                    return window in [children[i] for i in range(count.value)]
                finally:
                    if children:
                        x.XFree(children)

            def announcement():
                event = Event()
                while x.XCheckTypedWindowEvent(d, peer.root, 33, C.byref(event)):
                    m = event.message
                    if m.message_type == manager and m.data[1] == selection:
                        assert m.window == peer.root and m.format == 32
                        assert m.send_event and m.data[0] != 0
                        assert m.data[2] == x.XGetSelectionOwner(d, selection)
                        assert m.data[2] != 0 and list(m.data)[3:] == [0, 0]
                        evidence['announcement'] = list(m.data)
                        return m.data[2]
                return None

            if label == 'refused':
                assert core.wait_until_exit(time.monotonic() + 10) == 1
                assert x.XGetSelectionOwner(d, selection) == old and alive(old)
                evidence['peer_preserved'] = True
            else:
                owner = wait_for_predicate('manager announcement', announcement,
                    time.monotonic() + 12, process=core)
                if old:
                    assert core.poll() is None and alive(old)
                start = time.monotonic()
                if label == 'cooperative':
                    assert not endpoint.exists()
                    # ICCCM ordering: release redirect before destroying owner.
                    x.XSelectInput(d, peer.root, 1 << 17)
                    x.XDestroyWindow(d, old)
                    x.XSync(d, 0)
                elif label == 'cancelled':
                    core.process.send_signal(signal.SIGTERM)
                elif label == 'superseded':
                    successor = Client(display)
                    peers.append(successor)
                    window = successor.x.XCreateSimpleWindow(successor.display,
                        successor.root, 1, 1, 1, 1, 0, 0, 0)
                    x.XSetSelectionOwner(successor.display, selection, window, 0)
                    x.XSync(successor.display, 0)
                    assert x.XGetSelectionOwner(d, selection) == window
                if label in ['announcement', 'cooperative']:
                    wait_for_predicate('replacement readiness', endpoint.exists,
                        time.monotonic() + 12, process=core)
                    connection = Connection(endpoint)
                    connection.record()
                    connection.send(b'set echo\n')
                    connection.echo('manager-ready')
                    assert x.XGetSelectionOwner(d, selection) == owner
                    connection.send(b'Quit\n')
                    assert core.wait_until_exit(time.monotonic() + 5) == 0
                else:
                    limit = 13 if label == 'stalled' else 3
                    assert core.wait_until_exit(time.monotonic() + limit) == 1
                    elapsed = time.monotonic() - start
                    assert elapsed < limit
                    if label == 'stalled':
                        assert elapsed >= 8
                    assert alive(old) and not alive(owner)
                    expected = window if label == 'superseded' else 0
                    assert x.XGetSelectionOwner(d, selection) == expected
                    evidence.update(seconds=elapsed, peer_preserved=True,
                        successor_preserved=(label == 'superseded'))
            evidence['status'] = 'passed'
        finally:
            if connection is not None:
                connection.close()
            for peer in reversed(peers):
                peer.close()
            for process in reversed(runtime.processes):
                process.close(time.monotonic() + 5)
            for log in runtime.root.glob('*.log'):
                shutil.copy2(log, out / log.name)
            evidence['children_after_cleanup'] = {p.name: p.poll()
                for p in runtime.processes}
            (out / 'evidence.json').write_text(json.dumps(evidence, indent=2) + '\n')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ['fvwm', 'mfl', 'artifacts-dir']:
        parser.add_argument('--' + name, type=Path, required=True)
    parser.add_argument('--display')
    parser.add_argument('--case', choices=['announcement', 'replacement',
        'failure'], required=True)
    args = parser.parse_args()
    args.fvwm, args.mfl = args.fvwm.resolve(), args.mfl.resolve()
    cases = {'announcement': ['announcement', 'refused'],
        'replacement': ['cooperative', 'superseded'],
        'failure': ['stalled', 'cancelled']}
    for label in cases[args.case]:
        run_case(args, label)
    print('Manager handoff passed: ' + args.case)
