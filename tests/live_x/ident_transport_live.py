#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or any later version.
"""Check Ident/Pager popup waits through owned X servers and native pipes."""
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
from live_x_harness import HarnessRuntime, start_display_server, wait_for_predicate
from selection_live import Event, Key

class Reparent(ctypes.Structure):
    _fields_ = [('type', ctypes.c_int), ('serial', ctypes.c_ulong),
        ('send_event', ctypes.c_int), ('display', ctypes.c_void_p),
        ('event', ctypes.c_ulong), ('window', ctypes.c_ulong),
        ('parent', ctypes.c_ulong), ('x', ctypes.c_int), ('y', ctypes.c_int),
        ('override_redirect', ctypes.c_int)]


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
        with HarnessRuntime(prefix='fvwm-popup-transport-') as runtime:
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
                                    expected = b'FvwmPager' if args.pager else b'FvwmIdent'
                                    if ctypes.string_at(name) == expected:
                                        return children[index]
                                finally:
                                    library.XFree(name)
                    finally:
                        if children:
                            library.XFree(children)
                    return None

                cases = ['popup-eof', 'popup-term',
                         'empty-config' if args.pager else 'packet-bounds']
                if args.partial_only:
                    assert not args.pager
                    cases = ['partial-key-close', 'partial-eof', 'partial-term']
                if args.service_only:
                    assert not args.pager
                    cases = ['queued-x-eof']
                if can_high and not args.partial_only and not args.service_only:
                    cases.append('high-descriptor')
                elif not args.partial_only and not args.service_only:
                    results['high-descriptor'] = 'unavailable under inherited hard limit'
                for label in cases:
                    target = library.XCreateSimpleWindow(connection, root,
                        10, 10, 320, 240, 0, 0, 0)
                    library.XSync(connection, False)
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
                            '0' if args.pager else f'{target:x}', '0'], label + '.log',
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
                        if args.pager:
                            if label == 'empty-config':
                                for line in ['', ' ', '\t', '']:
                                    os.write(producer, packet(1 << 18,
                                        (0, 0, 0), line))
                            for line in ['DesktopSize 1 1',
                                         'DesktopName 0 FvwmPager',
                                         '*FvwmPagerGeometry 200x100+0+0',
                                         '*FvwmPagerFont fixed']:
                                os.write(producer, packet(1 << 18, (0, 0, 0), line))
                        else:
                            os.write(producer, packet(1 << 18, (0, 0, 0),
                                '*FvwmIdentColorset -1'))
                        os.write(producer, packet(1 << 19))
                        receive(lambda: 'NOP FINISHED STARTUP' in commands)
                        assert 'Send_WindowList' in commands
                        if label == 'packet-bounds':
                            os.write(producer, packet(1 << 30))
                            for kind in [1 << 10, 1 << 11, 1 << 12, 1 << 13]:
                                os.write(producer, packet(kind, (target, target)))
                                os.write(producer, packet(kind,
                                    (target, target, 0, int.from_bytes(b'A' * WORD,
                                        byteorder=sys.byteorder))))
                        if not args.pager:
                            configured = subprocess.check_output(
                                [str(args.fixture), str(target)], timeout=5)
                            os.write(producer, configured)
                            os.write(producer, packet(1 << 10, (target, target, 0),
                                'ordinary window name'))
                        os.write(producer, packet(1 << 14))
                        deadline = time.monotonic() + 10
                        shown = popup()
                        while shown is None:
                            assert process.process.poll() is None
                            assert time.monotonic() < deadline, label
                            time.sleep(0.01)
                            shown = popup()
                        library.XClearArea(connection, shown, 0, 0, 0, 0, True)
                        library.XSync(connection, False)
                        if args.service_only:
                            previous = len(commands)
                            stopped = False
                            try:
                                process.process.send_signal(signal.SIGSTOP)
                                def is_stopped():
                                    pid, status = os.waitpid(process.process.pid,
                                        os.WNOHANG | os.WUNTRACED)
                                    return pid != 0 and os.WIFSTOPPED(status)
                                wait_for_predicate('owned Ident stopped', is_stopped,
                                    time.monotonic() + 2, process=process)
                                stopped = True
                                library.XSendEvent.argtypes = [pointer, window_type,
                                    ctypes.c_int, ctypes.c_long, ctypes.POINTER(Event)]
                                event = Event()
                                fields = ctypes.cast(ctypes.byref(event),
                                    ctypes.POINTER(Reparent)).contents
                                fields.type = 21
                                fields.display = connection
                                fields.event = fields.window = shown
                                fields.parent = root
                                for _ in range(257):
                                    assert library.XSendEvent(connection, shown, 0, 0,
                                        ctypes.byref(event))
                                library.XSync(connection, False)
                                os.close(producer)
                                producer = -1
                                process.process.send_signal(signal.SIGCONT)
                                stopped = False
                                # Drain native replies concurrently with teardown.
                                with selectors.DefaultSelector() as ready:
                                    ready.register(consumer, selectors.EVENT_READ)
                                    deadline = time.monotonic() + 2
                                    while True:
                                        remaining = deadline - time.monotonic()
                                        assert remaining > 0 and ready.select(remaining)
                                        data = os.read(consumer, 65536)
                                        if not data:
                                            break
                                        buffer.extend(data)
                                while len(buffer) >= 2 * WORD:
                                    _, length = struct.unpack('@LL', buffer[:2 * WORD])
                                    size = 3 * WORD + length
                                    assert length <= 1000 and len(buffer) >= size
                                    commands.append(bytes(buffer[2 * WORD:
                                        2 * WORD + length]).decode())
                                    del buffer[:size]
                                count = commands[previous:].count('Raise')
                                assert count < 257, ('X queue drained before EOF', count)
                                results[label] = {'queued-events': 257,
                                    'events-before-pipe-eof': count,
                                    'bounded-eof-service': 'passed'}
                            finally:
                                if stopped:
                                    process.process.send_signal(signal.SIGCONT)
                        if args.partial_only:
                            wire = packet(1 << 18, (0, 0, 0), '*FvwmIdentColorset -1')
                            os.write(producer, wire[:4 * WORD + 1])
                            time.sleep(0.05)
                            if label == 'partial-key-close':
                                library.XSendEvent.argtypes = [pointer, window_type, ctypes.c_int,
                                    ctypes.c_long, ctypes.POINTER(Event)]
                                library.XKeysymToKeycode.argtypes = [pointer, window_type]
                                library.XKeysymToKeycode.restype = ctypes.c_uint
                                for kind in [2, 3]:
                                    event = Event()
                                    event.key = Key(type=kind, display=connection, window=shown,
                                        root=root, keycode=library.XKeysymToKeycode(connection, 0xff0d),
                                        same_screen=1)
                                    assert event.key.keycode
                                    assert library.XSendEvent(connection, shown, 0, 0, ctypes.byref(event))
                                library.XSync(connection, False)
                        if args.pager:
                            data = packet(0)
                            for offset in range(0, len(data), 3):
                                os.write(producer, data[offset:offset + 3])
                                time.sleep(0.001)
                        if label == 'packet-bounds':
                            os.write(producer, packet(1 << 18))
                            os.write(producer, packet(1 << 18, (0, 0, 0,
                                int.from_bytes(b'A' * WORD, byteorder=sys.byteorder))))
                            os.write(producer, packet((1 << 31) | (1 << 4), (0, 0)))
                            os.write(producer, packet(1 << 18, (0, 0, 0), 'Colorset 0'))
                        if label in ['popup-term', 'partial-term']:
                            process.process.send_signal(signal.SIGTERM)
                        elif label != 'partial-key-close' and not args.service_only:
                            os.close(producer)
                            producer = -1
                        assert process.wait_until_exit(time.monotonic() + 2) == 0
                        text = process.log_path.read_text()
                        assert not any(marker in text for marker in
                            ['AddressSanitizer', 'runtime error:', 'Assertion']), text
                        if not args.service_only:
                            results[label] = 'passed'
                    finally:
                        for descriptor in (incoming, producer, consumer, outgoing):
                            if descriptor >= 0:
                                os.close(descriptor)
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
    parser.add_argument('--pager', action='store_true')
    parser.add_argument('--partial-only', action='store_true')
    parser.add_argument('--service-only', action='store_true')
    run(parser.parse_args())
