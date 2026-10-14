#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
"""Observe deferred colorset publication in unchanged core objects."""
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



class XColor(C.Structure):
    _fields_ = [('pixel', C.c_ulong), ('red', C.c_ushort),
        ('green', C.c_ushort), ('blue', C.c_ushort),
        ('flags', C.c_char), ('pad', C.c_char)]


def records(path):
    if not path.exists():
        return []
    return [tuple(int(value, 16) for value in line.split()[1:])
        for line in path.read_text().splitlines(keepends=True)
        if line.endswith('\n') and line.startswith('Colorset ')]


def run(args):
    args.artifacts_dir.mkdir(parents=True, exist_ok=True)
    evidence = {}
    with HarnessRuntime(prefix='fvwm-image-colorsets-') as runtime:
        connection = client = core = None
        try:
            display = args.display
            if display is None:
                _, display = start_display_server(runtime, 'Xvfb',
                    ['Xvfb', '-screen', '0', f'800x600x{args.depth}', '-noreset'],
                    'xserver.log', time.monotonic() + 10)
            image = runtime.root / 'opaque.png'
            image.write_bytes(png())
            shape = runtime.root / 'shape.xbm'
            shape.write_text('#define shape_width 2\n#define shape_height 2\n'
                'static unsigned char shape_bits[] = {\n 0x03, 0x03};\n')
            sources = {}
            for name, content in [('stall', 'red'), ('red', 'red'),
                    ('blue', 'blue'), ('transparent', 'transparent'),
                    ('incomplete', 'red'), ('wide', 'red'),
                    ('half', 'half'), ('empty', 'empty'), ('large', 'transparent')]:
                sources[name] = runtime.root / (name + '.svg')
                sources[name].write_text(content)
            config = runtime.root / 'config'
            config.write_text(f'ModulePath {args.mfl.parent}:{args.probe.parent}\n'
                'Style * TileCascadePlacement\nColorset 0 bg black\n'
                'AddToFunc StartFunction I Module FvwmMFL\n')
            core = runtime.start('core', [str(args.fvwm), '-d', display,
                '-f', str(config), '-v', '-o', str(runtime.root / 'debug.log')],
                'core.log', runtime.environment({
                    'FVWMMFL_SOCKET_PATH': str(runtime.root),
                    'FVWM_IMAGE_SVG_WORKER': str(args.worker)}))
            endpoint = runtime.root / f"fvwm_mfl_{display.replace(':', '_')}.sock"
            wait_for_predicate('colorset fixture endpoint', endpoint.exists,
                time.monotonic() + 10, process=core)
            connection = Connection(endpoint)
            connection.record()
            connection.send(b'set echo\n')
            connection.echo('colorset-ready')
            client = Client(display)
            errors = []
            callback_type = C.CFUNCTYPE(C.c_int, C.c_void_p, C.c_void_p)

            @callback_type
            def x_error(_display, _event):
                errors.append('X request failed')
                return 0

            client.x.XSetErrorHandler.argtypes = [callback_type]
            client.x.XSetErrorHandler.restype = C.c_void_p
            client.x.XSetErrorHandler(x_error)
            client.x.XDefaultColormap.argtypes = [C.c_void_p, C.c_int]
            client.x.XDefaultColormap.restype = C.c_ulong
            client.x.XQueryColor.argtypes = [C.c_void_p, C.c_ulong,
                C.POINTER(XColor)]
            client.x.XQueryColor.restype = C.c_int

            def rgb(pixel):
                color = XColor(pixel=pixel)
                assert client.x.XQueryColor(client.display,
                    client.x.XDefaultColormap(client.display, 0), C.byref(color))
                assert not errors, errors
                return (color.red, color.green, color.blue)

            client.x.XGetImage.argtypes = [C.c_void_p, C.c_ulong,
                C.c_int, C.c_int, C.c_uint, C.c_uint, C.c_ulong, C.c_int]
            client.x.XGetImage.restype = C.c_void_p
            client.x.XGetPixel.argtypes = [C.c_void_p, C.c_int, C.c_int]
            client.x.XGetPixel.restype = C.c_ulong
            client.x.XDestroyImage.argtypes = [C.c_void_p]

            def pixmap_rgb(pixmap):
                image = client.x.XGetImage(client.display, pixmap,
                    0, 0, 1, 1, C.c_ulong(-1).value, 2)
                assert image and not errors, errors
                try:
                    return rgb(client.x.XGetPixel(image, 0, 0))
                finally:
                    client.x.XDestroyImage(image)

            sequence = 0

            def command(text):
                nonlocal sequence
                sequence += 1
                connection.send((text + '\n').encode())
                connection.echo(f'colorset-barrier-{sequence}')

            def snapshot(identifier):
                nonlocal sequence
                sequence += 1
                report = runtime.root / f'snapshot-{sequence}.txt'
                command(f'Module {args.probe.name} colorsets {report}')
                wait_for_predicate('colorset snapshot',
                    lambda: any(row[0] == identifier for row in records(report)),
                    time.monotonic() + 5, process=core)
                return next(row for row in records(report) if row[0] == identifier)

            def geometry(pixmap, depth, size=(2, 2)):
                root = C.c_ulong()
                x, y = C.c_int(), C.c_int()
                width, height, border, actual = (C.c_uint() for _ in range(4))
                assert client.x.XGetGeometry(client.display, pixmap,
                    C.byref(root), C.byref(x), C.byref(y), C.byref(width),
                    C.byref(height), C.byref(border), C.byref(actual))
                assert not errors, (pixmap, errors)
                assert (width.value, height.value, actual.value) == (*size, depth)

            report = runtime.root / 'updates.txt'
            command(f'Module {args.probe.name} colorsets {report}')
            wait_for_predicate('initial colorset packet',
                lambda: bool(records(report)), time.monotonic() + 5, process=core)

            def await_change(identifier, previous, field=8, after=0):
                def ready():
                    rows = [row for row in records(report)[after:]
                        if row[0] == identifier]
                    return bool(rows) and rows[-1][field] != previous

                wait_for_predicate('completed colorset publication', ready,
                    time.monotonic() + 8, process=core)
                return [row for row in records(report) if row[0] == identifier][-1]

            def published_image(identifier):
                row = snapshot(identifier)
                return row if row[8] != 0 else await_change(identifier, 0)

            command(f'Colorset 1 bg black, Pixmap {sources["stall"]}')
            assert snapshot(1)[8] == 0
            command('Colorset 1 Plain')
            evidence['first-load-absent-and-independent-command-service'] = 'passed'

            command(f'Colorset 2 TiledPixmap {image}')
            old = published_image(2)
            geometry(old[8], args.depth)
            command(f'Colorset 2 AspectPixmap {sources["stall"]}')
            pending = snapshot(2)
            assert pending[8:17] == old[8:17], (pending, old)
            geometry(old[8], args.depth)
            command(f'Colorset 2 AspectPixmap {sources["blue"]}')
            ready = await_change(2, old[8])
            assert ready[11:14] == (2, 2, 4), ready
            geometry(ready[8], args.depth)
            # Modules can still use the old packet before delayed cleanup.
            geometry(old[8], args.depth)
            evidence['replacement-retains-published-pixmap-and-type'] = 'passed'
            evidence['superseded-active-worker-releases-next-job'] = 'passed'

            command(f'Colorset 2 Pixmap {sources["incomplete"]}')
            wait_for_predicate('decoder refusal',
                lambda: 'SVG refused:' in (runtime.root / 'debug.log').read_text(),
                time.monotonic() + 8, process=core)
            assert snapshot(2)[8:17] == ready[8:17]
            evidence['failed-replacement-keeps-old-public-resources'] = 'passed'

            command(f'Colorset 3 AspectShape {shape}')
            old_shape = snapshot(3)
            geometry(old_shape[9], 1)
            command(f'Colorset 3 TiledShape {sources["stall"]}')
            assert snapshot(3)[9:17] == old_shape[9:17]
            geometry(old_shape[9], 1)
            command(f'Colorset 3 TiledShape {sources["transparent"]}')
            new_shape = await_change(3, old_shape[9], 9)
            assert new_shape[14:17] == (2, 2, 0), new_shape
            geometry(new_shape[9], 1)
            geometry(old_shape[9], 1)
            command(f'Colorset 3 Shape {sources["red"]}')
            wait_for_predicate('opaque async shape refusal',
                lambda: 'shape pixmap must be of depth 1' in
                    (runtime.root / 'debug.log').read_text(),
                time.monotonic() + 8, process=core)
            assert snapshot(3)[9:17] == new_shape[9:17]
            evidence['shape-replacement-retains-mask-dimensions-and-type'] = 'passed'
            evidence['opaque-async-shape-refusal-preserves-old-mask'] = 'passed'
            command(f'Colorset 3 Shape {sources["stall"]}')
            command('Colorset 3 NoShape')
            assert snapshot(3)[9] == 0
            evidence['no-shape-cancels-pending-interest'] = 'passed'

            # Two public shapes can borrow one acknowledged cache mask.
            # Clearing either owner must never free the other's resource.
            command(f'Colorset 12 Shape {sources["transparent"]}')
            command(f'Colorset 13 AspectShape {sources["transparent"]}')
            shared = snapshot(12)
            assert snapshot(13)[9] == shared[9], (snapshot(13), shared)
            assert shared[9] == new_shape[9], (shared, new_shape)
            command('Colorset 12 NoShape')
            command('CleanupColorsets')
            geometry(shared[9], 1)
            assert snapshot(13)[9] == shared[9]
            command(f'Colorset 13 Shape {sources["incomplete"]}')
            command('Colorset 13 NoShape')
            command('CleanupColorsets')
            assert snapshot(13)[9] == 0
            evidence['shared-shape-backing-survives-other-owner-cleanup'] = 'passed'
            evidence['shape-borrows-acknowledged-cache-mask-without-copy'] = 'passed'

            command(f'Colorset 4 Pixmap {sources["stall"]}')
            command(f'Colorset 5 Pixmap {sources["red"]}')
            command('Colorset 5 HGradient 2 red blue')
            gradient = snapshot(5)
            command('Colorset 4 Plain')
            command(f'Colorset 6 Pixmap {sources["blue"]}')
            await_change(6, 0)
            assert snapshot(5)[8:17] == gradient[8:17]
            evidence['gradient-override-cancels-queued-interest'] = 'passed'

            command(f'Colorset 7 Pixmap {image}, Tint green 50')
            prior = published_image(7)
            command(f'Colorset 7 Pixmap {sources["stall"]}, Tint blue 50')
            assert snapshot(7)[8] != 0
            count = len(records(report))
            command(f'Colorset 7 Pixmap {sources["wide"]}, NoTint, NoDither')
            wait_for_predicate('final SVG dimensions',
                lambda: any(row[0] == 7 and row[11:13] == (4, 2)
                    for row in records(report)[count:]),
                time.monotonic() + 8, process=core)
            final = [row for row in records(report) if row[0] == 7][-1]
            assert final[17] == 0
            evidence['pending-image-keeps-final-tint-and-dither-policy'] = 'passed'
            # Independent server colors check the approved source-RGB policy
            # for both ordinary and deferred images, without image readback.
            command(f'Colorset 14 bg Average, Pixmap {sources["blue"]}')
            opaque = snapshot(14)
            if opaque[8] == 0:
                opaque = await_change(14, 0)
            command(f'Colorset 15 bg Average, Pixmap {sources["transparent"]}')
            masked = snapshot(15)
            if masked[8] == 0:
                masked = await_change(15, 0)
            command(f'Colorset 16 bg Average, Pixmap {image}')
            native = published_image(16)
            assert rgb(opaque[2]) == (0, 0, 65535), opaque
            assert rgb(native[2]) == (65535, 0, 0), native
            assert rgb(masked[2]) == (0, 0, 65535), masked
            evidence['decoded-average-rgb-control'] = {
                'opaque-deferred': rgb(opaque[2]),
                'opaque-native-png': rgb(native[2]),
                'binary-mask-deferred': rgb(masked[2])}

            command(f'Colorset 17 bg Average, Pixmap {sources["half"]}')
            partial = snapshot(17)
            if partial[8] == 0:
                partial = await_change(17, 0)
            assert rgb(partial[2]) == (32896, 49344, 16448), partial
            command('Colorset 17 Tint red 50')
            assert rgb(snapshot(17)[2]) == rgb(partial[2])
            command(f'Colorset 18 bg Average, Pixmap {sources["empty"]}')
            empty = snapshot(18)
            if empty[8] == 0:
                empty = await_change(18, 0)
            assert rgb(empty[2]) == (65535, 65535, 65535), empty
            command(f'Colorset 19 bg green, Pixmap {image}')
            assert rgb(published_image(19)[2]) == (0, 65535, 0)
            command(f'Colorset 20 bg Average, Pixmap {sources["blue"]}')
            shared_average = snapshot(20)
            if shared_average[8] == 0:
                shared_average = await_change(20, 0)
            assert rgb(shared_average[2]) == rgb(opaque[2])
            evidence['source-average-partial-alpha-not-premultiplied'] = 'passed'
            evidence['source-average-empty-white-explicit-bg-and-tint'] = 'passed'
            evidence['source-average-shared-cache-consistency'] = 'passed'

            command(f'Colorset 21 bg black, Pixmap {sources["large"]}')
            large = await_change(21, 0)
            assert large[11:13] == (4096, 2), large
            geometry(large[8], args.depth, (4096, 2))
            command(f'Colorset 22 Shape {sources["large"]}')
            large_shape = await_change(22, 0, 9)
            assert large_shape[14:16] == (4096, 2), large_shape
            geometry(large_shape[9], 1, (4096, 2))
            evidence['4096-pixel-background-and-shape-dimensions'] = 'passed'

            # Observe actual effects before proposing source-RGB preparation.
            # The candidate here is scalar arithmetic, never a WM mutation.
            command(f'Colorset 23 bg black, Pixmap {image}, Tint green 50')
            tinted = published_image(23)
            current_tint = pixmap_rgb(tinted[8])
            candidate_tint = (127 * 257, 127 * 257, 0)
            command(f'Colorset 24 bg blue, Pixmap {image}, Tint green 50, '
                'ImageAlpha 50')
            composed = published_image(24)
            current_composed = pixmap_rgb(composed[8])
            candidate_composed = (63 * 257, 63 * 257, 128 * 257)
            evidence['legacy-effects-rgb-control'] = {
                'current-tint-50': current_tint,
                'candidate-source-tint-50': candidate_tint,
                'current-tint-50-image-alpha-50-over-blue': current_composed,
                'candidate-source-tint-50-image-alpha-50-over-blue':
                    candidate_composed}

            command('CleanupColorsets')
            for row in (ready, final):
                geometry(row[8], args.depth,
                    (4, 2) if row[0] == 7 else (2, 2))
            assert snapshot(3)[9] == 0
            evidence['delayed-cleanup-keeps-current-resources'] = 'passed'
            assert core.poll() is None
            log = (runtime.root / 'debug.log').read_text()
            assert 'X error' not in log, log
        except BaseException as error:
            evidence['error'] = str(error)
            evidence['wm_exit_status'] = core.poll() if core else None
            raise
        finally:
            if client:
                client.close()
            if connection:
                connection.close()
            for path in runtime.root.iterdir():
                if path.suffix in ('.txt', '.log'):
                    shutil.copy2(path, args.artifacts_dir / path.name)
            (args.artifacts_dir / 'evidence.json').write_text(
                json.dumps(evidence, indent=2) + '\n')
    print('Deferred colorset ownership and publication checks passed')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(allow_abbrev=False)
    for name in ('fvwm', 'mfl', 'probe', 'worker', 'artifacts-dir'):
        parser.add_argument('--' + name, type=Path, required=True)
    parser.add_argument('--display')
    parser.add_argument('--depth', type=int, choices=(8, 24), default=24)
    options = parser.parse_args()
    for key, value in vars(options).items():
        if isinstance(value, Path):
            setattr(options, key, value.resolve())
    run(options)
