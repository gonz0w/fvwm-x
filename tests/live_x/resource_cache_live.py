#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
"""Exercise production resource caches against a private X server."""
import argparse
from pathlib import Path
import shutil
import struct
import tempfile
import time
import zlib

from live_x_harness import HarnessError, HarnessRuntime, start_display_server


def chunk(kind, data):
    return (struct.pack('>I', len(data)) + kind + data
            + struct.pack('>I', zlib.crc32(kind + data)))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--probe', required=True)
    parser.add_argument('--artifacts-dir', type=Path, required=True)
    args = parser.parse_args()
    args.artifacts_dir.mkdir(parents=True, exist_ok=True)
    artifacts = Path(tempfile.mkdtemp(prefix='run-', dir=args.artifacts_dir))
    with HarnessRuntime(prefix='fvwm-resource-cache-') as runtime:
        png = runtime.root / 'picture.png'
        png.write_bytes(b'\x89PNG\r\n\x1a\n'
                        + chunk(b'IHDR', struct.pack(
                            '>IIBBBBB', 8, 8, 8, 6, 0, 0, 0))
                        + chunk(b'IDAT', zlib.compress(
                            (b'\x00' + b'\xff\x00\x00\xff' * 8) * 8))
                        + chunk(b'IEND', b''))
        # Ordinary incomplete files cover errors before/after pixel allocation
        # and while reading the end chunks. No advisory-specific input is used.
        valid = png.read_bytes()
        incomplete = [valid[:4], valid[:8], valid[:33],
                      valid[:-16], valid[:-12]]
        bad_crc = bytearray(valid)
        bad_crc[32] ^= 1
        incomplete.append(bytes(bad_crc))
        refused = []
        for index, data in enumerate(incomplete):
            path = runtime.root / f'incomplete-{index}.png'
            path.write_bytes(data)
            refused.append(str(path))
        bitmap = runtime.root / 'picture.xbm'
        bitmap.write_text('#define picture_width 8\n#define picture_height 8\n'
                          'static unsigned char picture_bits[] = {\n'
                          '0xff, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0xff};\n')
        try:
            _, display = start_display_server(
                runtime, 'Xvfb', ['Xvfb', '-screen', '0', '1280x720x24'],
                'xserver.log', time.monotonic() + 15)
            probe = runtime.start(
                'resource-cache', [args.probe, str(png), str(bitmap)] + refused,
                'probe.log', runtime.environment({'DISPLAY': display}))
            if probe.wait_until_exit(time.monotonic() + 30) != 0:
                raise HarnessError('resource-cache probe failed')
            print(probe.log_path.read_text())
        finally:
            for log in runtime.root.glob('*.log'):
                shutil.copyfile(log, artifacts / log.name)


if __name__ == '__main__':
    main()
