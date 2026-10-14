#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or any later version.
"""Check Pager's desk allowance, incomplete startup and extreme desk updates."""
import argparse
import ctypes as C
import ctypes.util
import json
import os
from pathlib import Path
import shutil
import subprocess
import time

from colorset_live import WindowAttributes
from live_x_harness import (
    HarnessRuntime, parse_xrandr_monitors, parse_xrandr_output_ids,
    start_display_server, wait_for_predicate,
)
from script_transport_live import packet, WORD


def run(args):
    args.module = args.module.resolve()
    args.artifacts_dir.mkdir(parents=True, exist_ok=True)
    (args.artifacts_dir / 'evidence.json').unlink(missing_ok=True)
    results = {}
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
    x.XCloseDisplay.argtypes = [P]
    connection = None
    with HarnessRuntime(prefix='fvwm-pager-startup-') as runtime:
        try:
            display = args.display
            if display is None:
                _, display = start_display_server(runtime, 'Xvfb',
                    ['Xvfb', '-screen', '0', '800x600x24', '-noreset'],
                    'xserver.log', time.monotonic() + 10)
            connection = x.XOpenDisplay(display.encode())
            assert connection
            root = x.XDefaultRootWindow(connection)
            listing = subprocess.check_output(['xrandr', '--display', display,
                '--verbose'], text=True, timeout=5)
            monitor_listing = subprocess.check_output(
                ['xrandr', '--display', display, '--listmonitors'],
                text=True, timeout=5)
            monitors = parse_xrandr_monitors(monitor_listing)
            monitor = next((m for m in monitors.values() if m['primary']),
                           next(iter(monitors.values()), None))
            # A server can expose outputs without any RandR monitors. The
            # production root-sized fallback has no output ID in that case.
            names = monitor['output_names'] if monitor is not None else []
            output = parse_xrandr_output_ids(listing)[names[0]] if names else 0
            (args.artifacts_dir / 'monitor-topology.json').write_text(json.dumps(
                {'monitors': monitors, 'selected_output': output,
                 'monitor_listing': monitor_listing, 'output_listing': listing},
                indent=2) + '\n')

            def title(window):
                name = P()
                if not x.XFetchName(connection, window, C.byref(name)):
                    return None
                try:
                    return C.string_at(name).decode() if name.value else None
                finally:
                    if name.value:
                        x.XFree(name)

            def popup():
                root_return, parent, count = W(), W(), C.c_uint()
                children = C.POINTER(W)()
                assert x.XQueryTree(connection, root, C.byref(root_return),
                    C.byref(parent), C.byref(children), C.byref(count))
                try:
                    for i in range(count.value):
                        attributes = WindowAttributes()
                        if (title(children[i]) == 'FvwmPager' and
                                x.XGetWindowAttributes(connection, children[i],
                                    C.byref(attributes)) and attributes.map_state == 2):
                            assert attributes.width > 0 and attributes.height > 0
                            return children[i]
                finally:
                    if children:
                        x.XFree(children)
                return None

            cases = [
                ('configured-cap', ['0', str(args.limit)], 2, True),
                ('arithmetic-cap', ['0', '2147483647'], 2, True),
                ('integer-overflow', ['2147483648'], 2, True),
                ('ordinary', ['0', '0'], 0, True),
                ('high-desk-ids', ['2147483647'], 0, True),
                ('missing-desktop-size', ['0', '0'], 0, False),
            ]
            for label, values, expected, desktop_size in cases:
                incoming, producer = os.pipe()
                consumer, outgoing = os.pipe()
                try:
                    process = runtime.start(label, [str(args.module), str(outgoing),
                        str(incoming), '/dev/null', '0', '0'] + values,
                        label + '.log', pass_fds=(incoming, outgoing),
                        environment=dict(os.environ, DISPLAY=display))
                    os.close(incoming)
                    incoming = -1
                    os.close(outgoing)
                    outgoing = -1
                    if expected == 0:
                        config = ['*FvwmPagerFont fixed',
                            '*FvwmPagerGeometry 200x100+0+0',
                            'DesktopName ' + values[0] + ' FvwmPager',
                            'DesktopName -2147483648 minimum',
                            'DesktopName 2147483647 maximum']
                        if label == 'high-desk-ids':
                            config.append('DesktopName 2147483647 FvwmPager')
                        if desktop_size:
                            config.insert(0, 'DesktopSize 1 1')
                        for line in config:
                            os.write(producer, packet(1 << 18, (0, 0, 0), line))
                        os.write(producer, packet(1 << 19))
                        os.write(producer, packet(1 << 14))
                        window = wait_for_predicate('mapped Pager with valid geometry',
                            popup, time.monotonic() + 10, process=process)
                        if label != 'high-desk-ids':
                            for desk, name in [(-2147483648, 'minimum'),
                                               (2147483647, 'maximum'), (0, 'FvwmPager')]:
                                encoded = desk & ((1 << (8 * WORD)) - 1)
                                os.write(producer, packet(1 << 1, (encoded, output)))
                                wait_for_predicate('desk update reflected in Pager title',
                                    lambda: title(window) == name,
                                    time.monotonic() + 5, process=process)
                        os.close(producer)
                        producer = -1
                    assert process.wait_until_exit(time.monotonic() + 10) == expected
                    text = process.log_path.read_text()
                    assert all(s not in text for s in
                        ['AddressSanitizer', 'runtime error:', 'Assertion', 'X error']), text
                    if expected == 2:
                        assert 'desk range exceeds the configured limit or is invalid' in text
                    results[label] = 'passed'
                finally:
                    for fd in [incoming, producer, consumer, outgoing]:
                        if fd >= 0:
                            os.close(fd)
        finally:
            if connection:
                x.XCloseDisplay(connection)
            for path in runtime.root.glob('*.log'):
                shutil.copy2(path, args.artifacts_dir / path.name)
    (args.artifacts_dir / 'evidence.json').write_text(json.dumps(results, indent=2) + '\n')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(allow_abbrev=False)
    parser.add_argument('--module', type=Path, required=True)
    parser.add_argument('--artifacts-dir', type=Path, required=True)
    parser.add_argument('--limit', type=int, required=True)
    parser.add_argument('--display')
    arguments = parser.parse_args()
    assert 1 <= arguments.limit <= 65536
    run(arguments)
