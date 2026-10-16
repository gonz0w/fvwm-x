#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
"""Check wildcard style application/removal on real managed X clients."""
import argparse
import ctypes as C
import hashlib
import json
from pathlib import Path
import shutil
import time

from geometry_transition_live import Client
from live_x_harness import HarnessRuntime, start_display_server, wait_for_predicate
from mfl_stream_live import Connection


def run(args):
    out = args.artifacts_dir.resolve()
    out.mkdir(parents=True, exist_ok=True)
    evidence = {'core_sha256': hashlib.sha256(args.fvwm.read_bytes()).hexdigest(),
                'mfl_sha256': hashlib.sha256(args.mfl.read_bytes()).hexdigest(),
                'driver_sha256': hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
                'cases': []}
    with HarnessRuntime(prefix='fvwm-style-wildcard-') as runtime:
        connection = client = None
        try:
            display = args.display
            if display is None:
                _, display = start_display_server(runtime, 'Xvfb',
                    ['Xvfb', '-screen', '0', '800x600x24', '-noreset'],
                    'server.log', time.monotonic() + 15)
            config = runtime.root / 'config'
            config.write_text(f'ModulePath {args.mfl.parent}\n'
                'Style * NoTitle, BorderWidth 3, HandleWidth 3\n'
                'AddToFunc StartFunction I Module FvwmMFL\n')
            core = runtime.start('core', [str(args.fvwm), '-d', display,
                '-f', str(config)], 'core.log', runtime.environment({
                    'FVWMMFL_SOCKET_PATH': str(runtime.root)}))
            endpoint = runtime.root / f"fvwm_mfl_{display.replace(':', '_')}.sock"
            wait_for_predicate('style endpoint', endpoint.exists,
                time.monotonic() + 15, process=core)
            connection = Connection(endpoint)
            connection.record()
            connection.send(b'set echo\n')
            connection.echo('style-ready')
            client = Client(display)
            client.create()
            wait_for_predicate('managed style client', client.frame,
                time.monotonic() + 10, process=core)
            x, d = client.x, client.display
            x.XInternAtom.argtypes = [C.c_void_p, C.c_char_p, C.c_int]
            x.XInternAtom.restype = C.c_ulong
            x.XGetWindowProperty.argtypes = [C.c_void_p, C.c_ulong, C.c_ulong,
                C.c_long, C.c_long, C.c_int, C.c_ulong, C.POINTER(C.c_ulong),
                C.POINTER(C.c_int), C.POINTER(C.c_ulong), C.POINTER(C.c_ulong),
                C.POINTER(C.c_void_p)]
            x.XGetWindowProperty.restype = C.c_int
            atom = x.XInternAtom(d, b'_NET_FRAME_EXTENTS', 0)

            def extents():
                actual, count, after = C.c_ulong(), C.c_ulong(), C.c_ulong()
                fmt, data = C.c_int(), C.c_void_p()
                status = x.XGetWindowProperty(d, client.window, atom, 0, 4,
                    0, 6, C.byref(actual), C.byref(fmt), C.byref(count),
                    C.byref(after), C.byref(data))
                try:
                    assert status == 0
                    if actual.value == 0:
                        return None
                    assert actual.value == 6
                    assert fmt.value == 32 and count.value == 4 and after.value == 0
                    return list(C.cast(data, C.POINTER(C.c_ulong))[:4])
                finally:
                    if data:
                        x.XFree(data)

            def check(width):
                def ready():
                    if extents() != [width]*4:
                        return False
                    frame = client.frame()
                    if frame is None:
                        return False
                    _, _, cw, ch = client.geometry(client.window)
                    _, _, fw, fh = client.geometry(frame)
                    return (fw, fh) == (cw + 2*width, ch + 2*width)
                wait_for_predicate('style extents and frame geometry', ready,
                    time.monotonic() + 10, process=core)

            check(3)
            cases = [('quoted-literal', '"FVWM transition probe"', 15),
                     ('minimum', 'FVWM' + '?'*40 + '*', 3),
                     ('suffix', 'FVWM?*wrong', 3),
                     ('single-star', 'FVWM*probe', 7),
                     ('question-fallback', 'FVWM?*probe', 9),
                     ('multi-star-fallback', 'FVWM**probe', 11),
                     ('non-overlap', '"FVWM transition *transition probe"', 3)]
            for label, selector, width in cases:
                connection.send((f'Style (Name {selector}) BorderWidth {width if width != 3 else 13}, '
                    f'HandleWidth {width if width != 3 else 13}\nUpdateStyles\n').encode())
                connection.echo(label + '-applied')
                check(width)
                connection.send((f'DestroyStyle (Name {selector})\n'
                    'UpdateStyles\n').encode())
                connection.echo(label + '-removed')
                check(3)
                evidence['cases'].append({'case': label, 'selector': selector,
                    'applied_width': width, 'removed_width': 3})
            evidence['status'] = 'passed'
        finally:
            if connection:
                connection.close()
            if client:
                client.close()
            for path in runtime.root.iterdir():
                if path.is_file():
                    shutil.copy2(path, out / path.name)
            (out / 'results.json').write_text(json.dumps(evidence, indent=2)+'\n')
    print('Real-client style wildcard application/removal: seven cases passed')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('fvwm', 'mfl', 'artifacts-dir'):
        parser.add_argument('--' + name, type=Path, required=True)
    parser.add_argument('--display')
    args = parser.parse_args()
    args.fvwm, args.mfl = args.fvwm.resolve(), args.mfl.resolve()
    run(args)
