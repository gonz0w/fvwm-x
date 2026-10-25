#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or any later version.
"""Real Script outgoing selection ownership and shutdown regression controls.

The silent case measures the shutdown grace; a baseline unrelated X event
can exhaust its retry counter. Drain and empty controls distinguish the
delivery and fixed-sleep regressions. The harness owns and reaps all children.
"""
import argparse
import ctypes as C
import ctypes.util
import json
import os
from pathlib import Path
import selectors
import shutil
import signal
import struct
import time

from live_x_harness import HarnessRuntime, start_display_server, wait_for_predicate
from script_transport_live import packet, WORD


class Selection(C.Structure):
    _fields_ = [('type', C.c_int), ('serial', C.c_ulong),
        ('send_event', C.c_int), ('display', C.c_void_p),
        ('requestor', C.c_ulong), ('selection', C.c_ulong),
        ('target', C.c_ulong), ('property', C.c_ulong), ('time', C.c_ulong)]


class Event(C.Union):
    _fields_ = [('type', C.c_int), ('selection', Selection),
        ('padding', C.c_long * 24)]


def xlib():
    x = C.CDLL(ctypes.util.find_library('X11'))
    p, w, i = C.c_void_p, C.c_ulong, C.c_int
    signatures = {
        'XOpenDisplay': ([C.c_char_p], p),
        'XDefaultRootWindow': ([p], w),
        'XCreateSimpleWindow': ([p, w, i, i, C.c_uint, C.c_uint,
            C.c_uint, w, w], w),
        'XInternAtom': ([p, C.c_char_p, i], w),
        'XSetSelectionOwner': ([p, w, w, w], i),
        'XGetSelectionOwner': ([p, w], w),
        'XConvertSelection': ([p, w, w, w, w, w], i),
        'XCheckTypedEvent': ([p, i, C.POINTER(Event)], i),
        'XGetWindowProperty': ([p, w, w, C.c_long, C.c_long, i, w,
            C.POINTER(w), C.POINTER(i), C.POINTER(w), C.POINTER(w),
            C.POINTER(p)], i),
        'XFlush': ([p], i), 'XSync': ([p, i], i),
        'XFree': ([p], i), 'XCloseDisplay': ([p], i),
        'XDestroyWindow': ([p, w], i),
    }
    for name, (arguments, result) in signatures.items():
        getattr(x, name).argtypes = arguments
        getattr(x, name).restype = result
    return x


def run(args):
    args.artifacts_dir.mkdir(parents=True, exist_ok=True)
    evidence = {}
    with HarnessRuntime(prefix='fvwm-script-message-') as runtime:
        x, display_handle = xlib(), None
        try:
            display = args.display
            if display is None:
                _, display = start_display_server(runtime, 'Xvfb',
                    ['Xvfb', '-screen', '0', '800x600x24', '-noreset'],
                    'xserver.log', time.monotonic() + 10)
            display_handle = x.XOpenDisplay(display.encode())
            assert display_handle
            root = x.XDefaultRootWindow(display_handle)
            cases = [args.case] if args.case != 'all' else [
                'silent', 'drain', 'invalid', 'cancel', 'empty']
            for label in cases:
                window = x.XCreateSimpleWindow(display_handle, root,
                    0, 0, 1, 1, 0, 0, 0)
                # A child Script's argv contains a literal non-UTF8 prefix,
                # exactly as LaunchScript constructs it; its parent owns this
                # selection but deliberately does not service wake requests.
                parent_name = b'\xa1message-peer-' + label.encode()
                child_name = parent_name + b'00'
                parent = x.XInternAtom(display_handle, parent_name, False)
                child = x.XInternAtom(display_handle, child_name, False)
                target = parent
                invalid = x.XInternAtom(display_handle, b'unqueued-target', False)
                prop = x.XInternAtom(display_handle, b'message-result', False)
                x.XSetSelectionOwner(display_handle, parent, window, 0)
                x.XSync(display_handle, False)
                report = runtime.root / (label + '.report')
                script = runtime.root / (label + '.script')
                script.write_text('WindowTitle {message-owner}\n'
                    'WindowSize 100 40\nFont {fixed}\nInit\nBegin\n'
                    ' Set $file={' + str(report) + '}\n'
                    ' WriteToFile $file {ready}\nEnd\n'
                    'QuitFunc\nBegin\n WriteToFile $file {quitting}\nEnd\n'
                    'Widget 1\nProperty\n Type Rectangle\n'
                    ' Position 1 1\n Size 10 10\nMain\nCase message of\n'
                    '1 :\nBegin\n For $i=1 To 41 Do\n Begin\n'
                    ' SendToScript 0 {literal # $ message:} $i\n End\n'
                    ' WriteToFile $file {queued}\nEnd\n'
                    '2 :\nBegin\n Quit\nEnd\nEnd\n')
                shutil.copy2(script, args.artifacts_dir / script.name)
                incoming, producer = os.pipe()
                consumer, outgoing = os.pipe()
                try:
                    process = runtime.start(label, [str(args.module.resolve()),
                        str(outgoing), str(incoming), '/dev/null', '0', '0',
                        str(script), os.fsdecode(child_name)], label + '.log',
                        pass_fds=(incoming, outgoing),
                        environment=runtime.environment({'DISPLAY': display}))
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
                                assert data, commands
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
                    os.write(producer, packet(1 << 19))
                    receive(lambda: 'NOP FINISHED STARTUP' in commands)
                    wait_for_predicate('Script initialization', report.exists,
                        time.monotonic() + 10, process=process)
                    # Let initial map/expose traffic finish before Quit. The old
                    # loop can exhaust its unrelated-event counter while leaving
                    # one queued X event untouched, concealing the silent-peer hang.
                    time.sleep(.2)
                    if label != 'empty':
                        os.write(producer, packet(1 << 22, (0, 0, 0),
                            'SendString 1 1'))
                        wait_for_predicate('queued messages',
                            lambda: 'queued' in report.read_text(),
                            time.monotonic() + 10, process=process)
                        wakes = 0
                        event = Event()
                        deadline = time.monotonic() + 5
                        while wakes < 40:
                            if x.XCheckTypedEvent(display_handle, 30, C.byref(event)):
                                wakes += 1
                            else:
                                assert time.monotonic() < deadline, wakes
                                time.sleep(.001)
                        evidence[label + '-admitted'] = wakes
                        time.sleep(.2)
                    sender = x.XGetSelectionOwner(display_handle, child)
                    assert sender not in (0, root)
                    start = time.monotonic()
                    os.write(producer, packet(1 << 22, (0, 0, 0),
                        'SendString 1 2'))
                    wait_for_predicate('QuitFunc entered',
                        lambda: 'quitting' in report.read_text(),
                        start + 10, process=process)

                    def request(requested, property_atom):
                        x.XConvertSelection(display_handle, child, requested,
                            property_atom, window, 0)
                        x.XFlush(display_handle)
                        event = Event()
                        deadline = time.monotonic() + 1
                        while not x.XCheckTypedEvent(display_handle, 31,
                                C.byref(event)):
                            assert time.monotonic() < deadline, label
                            assert process.poll() is None, label
                            time.sleep(.001)
                        reply = event.selection
                        assert reply.requestor == window
                        assert reply.selection == child
                        assert reply.target == requested
                        assert reply.time == 0
                        return reply.property

                    if label == 'drain':
                        # Refusal must retain the queue, and echo the target.
                        assert request(invalid, prop) == 0
                        for index in range(1, 41):
                            property_atom = prop if index % 2 else 0
                            actual = request(target, property_atom)
                            assert actual == (property_atom or target)
                            actual_type, count, after = (C.c_ulong() for _ in range(3))
                            form, data = C.c_int(), C.c_void_p()
                            assert x.XGetWindowProperty(display_handle, window,
                                actual, 0, 8192, True, target,
                                C.byref(actual_type), C.byref(form), C.byref(count),
                                C.byref(after), C.byref(data)) == 0
                            try:
                                assert actual_type.value == target and form.value == 8
                                assert after.value == 0
                                assert C.string_at(data, count.value) == (
                                    f'literal # $ message:{index}'.encode() + b'\0')
                            finally:
                                if data:
                                    x.XFree(data)
                        assert process.wait_until_exit(time.monotonic() + 1.5) == 0
                    elif label == 'invalid':
                        refusals = 0
                        while process.poll() is None and time.monotonic() - start < 1.5:
                            assert request(invalid, prop) == 0
                            refusals += 1
                            time.sleep(.01)
                        assert refusals > 0
                        # One second of scheduler allowance still rejects a
                        # deadline restarted by the last refusal (~3.5s).
                        assert process.wait_until_exit(start + 3) == 0
                        evidence['invalid-refusals'] = refusals
                    elif label == 'cancel':
                        process.process.send_signal(signal.SIGTERM)
                        assert process.wait_until_exit(time.monotonic() + 1) == 0
                    else:
                        assert process.wait_until_exit(start + (1.5 if label == 'empty' else 4)) == 0
                    elapsed = time.monotonic() - start
                    if label in ('silent', 'invalid'):
                        assert elapsed >= 1.9, elapsed
                    assert x.XGetSelectionOwner(display_handle, child) == 0
                    text = process.log_path.read_text()
                    assert not any(marker in text for marker in
                        ('AddressSanitizer', 'runtime error:', 'Assertion')), text
                    evidence[label] = {'status': 'passed', 'seconds': elapsed}
                finally:
                    for descriptor in (incoming, producer, consumer, outgoing):
                        if descriptor >= 0:
                            os.close(descriptor)
                    x.XDestroyWindow(display_handle, window)
                    x.XSync(display_handle, False)
        finally:
            if display_handle:
                x.XCloseDisplay(display_handle)
            for path in runtime.root.glob('*.log'):
                shutil.copy2(path, args.artifacts_dir / path.name)
            (args.artifacts_dir / 'evidence.json').write_text(
                json.dumps(evidence, indent=2) + '\n')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(allow_abbrev=False)
    parser.add_argument('--module', type=Path, required=True)
    parser.add_argument('--artifacts-dir', type=Path, required=True)
    parser.add_argument('--display')
    parser.add_argument('--case', choices=('all', 'silent', 'drain', 'invalid',
        'cancel', 'empty'), default='all')
    run(parser.parse_args())
