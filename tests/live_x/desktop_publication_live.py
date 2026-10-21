#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or any later version.
"""Independent pager clients observe bounded, coherent desktop publication."""
import argparse
import ctypes as C
import json
from pathlib import Path
import shutil
import time

from geometry_transition_live import Client
from live_x_harness import HarnessRuntime, start_display_server, wait_for_predicate
from manager_handoff_live import Event
from mfl_stream_live import Connection


def run(args):
    args.artifacts_dir.mkdir(parents=True, exist_ok=True)
    evidence = {'case': args.case, 'status': 'running'}
    with HarnessRuntime(prefix='fvwm-desktop-publication-') as runtime:
        connection = None
        client = None
        try:
            display = args.display
            if display is None:
                _, display = start_display_server(runtime, 'Xvfb',
                    ['Xvfb', '-screen', '0', '800x600x24', '-noreset'],
                    'xserver.log', time.monotonic() + 10)
            config = runtime.root / 'config'
            config.write_text(f'ModulePath {args.mfl.parent}\n'
                'Style * TileCascadePlacement\n'
                'AddToFunc StartFunction I Module FvwmMFL\n')
            core = runtime.start('core', [str(args.fvwm), '-d', display,
                '-f', str(config), '-v', '-o', str(runtime.root / 'debug.log')],
                'core.log', runtime.environment({'FVWMMFL_SOCKET_PATH':
                    str(runtime.root)}))
            endpoint = runtime.root / f"fvwm_mfl_{display.replace(':', '_')}.sock"
            wait_for_predicate('desktop endpoint', endpoint.exists,
                time.monotonic() + 12, process=core)
            connection = Connection(endpoint)
            connection.record()
            connection.send(b'set echo\n')
            client = Client(display)
            x, d = client.x, client.display
            x.XInternAtom.argtypes = [C.c_void_p, C.c_char_p, C.c_int]
            x.XInternAtom.restype = C.c_ulong
            x.XGetWindowProperty.argtypes = [C.c_void_p, C.c_ulong, C.c_ulong,
                C.c_long, C.c_long, C.c_int, C.c_ulong, C.POINTER(C.c_ulong),
                C.POINTER(C.c_int), C.POINTER(C.c_ulong), C.POINTER(C.c_ulong),
                C.POINTER(C.c_void_p)]
            x.XSendEvent.argtypes = [C.c_void_p, C.c_ulong, C.c_int, C.c_long,
                C.POINTER(Event)]
            atom = lambda name: x.XInternAtom(d, name.encode(), 0)

            def prop(name, window=None):
                actual, count, after = C.c_ulong(), C.c_ulong(), C.c_ulong()
                format_, data = C.c_int(), C.c_void_p()
                status = x.XGetWindowProperty(d, window or client.root,
                    atom(name), 0, 16384, 0, 0, C.byref(actual),
                    C.byref(format_), C.byref(count), C.byref(after),
                    C.byref(data))
                try:
                    assert status == 0 and actual.value == atom('CARDINAL'), name
                    assert format_.value == 32 and after.value == 0, name
                    assert count.value <= 16384, name
                    values = C.cast(data, C.POINTER(C.c_ulong))
                    return [values[i] for i in range(count.value)]
                finally:
                    if data:
                        x.XFree(data)

            def command(text):
                connection.send((text + '\n').encode())
                connection.echo('desktop-barrier')

            def message(name, value, format_=32):
                event = Event()
                event.message.type = 33
                event.message.window = client.root
                event.message.message_type = atom(name)
                event.message.format = format_
                event.message.data[0] = value
                # data[1] = 0: accept the EWMH timestamp-zero compatibility case.
                assert x.XSendEvent(d, client.root, 0,
                    (1 << 19) | (1 << 20), C.byref(event))
                x.XSync(d, 0)

            def expect(name, value, window=None):
                wait_for_predicate(name, lambda: prop(name, window) == value,
                    time.monotonic() + 5, process=core)

            def refused(name, values, property_, expected, format_=32):
                for value in values:
                    message(name, value, format_)
                    deadline = time.monotonic() + 0.2
                    while time.monotonic() < deadline:
                        assert core.poll() is None
                        assert prop(property_) == expected, (name, value)
                        time.sleep(0.01)
                    command('Nop')
                    assert prop(property_) == expected, (name, value)

            def snapshot(count, viewport=None):
                expect('_NET_NUMBER_OF_DESKTOPS', [count])
                wait_for_predicate('dependent array lengths', lambda:
                    len(prop('_NET_DESKTOP_VIEWPORT')) == count * 2 and
                    len(prop('_NET_WORKAREA')) == count * 4,
                    time.monotonic() + 5, process=core)
                view = prop('_NET_DESKTOP_VIEWPORT')
                work = prop('_NET_WORKAREA')
                assert len(view) == count * 2, (count, len(view))
                assert len(work) == count * 4, (count, len(work))
                assert all(work[i + 2] > 0 and work[i + 3] > 0
                    for i in range(0, len(work), 4))
                if viewport is not None:
                    assert view == viewport * count
                evidence.setdefault('snapshots', []).append({'count': count,
                    'viewport_items': len(view), 'workarea_items': len(work),
                    'first_viewport': view[:2], 'first_workarea': work[:4]})

            if args.case == 'count':
                command('EwmhNumberOfDesktops 2 8')
                expect('_NET_NUMBER_OF_DESKTOPS', [2])
                message('_NET_NUMBER_OF_DESKTOPS', 4)
                snapshot(4)
                refused('_NET_NUMBER_OF_DESKTOPS', [0, -1, 9, 0xffffffff],
                    '_NET_NUMBER_OF_DESKTOPS', [4])
                refused('_NET_NUMBER_OF_DESKTOPS', [5],
                    '_NET_NUMBER_OF_DESKTOPS', [4], 8)
                message('_NET_CURRENT_DESKTOP', 3)
                expect('_NET_CURRENT_DESKTOP', [3])
                refused('_NET_NUMBER_OF_DESKTOPS', [2],
                    '_NET_NUMBER_OF_DESKTOPS', [4])
                message('_NET_CURRENT_DESKTOP', 0)
                expect('_NET_CURRENT_DESKTOP', [0])
                client.create()
                wait_for_predicate('managed client', client.frame,
                    time.monotonic() + 5, process=core)
                command(f'WindowId {client.window} MoveToDesk 0 3')
                expect('_NET_WM_DESKTOP', [3], client.window)
                refused('_NET_NUMBER_OF_DESKTOPS', [2],
                    '_NET_NUMBER_OF_DESKTOPS', [4])
                command(f'WindowId {client.window} MoveToDesk 0 0')
                message('_NET_NUMBER_OF_DESKTOPS', 2)
                snapshot(2)
                evidence['populated_and_current_shrink_refused'] = True
            elif args.case == 'current':
                command('EwmhNumberOfDesktops 3 8')
                message('_NET_CURRENT_DESKTOP', 2)
                expect('_NET_CURRENT_DESKTOP', [2])
                client.create()
                wait_for_predicate('managed client', client.frame,
                    time.monotonic() + 5, process=core)
                expect('_NET_WM_DESKTOP', [2], client.window)
                refused('_NET_CURRENT_DESKTOP', [3, -1, 0xffffffff],
                    '_NET_CURRENT_DESKTOP', [2])
                refused('_NET_CURRENT_DESKTOP', [1],
                    '_NET_CURRENT_DESKTOP', [2], 8)
                message('_NET_CURRENT_DESKTOP', 0)
                expect('_NET_CURRENT_DESKTOP', [0])
                snapshot(3)
                evidence['root_request_and_real_client_desk'] = True
            else:
                snapshot(1)
                command('EwmhNumberOfDesktops 301')
                snapshot(301)
                command('DesktopSize 2 2')
                command('GotoPage 1 1')
                _, _, width, height = client.geometry(client.root)
                snapshot(301, [width, height])
                command('GotoDesk 0 400')
                snapshot(401)
                expect('_NET_CURRENT_DESKTOP', [400])
                command('GotoDesk 0 0')
                snapshot(301)
                previous = {n: prop(n) for n in ['_NET_NUMBER_OF_DESKTOPS',
                    '_NET_DESKTOP_VIEWPORT', '_NET_WORKAREA']}
                command('EwmhNumberOfDesktops 2147483647')
                assert previous == {n: prop(n) for n in previous}
                command('EwmhNumberOfDesktops 3')
                snapshot(3)
                refused('_NET_NUMBER_OF_DESKTOPS', [1025, 0x7fffffff],
                    '_NET_NUMBER_OF_DESKTOPS', [3])
                evidence['oversized_publication_refusal_and_recovery'] = True
            command('Nop')
            connection.send(b'Quit\n')
            assert core.wait_until_exit(time.monotonic() + 5) == 0
            evidence['status'] = 'passed'
        finally:
            if connection is not None:
                connection.close()
            if client is not None:
                client.close()
            for process in reversed(runtime.processes):
                process.close(time.monotonic() + 5)
            for log in runtime.root.glob('*.log'):
                shutil.copy2(log, args.artifacts_dir / log.name)
            evidence['children_after_cleanup'] = {p.name: p.poll()
                for p in runtime.processes}
            (args.artifacts_dir / 'evidence.json').write_text(
                json.dumps(evidence, indent=2) + '\n')
    print('Desktop publication passed: ' + args.case)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ['fvwm', 'mfl', 'artifacts-dir']:
        parser.add_argument('--' + name, type=Path, required=True)
    parser.add_argument('--display')
    parser.add_argument('--case', choices=['count', 'current', 'arrays'],
        required=True)
    args = parser.parse_args()
    args.fvwm, args.mfl = args.fvwm.resolve(), args.mfl.resolve()
    run(args)
