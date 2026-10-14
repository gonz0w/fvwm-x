#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
"""Observe Form input servicing while its ordinary configured timer runs."""
import argparse
import ctypes
import ctypes.util
import json
import fcntl
import resource
import signal
import os
from pathlib import Path
import selectors
import shutil
import struct
import time
from colorset_live import WindowAttributes
from live_x_harness import HarnessRuntime, start_display_server, wait_for_predicate
from script_transport_live import packet, WORD
from selection_live import Event, Key


class FormExpose(ctypes.Structure):
    _fields_ = [('type', ctypes.c_int), ('serial', ctypes.c_ulong),
        ('send_event', ctypes.c_int), ('display', ctypes.c_void_p),
        ('window', ctypes.c_ulong)] + [(name, ctypes.c_int)
        for name in ('x', 'y', 'width', 'height', 'count')]


def run(args):
    args.module = args.module.resolve()
    args.artifacts_dir.mkdir(parents=True, exist_ok=True)
    (args.artifacts_dir / 'evidence.json').unlink(missing_ok=True)
    results = {}
    with HarnessRuntime(prefix='fvwm-form-transport-') as runtime:
        x = ctypes.CDLL(ctypes.util.find_library('X11'))
        P, W = ctypes.c_void_p, ctypes.c_ulong
        x.XOpenDisplay.argtypes = [ctypes.c_char_p]
        x.XOpenDisplay.restype = P
        x.XDefaultRootWindow.argtypes = [P]
        x.XDefaultRootWindow.restype = W
        x.XQueryTree.argtypes = [P, W, ctypes.POINTER(W), ctypes.POINTER(W),
            ctypes.POINTER(ctypes.POINTER(W)), ctypes.POINTER(ctypes.c_uint)]
        x.XFetchName.argtypes = [P, W, ctypes.POINTER(P)]
        x.XFree.argtypes = [P]
        x.XGetWindowAttributes.argtypes = [P, W, ctypes.POINTER(WindowAttributes)]
        x.XSync.argtypes = [P, ctypes.c_int]
        x.XCloseDisplay.argtypes = [P]
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
            incoming, producer = os.pipe()
            consumer, outgoing = os.pipe()
            try:
                if args.high_descriptor:
                    replacement = fcntl.fcntl(incoming, fcntl.F_DUPFD_CLOEXEC, 4096)
                    os.close(incoming)
                    incoming = replacement
                process = runtime.start('form', [str(args.module), str(outgoing),
                    str(incoming), '/dev/null', '0', '0', 'FormTransport'],
                    'form.log', environment=dict(os.environ, DISPLAY=display),
                    pass_fds=(incoming, outgoing))
                os.close(incoming)
                incoming = -1
                os.close(outgoing)
                outgoing = -1
                os.set_blocking(consumer, False)
                buffer, commands = bytearray(), []

                def receive(predicate, timeout=10):
                    deadline = time.monotonic() + timeout
                    with selectors.DefaultSelector() as ready:
                        ready.register(consumer, selectors.EVENT_READ)
                        while not predicate():
                            left = deadline - time.monotonic()
                            assert left > 0 and ready.select(left), commands
                            data = os.read(consumer, 65536)
                            assert data, commands
                            buffer.extend(data)
                            while len(buffer) >= 2 * WORD:
                                _, length = struct.unpack('@LL', buffer[:2 * WORD])
                                assert length < 20000
                                size = 3 * WORD + length
                                if len(buffer) < size:
                                    break
                                commands.append(bytes(buffer[2 * WORD:2 * WORD + length]).decode())
                                del buffer[:size]

                receive(lambda: any('Send_ConfigInfo' in c for c in commands))
                if args.input_only:
                    for line in ['', '\n']:
                        os.write(producer, packet(1 << 18, (0, 0, 0), line))
                os.write(producer, packet(1 << 18, (0, 0, 0),
                    '*FvwmFormDefaultRead y'))
                os.write(producer, packet(1 << 19))
                receive(lambda: sum('Send_ConfigInfo' in c for c in commands) >= 2)
                config = ['*FormTransportTitle "FormTransport"',
                    '*FormTransportFont fixed', '*FormTransportTimeoutFont fixed',
                    '*FormTransportLine left', '*FormTransportText "Timed form"',
                    '*FormTransportLine left']
                if args.fields_only or args.data_only or args.arrays_only:
                    config.extend(['*FormTransportPosition 0',
                        '*FormTransportInputFont fixed',
                        '*FormTransportSelection fields single',
                        '*FormTransportChoice enabled yes on',
                        '*FormTransportChoice disabled no off',
                        '*FormTransportInput empty 10'])
                command_name = 'n' * 1024
                if args.partial_only:
                    config.extend(['*FormTransportButton continue "Alive" F12',
                        '*FormTransportCommand Nop form-partial-alive'])
                if args.service_only:
                    config.extend(['*FormTransportButton continue "Busy" F12',
                        '*FormTransportButton continue "Done" F11',
                        '*FormTransportCommand Nop form-service-done'])
                if args.history_only:
                    config.extend(['*FormTransportInputFont fixed',
                        '*FormTransportInput first 10 "fixed"',
                        '*FormTransportInput second 10 "éinit"',
                        '*FormTransportButton restart "Save" F11',
                        '*FormTransportCommand Nop form-history-saved',
                        '*FormTransportButton continue "Read" F12',
                        '*FormTransportCommand Nop form-history:$(first)/$(second)'])
                if args.command_only:
                    config.extend(['*FormTransportInputFont fixed',
                        '*FormTransportInput ' + command_name + ' 10 "ready"',
                        '*FormTransportButton continue "Commands" F12',
                        '*FormTransportCommand Nop form-command-before:$(' + command_name + ')',
                        '*FormTransportCommand Nop $(' + command_name,
                        '*FormTransportCommand Nop form-command-after'])
                if args.geometry_only:
                    config.extend(['*FormTransportInputFont fixed',
                        '*FormTransportInput good 10 "kept"',
                        '*FormTransportInput', '*FormTransportInput missing',
                        '*FormTransportInput zero 0', '*FormTransportInput negative -1',
                        '*FormTransportInput text other',
                        '*FormTransportInput range 999999999999999999999',
                        '*FormTransportInput pixels 2147483647',
                        '*FormTransportInput next 5tail "follow"'])
                if args.arrays_only:
                    config.extend(['*FormTransportLine left'] +
                        ['*FormTransportText ""'] * 40 +
                        ['*FormTransportLine left',
                         '*FormTransportSelection many multiple'])
                    for index in range(18):
                        if index and index % 6 == 0:
                            config.append('*FormTransportLine left')
                        config.append('*FormTransportChoice c{0} v{0} on ""'.format(index))
                    config.extend(['*FormTransportLine left',
                        '*FormTransportButton continue "Arrays" F12'])
                    for index in range(20):
                        config.append('*FormTransportCommand Nop form-array-' + str(index))
                    config.append('*FormTransportCommand Nop form-array-values:$(many)')
                data_name = '/tmp/' + 'form-data-' * 24
                if args.data_only:
                    config.extend(['*FormTransportUseData unused Old_',
                        '*FormTransportUseData ' + data_name + ' Data_',
                        '*FormTransportUseData', '*FormTransportUseData missing'])
                expected_button = ('Nop form-command-after' if args.command_only
                    else 'Nop form-geometry:kept/follow' if args.geometry_only
                    else 'Nop form-array-values:' + ' '.join('v' + str(i) for i in range(18)) if args.arrays_only
                    else 'Nop form-fields:loaded/yes' if args.data_only
                    else 'Nop form-fields:/yes' if args.fields_only
                    else 'Nop form-button-fired')
                if args.button_only or args.fields_only or args.data_only or args.geometry_only:
                    config.extend(['*FormTransportButton continue "Say \\\"hello\\\"" F12',
                        '*FormTransportCommand ' + ('Nop form-fields:$(empty)/$(fields)'
                            if args.fields_only or args.data_only else 'Nop form-geometry:$(good)/$(next)' if args.geometry_only else expected_button)])
                if args.input_only:
                    config.extend(['', '\n', '*FormTransportMessage',
                        '*FormTransportTimeout', '*FormTransportTimeout 20',
                        '*FormTransportTimeout 20 Nop',
                        '*FormTransportTimeout 20 "Nop',
                        '*FormTransportTimeout 20 Nop "Text'])
                expected_command = 'Nop form-timer-fired'
                if not args.idle_only:
                    if args.input_only:
                        expected_command += ' "quoted"'
                        config.append('*FormTransportTimeout 20 "Nop form-timer-fired '
                            '\\"quoted\\"" "Remaining %%"')
                    elif args.command_only:
                        config.append('*FormTransportTimeout 20 "Nop $(' + command_name + '" "Remaining %%"')
                    else:
                        config.append('*FormTransportTimeout 20 "Nop form-timer-fired" "Remaining %%"')
                (args.artifacts_dir / 'form-config.txt').write_text('\n'.join(config) + '\n')
                for line in config:
                    os.write(producer, packet(1 << 18, (0, 0, 0), line))
                os.write(producer, packet(1 << 19))
                if args.data_only:
                    receive(lambda: 'Send_ConfigInfo Data_' in commands)
                    assert 'read ' + data_name + ' Quiet' in commands, commands
                    for line in ['Data_', 'Data_\n', 'Data_empty loaded']:
                        os.write(producer, packet(1 << 18, (0, 0, 0), line))
                    os.write(producer, packet(1 << 19))
                receive(lambda: 'NOP FINISHED STARTUP' in commands)

                def frame():
                    rr, parent = W(), W()
                    children, count = ctypes.POINTER(W)(), ctypes.c_uint()
                    assert x.XQueryTree(connection, root, ctypes.byref(rr),
                        ctypes.byref(parent), ctypes.byref(children), ctypes.byref(count))
                    try:
                        for index in range(count.value):
                            name = P()
                            if x.XFetchName(connection, children[index], ctypes.byref(name)) and name:
                                try:
                                    if ctypes.string_at(name) == b'FormTransport':
                                        return children[index]
                                finally:
                                    x.XFree(name)
                    finally:
                        if children:
                            x.XFree(children)
                    return None

                attributes = WindowAttributes()

                def mapped_frame():
                    window = frame()
                    return window if window and x.XGetWindowAttributes(connection,
                        window, ctypes.byref(attributes)) and attributes.map_state == 2 else None

                wait_for_predicate('mapped Form window', lambda: mapped_frame() is not None,
                    time.monotonic() + 5, process=process)
                shown = mapped_frame()
                assert shown is not None
                timer_start = time.monotonic()
                if args.partial_only:
                    x.XKeysymToKeycode.argtypes = [P, W]
                    x.XKeysymToKeycode.restype = ctypes.c_uint
                    x.XSendEvent.argtypes = [P, W, ctypes.c_int,
                        ctypes.c_long, ctypes.POINTER(Event)]
                    event = Event()
                    event.key.type = 2
                    event.key.display = connection
                    event.key.window = shown
                    event.key.root = root
                    event.key.same_screen = 1
                    event.key.keycode = x.XKeysymToKeycode(connection, 0xffc9)
                    assert event.key.keycode
                    wire = packet(1 << 18, (0, 0, 0), '*FormTransportUnMap')
                    previous = 0
                    for offset in [1, WORD, 4 * WORD - 1, 4 * WORD, len(wire) - 1]:
                        os.write(producer, wire[previous:offset])
                        previous = offset
                        time.sleep(.05)
                        count = commands.count('Nop form-partial-alive')
                        assert x.XSendEvent(connection, shown, 0, 1, ctypes.byref(event))
                        x.XSync(connection, 0)
                        receive(lambda: commands.count('Nop form-partial-alive') > count, .5)
                    results['partial-header-body-key-progress'] = 'passed'
                    if not args.idle_only:
                        receive(lambda: expected_command in commands)
                        results['timer-during-partial-input'] = 'passed'
                    os.write(producer, wire[previous:])
                    wait_for_predicate('Form completes fragmented UnMap',
                        lambda: x.XGetWindowAttributes(connection, shown,
                            ctypes.byref(attributes)) and attributes.map_state == 0,
                        time.monotonic() + .5, process=process)
                    os.write(producer, packet(1 << 18, (0, 0, 0), '*FormTransportMap'))
                    wait_for_predicate('Form following packet retains order',
                        lambda: x.XGetWindowAttributes(connection, shown,
                            ctypes.byref(attributes)) and attributes.map_state == 2,
                        time.monotonic() + .5, process=process)
                    results['fragment-completion-following-command-order'] = 'passed'
                if args.service_only:
                    x.XKeysymToKeycode.argtypes = [P, W]
                    x.XKeysymToKeycode.restype = ctypes.c_uint
                    x.XSendEvent.argtypes = [P, W, ctypes.c_int,
                        ctypes.c_long, ctypes.POINTER(Event)]
                    event = Event()
                    event.key.type = 2
                    event.key.display = connection
                    event.key.window = shown
                    event.key.root = root
                    event.key.same_screen = 1
                    event.key.keycode = x.XKeysymToKeycode(connection, 0xffc9)
                    assert event.key.keycode
                    exposed = Event()
                    fields = ctypes.cast(ctypes.byref(exposed),
                        ctypes.POINTER(FormExpose)).contents
                    fields.type = 12  # Expose
                    fields.display = connection
                    fields.window = shown
                    fields.width = fields.height = 1
                    for _ in range(64):
                        assert x.XSendEvent(connection, shown, 0,
                            1 << 15, ctypes.byref(exposed))
                    # Existing button feedback waits ten milliseconds per key.
                    # No command output or unbounded sender is part of this load.
                    for index in range(256):
                        if index % 16 == 0:
                            assert x.XSendEvent(connection, shown, 0,
                                1 << 15, ctypes.byref(exposed))
                        assert x.XSendEvent(connection, shown, 0, 1, ctypes.byref(event))
                    event.key.keycode = x.XKeysymToKeycode(connection, 0xffc8)
                    assert event.key.keycode
                    assert x.XSendEvent(connection, shown, 0, 1, ctypes.byref(event))
                    x.XSync(connection, 0)
                    requested = time.monotonic()
                    os.write(producer, packet(1 << 18, (0, 0, 0), '*FormTransportUnMap'))
                    wait_for_predicate('Form pipe command during queued keys',
                        lambda: x.XGetWindowAttributes(connection, shown,
                            ctypes.byref(attributes)) and attributes.map_state == 0,
                        time.monotonic() + 1.5, process=process)
                    assert 'Nop form-service-done' not in commands
                    results['queued-input-services-module-command'] = 'passed'
                    results['service_seconds'] = time.monotonic() - requested
                    # The end marker requires continuing all buffered events,
                    # including on an idle Form with no timer or new fd input.
                    receive(lambda: 'Nop form-service-done' in commands)
                    if not args.idle_only:
                        assert expected_command in commands, commands
                        results['timer-during-queued-input'] = 'passed'
                    results['queued-X-continuation-without-new-input'] = 'passed'
                    results['queued_keys'] = 257
                    results['queued_exposes'] = 80
                if args.history_only:
                    x.XKeysymToKeycode.argtypes = [P, W]
                    x.XKeysymToKeycode.restype = ctypes.c_uint
                    x.XSendEvent.argtypes = [P, W, ctypes.c_int,
                        ctypes.c_long, ctypes.POINTER(Event)]
                    x.XLookupString.argtypes = [ctypes.POINTER(Key),
                        ctypes.c_char_p, ctypes.c_int, ctypes.POINTER(W), P]

                    def key(symbol, state=0):
                        event = Event()
                        event.key.type = 2
                        event.key.display = connection
                        event.key.window = shown
                        event.key.root = root
                        event.key.keycode = x.XKeysymToKeycode(connection, symbol)
                        event.key.same_screen = 1
                        event.key.state = state
                        assert event.key.keycode
                        if symbol in (ord(','), ord('.')) and state == 9:
                            resolved = W()
                            text = ctypes.create_string_buffer(10)
                            x.XLookupString(ctypes.byref(event.key), text,
                                len(text), ctypes.byref(resolved), None)
                            assert resolved.value == (ord('<') if symbol == ord(',')
                                else ord('>')), resolved.value
                        assert x.XSendEvent(connection, shown, 0, 1, ctypes.byref(event))
                        x.XSync(connection, 0)

                    def observe(value):
                        previous = sum(c.startswith('Nop form-history:') for c in commands)
                        key(0xffc9)  # F12
                        receive(lambda: sum(c.startswith('Nop form-history:')
                            for c in commands) > previous)
                        assert [c for c in commands if c.startswith('Nop form-history:')][-1] == (
                            'Nop form-history:fixed/' + value), commands

                    def restart():
                        previous = commands.count('Nop form-history-saved')
                        key(0xffc8)  # F11
                        receive(lambda: commands.count('Nop form-history-saved') > previous)
                        key(0xff09)  # Tab: second input after Restart selects first.

                    key(0xff09)
                    observe('éinit')
                    # Truncation must update bytes; truncating at End is a no-op.
                    key(0xff57)  # End
                    key(ord('k'), 4)  # Control-K
                    key(ord('x'))
                    observe('éinitx')
                    key(ord('a'), 4)
                    key(0xff53)  # Right: past the two-byte initial character.
                    key(ord('k'), 4)
                    key(ord('z'))
                    observe('éz')
                    key(ord('u'), 4)
                    key(ord('a'))
                    observe('a')
                    restart()
                    observe('éinit')
                    key(ord(','), 9)  # Shift+Meta-comma resolves to "<".
                    observe('a')
                    key(0xff57)
                    key(ord('b'))
                    observe('ab')
                    restart()
                    # Different per-field rings: the first stores only "fixed".
                    for index in range(55):
                        key(ord('u'), 4)
                        for character in 'v' + str(index):
                            key(ord(character))
                        observe('v' + str(index))
                        restart()
                        observe('éinit')
                    key(ord(','), 9)
                    observe('v54')
                    key(ord(','), 9)
                    observe('v53')
                    key(ord('.'), 9)
                    observe('v54')
                    for _ in range(60):
                        key(ord(','), 9)
                    key(0xff57)
                    key(ord('q'))
                    # Capture the selected ring value without relying on its slot.
                    previous = sum(c.startswith('Nop form-history:') for c in commands)
                    key(0xffc9)
                    receive(lambda: sum(c.startswith('Nop form-history:')
                        for c in commands) > previous)
                    last = [c for c in commands if c.startswith('Nop form-history:')][-1]
                    assert last.startswith('Nop form-history:fixed/v') and last.endswith('q'), last
                    results['byte-count-clear-truncate-append'] = 'passed'
                    results['recall-restart-and-multibyte-initial'] = 'passed'
                    results['per-field-ring-wrap-and-following-insertion'] = 'passed'
                    results['restart_count'] = 57
                    results['repeat_recall_count'] = 60
                    results['history-key-symbols-verified'] = 'passed'
                if args.button_only or args.fields_only or args.data_only or args.arrays_only or args.geometry_only or args.command_only:
                    x.XKeysymToKeycode.argtypes = [P, W]
                    x.XKeysymToKeycode.restype = ctypes.c_uint
                    x.XSendEvent.argtypes = [P, W, ctypes.c_int,
                        ctypes.c_long, ctypes.POINTER(Event)]
                    event = Event()
                    event.key.type = 2  # KeyPress
                    event.key.display = connection
                    event.key.window = shown
                    event.key.root = root
                    event.key.keycode = x.XKeysymToKeycode(connection, 0xffc9)  # F12
                    event.key.same_screen = 1
                    assert event.key.keycode
                    assert x.XSendEvent(connection, shown, 0, 1, ctypes.byref(event))
                    x.XSync(connection, 0)
                    receive(lambda: expected_button in commands)
                    results['escaped-label-function-key-command'] = 'passed'
                    if args.command_only:
                        emitted = [c for c in commands if c.startswith('Nop form-command-')]
                        assert emitted == ['Nop form-command-before:ready', expected_button], emitted
                        assert not any(c.startswith('Nop $(') for c in commands), commands
                        results['long-name-refusal-following-command'] = 'passed'
                    if args.geometry_only:
                        rr, parent = W(), W()
                        children, count = ctypes.POINTER(W)(), ctypes.c_uint()
                        assert x.XQueryTree(connection, shown, ctypes.byref(rr),
                            ctypes.byref(parent), ctypes.byref(children), ctypes.byref(count))
                        try:
                            assert count.value == 3, count.value  # Two inputs and one button.
                        finally:
                            if children:
                                x.XFree(children)
                        results['refused-widths-preserve-valid-inputs-and-item-count'] = 'passed'
                    if args.arrays_only:
                        emitted = [c for c in commands if c.startswith('Nop form-array-')]
                        assert emitted == ['Nop form-array-' + str(i) for i in range(20)] + [expected_button], emitted
                        results['line-items-choice-order-command-order'] = 'passed'
                        results['line_items'] = 40
                        results['choices'] = 18
                        results['button_commands'] = 21
                    if args.fields_only:
                        results['terminal-fields-and-selected-value'] = 'passed'
                    if args.data_only:
                        results['owned-data-replacement-and-long-request'] = 'passed'
                        results['empty-data-line-and-loaded-value'] = 'passed'
                if args.input_only:
                    # Each refused body is followed by an ordinary valid command.
                    # The timed form must keep processing the input owner.
                    groups = []
                    for kind in [1 << 18, 1 << 17, 1 << 22]:
                        groups.append([packet(kind, (0,) * n) for n in range(4)] +
                            [packet(kind, (0, 0, 0, struct.unpack('@L', b'x' * WORD)[0]))])
                    groups.append([packet((1 << 31) | (1 << 4), (0,) * n)
                        for n in range(3)])
                    for group in groups:
                        for wire in group:
                            os.write(producer, wire)
                        os.write(producer, packet(1 << 18, (0, 0, 0), '*FormTransportUnMap'))
                        wait_for_predicate('Form command after refused packet body',
                            lambda: x.XGetWindowAttributes(connection, shown,
                                ctypes.byref(attributes)) and attributes.map_state == 0,
                            time.monotonic() + .5, process=process)
                        os.write(producer, packet(1 << 18, (0, 0, 0), '*FormTransportMap'))
                        wait_for_predicate('Form remap after refused packet body',
                            lambda: x.XGetWindowAttributes(connection, shown,
                                ctypes.byref(attributes)) and attributes.map_state == 2,
                            time.monotonic() + .5, process=process)
                    for kind in [1 << 18, 1 << 17, 1 << 22]:
                        for text in ['', '\n']:
                            os.write(producer, packet(kind, (0, 0, 0), text))
                    results['packet-groups-followed-by-valid-command'] = len(groups)
                    results['empty-default-config-active-message'] = 'passed'
                start = time.monotonic()
                os.write(producer, packet(1 << 18, (0, 0, 0), '*FormTransportUnMap'))
                while time.monotonic() - start < .5:
                    assert x.XGetWindowAttributes(connection, shown, ctypes.byref(attributes))
                    if attributes.map_state == 0:
                        break
                    assert process.poll() is None
                    time.sleep(.01)
                serviced = attributes.map_state == 0
                results['active-command-before-timeout'] = serviced
                if args.legacy_timer_control:
                    assert not serviced, 'old timer-control unexpectedly serviced input'
                else:
                    assert serviced, 'timed Form did not process active command'
                    os.write(producer, packet(1 << 18, (0, 0, 0), '*FormTransportMap'))
                    wait_for_predicate('Form remap with timer active',
                        lambda: x.XGetWindowAttributes(connection, shown,
                            ctypes.byref(attributes)) and attributes.map_state == 2,
                        time.monotonic() + .5, process=process)
                if args.command_only:
                    wait_for_predicate('refused timer remains available',
                        lambda: time.monotonic() >= timer_start + 2.5,
                        time.monotonic() + 4, process=process)
                    assert x.XGetWindowAttributes(connection, shown, ctypes.byref(attributes))
                    assert attributes.map_state == 2
                    assert not any(c.startswith('Nop $(') for c in commands), commands
                    results['timer-refusal-preserves-mapped-form'] = 'passed'
                elif not args.idle_only:
                    receive(lambda: expected_command in commands)
                    elapsed = time.monotonic() - (timer_start if args.input_only or args.service_only or args.partial_only else start)
                    assert elapsed >= 1, elapsed
                    results['timer-command'] = 'passed'
                    if args.input_only:
                        results['invalid-timeout-refused-valid-escaped-command'] = 'passed'
                    results['elapsed_seconds'] = elapsed
                if args.terminate:
                    process.process.send_signal(signal.SIGTERM)
                else:
                    os.close(producer)
                    producer = -1
                assert process.wait_until_exit(time.monotonic() + 5) == 0
                results['termination' if args.terminate else 'normal-eof'] = 'passed'
            finally:
                for fd in (incoming, producer, consumer, outgoing):
                    if fd >= 0:
                        os.close(fd)
        finally:
            if connection:
                x.XCloseDisplay(connection)
            for p in runtime.root.glob('*.log'):
                shutil.copy2(p, args.artifacts_dir / p.name)
    (args.artifacts_dir / 'evidence.json').write_text(json.dumps(results, indent=2) + '\n')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(allow_abbrev=False)
    parser.add_argument('--module', type=Path, required=True)
    parser.add_argument('--artifacts-dir', type=Path, required=True)
    parser.add_argument('--display')
    parser.add_argument('--legacy-timer-control', action='store_true')
    parser.add_argument('--input-only', action='store_true')
    parser.add_argument('--button-only', action='store_true')
    parser.add_argument('--fields-only', action='store_true')
    parser.add_argument('--data-only', action='store_true')
    parser.add_argument('--arrays-only', action='store_true')
    parser.add_argument('--geometry-only', action='store_true')
    parser.add_argument('--command-only', action='store_true')
    parser.add_argument('--history-only', action='store_true')
    parser.add_argument('--service-only', action='store_true')
    parser.add_argument('--partial-only', action='store_true')
    args = parser.parse_args()
    old_limit = resource.getrlimit(resource.RLIMIT_NOFILE)
    can_high = old_limit[1] == resource.RLIM_INFINITY or old_limit[1] > 4096
    summary = {}
    try:
        labels = ['partial-idle', 'partial-timer'] if args.partial_only else ['service-idle', 'service-timer'] if args.service_only else ['history'] if args.history_only else ['command'] if args.command_only else ['geometry'] if args.geometry_only else ['arrays'] if args.arrays_only else ['data'] if args.data_only else ['fields'] if args.fields_only else ['button'] if args.button_only else ['input'] if args.input_only else ['timer'] if args.legacy_timer_control else ['timer', 'idle-eof', 'idle-term']
        if not args.legacy_timer_control and not args.input_only and not args.button_only and not args.fields_only and not args.data_only and not args.arrays_only and not args.geometry_only and not args.command_only and not args.history_only and not args.service_only and not args.partial_only:
            if can_high:
                if old_limit[0] <= 4096:
                    resource.setrlimit(resource.RLIMIT_NOFILE, (4097, old_limit[1]))
                labels.append('high-descriptor')
            else:
                summary['high-descriptor'] = 'unavailable under inherited hard limit'
        root = args.artifacts_dir
        for label in labels:
            args.idle_only = label not in ('timer', 'input', 'button', 'fields', 'data', 'arrays', 'geometry', 'command', 'service-timer', 'partial-timer')
            args.high_descriptor = label == 'high-descriptor'
            args.terminate = label == 'idle-term'
            args.artifacts_dir = root / label
            run(args)
            summary[label] = json.loads((args.artifacts_dir / 'evidence.json').read_text())
        root.mkdir(parents=True, exist_ok=True)
        (root / 'evidence.json').write_text(json.dumps(summary, indent=2) + '\n')
    finally:
        resource.setrlimit(resource.RLIMIT_NOFILE, old_limit)
