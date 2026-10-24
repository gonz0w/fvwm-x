#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
"""Independent PNG/RGB, private-file and refusal checks for native capture."""

import argparse
import ctypes as C
import hashlib
import json
import os
from pathlib import Path
import resource
import stat
import struct
import subprocess
import time
import zlib

from desktop_live import connect_client
from live_x_harness import HarnessRuntime, start_display_server


def png_pixels(data):
    assert data.startswith(b'\x89PNG\r\n\x1a\n')
    offset, compressed, dimensions = 8, bytearray(), None
    ended = False
    while offset < len(data):
        length = struct.unpack_from('!I', data, offset)[0]
        kind = data[offset + 4:offset + 8]
        payload = data[offset + 8:offset + 8 + length]
        assert len(payload) == length
        crc = struct.unpack_from('!I', data, offset + 8 + length)[0]
        assert zlib.crc32(kind + payload) & 0xffffffff == crc
        if kind == b'IHDR':
            width, height, depth, color, compression, filtering, interlace = struct.unpack('!IIBBBBB', payload)
            assert (depth, color, compression, filtering, interlace) == (8, 2, 0, 0, 0)
            dimensions = (width, height)
        elif kind == b'IDAT':
            compressed.extend(payload)
        elif kind == b'IEND':
            assert length == 0
            ended = True
        offset += length + 12
    assert ended and offset == len(data) and dimensions
    width, height = dimensions
    raw = zlib.decompress(compressed)
    stride = width * 3
    assert len(raw) == (stride + 1) * height
    previous = bytes(stride)
    pixels = bytearray()
    for y in range(height):
        base = y * (stride + 1)
        filtering, row = raw[base], bytearray(raw[base + 1:base + 1 + stride])
        assert filtering in range(5)
        for x in range(stride):
            left = row[x - 3] if x >= 3 else 0
            up, corner = previous[x], previous[x - 3] if x >= 3 else 0
            if filtering == 1:
                predictor = left
            elif filtering == 2:
                predictor = up
            elif filtering == 3:
                predictor = (left + up) // 2
            elif filtering == 4:
                value = left + up - corner
                distances = [abs(value - n) for n in (left, up, corner)]
                predictor = (left, up, corner)[distances.index(min(distances))]
            else:
                predictor = 0
            row[x] = (row[x] + predictor) & 255
        pixels.extend(row)
        previous = row
    return dimensions, bytes(pixels)


def run(options):
    artifact = options.artifacts_dir.resolve()
    artifact.mkdir(parents=True, exist_ok=True)
    executable = str(options.capture.resolve())
    evidence = {'executable_sha256': hashlib.sha256(Path(executable).read_bytes()).hexdigest()}
    with HarnessRuntime(prefix='fvwm-native-capture-') as runtime:
        for depth, color, expected in ((16, 0xf800, b'\xff\0\0'),
                (24, 0x224466, b'\x22\x44\x66'),
                (30, 0x3ff80000, b'\xff\x7f\0')):
            _, display = start_display_server(runtime, 'capture-' + str(depth),
                ['Xvfb', '-screen', '0', '64x48x' + str(depth), '-noreset'],
                'capture-' + str(depth) + '.log', time.monotonic() + 10)
            env = runtime.environment({'DISPLAY': display})
            client = connect_client(display)
            try:
                client.x.XSetWindowBackground.argtypes = [C.c_void_p, C.c_ulong, C.c_ulong]
                client.x.XClearWindow.argtypes = [C.c_void_p, C.c_ulong]
                client.x.XSetWindowBackground(client.display, client.root, color)
                client.x.XClearWindow(client.display, client.root)
                client.x.XSync(client.display, 0)
                directory = artifact / ('depth-' + str(depth)) / "nested output's space"
                result = subprocess.run([executable, str(directory)], env=env,
                    capture_output=True, text=True, timeout=10)
                assert result.returncode == 0, result.stderr
                filename = Path(result.stdout.rstrip('\n').removeprefix('Screenshot saved: '))
                assert filename.parent == directory and filename.suffix == '.png'
                assert stat.S_IMODE(filename.stat().st_mode) == 0o600
                dimensions, pixels = png_pixels(filename.read_bytes())
                assert dimensions == (64, 48) and pixels == expected * (64 * 48), (depth, pixels[:3])
                original = filename.read_bytes()
                again = subprocess.run([executable, str(directory)], env=env,
                    capture_output=True, text=True, timeout=10)
                assert again.returncode == 0 and again.stdout != result.stdout
                assert filename.read_bytes() == original and len(list(directory.glob('*.png'))) == 2
                evidence['rgb-' + str(depth)] = {'status': 'passed', 'dimensions': dimensions,
                    'rgb': list(expected), 'unique_private_files': True}
                if depth == 24:
                    failed = artifact / 'failed-write'
                    def quota():
                        resource.setrlimit(resource.RLIMIT_FSIZE, (32, 32))
                    refused = subprocess.run([executable, str(failed)], env=env,
                        preexec_fn=quota, capture_output=True, text=True, timeout=10)
                    assert refused.returncode == 1 and not list(failed.glob('*.png')), refused
                    assert 'Screenshot saved:' not in refused.stdout
                    occupied = artifact / 'occupied'
                    occupied.write_text('preserve me')
                    refused = subprocess.run([executable, str(occupied)], env=env,
                        capture_output=True, text=True, timeout=10)
                    assert refused.returncode == 1 and occupied.read_text() == 'preserve me'
                    evidence['write_refusal_cleans_partial_and_preserves_existing'] = 'passed'
            finally:
                client.close()
        for args in ([], [''], ['x' * 4097], ['one', 'two']):
            result = subprocess.run([executable, *args], capture_output=True, text=True, timeout=5)
            assert result.returncode == 2 and not result.stdout, result
        missing = artifact / 'missing-display'
        result = subprocess.run([executable, str(missing)],
            env=dict(os.environ, DISPLAY=':65534'), capture_output=True, text=True, timeout=5)
        assert result.returncode == 1 and not missing.exists() and 'Cannot open' in result.stderr
        _, display = start_display_server(runtime, 'indexed',
            ['Xvfb', '-screen', '0', '64x48x8', '-noreset'],
            'indexed.log', time.monotonic() + 10)
        result = subprocess.run([executable, str(artifact / 'indexed')],
            env=runtime.environment({'DISPLAY': display}), capture_output=True, text=True, timeout=5)
        assert result.returncode == 1 and 'TrueColor' in result.stderr and not result.stdout
        evidence['argument_display_and_indexed_refusals'] = 'passed'
    (artifact / 'evidence.json').write_text(json.dumps(evidence, indent=2) + '\n')
    print(json.dumps(evidence))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--capture', type=Path, required=True)
    parser.add_argument('--artifacts-dir', type=Path, required=True)
    run(parser.parse_args())
