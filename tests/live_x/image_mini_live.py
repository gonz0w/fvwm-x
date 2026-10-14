#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
"""Observe deferred file/mini-icons, displayed pixels and EWMH donation."""
import argparse
import ctypes as C
import json
from pathlib import Path
import shutil
import time

from live_x_harness import HarnessRuntime, start_display_server, wait_for_predicate
from mfl_stream_live import Connection
from geometry_transition_live import Client
from colorset_live import WindowAttributes


class XColor(C.Structure):
    _fields_ = [('pixel', C.c_ulong), ('red', C.c_ushort),
        ('green', C.c_ushort), ('blue', C.c_ushort),
        ('flags', C.c_ubyte), ('pad', C.c_ubyte)]


class WMHints(C.Structure):
    _fields_ = [('flags', C.c_long), ('input', C.c_int),
        ('initial_state', C.c_int), ('icon_pixmap', C.c_ulong),
        ('icon_window', C.c_ulong), ('icon_x', C.c_int), ('icon_y', C.c_int),
        ('icon_mask', C.c_ulong), ('window_group', C.c_ulong)]


def run(args, factor):
    out = args.artifacts_dir / f'scale-{factor}'
    out.mkdir(parents=True, exist_ok=True)
    evidence = {}
    with HarnessRuntime(prefix='fvwm-image-mini-') as runtime:
        client = connection = core = None
        try:
            _, display = start_display_server(runtime, 'Xvfb',
                ['Xvfb', '-screen', '0', f'1000x800x{args.depth}', '-noreset'],
                'xserver.log', time.monotonic() + 10)
            sources = {}
            for name in ('red', 'blue', 'stall', 'incomplete'):
                sources[name] = runtime.root / (name + '.svg')
                sources[name].write_text(name)
            config = runtime.root / 'config'
            feature = 'Icon' if args.file_icon else 'MiniIcon'
            style = ('IconOverride, EWMHDonateIcon, IconSize 20 20 Shrunk'
                if args.file_icon else 'EWMHDonateMiniIcon')
            config.write_text(f'ModulePath {args.mfl.parent}:{args.probe.parent}\n'
                f'MonitorScale "screen" {factor}\nDefaultFont fixed\n'
                'Colorset 0 fg white, bg black\n'
                f'Style * TileCascadePlacement, Colorset 0, IconTitleColorset 0, {style}\n'
                'ButtonStyle 1 MiniIcon\n'
                'AddToFunc StartFunction I Module FvwmMFL\n')
            core = runtime.start('core', [str(args.fvwm), '-d', display,
                '-f', str(config), '-v', '-o', str(runtime.root / 'debug.log')],
                'core.log', runtime.environment({
                    'FVWMMFL_SOCKET_PATH': str(runtime.root),
                    'FVWM_IMAGE_SVG_WORKER': str(args.worker)}))
            endpoint = runtime.root / f"fvwm_mfl_{display.replace(':', '_')}.sock"
            wait_for_predicate('mini fixture endpoint', endpoint.exists,
                time.monotonic() + 10, process=core)
            connection = Connection(endpoint)
            connection.record()
            connection.send(b'set echo\n')
            sequence = 0

            def command(text):
                nonlocal sequence
                sequence += 1
                connection.send((text + '\n').encode())
                connection.echo(f'mini-barrier-{sequence}')

            report = runtime.root / 'mini-icons.txt'
            command(f'Module {args.probe.name} {report}')
            wait_for_predicate('mini observer ready', report.exists,
                time.monotonic() + 5, process=core)
            command(f'Style * {feature} {sources["stall"]}')
            client = Client(display)
            client.create()
            wait_for_predicate('mini client frame', client.frame,
                time.monotonic() + 5, process=core)
            if args.file_icon:
                command(f'WindowId {client.window} Iconify')
            x, d = client.x, client.display
            errors = []
            callback_type = C.CFUNCTYPE(C.c_int, C.c_void_p, C.c_void_p)

            @callback_type
            def error_handler(_display, _event):
                errors.append('X request failed')
                return 0

            x.XSetErrorHandler.argtypes = [callback_type]
            x.XSetErrorHandler(error_handler)
            x.XGetImage.argtypes = [C.c_void_p, C.c_ulong, C.c_int, C.c_int,
                C.c_uint, C.c_uint, C.c_ulong, C.c_int]
            x.XGetImage.restype = C.c_void_p
            x.XGetPixel.argtypes = [C.c_void_p, C.c_int, C.c_int]
            x.XGetPixel.restype = C.c_ulong
            x.XDestroyImage.argtypes = [C.c_void_p]
            x.XInternAtom.argtypes = [C.c_void_p, C.c_char_p, C.c_int]
            x.XInternAtom.restype = C.c_ulong
            x.XGetWindowProperty.argtypes = [C.c_void_p, C.c_ulong, C.c_ulong,
                C.c_long, C.c_long, C.c_int, C.c_ulong, C.POINTER(C.c_ulong),
                C.POINTER(C.c_int), C.POINTER(C.c_ulong), C.POINTER(C.c_ulong),
                C.POINTER(C.c_void_p)]
            atom = x.XInternAtom(d, b'_NET_WM_ICON', 0)
            x.XGetWindowAttributes.argtypes = [C.c_void_p, C.c_ulong,
                C.POINTER(WindowAttributes)]
            x.XQueryColors.argtypes = [C.c_void_p, C.c_ulong,
                C.POINTER(XColor), C.c_int]

            def donated():
                actual, count, after = C.c_ulong(), C.c_ulong(), C.c_ulong()
                fmt, data = C.c_int(), C.c_void_p()
                assert x.XGetWindowProperty(d, client.window, atom, 0, 10000,
                    0, 6, C.byref(actual), C.byref(fmt), C.byref(count),
                    C.byref(after), C.byref(data)) == 0
                try:
                    assert not after.value and not errors
                    return tuple(C.cast(data, C.POINTER(C.c_ulong))[i]
                        for i in range(count.value)) if data.value else ()
                finally:
                    if data.value:
                        x.XFree(data)

            def packets():
                return [tuple(map(int, line.split()))
                    for line in report.read_text().splitlines(keepends=True)
                    if line.endswith('\n') and len(line.split()) == 4]

            x.XGrabServer.argtypes = [C.c_void_p]
            x.XUngrabServer.argtypes = [C.c_void_p]

            def file_window(color):
                # Keep the observer tree and its attributes/pixels consistent
                # while the WM replaces an icon window.
                x.XGrabServer(d)
                root, parent = C.c_ulong(), C.c_ulong()
                children, count = C.POINTER(C.c_ulong)(), C.c_uint()
                assert x.XQueryTree(d, client.root, C.byref(root), C.byref(parent),
                    C.byref(children), C.byref(count))
                try:
                    for i in range(count.value):
                        attrs = WindowAttributes()
                        window = children[i]
                        assert x.XGetWindowAttributes(d, window, C.byref(attrs))
                        if attrs.map_state != 2 or not (10 < attrs.width < 80 and
                                10 < attrs.height < 80):
                            continue
                        picture = x.XGetImage(d, window, 0, 0, attrs.width,
                            attrs.height, C.c_ulong(-1).value, 2)
                        assert picture and not errors
                        try:
                            samples = [x.XGetPixel(picture, px, py)
                                for py in range(attrs.height)
                                for px in range(attrs.width)]
                        finally:
                            x.XDestroyImage(picture)
                        palette = (XColor * len(set(samples)))()
                        for entry, value in zip(palette, set(samples)):
                            entry.pixel = value
                        assert x.XQueryColors(d, attrs.colormap, palette, len(palette))
                        matching = {entry.pixel for entry in palette if (
                            (entry.red >> 8) << 16 | (entry.green >> 8) << 8 |
                            (entry.blue >> 8)) == color}
                        if sum(p in matching for p in samples) >= 100:
                            return (window, attrs.width, attrs.height)
                finally:
                    if children:
                        x.XFree(children)
                    x.XUngrabServer(d)
                    x.XSync(d, 0)
                return None

            def completed(color, start):
                def ready():
                    if args.file_icon:
                        icon = donated()
                        window = file_window(color)
                        return window if window and icon[:2] == (20, 20) and (
                            icon[2] & 0xffffff) == color else None
                    rows = packets()[start:]
                    if not rows:
                        return None
                    row = rows[-1]
                    assert row[0] == client.window and row[2:] == (16 * factor,) * 2
                    picture = x.XGetImage(d, row[1], 0, 0, row[2], row[3],
                        C.c_ulong(-1).value, 2)
                    assert picture and not errors
                    try:
                        pixel = x.XGetPixel(picture, 0, 0)
                    finally:
                        x.XDestroyImage(picture)
                    icon = donated()
                    return row if pixel == color and icon[:2] == row[2:] and (
                        icon[2] & 0xffffff) == color else None
                return wait_for_predicate('ready mini packet and donation', ready,
                    time.monotonic() + 8, process=core)

            command('Nop')
            assert not packets() and not donated()
            evidence['pending-first-load-has-no-packet-or-donation'] = 'passed'
            command(f'Style * {feature} {sources["red"]}')
            red = completed(0xff0000, 0)
            old = donated()
            command(f'Style * {feature} {sources["stall"]}')
            count = len(packets())
            if args.file_icon:
                assert file_window(0xff0000)[1:] == red[1:] and donated() == old
            else:
                assert packets()[-1] == red and donated() == old
            command('Nop')
            assert len(packets()) == count
            evidence['pending-replacement-retains-published-icon'] = 'passed'
            command(f'Style * {feature} {sources["blue"]}')
            blue = completed(0x0000ff, count)
            command(f'Style * {feature} {sources["incomplete"]}')
            count = len(packets())
            wait_for_predicate('mini refusal',
                lambda: 'SVG refused:' in (runtime.root / 'debug.log').read_text(),
                time.monotonic() + 8, process=core)
            command('Nop')
            assert len(packets()) == count
            if args.file_icon:
                assert file_window(0x0000ff)[1:] == blue[1:]
                for mode in ('Adjusted', 'Stretched', 'Shrunk'):
                    command(f'Style * IconSize 20 20 {mode}')
                    completed(0x0000ff, count)
                evidence['derived-icon-resize-ownership-and-donation'] = 'passed'
                command(f'Style * Icon {sources["stall"]}')
                command('Style * NoIconOverride, EWMHDontDonateIcon')
                x.XDefaultColormap.argtypes = [C.c_void_p, C.c_int]
                x.XDefaultColormap.restype = C.c_ulong
                x.XAllocNamedColor.argtypes = [C.c_void_p, C.c_ulong,
                    C.c_char_p, C.POINTER(XColor), C.POINTER(XColor)]
                actual, exact = XColor(), XColor()
                assert x.XAllocNamedColor(d, x.XDefaultColormap(d, 0), b'green',
                    C.byref(actual), C.byref(exact))
                application_icon = x.XCreateSimpleWindow(d, client.root,
                    700, 600, 20, 20, 0, 0, actual.pixel)
                x.XSetWMHints.argtypes = [C.c_void_p, C.c_ulong,
                    C.POINTER(WMHints)]
                hint = WMHints(flags=1 << 3, icon_window=application_icon)
                x.XSetWMHints(d, client.window, C.byref(hint))
                x.XSync(d, 0)
                wait_for_predicate('application icon supersedes file lease',
                    lambda: file_window(0x00ff00), time.monotonic() + 8,
                    process=core)
                command('Nop')
                assert file_window(0x00ff00)
                evidence['application-icon-supersedes-pending-file'] = 'passed'
                hint.flags = 0
                x.XSetWMHints(d, client.window, C.byref(hint))
                x.XSync(d, 0)
                command(f'Style * IconOverride, EWMHDonateIcon, Icon {sources["blue"]}')
                blue = completed(0x0000ff, count)
            else:
                assert packets()[-1] == blue
            assert (donated()[2] & 0xffffff) == 0x0000ff
            evidence['refusal-retains-pixels-and-donation'] = 'passed'
            command(f'Style * {feature} {sources["stall"]}')
            client.destroy()
            command(f'Style * {feature} {sources["red"]}')
            client.create()
            wait_for_predicate('replacement client frame', client.frame,
                time.monotonic() + 5, process=core)
            if args.file_icon:
                command(f'WindowId {client.window} Iconify')
            completed(0xff0000, count)
            evidence['destroyed-client-cancels-lease-next-owner-completes'] = 'passed'
            assert not errors and core.poll() is None
            assert 'X error' not in (runtime.root / 'debug.log').read_text()
        except BaseException as error:
            evidence['error'] = str(error)
            evidence['wm_exit_status'] = core.poll() if core else None
            raise
        finally:
            if client:
                client.close()
            if connection:
                connection.close()
            for path in runtime.root.glob('*.log'):
                shutil.copy2(path, out / path.name)
            (out / 'evidence.json').write_text(json.dumps(evidence, indent=2) + '\n')
    print(f'{feature} completion, depth {args.depth}, scale {factor} checks passed')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(allow_abbrev=False)
    for name in ('fvwm', 'mfl', 'worker', 'probe', 'artifacts-dir'):
        parser.add_argument('--' + name, type=Path, required=True)
    parser.add_argument('--file-icon', action='store_true')
    parser.add_argument('--depth', type=int, choices=(8, 24), default=24)
    options = parser.parse_args()
    for factor in (1, 2):
        run(options, factor)
