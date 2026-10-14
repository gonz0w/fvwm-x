#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
"""Load the real baseline, switch palettes, exercise services, and capture X."""

import argparse
import ctypes as C
import importlib.util
import json
import os
import re
from pathlib import Path
import shutil
import shlex
import sys
import time
import struct
import zlib

from geometry_transition_live import Client, Pointer
from live_x_harness import HarnessError, HarnessRuntime, start_display_server, wait_for_predicate
from mfl_stream_live import Connection


def connect_client(display):
    deadline = time.monotonic() + 5
    while True:
        try:
            return Client(display)
        except HarnessError:
            if time.monotonic() >= deadline:
                raise
            time.sleep(.1)


def verify_capture_formats(runtime, capture_module, artifact):
    """Compare encoded RGB against known X pixel values at two visual depths."""
    for depth, color, expected in ((16, 0xf800, b'\xff\0\0'),
                                    (24, 0x224466, b'\x22\x44\x66')):
        _, display = start_display_server(runtime, 'capture-' + str(depth),
            ['Xvfb', '-screen', '0', '64x48x' + str(depth), '-noreset'],
            'capture-' + str(depth) + '.log', time.monotonic() + 10)
        probe = connect_client(display)
        try:
            probe.x.XSetWindowBackground.argtypes = [C.c_void_p, C.c_ulong, C.c_ulong]
            probe.x.XClearWindow.argtypes = [C.c_void_p, C.c_ulong]
            probe.x.XSetWindowBackground(probe.display, probe.root, color)
            probe.x.XClearWindow(probe.display, probe.root)
            probe.x.XSync(probe.display, 0)
            os.environ['DISPLAY'] = display
            filename = capture_module.screenshot(artifact).removeprefix('Screenshot saved: ')
            data = Path(filename).read_bytes()
            offset, compressed = 8, bytearray()
            while offset < len(data):
                size = struct.unpack_from('!I', data, offset)[0]
                if data[offset + 4:offset + 8] == b'IDAT':
                    compressed.extend(data[offset + 8:offset + 8 + size])
                offset += size + 12
            row = zlib.decompress(compressed)
            assert row[1:4] == expected, (depth, row[1:4], expected)
            Path(filename).unlink()
        finally:
            probe.close()


def run(options):
    runtime = HarnessRuntime(prefix='fvwm-desktop-')
    artifact = options.artifacts_dir.resolve()
    artifact.mkdir(parents=True, exist_ok=True)
    build = options.build.resolve()
    source = Path(__file__).resolve().parents[2]
    client = connection = None
    evidence = {}
    try:
        _, display = start_display_server(runtime, 'Xvfb',
            ['Xvfb', '-screen', '0', '1440x900x24', '-noreset'],
            'xserver.log', time.monotonic() + 10)
        root = source / 'default-config'
        user = runtime.root / 'user'
        user.mkdir()
        data = runtime.root / 'data'
        apps = data / 'applications'
        apps.mkdir(parents=True)
        marker = runtime.root / 'launched'
        app_script = runtime.root / 'launch.py'
        app_script.write_text('from pathlib import Path\nPath(' + repr(str(marker)) + ').write_text("launched")\n')
        (apps / 'fixture.desktop').write_text(
            '[Desktop Entry]\nType=Application\nName=Baseline test application\n'
            'Exec=' + shlex.join([sys.executable, str(app_script)]) + '\n')
        config = runtime.root / 'config'
        wrappers = runtime.root / 'modules'
        wrappers.mkdir()
        for module in ('FvwmBacker', 'FvwmButtons', 'FvwmEvent'):
            wrapper = wrappers / module
            wrapper.write_text('#!/bin/sh\n' + shlex.quote(str(build / 'modules' / module / module)) +
                ' "$@" 2>' + shlex.quote(str(runtime.root / (module + '.log'))) +
                '\nresult=$?\necho "exit status: $result" >>' +
                shlex.quote(str(runtime.root / (module + '.log'))) + '\nexit "$result"\n')
            wrapper.chmod(0o755)
        # One private lookup directory keeps setup independent of build-path
        # length, while retaining the wrappers that collect module logs.
        for directory in (build / 'modules').iterdir():
            module = directory / directory.name
            alias = wrappers / directory.name
            if module.is_file() and not alias.exists():
                alias.symlink_to(module)
        modulepath = ':'.join(str(p) for p in
                              [wrappers, build / 'default-config'])
        config.write_text('ModulePath ' + modulepath + '\n' +
            (root / 'config').read_text().replace('$[FVWM_DATADIR]/default-config', str(root)) +
            # An uninstalled build cannot find the core's auto-started MFL.
            ('\nAddToFunc StartFunction I Module FvwmMFL\n' if options.installed_modules is None
             or not (options.installed_modules / 'FvwmMFL').is_file() else ''))
        env = runtime.environment({'HOME':str(runtime.root), 'FVWM_USERDIR':str(user),
                                  'FVWM_DESKTOP_ROOT':str(root), 'FVWMMFL_SOCKET_PATH':str(runtime.root),
                                  'XDG_DATA_HOME':str(data), 'XDG_DATA_DIRS':str(data), 'DISPLAY':display})
        process = runtime.start('fvwm', [str(build / 'fvwm3'), '-d', display,
             '-f', str(config), '-v', '-o', str(runtime.root / 'core.log')], 'fvwm.log', env)
        endpoint = runtime.root / ('fvwm_mfl_' + display.replace(':', '_') + '.sock')
        wait_for_predicate('MFL endpoint', endpoint.exists, time.monotonic() + 15, process=process)
        connection = Connection(endpoint)
        connection.record()
        connection.send(b'set echo\n')
        connection.echo('desktop-ready')
        client = connect_client(display)
        pointer = Pointer(client)
        client.x.XFetchName.argtypes = [C.c_void_p, C.c_ulong, C.POINTER(C.c_void_p)]
        client.x.XFetchName.restype = C.c_int
        client.x.XGetImage.argtypes = [C.c_void_p, C.c_ulong, C.c_int, C.c_int,
                                      C.c_uint, C.c_uint, C.c_ulong, C.c_int]
        client.x.XGetImage.restype = C.c_void_p
        client.x.XGetPixel.argtypes = [C.c_void_p, C.c_int, C.c_int]
        client.x.XGetPixel.restype = C.c_ulong
        client.x.XDestroyImage.argtypes = [C.c_void_p]
        client.x.XDestroyImage.restype = C.c_int

        def pixel(x, y):
            image = client.x.XGetImage(client.display, client.root, x, y, 1, 1,
                                      C.c_ulong(-1).value, 2)
            assert image
            try:
                return client.x.XGetPixel(image, 0, 0)
            finally:
                client.x.XDestroyImage(image)

        def children(window):
            root_return, parent = C.c_ulong(), C.c_ulong()
            items, count = C.POINTER(C.c_ulong)(), C.c_uint()
            if not client.x.XQueryTree(client.display, window, C.byref(root_return),
                                       C.byref(parent), C.byref(items), C.byref(count)):
                return []
            result = [items[i] for i in range(min(count.value, 256))]
            if items:
                client.x.XFree(items)
            return result

        def named_windows():
            found = {}
            queue = [(client.root, 0)]
            while queue:
                window, depth = queue.pop()
                name = C.c_void_p()
                if client.x.XFetchName(client.display, window, C.byref(name)) and name:
                    text = C.string_at(name).decode('utf-8', 'replace')
                    client.x.XFree(name)
                    found[text] = window
                if depth < 3:
                    queue.extend((w, depth + 1) for w in children(window))
            return found

        wait_for_predicate('top bar', lambda: 'SlateBar' in named_windows(),
                           time.monotonic() + 15, process=process)
        bar = named_windows()['SlateBar']
        wait_for_predicate('configured top bar size',
            lambda: client.geometry(bar)[2:] == (1440, 36),
            time.monotonic() + 15, process=process)
        self_geometry = client.geometry(bar)
        assert self_geometry[2:] == (1440, 36), self_geometry
        evidence['primary_bar_size'] = list(self_geometry[2:])
        state = user / 'desktop-state.json'
        barrier_id = 0
        def barrier():
            nonlocal barrier_id
            barrier_id += 1
            token = 'desktop-applied-' + str(barrier_id)
            connection.send(('SendToModule FvwmDesktop barrier ' + token + '\n').encode())
            # Theme painting on emulated BSD is setup, not a latency gate.
            previous_timeout = connection.socket.gettimeout()
            connection.socket.settimeout(15)
            try:
                connection.expect_echo(token)
            finally:
                connection.socket.settimeout(previous_timeout)

        connection.send(b'SendToModule FvwmDesktop set appearance dark\n')
        wait_for_predicate('wallpaper painted', lambda: pixel(10, 850) != 0,
                           time.monotonic() + 10, process=process)
        barrier()
        previous_background = pixel(10, 850)
        for palette in ('dune', 'grove', 'ink', 'slate'):
            connection.send(('SendToModule FvwmDesktop set palette ' + palette + '\n').encode())
            wait_for_predicate('saved palette ' + palette,
                lambda: state.exists() and json.loads(state.read_text()).get('palette') == palette,
                time.monotonic() + 5, process=process)
            barrier()
            wait_for_predicate('painted palette ' + palette,
                lambda: pixel(10, 850) != previous_background,
                time.monotonic() + 10, process=process)
            previous_background = pixel(10, 850)
        connection.send(b'SendToModule FvwmDesktop set appearance dark\n')
        wait_for_predicate('dark appearance', lambda: json.loads(state.read_text())['appearance'] == 'dark',
                           time.monotonic() + 5, process=process)
        connection.send(b'SendToModule FvwmDesktop times 08:15 20:30\n')
        wait_for_predicate('saved schedule', lambda: json.loads(state.read_text())['day'] == '08:15',
                           time.monotonic() + 5, process=process)
        connection.send(b'SendToModule FvwmDesktop toggle transparency\n')
        barrier()
        wait_for_predicate('opaque mode', lambda: json.loads(state.read_text())['transparency'] is False,
                           time.monotonic() + 5, process=process)
        barrier()
        palettes = json.loads((root / 'palettes.json').read_text())
        opaque = int(palettes['slate']['dark']['surface'][1:], 16)
        try:
            wait_for_predicate('opaque bar painted', lambda: pixel(5, 5) == opaque,
                               time.monotonic() + 20, process=process)
        except Exception as error:
            evidence['unexpected_opaque_pixel'] = hex(pixel(5, 5))
            raise AssertionError(str(error) + ': got ' + hex(pixel(5, 5)) +
                                 ', expected ' + hex(opaque)) from error
        connection.send(b'SendToModule FvwmDesktop toggle transparency\n')
        barrier()
        wait_for_predicate('tinted bar restored', lambda: pixel(5, 5) != opaque,
                           time.monotonic() + 20, process=process)
        evidence['palettes_appearance_schedule_opaque_switch'] = 'passed'

        def key(name):
            client.x.XStringToKeysym.argtypes = [C.c_char_p]
            client.x.XStringToKeysym.restype = C.c_ulong
            client.x.XKeysymToKeycode.argtypes = [C.c_void_p, C.c_ulong]
            client.x.XKeysymToKeycode.restype = C.c_ubyte
            pointer.x.XTestFakeKeyEvent.argtypes = [C.c_void_p, C.c_uint, C.c_int, C.c_ulong]
            pointer.x.XTestFakeKeyEvent.restype = C.c_int
            code = client.x.XKeysymToKeycode(client.display, client.x.XStringToKeysym(name))
            assert code
            for pressed in (1, 0):
                pointer.x.XTestFakeKeyEvent(client.display, code, pressed, 0)
            client.x.XSync(client.display, 0)

        connection.send(b'SlateLauncher\n')
        wait_for_predicate('centered launcher grab', pointer.grabbed,
                           time.monotonic() + 5, process=process)
        key(b'Escape')
        wait_for_predicate('launcher dismissed', lambda: not pointer.grabbed(),
                           time.monotonic() + 5, process=process)
        # Application 0 is the isolated fixture: no system app is launched.
        connection.send(b'SendToModule FvwmDesktop launch 0\n')
        wait_for_predicate('application launched', marker.exists,
                           time.monotonic() + 5, process=process)
        evidence['launcher_and_safe_application_launch'] = 'passed'
        connection.send(b'SendToModule FvwmDesktop search baseline\n')
        wait_for_predicate('search results', pointer.grabbed,
                           time.monotonic() + 5, process=process)
        key(b'Escape')
        wait_for_predicate('search dismissed', lambda: not pointer.grabbed(),
                           time.monotonic() + 5, process=process)
        connection.send(b'SendToModule FvwmDesktop calendar\n')
        wait_for_predicate('calendar menu', pointer.grabbed,
                           time.monotonic() + 5, process=process)
        key(b'Escape')
        wait_for_predicate('calendar dismissed', lambda: not pointer.grabbed(),
                           time.monotonic() + 5, process=process)
        evidence['search_and_calendar'] = 'passed'

        # Screenshot action is exercised while the core continues serving Echo.
        connection.send(b'SendToModule FvwmDesktop screenshot\n')
        connection.echo('core-alive-during-screenshot')
        pictures = runtime.root / 'Pictures/Screenshots'
        wait_for_predicate('bundled screenshot', lambda: pictures.is_dir() and any(pictures.glob('*.png')),
                           time.monotonic() + 60, process=process)
        capture = next(pictures.glob('*.png'))
        wait_for_predicate('complete PNG', lambda: capture.stat().st_size > 32 and
                           capture.read_bytes().endswith(b'IEND\xaeB`\x82'),
                           time.monotonic() + 10, process=process)
        shutil.copy2(capture, artifact / 'baseline-capture.png')
        evidence['screenshot_and_core_responsiveness'] = 'passed'
        connection.send(b'KillModule SlateNoticeForm\n')

        # Capture review images with a real terminal and the live native launcher.
        spec = importlib.util.spec_from_file_location('desktop_capture', root / 'desktop.py')
        capture_module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(capture_module)
        if shutil.which('xterm'):
            terminal = runtime.start('terminal', ['xterm', '-T', 'Terminal', '-fa', 'Monospace', '-fs', '11',
                '-bg', '#202a31', '-fg', '#e5edf0', '-geometry', '76x23+100+120', '-e', '/bin/sh', '-c',
                'printf "FVWM-X / Slate desktop\n\nFour palettes. Light, dark, automatic.\n\nSuper+Space   Applications\nSuper+Enter   Terminal\nSuper+D       Files\nPrint         Screenshot\n\n"; exec /bin/sh'], 'terminal.log', env)
            wait_for_predicate('terminal window', lambda: 'Terminal' in named_windows(),
                               time.monotonic() + 10, process=terminal)
            window = named_windows()['Terminal']
            wait_for_predicate('terminal framed', lambda: client.parent(window) != client.root,
                               time.monotonic() + 10, process=process)
            connection.echo('terminal-managed')
        else:
            client.create()
        old_display = os.environ.get('DISPLAY')
        os.environ['DISPLAY'] = display
        try:
            previous_review = ('slate', 'dark')
            for palette, mode in [(p, m) for p in ('slate', 'dune', 'grove', 'ink') for m in ('dark', 'light')]:
                connection.send(('SendToModule FvwmDesktop set palette ' + palette + '\nSendToModule FvwmDesktop set appearance ' + mode + '\n').encode())
                wait_for_predicate('review palette', lambda: json.loads(state.read_text())['appearance'] == mode
                                   and json.loads(state.read_text())['palette'] == palette,
                                   time.monotonic() + 5, process=process)
                barrier()
                if (palette, mode) != previous_review:
                    wait_for_predicate('review wallpaper painted', lambda: pixel(10, 850) != previous_background,
                                       time.monotonic() + 10, process=process)
                previous_background = pixel(10, 850)
                previous_review = (palette, mode)
                connection.send(b'SlateLauncher\n')
                wait_for_predicate('review launcher', pointer.grabbed,
                                   time.monotonic() + 5, process=process)
                # Let asynchronous XExpose drawing finish before reading pixels.
                time.sleep(.2)
                file = capture_module.screenshot(artifact)
                Path(file.removeprefix('Screenshot saved: ')).rename(artifact / (palette + '-' + mode + '.png'))
                key(b'Escape')
                wait_for_predicate('review dismissed', lambda: not pointer.grabbed(),
                                   time.monotonic() + 5, process=process)
        finally:
            if old_display is None:
                os.environ.pop('DISPLAY', None)
            else:
                os.environ['DISPLAY'] = old_display
        try:
            verify_capture_formats(runtime, capture_module, artifact)
            evidence['screenshot_rgb_at_16_and_24_bit_depth'] = 'passed'
        finally:
            if old_display is None:
                os.environ.pop('DISPLAY', None)
            else:
                os.environ['DISPLAY'] = old_display
        log = (runtime.root / 'core.log').read_text()
        for bad in ('Unknown style', 'Bad geometry', 'Invalid button', 'No such command', 'error in config'):
            assert bad.lower() not in log.lower(), log
        evidence['baseline_parser'] = 'passed'
        for path in runtime.root.glob('*.log'):
            text = path.read_text()
            assert not any(bad in text for bad in ('Segmentation fault', 'Abort trap', 'BadGC')), text
            assert not re.search(r'exit status: [1-9]', text), text
    finally:
        if connection:
            connection.close()
        if client:
            client.close()
        for path in runtime.root.glob('*.log'):
            shutil.copy2(path, artifact / path.name)
        runtime.close()
        (artifact / 'evidence.json').write_text(json.dumps(evidence, indent=2) + '\n')
    print(json.dumps(evidence))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--artifacts-dir', type=Path, required=True)
    parser.add_argument('--installed-modules', type=Path)
    run(parser.parse_args())
