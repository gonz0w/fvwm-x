#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
"""Observe real root stacking above override windows and InputOnly siblings."""
import argparse
import ctypes as C
import json
from pathlib import Path
import shutil
import time

from geometry_transition_live import Client
from colorset_live import WindowAttributes
from live_x_harness import HarnessRuntime, start_display_server, wait_for_predicate
from mfl_stream_live import Connection


class SetAttributes(C.Structure):
    _fields_ = [(name, kind) for name, kind in (
        ('background_pixmap', C.c_ulong), ('background_pixel', C.c_ulong),
        ('border_pixmap', C.c_ulong), ('border_pixel', C.c_ulong),
        ('bit_gravity', C.c_int), ('win_gravity', C.c_int),
        ('backing_store', C.c_int), ('backing_planes', C.c_ulong),
        ('backing_pixel', C.c_ulong), ('save_under', C.c_int),
        ('event_mask', C.c_long), ('do_not_propagate_mask', C.c_long),
        ('override_redirect', C.c_int), ('colormap', C.c_ulong),
        ('cursor', C.c_ulong))]


def run(args):
    args.artifacts_dir.mkdir(parents=True, exist_ok=True)
    clients, popups, evidence = [], [], {}
    connection = None
    with HarnessRuntime(prefix='fvwm-raise-unmanaged-') as runtime:
        try:
            display = args.display
            if display is None:
                _, display = start_display_server(runtime, 'Xvfb',
                    ['Xvfb', '-screen', '0', '1000x600x24', '-noreset'],
                    'xserver.log', time.monotonic() + 10)
            config, log = runtime.root / 'config', runtime.root / 'debug.log'
            config.write_text(f'ModulePath {args.mfl.parent}\nDefaultFont fixed\n'
                'BugOpts RaiseOverUnmanaged on\n'
                'Style * ClickToFocus, Layer 4, TileCascadePlacement\n'
                'AddToFunc StartFunction I Module FvwmMFL\n')
            core = runtime.start('core', [str(args.fvwm), '-d', display,
                '-f', str(config), '-v', '-o', str(log)], 'core.log',
                runtime.environment({'FVWMMFL_SOCKET_PATH': str(runtime.root)}))
            endpoint = runtime.root / f"fvwm_mfl_{display.replace(':', '_')}.sock"
            wait_for_predicate('raise endpoint', endpoint.exists,
                time.monotonic() + 10, process=core)
            connection = Connection(endpoint)
            connection.record()
            connection.send(b'set echo\n')
            connection.echo('raise-ready')
            for _ in range(2):
                client = Client(display)
                clients.append(client)
                client.create()
                wait_for_predicate('raise frame', client.frame,
                    time.monotonic() + 5, process=core)
                client.x.XGetWindowAttributes.argtypes = [C.c_void_p,
                    C.c_ulong, C.POINTER(WindowAttributes)]
                def viewable():
                    value = WindowAttributes()
                    return client.x.XGetWindowAttributes(client.display,
                        client.window, C.byref(value)) and value.map_state == 2
                wait_for_predicate('raise client viewable', viewable,
                    time.monotonic() + 5, process=core)
                connection.echo(f'raise-client-ready-{len(clients)}')
            client = clients[0]
            x, d = client.x, client.display
            x.XChangeWindowAttributes.argtypes = [C.c_void_p, C.c_ulong,
                C.c_ulong, C.POINTER(SetAttributes)]
            x.XCreateWindow.argtypes = [C.c_void_p, C.c_ulong, C.c_int,
                C.c_int, C.c_uint, C.c_uint, C.c_uint, C.c_int, C.c_uint,
                C.c_void_p, C.c_ulong, C.POINTER(SetAttributes)]
            x.XCreateWindow.restype = C.c_ulong
            x.XRaiseWindow.argtypes = [C.c_void_p, C.c_ulong]
            x.XGetInputFocus.argtypes = [C.c_void_p, C.POINTER(C.c_ulong),
                C.POINTER(C.c_int)]
            attr = SetAttributes(override_redirect=1)
            for index in range(2):
                window = x.XCreateSimpleWindow(d, client.root,
                    200 + index * 100, 100, 80, 60, 0, 0, 0)
                popups.append(window)
                x.XChangeWindowAttributes(d, window, 1 << 9, C.byref(attr))
                x.XMapWindow(d, window)
            input_only = x.XCreateWindow(d, client.root, 0, 0, 1, 1, 0,
                0, 2, None, 1 << 9, C.byref(attr))
            assert input_only
            popups.append(input_only)
            x.XMapWindow(d, input_only)
            x.XSync(d, 0)
            connection.send(f'WindowId {client.window} Focus\n'.encode())
            connection.echo('raise-focused')

            def focused():
                focus, revert = C.c_ulong(), C.c_int()
                x.XGetInputFocus(d, C.byref(focus), C.byref(revert))
                return focus.value == client.window
            wait_for_predicate('raise focus confirmed', focused,
                time.monotonic() + 5, process=core)

            def stack():
                root, parent, result, count = (C.c_ulong(), C.c_ulong(),
                    C.POINTER(C.c_ulong)(), C.c_uint())
                assert x.XQueryTree(d, client.root, C.byref(root),
                    C.byref(parent), C.byref(result), C.byref(count))
                try:
                    return list(result[:count.value])
                finally:
                    if result:
                        x.XFree(result)

            for turn, order in enumerate((popups[:], list(reversed(popups)))):
                for window in order:
                    x.XRaiseWindow(d, window)
                x.XSync(d, 0)
                assert stack().index(client.frame()) < max(
                    stack().index(window) for window in popups[:2])
                connection.send(f'WindowId {client.window} Raise\n'.encode())
                connection.echo(f'raise-observed-{turn}')
                observed = stack()
                assert all(observed.index(client.frame()) >
                    observed.index(window) for window in popups[:2])
                assert focused()
            evidence['highest-real-override-and-input-only-order'] = 'passed'
            for window in popups[:]:
                x.XDestroyWindow(d, window)
                popups.remove(window)
            x.XSync(d, 0)
            connection.send(f'WindowId {client.window} Lower\n'
                f'WindowId {client.window} Raise\n'.encode())
            connection.echo('raise-after-popup-teardown')
            assert stack().index(client.frame()) > stack().index(clients[1].frame())
            assert focused()
            evidence['popup-teardown-following-raise-and-focus'] = 'passed'
            for failure in ('X error', 'AddressSanitizer', 'runtime error:'):
                assert failure not in log.read_text(), log.read_text()
        except BaseException as error:
            evidence['error'] = repr(error)
            raise
        finally:
            if connection:
                connection.close()
            if clients:
                for window in popups:
                    clients[0].x.XDestroyWindow(clients[0].display, window)
            for client in reversed(clients):
                client.close()
            for path in runtime.root.glob('*.log'):
                shutil.copy2(path, args.artifacts_dir / path.name)
            (args.artifacts_dir / 'evidence.json').write_text(
                json.dumps(evidence, indent=2) + '\n')
    print(json.dumps(evidence, indent=2))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(allow_abbrev=False)
    for name in ('fvwm', 'mfl', 'artifacts-dir'):
        parser.add_argument('--' + name, type=Path, required=True)
    parser.add_argument('--display')
    run(parser.parse_args())
