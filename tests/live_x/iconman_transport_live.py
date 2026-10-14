#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or any later version.
"""Check IconMan readiness through owned X servers and native pipes."""
import argparse
import ctypes
import ctypes.util
import fcntl
import json
import os
from pathlib import Path
import resource
import selectors
import shutil
import signal
import struct
import sys
import subprocess
import time
from live_x_harness import HarnessRuntime, start_display_server
from colorset_live import WindowAttributes

WORD = struct.calcsize('@L')


def packet(kind, words=(), text=None):
    body = struct.pack('@' + 'L' * len(words), *words)
    if text is not None:
        raw = text.encode() + b'\0'
        body += raw + b'\0' * (-len(raw) % WORD)
    return struct.pack('@LLLL', 0xffffffff, kind, 4 + len(body) // WORD, 0) + body


def run(args):
    args.artifacts_dir.mkdir(parents=True, exist_ok=True)
    results = {}
    old_limit = resource.getrlimit(resource.RLIMIT_NOFILE)
    can_high = old_limit[1] == resource.RLIM_INFINITY or old_limit[1] > 4096
    if can_high and old_limit[0] <= 4096:
        resource.setrlimit(resource.RLIMIT_NOFILE, (4097, old_limit[1]))
    try:
        with HarnessRuntime(prefix='fvwm-iconman-transport-') as runtime:
            library = ctypes.CDLL(ctypes.util.find_library('X11'))
            pointer = ctypes.c_void_p
            window_type = ctypes.c_ulong
            library.XOpenDisplay.argtypes = [ctypes.c_char_p]
            library.XOpenDisplay.restype = pointer
            library.XDefaultRootWindow.argtypes = [pointer]
            library.XDefaultRootWindow.restype = window_type
            library.XCreateSimpleWindow.argtypes = [pointer, window_type,
                ctypes.c_int, ctypes.c_int, ctypes.c_uint, ctypes.c_uint,
                ctypes.c_uint, window_type, window_type]
            library.XCreateSimpleWindow.restype = window_type
            library.XQueryTree.argtypes = [pointer, window_type,
                ctypes.POINTER(window_type), ctypes.POINTER(window_type),
                ctypes.POINTER(ctypes.POINTER(window_type)),
                ctypes.POINTER(ctypes.c_uint)]
            library.XFetchName.argtypes = [pointer, window_type,
                ctypes.POINTER(pointer)]
            library.XFree.argtypes = [pointer]
            library.XDestroyWindow.argtypes = [pointer, window_type]
            library.XClearArea.argtypes = [pointer, window_type, ctypes.c_int,
                ctypes.c_int, ctypes.c_uint, ctypes.c_uint, ctypes.c_int]
            library.XSync.argtypes = [pointer, ctypes.c_int]
            library.XCloseDisplay.argtypes = [pointer]
            library.XMapWindow.argtypes = [pointer, window_type]
            library.XReparentWindow.argtypes = [pointer, window_type,
                window_type, ctypes.c_int, ctypes.c_int]
            library.XGetWindowAttributes.argtypes = [pointer, window_type,
                ctypes.POINTER(WindowAttributes)]
            library.XWarpPointer.argtypes = [pointer, window_type, window_type,
                ctypes.c_int, ctypes.c_int, ctypes.c_uint, ctypes.c_uint,
                ctypes.c_int, ctypes.c_int]

            connection = None
            try:
                display = args.display
                if display is None:
                    _, display = start_display_server(runtime, 'Xvfb',
                        ['Xvfb', '-screen', '0', '800x600x24', '-noreset'],
                        'xserver.log', time.monotonic() + 10)
                connection = library.XOpenDisplay(display.encode())
                assert connection
                root = library.XDefaultRootWindow(connection)

                def popup():
                    root_return, parent = window_type(), window_type()
                    children = ctypes.POINTER(window_type)()
                    count = ctypes.c_uint()
                    assert library.XQueryTree(connection, root,
                        ctypes.byref(root_return), ctypes.byref(parent),
                        ctypes.byref(children), ctypes.byref(count))
                    try:
                        for index in range(count.value):
                            name = pointer()
                            if library.XFetchName(connection, children[index],
                                    ctypes.byref(name)) and name.value:
                                try:
                                    expected = b'FvwmIconMan'
                                    if ctypes.string_at(name) == expected:
                                        return children[index]
                                finally:
                                    library.XFree(name)
                    finally:
                        if children:
                            library.XFree(children)
                    return None

                cases = ['popup-eof', 'popup-term', 'fragmented-packet',
                         'tooltip-delay', 'long-tooltip-term']
                if can_high:
                    cases.append('high-descriptor')
                else:
                    results['high-descriptor'] = 'unavailable under inherited hard limit'
                for label in cases:
                    target = library.XCreateSimpleWindow(connection, root,
                        10, 10, 320, 240, 0, 0, 0)
                    library.XSync(connection, False)
                    frame = None
                    incoming, producer = os.pipe()
                    consumer, outgoing = os.pipe()
                    try:
                        if label == 'high-descriptor':
                            replacement = fcntl.fcntl(incoming,
                                fcntl.F_DUPFD_CLOEXEC, 4096)
                            os.close(incoming)
                            incoming = replacement
                        process = runtime.start(label, [str(args.module),
                            str(outgoing), str(incoming), '/dev/null',
                            '0', '0'], label + '.log',
                            pass_fds=(incoming, outgoing),
                            environment=dict(os.environ, DISPLAY=display))
                        os.close(incoming)
                        incoming = -1
                        os.close(outgoing)
                        outgoing = -1
                        buffer, commands = bytearray(), []
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
                                    while len(buffer) >= 2 * WORD:
                                        _, length = struct.unpack('@LL', buffer[:2 * WORD])
                                        assert length <= 1000
                                        size = 3 * WORD + length
                                        if len(buffer) < size:
                                            break
                                        commands.append(bytes(buffer[2 * WORD:
                                            2 * WORD + length]).decode())
                                        del buffer[:size]

                        receive(lambda: any('Send_ConfigInfo' in c for c in commands))
                        for line in ['DesktopSize 1 1',
                                     '*FvwmIconMan Title FvwmIconMan',
                                     '*FvwmIconMan Resolution global',
                                     '*FvwmIconMan ManagerGeometry 1x1+0+0',
                                     '*FvwmIconMan Font fixed',
                                     '*FvwmIconMan Tips always',
                                     '*FvwmIconMan TipsFont fixed',
                                     '*FvwmIconMan TipsDelays ' +
                                     ('2147483647' if label == 'long-tooltip-term' else '150')]:
                            os.write(producer, packet(1 << 18, (0, 0, 0), line))
                        os.write(producer, packet(1 << 19))
                        receive(lambda: 'NOP FINISHED STARTUP' in commands)
                        assert 'Send_WindowList' in commands
                        configured = subprocess.check_output(
                            [str(args.fixture), str(target)], timeout=5)
                        os.write(producer, configured)
                        for kind, text in [(1 << 12, 'ordinary-class'),
                                           (1 << 13, 'ordinary-resource'),
                                           (1 << 11, 'ordinary-icon'),
                                           (1 << 10, 'ordinary window name'),
                                           (1 << 26, 'ordinary visible name'),
                                           ((1 << 31) | (1 << 1), 'ordinary visible icon')]:
                            os.write(producer, packet(kind, (target, target, 0), text))
                        os.write(producer, packet(1 << 14))
                        deadline = time.monotonic() + 10
                        shown = popup()
                        while shown is None:
                            assert process.process.poll() is None
                            assert time.monotonic() < deadline, label
                            time.sleep(0.01)
                            shown = popup()
                        frame = library.XCreateSimpleWindow(connection, root,
                            20, 20, 400, 100, 0, 0, 0)
                        library.XReparentWindow(connection, shown, frame, 0, 0)
                        library.XMapWindow(connection, frame)
                        library.XSync(connection, False)
                        attributes = WindowAttributes()
                        deadline = time.monotonic() + 10
                        while True:
                            assert library.XGetWindowAttributes(connection, shown,
                                ctypes.byref(attributes))
                            if attributes.map_state == 2:
                                break
                            assert process.process.poll() is None
                            assert time.monotonic() < deadline, label
                            time.sleep(0.01)
                        if label in ['tooltip-delay', 'long-tooltip-term']:
                            library.XWarpPointer(connection, 0, shown,
                                0, 0, 0, 0, max(1, attributes.width // 2),
                                max(1, attributes.height // 2))
                            library.XSync(connection, False)
                            if label == 'tooltip-delay':
                                deadline = time.monotonic() + 5
                                while True:
                                    root_return, parent = window_type(), window_type()
                                    children = ctypes.POINTER(window_type)()
                                    count = ctypes.c_uint()
                                    assert library.XQueryTree(connection, root,
                                        ctypes.byref(root_return), ctypes.byref(parent),
                                        ctypes.byref(children), ctypes.byref(count))
                                    mapped_tip = False
                                    try:
                                        for index in range(count.value):
                                            attrs = WindowAttributes()
                                            assert library.XGetWindowAttributes(connection,
                                                children[index], ctypes.byref(attrs))
                                            if attrs.override_redirect and attrs.map_state == 2:
                                                mapped_tip = True
                                    finally:
                                        if children:
                                            library.XFree(children)
                                    if mapped_tip:
                                        break
                                    assert process.process.poll() is None
                                    assert time.monotonic() < deadline, 'tooltip never mapped'
                                    time.sleep(0.01)
                        library.XClearArea(connection, shown, 0, 0, 0, 0, True)
                        library.XSync(connection, False)
                        if label == 'fragmented-packet':
                            data = packet(0)
                            for offset in range(0, len(data), 3):
                                os.write(producer, data[offset:offset + 3])
                                time.sleep(0.001)
                        if label in ['popup-term', 'long-tooltip-term']:
                            process.process.send_signal(signal.SIGTERM)
                        else:
                            os.close(producer)
                            producer = -1
                        assert process.wait_until_exit(time.monotonic() + 2) == 0
                        text = process.log_path.read_text()
                        assert not any(marker in text for marker in
                            ['AddressSanitizer', 'runtime error:', 'Assertion']), text
                        results[label] = 'passed'
                    finally:
                        for descriptor in (incoming, producer, consumer, outgoing):
                            if descriptor >= 0:
                                os.close(descriptor)
                        if frame is not None:
                            library.XDestroyWindow(connection, frame)
                        library.XDestroyWindow(connection, target)
                        library.XSync(connection, False)
            finally:
                if connection:
                    library.XCloseDisplay(connection)
                for path in runtime.root.glob('*.log'):
                    shutil.copy2(path, args.artifacts_dir / path.name)
    finally:
        resource.setrlimit(resource.RLIMIT_NOFILE, old_limit)
    (args.artifacts_dir / 'evidence.json').write_text(json.dumps(results, indent=2) + '\n')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(allow_abbrev=False)
    parser.add_argument('--module', type=Path, required=True)
    parser.add_argument('--fixture', type=Path, required=True)
    parser.add_argument('--artifacts-dir', type=Path, required=True)
    parser.add_argument('--display')
    run(parser.parse_args())
