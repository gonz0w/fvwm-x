#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or any later version.
"""Independent dock clients observe root work areas and reservation lifetime."""
import argparse
import ctypes as C
import json
from pathlib import Path
import shutil
import time

from geometry_transition_live import Client
from live_x_harness import HarnessRuntime, start_display_server, wait_for_predicate
from mfl_stream_live import Connection


def run(args):
    args.artifacts_dir.mkdir(parents=True, exist_ok=True)
    evidence = {'case': args.case, 'status': 'running'}
    with HarnessRuntime(prefix='fvwm-workarea-') as runtime:
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
                'Style * TileCascadePlacement, StickyAcrossPages\n'
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

            def expect(name, value, window=None):
                wait_for_predicate(name, lambda: prop(name, window) == value,
                    time.monotonic() + 5, process=core)

            x.XChangeProperty.argtypes = [C.c_void_p, C.c_ulong,
                C.c_ulong, C.c_ulong, C.c_int, C.c_int, C.c_void_p, C.c_int]
            x.XDeleteProperty.argtypes = [C.c_void_p, C.c_ulong, C.c_ulong]
            command('EwmhNumberOfDesktops 3')
            client.create()
            wait_for_predicate('managed dock', client.frame,
                time.monotonic() + 5, process=core)
            _, _, width, height = client.geometry(client.root)

            def reserve(values=None, kind='CARDINAL', format_=32):
                if values is None:
                    x.XDeleteProperty(d, client.window, atom('_NET_WM_STRUT'))
                else:
                    words = (C.c_ulong * len(values))(*values)
                    x.XChangeProperty(d, client.window, atom('_NET_WM_STRUT'),
                        atom(kind), format_, 0, words, len(values))
                x.XSync(d, 0)

            def area(edges=(0, 0, 0, 0), desk=0):
                left, right, top, bottom = edges
                expected = [0, 0, width, height] * 3
                expected[desk * 4:desk * 4 + 4] = [left, top,
                    width - left - right, height - top - bottom]
                expect('_NET_WORKAREA', expected)
                evidence.setdefault('areas', []).append(expected)

            if args.case == 'publication':
                reserve([17, 13, 23, 19])
                area([17, 13, 23, 19])
                command(f'WindowId {client.window} StickAcrossPages off')
                area()
                command(f'WindowId {client.window} StickAcrossPages on')
                area([17, 13, 23, 19])
                command(f'WindowId {client.window} StickAcrossDesks on')
                expect('_NET_WORKAREA', [17, 23, width - 30, height - 42] * 3)
                command(f'WindowId {client.window} StickAcrossDesks off')
                area([17, 13, 23, 19])
                command('DesktopSize 2 2')
                command('GotoPage 1 1')
                area([17, 13, 23, 19])
                command(f'WindowId {client.window} MoveToDesk 0 2')
                area([17, 13, 23, 19], 2)
                command(f'WindowId {client.window} MoveToDesk 0 0')
                command('EwmhBaseStruts 25 0 0 0')
                expect('_NET_WORKAREA', [25, 23, width - 38, height - 42] +
                    [25, 0, width - 25, height] * 2)
                command('EwmhBaseStruts 0 0 0 0')
                client.destroy()
                area()
                evidence['page_desk_base_destroy'] = True
            elif args.case == 'revocation':
                reserve([31, 0, 0, 0])
                area([31, 0, 0, 0])
                reserve([0, 0, 0, 0])
                area()
                reserve([0, 0, 29, 0])
                area([0, 0, 29, 0])
                reserve()
                area()
                reserve([0, 0, 0, 37])
                area([0, 0, 0, 37])
                command(f'Style "FVWM transition probe" EWMHIgnoreStrutHints')
                command('UpdateStyles')
                area()
                command(f'Style "FVWM transition probe" EWMHUseStrutHints')
                command('UpdateStyles')
                area([0, 0, 0, 37])
                evidence['zero_delete_ignore_restore'] = True
            else:
                reserve([19, 0, 0, 0])
                area([19, 0, 0, 0])
                for values, kind, fmt in [
                        ([0xffffffff, 0, 0, 0], 'CARDINAL', 32),
                        ([width, 1, 0, 0], 'CARDINAL', 32),
                        ([0, 0, height + 1, 0], 'CARDINAL', 32),
                        ([0, 0, 0], 'CARDINAL', 32),
                        ([0, 0, 0, 0, 0], 'CARDINAL', 32),
                        ([0, 0, 0, 0], 'INTEGER', 32),
                        ([0, 0, 0, 0], 'CARDINAL', 8)]:
                    reserve(values, kind, fmt)
                    # XSync orders the independent producer. Observe a
                    # bounded quiet interval, then a core command barrier.
                    time.sleep(0.08)
                    command('Nop')
                    area([19, 0, 0, 0])
                reserve([0, 0, 13, 0])
                area([0, 0, 13, 0])
                evidence['malformed_retention_and_recovery'] = True
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
    print('Work area passed: ' + args.case)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ['fvwm', 'mfl', 'artifacts-dir']:
        parser.add_argument('--' + name, type=Path, required=True)
    parser.add_argument('--display')
    parser.add_argument('--case', choices=['publication', 'revocation', 'bounds'],
        required=True)
    args = parser.parse_args()
    args.fvwm, args.mfl = args.fvwm.resolve(), args.mfl.resolve()
    run(args)
