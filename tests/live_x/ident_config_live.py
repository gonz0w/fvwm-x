#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or any later version.
"""Verify short configuration lines advance in the actual Ident module."""
import argparse
import ctypes
import ctypes.util
import json
import os
from pathlib import Path
import selectors
import shutil
import signal
import struct
import time
from live_x_harness import HarnessRuntime, start_display_server


def run(args):
    args.artifacts_dir.mkdir(parents=True, exist_ok=True)
    results = {}
    with HarnessRuntime(prefix='fvwm-ident-config-') as runtime:
        try:
            display = args.display
            if display is None:
                _, display = start_display_server(runtime, 'Xvfb',
                    ['Xvfb', '-screen', '0', '800x600x24', '-noreset'],
                    'xserver.log', time.monotonic() + 10)
            library = ctypes.CDLL(ctypes.util.find_library('X11'))
            library.XOpenDisplay.argtypes = [ctypes.c_char_p]
            library.XOpenDisplay.restype = ctypes.c_void_p
            library.XDefaultRootWindow.argtypes = [ctypes.c_void_p]
            library.XDefaultRootWindow.restype = ctypes.c_ulong
            library.XCloseDisplay.argtypes = [ctypes.c_void_p]
            connection = library.XOpenDisplay(display.encode())
            assert connection
            root = library.XDefaultRootWindow(connection)
            library.XCloseDisplay(connection)
            word = struct.calcsize('@L')
            cases = {'empty': [''], 'single-byte': ['x'],
                     'whitespace': [' \t'], 'multiple': ['', 'x', ' ', '']}
            cases['configuration-term'] = []
            for label, lines in cases.items():
                incoming, producer = os.pipe()
                consumer, outgoing = os.pipe()
                try:
                    process = runtime.start(label, [str(args.module),
                        str(outgoing), str(incoming), '/dev/null',
                        f'{root:x}', '0'], label + '.log',
                        pass_fds=(incoming, outgoing),
                        environment=dict(os.environ, DISPLAY=display))
                    os.close(incoming)
                    incoming = -1
                    os.close(outgoing)
                    outgoing = -1
                    buffer = bytearray()
                    commands = []
                    os.set_blocking(consumer, False)

                    def receive(predicate):
                        deadline = time.monotonic() + 10
                        with selectors.DefaultSelector() as ready:
                            ready.register(consumer, selectors.EVENT_READ)
                            while not predicate():
                                remaining = deadline - time.monotonic()
                                assert remaining > 0 and ready.select(remaining)
                                data = os.read(consumer, 65536)
                                assert data, (label, commands)
                                buffer.extend(data)
                                while len(buffer) >= 2 * word:
                                    _, length = struct.unpack('@LL', buffer[:2 * word])
                                    assert length <= 1000
                                    size = 3 * word + length
                                    if len(buffer) < size:
                                        break
                                    commands.append(bytes(buffer[2 * word:
                                        2 * word + length]).decode())
                                    del buffer[:size]

                    receive(lambda: any('Send_ConfigInfo' in c for c in commands))
                    if label == 'configuration-term':
                        process.process.send_signal(signal.SIGTERM)
                    else:
                        lines = lines + ['*FvwmIdentColorset -1']
                        for line in lines:
                            raw = line.encode() + b'\0'
                            body = struct.pack('@LLL', 0, 0, 0) + raw
                            body += b'\0' * (-len(body) % word)
                            os.write(producer, struct.pack('@LLLL',
                                0xffffffff, 1 << 18, 4 + len(body) // word, 0) + body)
                        os.write(producer, struct.pack('@LLLL',
                            0xffffffff, 1 << 19, 4, 0))
                        receive(lambda: 'NOP FINISHED STARTUP' in commands)
                        assert 'Send_WindowList' in commands
                    assert process.wait_until_exit(time.monotonic() + 2) == 0
                    text = process.log_path.read_text()
                    assert not any(marker in text for marker in
                        ['AddressSanitizer', 'runtime error:', 'Assertion']), text
                    results[label] = 'passed'
                finally:
                    for descriptor in (incoming, producer, consumer, outgoing):
                        if descriptor >= 0:
                            os.close(descriptor)
        finally:
            for path in runtime.root.glob('*.log'):
                shutil.copy2(path, args.artifacts_dir / path.name)
    (args.artifacts_dir / 'evidence.json').write_text(json.dumps(results, indent=2) + '\n')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(allow_abbrev=False)
    parser.add_argument('--module', type=Path, required=True)
    parser.add_argument('--artifacts-dir', type=Path, required=True)
    parser.add_argument('--display')
    run(parser.parse_args())
