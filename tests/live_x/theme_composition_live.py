#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
"""Independent wallpaper and high contrast through the desktop owner and real X."""
import argparse
from collections import Counter
import ctypes as C
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import time

from colorset_live import WindowAttributes
from desktop_live import connect_client
from geometry_transition_live import Pointer
from selection_live import Event
from live_x_harness import HarnessRuntime, start_display_server, wait_for_predicate
from mfl_request_live import frame, reply
from mfl_stream_live import Connection
from session_x_errors import SessionXErrors


class DestroyNotify(C.Structure):
    _fields_ = [('type', C.c_int), ('serial', C.c_ulong),
        ('send_event', C.c_int), ('display', C.c_void_p),
        ('event', C.c_ulong), ('window', C.c_ulong)]


def run(args, profile):
    out = args.artifacts_dir / profile
    out.mkdir(parents=True, exist_ok=False)
    evidence = {'profile': profile, 'settings': [], 'paint_observations': {}}
    source, build = Path(__file__).resolve().parents[2], args.build.resolve()
    with HarnessRuntime(prefix='fvwm-composition-') as runtime:
        control = rpc = client = errors = None
        try:
            _, display = start_display_server(runtime, 'Xvfb',
                ['Xvfb', '-screen', '0', '1440x1000x24', '-noreset'],
                'xserver.log', time.monotonic() + 10)
            user, modules = runtime.root / 'user', runtime.root / 'modules'
            user.mkdir(mode=0o700)
            modules.mkdir()
            for directory in (build / 'modules').iterdir():
                executable = directory / directory.name
                if executable.is_file():
                    (modules / directory.name).symlink_to(executable)
            (modules / 'FvwmDesktop').symlink_to(build / 'bin/FvwmPrompt/FvwmDesktop')
            root = source / 'default-config'
            body = (root / 'config').read_text().replace(
                '$[FVWM_DATADIR]/default-config', str(root))
            if profile == 'classic-large':
                body = body.replace('Sans:size=10:antialias=True',
                    'DejaVu Sans Mono:size=18:antialias=True')
            config = runtime.root / 'config'
            config.write_text('ModulePath ' + str(modules) + '\nRead ' +
                str(source / 'fvwm/ConfigFvwmDefaults') + '\n' + body +
                '\nAddToFunc StartFunction I Module FvwmMFL\n' +
                '*ThemeContrastProbe: Title \"Theme contrast probe\"\n' +
                '*ThemeContrastProbe: Colorset 0\n' +
                '*ThemeContrastProbe: ItemColorset 0\n' +
                '*ThemeContrastProbe: Position 80 180\n' +
                '*ThemeContrastProbe: Line left\n' +
                '*ThemeContrastProbe: Text \"Existing Form colorset zero\"\n' +
                '*ThemeContrastProbe: Line left\n' +
                '*ThemeContrastProbe: Button quit \"Close\" ^[\n' +
                '*ThemeContrastProbe: Command Nop\n')
            (out / 'configuration.fvwm').write_bytes(config.read_bytes())
            evidence['configuration_sha256'] = hashlib.sha256(config.read_bytes()).hexdigest()
            evidence['palettes_sha256'] = hashlib.sha256((root / 'palettes.json').read_bytes()).hexdigest()
            env = runtime.environment({'DISPLAY': display, 'HOME': str(runtime.root),
                'FVWM_USERDIR': str(user), 'FVWM_DESKTOP_ROOT': str(root),
                'FVWMMFL_SOCKET_PATH': str(runtime.root),
                'XDG_DATA_HOME': str(runtime.root / 'data'),
                'XDG_DATA_DIRS': str(runtime.root / 'absent')})
            core = runtime.start('core', [str(build / 'fvwm3'), '-d', display,
                '-f', str(config)], 'core.log', env)
            endpoint = runtime.root / ('fvwm_mfl_' + display.replace(':', '_') + '.sock')
            wait_for_predicate('composition MFL', endpoint.exists,
                time.monotonic() + 10, process=core)
            control, rpc = Connection(endpoint), Connection(endpoint)
            for connection in (control, rpc):
                connection.socket.settimeout(15)
                connection.record()
            control.send(b'set echo\n')
            serial = 0

            def request(method='query', resource='settings', **fields):
                nonlocal serial
                serial += 1
                token = 'composition-' + str(serial)
                rpc.send(frame(token, method=method, resource=resource, **fields))
                return reply(rpc, token)

            deadline = time.monotonic() + 15
            while True:
                serial += 1
                token = 'ready-' + str(serial)
                rpc.send(frame(token, resource='settings', scope='all'))
                message = rpc.record()['reply']
                if message['status'] == 'ok':
                    snapshot = message['result']
                    if snapshot['settings']['palette']['application'] == 'submitted-to-fvwm':
                        break
                assert time.monotonic() < deadline, message
                time.sleep(.02)

            def barrier():
                nonlocal serial
                serial += 1
                token = 'composition-barrier-' + str(serial)
                control.send(('SendToModule FvwmDesktop barrier ' + token + '\n').encode())
                control.expect_echo(token)

            def change(values):
                nonlocal snapshot
                snapshot = request('action', operation='set', provider=snapshot['provider'],
                    generation=snapshot['generation'], values=values)
                barrier()
                evidence['settings'].append({'values': values, 'result': snapshot})
                return snapshot

            palette, mode = ('dune', 'light') if profile == 'dune-light' else ('slate', 'dark')
            change({'palette': palette, 'appearance': mode, 'transparency': False})
            client = connect_client(display)
            errors = SessionXErrors(client, evidence).install()
            x, d = client.x, client.display
            x.XGetImage.argtypes = [C.c_void_p, C.c_ulong, C.c_int, C.c_int,
                C.c_uint, C.c_uint, C.c_ulong, C.c_int]
            x.XGetImage.restype = C.c_void_p
            x.XGetPixel.argtypes = [C.c_void_p, C.c_int, C.c_int]
            x.XGetPixel.restype = C.c_ulong
            x.XDestroyImage.argtypes = [C.c_void_p]
            x.XFetchName.argtypes = [C.c_void_p, C.c_ulong, C.POINTER(C.c_void_p)]
            x.XGetWindowAttributes.argtypes = [C.c_void_p, C.c_ulong,
                C.POINTER(WindowAttributes)]

            def pixel(px, py):
                image = x.XGetImage(d, client.root, px, py, 1, 1, C.c_ulong(-1).value, 2)
                assert image
                try:
                    return x.XGetPixel(image, 0, 0)
                finally:
                    x.XDestroyImage(image)

            def wallpaper():
                # Require a stable observed root sample, not only command submission.
                previous, matches = None, 0
                def stable():
                    nonlocal previous, matches
                    value = pixel(10, 950)
                    matches = matches + 1 if value == previous and value != 0 else 0
                    previous = value
                    return value if matches >= 4 else None
                return wait_for_predicate('stable wallpaper sample', stable,
                    time.monotonic() + 10, process=core)

            palettes = json.loads((root / 'palettes.json').read_text())
            def bar_color(name, appearance):
                expected = int(palettes[name][appearance]['surface'][1:], 16)
                wait_for_predicate('native bar palette', lambda: pixel(5, 5) == expected,
                    time.monotonic() + 10, process=core)
                return expected

            def attributes(window):
                attrs = WindowAttributes()
                if errors.read(window, 3,
                        lambda: x.XGetWindowAttributes(d, window, C.byref(attrs))):
                    return attrs
                return None

            def children(window):
                root_, parent, count = C.c_ulong(), C.c_ulong(), C.c_uint()
                items = C.POINTER(C.c_ulong)()
                try:
                    if errors.read(window, 15, lambda: x.XQueryTree(d, window,
                            C.byref(root_), C.byref(parent), C.byref(items), C.byref(count))):
                        return list(items[:min(count.value, 512)])
                    return []
                finally:
                    if items:
                        x.XFree(items)

            def named_windows():
                found, queue = {}, [(client.root, 0)]
                visited = 0
                while queue and visited < 2048:
                    window, depth = queue.pop()
                    visited += 1
                    name = C.c_void_p()
                    try:
                        ok = errors.read(window, 20,
                            lambda: x.XFetchName(d, window, C.byref(name)))
                        if ok and name.value:
                            attrs = attributes(window)
                            if attrs and attrs.map_state == 2:
                                found[C.string_at(name).decode('utf-8', 'replace')] = window
                    finally:
                        if name.value:
                            x.XFree(name)
                    if depth < 4:
                        queue.extend((child, depth + 1) for child in children(window))
                return found

            def capture(filename):
                result = subprocess.run([str(build / 'bin/fvwm-desktop-capture'), str(out)],
                    env=env, capture_output=True, text=True, timeout=35)
                assert result.returncode == 0, result.stderr
                Path(result.stdout.strip().removeprefix('Screenshot saved: ')).replace(out / filename)
                evidence.setdefault('captures', []).append({'file': filename,
                    'sha256': hashlib.sha256((out / filename).read_bytes()).hexdigest(),
                    'bytes': (out / filename).stat().st_size})

            def contrast_cases():
                nonlocal snapshot
                pointer = Pointer(client)
                x.XStringToKeysym.argtypes = [C.c_char_p]
                x.XStringToKeysym.restype = C.c_ulong
                x.XKeysymToKeycode.argtypes = [C.c_void_p, C.c_ulong]
                x.XKeysymToKeycode.restype = C.c_ubyte
                pointer.x.XTestFakeKeyEvent.argtypes = [C.c_void_p, C.c_uint, C.c_int, C.c_ulong]
                pointer.x.XTestFakeKeyEvent.restype = C.c_int
                x.XCheckTypedWindowEvent.argtypes = [C.c_void_p, C.c_ulong, C.c_int,
                    C.POINTER(Event)]

                def key(symbol):
                    code = x.XKeysymToKeycode(d, x.XStringToKeysym(symbol))
                    assert code
                    for pressed in (1, 0):
                        assert pointer.x.XTestFakeKeyEvent(d, code, pressed, 0)
                    x.XSync(d, 0)

                def opacity():
                    page = request(resource='menu-styles', limit=16)
                    return next(item['opacity_percent'] for item in page['menu_styles']
                        if item['name'] == 'SlateLauncherStyle')

                def histogram(window):
                    attrs = attributes(window)
                    assert attrs and attrs.map_state == 2
                    assert 0 < attrs.width <= 1440 and 0 < attrs.height <= 1000
                    image = x.XGetImage(d, window, 0, 0, attrs.width, attrs.height,
                        C.c_ulong(-1).value, 2)
                    assert image
                    counts = Counter()
                    try:
                        # Sample every other pixel; retain a bounded numerical
                        # surface/text observation independently of screenshots.
                        for py in range(2, attrs.height - 2, 2):
                            for px in range(2, attrs.width - 2, 2):
                                counts[x.XGetPixel(image, px, py)] += 1
                    finally:
                        x.XDestroyImage(image)
                    return counts

                def monochrome(window, surface, text):
                    counts = histogram(window)
                    if counts[surface] <= sum(counts.values()) * .35 or not counts[text]:
                        return None
                    return {'surface': surface, 'text': text,
                        'surface_samples': counts[surface], 'text_samples': counts[text],
                        'total_samples': sum(counts.values()), 'window': window}

                def open_editor():
                    control.send(b'SendToModule FvwmDesktop edit-settings group:appearance\n')
                    return wait_for_predicate('high contrast editor',
                        lambda: named_windows().get('Desktop settings'),
                        time.monotonic() + 10, process=core)

                def dismiss_editor(window):
                    control.send(('WindowId ' + str(window) + ' Close\n').encode())
                    wait_for_predicate('contrast editor closed',
                        lambda: 'Desktop settings' not in named_windows(),
                        time.monotonic() + 10, process=core)
                    barrier()

                def menu_pixels(appearance):
                    def mapped_menus():
                        # Match the viewable save-under surface predicate used
                        # by the existing native menu fixtures.
                        found = []
                        for window in children(client.root):
                            attrs = attributes(window)
                            if (attrs and attrs.map_state == 2 and attrs.save_under
                                    and attrs.width > 20 and attrs.height > 20):
                                found.append({'window': window,
                                    'width': attrs.width, 'height': attrs.height,
                                    'save_under': attrs.save_under,
                                    'override_redirect': attrs.override_redirect})
                        evidence.setdefault('launcher_discovery', {})[appearance] = found
                        return found

                    before = {item['window'] for item in mapped_menus()}
                    pointer.motion(1050, 100)
                    control.send(b'SlateLauncher\n')
                    wait_for_predicate('high contrast launcher grab', pointer.grabbed,
                        time.monotonic() + 10, process=core)
                    def mapped_menu():
                        menus = [item['window'] for item in mapped_menus()
                            if item['window'] not in before]
                        return menus[0] if len(menus) == 1 else None
                    try:
                        window = wait_for_predicate('actual launcher window', mapped_menu,
                            time.monotonic() + 5, process=core)
                        surface, text = (0, 0xffffff) if appearance == 'dark' else (0xffffff, 0)
                        observed = wait_for_predicate('opaque launcher pixels',
                            lambda: monochrome(window, surface, text),
                            time.monotonic() + 5, process=core)
                        capture('high-' + appearance + '-menu.png')
                        return observed
                    finally:
                        key(b'Escape')
                        wait_for_predicate('launcher dismissed before changing theme',
                            lambda: not pointer.grabbed(), time.monotonic() + 5, process=core)

                def resource_preview_cases(probe, normal_bar, independent):
                    nonlocal snapshot
                    saved = request(scope='all')
                    baseline_values = {name: value['saved'] for name, value in saved['settings'].items()}

                    def restore_baseline():
                        # Setup uses the existing eight-value IPC bound. The
                        # all-eleven-field native Apply assertion stays above.
                        names = sorted(baseline_values)
                        for offset in range(0, len(names), 8):
                            change({name: baseline_values[name]
                                for name in names[offset:offset + 8]})
                    defaults = dict(palette='slate', appearance='auto',
                        day='07:00', night='19:00', transparency=True, motion=False,
                        keep_wallpaper=False, launcher_opacity=94,
                        wallpaper_palette='follow', wallpaper_appearance='follow',
                        contrast='normal')
                    cases = {}

                    def wallpaper_owner():
                        # The enlarged all-fields editor may cover the usual
                        # screen sample. Observe the actual wallpaper property
                        # while it is mapped, rather than its overlying pixels.
                        x.XInternAtom.argtypes = [C.c_void_p, C.c_char_p, C.c_int]
                        x.XInternAtom.restype = C.c_ulong
                        x.XGetWindowProperty.argtypes = [C.c_void_p, C.c_ulong,
                            C.c_ulong, C.c_long, C.c_long, C.c_int, C.c_ulong,
                            C.POINTER(C.c_ulong), C.POINTER(C.c_int),
                            C.POINTER(C.c_ulong), C.POINTER(C.c_ulong),
                            C.POINTER(C.POINTER(C.c_ubyte))]
                        atom = x.XInternAtom(d, b'_XROOTPMAP_ID', 0)
                        type_, count, after = C.c_ulong(), C.c_ulong(), C.c_ulong()
                        format_, data = C.c_int(), C.POINTER(C.c_ubyte)()
                        try:
                            assert x.XGetWindowProperty(d, client.root, atom, 0, 1,
                                0, 0, C.byref(type_), C.byref(format_), C.byref(count),
                                C.byref(after), C.byref(data)) == 0
                            assert format_.value == 32 and count.value == 1 and data
                            value = C.cast(data, C.POINTER(C.c_ulong))[0]
                            assert value > 1
                            return value
                        finally:
                            if data:
                                x.XFree(data)

                    x.XGetInputFocus.argtypes = [C.c_void_p,
                        C.POINTER(C.c_ulong), C.POINTER(C.c_int)]
                    x.XTranslateCoordinates.argtypes = [C.c_void_p, C.c_ulong,
                        C.c_ulong, C.c_int, C.c_int, C.POINTER(C.c_int),
                        C.POINTER(C.c_int), C.POINTER(C.c_ulong)]

                    def parent_of(window):
                        root_, parent, count = C.c_ulong(), C.c_ulong(), C.c_uint()
                        items = C.POINTER(C.c_ulong)()
                        try:
                            ok = errors.read(window, 15, lambda: x.XQueryTree(d,
                                window, C.byref(root_), C.byref(parent),
                                C.byref(items), C.byref(count)))
                            return parent.value if ok else None
                        finally:
                            if items:
                                x.XFree(items)

                    def form_key(window, symbol):
                        witness = {'window': window, 'keysym': symbol.decode('ascii'),
                            'status': 'waiting-for-native-form'}
                        evidence.setdefault('keyboard_observations', []).append(witness)
                        def ready():
                            attrs = attributes(window)
                            parent = parent_of(window)
                            witness['readiness'] = {'parent': parent,
                                'map_state': attrs.map_state if attrs else None}
                            if not attrs or attrs.map_state != 2 or parent in (None, client.root):
                                return None
                            frame = parent_of(parent)
                            mapped_controls = sum(bool(control_attrs and
                                control_attrs.map_state == 2 and
                                control_attrs.all_event_masks & (1 << 2))
                                for control_attrs in (attributes(child)
                                    for child in children(window)))
                            witness['readiness'] = {'parent': parent, 'frame': frame,
                                'map_state': attrs.map_state,
                                'keypress_selected': bool(attrs.all_event_masks & 1),
                                'mapped_button_controls': mapped_controls}
                            return (witness['readiness'] if frame not in (None, client.root)
                                and parent_of(frame) == client.root and
                                attrs.all_event_masks & 1 and mapped_controls else None)
                        wait_for_predicate('managed native Form keyboard readiness', ready,
                            time.monotonic() + 10, process=core)
                        geometry = {'profile': profile, 'screen_width': 1440,
                            'screen_height': 1000, 'mapped_controls': []}
                        witness['geometry'] = geometry
                        def rectangle(target, destination, attrs):
                            tx, ty, child = C.c_int(), C.c_int(), C.c_ulong()
                            assert x.XTranslateCoordinates(d, target, destination,
                                0, 0, C.byref(tx), C.byref(ty), C.byref(child))
                            # Translate returns the interior origin. Include
                            # the real server border in each containment check.
                            border = attrs.border_width
                            return {'window': target, 'x': tx.value - border,
                                'y': ty.value - border,
                                'width': attrs.width + 2 * border,
                                'height': attrs.height + 2 * border,
                                'interior_width': attrs.width,
                                'interior_height': attrs.height,
                                'border_width': border}
                        form_attrs = attributes(window)
                        frame = witness['readiness']['frame']
                        frame_attrs = attributes(frame)
                        assert form_attrs and frame_attrs
                        for label, target, attrs in (('client', window, form_attrs),
                                ('frame', frame, frame_attrs)):
                            assert attrs.map_state == 2
                            rect = rectangle(target, client.root, attrs)
                            geometry[label] = rect
                            assert (rect['x'] >= 0 and rect['y'] >= 0 and
                                rect['x'] + rect['width'] <= 1440 and
                                rect['y'] + rect['height'] <= 1000), geometry
                        # Labels drawn directly into the Form have no X child;
                        # only actual mapped immediate control windows apply.
                        for child in children(window):
                            attrs = attributes(child)
                            if not attrs or attrs.map_state != 2:
                                continue
                            relative = rectangle(child, window, attrs)
                            observed = {'relative_to_client': relative,
                                'root': rectangle(child, client.root, attrs)}
                            geometry['mapped_controls'].append(observed)
                            assert (relative['x'] >= 0 and relative['y'] >= 0 and
                                relative['x'] + relative['width'] <= form_attrs.width and
                                relative['y'] + relative['height'] <= form_attrs.height), geometry
                        geometry['within_bounds'] = True
                        # A visible title and another module's barrier do not
                        # establish that the Form is managed or has X focus.
                        # Its top-level window selects KeyPress; verify that
                        # actual recipient before injecting the real key.
                        control.send(('WindowId ' + str(window) + ' Focus NoWarp\n').encode())
                        barrier()
                        def focused():
                            actual, revert = C.c_ulong(), C.c_int()
                            x.XGetInputFocus(d, C.byref(actual), C.byref(revert))
                            witness['actual_focus'] = actual.value
                            witness['revert_to'] = revert.value
                            return actual.value == window
                        wait_for_predicate('actual native Form keyboard focus', focused,
                            time.monotonic() + 10, process=core)
                        assert focused(), witness
                        witness['status'] = 'focus-observed'
                        key(symbol)
                        witness['status'] = 'focused-key-submitted'

                    def preview_from_editor():
                        # Defaults supplies a deterministic, independent draft
                        # differing from the committed opacity/wallpaper. F5
                        # must capture it without applying these other fields.
                        control.send(b'SendToModule FvwmDesktop edit-settings all\n')
                        editor = wait_for_predicate('complete settings editor',
                            lambda: named_windows().get('Desktop settings'),
                            time.monotonic() + 10, process=core)
                        previous = editor
                        attrs = attributes(previous)
                        assert attrs and attrs.map_state == 2
                        replacement = {'old_window': previous,
                            'observer_event_mask_before': attrs.your_event_mask,
                            'status': 'waiting-for-old-form-destruction',
                            'drained_prior_destroy_events': 0}
                        evidence.setdefault('editor_replacements', []).append(replacement)
                        x.XSelectInput(d, previous, attrs.your_event_mask | (1 << 17))
                        x.XSync(d, 0)
                        event = Event()
                        while x.XCheckTypedWindowEvent(d, previous, 17, C.byref(event)):
                            replacement['drained_prior_destroy_events'] += 1
                        form_key(editor, b'F2')
                        def destroyed():
                            if not x.XCheckTypedWindowEvent(d, previous, 17, C.byref(event)):
                                return None
                            observed = C.cast(C.byref(event), C.POINTER(DestroyNotify)).contents
                            assert observed.type == 17 and not observed.send_event
                            assert observed.event == previous and observed.window == previous
                            replacement.update(status='old-form-destroyed',
                                destroyed_window=observed.window,
                                destroy_event_window=observed.event,
                                destroy_event_serial=observed.serial,
                                destroy_event_synthetic=bool(observed.send_event))
                            return True
                        wait_for_predicate('actual old Defaults editor destruction', destroyed,
                            time.monotonic() + 10, process=core)
                        # The server can reuse a departed Form process's XID.
                        # Destruction establishes the incarnation boundary;
                        # unequal integer IDs do not establish freshness.
                        editor = wait_for_predicate('defaults draft editor',
                            lambda: named_windows().get('Desktop settings'),
                            time.monotonic() + 10, process=core)
                        replacement.update(status='mapped-replacement-observed',
                            replacement_window=editor, xid_reused=editor == previous)
                        form_key(editor, b'F5')
                        return wait_for_predicate('actual high contrast Save Cancel Form',
                            lambda: named_windows().get('Preview high contrast'),
                            time.monotonic() + 10, process=core)

                    for ending in ('cancel', 'save'):
                        before = state.read_bytes()
                        raw = json.loads(before)
                        original_wallpaper = wallpaper_owner()
                        preview = preview_from_editor()
                        assert state.read_bytes() == before
                        assert wallpaper() == independent and opacity() == 100
                        surface, text = (0, 0xffffff) if mode == 'dark' else (0xffffff, 0)
                        observed = wait_for_predicate('preview recolors existing native Form',
                            lambda: monochrome(probe, surface, text),
                            time.monotonic() + 5, process=core)
                        assert named_windows().get('Theme contrast probe') == probe
                        capture('resource-preview-' + ending + '-active.png')
                        form_key(preview, b'Escape' if ending == 'cancel' else b'Return')
                        wait_for_predicate('contrast preview retired after ' + ending,
                            lambda: 'Preview high contrast' not in named_windows(),
                            time.monotonic() + 10, process=core)
                        editor = wait_for_predicate('captured unrelated draft returned',
                            lambda: named_windows().get('Desktop settings'),
                            time.monotonic() + 10, process=core)
                        barrier()
                        if ending == 'cancel':
                            assert state.read_bytes() == before
                            wait_for_predicate('cancel restores actual normal bar',
                                lambda: pixel(5, 5) == normal_bar,
                                time.monotonic() + 10, process=core)
                            assert opacity() == 82
                        else:
                            wait_for_predicate('Save persists contrast only',
                                lambda: json.loads(state.read_text()).get('contrast') == 'high',
                                time.monotonic() + 5, process=core)
                            expected = dict(raw, contrast='high')
                            assert json.loads(state.read_text()) == expected
                            assert opacity() == 100
                            # Slot release is observable only after the core's
                            # terminal confirmation, rather than queued Save.
                            capability = request(resource='appearance')['resource_preview']
                            assert capability['slot_available'] is True, capability
                        assert wallpaper_owner() == original_wallpaper
                        capture('resource-preview-' + ending + '-returned-draft.png')
                        # Applying the returned draft proves every captured
                        # field survived. Save updates only its contrast field.
                        form_key(editor, b'F3')
                        wanted = dict(defaults, contrast='high' if ending == 'save' else 'normal')
                        def draft_applied():
                            observed_ = request(scope='all')
                            values = {name: value['saved'] for name, value in observed_['settings'].items()}
                            return observed_ if values == wanted else None
                        snapshot = wait_for_predicate('all unrelated draft values retained',
                            draft_applied, time.monotonic() + 10, process=core)
                        assert json.loads(state.read_text())['launcher_opacity'] == 94
                        cases[ending] = {'existing_form': observed,
                            'contrast_only_before_apply': True, 'returned_draft': wanted}
                        snapshot = request(scope='all')
                        restore_baseline()
                        wait_for_predicate('normal baseline resources recovered',
                            lambda: pixel(5, 5) == normal_bar and opacity() == 82,
                            time.monotonic() + 10, process=core)
                        assert wallpaper() == independent
                        control.send(b'KillModule FvwmForm SlateNoticeForm\n')

                    # An actual external file edit must survive a later Save
                    # callback. The controller must recover without persisting
                    # a stale contrast snapshot or replacing the external bytes.
                    before = json.loads(state.read_text())
                    snapshot = request(scope='all')
                    control.send(('SendToModule FvwmDesktop preview-contrast ' +
                        snapshot['provider'] + '\n').encode())
                    preview = wait_for_predicate('menu entry high contrast Form',
                        lambda: named_windows().get('Preview high contrast'),
                        time.monotonic() + 10, process=core)
                    external = dict(before, launcher_opacity=37)
                    external_bytes = (json.dumps(external, indent=3) + '\n').encode()
                    state.write_bytes(external_bytes)
                    form_key(preview, b'Return')
                    wait_for_predicate('external-edit preview refusal recovery',
                        lambda: 'Preview high contrast' not in named_windows() and
                            opacity() == 82 and pixel(5, 5) == normal_bar,
                        time.monotonic() + 10, process=core)
                    assert state.read_bytes() == external_bytes
                    capture('resource-preview-external-edit-recovered.png')
                    cases['external_file_edit_preserved'] = True
                    snapshot = request(scope='all')
                    snapshot = request('action', operation='reload', provider=snapshot['provider'],
                        generation=snapshot['generation'])
                    barrier()
                    snapshot = request(scope='all')
                    restore_baseline()
                    control.send(b'KillModule FvwmForm SlateNoticeForm\n')
                    wait_for_predicate('resource preview final normal baseline',
                        lambda: pixel(5, 5) == normal_bar and opacity() == 82,
                        time.monotonic() + 10, process=core)
                    evidence['resource_preview'] = cases

                change({'contrast': 'normal', 'transparency': True, 'launcher_opacity': 82,
                    'wallpaper_palette': 'grove', 'wallpaper_appearance': 'light'})
                independent = wallpaper()
                wait_for_predicate('normal requested launcher opacity', lambda: opacity() == 82,
                    time.monotonic() + 10, process=core)
                normal_bar = wait_for_predicate('normal tinted bar',
                    lambda: pixel(5, 5) if pixel(5, 5) !=
                        int(palettes[palette][mode]['surface'][1:], 16) else None,
                    time.monotonic() + 10, process=core)
                # This named consumer reads its complete module configuration
                # at launch. Theme transitions must recolor its existing window.
                control.send(b'Module FvwmForm ThemeContrastProbe\n')
                probe = wait_for_predicate('standalone colorset zero Form',
                    lambda: named_windows().get('Theme contrast probe'),
                    time.monotonic() + 10, process=core)
                x.XSelectInput(d, probe, 1 << 17)
                x.XSync(d, 0)
                if args.contrast_preview:
                    resource_preview_cases(probe, normal_bar, independent)
                observations = {}
                for appearance in ('dark', 'light'):
                    change({'contrast': 'high', 'appearance': appearance})
                    surface, text = (0, 0xffffff) if appearance == 'dark' else (0xffffff, 0)
                    wait_for_predicate('exact opaque bar ' + appearance,
                        lambda: pixel(5, 5) == surface, time.monotonic() + 10, process=core)
                    assert wallpaper() == independent
                    snapshot = request(scope='all')
                    for setting, resolved in [('transparency', False), ('launcher_opacity', 100)]:
                        item = snapshot['settings'][setting]
                        assert item['resolved'] == resolved and item['resolution'] == 'high-contrast', item
                        assert item['runtime_constraint'] == 'high-contrast', item
                        assert item['composition_version'] == 2, item
                        assert item['effective'] == (True if setting == 'transparency' else 82), item
                    raw = json.loads(state.read_text())
                    assert raw['schema_version'] == 4 and raw['contrast'] == 'high'
                    assert raw['transparency'] is True and raw['launcher_opacity'] == 82
                    assert raw['wallpaper'] == {'palette': 'grove', 'appearance': 'light'}
                    assert opacity() == 100
                    assert named_windows().get('Theme contrast probe') == probe
                    event = Event()
                    assert not x.XCheckTypedWindowEvent(d, probe, 17, C.byref(event)), 'Form recreated'
                    form_pixels = wait_for_predicate('existing Form recolored ' + appearance,
                        lambda: monochrome(probe, surface, text),
                        time.monotonic() + 10, process=core)
                    capture('high-' + appearance + '-form.png')
                    editor = open_editor()
                    capture('high-' + appearance + '-editor.png')
                    dismiss_editor(editor)
                    observations[appearance] = {'bar': surface, 'form': form_pixels,
                        'menu': menu_pixels(appearance), 'launcher_opacity': 100}

                def assert_normal():
                    assert opacity() == 82
                    wait_for_predicate('requested normal tint restored',
                        lambda: pixel(5, 5) == normal_bar, time.monotonic() + 10, process=core)
                    assert wallpaper() == independent

                change({'contrast': 'normal', 'appearance': mode})
                assert_normal()
                # Both native entry points own a core preview lease. Enabling
                # high contrast must retire each dialog and release that lease.
                for command in ('preview-setting', 'preview-opacity'):
                    control.send(('SendToModule FvwmDesktop ' + command + ' ' +
                        snapshot['provider'] + ' 70\n').encode())
                    wait_for_predicate(command + ' active dialog',
                        lambda: named_windows().get('Preview menu opacity'),
                        time.monotonic() + 10, process=core)
                    wait_for_predicate(command + ' actual opacity', lambda: opacity() == 70,
                        time.monotonic() + 5, process=core)
                    change({'contrast': 'high'})
                    wait_for_predicate(command + ' closes under high contrast',
                        lambda: 'Preview menu opacity' not in named_windows(),
                        time.monotonic() + 5, process=core)
                    assert opacity() == 100
                    # Acquire an independent scalar-100 lease without relaxing
                    # high contrast; success proves the predecessor lease ended.
                    generation = request(resource='appearance')['generation']
                    lease = request('action', resource='preview', operation='start',
                        target='SlateLauncherStyle', generation=generation,
                        values={'opacity': 100}, timeout_ms=1000)
                    assert lease['state'] == 'active'
                    ended = request('action', resource='preview', operation='revert',
                        target=lease['lease'], generation=lease['generation'])
                    assert ended['state'] == 'reverted' and opacity() == 100
                    before = state.read_bytes()
                    control.send(b'KillModule FvwmForm SlateNoticeForm\n')
                    wait_for_predicate('transition notice retired',
                        lambda: 'FVWM-X' not in named_windows(),
                        time.monotonic() + 5, process=core)
                    for refused in ('preview-setting', 'preview-opacity'):
                        control.send(('SendToModule FvwmDesktop ' + refused + ' ' +
                            snapshot['provider'] + ' 70\n').encode())
                        wait_for_predicate(refused + ' high contrast refusal notice',
                            lambda: named_windows().get('FVWM-X'),
                            time.monotonic() + 5, process=core)
                        barrier()
                        assert 'Preview menu opacity' not in named_windows()
                        assert opacity() == 100 and state.read_bytes() == before
                        control.send(b'KillModule FvwmForm SlateNoticeForm\n')
                        wait_for_predicate('refusal notice retired',
                            lambda: 'FVWM-X' not in named_windows(),
                            time.monotonic() + 5, process=core)
                    change({'contrast': 'normal'})
                    assert_normal()

                change({'contrast': 'high'})
                snapshot = request('action', operation='reload', provider=snapshot['provider'],
                    generation=snapshot['generation'])
                barrier()
                assert snapshot['settings']['contrast']['effective'] == 'high' and opacity() == 100
                snapshot = request('action', operation='reset', provider=snapshot['provider'],
                    generation=snapshot['generation'], scope='contrast')
                barrier()
                assert snapshot['settings']['contrast']['saved'] == 'normal'
                assert_normal()
                external = json.loads(state.read_text())
                external['contrast'] = 'high'
                state.write_text(json.dumps(external))
                snapshot = request('action', operation='reload', provider=snapshot['provider'],
                    generation=snapshot['generation'])
                barrier()
                assert snapshot['settings']['contrast']['effective'] == 'high' and opacity() == 100
                change({'contrast': 'normal'})
                assert_normal()
                control.send(b'KillModule FvwmForm ThemeContrastProbe\n')
                wait_for_predicate('standalone probe cleanup',
                    lambda: 'Theme contrast probe' not in named_windows(),
                    time.monotonic() + 5, process=core)
                evidence['contrast'] = {'observations': observations,
                    'normal_bar': normal_bar, 'wallpaper': independent,
                    'requests_retained': True, 'existing_form_recolored': True,
                    'saved_and_runtime_preview_retired_and_lease_released': True,
                    'fresh_native_previews_refused': True, 'reset_and_reload': True,
                    'normal_restores_requests': True}

            state = user / 'desktop-state.json'
            if args.contrast:
                contrast_cases()
            else:
                initial = wallpaper()
                base_bar = bar_color(palette, mode)
                change({'wallpaper_palette': 'grove', 'wallpaper_appearance': 'light'})
                wait_for_predicate('independent wallpaper pixels',
                    lambda: pixel(10, 950) != initial, time.monotonic() + 10, process=core)
                independent = wallpaper()
                assert pixel(5, 5) == base_bar
                assert json.loads(state.read_text())['wallpaper'] == {'palette': 'grove', 'appearance': 'light'}
                opposite = 'ink' if palette != 'ink' else 'slate'
                change({'palette': opposite})
                bar_color(opposite, mode)
                assert wallpaper() == independent
                change({'keep_wallpaper': True, 'wallpaper_palette': 'dune',
                    'wallpaper_appearance': 'dark'})
                held = snapshot['settings']['wallpaper_palette']
                assert held['resolved'] == 'grove' and held['resolution'] == 'retained'
                assert held['runtime_constraint'] == 'wallpaper-retained'
                assert wallpaper() == independent
                snapshot = request('action', operation='reload', provider=snapshot['provider'],
                    generation=snapshot['generation'])
                barrier()
                assert wallpaper() == independent
                change({'keep_wallpaper': False})
                wait_for_predicate('released wallpaper choice',
                    lambda: pixel(10, 950) != independent, time.monotonic() + 10, process=core)
                released = wallpaper()
                snapshot = request('action', operation='reset', provider=snapshot['provider'],
                    generation=snapshot['generation'], scope='group:wallpaper')
                barrier()
                assert set(snapshot['settings']) == {'wallpaper_palette', 'wallpaper_appearance'}
                assert json.loads(state.read_text())['schema_version'] == 4
                assert json.loads(state.read_text())['wallpaper'] == {'palette': opposite, 'appearance': mode}
                snapshot = request(scope='all')
                change({'palette': palette, 'wallpaper_palette': 'grove', 'wallpaper_appearance': 'light'})
                bar_color(palette, mode)
                wallpaper()
                evidence['paint_observations'] = {'initial': initial, 'independent': independent,
                    'released': released, 'base_bar': base_bar,
                    'wallpaper_only_preserves_bar': True, 'ui_change_preserves_explicit_wallpaper': True}
                control.send(b'SendToModule FvwmDesktop edit-settings group:wallpaper\n')
                barrier()

                def find_form(_):
                    return named_windows().get('Desktop settings')

                form = wait_for_predicate('composition editor', lambda: find_form(client.root),
                    time.monotonic() + 10, process=core)
                time.sleep(.1)
                result = subprocess.run([str(build / 'bin/fvwm-desktop-capture'), str(out)],
                    env=env, capture_output=True, text=True, timeout=35)
                assert result.returncode == 0, result.stderr
                Path(result.stdout.strip().removeprefix('Screenshot saved: ')).replace(out / 'composition-editor.png')
                control.send(('WindowId ' + str(form) + ' Close\n').encode())
                barrier()
                before = state.read_bytes()
                control.send(b'*FvwmDesktop: wallpaper_palette ink\n')
                # Module configuration is acquired at startup, as documented; a
                # core command barrier does not make a running helper reread it.
                old_provider = snapshot['provider']
                control.send(b'KillModule FvwmDesktop\n'
                    b'ModuleSynchronous Timeout 15 FvwmDesktop\n')
                deadline = time.monotonic() + 15
                while True:
                    serial += 1
                    token = 'override-ready-' + str(serial)
                    rpc.send(frame(token, resource='settings', scope='group:wallpaper'))
                    response = rpc.record()['reply']
                    if response['status'] == 'ok':
                        observed = response['result']
                        if (observed['provider'] != old_provider and
                                observed['settings']['wallpaper_palette']['source'] ==
                                'module-configuration'):
                            break
                    assert time.monotonic() < deadline, response
                    time.sleep(.02)
                assert observed['settings']['wallpaper_palette']['source'] == 'module-configuration'
                assert observed['settings']['wallpaper_palette']['effective'] == 'ink'
                assert state.read_bytes() == before
                evidence['module_override_no_write'] = observed
                snapshot = request(scope='all')
                change({'keep_wallpaper': True})
                control.send(b'SendToModule FvwmDesktop edit-settings group:wallpaper\n')
                barrier()
                wait_for_predicate('retained composition editor', lambda: find_form(client.root),
                    time.monotonic() + 10, process=core)
                time.sleep(.1)
                result = subprocess.run([str(build / 'bin/fvwm-desktop-capture'), str(out)],
                    env=env, capture_output=True, text=True, timeout=35)
                assert result.returncode == 0, result.stderr
                Path(result.stdout.strip().removeprefix('Screenshot saved: ')).replace(
                    out / 'composition-retained.png')
            evidence['state'] = json.loads(state.read_text())
            evidence['configuration_sha256'] = hashlib.sha256(config.read_bytes()).hexdigest()
            evidence['palettes_sha256'] = hashlib.sha256((root/'palettes.json').read_bytes()).hexdigest()
        finally:
            for connection in (control, rpc):
                if connection:
                    connection.close()
            if errors:
                errors.close_display()
            elif client:
                client.close()
            for log in runtime.root.glob('*.log'):
                shutil.copy2(log, out / log.name)
            (out / 'evidence.json').write_text(json.dumps(evidence, indent=2) + '\n')
            (out / 'capture-manifest.json').write_text(json.dumps({
                'profile': profile,
                'configuration_sha256': evidence.get('configuration_sha256'),
                'palettes_sha256': evidence.get('palettes_sha256'),
                'captures': evidence.get('captures', []),
                'inspection': 'pending human/lead image inspection'}, indent=2) + '\n')
    assert not evidence['observer_unexpected_x_errors'], evidence


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--artifacts-dir', type=Path, required=True)
    parser.add_argument('--contrast', action='store_true',
        help='Run high contrast, native Form/menu pixels and opacity recovery cases')
    parser.add_argument('--contrast-preview', action='store_true',
        help='Also run native resource Save/Cancel and captured editor draft cases')
    parser.add_argument('--profiles', nargs='+', default=['slate-dark', 'dune-light', 'classic-large'],
        choices=['slate-dark', 'dune-light', 'classic-large'])
    options = parser.parse_args()
    if options.contrast_preview:
        options.contrast = True
    for selected in options.profiles:
        run(options, selected)
