#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
"""Check production decoder handoffs and live core recovery on owned fixtures."""
import argparse
import gzip
import json
import os
from pathlib import Path
import shutil
import socket
import struct
import subprocess
import tempfile
import time
import zlib

from geometry_transition_live import Client
from live_x_harness import (HarnessError, HarnessRuntime, start_display_server,
                            wait_for_predicate)
from mfl_stream_live import Connection
from resource_cache_live import chunk


def png(color_type, depth, row, extra=b''):
    return (b'\x89PNG\r\n\x1a\n'
            + chunk(b'IHDR', struct.pack('>IIBBBBB', 2, 2, depth,
                                        color_type, 0, 0, 0))
            + extra + chunk(b'IDAT', zlib.compress((b'\0' + row) * 2))
            + chunk(b'IEND', b''))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--probe', required=True)
    parser.add_argument('--fvwm', required=True)
    parser.add_argument('--mfl', type=Path, required=True)
    parser.add_argument('--display')
    parser.add_argument('--artifacts-dir', type=Path, required=True)
    args = parser.parse_args()
    features = json.loads(subprocess.check_output(
        [args.probe, '--features'], text=True))
    args.artifacts_dir.mkdir(parents=True, exist_ok=True)
    artifacts = Path(tempfile.mkdtemp(prefix='run-', dir=args.artifacts_dir))
    results = {}
    core_formats = []
    with HarnessRuntime(prefix='fvwm-image-input-') as runtime:
        connection = client = listener = None
        try:
            display = args.display
            if display is None:
                _, display = start_display_server(
                    runtime, 'Xvfb', ['Xvfb', '-screen', '0', '800x600x24',
                                     '-noreset'], 'xserver.log',
                    time.monotonic() + 15)

            def probe(name, *arguments):
                process = runtime.start(
                    name, [args.probe, *map(str, arguments)], name + '.log',
                    runtime.environment({'DISPLAY': display}))
                if process.wait_until_exit(time.monotonic() + 15) != 0:
                    raise HarnessError(f'{name} failed: '
                                       + process.log_path.read_text())
                results[name] = 'passed'

            def fixture(name, data, kind, expected):
                path = runtime.root / name
                path.write_bytes(data)
                core_formats.append(path)
                probe(name + '-pixels', 'load', path, kind, expected)
                replaced = runtime.root / ('swap-' + name)
                replaced.write_bytes(data)
                probe(name + '-opened-inode', 'swap', replaced, kind, expected)
                return path

            bitmap = fixture('image.xbm', (
                '#define image_width 2\n#define image_height 2\n'
                'static unsigned char image_bits[] = {\n0x03,0x03};\n'
            ).encode(), 'bitmap', '0')
            fixture('image-x10.xbm', (
                '#define image_width 2\n#define image_height 2\n'
                'static short image_bits[] = {\n0x0003,0x0003};\n'
            ).encode(), 'bitmap', '0')
            symlink = runtime.root / 'link.xbm'
            symlink.symlink_to(bitmap.name)
            probe('regular-symlink', 'load', symlink, 'bitmap', '0')
            good = bitmap
            if features['png']:
                good = fixture('rgb.png', png(2, 8, b'\xff\0\0' * 2),
                               'color', 'ffff0000')
                fixture('rgba.png', png(6, 8, b'\xff\0\0\x80' * 2),
                        'color', '80ff0000')
                fixture('gray.png', png(0, 8, b'\x80' * 2),
                        'color', 'ff808080')
                fixture('gray16.png', png(0, 16, b'\x80\0' * 2),
                        'color', 'ff808080')
                fixture('gray1.png', png(0, 1, b'\xc0'),
                        'color', 'ffffffff')
                fixture('gray-alpha.png', png(4, 8, b'\x80\x80' * 2),
                        'color', '80808080')
                fixture('palette.png', png(3, 8, b'\0\0',
                        chunk(b'PLTE', b'\xff\0\0')
                        + chunk(b'tRNS', b'\x80')), 'color', '80ff0000')
                probe('image-cursor', 'cursor', good)
            if features['xpm']:
                xpm = (b'/* XPM */\nstatic char *image[] = {\n'
                       b'"2 2 2 1 0 1",\n"r c #ff0000",\n'
                       b'". c None",\n"rr",\n"r."};\n')
                path = fixture('image.xpm', xpm, 'color', 'ffff0000')
                probe('xpm-cursor', 'cursor', path, 'hotspot')
                if features['gzip']:
                    compressed = gzip.compress(xpm, mtime=0)
                    for suffix in ['.gz', '.Z']:
                        path = fixture('compressed.xpm' + suffix, compressed,
                                       'color', 'ffff0000')
                        probe('compressed-cursor' + suffix, 'cursor', path,
                              'hotspot')
                    compress = shutil.which('compress')
                    if compress:
                        fixture('unix-compress.xpm.Z', subprocess.check_output(
                            [compress, '-c'], input=xpm), 'color', 'ffff0000')
                    # These names are absent: libXpm's legacy suffix lookup
                    # must continue to work without bypassing acquisition.
                    base = runtime.root / 'bare.xpm'
                    Path(str(base) + '.gz').write_bytes(compressed)
                    probe('bare-compressed-name', 'load', base,
                          'color', 'ffff0000')
                    expanded = runtime.root / 'expanded-limit.xpm.gz'
                    encoder = zlib.compressobj(wbits=31)
                    with expanded.open('wb') as stream:
                        remaining = features['expanded_limit'] + 1
                        block = b' ' * 1048576
                        while remaining:
                            count = min(remaining, len(block))
                            stream.write(encoder.compress(block[:count]))
                            remaining -= count
                        stream.write(encoder.flush())
                    probe('compressed-expanded-limit-refusal',
                          'refuse-pixmap', expanded)
            if features['svg']:
                svg = (
                    '<svg xmlns="http://www.w3.org/2000/svg" '
                    'width="2" height="2"><rect width="2" height="2" '
                    'fill="red"/></svg>').encode()
                path = fixture('image.svg', svg, 'color', 'ffff0000')
                probe('svg-render-options', 'load', f':2x2:{path}',
                      'color', 'ffff0000')
                fixture('image.svgz', gzip.compress(svg, mtime=0),
                        'color', 'ffff0000')
                if features['png']:
                    fixture('relative.svg', (
                        '<svg xmlns="http://www.w3.org/2000/svg" '
                        'xmlns:xlink="http://www.w3.org/1999/xlink" '
                        'width="2" height="2"><image xlink:href="rgb.png" '
                        'width="2" height="2"/></svg>').encode(),
                        'color', 'ffff0000')
            if features['xcursor']:
                image_type = 0xfffd0002
                cursor = struct.pack('<4sIII', b'Xcur', 16, 0x10000, 2)
                cursor += struct.pack('<IIIIII', image_type, 24, 40,
                                      image_type, 24, 92)
                for index, color in enumerate([0xffff0000, 0xff0000ff]):
                    cursor += struct.pack('<IIIIIIIII', 36, image_type, 24,
                                          1, 2, 2, 0, 1, 100 * (index + 1))
                    cursor += struct.pack('<IIII', *([color] * 4))
                path = runtime.root / 'animated.cursor'
                path.write_bytes(cursor)
                probe('animated-cursor-metadata', 'cursor', path, 'animated')

            fifo = runtime.root / 'image-fifo.png'
            os.mkfifo(fifo, 0o600)
            link_fifo = runtime.root / 'fifo-link.xpm'
            link_fifo.symlink_to(fifo.name)
            directory = runtime.root / 'image-directory.svg'
            directory.mkdir()
            endpoint = runtime.root / 'image-socket.xbm'
            listener = socket.socket(socket.AF_UNIX)
            listener.bind(str(endpoint))
            oversize = runtime.root / 'image-oversize.png'
            with oversize.open('wb') as stream:
                stream.truncate(features['limit'] + 1)
            incomplete = runtime.root / 'incomplete.png'
            incomplete.write_bytes(b'\x89PNG\r\n\x1a\n')
            refused = [fifo, link_fifo, directory, endpoint, oversize,
                       incomplete, Path('/dev/null')]
            probe('special-input-output-reset-and-descriptors',
                  'refuse', *refused)

            config = runtime.root / 'config'
            config.write_text(f'ModulePath {args.mfl.parent}\n'
                              'Style * TileCascadePlacement\n'
                              'AddToFunc StartFunction I Module FvwmMFL\n')
            core = runtime.start(
                'core', [args.fvwm, '-d', display, '-f', str(config), '-v',
                         '-o', str(runtime.root / 'core-debug.log')],
                'core.log', runtime.environment(
                    {'FVWMMFL_SOCKET_PATH': str(runtime.root)}))
            mfl_endpoint = runtime.root / (
                f"fvwm_mfl_{display.replace(':', '_')}.sock")
            wait_for_predicate('image-input endpoint', mfl_endpoint.exists,
                               time.monotonic() + 10, process=core)
            connection = Connection(mfl_endpoint)
            connection.record()
            connection.send(b'set echo\n')
            connection.echo('image-input-ready')
            # Exercise the real WM after librsvg has used its relative-resource
            # path, then return to compression with the WM reaper installed.
            compressed = runtime.root / 'compressed.xpm.gz'
            if compressed.exists():
                core_formats.append(compressed)
            for index, path in enumerate(core_formats):
                connection.send(f'Colorset 7 Pixmap {path}\n'.encode())
                connection.echo(f'valid-format-{index}')
                results[f'core-valid-format-{index}'] = 'passed'
            for index, path in enumerate(refused):
                connection.send(f'Colorset 7 Pixmap {path}\n'.encode())
                connection.echo(f'refused-{index}')
                connection.send(f'Colorset 7 Pixmap {good}\n'.encode())
                connection.echo(f'recovered-{index}')
                results[f'core-refusal-and-recovery-{index}'] = 'passed'
            client = Client(display)
            client.create()
            wait_for_predicate('client after refused images',
                               lambda: client.frame() is not None,
                               time.monotonic() + 10, process=core)
            results['core-manages-later-client'] = 'passed'
        finally:
            if connection:
                connection.close()
            if client:
                client.close()
            if listener:
                listener.close()
            for log in runtime.root.glob('*.log'):
                shutil.copyfile(log, artifacts / log.name)
            (artifacts / 'results.json').write_text(json.dumps(
                {'features': features, 'checks': results}, indent=2) + '\n')
    print(f'{len(results)} production image handoff and recovery checks passed')


if __name__ == '__main__':
    main()
