#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or any later version.
"""Check actual Rearrange monitor preparation through owned module pipes."""
import argparse
import ctypes as C
import ctypes.util
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import time

from colorset_live import WindowAttributes
from live_x_harness import (HarnessRuntime, parse_xrandr_monitors,
    start_display_server, wait_for_predicate)
from script_transport_live import packet


def run(args):
    args.artifacts_dir.mkdir(parents=True, exist_ok=True)
    (args.artifacts_dir / 'evidence.json').unlink(missing_ok=True)
    results = {}
    with HarnessRuntime(prefix='fvwm-rearrange-monitor-') as runtime:
        connection = None
        x = C.CDLL(ctypes.util.find_library('X11'))
        x.XOpenDisplay.argtypes = [C.c_char_p]
        x.XOpenDisplay.restype = C.c_void_p
        x.XDefaultRootWindow.argtypes = [C.c_void_p]
        x.XDefaultRootWindow.restype = C.c_ulong
        x.XGetWindowAttributes.argtypes = [C.c_void_p, C.c_ulong,
                                         C.POINTER(WindowAttributes)]
        x.XCloseDisplay.argtypes = [C.c_void_p]
        try:
            display = args.display
            if display is None:
                _, display = start_display_server(runtime, 'Xvfb',
                    ['Xvfb', '-screen', '0', '800x600x24', '-noreset'],
                    'xserver.log', time.monotonic() + 10)
            connection = x.XOpenDisplay(display.encode())
            assert connection
            attributes = WindowAttributes()
            assert x.XGetWindowAttributes(connection,
                x.XDefaultRootWindow(connection), C.byref(attributes))
            listed = subprocess.check_output(['xrandr', '--display', display,
                '--listmonitors'], text=True, timeout=5)
            monitors = parse_xrandr_monitors(listed)
            if monitors:
                name, monitor = next(iter(monitors.items()))
                initial = [monitor[k] for k in ('x', 'y', 'width', 'height')]
            else:
                name = 'FVWM-X-Root'
                initial = [0, 0, attributes.width, attributes.height]
            quoted = "'" + name.replace('\\', '\\\\').replace("'", "\\'") + "'"
            cases = [('invalid-preserves-initial', [], initial + [0]),
                     ('valid-insets-then-invalid', [
                         f'Monitor {quoted} 0 1 1 0 0 0 0 -2 800 600 10 20 30 40'],
                         [initial[0] + 10, initial[1] + 30,
                          initial[2] - 30, initial[3] - 70, -2]),
                     ('ignore-work-area', [
                         f'Monitor {quoted} 0 1 1 0 0 0 0 -2 800 600 10 20 30 40'],
                         initial + [-2])]
            invalid = ['Monitor', 'Monitor unknown 0 1 1',
                f'Monitor {quoted} 0 1 1',
                f'Monitor {quoted} 2147483648 1 1 0 0 0 0 9 800 600 0 0 0 0',
                f'Monitor {quoted} 0 1 1 0 0 0 0 9 800 600 0 0 0 x',
                f'Monitor {quoted} 0 1 1 0 0 0 0 9 800 600 2147483647 0 0 0',
                f'Monitor {quoted} 0 1 1 0 0 0 0 9 800 600 0 0 2147483647 0',
                f'Monitor {quoted} 0 1 1 0 0 0 0 9 800 600 -2147483648 -2147483648 0 0']
            for label, valid, expected in cases:
                incoming, producer = os.pipe()
                consumer, outgoing = os.pipe()
                try:
                    options = ['-screen', name]
                    if label == 'ignore-work-area':
                        options += ['-ewmhiwa']
                    process = runtime.start(label, [str(args.probe),
                        str(outgoing), str(incoming), '/dev/null', '0', '0'] + options,
                        label + '.log', pass_fds=(incoming, outgoing),
                        environment=dict(os.environ, DISPLAY=display))
                    os.close(incoming)
                    incoming = -1
                    os.close(outgoing)
                    outgoing = -1
                    wait_for_predicate(label, lambda:
                        'rearrange-configuration-read: ready' in process.log_path.read_text(),
                        time.monotonic() + 10, process=process)
                    # Extreme insets are meaningful when they are applied.
                    lines = valid + (invalid if label != 'ignore-work-area' else invalid[:5])
                    for line in lines:
                        os.write(producer, packet(1 << 18, (0, 0, 0), line))
                    os.write(producer, packet(1 << 19))
                    marker = 'rearrange-original-ten-second-deadline: armed'
                    wait_for_predicate(label, lambda:
                        marker in process.log_path.read_text(),
                        time.monotonic() + 10, process=process)
                    os.close(producer)
                    producer = -1
                    assert process.wait_until_exit(time.monotonic() + 2) == 0
                    text = process.log_path.read_text()
                    match = re.search(r'rearrange-prepared-area: (-?\d+) (-?\d+) (-?\d+) (-?\d+) (-?\d+)', text)
                    assert match and list(map(int, match.groups())) == expected, text
                    assert text.count('rearrange-normal-flow-cleanup: passed') == 1
                    assert all(bad not in text for bad in
                        ('AddressSanitizer', 'runtime error:', 'Assertion'))
                    results[label] = {'prepared-area': expected,
                                      'normal-eof': 'passed'}
                finally:
                    for descriptor in (incoming, producer, consumer, outgoing):
                        if descriptor >= 0:
                            os.close(descriptor)
        finally:
            if connection:
                x.XCloseDisplay(connection)
            for path in runtime.root.glob('*.log'):
                shutil.copy2(path, args.artifacts_dir / path.name)
    (args.artifacts_dir / 'evidence.json').write_text(
        json.dumps(results, indent=2) + '\n')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(allow_abbrev=False)
    parser.add_argument('--probe', type=Path, required=True)
    parser.add_argument('--artifacts-dir', type=Path, required=True)
    parser.add_argument('--display')
    run(parser.parse_args())
