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
from colorset_live import WindowAttributes
from selection_live import Event


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
    previous_handler = None
    unexpected_errors = []
    evidence = {}
    try:
        display = options.display
        if display is None:
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
        if options.installed_desktop:
            (wrappers / 'FvwmDesktop').symlink_to(options.installed_desktop.resolve())
        modulepath = ':'.join(str(p) for p in
                              [wrappers, build / 'default-config'])
        config.write_text('ModulePath ' + modulepath + '\nRead ' +
            str(source / 'fvwm/ConfigFvwmDefaults') + '\n' +
            (root / 'config').read_text().replace('$[FVWM_DATADIR]/default-config', str(root)) +
            # An uninstalled build cannot find the core's auto-started MFL.
            ('\nAddToFunc StartFunction I Module FvwmMFL\n' if options.installed_core or options.installed_modules is None
             or not (options.installed_modules / 'FvwmMFL').is_file() else ''))
        env = runtime.environment({'HOME':str(runtime.root), 'FVWM_USERDIR':str(user),
                                  'FVWM_DESKTOP_ROOT':str(root), 'FVWMMFL_SOCKET_PATH':str(runtime.root),
                                  'XDG_DATA_HOME':str(data), 'XDG_DATA_DIRS':str(data), 'DISPLAY':display})
        process = runtime.start('fvwm', [str(options.installed_core or build / 'fvwm3'), '-d', display,
             '-f', str(config), '-v', '-o', str(runtime.root / 'core.log')], 'fvwm.log', env)
        endpoint = runtime.root / ('fvwm_mfl_' + display.replace(':', '_') + '.sock')
        wait_for_predicate('MFL endpoint', endpoint.exists, time.monotonic() + 15, process=process)
        connection = Connection(endpoint)
        connection.record()
        connection.send(b'set echo\n')
        connection.echo('desktop-ready')
        client = connect_client(display)
        pointer = Pointer(client)
        class XError(C.Structure):
            _fields_ = [('type', C.c_int), ('display', C.c_void_p),
                ('resource', C.c_ulong), ('serial', C.c_ulong),
                ('code', C.c_ubyte), ('request', C.c_ubyte), ('minor', C.c_ubyte)]

        observed_windows = set()
        handler_type = C.CFUNCTYPE(C.c_int, C.c_void_p, C.POINTER(XError))

        @handler_type
        def on_error(_, error):
            event = error.contents
            # Enumeration races native Form destruction/replacement. Only the
            # exact borrowed window and read operation may disappear here.
            if (event.code == 3 and event.resource in observed_windows
                    and event.request in (15, 20)):
                evidence['observer_retired_windows'] = evidence.get('observer_retired_windows', 0) + 1
            else:
                unexpected_errors.append((event.code, event.request, event.resource))
            return 0

        client.x.XSetErrorHandler.argtypes = [handler_type]
        client.x.XSetErrorHandler.restype = C.c_void_p
        previous_handler = client.x.XSetErrorHandler(on_error)

        def observe_window(window, operation):
            observed_windows.add(window)
            try:
                return operation()
            finally:
                client.x.XSync(client.display, 0)
                observed_windows.remove(window)

        for _ in range(32):
            connection.send(b'AddToMenu SettingsGreyed Greyed\n+ Greyed ""\n'
                            b'+ Greyed "Owned replacement label"\nDestroyMenu SettingsGreyed\n')
        connection.echo('greyed-label-ownership-alive')
        evidence['greyed_label_empty_quoted_destroy_ownership'] = 32
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
            if options.display:
                _, _, width, height = client.geometry(client.root)
                x, y = min(x, width - 1), min(y, height - 5)
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
            if not observe_window(window, lambda: client.x.XQueryTree(client.display,
                    window, C.byref(root_return), C.byref(parent), C.byref(items), C.byref(count))):
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
                if observe_window(window, lambda: client.x.XFetchName(
                        client.display, window, C.byref(name))) and name:
                    text = C.string_at(name).decode('utf-8', 'replace')
                    client.x.XFree(name)
                    found[text] = window
                if depth < 3:
                    queue.extend((w, depth + 1) for w in children(window))
            return found

        wait_for_predicate('top bar', lambda: 'SlateBar' in named_windows(),
                           time.monotonic() + 15, process=process)
        bar = named_windows()['SlateBar']
        expected_width = client.geometry(client.root)[2] if options.display else 1440
        wait_for_predicate('configured top bar size',
            lambda: client.geometry(bar)[2:] == (expected_width, 36),
            time.monotonic() + 15, process=process)
        self_geometry = client.geometry(bar)
        assert self_geometry[2:] == (expected_width, 36), self_geometry
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

        def settings_query(scope='all'):
            connection.send(('SendToModule FvwmDesktop query-settings test ' + scope + '\n').encode())
            deadline = time.monotonic() + 15
            result = {}
            while time.monotonic() < deadline:
                text = connection.record().get('echo', {}).get('message', '')
                if text.startswith('desktop-settings-test '):
                    if text == 'desktop-settings-test done':
                        return result
                    _, key, value = text.split(' ', 2)
                    result[key] = json.loads(value)
            raise AssertionError('No settings query reply')

        query = settings_query()
        assert query['palette']['saved'] == 'slate'
        assert query['palette']['effective'] == 'slate'
        assert query['palette']['preview'] is False
        assert query['day']['saved'] == '08:15'
        before = state.read_bytes()
        connection.send(b'SendToModule FvwmDesktop set palette unknown\n')
        barrier()
        assert state.read_bytes() == before
        connection.send(b'KillModule FvwmForm SlateNoticeForm\n')
        connection.send(b'SendToModule FvwmDesktop reset group:schedule\n')
        barrier()
        query = settings_query()
        assert query['day']['saved'] == '07:00' and query['night']['saved'] == '19:00'
        assert query['appearance']['saved'] == 'dark'
        # External edit is data, and must not be overwritten by a stale writer.
        edited = json.loads(state.read_text())
        edited['palette'] = 'grove'
        edited['transparency'] = False
        state.write_text(json.dumps(edited))
        connection.send(b'SendToModule FvwmDesktop set palette ink\n')
        barrier()
        assert json.loads(state.read_text())['palette'] == 'grove'
        assert settings_query('palette')['palette']['effective'] == 'slate'
        connection.send(b'KillModule FvwmForm SlateNoticeForm\nSendToModule FvwmDesktop reload-settings\n')
        barrier()
        assert settings_query('palette')['palette']['effective'] == 'grove'
        grove = int(palettes['grove']['dark']['surface'][1:], 16)
        wait_for_predicate('reloaded settings painted', lambda: pixel(5, 5) == grove,
                           time.monotonic() + 20, process=process)
        evidence['typed_queries_scoped_reset_reload_stale_refusal'] = 'passed'
        existing_windows = set(children(client.root))
        connection.send(b'SendToModule FvwmDesktop settings group:effects\n')
        wait_for_predicate('native settings menu', pointer.grabbed,
                           time.monotonic() + 10, process=process)
        client.x.XGetWindowAttributes.argtypes = [C.c_void_p, C.c_ulong,
                                                C.POINTER(WindowAttributes)]
        def mapped_menus():
            result = []
            for window in children(client.root):
                attrs = WindowAttributes()
                if (window not in existing_windows
                        and client.x.XGetWindowAttributes(client.display, window, C.byref(attrs))
                        and attrs.map_state == 2
                        and attrs.width > 40 and attrs.height > 40):
                    result.append(window)
            return result
        wait_for_predicate('settings menu mapped', mapped_menus,
                           time.monotonic() + 5, process=process)
        menus_before = set(mapped_menus())
        key(b'Home')
        key(b'Right')
        wait_for_predicate('settings choice submenu mapped',
            lambda: set(mapped_menus()) - menus_before,
            time.monotonic() + 5, process=process)
        key(b'Home')
        key(b'Return')
        wait_for_predicate('native setting activated',
            lambda: json.loads(state.read_text())['transparency'] is True,
            time.monotonic() + 10, process=process)
        wait_for_predicate('settings selection dismissed', lambda: not pointer.grabbed(),
                           time.monotonic() + 5, process=process)
        connection.send(b'SendToModule FvwmDesktop settings group:effects\n')
        wait_for_predicate('settings reopened', pointer.grabbed,
                           time.monotonic() + 5, process=process)
        key(b'Escape')
        wait_for_predicate('settings menu dismissed', lambda: not pointer.grabbed(),
                           time.monotonic() + 5, process=process)
        evidence['native_registry_menu_keyboard_activate_cancel'] = 'passed'
        connection.send(b'SendToModule FvwmDesktop reset palette\n'
                        b'SendToModule FvwmDesktop set transparency true\n')
        barrier()
        assert settings_query('palette')['palette']['effective'] == 'slate'
        connection.send(b'KillModule FvwmDesktop\n*FvwmDesktop: palette grove\n'
                        b'ModuleSynchronous Timeout 15 FvwmDesktop\n')
        barrier()
        query = settings_query('palette')['palette']
        assert (query['saved'], query['effective'], query['source']) == (
            'slate', 'grove', 'module-configuration'), query
        connection.send(b'SendToModule FvwmDesktop set palette ink\n')
        barrier()
        query = settings_query('palette')['palette']
        assert (query['saved'], query['effective']) == ('ink', 'grove'), query
        connection.send(b'SendToModule FvwmDesktop reset palette\n')
        barrier()
        assert settings_query('palette')['palette']['effective'] == 'grove'
        connection.send(b'KillModule FvwmDesktop\nDestroyModuleConfig FvwmDesktop:*\n'
                        b'ModuleSynchronous Timeout 15 FvwmDesktop\n')
        barrier()
        assert settings_query('palette')['palette']['effective'] == 'slate'
        evidence['restart_persistence_and_explicit_override'] = 'passed'

        spec = importlib.util.spec_from_file_location('desktop_capture', root / 'desktop.py')
        capture_module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(capture_module)

        def capture(name):
            previous = os.environ.get('DISPLAY')
            os.environ['DISPLAY'] = display
            try:
                # Screenshots are review artifacts, not timing assertions.
                time.sleep(.2)
                file = capture_module.screenshot(artifact)
                Path(file.removeprefix('Screenshot saved: ')).rename(artifact / name)
            finally:
                if previous is None:
                    os.environ.pop('DISPLAY', None)
                else:
                    os.environ['DISPLAY'] = previous

        def editor_window():
            return named_windows().get('Desktop settings')

        def open_editor(scope='all'):
            connection.send(('SendToModule FvwmDesktop edit-settings ' + scope + '\n').encode())
            window = wait_for_predicate('settings editor mapped', editor_window,
                time.monotonic() + 15, process=process)
            wait_for_predicate('settings editor children', lambda: len(children(window)) >= 4,
                time.monotonic() + 5, process=process)
            return window

        def form_key(window, symbol):
            event = Event()
            event.key.type, event.key.display = 2, client.display
            event.key.window, event.key.root, event.key.same_screen = window, client.root, 1
            event.key.keycode = client.x.XKeysymToKeycode(client.display,
                client.x.XStringToKeysym(symbol))
            client.x.XSendEvent.argtypes = [C.c_void_p, C.c_ulong, C.c_int,
                                          C.c_long, C.POINTER(Event)]
            assert client.x.XSendEvent(client.display, window, 0, 1, C.byref(event))
            client.x.XSync(client.display, 0)

        def click_child(window):
            client.x.XTranslateCoordinates.argtypes = [C.c_void_p, C.c_ulong, C.c_ulong,
                C.c_int, C.c_int, C.POINTER(C.c_int), C.POINTER(C.c_int), C.POINTER(C.c_ulong)]
            _, _, width, height = client.geometry(window)
            x, y, child = C.c_int(), C.c_int(), C.c_ulong()
            assert client.x.XTranslateCoordinates(client.display, window, client.root,
                width // 2, height // 2, C.byref(x), C.byref(y), C.byref(child))
            pointer.motion(x.value, y.value)
            pointer.button(1)
            pointer.button(0)

        def choice_pixels(window):
            _, _, width, height = client.geometry(window)
            image = client.x.XGetImage(client.display, window, 0, 0, width, height,
                                       C.c_ulong(-1).value, 2)
            assert image
            try:
                return tuple(client.x.XGetPixel(image, x, y)
                             for y in range(height) for x in range(width))
            finally:
                client.x.XDestroyImage(image)

        def choose(window):
            before = choice_pixels(window)
            click_child(window)
            wait_for_predicate('native choice repainted', lambda: choice_pixels(window) != before,
                time.monotonic() + 10, process=process)

        before = state.read_bytes()
        window = open_editor()
        capture('settings-editor.png')
        choices = sorted((client.geometry(w)[1], client.geometry(w)[0], w)
            for w in children(window) if client.geometry(w)[2] == client.geometry(w)[3])
        assert len(choices) == 13, choices
        choose(choices[1][2])  # Dune palette.
        choose(choices[4][2])  # Light appearance.
        capture('settings-draft.png')
        assert state.read_bytes() == before
        form_key(window, b'F3')
        try:
            wait_for_predicate('grouped native apply', lambda: json.loads(state.read_text())['palette'] == 'dune'
                and json.loads(state.read_text())['appearance'] == 'light',
                time.monotonic() + 10, process=process)
        finally:
            evidence['editor_saved_after_apply'] = json.loads(state.read_text())
            capture('settings-after-apply.png')
        wait_for_predicate('successful editor closes', lambda: not editor_window(),
            time.monotonic() + 10, process=process)
        barrier()
        connection.send(b'SendToModule FvwmDesktop settings\n')
        wait_for_predicate('settings menu for review', pointer.grabbed,
            time.monotonic() + 10, process=process)
        capture('settings-menu.png')
        key(b'Escape')
        wait_for_predicate('review menu dismissed', lambda: not pointer.grabbed(),
            time.monotonic() + 5, process=process)
        before = state.read_bytes()
        window = open_editor('group:effects')
        client.x.XSelectInput(client.display, window, 1 << 17)
        client.x.XSync(client.display, 0)
        form_key(window, b'F2')  # Defaults reloads only the unsaved scoped draft.
        client.x.XCheckTypedWindowEvent.argtypes = [C.c_void_p, C.c_ulong, C.c_int,
                                                  C.POINTER(Event)]
        def predecessor_closed():
            event = Event()
            return client.x.XCheckTypedWindowEvent(client.display, window, 17, C.byref(event))
        wait_for_predicate('defaults closes predecessor Form', predecessor_closed,
            time.monotonic() + 10, process=process)
        # X resource IDs can be reused by the next Form connection.
        replacement = wait_for_predicate('defaults editor mapped', editor_window,
            time.monotonic() + 10, process=process)
        capture('settings-defaults.png')
        assert state.read_bytes() == before
        form_key(replacement, b'Escape')
        wait_for_predicate('cancel editor closes', lambda: not editor_window(),
            time.monotonic() + 10, process=process)
        assert state.read_bytes() == before
        # A different accepted command retires the open draft immediately.
        generation = settings_query('palette')['palette']['generation']
        open_editor()
        connection.send(b'SendToModule FvwmDesktop set palette ink\n')
        barrier()
        wait_for_predicate('changed settings retire stale editor', lambda: not editor_window(),
            time.monotonic() + 10, process=process)
        assert settings_query('palette')['palette']['generation'] != generation
        # An external file edit is refused at Apply without losing the draft.
        window = open_editor()
        edited = json.loads(state.read_text())
        edited['palette'] = 'grove'
        state.write_text(json.dumps(edited))
        before = state.read_bytes()
        form_key(window, b'F3')
        wait_for_predicate('external edit refusal notice', lambda: 'FVWM-X' in named_windows(),
            time.monotonic() + 10, process=process)
        barrier()
        assert state.read_bytes() == before and editor_window() == window
        assert settings_query('palette')['palette']['effective'] == 'ink'
        connection.send(b'KillModule FvwmForm SlateNoticeForm\nSendToModule FvwmDesktop reload-settings\n')
        barrier()
        wait_for_predicate('reload retires draft', lambda: not editor_window(),
            time.monotonic() + 10, process=process)
        assert settings_query('palette')['palette']['effective'] == 'grove'
        open_editor()
        connection.send(b'KillModule FvwmDesktop\nModuleSynchronous Timeout 15 FvwmDesktop\n')
        barrier()
        wait_for_predicate('restart retires predecessor Form', lambda: not editor_window(),
            time.monotonic() + 10, process=process)
        assert state.read_bytes() == before
        evidence['native_grouped_editor_apply_defaults_cancel_stale_reload_restart'] = 'passed'
        evidence['settings_screenshots'] = ['settings-editor.png', 'settings-draft.png',
                                           'settings-menu.png', 'settings-defaults.png']
        if options.settings_only:
            return

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
        connection.send(b'KillModule FvwmForm SlateNoticeForm\n')

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
            client.x.XSetErrorHandler.argtypes = [C.c_void_p]
            client.x.XSetErrorHandler(previous_handler)
        for path in runtime.root.glob('*.log'):
            shutil.copy2(path, artifact / path.name)
        runtime.close()
        evidence['observer_unexpected_x_errors'] = unexpected_errors
        (artifact / 'evidence.json').write_text(json.dumps(evidence, indent=2) + '\n')
        assert not unexpected_errors, unexpected_errors
    print(json.dumps(evidence))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--artifacts-dir', type=Path, required=True)
    parser.add_argument('--installed-modules', type=Path)
    parser.add_argument('--installed-desktop', type=Path)
    parser.add_argument('--installed-core', type=Path)
    parser.add_argument('--display')
    parser.add_argument('--settings-only', action='store_true')
    run(parser.parse_args())
