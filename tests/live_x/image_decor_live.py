#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
"""Observe face transfer, scale leases and cancellation through core pixels."""
import argparse
import ctypes as C
import json
from pathlib import Path
import shutil
import time

from live_x_harness import HarnessRuntime, start_display_server, wait_for_predicate
from mfl_stream_live import Connection
from geometry_transition_live import Client
from image_owners_live import png


def run(args, factor):
    out = args.artifacts_dir / f'scale-{factor}'
    out.mkdir(parents=True, exist_ok=True)
    evidence = {}
    with HarnessRuntime(prefix='fvwm-image-decor-') as runtime:
        client = connection = core = None
        try:
            _, display = start_display_server(runtime, 'Xvfb',
                ['Xvfb', '-screen', '0', '1000x800x24', '-noreset'],
                'xserver.log', time.monotonic() + 10)
            sources = {}
            for name, text in [('red', 'red'), ('blue', 'blue'),
                    ('stall', 'red'), ('incomplete', 'red')]:
                sources[name] = runtime.root / (name + '.svg')
                if args.real and name != 'incomplete':
                    text = ('<svg xmlns="http://www.w3.org/2000/svg" '
                        f'width="2" height="2"><rect width="2" '
                        f'height="2" fill="{name}"/></svg>')
                sources[name].write_text(text)
            image = runtime.root / 'opaque.png'
            image.write_bytes(png())
            config = runtime.root / 'config'
            config.write_text(f'ModulePath {args.mfl.parent}\n'
                f'MonitorScale "screen" {factor}\n'
                'Style * TileCascadePlacement\nDefaultFont fixed\n'
                'Colorset 0 fg white, bg black\nStyle * Colorset 0, HilightColorset 0\n'
                'TitleStyle (Solid black -- Flat)\n'
                'AddToFunc StartFunction I Module FvwmMFL\n')
            core = runtime.start('core', [str(args.fvwm), '-d', display,
                '-f', str(config), '-v', '-o', str(runtime.root / 'debug.log')],
                'core.log', runtime.environment({
                    'FVWMMFL_SOCKET_PATH': str(runtime.root),
                    'FVWM_IMAGE_SVG_WORKER': str(args.worker)}))
            endpoint = runtime.root / f"fvwm_mfl_{display.replace(':', '_')}.sock"
            wait_for_predicate('decor fixture endpoint', endpoint.exists,
                time.monotonic() + 10, process=core)
            connection = Connection(endpoint)
            connection.record()
            connection.send(b'set echo\n')
            connection.echo('decor-ready')
            client = Client(display)
            client.create()
            client.x.XStoreName(client.display, client.window, b'')
            frame = wait_for_predicate('decor client frame', client.frame,
                time.monotonic() + 10, process=core)
            x, d = client.x, client.display
            x.XGetImage.argtypes = [C.c_void_p, C.c_ulong, C.c_int, C.c_int,
                C.c_uint, C.c_uint, C.c_ulong, C.c_int]
            x.XGetImage.restype = C.c_void_p
            x.XGetPixel.argtypes = [C.c_void_p, C.c_int, C.c_int]
            x.XGetPixel.restype = C.c_ulong
            x.XDestroyImage.argtypes = [C.c_void_p]
            errors = []
            callback_type = C.CFUNCTYPE(C.c_int, C.c_void_p, C.c_void_p)

            @callback_type
            def error_handler(_display, _event):
                errors.append('X request failed')
                return 0

            x.XSetErrorHandler.argtypes = [callback_type]
            x.XSetErrorHandler(error_handler)
            sequence = 0

            def command(text):
                nonlocal sequence
                sequence += 1
                connection.send((text + '\n').encode())
                connection.echo(f'decor-barrier-{sequence}')

            def pixels():
                # Read the top title strip of the real frame, including
                # inferior decoration windows, with unchanged client geometry.
                _, _, width, height = client.geometry(frame)
                height = height - client.geometry(client.window)[3]
                assert 0 < width <= 1000 and 0 < height < 300
                picture = x.XGetImage(d, frame, 0, 0, width, height,
                    C.c_ulong(-1).value, 2)
                assert picture and not errors, errors
                try:
                    return tuple(x.XGetPixel(picture, px, py)
                        for py in range(height) for px in range(width))
                finally:
                    x.XDestroyImage(picture)

            def painted(color):
                return wait_for_predicate('completed decor paint',
                    lambda: pixels() if pixels().count(color) > 100 else None,
                    time.monotonic() + 10, process=core)

            command(f'TitleStyle (TiledPixmap {image} -- Flat)')
            red = painted(0xff0000)
            geometry = client.geometry(frame)
            if not args.real:
                command(f'TitleStyle (TiledPixmap {sources["stall"]} -- Flat)')
                assert pixels() == red
                evidence['pending-replacement-retains-scaled-old-pixels'] = 'passed'
            command(f'TitleStyle (TiledPixmap {sources["blue"]} -- Flat)')
            blue = painted(0x0000ff)
            assert client.geometry(frame) == geometry
            evidence['deferred-title-transfer-and-scale-completion'] = 'passed'
            command(f'TitleStyle (TiledPixmap {sources["incomplete"]} -- Flat)')
            wait_for_predicate('decor worker refusal',
                lambda: 'SVG refused:' in (runtime.root / 'debug.log').read_text(),
                time.monotonic() + 8, process=core)
            assert pixels() == blue
            evidence['failed-title-replacement-preserves-scaled-pixels'] = 'passed'

            if not args.real:
                # Prepare through BorderStyle's temporary assignment, then
                # destroy the containing decor before its worker completes.
                command('AddToDecor Transient TitleStyle (Solid black)')
                command(f'AddToDecor Transient BorderStyle Active TiledPixmap {sources["stall"]}')
                command('DestroyDecor Transient')
                command(f'TitleStyle (MultiPixmap Main {sources["red"]}, Buttons {sources["red"]} -- Flat)')
                painted(0xff0000)
                evidence['destroyed-decor-cancels-temporary-border-lease'] = 'passed'
                evidence['multi-pixmap-buttons-own-separate-leases'] = 'passed'
                command(f'AddTitleStyle (TiledPixmap {sources["stall"]} -- Flat)')
                command('TitleStyle (Solid black -- Flat)')
                command(f'TitleStyle (TiledPixmap {sources["blue"]} -- Flat)')
                painted(0x0000ff)
                evidence['compound-face-override-cancels-interest'] = 'passed'
            else:
                command(f'TitleStyle (TiledPixmap {sources["red"]} -- Flat)')
                painted(0xff0000)
                evidence['real-svg-decoder-replacement-recovery'] = 'passed'
            assert core.poll() is None and not errors
            assert 'X error' not in (runtime.root / 'debug.log').read_text()
        except BaseException as error:
            evidence['error'] = str(error)
            evidence['wm_exit_status'] = core.poll() if core else None
            raise
        finally:
            if client:
                client.close()
            if connection:
                connection.close()
            for path in runtime.root.glob('*.log'):
                shutil.copy2(path, out / path.name)
            (out / 'evidence.json').write_text(json.dumps(evidence, indent=2) + '\n')
    print(f'Decor replacement and scale {factor} ownership checks passed')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(allow_abbrev=False)
    for name in ('fvwm', 'mfl', 'worker', 'artifacts-dir'):
        parser.add_argument('--' + name, type=Path, required=True)
    parser.add_argument('--real', action='store_true')
    options = parser.parse_args()
    for key, value in vars(options).items():
        if isinstance(value, Path):
            setattr(options, key, value.resolve())
    for scale in (1, 2):
        run(options, scale)
