#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or any later version.
"""Observe Buttons/Pager UI progress while native packet input is partial."""
import argparse
import ctypes as C
import ctypes.util
import json
import os
from pathlib import Path
import selectors
import shutil
import signal
import subprocess
import struct
import sys
import time

from colorset_live import WindowAttributes
from live_x_harness import (HarnessRuntime, parse_xrandr_monitors,
    start_display_server, wait_for_predicate)
from selection_live import Event, Key, Button
from script_transport_live import packet, WORD


class Expose(C.Structure):
    _fields_ = [('type', C.c_int), ('serial', C.c_ulong),
        ('send_event', C.c_int), ('display', C.c_void_p), ('window', C.c_ulong)] + [
        (name, C.c_int) for name in ('x', 'y', 'width', 'height', 'count')]


def run(args):
    args.module = args.module.resolve()
    args.artifacts_dir.mkdir(parents=True, exist_ok=True)
    (args.artifacts_dir / 'evidence.json').unlink(missing_ok=True)
    results = {}
    with HarnessRuntime(prefix='fvwm-' + args.kind + '-partial-') as runtime:
        x = C.CDLL(ctypes.util.find_library('X11'))
        P, W = C.c_void_p, C.c_ulong
        x.XOpenDisplay.argtypes = [C.c_char_p]
        x.XOpenDisplay.restype = P
        x.XDefaultRootWindow.argtypes = [P]
        x.XDefaultRootWindow.restype = W
        x.XQueryTree.argtypes = [P, W, C.POINTER(W), C.POINTER(W),
            C.POINTER(C.POINTER(W)), C.POINTER(C.c_uint)]
        x.XFetchName.argtypes = [P, W, C.POINTER(P)]
        x.XFree.argtypes = [P]
        x.XGetWindowAttributes.argtypes = [P, W, C.POINTER(WindowAttributes)]
        x.XSync.argtypes = [P, C.c_int]
        x.XSendEvent.argtypes = [P, W, C.c_int, C.c_long, C.POINTER(Event)]
        x.XKeysymToKeycode.argtypes = [P, W]
        x.XKeysymToKeycode.restype = C.c_uint
        x.XCloseDisplay.argtypes = [P]
        x.XCreateSimpleWindow.argtypes = [P, W, C.c_int, C.c_int,
            C.c_uint, C.c_uint, C.c_uint, W, W]
        x.XCreateSimpleWindow.restype = W
        x.XDestroyWindow.argtypes = [P, W]
        connection = None
        try:
            display = args.display
            if display is None:
                _, display = start_display_server(runtime, 'Xvfb',
                    ['Xvfb', '-screen', '0', '800x600x24', '-noreset'],
                    'xserver.log', time.monotonic() + 10)
            connection = x.XOpenDisplay(display.encode())
            assert connection
            root = x.XDefaultRootWindow(connection)
            module_name = {'buttons': 'FvwmButtons', 'pager': 'FvwmPager',
                           'iconman': 'FvwmIconMan'}[args.kind]

            def popup():
                root_return, parent = W(), W()
                children = C.POINTER(W)()
                count = C.c_uint()
                assert x.XQueryTree(connection, root, C.byref(root_return),
                    C.byref(parent), C.byref(children), C.byref(count))
                try:
                    for index in range(count.value):
                        name = P()
                        if x.XFetchName(connection, children[index], C.byref(name)):
                            try:
                                attributes = WindowAttributes()
                                if (name.value and C.string_at(name) == module_name.encode()
                                        and x.XGetWindowAttributes(connection, children[index],
                                            C.byref(attributes)) and attributes.map_state == 2):
                                    return children[index]
                            finally:
                                if name.value:
                                    x.XFree(name)
                finally:
                    if children:
                        x.XFree(children)
                return None

            for label in (['packet-bounds'] if args.packet_only else
                          ['x-service'] if args.service_only else
                          ['partial-progress', 'partial-eof', 'partial-term']):
                target = x.XCreateSimpleWindow(connection, root, 10, 10, 320, 240, 0, 0, 0)
                x.XSync(connection, False)
                incoming, producer = os.pipe()
                consumer, outgoing = os.pipe()
                try:
                    argv = [str(args.module), str(outgoing), str(incoming),
                            '/dev/null', '0', '0']
                    if args.kind == 'pager':
                        argv += ['-transient', '0', '0']
                    process = runtime.start(label, argv, label + '.log',
                        environment=dict(os.environ, DISPLAY=display),
                        pass_fds=(incoming, outgoing))
                    os.close(incoming)
                    incoming = -1
                    os.close(outgoing)
                    outgoing = -1
                    os.set_blocking(consumer, False)
                    commands, buffer = [], bytearray()

                    def receive(predicate, timeout=10):
                        deadline = time.monotonic() + timeout
                        with selectors.DefaultSelector() as ready:
                            ready.register(consumer, selectors.EVENT_READ)
                            while not predicate():
                                remaining = deadline - time.monotonic()
                                assert remaining > 0 and ready.select(remaining), (label, commands)
                                data = os.read(consumer, 65536)
                                assert data, (label, commands)
                                buffer.extend(data)
                                while len(buffer) >= 2 * WORD:
                                    _, length = struct.unpack('@LL', buffer[:2 * WORD])
                                    assert length <= 4096
                                    size = 3 * WORD + length
                                    if len(buffer) < size:
                                        break
                                    commands.append(bytes(buffer[2 * WORD:2 * WORD + length]).decode())
                                    del buffer[:size]

                    receive(lambda: any('Send_ConfigInfo' in c for c in commands))
                    config = (['*FvwmButtons Rows 1', '*FvwmButtons Columns 1',
                               '*FvwmButtons Geometry 200x60+0+0',
                               '*FvwmButtons Font fixed',
                               '*FvwmButtons (Id Probe, Title Ordinary, Action Echo ui-live)']
                              if args.kind == 'buttons' else
                              ['DesktopSize 1 1', 'DesktopName 0 FvwmPager',
                               '*FvwmPagerGeometry 200x100+0+0', '*FvwmPagerFont fixed'])
                    if args.packet_only and args.kind == 'pager':
                        config.append('*FvwmPagerNoDeskHilight')
                        for number in ['x', '2147483648', '-2147483649',
                                       '99999999999999999999999999']:
                            config += [f'*FvwmPagerColorset {number} 0',
                                       f'*FvwmPagerBack {number} black',
                                       f'*FvwmPagerLabelPixmap {number} false',
                                       f'*FvwmPagerPixmap {number} /dev/null',
                                       f'*FvwmPagerHilightPixmap {number} /dev/null',
                                       f'*FvwmPagerMouse {number} MoveWindow']
                        config += ['*FvwmPagerMouse -2147483648 MoveWindow',
                                   '*FvwmPagerColorset 0suffix 0',
                                   '*FvwmPagerColorset * 0',
                                   '*FvwmPagerBack 0suffix black',
                                   '*FvwmPagerLabelPixmap 0suffix false']
                    if args.kind == 'iconman':
                        config = ['DesktopSize 1 1', '*FvwmIconMan Title FvwmIconMan',
                                  '*FvwmIconMan Resolution global',
                                  '*FvwmIconMan ManagerGeometry 1x1+0+0',
                                  '*FvwmIconMan Font fixed',
                                  '*FvwmIconMan Action Key F12 N gotomanager 0, gotobutton 0, select, sendcommand "Echo ui-live"']
                        if args.packet_only:
                            config += ['IgnoreModifiers 0', 'LoggingFD -1',
                                'IgnoreModifiers', 'LoggingFD',
                                'IgnoreModifiers 2147483648', 'LoggingFD 2147483648',
                                'IgnoreModifiers -2147483649', 'LoggingFD -2147483649',
                                'IgnoreModifiers 999999999999999999999999999999']
                    for line in config:
                        os.write(producer, packet(1 << 18, (0, 0, 0), line))
                    os.write(producer, packet(1 << 19))
                    receive(lambda: 'NOP FINISHED STARTUP' in commands)
                    if args.kind == 'iconman':
                        assert args.fixture
                        os.write(producer, subprocess.check_output([str(args.fixture), str(target)], timeout=5))
                        for kind, name in [(1 << 12, 'ordinary-class'), (1 << 13, 'ordinary-resource'),
                                           (1 << 11, 'ordinary-icon'), (1 << 10, 'ordinary name'),
                                           (1 << 26, 'ordinary visible name')]:
                            os.write(producer, packet(kind, (target, target, 0), name))
                    os.write(producer, packet(1 << 14))
                    wait_for_predicate('mapped module window', popup,
                        time.monotonic() + 10, process=process)
                    shown = popup()
                    assert shown

                    def ui_action():
                        previous = len(commands)
                        attributes = WindowAttributes()
                        assert x.XGetWindowAttributes(connection, shown, C.byref(attributes))
                        if args.kind == 'buttons':
                            for kind in [4, 5]:
                                event = Event()
                                event.button = Button(type=kind, display=connection,
                                    window=shown, root=root, x=attributes.width // 2,
                                    y=attributes.height // 2,
                                    button=1, same_screen=1)
                                assert x.XSendEvent(connection, shown, 0, 0, C.byref(event))
                            expected = 'Echo ui-live'
                        else:
                            event = Event()
                            event.key = Key(type=2, display=connection, window=shown,
                                root=root, x=10, y=10, same_screen=1,
                                keycode=x.XKeysymToKeycode(connection,
                                    0xffc9 if args.kind == 'iconman' else 0xff53))
                            assert event.key.keycode
                            assert x.XSendEvent(connection, shown, 0, 0, C.byref(event))
                            expected = 'Echo ui-live' if args.kind == 'iconman' else 'Scroll 100 0'
                        x.XSync(connection, False)
                        started = time.monotonic()
                        receive(lambda: expected in commands[previous:], timeout=0.5)
                        return time.monotonic() - started

                    # Verify delivery to the owning client before withholding bytes.
                    if args.kind == 'buttons':
                        os.write(producer, packet(1 << 22, (0, 0, 0), 'PressButton Probe 1'))
                        receive(lambda: 'Echo ui-live' in commands)
                    ui_action()
                    if args.kind == 'iconman':
                        # A missing initial display label must not kill ordinary input.
                        os.write(producer, packet((1 << 31) | (1 << 1),
                            (target, target, 0), 'ordinary visible icon'))
                        ui_action()
                    first_command = ('gotomanager 0, gotobutton 0, select, sendcommand "Echo packet-first"'
                                     if args.kind == 'iconman' else
                                     'ExpandButtonVars Probe Echo packet-first')
                    wire = (packet(1 << 22, (0, 0, 0), first_command)
                            if args.kind in ['buttons', 'iconman'] else
                            packet(1 << 22, (0, 0, 0), 'ScrollDone'))
                    offset = 0
                    if args.packet_only and args.kind != 'pager':
                        assert args.fixture
                        configured = subprocess.check_output([str(args.fixture), str(target)], timeout=5)
                        configured_words = struct.unpack('@LLLL', configured[:4 * WORD])[2] - 4
                        rejected = [(1 << 29, configured_words - 1),
                                    (1 << 30, configured_words - 1),
                                    (1 << 1, 0)]
                        if args.kind == 'iconman':
                            rejected += [(1 << 23, 8), (1 << 0, 2),
                                         (1 << 6, 0), (1 << 7, 0),
                                         (1 << 8, 6), (1 << 9, 6),
                                         (1 << 15, 6), (1 << 24, 0),
                                         (1 << 25, 0)]
                        else:
                            rejected += [(1 << 16, 0)]
                        previous_unlocks = sum(c == 'NOP UNLOCK' for c in commands)
                        for kind, words in rejected:
                            os.write(producer, packet(kind, (0,) * words))
                        if args.kind == 'iconman':
                            receive(lambda: sum(c == 'NOP UNLOCK' for c in commands)
                                == previous_unlocks + 2)
                        time.sleep(0.05)
                        ui_action()
                        text_types = [1 << 12, 1 << 13, 1 << 10, 1 << 18, 1 << 22]
                        if args.kind == 'iconman':
                            text_types += [1 << 11, 1 << 26, (1 << 31) | (1 << 1)]
                        for kind in text_types:
                            os.write(producer, packet(kind, (0, 0, 0)))
                            os.write(producer, packet(kind,
                                (0, 0, 0, int.from_bytes(b'x' * WORD,
                                    byteorder=sys.byteorder))))
                        time.sleep(0.05)
                        ui_action()
                        for words in [0, 1, 2]:
                            os.write(producer, packet((1 << 31) | (1 << 4), (0,) * words))
                        # Swallow activation consumes text; background and disabling
                        # notifications are valid with only their three prefix words.
                        for body in [(2, 1, shown), (2, 1, shown,
                                      int.from_bytes(b'x' * WORD, byteorder=sys.byteorder)),
                                     (2, 0, shown), (1, 0, 0)]:
                            os.write(producer, packet((1 << 31) | (1 << 4), body))
                        time.sleep(0.05)
                        ui_action()
                        if args.kind == 'iconman':
                            for value in ['', 'x', '2147483648', '-2147483649',
                                          '999999999999999999999999999999', '0suffix']:
                                os.write(producer, packet(1 << 18, (0, 0, 0),
                                    'IgnoreModifiers ' + value))
                            for value in ['', 'x', '-1', '4294967296',
                                          '18446744073709551616', str(root)]:
                                os.write(producer, packet((1 << 31) | (1 << 4),
                                    (2, 1, shown), value))
                            os.write(producer, packet((1 << 31) | (1 << 4), (2, 0, shown)))
                            time.sleep(0.05)
                            ui_action()
                        results[label] = {'packet-groups-followed-by-ui': 3,
                            'declared-text-boundaries': 'passed',
                            'conditional-property-text': 'passed',
                            'synchronous-refusal-unlocks': 2 if args.kind == 'iconman' else 0,
                            'normal-eof': 'passed'}
                    elif args.packet_only:
                        assert args.kind == 'pager' and args.fixture
                        configured = subprocess.check_output([str(args.fixture), str(target)], timeout=5)
                        configured_words = struct.unpack('@LLLL', configured[:4 * WORD])[2] - 4
                        for kind, words in [(1 << 29, configured_words - 1),
                                            (1 << 30, configured_words - 1),
                                            (1 << 23, 8), (1 << 0, 7),
                                            (1 << 6, 4), (1 << 1, 1),
                                            (1 << 3, 0), (1 << 4, 0),
                                            (1 << 7, 0),
                                            (1 << 8, 6), (1 << 9, 0),
                                            (1 << 15, 6), ((1 << 31) | (1 << 4), 2)]:
                            os.write(producer, packet(kind, (0,) * words))
                        time.sleep(0.05)
                        ui_action()
                        for kind in [1 << 12, 1 << 13, 1 << 26,
                                     (1 << 31) | (1 << 1), 1 << 18,
                                     1 << 22, (1 << 31) | (1 << 10)]:
                            os.write(producer, packet(kind, (0, 0, 0)))
                            os.write(producer, packet(kind,
                                (0, 0, 0, int.from_bytes(b'x' * WORD,
                                    byteorder=sys.byteorder))))
                        time.sleep(0.05)
                        ui_action()
                        for words in [0, 1, 2, 3, 251, 252]:
                            os.write(producer, packet(1 << 28, (0,) * words))
                        time.sleep(0.05)
                        ui_action()
                        for _ in range(300):
                            os.write(producer, packet(1 << 22, (0, 0, 0), 'UnusedPagerCommand'))
                        for command in ['IsShared', 'IsNotShared', 'MonitorLabels', 'NoMonitorLabels']:
                            os.write(producer, packet(1 << 22, (0, 0, 0), command))
                        time.sleep(0.05)
                        ui_action()
                        for line in ['DesktopSize', 'DesktopSize x 1',
                                     'DesktopSize 2147483648 1',
                                     'DesktopSize 2147483647 2147483647',
                                     'DesktopSize 0 -1', 'DesktopSize 2 1',
                                     'DesktopSize 1 1']:
                            os.write(producer, packet(1 << 18, (0, 0, 0), line))
                        time.sleep(0.05)
                        ui_action()
                        for line in ['DesktopConfiguration', 'DesktopConfiguration x 1',
                                     'DesktopConfiguration 2147483648 1',
                                     'DesktopConfiguration 1 -2147483649',
                                     'DesktopConfiguration 1 0']:
                            os.write(producer, packet(1 << 18, (0, 0, 0), line))
                        time.sleep(0.05)
                        ui_action()
                        root_attributes = WindowAttributes()
                        assert x.XGetWindowAttributes(connection, root,
                            C.byref(root_attributes))
                        width, height = root_attributes.width, root_attributes.height
                        listed = subprocess.check_output(['xrandr', '--display', display,
                            '--listmonitors'], text=True, timeout=5)
                        names = list(parse_xrandr_monitors(listed)) or ['FVWM-X-Root']
                        name = names[0]
                        ordinary = f'0 1 1 0 0 0 0 0 {width} {height} 0 0 0 0'
                        for line in ['Monitor', 'Monitor no-such-monitor',
                                     f'Monitor {name} 0 1 1',
                                     f'Monitor {name} {ordinary[:-1]}x',
                                     f'Monitor {name} 0 2147483647 1 0 0 0 0 0 {width} {height} 0 0 0 0',
                                     f'Monitor {name} {ordinary}',
                                     f'Monitor {name} 2 1 1 0 0 0 0 0 {width} {height} 0 0 0 0',
                                     f'Monitor {name} {ordinary}']:
                            os.write(producer, packet(1 << 18, (0, 0, 0), line))
                        time.sleep(0.05)
                        ui_action()
                        quoted = "'" + name.replace('\\', '\\\\').replace("'", "\\'") + "'"
                        os.write(producer, packet(1 << 22, (0, 0, 0),
                            f'Monitor {quoted}'))
                        for flags in (2, 0):
                            os.write(producer, packet(1 << 18, (0, 0, 0),
                                f'Monitor {quoted} {flags} 1 1 0 0 0 0 0 {width} {height} 0 0 0 0'))
                            time.sleep(0.05)
                            ui_action()
                        os.write(producer, packet(1 << 22, (0, 0, 0), 'Monitor none'))
                        results[label] = {'packet-groups-followed-by-ui': 7,
                            'max-restack-entries': 84, 'unknown-command-count': 300,
                            'desktop-size-lines-followed-by-ui': 'passed',
                            'desktop-configuration-lines-followed-by-ui': 'passed',
                            'monitor-lines-disable-reenable-without-highlight': 'passed',
                            'tracked-quoted-monitor-disabled-and-reenabled-ui': 'passed',
                            'monitor-name': name,
                            'normal-eof': 'passed'}
                    elif args.service_only:
                        assert args.kind in ['iconman', 'buttons', 'pager']
                        previous = len(commands)
                        stopped = False
                        try:
                            process.process.send_signal(signal.SIGSTOP)
                            def is_stopped():
                                pid, status = os.waitpid(process.process.pid,
                                    os.WNOHANG | os.WUNTRACED)
                                return pid != 0 and os.WIFSTOPPED(status)
                            wait_for_predicate('owned module stopped', is_stopped,
                                time.monotonic() + 2, process=process)
                            stopped = True
                            event = Event()
                            event.key = Key(type=2, display=connection, window=shown,
                                root=root, x=10, y=10, same_screen=1,
                                keycode=x.XKeysymToKeycode(connection,
                                    0xff53 if args.kind == 'pager' else 0xffc9))
                            exposed = Event()
                            fields = C.cast(C.byref(exposed), C.POINTER(Expose)).contents
                            fields.type = 12
                            fields.display = connection
                            fields.window = shown
                            fields.width = fields.height = 1
                            for _ in range(64):
                                assert x.XSendEvent(connection, shown, 0, 0,
                                    C.byref(exposed))
                            for index in range(257):
                                if index < 256 and index % 16 == 0:
                                    assert x.XSendEvent(connection, shown, 0, 0,
                                        C.byref(exposed))
                                if args.kind == 'buttons':
                                    attributes = WindowAttributes()
                                    assert x.XGetWindowAttributes(connection, shown,
                                        C.byref(attributes))
                                    for kind in [4, 5]:
                                        event.button = Button(type=kind, display=connection,
                                            window=shown, root=root,
                                            x=attributes.width // 2, y=attributes.height // 2,
                                            button=1, same_screen=1)
                                        assert x.XSendEvent(connection, shown, 0, 0,
                                            C.byref(event))
                                else:
                                    assert x.XSendEvent(connection, shown, 0, 0,
                                        C.byref(event))
                            x.XSync(connection, False)
                            if args.kind == 'pager':
                                os.close(producer)
                                producer = -1
                            else:
                                os.write(producer, wire)
                            started = time.monotonic()
                            process.process.send_signal(signal.SIGCONT)
                            stopped = False
                            if args.kind == 'pager':
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
                                    assert length <= 4096 and len(buffer) >= size
                                    commands.append(bytes(buffer[2 * WORD:2 * WORD + length]).decode())
                                    del buffer[:size]
                                before = commands[previous:].count('Scroll 100 0')
                                assert before < 257, ('X queue drained before EOF', before)
                                results[label] = {'queued-keys': 257, 'queued-exposes': 80,
                                    'keys-before-pipe-eof': before,
                                    'bounded-eof-service': 'passed'}
                            else:
                                receive(lambda: 'Echo packet-first' in commands[previous:])
                                marker = commands.index('Echo packet-first', previous)
                                before = commands[previous:marker].count('Echo ui-live')
                                assert before < 257, ('X queue drained before pipe', before)
                                receive(lambda: commands[previous:].count('Echo ui-live') == 257)
                                results[label] = {('queued-keys' if args.kind == 'iconman' else 'queued-clicks'): 257, 'queued-exposes': 80,
                                    'keys-before-pipe-command': before,
                                    'service-seconds': time.monotonic() - started,
                                    'queued-X-continuation-without-new-input': 'passed'}
                        finally:
                            if stopped:
                                process.process.send_signal(signal.SIGCONT)
                    elif label == 'partial-progress':
                        elapsed = []
                        for boundary in [1, WORD, 4 * WORD - 1, 4 * WORD, len(wire) - 1]:
                            os.write(producer, wire[offset:boundary])
                            offset = boundary
                            time.sleep(0.05)
                            elapsed.append(ui_action())
                            assert 'Echo packet-first' not in commands
                        os.write(producer, wire[offset:])
                        if args.kind in ['buttons', 'iconman']:
                            second_command = ('gotomanager 0, gotobutton 0, select, sendcommand "Echo packet-second"'
                                              if args.kind == 'iconman' else
                                              'ExpandButtonVars Probe Echo packet-second')
                            os.write(producer, packet(1 << 22, (0, 0, 0), second_command))
                            receive(lambda: 'Echo packet-second' in commands)
                            assert commands.index('Echo packet-first') < commands.index('Echo packet-second')
                        else:
                            ui_action()
                        results[label] = {'partial-boundaries': 5,
                            'ui-seconds': elapsed, 'following-completion': 'passed'}
                        if args.kind == 'iconman':
                            results[label]['missing-display-text-followed-by-name-update'] = 'passed'
                    else:
                        os.write(producer, wire[:4 * WORD + 1])
                        time.sleep(0.05)
                        ui_action()
                    if label == 'partial-term':
                        process.process.send_signal(signal.SIGTERM)
                    elif producer >= 0:
                        os.close(producer)
                        producer = -1
                    assert process.wait_until_exit(time.monotonic() + 2) == 0
                    text = process.log_path.read_text()
                    assert not any(marker in text for marker in
                        ['AddressSanitizer', 'runtime error:', 'Assertion']), text
                    if label not in ['partial-progress', 'x-service', 'packet-bounds']:
                        results[label] = 'passed'
                finally:
                    for descriptor in [incoming, producer, consumer, outgoing]:
                        if descriptor >= 0:
                            os.close(descriptor)
                    x.XDestroyWindow(connection, target)
                    x.XSync(connection, False)
        finally:
            if connection:
                x.XCloseDisplay(connection)
            for path in runtime.root.glob('*.log'):
                shutil.copy2(path, args.artifacts_dir / path.name)
    (args.artifacts_dir / 'evidence.json').write_text(json.dumps(results, indent=2) + '\n')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(allow_abbrev=False)
    parser.add_argument('--module', type=Path, required=True)
    parser.add_argument('--kind', choices=['buttons', 'pager', 'iconman'], required=True)
    parser.add_argument('--artifacts-dir', type=Path, required=True)
    parser.add_argument('--display')
    parser.add_argument('--fixture', type=Path)
    parser.add_argument('--service-only', action='store_true')
    parser.add_argument('--packet-only', action='store_true')
    run(parser.parse_args())
