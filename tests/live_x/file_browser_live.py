#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or any later version.
"""Exercise the real Go owner, native Form and isolated filesystem worker."""
import argparse
import base64
import ctypes as C
import importlib.util
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import time
from urllib.parse import quote_from_bytes

from geometry_transition_live import Client, Pointer
from colorset_live import WindowAttributes
from live_x_harness import HarnessRuntime, start_display_server, wait_for_predicate
from mfl_stream_live import Connection


def run(args):
    artifact = args.artifacts_dir.resolve()
    artifact.mkdir(parents=True, exist_ok=True)
    evidence = {}
    previous_handler = None
    unexpected_errors = []
    observed_windows = set()
    with HarnessRuntime(prefix='file-browser-') as runtime:
        connection = client = pointer = None
        try:
            _, display = start_display_server(runtime, 'Xvfb',
                ['Xvfb', '-screen', '0', '1440x1000x24', '-noreset'],
                'xserver.log', time.monotonic() + 10)
            root = runtime.root / 'files'
            root.mkdir()
            (root / '00-folder').mkdir()
            (root / '00-folder' / 'nested').write_text('inside')
            for i in range(15):
                (root / ('file-%02d' % i)).write_text('fixture')
            literal = 'quote " $[exec.touch] percent% ;`\nnext'
            (root / literal).write_text('literal')
            invalid = os.fsencode(root) + b'/raw-\xff'
            descriptor = os.open(invalid, os.O_CREAT | os.O_WRONLY, 0o600)
            os.close(descriptor)
            (root / '.hidden').write_text('hidden')
            (root / 'link').symlink_to(root / 'file-00')
            blocked = runtime.root / 'blocked_fifo'
            os.mkfifo(blocked, 0o600)
            report = runtime.root / 'opened.json'
            opener = runtime.root / 'opener'
            opener.write_text('#!' + sys.executable + '\nimport base64,json,os,sys\n'
                'from pathlib import Path\nPath(' + repr(str(report)) + ').write_text(json.dumps('
                '{"argv":[base64.b64encode(os.fsencode(v)).decode() for v in sys.argv[1:]],'
                '"cwd":base64.b64encode(os.fsencode(os.getcwd())).decode()}))\n')
            opener.chmod(0o700)
            config = runtime.root / 'config'
            config.write_text('DefaultFont fixed\nStyle * TileCascadePlacement, MouseFocus, MouseFocusClickRaisesOff, ClickToFocusPassesClick\n'
                'Colorset 0 fg #e6ebef, bg #24343e\n'
                '*FvwmFormDefault: Colorset 0\n*FvwmFormDefault: ItemColorset 0\n'
                '*FvwmFormDefault: Font fixed\n*FvwmFormDefault: ButtonFont fixed\n'
                '*FvwmFormDefault: InputFont fixed\n'
                'ModulePath ' + str(args.mfl.parent) + ':' + str(args.form.parent) + '\n'
                'AddToFunc StartFunction I Module FvwmMFL\n')
            env = runtime.environment({'DISPLAY': display,
                'FVWMMFL_SOCKET_PATH': str(runtime.root), 'FVWM_USERDIR': str(runtime.root)})
            core = runtime.start('core', [str(args.fvwm), '-d', display, '-f', str(config)], 'core.log', env)
            endpoint = runtime.root / ('fvwm_mfl_' + display.replace(':', '_') + '.sock')
            wait_for_predicate('MFL endpoint', endpoint.exists, time.monotonic() + 10, process=core)
            connection = Connection(endpoint)
            connection.record()
            connection.send(b'set echo\n')
            connection.echo('browser-probe-ready')
            snapshot = subprocess.run([str(args.browser), '--inspect', '--dir', str(root)],
                env=env, capture_output=True, timeout=15)
            assert snapshot.returncode == 0, snapshot
            snapshot = json.loads(snapshot.stdout)['snapshot']
            assert len(snapshot['items']) == 19
            assert any(base64.b64decode(i['name_bytes']) == b'raw-\xff' for i in snapshot['items'])
            app = runtime.start('browser', [str(args.browser), '--socket', str(endpoint),
                '--dir', str(root), '--opener', str(opener), '--terminal', str(opener)], 'browser.log', env)
            client, pointer = Client(display), None
            pointer = Pointer(client)
            x, d = client.x, client.display
            x.XGetWindowAttributes.argtypes = [C.c_void_p, C.c_ulong, C.POINTER(WindowAttributes)]
            x.XFetchName.argtypes = [C.c_void_p, C.c_ulong, C.POINTER(C.c_void_p)]
            x.XKeysymToKeycode.argtypes = [C.c_void_p, C.c_ulong]
            x.XStringToKeysym.argtypes = [C.c_char_p]
            x.XStringToKeysym.restype = C.c_ulong
            x.XSetInputFocus.argtypes = [C.c_void_p, C.c_ulong, C.c_int, C.c_ulong]
            x.XTranslateCoordinates.argtypes = [C.c_void_p, C.c_ulong, C.c_ulong,
                C.c_int, C.c_int, C.POINTER(C.c_int), C.POINTER(C.c_int), C.POINTER(C.c_ulong)]
            pointer.x.XTestFakeKeyEvent.argtypes = [C.c_void_p, C.c_uint, C.c_int, C.c_ulong]
            x.XGrabServer.argtypes = x.XUngrabServer.argtypes = [C.c_void_p]

            class XError(C.Structure):
                _fields_ = [('type', C.c_int), ('display', C.c_void_p),
                    ('resource', C.c_ulong), ('serial', C.c_ulong),
                    ('code', C.c_ubyte), ('request', C.c_ubyte), ('minor', C.c_ubyte)]
            handler_type = C.CFUNCTYPE(C.c_int, C.c_void_p, C.POINTER(XError))

            @handler_type
            def on_error(_, error):
                event = error.contents
                if event.code == 3 and event.resource in observed_windows and event.request in (3, 15, 20):
                    evidence['observer_retired_windows'] = evidence.get('observer_retired_windows', 0) + 1
                else:
                    unexpected_errors.append((event.code, event.request, event.resource))
                return 0

            x.XSetErrorHandler.argtypes = [handler_type]
            x.XSetErrorHandler.restype = C.c_void_p
            previous_handler = x.XSetErrorHandler(on_error)

            def observe(window, operation):
                observed_windows.add(window)
                try:
                    return operation()
                finally:
                    x.XSync(d, 0)
                    observed_windows.remove(window)

            def children(parent):
                r, above, count = C.c_ulong(), C.c_ulong(), C.c_uint()
                values = C.POINTER(C.c_ulong)()
                if not observe(parent, lambda: x.XQueryTree(d, parent, C.byref(r), C.byref(above), C.byref(values), C.byref(count))):
                    return []
                try:
                    return list(values[:count.value])
                finally:
                    if values:
                        x.XFree(values)

            def find(parent):
                for window in children(parent):
                    name = C.c_void_p()
                    if observe(window, lambda: x.XFetchName(d, window, C.byref(name))) and name.value:
                        try:
                            if C.string_at(name) == b'File browser':
                                return window
                        finally:
                            x.XFree(name)
                    value = find(window)
                    if value:
                        return value
                return None

            last_inventory_alias = ''

            def inventory():
                nonlocal last_inventory_alias
                x.XGrabServer(d)
                try:
                    window = find(client.root)
                    if not window:
                        return None, [], []
                    attrs = WindowAttributes()
                    if not observe(window, lambda: x.XGetWindowAttributes(d, window, C.byref(attrs))):
                        return None, [], []
                    if attrs.map_state != 2:
                        return None, [], []
                    hint = ClassHint()
                    if not observe(window, lambda: x.XGetClassHint(d, window, C.byref(hint))):
                        return None, [], []
                    try:
                        last_inventory_alias = C.string_at(hint.name).decode()
                    finally:
                        if hint.name: x.XFree(hint.name)
                        if hint.class_: x.XFree(hint.class_)
                    values = [(client.geometry(w), w) for w in children(window)]
                    squares = sorted((g[1], w) for g, w in values if g[2] == g[3])
                    return window, squares, values
                finally:
                    x.XUngrabServer(d)
                    x.XSync(d, 0)

            class ClassHint(C.Structure):
                _fields_ = [('name', C.c_void_p), ('class_', C.c_void_p)]
            x.XGetClassHint.argtypes = [C.c_void_p, C.c_ulong, C.POINTER(ClassHint)]
            seen_alias = None

            def ready(old=None, choices=None):
                def mapped():
                    window, squares, values = inventory()
                    if not window:
                        return None
                    alias = last_inventory_alias
                    # XIDs may be reused after Form retirement. The per-view
                    # resource identity and complete child count own readiness.
                    if alias.endswith('-ready') and alias != seen_alias and len(values) == 2 * len(squares) + 21 and (choices is None or len(squares) == choices):
                        return window, alias
                    return None
                nonlocal seen_alias
                window, seen_alias = wait_for_predicate('complete native browser', mapped, time.monotonic() + 10, process=app)
                return window

            def key(window, symbol):
                x.XSetInputFocus(d, window, 1, 0)
                code = x.XKeysymToKeycode(d, x.XStringToKeysym(symbol))
                assert code, symbol
                # XKeysymToKeycode does not apply the modifier needed for
                # underscore. Temporary fixture paths may contain that byte.
                shift = x.XKeysymToKeycode(d, x.XStringToKeysym(b'Shift_L')) if symbol == b'underscore' else 0
                if shift:
                    assert pointer.x.XTestFakeKeyEvent(d, shift, 1, 0)
                for down in (1, 0):
                    assert pointer.x.XTestFakeKeyEvent(d, code, down, 0)
                if shift:
                    assert pointer.x.XTestFakeKeyEvent(d, shift, 0, 0)
                x.XSync(d, 0)

            def click(window, wait_grab=True):
                _, _, width, height = client.geometry(window)
                cx, cy, child = C.c_int(), C.c_int(), C.c_ulong()
                assert x.XTranslateCoordinates(d, window, client.root, width // 2, height // 2,
                    C.byref(cx), C.byref(cy), C.byref(child))
                pointer.motion(cx.value, cy.value)
                connection.echo('pointer-positioned')
                pointer.button(True)
                # Give the other X client a realistic press interval. The
                # server's implicit grab alone does not acknowledge Form's
                # explicit release grab or input-field selection.
                time.sleep(0.05)
                if width != height and wait_grab:
                    wait_for_predicate('native button pointer grab', pointer.grabbed,
                        time.monotonic() + 5, process=app)
                pointer.button(False)

            def callback():
                deadline = time.monotonic() + 5
                while time.monotonic() < deadline:
                    text = connection.record().get('echo', {}).get('message', '')
                    if text.startswith('FVWM_FILES_1 '):
                        return text
                raise AssertionError('missing browser callback')

            source = Path(__file__).resolve().parents[2]
            spec = importlib.util.spec_from_file_location('capture', source / 'default-config/desktop.py')
            capture_module = importlib.util.module_from_spec(spec)
            spec.loader.exec_module(capture_module)

            def capture(name):
                previous = os.environ.get('DISPLAY')
                os.environ['DISPLAY'] = display
                try:
                    file = capture_module.screenshot(artifact).removeprefix('Screenshot saved: ')
                    Path(file).rename(artifact / name)
                finally:
                    if previous is None:
                        os.environ.pop('DISPLAY', None)
                    else:
                        os.environ['DISPLAY'] = previous

            window = ready(choices=12)
            capture('file-browser-first-page.png')
            _, squares, _ = inventory()
            click(squares[0][1])
            key(window, b'F8')
            old_callback = callback()
            assert base64.urlsafe_b64decode(old_callback.split()[5][1:] + '==')
            window = ready(window, 7)
            capture('file-browser-second-page.png')
            key(window, b'F7')
            callback()
            window = ready(window, 12)
            key(window, b'F3')
            selected_callback = callback()
            assert selected_callback.split()[5] == old_callback.split()[5]
            window = ready(window, 1)
            connection.echo('core-responsive-after-folder-navigation')
            evidence['paged_selection_and_folder_activation'] = True
            # Up through an actual native button: the navigation row follows
            # the selection controls; locate Home/Up/Back horizontally.
            _, squares, values = inventory()
            last_row = max(g[1] for g, w in values if g[2] == g[3])
            nav_y = sorted({g[1] for g, w in values if g[1] > last_row + 22 and g[2] != g[3]})[1]
            nav = sorted((g[0], w) for g, w in values if g[1] == nav_y and g[2] != g[3])
            click(nav[1][1])
            callback()
            window = ready(window, 12)
            key(window, b'F8')
            callback()
            window = ready(window, 7)
            _, squares, values = inventory()
            index = next(i for i, item in enumerate(snapshot['items'][12:])
                if base64.b64decode(item['name_bytes']) == os.fsencode(literal))
            square_geometry = client.geometry(squares[index][1])
            row_y = square_geometry[1] + square_geometry[3] / 2
            row_buttons = [(g[0], w) for g, w in values if g[2] > g[3]
                and abs(g[1] + g[3] / 2 - row_y) <= 2]
            assert row_buttons
            click(max(row_buttons)[1])
            callback()
            wait_for_predicate('literal file dispatched', report.exists, time.monotonic() + 5, process=app)
            opened = json.loads(report.read_text())
            assert base64.b64decode(opened['argv'][0]) == os.fsencode(root / literal)
            evidence['literal_filename_argv'] = True
            window = ready(window, 7)
            # A stale callback from page 1 cannot navigate after newer controls.
            connection.send(('Echo ' + old_callback + '\n').encode())
            connection.expect_echo(old_callback)
            window = ready(window, 7)
            assert app.poll() is None and core.poll() is None
            evidence['stale_control_refused'] = True
            capture('file-browser-action-result.png')
            # A FIFO path deliberately blocks open() in the filesystem worker.
            # The UI/core remain responsive and Cancel owns the child until reap.
            symbols = {'/': b'slash', '-': b'minus', '_': b'underscore', '.': b'period'}
            _, _, values = inventory()
            field = max((g, w) for g, w in values if g[2] > 200)
            click(field[1], wait_grab=False)
            for char in str(blocked):
                key(window, symbols.get(char, char.encode()))
            fg = field[0]
            controls = sorted((g[0], w) for g, w in values if g[0] > fg[0] + fg[2]
                and abs(g[1] + g[3] / 2 - fg[1] - fg[3] / 2) < 3)
            assert len(controls) == 3
            click(controls[0][1])
            path_callback = callback()
            assert base64.urlsafe_b64decode(path_callback.split()[4][1:] + '==') == os.fsencode(blocked)
            def busy_view():
                current, _, _ = inventory()
                return current if last_inventory_alias.endswith('-busy') else None
            wait_for_predicate('blocked worker view', busy_view,
                time.monotonic() + 5, process=app)
            connection.echo('core-responsive-during-blocked-worker')
            _, _, values = inventory()
            field = max((g, w) for g, w in values if g[2] > 200)
            fg = field[0]
            controls = sorted((g[0], w) for g, w in values if g[0] > fg[0] + fg[2]
                and abs(g[1] + g[3] / 2 - fg[1] - fg[3] / 2) < 3)
            click(controls[1][1])
            callback()
            window = ready(window, 7)
            key(window, b'F5')
            callback()
            window = ready(window, 7)
            evidence['blocked_worker_cancel_reap_refresh_and_core_responsiveness'] = True
            capture('file-browser-worker-recovery.png')
            # Filter through the real native input/button. Selected hidden IDs
            # survive; the properties panel describes the last scan only.
            key(window, b'F7')
            callback()
            window = ready(window, 12)
            _, squares, values = inventory()
            click(squares[1][1])
            field = min(((g, w) for g, w in values if g[2] > 140), key=lambda value: value[0][1])
            click(field[1], wait_grab=False)
            for char in 'file-14':
                key(window, symbols.get(char, char.encode()))
            fg = field[0]
            controls = sorted((g[0], w) for g, w in values if g[0] > fg[0] + fg[2]
                and abs(g[1] + g[3] / 2 - fg[1] - fg[3] / 2) < 3)
            assert len(controls) == 2
            click(controls[0][1])
            filtered_callback = callback()
            assert base64.urlsafe_b64decode(filtered_callback.split()[4][1:] + '==') == b'file-14'
            window = ready(window, 1)
            capture('file-browser-filtered-selection.png')
            # The selection row is immediately after the last choice row.
            def selection_controls():
                _, squares, values = inventory()
                row_end = max(g[1] + g[3] for g, w in values if g[2] == g[3])
                y = min(g[1] for g, w in values if g[1] > row_end and g[2] != g[3])
                return sorted((g[0], w) for g, w in values if g[1] == y and g[2] != g[3])
            buttons = selection_controls()
            assert len(buttons) == 3
            click(buttons[1][1])
            callback()
            window = ready(window, 1)
            buttons = selection_controls()
            click(buttons[0][1])
            callback()
            window = ready(window, 1)
            key(window, b'F2')
            callback()
            window = ready(window, 1)
            capture('file-browser-properties.png')
            # Cycle ordering via the native sort control and invert direction.
            _, squares, values = inventory()
            first_row = min(g[1] for g, w in values if g[2] == g[3])
            sort_y = max(g[1] for g, w in values if g[1] + g[3] < first_row and g[2] != g[3])
            sort_buttons = sorted((g[0], w) for g, w in values if g[1] == sort_y and g[2] != g[3])
            assert len(sort_buttons) == 2
            click(sort_buttons[0][1])
            callback()
            window = ready(window, 1)
            _, _, values = inventory()
            sort_buttons = sorted((g[0], w) for g, w in values if g[1] == sort_y and g[2] != g[3])
            click(sort_buttons[1][1])
            callback()
            window = ready(window, 1)
            # Submit a no-match filter; the page has no stale rows or controls.
            _, _, values = inventory()
            field = min(((g, w) for g, w in values if g[2] > 140), key=lambda value: value[0][1])
            click(field[1], wait_grab=False)
            for char in 'no-match':
                key(window, symbols.get(char, char.encode()))
            fg = field[0]
            controls = sorted((g[0], w) for g, w in values if g[0] > fg[0] + fg[2]
                and abs(g[1] + g[3] / 2 - fg[1] - fg[3] / 2) < 3)
            click(controls[0][1])
            callback()
            window = ready(window, 0)
            capture('file-browser-empty-filter.png')
            _, _, values = inventory()
            field = min(((g, w) for g, w in values if g[2] > 140), key=lambda value: value[0][1])
            fg = field[0]
            controls = sorted((g[0], w) for g, w in values if g[0] > fg[0] + fg[2]
                and abs(g[1] + g[3] / 2 - fg[1] - fg[3] / 2) < 3)
            click(controls[1][1])
            callback()
            window = ready(window, 12)
            evidence['native_filter_bulk_selection_properties_sort_and_empty_recovery'] = True
            key(window, b'Escape')
            callback()
            wait_for_predicate('browser normal close', lambda: app.poll() is not None,
                time.monotonic() + 5, process=core)
            assert app.poll() == 0
            evidence['close_cleanup'] = True
            # A portable reveal request selects across pages without opening
            # the targets. Dot entries become visible when explicitly named.
            reveal_args = ['--reveal', str(root / 'file-14'), '--reveal', str(root / '.hidden')]
            located = subprocess.run([str(args.browser), '--inspect', *reveal_args],
                env=env, capture_output=True, timeout=15)
            assert located.returncode == 0, located
            located = json.loads(located.stdout)
            assert located['snapshot']['hidden'] and len(located['selected_ids']) == 2
            assert report.read_text() == json.dumps(opened)
            app = runtime.start('browser-reveal', [str(args.browser), '--socket', str(endpoint),
                *reveal_args, '--opener', str(opener)], 'reveal.log', env)
            window = ready(choices=8)
            capture('file-browser-initial-reveal.png')
            key(window, b'F3')
            reveal_callback = callback()
            picked = base64.urlsafe_b64decode(reveal_callback.split()[5][1:] + '==').split()
            assert len(picked) == 1  # other selected item remains off-page
            window = ready(window, 8)
            assert report.read_text() == json.dumps(opened)
            key(window, b'Escape')
            callback()
            wait_for_predicate('revealed browser close', lambda: app.poll() is not None,
                time.monotonic() + 5, process=core)
            assert app.poll() == 0
            # Percent-encoded invalid UTF-8 and a literal percent sign use one
            # decoding step. Properties on a symlink remain an observation.
            raw_uri = 'file://localhost' + quote_from_bytes(invalid, safe='/')
            located = subprocess.run([str(args.browser), '--inspect', '--properties', raw_uri],
                env=env, capture_output=True, timeout=15)
            assert located.returncode == 0, located
            located = json.loads(located.stdout)
            selected = located['selected_ids'][0]
            item = next(i for i in located['snapshot']['items'] if i['id'] == selected)
            assert base64.b64decode(item['name_bytes']) == b'raw-\xff'
            app = runtime.start('browser-properties', [str(args.browser), '--socket', str(endpoint),
                '--opener', str(opener), '--properties', 'file://localhost' + quote_from_bytes(os.fsencode(root / 'link'), safe='/')],
                'properties.log', env)
            window = ready(choices=7)
            capture('file-browser-initial-properties.png')
            key(window, b'Escape')
            callback()
            wait_for_predicate('properties browser close', lambda: app.poll() is not None,
                time.monotonic() + 5, process=core)
            assert app.poll() == 0
            assert report.read_text() == json.dumps(opened)
            # Invalid external requests fail before opening a browser/socket;
            # an unavailable target returns a failed isolated result.
            bad = subprocess.run([str(args.browser), '--inspect', '--reveal', 'file://remote/path'],
                env=env, capture_output=True, timeout=15)
            assert bad.returncode == 2
            missing = subprocess.run([str(args.browser), '--inspect', '--reveal', str(root / 'missing')],
                env=env, capture_output=True, timeout=15)
            assert missing.returncode != 0 and json.loads(missing.stdout)['status'] == 'failed'
            evidence['portable_reveal_properties_URI_bytes_no_activation_and_refusal'] = True

            assert not unexpected_errors, unexpected_errors
            evidence['unexpected_observer_x_errors'] = unexpected_errors
            evidence['screenshots'] = ['file-browser-first-page.png', 'file-browser-second-page.png',
                'file-browser-action-result.png', 'file-browser-worker-recovery.png',
                'file-browser-filtered-selection.png', 'file-browser-properties.png',
                'file-browser-empty-filter.png', 'file-browser-initial-reveal.png',
                'file-browser-initial-properties.png']
            evidence['scope'] = 'native paged foundation; full scrolling/grid, files provider and mutations remain open'
        finally:
            if sys.exc_info()[0] is not None and 'capture' in locals():
                capture('file-browser-failure.png')
                if 'inventory' in locals():
                    (artifact / 'failure-inventory.json').write_text(json.dumps(inventory()))
            if connection:
                connection.close()
            if pointer:
                pointer.button(False)
            if client:
                client.x.XSync(client.display, 0)
                if previous_handler is not None:
                    client.x.XSetErrorHandler.argtypes = [C.c_void_p]
                    client.x.XSetErrorHandler(previous_handler)
                client.close()
            for name in ['core.log', 'browser.log', 'xserver.log']:
                path = runtime.root / name
                if path.exists():
                    shutil.copy2(path, artifact / name)
            (artifact / 'result.json').write_text(json.dumps(evidence, indent=2) + '\n')
    print(json.dumps(evidence))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('fvwm', 'mfl', 'form', 'browser', 'artifacts-dir'):
        parser.add_argument('--' + name, type=Path, required=True)
    run(parser.parse_args())
