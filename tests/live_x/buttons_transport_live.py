#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or any later version.
"""Check Buttons readiness through owned X servers and native pipes."""
import argparse
import ctypes
import ctypes.util
import fcntl
import json
import os
from pathlib import Path
import resource
import selectors
import shlex
import socket
import shutil
import signal
import struct
import sys
import subprocess
import time
from live_x_harness import HarnessRuntime, start_display_server, wait_for_predicate
from colorset_live import WindowAttributes

WORD = struct.calcsize('@L')


def packet(kind, words=(), text=None):
    body = struct.pack('@' + 'L' * len(words), *words)
    if text is not None:
        raw = text.encode() + b'\0'
        body += raw + b'\0' * (-len(raw) % WORD)
    return struct.pack('@LLLL', 0xffffffff, kind, 4 + len(body) // WORD, 0) + body


def run(args):
    args.module = args.module.resolve()
    if args.fvwm is not None:
        args.fvwm = args.fvwm.resolve()
    if args.session_probe is not None:
        args.session_probe = args.session_probe.resolve()
    args.artifacts_dir.mkdir(parents=True, exist_ok=True)
    results = {}
    old_limit = resource.getrlimit(resource.RLIMIT_NOFILE)
    can_high = old_limit[1] == resource.RLIM_INFINITY or old_limit[1] > 4096
    if can_high and not args.session_only and old_limit[0] <= 4096:
        resource.setrlimit(resource.RLIMIT_NOFILE, (4097, old_limit[1]))
    try:
        with HarnessRuntime(prefix='fvwm-buttons-transport-') as runtime:
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
            library.XGetWindowAttributes.argtypes = [pointer, window_type,
                ctypes.POINTER(WindowAttributes)]
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
                                    expected = b'FvwmButtons'
                                    if ctypes.string_at(name) == expected:
                                        return children[index]
                                finally:
                                    library.XFree(name)
                    finally:
                        if children:
                            library.XFree(children)
                    return None

                cases = (['session-readiness'] if args.session_only else
                         ['popup-eof', 'popup-term', 'fragmented-packet'])
                if can_high and not args.session_only:
                    cases.append('high-descriptor')
                elif not args.session_only:
                    results['high-descriptor'] = 'unavailable under inherited hard limit'
                for label in cases:
                    target = library.XCreateSimpleWindow(connection, root,
                        10, 10, 320, 240, 0, 0, 0)
                    library.XSync(connection, False)
                    environment = dict(os.environ, DISPLAY=display)
                    if label == 'session-readiness':
                        assert args.session_probe and args.fvwm
                        save_dir = runtime.root / "session-save"
                        save_dir.mkdir()
                        environment.update(SM_SAVE_DIR=str(save_dir),
                            ICEAUTHORITY=str(runtime.root / "ICEauthority"))
                        endpoint = runtime.root / "original-manager.txt"
                        server = runtime.start("session-probe",
                            [str(args.session_probe), str(endpoint)],
                            "session-probe.log", environment=environment)
                        wait_for_predicate("original session endpoint",
                            lambda: endpoint.exists() and endpoint.stat().st_size > 0,
                            time.monotonic() + 10, process=server)
                        environment["SESSION_MANAGER"] = endpoint.read_text().strip()
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
                            environment=environment)
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
                        for line in ['*FvwmButtons Rows 1',
                                     '*FvwmButtons Columns 1',
                                     '*FvwmButtons Geometry 200x60+0+0',
                                     '*FvwmButtons Font fixed',
                                     '*FvwmButtons (Title Ordinary)']:
                            os.write(producer, packet(1 << 18, (0, 0, 0), line))
                        if label == 'session-readiness':
                            os.write(producer, packet(1 << 18, (0, 0, 0),
                                '*FvwmButtons (Swallow(UseOld) "MissingOwnedClient" "Nop")'))
                        os.write(producer, packet(1 << 19))
                        receive(lambda: 'NOP FINISHED STARTUP' in commands)
                        assert 'Send_WindowList' in commands
                        os.write(producer, packet(1 << 14))
                        deadline = time.monotonic() + 10
                        shown = popup()
                        while shown is None:
                            assert process.process.poll() is None
                            assert time.monotonic() < deadline, label
                            time.sleep(0.01)
                            shown = popup()
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
                        if label == 'session-readiness':
                            # Hold a runtime fragment while real session work runs.
                            fragment = packet(0)
                            os.write(producer, fragment[:1])
                            time.sleep(0.05)
                            receive(lambda: any(c.startswith('FSMExecFuncWithSessionManagment ')
                                for c in commands))
                            command = next(c for c in commands if
                                c.startswith('FSMExecFuncWithSessionManagment '))
                            manager = shlex.split(command)[1]
                            addresses = [item.split(':', 1)[1] for item in manager.split(',')
                                if item.startswith(('unix/', 'local/'))]
                            assert addresses, manager
                            for _ in range(3):
                                with socket.socket(socket.AF_UNIX) as pending:
                                    address = addresses[0]
                                    pending.connect('\0' + address[1:]
                                        if address.startswith('@') else address)
                                    time.sleep(0.03)
                                time.sleep(0.03)
                                assert process.process.poll() is None
                            core_log = runtime.root / 'session-core-debug.log'
                            core = runtime.start('session-core', [str(args.fvwm),
                                '-d', display, '-v', '-o', str(core_log),
                                '-c', 'AddToFunc SessionInitFunction I Echo buttons-session-connected',
                                '-c', 'Style * TileCascadePlacement'],
                                'session-core.log', environment=dict(environment,
                                    SESSION_MANAGER=manager))
                            wait_for_predicate('Buttons real XSMP registration',
                                lambda: core_log.exists() and
                                    'buttons-session-connected' in core_log.read_text(),
                                time.monotonic() + 10, process=core)
                            core.process.send_signal(signal.SIGTERM)
                            assert core.wait_until_exit(time.monotonic() + 5) == 0
                            time.sleep(0.03)
                            assert process.process.poll() is None
                            os.write(producer, fragment[1:])
                        library.XClearArea(connection, shown, 0, 0, 0, 0, True)
                        library.XSync(connection, False)
                        if label == 'fragmented-packet':
                            data = packet(0)
                            for offset in range(0, len(data), 3):
                                os.write(producer, data[offset:offset + 3])
                                time.sleep(0.001)
                        if label == 'popup-term':
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
    parser.add_argument('--artifacts-dir', type=Path, required=True)
    parser.add_argument('--display')
    parser.add_argument('--session-probe', type=Path)
    parser.add_argument('--fvwm', type=Path)
    parser.add_argument('--session-only', action='store_true')
    run(parser.parse_args())
