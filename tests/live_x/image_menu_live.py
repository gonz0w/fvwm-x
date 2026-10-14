#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
"""Check popup image stability and next-popup completion at both scales."""
import argparse
import ctypes as C
import json
from pathlib import Path
import shutil
import time

from live_x_harness import HarnessRuntime, start_display_server, wait_for_predicate
from mfl_stream_live import Connection
from geometry_transition_live import Client, Pointer
from colorset_live import WindowAttributes
from image_owners_live import png


def run(args, factor):
    out = args.artifacts_dir / f'scale-{factor}'
    out.mkdir(parents=True, exist_ok=True)
    evidence = {}
    with HarnessRuntime(prefix='fvwm-image-menu-') as runtime:
        client = connection = core = None
        try:
            _, display = start_display_server(runtime, 'Xvfb',
                ['Xvfb', '-screen', '0', '1000x800x24', '-noreset'],
                'xserver.log', time.monotonic() + 10)
            image = runtime.root / 'red.png'
            image.write_bytes(png())
            sources = {}
            for name in ('red', 'blue', 'stall', 'incomplete'):
                sources[name] = runtime.root / (name + '.svg')
                sources[name].write_text(name)
            config = runtime.root / 'config'
            config.write_text(f'ModulePath {args.mfl.parent}\n'
                f'MonitorScale "screen" {factor}\nDefaultFont fixed\n'
                'Colorset 0 fg white, bg black\nMenuStyle * Font fixed, MenuColorset 0\n'
                'AddToFunc StartFunction I Module FvwmMFL\n')
            core = runtime.start('core', [str(args.fvwm), '-d', display,
                '-f', str(config), '-v', '-o', str(runtime.root / 'debug.log')],
                'core.log', runtime.environment({
                    'FVWMMFL_SOCKET_PATH': str(runtime.root),
                    'FVWM_IMAGE_SVG_WORKER': str(args.worker)}))
            endpoint = runtime.root / f"fvwm_mfl_{display.replace(':', '_')}.sock"
            wait_for_predicate('menu fixture endpoint', endpoint.exists,
                time.monotonic() + 10, process=core)
            connection = Connection(endpoint)
            connection.record()
            connection.send(b'set echo\n')
            client = Client(display)
            x, d = client.x, client.display
            pointer = Pointer(client)
            errors = []
            callback_type = C.CFUNCTYPE(C.c_int, C.c_void_p, C.c_void_p)

            @callback_type
            def error_handler(_display, _event):
                errors.append('X request failed')
                return 0

            x.XSetErrorHandler.argtypes = [callback_type]
            x.XSetErrorHandler(error_handler)
            x.XGetWindowAttributes.argtypes = [C.c_void_p, C.c_ulong,
                C.POINTER(WindowAttributes)]
            x.XGetImage.argtypes = [C.c_void_p, C.c_ulong, C.c_int, C.c_int,
                C.c_uint, C.c_uint, C.c_ulong, C.c_int]
            x.XGetImage.restype = C.c_void_p
            x.XGetPixel.argtypes = [C.c_void_p, C.c_int, C.c_int]
            x.XGetPixel.restype = C.c_ulong
            x.XDestroyImage.argtypes = [C.c_void_p]
            x.XKeysymToKeycode.argtypes = [C.c_void_p, C.c_ulong]
            x.XKeysymToKeycode.restype = C.c_uint
            x.XFetchName.argtypes = [C.c_void_p, C.c_ulong,
                C.POINTER(C.c_void_p)]
            pointer.x.XTestFakeKeyEvent.argtypes = [C.c_void_p, C.c_uint,
                C.c_int, C.c_ulong]
            sequence = 0

            def command(text):
                nonlocal sequence
                sequence += 1
                connection.send((text + '\n').encode())
                connection.echo(f'menu-barrier-{sequence}')

            def popup_window():
                root, parent = C.c_ulong(), C.c_ulong()
                children, count = C.POINTER(C.c_ulong)(), C.c_uint()
                assert x.XQueryTree(d, client.root, C.byref(root), C.byref(parent),
                    C.byref(children), C.byref(count))
                try:
                    for index in range(count.value):
                        attrs = WindowAttributes()
                        window = children[index]
                        assert x.XGetWindowAttributes(d, window, C.byref(attrs))
                        if attrs.map_state == 2 and attrs.save_under and (
                                attrs.width > 20 and attrs.height > 10):
                            return window
                finally:
                    if children:
                        x.XFree(children)
                return None

            def pixels(window):
                geometry = client.geometry(window)
                width, height = geometry[2:]
                image = x.XGetImage(d, window, 0, 0, width, height,
                    C.c_ulong(-1).value, 2)
                assert image and not errors
                try:
                    return geometry, tuple(x.XGetPixel(image, px, py)
                        for py in range(height) for px in range(width))
                finally:
                    x.XDestroyImage(image)

            def popup(name, color=None, hold=False, tear=False):
                connection.send(f'Popup {name}\n'.encode())
                window = wait_for_predicate('image popup mapped', popup_window,
                    time.monotonic() + 5, process=core)
                first = pixels(window)
                if color is not None:
                    first = wait_for_predicate('popup image painted',
                        lambda: pixels(window) if pixels(window)[1].count(color) > 0
                            else None, time.monotonic() + 5, process=core)
                if hold:
                    # Give the finite synthetic worker time to finish while
                    # the modal popup continues servicing only transport.
                    time.sleep(0.25)
                    later = pixels(window)
                    def image_pixels(picture):
                        return tuple((i, p) for i, p in enumerate(picture[1])
                            if p in (0xff0000, 0x0000ff))
                    # Ordinary selection/highlight can repaint text. Image
                    # pixels and popup dimensions must remain fixed.
                    assert later[0] == first[0] and (
                        image_pixels(later) == image_pixels(first))
                key = x.XKeysymToKeycode(d, 0xff08 if tear else 0xff1b)
                assert pointer.x.XTestFakeKeyEvent(d, key, 1, 0)
                assert pointer.x.XTestFakeKeyEvent(d, key, 0, 0)
                if tear:
                    pointer.motion(5, client.geometry(client.root)[3] - 10)
                x.XSync(d, 0)
                connection.echo('image-menu-closed')
                return first

            def named_window(title):
                pending = [(client.root, 0)]
                while pending:
                    window, depth = pending.pop()
                    data = C.c_void_p()
                    if x.XFetchName(d, window, C.byref(data)) and data.value:
                        try:
                            if C.string_at(data) == title.encode():
                                return window
                        finally:
                            x.XFree(data)
                    if depth < 3:
                        root, parent = C.c_ulong(), C.c_ulong()
                        children, count = C.POINTER(C.c_ulong)(), C.c_uint()
                        assert x.XQueryTree(d, window, C.byref(root), C.byref(parent),
                            C.byref(children), C.byref(count))
                        pending.extend((children[i], depth + 1)
                            for i in range(count.value))
                        if children:
                            x.XFree(children)
                return None

            command(f'AddToMenu Images "Entry%{sources["stall"]}%" Nop')
            first = popup('Images', hold=True)
            assert first[1].count(0xff0000) == 0 and first[1].count(0x0000ff) == 0
            evidence['pending-first-item-image-uses-absent-rendering'] = 'passed'
            command('DestroyMenu Images')
            command(f'AddToMenu Images "Entry%{sources["blue"]}%" Nop')
            popup('Images', hold=True)
            # Closing the popup exposes the normal commit boundary. Retry
            # subsequent popups until the acknowledged image is installed.
            wait_for_predicate('menu image committed for next popup',
                lambda: popup('Images')[1].count(0x0000ff) > 0,
                time.monotonic() + 8, process=core)
            evidence['mapped-item-is-stable-next-popup-gets-completion'] = 'passed'
            command(f'MenuStyle * SidePic {image}')
            command('AddToMenu Side "Entry" Nop')
            popup('Side', 0xff0000)
            command(f'MenuStyle * SidePic {sources["stall"]}')
            popup('Side', 0xff0000, hold=True)
            command(f'MenuStyle * SidePic {sources["blue"]}')
            wait_for_predicate('new style sidepic in next popup',
                lambda: popup('Side')[1].count(0x0000ff) > 0,
                time.monotonic() + 8, process=core)
            evidence['style-sidepic-replacement-retains-old-then-updates'] = 'passed'
            command('Key BackSpace M A MenuTearOff')
            popup('Side', 0x0000ff, tear=True)
            tearoff = wait_for_predicate('image tearoff client',
                lambda: named_window('Side'), time.monotonic() + 5, process=core)
            def tearoff_viewable():
                attrs = WindowAttributes()
                root, parent = C.c_ulong(), C.c_ulong()
                children, count = C.POINTER(C.c_ulong)(), C.c_uint()
                assert x.XGetWindowAttributes(d, tearoff, C.byref(attrs))
                assert x.XQueryTree(d, tearoff, C.byref(root), C.byref(parent),
                    C.byref(children), C.byref(count))
                if children:
                    x.XFree(children)
                # Naming precedes normal client management and mapping. Window
                # readback requires the managed client to be viewable first.
                return attrs.map_state == 2 and parent.value != client.root

            wait_for_predicate('image tearoff managed and viewable',
                tearoff_viewable, time.monotonic() + 5, process=core)
            # Whole-window XGetImage requires an on-screen rectangle. Place
            # the tearoff explicitly on smaller native framebuffer screens.
            command('All (Side) Move 100p 100p')
            frozen = pixels(tearoff)
            command(f'AddToMenu Independent "Entry%{sources["red"]}%" Nop')
            wait_for_predicate('independent completion with tearoff open',
                lambda: popup('Independent')[1].count(0xff0000) > 0,
                time.monotonic() + 8, process=core)
            assert pixels(tearoff) == frozen
            command('All (Side) Close')
            wait_for_predicate('image tearoff closed',
                lambda: named_window('Side') is None,
                time.monotonic() + 5, process=core)
            command('DestroyMenu Independent')
            evidence['tearoff-snapshot-keeps-pixels-independent-menu-completes'] = 'passed'
            command(f'MenuStyle * SidePic {sources["incomplete"]}')
            wait_for_predicate('style refusal',
                lambda: 'SVG refused:' in (runtime.root / 'debug.log').read_text(),
                time.monotonic() + 8, process=core)
            popup('Side', 0x0000ff)
            command('MenuStyle * SidePic')
            assert popup('Side')[1].count(0x0000ff) == 0
            evidence['style-refusal-retains-old-and-explicit-clear-removes'] = 'passed'
            command(f'AddToMenu "Root@{sources["stall"]}@" "Entry" Nop')
            command('DestroyMenu Root')
            command('DestroyMenu Images')
            command('DestroyMenu Side')
            evidence['destroyed-root-and-item-cancel-pending-views'] = 'passed'
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
    print(f'Menu completion and scale {factor} ownership checks passed')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(allow_abbrev=False)
    for name in ('fvwm', 'mfl', 'worker', 'artifacts-dir'):
        parser.add_argument('--' + name, type=Path, required=True)
    options = parser.parse_args()
    for factor in (1, 2):
        run(options, factor)
