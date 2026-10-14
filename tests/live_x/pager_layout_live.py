#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or any later version.
"""Check Pager grid, shape and scale preparation with real X windows."""
import argparse
import ctypes as C
import ctypes.util
import json
import os
from pathlib import Path
import shutil
import subprocess
import struct
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
    x.XCreateSimpleWindow.argtypes = [P, W, C.c_int, C.c_int,
        C.c_uint, C.c_uint, C.c_uint, C.c_ulong, C.c_ulong]
    x.XCreateSimpleWindow.restype = W
    x.XDestroyWindow.argtypes = [P, W]
    x.XSync.argtypes = [P, C.c_int]
    class ErrorEvent(C.Structure):
        _fields_ = [('type', C.c_int), ('display', P), ('resource', W),
            ('serial', C.c_ulong), ('code', C.c_ubyte),
            ('request', C.c_ubyte), ('minor', C.c_ubyte)]
    errors = []
    ErrorHandler = C.CFUNCTYPE(C.c_int, P, C.POINTER(ErrorEvent))
    @ErrorHandler
    def on_error(display, event):
        item = event.contents
        # Querying a window that disappeared after QueryTree is expected.
        if item.code != 3 or item.request not in (3, 15, 20):
            errors.append((item.code, item.request, item.resource))
        return 0
    x.XSetErrorHandler.argtypes = [P]
    x.XSetErrorHandler.restype = P
    previous_handler = x.XSetErrorHandler(C.cast(on_error, P))
    connection = None
    with HarnessRuntime(prefix='fvwm-pager-layout-') as runtime:
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
                ('shaped-empty-grid', ['0', '0'], 0, True),
                ('extreme-empty-grid', ['0', '0'], 0, True),
                ('invalid-scale-preserves-default', ['0', '0'], 0, True),
                ('invalid-grid-preserves-default', ['0', '0'], 0, True),
                ('window-projection', ['0', '0'], 0, True),
                ('invalid-window-metrics', ['0', '0'], 0, True),
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
                        if label in ['shaped-empty-grid', 'extreme-empty-grid']:
                            grid = '2147483647' if label == 'extreme-empty-grid' else '100suffix'
                            config += ['*FvwmPagerRows ' + grid,
                                '*FvwmPagerColumns ' + grid,
                                '*FvwmPagerGeometry 60000x60000+0+0',
                                '*FvwmPagerShapeLabels']
                        elif label == 'invalid-scale-preserves-default':
                            config = [line for line in config if 'Geometry' not in line]
                            config += ['*FvwmPagerDesktopScale 0',
                                '*FvwmPagerDesktopScale -1',
                                '*FvwmPagerDesktopScale 2147483648']
                        elif label == 'invalid-grid-preserves-default':
                            config += ['*FvwmPagerRows 2147483648',
                                '*FvwmPagerColumns -2147483649']
                        elif label == 'invalid-window-metrics':
                            config += ['*FvwmPagerWindowBorderWidth -1',
                                '*FvwmPagerWindowMinSize -1',
                                '*FvwmPagerWindowBorderWidth 2147483648',
                                '*FvwmPagerWindowMinSize 2147483648',
                                '*FvwmPagerWindowBorderWidth 0',
                                '*FvwmPagerWindowMinSize 0']
                        if desktop_size:
                            config.insert(0, 'DesktopSize 1 1')
                        for line in config:
                            os.write(producer, packet(1 << 18, (0, 0, 0), line))
                        os.write(producer, packet(1 << 19))
                        os.write(producer, packet(1 << 14))
                        window = wait_for_predicate('mapped Pager with valid geometry',
                            popup, time.monotonic() + 10, process=process)
                        if label in ['shaped-empty-grid', 'extreme-empty-grid']:
                            class Rectangle(C.Structure):
                                _fields_ = [('x', C.c_short), ('y', C.c_short),
                                    ('width', C.c_ushort), ('height', C.c_ushort)]
                            extension = C.CDLL(ctypes.util.find_library('Xext'))
                            extension.XShapeGetRectangles.argtypes = [P, W, C.c_int,
                                C.POINTER(C.c_int), C.POINTER(C.c_int)]
                            extension.XShapeGetRectangles.restype = C.POINTER(Rectangle)
                            def shape_ready():
                                count, ordering = C.c_int(), C.c_int()
                                rectangles = extension.XShapeGetRectangles(connection,
                                    window, 0, C.byref(count), C.byref(ordering))
                                try:
                                    return (count.value == 1 and rectangles and
                                        rectangles[0].x == 0 and rectangles[0].y == 0 and
                                        rectangles[0].width == (2 if label == 'extreme-empty-grid' else 600) and
                                        0 < rectangles[0].height < 1000)
                                finally:
                                    if rectangles:
                                        x.XFree(rectangles)
                            wait_for_predicate('single selected-desk shape rectangle',
                                shape_ready, time.monotonic() + 5, process=process)
                        if label == 'window-projection':
                            assert args.fixture
                            target = x.XCreateSimpleWindow(connection, root,
                                0, 0, 5, 5, 0, 0, 0)
                            x.XSync(connection, False)
                            try:
                                wire = bytearray(subprocess.check_output(
                                    [str(args.fixture.resolve()), str(target)], timeout=5))
                                struct.pack_into('@L', wire, (4 + 8) * WORD, output)
                                for px, py in [(10, 20), (2147483647, -2147483648),
                                               (-2147483648, 2147483647), (10, 20)]:
                                    struct.pack_into('@l', wire, (4 + 3) * WORD, px)
                                    struct.pack_into('@l', wire, (4 + 4) * WORD, py)
                                    os.write(producer, wire)
                                    os.write(producer, packet(1 << 1,
                                        ((-2147483648) & ((1 << (8 * WORD)) - 1), output)))
                                    wait_for_predicate('configured window followed by UI',
                                        lambda: title(window) == 'minimum',
                                        time.monotonic() + 5, process=process)
                                    os.write(producer, packet(1 << 1, (0, output)))
                                    wait_for_predicate('ordinary desk restored',
                                        lambda: title(window) == 'FvwmPager',
                                        time.monotonic() + 5, process=process)
                                os.write(producer, packet(1 << 7, (target,)))
                            finally:
                                x.XDestroyWindow(connection, target)
                                x.XSync(connection, False)
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
                        ['AddressSanitizer', 'runtime error:', 'Assertion', 'X error', 'X Error']), text
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
            x.XSetErrorHandler(previous_handler)
            for path in runtime.root.glob('*.log'):
                shutil.copy2(path, args.artifacts_dir / path.name)
    assert not errors, errors
    (args.artifacts_dir / 'evidence.json').write_text(json.dumps(results, indent=2) + '\n')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(allow_abbrev=False)
    parser.add_argument('--module', type=Path, required=True)
    parser.add_argument('--artifacts-dir', type=Path, required=True)
    parser.add_argument('--limit', type=int, default=4096)
    parser.add_argument('--display')
    parser.add_argument('--fixture', type=Path)
    arguments = parser.parse_args()
    assert 1 <= arguments.limit <= 65536
    run(arguments)
