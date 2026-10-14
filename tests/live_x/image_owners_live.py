#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
"""Exercise ordinary colorset image ownership and rejected shape cleanup."""
import argparse
import ctypes as C
import json
from pathlib import Path
import shutil
import struct
import time
import zlib

from live_x_harness import HarnessRuntime, start_display_server, wait_for_predicate
from mfl_stream_live import Connection
from geometry_transition_live import Client, Pointer


def png():
    def chunk(kind, content):
        checksum = zlib.crc32(kind + content) & 0xffffffff
        return struct.pack('>I', len(content)) + kind + content + struct.pack('>I', checksum)

    # An ordinary 2x2 RGB PNG has no alpha/mask and cannot serve as a shape.
    return (b'\x89PNG\r\n\x1a\n' +
        chunk(b'IHDR', struct.pack('>IIBBBBB', 2, 2, 8, 2, 0, 0, 0)) +
        chunk(b'IDAT', zlib.compress(b'\0\xff\0\0\xff\0\0' * 2)) +
        chunk(b'IEND', b''))


def run(args):
    args.artifacts_dir.mkdir(parents=True, exist_ok=True)
    evidence = {}
    with HarnessRuntime(prefix='fvwm-image-owners-') as runtime:
        connection = None
        core = None
        client = None
        try:
            display = args.display
            if display is None:
                _, display = start_display_server(runtime, 'Xvfb',
                    ['Xvfb', '-screen', '0', '800x600x24', '-noreset'],
                    'xserver.log', time.monotonic() + 10)
            image = runtime.root / 'opaque.png'
            image.write_bytes(png())
            shape = runtime.root / 'shape.xbm'
            shape.write_text('#define shape_width 2\n#define shape_height 2\n'
                'static unsigned char shape_bits[] = {\n 0x03, 0x03};\n')
            config = runtime.root / 'config'
            config.write_text(f'ModulePath {args.mfl.parent}:{args.probe.parent}\n'
                'Style * TileCascadePlacement\n'
                'Colorset 0 bg black\n'
                'AddToFunc StartFunction I Module FvwmMFL\n')
            core = runtime.start('core', [str(args.fvwm), '-d', display,
                '-f', str(config), '-v', '-o', str(runtime.root / 'debug.log')],
                'core.log', runtime.environment({'FVWMMFL_SOCKET_PATH': str(runtime.root)}))
            endpoint = runtime.root / f"fvwm_mfl_{display.replace(':', '_')}.sock"
            wait_for_predicate('image owner endpoint', endpoint.exists,
                time.monotonic() + 10, process=core)
            connection = Connection(endpoint)
            connection.record()
            connection.send(b'set echo\n')
            connection.echo('image-owners-ready')
            # First reject an uncached image, then reject a cached image shared
            # with a background. Both paths must release exactly one reference.
            connection.send(f'Colorset 0 Shape {image}\n'.encode())
            connection.echo('after-first-shape-refusal')
            evidence['uncached-opaque-shape-refusal'] = 'passed'
            connection.send(f'Colorset 0 Pixmap {image}\n'.encode())
            connection.echo('background-loaded')
            connection.send(f'Colorset 0 Shape {image}\n'.encode())
            connection.echo('after-shared-shape-refusal')
            evidence['shared-background-survives-shape-refusal'] = 'passed'
            # Ordinary valid shape/clear/background reload must still work.
            for i in range(8):
                connection.send((f'Colorset 0 Shape {shape}\n'
                    f'Colorset 0 NoShape, Plain\n'
                    f'Colorset 0 Pixmap {image}\n').encode())
                connection.echo(f'owner-reload-{i}')
            evidence['valid-shape-clear-and-background-reload'] = 'passed'
            connection.send(f'Style * MiniIcon {image}\n'.encode())
            connection.echo('mini-icon-style-ready')
            client = Client(display)
            client.create()
            wait_for_predicate('mini-icon client frame',
                lambda: client.frame() is not None,
                time.monotonic() + 10, process=core)
            report = runtime.root / 'mini-icons.txt'
            connection.send(f'Module {args.probe.name} {report}\n'.encode())

            def packets():
                if not report.exists():
                    return []
                return [tuple(map(int, line.split()))
                    for line in report.read_text().splitlines(keepends=True)
                    if line.endswith('\n') and len(line.split()) == 4]

            wait_for_predicate('initial mini-icon packet',
                lambda: len(packets()) >= 1, time.monotonic() + 5, process=core)
            first = packets()[0]
            assert first[0] == client.window and first[1] != 0
            for i in range(3):
                count = len(packets())
                connection.send(f'Style * MiniIcon {image}\n'.encode())
                connection.echo(f'same-key-mini-icon-{i}')
                wait_for_predicate('same-key mini-icon update',
                    lambda: len(packets()) > count,
                    time.monotonic() + 5, process=core)
                assert packets()[-1] == first, packets()
            evidence['same-key-mini-icon-retains-pixmap'] = 'passed'
            pointer = Pointer(client)
            client.x.XStringToKeysym.argtypes = [C.c_char_p]
            client.x.XStringToKeysym.restype = C.c_ulong
            client.x.XKeysymToKeycode.argtypes = [C.c_void_p, C.c_ulong]
            client.x.XKeysymToKeycode.restype = C.c_ubyte
            pointer.x.XTestFakeKeyEvent.argtypes = [C.c_void_p, C.c_uint,
                C.c_int, C.c_ulong]
            pointer.x.XTestFakeKeyEvent.restype = C.c_int
            code = client.x.XKeysymToKeycode(client.display,
                client.x.XStringToKeysym(b'Escape'))
            assert code
            for i in range(4):
                connection.send(b'WindowList\n')
                wait_for_predicate('WindowList snapshot mapped', pointer.grabbed,
                    time.monotonic() + 5, process=core)
                for pressed in (1, 0):
                    assert pointer.x.XTestFakeKeyEvent(client.display,
                        code, pressed, 0)
                client.x.XSync(client.display, 0)
                wait_for_predicate('WindowList snapshot released',
                    lambda: not pointer.grabbed(),
                    time.monotonic() + 5, process=core)
                next_icon = shape if i % 2 == 0 else image
                connection.send(f'Style * MiniIcon {next_icon}\n'.encode())
                connection.echo(f'windowlist-mini-icon-reload-{i}')
            evidence['windowlist-mini-icon-snapshot-release-and-reload'] = 'passed'
            log = (runtime.root / 'debug.log').read_text()
            assert log.count('shape pixmap must be of depth 1') == 2, log
            assert core.poll() is None
        except BaseException as error:
            evidence['error'] = str(error)
            evidence['wm_exit_status'] = core.poll() if core else None
            raise
        finally:
            if client:
                client.close()
            if connection:
                connection.close()
            packet_report = runtime.root / 'mini-icons.txt'
            if packet_report.exists():
                shutil.copy2(packet_report, args.artifacts_dir / packet_report.name)
            for path in runtime.root.glob('*.log'):
                shutil.copy2(path, args.artifacts_dir / path.name)
            (args.artifacts_dir / 'evidence.json').write_text(
                json.dumps(evidence, indent=2) + '\n')
    print('Colorset image owner refusal and cleanup checks passed')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(allow_abbrev=False)
    for name in ('fvwm', 'mfl', 'probe', 'artifacts-dir'):
        parser.add_argument('--' + name, type=Path, required=True)
    parser.add_argument('--display')
    args = parser.parse_args()
    for name in ('fvwm', 'mfl', 'probe', 'artifacts_dir'):
        setattr(args, name, getattr(args, name).resolve())
    run(args)
