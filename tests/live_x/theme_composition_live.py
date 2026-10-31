#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
"""Independent wallpaper and contrast through the desktop owner and real X.

--complete-bundle-preview selects the current compact F5 editor separately
from the historical v1/v2 UI branches. It observes all eight keys through real
menu callbacks and Save/Cancel consumers, captured eleven-field drafts, root
properties/full pixels, native roles/buffer10, and independent menu scalars.
The actual --probe and --worker default to the selected build. Three profiles
produce one new editor capture each, with actual managed-frame/focus readiness
and no forced repaint/resize. Capture inspection remains the lead's gate.

Core decode/allocation/topology/expiry/nested-menu/Wait faults use the companion
contrast_resource_live.py v3 group. Persistence-failure overlays and phase races
retain their separate Go checks. Labels do not imply native-CPU qualification.
"""
import argparse
from settings_snapshot import expanded_settings
from collections import Counter
import ctypes as C
import hashlib
import json
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import sys
import time

from colorset_live import WindowAttributes
from desktop_live import connect_client
from geometry_transition_live import Pointer
from selection_live import Event
from live_x_harness import HarnessRuntime, start_display_server, wait_for_predicate
from mfl_request_live import frame, reply
from mfl_stream_live import Connection
from session_x_errors import SessionXErrors
from browser_continuity_live import cleanup_receipts
from browser_handoff_live import child_identity
from native_module_proxy import module_proxy
from settings_preview_live import PreviewWitness, binary_hash, hashes
from image_colorsets_live import records


class DestroyNotify(C.Structure):
    _fields_ = [('type', C.c_int), ('serial', C.c_ulong),
        ('send_event', C.c_int), ('display', C.c_void_p),
        ('event', C.c_ulong), ('window', C.c_ulong)]


def run(args, profile):
    out = args.artifacts_dir / profile
    out.mkdir(parents=True, exist_ok=False)
    evidence = {'profile': profile, 'status': 'running', 'settings': [],
        'execution_profile': args.execution_profile, 'paint_observations': {},
        'platform': dict(system=os.uname().sysname,
            release=os.uname().release, machine=os.uname().machine),
        'limits': ['No native CPU, physical hardware or release inference.',
            'Topology and allocation/decode faults use the separate core fixture.']}
    source, build = Path(__file__).resolve().parents[2], args.build.resolve()
    with HarnessRuntime(prefix='fvwm-composition-') as runtime:
        control = rpc = client = errors = core = native = None
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
            if args.complete_bundle_preview:
                native = runtime.root / 'native-records'
                native.mkdir()
                for name, executable in (
                        ('FvwmDesktop', build / 'bin/FvwmPrompt/FvwmDesktop'),
                        ('FvwmForm', build / 'modules/FvwmForm/FvwmForm'),
                        ('FvwmBacker', build / 'modules/FvwmBacker/FvwmBacker'),
                        ('FvwmMFL', build / 'modules/FvwmMFL/FvwmMFL'),
                        ('PreviewObserver', args.probe)):
                    wrapper = modules / name
                    wrapper.unlink(missing_ok=True)
                    wrapper.write_text('#!/bin/sh\nexec ' + shlex.join([
                        sys.executable, str(Path(__file__).resolve()),
                        '--module-proxy', str(executable), str(native)]) +
                        ' "$@"\n')
                    wrapper.chmod(0o700)
            root = source / 'default-config'
            if args.complete_bundle_preview:
                assert '*FvwmBacker: CoreOwnedPixmap' in (
                    root / 'desktop.fvwm').read_text()
            body = (root / 'config').read_text().replace(
                '$[FVWM_DATADIR]/default-config', str(root))
            if profile == 'classic-large':
                body = body.replace('Sans:size=10:antialias=True',
                    'DejaVu Sans Mono:size=18:antialias=True')
            config = runtime.root / 'config'
            config.write_text('ModulePath ' + str(modules) + '\nRead ' +
                str(source / 'fvwm/ConfigFvwmDefaults') + '\n' + body +
                '\nAddToFunc StartFunction I Module FvwmMFL\n' +
                ('Style ThemeContrastProbe Layer 6\n'
                    if profile == 'classic-large' and args.palette_preview else '') +
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
            if args.complete_bundle_preview:
                env['FVWM_IMAGE_SVG_WORKER'] = str(args.worker)
            core = runtime.start('core', [str(build / 'fvwm3'), '-d', display,
                '-f', str(config)], 'core.log', env)
            evidence['core_identity'] = child_identity(core.process.pid, os.getpid())
            evidence['binary_inputs'] = {name: binary_hash(build / path)
                for name, path in [('core', 'fvwm3'),
                    ('desktop', 'bin/FvwmPrompt/FvwmDesktop'),
                    ('mfl', 'modules/FvwmMFL/FvwmMFL'),
                    ('form', 'modules/FvwmForm/FvwmForm'),
                    ('backer', 'modules/FvwmBacker/FvwmBacker'),
                    ('capture', 'bin/fvwm-desktop-capture')]}
            inputs = ['tests/live_x/theme_composition_live.py',
                'tests/live_x/settings_preview_live.py',
                'tests/live_x/native_module_proxy.py',
                'tests/live_x/live_x_harness.py', 'tests/live_x/settings_snapshot.py',
                'tests/live_x/session_x_errors.py', 'tests/live_x/colorset_live.py',
                'tests/live_x/mfl_request_live.py', 'tests/live_x/mfl_stream_live.py',
                'tests/live_x/image_colorsets_live.py',
                'tests/live_x/geometry_transition_live.py',
                'tests/live_x/browser_continuity_live.py',
                'tests/live_x/browser_handoff_live.py',
                'fvwm/appearance_preview.c', 'fvwm/preview_policy.c',
                'fvwm/colorset.c', 'fvwm/image_async.c', 'fvwm/image_async.h',
                'fvwm/image_async_wait.c', 'fvwm/module_interface.c',
                'fvwm/query_json.c', 'libs/PictureImageLoader.c',
                'modules/FvwmMFL/request.c', 'modules/FvwmMFL/FvwmMFL.c',
                'modules/FvwmBacker/FvwmBacker.c',
                'default-config/config', 'default-config/desktop.fvwm']
            for directory in ('bin/FvwmPrompt/internal/desktopcontroller',
                    'bin/FvwmPrompt/internal/desktopsettings'):
                inputs.extend(str(path.relative_to(source))
                    for path in sorted((source / directory).glob('*.go')))
            inputs.extend(str(path.relative_to(source))
                for path in sorted((source / 'fvwm').glob('root_background.*')))
            for pattern in ('picture_async*.[ch]',
                    'image_background*.[ch]', 'image_effect*.[ch]',
                    'image_png*.[ch]', 'image_process*.[ch]'):
                inputs.extend(str(path.relative_to(source))
                    for path in sorted((source / 'libs').glob(pattern)))
            inputs.append('bin/fvwm-image-svg-worker.c')
            inputs.extend(str(path.relative_to(source))
                for path in sorted((root / 'wallpapers').glob('*.png')))
            evidence['source_sha256'] = hashes(source, inputs)
            evidence['generated_config'] = binary_hash(build / 'config.h')
            if args.complete_bundle_preview:
                evidence['binary_inputs']['probe'] = binary_hash(args.probe)
                evidence['binary_inputs']['worker'] = binary_hash(args.worker)
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
                    snapshot = expanded_settings(message['result'])
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
            preview_updates = runtime.root / 'preview-roles.txt'
            if args.complete_bundle_preview:
                control.send(('Module PreviewObserver colorsets ' +
                    str(preview_updates) + '\n').encode())
                wait_for_predicate('actual native preview subscriber snapshot',
                    lambda: set((0, 1, 2, 3, 4, 5, 6, 7, 8, 10, 11, 12,
                        13, 14, 20)) <= {row[0] for row in records(preview_updates)},
                    time.monotonic() + 10, process=core)

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

                def press_key(symbol):
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
                        press_key(b'Escape')
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

                    def managed_frame(window):
                        parent = parent_of(window)
                        if parent in (None, client.root):
                            return None
                        frame = parent_of(parent)
                        return frame if frame not in (None, client.root) and parent_of(frame) == client.root else None

                    def above_probe(window):
                        frame, probe_frame = managed_frame(window), managed_frame(probe)
                        order = children(client.root)
                        if frame not in order or probe_frame not in order:
                            return None
                        rectangles = []
                        for target in (frame, probe_frame):
                            attrs = attributes(target)
                            if not attrs or attrs.map_state != 2:
                                return None
                            px, py, child = C.c_int(), C.c_int(), C.c_ulong()
                            assert x.XTranslateCoordinates(d, target, client.root,
                                0, 0, C.byref(px), C.byref(py), C.byref(child))
                            border = attrs.border_width
                            rectangles.append((px.value - border, py.value - border,
                                attrs.width + 2 * border, attrs.height + 2 * border))
                        form_rect, probe_rect = rectangles
                        overlap = (form_rect[0] < probe_rect[0] + probe_rect[2] and
                            probe_rect[0] < form_rect[0] + form_rect[2] and
                            form_rect[1] < probe_rect[1] + probe_rect[3] and
                            probe_rect[1] < form_rect[1] + form_rect[3])
                        if overlap and order.index(frame) <= order.index(probe_frame):
                            return None
                        # A lower-layer preview can already be unobscured when
                        # its actual frame does not intersect the pixel probe.
                        return {'frame': frame, 'probe_frame': probe_frame,
                            'frame_rectangle': form_rect, 'probe_rectangle': probe_rect,
                            'overlaps_probe': overlap, 'frame_stack_index': order.index(frame),
                            'probe_stack_index': order.index(probe_frame), 'root_bottom_to_top': order}

                    def form_key(window, symbol=None):
                        witness = {'window': window,
                            'keysym': symbol.decode('ascii') if symbol else None,
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
                        raise_command = ('WindowId ' + str(window) + ' Raise\n'
                            if args.complete_bundle_preview or
                            profile == 'classic-large' and args.palette_preview else '')
                        control.send((raise_command + 'WindowId ' + str(window) + ' Focus NoWarp\n').encode())
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
                        if profile == 'classic-large' and args.palette_preview:
                            witness['capture_stacking'] = wait_for_predicate('capture Form unobscured by pixel probe',
                                lambda: above_probe(window), time.monotonic() + 5, process=core)
                        witness['status'] = 'focus-observed'
                        if symbol:
                            press_key(symbol)
                            witness['status'] = 'focused-key-submitted'

                    def defaults_editor():
                        # Defaults supplies a deterministic, independent draft
                        # differing from the committed opacity/wallpaper. Each
                        # preview key must capture the other fields unchanged.
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
                        return editor

                    def complete_editor_cases():
                        nonlocal snapshot
                        capabilities = request(resource='capabilities')
                        assert capabilities['resource_transaction_version'] == 3
                        assert capabilities['complete_bundle_preview'] is True
                        evidence['v3_capabilities'] = capabilities
                        ids = (0, 1, 2, 3, 4, 5, 6, 7, 8, 10, 11, 12,
                            13, 14, 20)
                        menu_keys = ('palette', 'appearance', 'keep_wallpaper',
                            'contrast', 'transparency', 'launcher_opacity',
                            'wallpaper_palette', 'wallpaper_appearance')
                        draft_keys = ('palette', 'appearance', 'keep_wallpaper',
                            'contrast', 'day', 'night', 'transparency',
                            'motion',
                            'launcher_opacity', 'wallpaper_palette',
                            'wallpaper_appearance')
                        witness = PreviewWitness(client)
                        cases = {}

                        def scalar_pair():
                            page = request(resource='menu-styles', limit=8)
                            values = {row['name']: row['opacity_percent']
                                for row in page['menu_styles'] if row['name'] in
                                ('*', 'SlateLauncherStyle')}
                            assert len(values) == 2
                            return values

                        def observed():
                            rows = {row[0]: row for row in records(
                                preview_updates)
                                if row[0] in ids}
                            assert set(rows) == set(ids), rows
                            root_state = witness.root_state()
                            return dict(roles=rows, root=root_state,
                                buffered10=(witness.drawable(rows[10][8])
                                    if rows[10][8] > 1 else None),
                                scalars=scalar_pair())

                        def local_phase():
                            current = time.localtime()
                            minute = current.tm_hour * 60 + current.tm_min
                            return dict(hour=current.tm_hour,
                                minute=current.tm_min,
                                phase='light' if 7 * 60 <= minute < 19 * 60
                                    else 'dark')

                        def close_forms():
                            for title, window in named_windows().items():
                                if title == 'Desktop settings':
                                    control.send(('WindowId %d Close\n' %
                                        window).encode())
                            control.send(
                                b'KillModule FvwmForm SlateNoticeForm\n')
                            barrier()
                            wait_for_predicate('v3 prior forms retired',
                                lambda: 'Desktop settings' not in (
                                    named_windows())
                                    and 'FVWM-X' not in named_windows(),
                                time.monotonic() + 8, process=core)

                        def resolved_wallpaper():
                            settings = expanded_settings(request(scope='all'))[
                                'settings']
                            name = settings['wallpaper_palette']['resolved']
                            appearance = settings[
                                'wallpaper_appearance']['resolved']
                            if appearance == 'auto':
                                appearance = local_phase()['phase']
                            assert name in palettes, name
                            assert appearance in ('light', 'dark'), appearance
                            return dict(palette=name, appearance=appearance)

                        def settle(previous=None, previous_pair=None):
                            # One overwritten receipt bounds diagnostics while
                            # retaining the actual last failed readiness gate.
                            def ready():
                                appearance = request(resource='appearance')
                                capability = appearance[
                                    'complete_bundle_preview']
                                colorsets = {identifier: request(
                                    resource='colorsets', target=str(
                                        identifier))['colorsets'][0]
                                    for identifier in ids}
                                pair = resolved_wallpaper()
                                pending = [identifier for identifier, row in
                                    colorsets.items() if row['update_state'] !=
                                    'idle']
                                diagnostic = dict(
                                    context=evidence.get('v3_case_context'),
                                    appearance=appearance,
                                    colorset20=colorsets[20],
                                    root_owned=capability['root_owned'],
                                    pending_colorsets=pending,
                                    resolved_pair=pair,
                                    previous_resolved_pair=previous_pair,
                                    persisted_wallpaper=json.loads(
                                        state.read_text()).get('wallpaper'),
                                    root_change_required=(previous is not None
                                        and pair != previous_pair))
                                diagnostic['observed_root'] = (
                                    witness.root_state(owned=False) if
                                    witness.property('_XROOTPMAP_ID') else None)
                                evidence['v3_last_settle_poll'] = diagnostic
                                if not capability['root_owned']:
                                    diagnostic['waiting_for'] = 'root_owned'
                                    return None
                                if pending:
                                    diagnostic['waiting_for'] = 'colorset_idle'
                                    return None
                                result = observed()
                                diagnostic['observed_root'] = result['root']
                                diagnostic['previous_root'] = (previous[
                                    'root'] if previous is not None else None)
                                if (diagnostic['root_change_required'] and
                                        result['root'] == previous['root']):
                                    diagnostic['waiting_for'] = 'root_change'
                                    return None
                                diagnostic['waiting_for'] = None
                                return result
                            return wait_for_predicate('v3 submitted resources',
                                ready, time.monotonic() + 12, process=core)

                        def setup(setting):
                            nonlocal snapshot
                            close_forms()
                            phase = local_phase()
                            selected_mode = mode
                            if setting == 'appearance':
                                selected_mode = ('dark' if phase['phase'] ==
                                    'light' else 'light')
                            snapshot = request(scope='all')
                            previous = observed()
                            old_pair = resolved_wallpaper()
                            change(dict(palette=palette,
                                appearance=selected_mode,
                                contrast=('high' if setting == 'contrast'
                                    else 'normal'),
                                transparency=setting != 'transparency',
                                keep_wallpaper=False, launcher_opacity=82,
                                wallpaper_palette='grove',
                                wallpaper_appearance=('light'
                                    if selected_mode == 'dark'
                                    else 'dark')))
                            change(dict(day='07:00', night='19:00'))
                            settle(previous, old_pair)
                            if setting == 'keep_wallpaper':
                                previous = observed()
                                old_pair = resolved_wallpaper()
                                change(dict(keep_wallpaper=True,
                                    wallpaper_palette='dune',
                                    wallpaper_appearance=selected_mode))
                                settle(previous, old_pair)
                            elif setting == 'appearance':
                                previous = observed()
                                old_pair = resolved_wallpaper()
                                change(dict(wallpaper_palette='follow',
                                    wallpaper_appearance='follow'))
                                settle(previous, old_pair)
                            if setting not in ('contrast', 'launcher_opacity'):
                                control.send(b'MenuStyle * Translucent 69\n'
                                    b'MenuStyle SlateLauncherStyle '
                                    b'Translucent 87\n')
                                barrier()
                            snapshot = request(scope='all')
                            effective = snapshot['settings']
                            assert effective['palette']['effective'] == palette
                            assert effective['appearance']['effective'] == (
                                selected_mode)
                            return phase, observed()

                        def native_callback(setting, since, draft_values):
                            for path in native.glob('module-*.jsonl'):
                                for line in path.read_text().splitlines(
                                        keepends=True):
                                    if not line.endswith('\n'):
                                        continue
                                    row = json.loads(line)
                                    text = row.get('text', '')
                                    prefix = ('preview-resource-editor ' +
                                        setting + ' ')
                                    if (row.get('direction') == 'in' and
                                            row['time'] >= since and
                                            text.startswith(prefix)):
                                        words = text.split()
                                        assert len(words) == (
                                            3 + len(draft_keys)), row
                                        expected = [str(
                                            draft_values[key]).lower()
                                            for key in draft_keys]
                                        assert words[3:] == expected, row
                                        return row
                            return None

                        def recovery_notice(since, reason='cancelled'):
                            assert reason in ('cancelled', 'abandoned')
                            message = ('Resource preview ' + reason +
                                '; previous resources restored.')
                            expected = ('*SlateNoticeForm: TextHex x' +
                                message.encode().hex())
                            for identity in native.glob('module-*.json'):
                                metadata = json.loads(identity.read_text())
                                if Path(metadata['real']).name != 'FvwmDesktop':
                                    continue
                                path = identity.with_suffix('.jsonl')
                                if not path.exists():
                                    continue
                                for line in path.read_text().splitlines(
                                        keepends=True):
                                    if not line.endswith('\n'):
                                        continue
                                    row = json.loads(line)
                                    if (row.get('direction') == 'out' and
                                            row['time'] >= since and
                                            row.get('text') == expected):
                                        return dict(row, decoded_notice=message,
                                            actual_desktop=metadata)
                            return None

                        def from_editor(setting, draft_values):
                            editor = defaults_editor()
                            form_key(editor)
                            if draft_values['palette'] != defaults['palette']:
                                choices = []
                                for child in children(editor):
                                    attrs = attributes(child)
                                    if (attrs and attrs.map_state == 2 and
                                            attrs.width == attrs.height):
                                        choices.append((attrs.y, attrs.x,
                                            child, attrs))
                                choices.sort(key=lambda item: item[:2])
                                palette_row = [item for item in choices
                                    if item[0] == choices[0][0]]
                                assert len(palette_row) == 4
                                selected = ('slate', 'dune', 'grove',
                                    'ink').index(
                                    draft_values['palette'])
                                _, _, child, attrs = palette_row[selected]
                                px, py = C.c_int(), C.c_int()
                                under = C.c_ulong()
                                assert x.XTranslateCoordinates(d, child,
                                    client.root, attrs.width // 2,
                                    attrs.height // 2, C.byref(px),
                                    C.byref(py), C.byref(under))
                                pointer.motion(px.value, py.value)
                                pointer.button(True)
                                pointer.button(False)
                                x.XSync(d, 0)
                                clicks = evidence.setdefault(
                                    'v3_palette_choice', [])
                                clicks.append(
                                    dict(window=editor, choice=child,
                                        value=draft_values['palette'],
                                        x=px.value, y=py.value))
                            if not evidence.get('complete_editor_capture'):
                                capture('complete-bundle-editor.png')
                                evidence['complete_editor_capture'] = True
                            started = time.monotonic()
                            form_key(editor, b'F5')
                            wait_for_predicate('actual F5 choice menu grab',
                                pointer.grabbed, time.monotonic() + 8,
                                process=core)
                            # Home selects the first actionable native menu row.
                            # The real callback receipt proves which row ran.
                            press_key(b'Home')
                            for _ in range(menu_keys.index(setting)):
                                press_key(b'Down')
                            if not evidence.get('complete_chooser_capture'):
                                mapped = []
                                for target in children(client.root):
                                    attrs = attributes(target)
                                    if (attrs and attrs.map_state == 2 and
                                            attrs.save_under and
                                            attrs.width > 20 and
                                            attrs.height > 20):
                                        mapped.append(target)
                                assert mapped and pointer.grabbed(), mapped
                                capture('complete-bundle-chooser.png')
                                evidence['complete_chooser_capture'] = dict(
                                    selected_key=setting, mapped_menus=mapped)
                            press_key(b'Return')
                            receipt = wait_for_predicate('selected callback',
                                lambda: native_callback(setting, started,
                                    draft_values),
                                time.monotonic() + 8, process=core)
                            title = ('Preview menu opacity' if setting ==
                                'launcher_opacity' else 'Preview ' + setting)
                            window = wait_for_predicate('actual ' + title,
                                lambda: named_windows().get(title),
                                time.monotonic() + 12, process=core)
                            form_key(window)
                            if (setting == 'wallpaper_palette' and not
                                    evidence.get('wallpaper_preview_capture')):
                                capture('complete-wallpaper-preview.png')
                                evidence['wallpaper_preview_capture'] = dict(
                                    selected_key=setting, title=title,
                                    window=window)
                            return window, title, receipt

                        metadata = expanded_settings(request(scope='all'))
                        assert {key for key, row in metadata['settings'].items()
                            if row['native_preview']} == set(menu_keys)
                        for setting in menu_keys:
                            for ending in ('cancel', 'save'):
                                evidence['v3_case_context'] = dict(
                                    setting=setting, ending=ending,
                                    stage='setup')
                                phase, original = setup(setting)
                                draft_values = dict(defaults)
                                if setting == 'palette' and palette == 'slate':
                                    draft_values['palette'] = 'dune'
                                before = state.read_bytes()
                                baseline = json.loads(before)
                                window, title, callback = from_editor(
                                    setting, draft_values)
                                candidate = observed()
                                assert candidate != original, (
                                    setting, candidate)
                                assert state.read_bytes() == before
                                if setting in ('palette', 'appearance',
                                        'keep_wallpaper', 'wallpaper_palette',
                                        'wallpaper_appearance'):
                                    assert candidate['scalars'] == (
                                        original['scalars'])
                                symbol = (b'Escape' if ending == 'cancel' else
                                    b'F2' if setting == 'launcher_opacity'
                                    else b'Return')
                                form_key(window, symbol)
                                wait_for_predicate('v3 preview Form retired',
                                    lambda: title not in named_windows(),
                                    time.monotonic() + 8, process=core)
                                if ending == 'cancel':
                                    wait_for_predicate('exact v3 rollback',
                                        lambda: observed() == original,
                                        time.monotonic() + 10, process=core)
                                    assert state.read_bytes() == before
                                else:
                                    def saved_one():
                                        actual = json.loads(state.read_text())
                                        expected = dict(baseline,
                                            **{setting: draft_values[setting]})
                                        actual.pop('wallpaper', None)
                                        expected.pop('wallpaper', None)
                                        return actual == expected
                                    wait_for_predicate('preference saved',
                                        saved_one, time.monotonic() + 8,
                                        process=core)
                                    assert observed() == candidate
                                returned = wait_for_predicate('draft returned',
                                    lambda: named_windows().get(
                                        'Desktop settings'),
                                    time.monotonic() + 8, process=core)
                                before_apply = observed()
                                before_apply_pair = resolved_wallpaper()
                                evidence['v3_case_context']['stage'] = (
                                    'apply-returned-draft')
                                form_key(returned, b'F3')
                                def applied_draft():
                                    value = request(scope='all')
                                    return (value if all(value['settings'][key][
                                        'saved'] == wanted for key, wanted in
                                        draft_values.items()) else None)
                                snapshot = wait_for_predicate('draft values',
                                    applied_draft, time.monotonic() + 10,
                                    process=core)
                                settle(before_apply, before_apply_pair)
                                end_phase = local_phase()
                                assert phase['phase'] == end_phase['phase'], (
                                    'schedule crossed during preview',
                                    phase, end_phase)
                                cases[setting + '-' + ending] = dict(
                                    callback=callback, original=original,
                                    candidate=candidate,
                                    selected_value=draft_values[setting],
                                    saved_baseline=baseline,
                                    phase_before=phase, phase_after=end_phase,
                                    draft_preserved=True,
                                    ordinary_save_available=True)

                        # Defaults exercises high->normal and false->true.
                        # Observe the opposite directions through the same
                        # native controller command and confirmation consumer.
                        for setting, value in (('contrast', 'high'),
                                ('transparency', False)):
                            close_forms()
                            snapshot = request(scope='all')
                            before = state.read_bytes()
                            original = settle()
                            text = str(value).lower()
                            control.send(('SendToModule FvwmDesktop '
                                'preview-resource ' + snapshot['provider'] +
                                ' ' + setting + ' ' + text + '\n').encode())
                            title = 'Preview ' + setting
                            window = wait_for_predicate('opposite preview',
                                lambda: named_windows().get(title),
                                time.monotonic() + 12, process=core)
                            form_key(window)
                            candidate = observed()
                            assert candidate != original
                            if setting == 'contrast':
                                assert candidate['scalars'] == {
                                    '*': 100, 'SlateLauncherStyle': 100}
                            else:
                                assert candidate['buffered10'] is None
                            cancel_started = time.monotonic()
                            form_key(window, b'Escape')
                            wait_for_predicate('opposite exact rollback',
                                lambda: observed() == original,
                                time.monotonic() + 10, process=core)
                            # The controller clears its pending transaction
                            # only after consuming terminal recovery. Pixels
                            # alone can precede that actual consumer callback.
                            completed = wait_for_predicate(
                                'Desktop consumed terminal recovery',
                                lambda: recovery_notice(cancel_started),
                                time.monotonic() + 10, process=core)
                            assert state.read_bytes() == before
                            cases['opposite-' + setting] = dict(
                                original=original, candidate=candidate,
                                terminal_consumer=completed)

                        close_forms()
                        snapshot = request(scope='all')
                        before, original = state.read_bytes(), settle()
                        control.send(('SendToModule FvwmDesktop '
                            'preview-resource ' + snapshot['provider'] +
                            ' keep_wallpaper true\n').encode())
                        wait_for_predicate('unchanged projection refusal',
                            lambda: named_windows().get('FVWM-X'),
                            time.monotonic() + 8, process=core)
                        barrier()
                        assert not any(title.startswith('Preview ')
                            for title in named_windows())
                        assert observed() == original
                        assert state.read_bytes() == before
                        change({'keep_wallpaper': True})
                        assert json.loads(
                            state.read_text())['keep_wallpaper'] is True
                        saved_visible = settle()
                        assert saved_visible['root'] == original['root']
                        assert saved_visible['scalars'] == original['scalars']
                        assert all(saved_visible['roles'][identifier][1:5] ==
                            original['roles'][identifier][1:5]
                            for identifier in ids)
                        assert saved_visible['buffered10']['pixels_sha256'] == (
                            original['buffered10']['pixels_sha256'])
                        cases['retain-current-refusal-ordinary-save'] = dict(
                            original=original, ordinary_save=True,
                            reason='no changed visible projection')

                        close_forms()
                        snapshot = request(scope='all')
                        change({'keep_wallpaper': False})
                        original = settle()
                        snapshot = request(scope='all')
                        before = state.read_bytes()
                        control.send(('SendToModule FvwmDesktop '
                            'preview-resource ' + snapshot['provider'] +
                            ' palette dune\n').encode())
                        window = wait_for_predicate('file-race preview active',
                            lambda: named_windows().get('Preview palette'),
                            time.monotonic() + 12, process=core)
                        form_key(window)
                        external = dict(json.loads(before), motion=True)
                        external_bytes = (json.dumps(
                            external, indent=3) + '\n').encode()
                        state.write_bytes(external_bytes)
                        abandon_started = time.monotonic()
                        form_key(window, b'Return')
                        wait_for_predicate('external-file runtime recovery',
                            lambda: observed() == original,
                            time.monotonic() + 10, process=core)
                        abandoned = wait_for_predicate(
                            'Desktop consumed file-race abandonment',
                            lambda: recovery_notice(abandon_started,
                                'abandoned'),
                            time.monotonic() + 10, process=core)
                        assert state.read_bytes() == external_bytes
                        snapshot = request(scope='all')
                        snapshot = request('action', operation='reload',
                            provider=snapshot['provider'],
                            generation=snapshot['generation'])
                        barrier()
                        cases['external-file-change-preserved'] = dict(
                            original=original, terminal_consumer=abandoned,
                            external_sha256=hashlib.sha256(
                                external_bytes).hexdigest())

                        # The controller is an actual proxied native module.
                        # Its death must retire its lease, preserving the core
                        # root even when Backer and its source are unrelated.
                        close_forms()
                        snapshot = request(scope='all')
                        change({'keep_wallpaper': False})
                        original = settle()
                        snapshot = request(scope='all')
                        control.send(('SendToModule FvwmDesktop '
                            'preview-resource ' + snapshot['provider'] +
                            ' palette dune\n').encode())
                        window = wait_for_predicate('controller-loss preview',
                            lambda: named_windows().get('Preview palette'),
                            time.monotonic() + 12, process=core)
                        form_key(window)
                        module = max((json.loads(path.read_text()) for path in
                            native.glob('module-*.json') if json.loads(
                                path.read_text())['real'] == str(
                                    build / 'bin/FvwmPrompt/FvwmDesktop')),
                            key=lambda row: row['started'])
                        identity = child_identity(
                            module['child'], module['proxy'])
                        assert identity
                        control.send(b'KillModule FvwmDesktop\n')
                        waited_path = native / (
                            'stopped-%d.json' % module['proxy'])
                        waited = wait_for_predicate('exact Desktop native Wait',
                            lambda: json.loads(waited_path.read_text())
                                if waited_path.exists() else None,
                            time.monotonic() + 8, process=core)
                        assert waited['child'] == module['child']
                        assert waited['returncode'] is not None
                        wait_for_predicate('controller-loss exact rollback',
                            lambda: observed() == original,
                            time.monotonic() + 10, process=core)
                        old_provider = snapshot['provider']
                        control.send(
                            b'ModuleSynchronous Timeout 15 FvwmDesktop\n')
                        def restarted():
                            nonlocal serial
                            serial += 1
                            token = 'v3-controller-restart-%d' % serial
                            rpc.send(frame(token,
                                resource='settings', scope='all'))
                            message = rpc.record()['reply']
                            assert message['id'] == token
                            if message['status'] != 'ok':
                                assert message['error']['code'] == (
                                    'not_ready'), message
                                return None
                            value = message['result']
                            return (value if value['provider'] != old_provider
                                else None)
                        snapshot = wait_for_predicate('replacement Desktop',
                            restarted, time.monotonic() + 15, process=core)
                        barrier()
                        cases['actual-Desktop-loss'] = dict(
                            identity=identity, wait=waited,
                            replacement_provider=snapshot['provider'],
                            exact_original_restored=True,
                            old_confirmation_still_mapped=(
                                named_windows().get(
                                    'Preview palette') == window))
                        if named_windows().get('Preview palette') == window:
                            control.send((
                                'WindowId %d Close\n' % window).encode())

                        close_forms()
                        snapshot = request(scope='all')
                        restore_baseline()
                        settle()
                        evidence['complete_bundle_editor'] = cases

                    if args.complete_bundle_preview:
                        complete_editor_cases()
                        return

                    def preview_from_editor():
                        editor = defaults_editor()
                        form_key(editor, b'F5')
                        return wait_for_predicate('actual high contrast Save Cancel Form',
                            lambda: named_windows().get('Preview high contrast'),
                            time.monotonic() + 10, process=core)

                    def palette_appearance_cases():
                        nonlocal snapshot
                        observed_cases = {}
                        role_ids = (0, 1, 2, 3, 4, 5, 6, 7, 8, 10, 11, 12, 13, 14)

                        def close_notice():
                            control.send(b'KillModule FvwmForm SlateNoticeForm\n')
                            barrier()
                            wait_for_predicate('preview notice retired',
                                lambda: 'FVWM-X' not in named_windows(),
                                time.monotonic() + 5, process=core)

                        def scalars():
                            page = request(resource='menu-styles', limit=8)
                            values = {item['name']: item['opacity_percent']
                                for item in page['menu_styles']
                                if item['name'] in ('*', 'SlateLauncherStyle')}
                            assert set(values) == {'*', 'SlateLauncherStyle'}, page
                            return values

                        def published_roles():
                            return {identifier: request(resource='colorsets', target=str(identifier))[
                                'colorsets'][0] for identifier in role_ids}

                        def wallpaper_witness():
                            owner = wallpaper_owner()
                            image = x.XGetImage(d, owner, 0, 0, 2, 2,
                                C.c_ulong(-1).value, 2)
                            assert image
                            try:
                                samples = [x.XGetPixel(image, px, py)
                                    for py in range(2) for px in range(2)]
                            finally:
                                x.XDestroyImage(image)
                            return {'pixmap': owner, 'pixels': samples}

                        def presentation(name, appearance, contrast):
                            surface = (0 if appearance == 'dark' else 0xffffff) if contrast == 'high' else int(
                                palettes[name][appearance]['surface'][1:], 16)
                            text = (0xffffff if appearance == 'dark' else 0) if contrast == 'high' else int(
                                palettes[name][appearance]['text'][1:], 16)
                            wait_for_predicate('actual opaque panel presentation',
                                lambda: pixel(5, 5) == surface,
                                time.monotonic() + 10, process=core)
                            if profile == 'classic-large':
                                assert named_windows().get('Theme contrast probe') == probe
                                visibility = {'profile': profile, 'client': probe,
                                    'command': 'WindowId ' + str(probe) + ' Raise'}
                                evidence.setdefault('probe_visibility', []).append(visibility)
                                control.send((visibility['command'] + '\n').encode())
                                barrier()
                                def bounds(window):
                                    attrs = attributes(window)
                                    if not attrs or attrs.map_state != 2:
                                        return None
                                    px, py, child = C.c_int(), C.c_int(), C.c_ulong()
                                    assert x.XTranslateCoordinates(d, window, client.root,
                                        0, 0, C.byref(px), C.byref(py), C.byref(child))
                                    border = attrs.border_width
                                    return (px.value - border, py.value - border,
                                        attrs.width + 2 * border, attrs.height + 2 * border)
                                def visible():
                                    frame = managed_frame(probe)
                                    order = children(client.root)
                                    rectangle = bounds(frame) if frame else None
                                    if frame not in order or rectangle is None:
                                        return None
                                    overlaps = []
                                    for title, window in named_windows().items():
                                        if title != 'Desktop settings' and title != 'FVWM-X' and not title.startswith('Preview '):
                                            continue
                                        other = managed_frame(window)
                                        rect = bounds(other) if other else None
                                        if other not in order or rect is None:
                                            continue
                                        if (rectangle[0] < rect[0] + rect[2] and rect[0] < rectangle[0] + rectangle[2]
                                                and rectangle[1] < rect[1] + rect[3] and rect[1] < rectangle[1] + rectangle[3]):
                                            overlaps.append({'title': title, 'client': window,
                                                'frame': other, 'rectangle': rect, 'stack_index': order.index(other)})
                                    visibility.update(frame=frame, rectangle=rectangle,
                                        root_bottom_to_top=order, overlapping_forms=overlaps)
                                    return visibility if all(order.index(frame) > item['stack_index'] for item in overlaps) else None
                                wait_for_predicate('same probe exposed above overlapping Forms', visible,
                                    time.monotonic() + 5, process=core)
                            return wait_for_predicate('actual existing Form presentation',
                                lambda: monochrome(probe, surface, text),
                                time.monotonic() + 10, process=core)

                        def setup(name='slate', appearance='dark', contrast='normal',
                                transparency=False, follow=False):
                            nonlocal snapshot
                            close_notice()
                            old_wall = wallpaper_witness()
                            old_pair = json.loads(state.read_text())['wallpaper']
                            # Native Save/Apply can advance the provider between
                            # cases. Observe its current generation for this new
                            # intentional action; never retry a stale action.
                            snapshot = request(scope='all')
                            change({'palette': name, 'appearance': appearance,
                                'contrast': contrast, 'transparency': transparency,
                                'keep_wallpaper': False, 'launcher_opacity': 82,
                                'wallpaper_palette': 'follow' if follow else 'grove',
                                'wallpaper_appearance': 'follow' if follow else 'light'})
                            def settled():
                                return all(request(resource='colorsets', target=str(identifier))[
                                    'colorsets'][0]['update_state'] == 'idle' for identifier in role_ids)
                            wait_for_predicate('all fourteen submitted roles settled', settled,
                                time.monotonic() + 10, process=core)
                            new_pair = json.loads(state.read_text())['wallpaper']
                            if new_pair != old_pair:
                                wait_for_predicate('actual setup wallpaper change',
                                    lambda: wallpaper_witness() != old_wall,
                                    time.monotonic() + 10, process=core)
                                wallpaper()
                            if not transparency or contrast == 'high':
                                presentation(name, appearance, contrast)
                            if contrast == 'normal' and not transparency:
                                # These are actual external menu scalars, distinct
                                # from both requested opacity and resolved policy.
                                control.send(b'MenuStyle * Translucent 69\n'
                                    b'MenuStyle SlateLauncherStyle Translucent 87\n')
                                barrier()
                                assert scalars() == {'*': 69, 'SlateLauncherStyle': 87}
                            snapshot = request(scope='all')

                        def start_native(setting, value):
                            nonlocal snapshot
                            close_notice()
                            snapshot = request(scope='all')
                            control.send(('SendToModule FvwmDesktop preview-' + setting + ' ' +
                                snapshot['provider'] + ' ' + value + '\n').encode())
                            return wait_for_predicate('native ' + setting + ' preview Form',
                                lambda: named_windows().get('Preview ' + setting),
                                time.monotonic() + 10, process=core)

                        def finish(window, setting, value, ending, before, wall,
                                original_scalars, name, appearance, contrast, label, editor_expected=False):
                            assert state.read_bytes() == before
                            assert wallpaper_witness() == wall and scalars() == original_scalars
                            painted = presentation(name, appearance, contrast)
                            if profile == 'classic-large':
                                form_key(window)
                            capture(label + '-active.png')
                            form_key(window, b'Return' if ending == 'save' else b'Escape')
                            wait_for_predicate('native ' + setting + ' preview retired',
                                lambda: 'Preview ' + setting not in named_windows(),
                                time.monotonic() + 10, process=core)
                            capture_form = None
                            if profile == 'classic-large':
                                title = 'Desktop settings' if editor_expected else 'FVWM-X'
                                capture_form = wait_for_predicate('existing returned capture Form',
                                    lambda: named_windows().get(title),
                                    time.monotonic() + 10, process=core)
                                wait_for_predicate('returned capture Form managed',
                                    lambda: managed_frame(capture_form),
                                    time.monotonic() + 10, process=core)
                            if ending == 'save':
                                expected = dict(json.loads(before), **{setting: value})
                                wait_for_predicate('only ' + setting + ' preference saved',
                                    lambda: json.loads(state.read_text()) == expected,
                                    time.monotonic() + 5, process=core)
                                capability = request(resource='appearance')['opaque_colors_preview']
                                assert capability['slot_available'], capability
                                presentation(name, appearance, contrast)
                            else:
                                assert state.read_bytes() == before
                                baseline = json.loads(before)
                                presentation(baseline['palette'], baseline['appearance'], baseline['contrast'])
                            assert wallpaper_witness() == wall and scalars() == original_scalars
                            if capture_form:
                                if editor_expected:
                                    form_key(capture_form)
                                else:
                                    # Notices are deliberately NeverFocus.
                                    control.send(('WindowId ' + str(capture_form) + ' Raise\n').encode())
                                    barrier()
                                    wait_for_predicate('capture notice unobscured by pixel probe',
                                        lambda: above_probe(capture_form), time.monotonic() + 5, process=core)
                            capture(label + '-finished.png')
                            observed_cases[label] = {'setting': setting, 'candidate': value,
                                'ending': ending, 'actual_presentation': painted,
                                'unchanged_wallpaper': wall, 'unchanged_menu_scalars': original_scalars}

                        def choose_appearance(editor, value):
                            form_key(editor)
                            rows = {}
                            for child in children(editor):
                                attrs = attributes(child)
                                if attrs and attrs.map_state == 2 and attrs.width == attrs.height:
                                    rows.setdefault(attrs.y, []).append((attrs.x, child, attrs))
                            choices = [sorted(items) for _, items in sorted(rows.items())]
                            evidence.setdefault('appearance_choice_rows', []).append([
                                [{'window': child, 'x': attrs.x, 'y': attrs.y,
                                    'width': attrs.width, 'height': attrs.height}
                                    for _, child, attrs in items] for items in choices])
                            # Public editor ordering: four palette choices,
                            # then Light/Dark/Auto. Labels are drawn, while each
                            # real Choice checkbox owns a square X child.
                            assert len(choices) >= 2 and len(choices[0]) == 4 and len(choices[1]) == 3, choices
                            _, child, attrs = choices[1][{'light': 0, 'dark': 1, 'auto': 2}[value]]
                            px, py, under = C.c_int(), C.c_int(), C.c_ulong()
                            assert x.XTranslateCoordinates(d, child, client.root,
                                attrs.width // 2, attrs.height // 2,
                                C.byref(px), C.byref(py), C.byref(under))
                            pointer.motion(px.value, py.value)
                            root_, actual_child = C.c_ulong(), C.c_ulong()
                            rx, ry, wx, wy = (C.c_int() for _ in range(4))
                            mask = C.c_uint()
                            assert x.XQueryPointer(d, editor, C.byref(root_), C.byref(actual_child),
                                C.byref(rx), C.byref(ry), C.byref(wx), C.byref(wy),
                                C.byref(mask))
                            assert actual_child.value == child, (child, actual_child.value)
                            pointer.button(True)
                            pointer.button(False)
                            x.XSync(d, 0)
                            evidence.setdefault('appearance_choice_clicks', []).append({
                                'window': editor, 'choice_window': child, 'value': value,
                                'root_x': px.value, 'root_y': py.value})

                        def apply_returned_draft(wanted):
                            nonlocal snapshot
                            editor = wait_for_predicate('whole captured draft returned',
                                lambda: named_windows().get('Desktop settings'),
                                time.monotonic() + 10, process=core)
                            close_notice()
                            old_wall = wallpaper_witness()
                            old_pair = json.loads(state.read_text())['wallpaper']
                            form_key(editor, b'F3')
                            def applied():
                                observed = request(scope='all')
                                actual = {key_: item['saved'] for key_, item in observed['settings'].items()}
                                return observed if actual == wanted else None
                            snapshot = wait_for_predicate('all eleven captured draft fields applied', applied,
                                time.monotonic() + 10, process=core)
                            if json.loads(state.read_text())['wallpaper'] != old_pair:
                                # Ordinary Apply may intentionally change the
                                # captured follow-wallpaper draft. Observe that
                                # actual publication before the next setup;
                                # preference persistence is not paint completion.
                                wait_for_predicate('actual wallpaper from ordinary draft Apply',
                                    lambda: wallpaper_witness() != old_wall,
                                    time.monotonic() + 10, process=core)
                                wallpaper()

                        # A changed committed palette permits Defaults' Slate
                        # draft while all ten other default draft fields differ
                        # from, or remain independent of, the running candidate.
                        for contrast in ('normal', 'high'):
                            setup(name='dune', appearance=mode, contrast=contrast,
                                transparency=contrast == 'high')
                            before, wall, actual_scalars = state.read_bytes(), wallpaper_witness(), scalars()
                            editor = defaults_editor()
                            form_key(editor, b'F6')
                            preview = wait_for_predicate('real F6 palette preview',
                                lambda: named_windows().get('Preview palette'),
                                time.monotonic() + 10, process=core)
                            finish(preview, 'palette', 'slate', 'save', before, wall, actual_scalars,
                                'slate', mode, contrast, 'palette-f6-whole-draft-' + contrast,
                                editor_expected=True)
                            apply_returned_draft(dict(defaults))

                        for appearance, ending in (('light', 'cancel'), ('dark', 'save')):
                            baseline_mode = 'dark' if appearance == 'light' else 'light'
                            setup(appearance=baseline_mode)
                            before, wall, actual_scalars = state.read_bytes(), wallpaper_witness(), scalars()
                            editor = defaults_editor()
                            choose_appearance(editor, appearance)
                            form_key(editor, b'F7')
                            preview = wait_for_predicate('real F7 appearance preview',
                                lambda: named_windows().get('Preview appearance'),
                                time.monotonic() + 10, process=core)
                            label = 'appearance-f7-' + appearance + '-' + ending
                            finish(preview, 'appearance', appearance, ending, before, wall, actual_scalars,
                                'slate', appearance, 'normal', label, editor_expected=True)
                            if ending == 'cancel':
                                presentation('slate', baseline_mode, 'normal')
                            apply_returned_draft(dict(defaults, appearance=appearance))

                        # Exercise all four palette values and both appearance
                        # directions through the public native menu commands.
                        for name, ending in (('dune', 'save'), ('grove', 'cancel')):
                            setup()
                            before, wall, actual_scalars = state.read_bytes(), wallpaper_witness(), scalars()
                            preview = start_native('palette', name)
                            finish(preview, 'palette', name, ending, before, wall, actual_scalars,
                                name, 'dark', 'normal', 'palette-native-' + name)
                            if ending == 'cancel':
                                presentation('slate', 'dark', 'normal')
                        setup(contrast='high', transparency=True)
                        before, wall, actual_scalars = state.read_bytes(), wallpaper_witness(), scalars()
                        preview = start_native('palette', 'ink')
                        finish(preview, 'palette', 'ink', 'save', before, wall, actual_scalars,
                            'ink', 'dark', 'high', 'palette-native-high-ink')
                        for appearance, ending in (('light', 'save'), ('dark', 'cancel')):
                            before, wall, actual_scalars = state.read_bytes(), wallpaper_witness(), scalars()
                            preview = start_native('appearance', appearance)
                            finish(preview, 'appearance', appearance, ending, before, wall, actual_scalars,
                                'ink', appearance, 'high', 'appearance-native-high-' + appearance)
                            if ending == 'cancel':
                                presentation('ink', 'light', 'high')

                        for refusal in ('transparent', 'follow-wallpaper', 'auto', 'no-change'):
                            setup(transparency=refusal == 'transparent', follow=refusal == 'follow-wallpaper')
                            before, wall, actual_scalars = state.read_bytes(), wallpaper_witness(), scalars()
                            before_roles = published_roles()
                            setting = 'appearance' if refusal == 'auto' else 'palette'
                            value = 'auto' if refusal == 'auto' else 'slate' if refusal == 'no-change' else 'dune'
                            snapshot = request(scope='all')
                            control.send(('SendToModule FvwmDesktop preview-' + setting + ' ' +
                                snapshot['provider'] + ' ' + value + '\n').encode())
                            wait_for_predicate('explicit native refusal notice',
                                lambda: named_windows().get('FVWM-X'),
                                time.monotonic() + 10, process=core)
                            barrier()
                            assert not any('Preview ' + key_ in named_windows()
                                for key_ in ('palette', 'appearance'))
                            assert state.read_bytes() == before and wallpaper_witness() == wall
                            assert scalars() == actual_scalars
                            assert published_roles() == before_roles
                            capture('opaque-refused-' + refusal + '.png')
                            observed_cases['refusal-' + refusal] = {
                                'unchanged_published_roles_sha256': hashlib.sha256(json.dumps(
                                    before_roles, sort_keys=True).encode()).hexdigest(),
                                'unchanged_wallpaper': wall, 'unchanged_menu_scalars': actual_scalars,
                                'file_unchanged': True}

                        setup()
                        before, wall, actual_scalars = state.read_bytes(), wallpaper_witness(), scalars()
                        preview = start_native('palette', 'dune')
                        external = dict(json.loads(before), motion=True)
                        external_bytes = (json.dumps(external, indent=3) + '\n').encode()
                        state.write_bytes(external_bytes)
                        form_key(preview, b'Return')
                        wait_for_predicate('opaque external-file edit recovery',
                            lambda: 'Preview palette' not in named_windows(),
                            time.monotonic() + 10, process=core)
                        presentation('slate', 'dark', 'normal')
                        assert state.read_bytes() == external_bytes and scalars() == actual_scalars
                        assert wallpaper_witness() == wall
                        capture('opaque-external-file-edit-recovered.png')
                        observed_cases['external-file-edit'] = 'external-bytes-preserved-and-old-resources-restored'
                        snapshot = request(scope='all')
                        snapshot = request('action', operation='reload', provider=snapshot['provider'],
                            generation=snapshot['generation'])
                        barrier()
                        snapshot = request(scope='all')
                        restore_baseline()
                        close_notice()
                        wait_for_predicate('original tinted profile recovered after opaque cases',
                            lambda: pixel(5, 5) == normal_bar and opacity() == 82,
                            time.monotonic() + 10, process=core)
                        assert wallpaper() == independent
                        evidence['palette_appearance_preview'] = observed_cases

                    if args.palette_preview:
                        palette_appearance_cases()
                        if not args.contrast_preview:
                            return

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
                if (args.contrast_preview or args.palette_preview or
                        args.complete_bundle_preview):
                    resource_preview_cases(probe, normal_bar, independent)
                    if args.complete_bundle_preview:
                        return
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
                    assert raw['schema_version'] == 9 and raw['contrast'] == 'high'
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
                assert json.loads(state.read_text())['schema_version'] == 9
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
                        observed = expanded_settings(response['result'])
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
            assert evidence['source_sha256'] == hashes(source, inputs), (
                'source changed during run')
            assert all(binary_hash(item['path']) == item for item in
                evidence['binary_inputs'].values()), 'binary changed during run'
            assert not errors.unexpected, errors.unexpected
            evidence['status'] = 'passed'
        except BaseException as failure:
            evidence.update(status='failed', failure=repr(failure))
            raise
        finally:
            def clean(label, operation):
                try:
                    return operation()
                except BaseException as failure:
                    evidence['status'] = 'failed'
                    evidence.setdefault('cleanup_failures', []).append(dict(
                        step=label, error=repr(failure)))
            for connection in (control, rpc):
                if connection:
                    clean('connection', connection.close)
            if core:
                clean('core', lambda: core.close(time.monotonic() + 6))
                evidence['core_wait'] = dict(returncode=core.process.returncode)
            if native:
                evidence['module_waits'] = clean('actual module Wait receipts',
                    lambda: cleanup_receipts(native))
                clean('module records', lambda:
                    shutil.copytree(native, out / 'native-records'))
            if errors:
                clean('X observer', errors.close_display)
            elif client:
                clean('X client', client.close)
            for log in runtime.root.glob('*.log'):
                clean('copy ' + log.name,
                    lambda p=log: shutil.copy2(p, out / p.name))
            if evidence.get('observer_unexpected_x_errors'):
                evidence['status'] = 'failed'
            (out / 'evidence.json').write_text(json.dumps(evidence, indent=2) + '\n')
            (out / 'capture-manifest.json').write_text(json.dumps({
                'profile': profile,
                'configuration_sha256': evidence.get('configuration_sha256'),
                'palettes_sha256': evidence.get('palettes_sha256'),
                'captures': evidence.get('captures', []),
                'inspection': 'pending human/lead image inspection'}, indent=2) + '\n')
            if evidence['status'] != 'passed' and sys.exc_info()[0] is None:
                raise AssertionError(evidence.get('cleanup_failures', evidence))
    assert not evidence['observer_unexpected_x_errors'], evidence


if __name__ == '__main__':
    if len(sys.argv) > 1 and sys.argv[1] == '--module-proxy':
        raise SystemExit(module_proxy(sys.argv[2], sys.argv[3], sys.argv[4:]))
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--artifacts-dir', type=Path, required=True)
    parser.add_argument('--execution-profile', default='unspecified',
        help='Record native/emulated execution separately from passing checks')
    parser.add_argument('--complete-bundle-preview', action='store_true',
        help='Exercise the actual F5 eight-key editor and v3 root bundles')
    parser.add_argument('--probe', type=Path,
        help='Actual image-owner-probe native subscriber for v3 root witnesses')
    parser.add_argument('--worker', type=Path,
        help='Actual private background/image worker; default build-tree helper')
    parser.add_argument('--contrast', action='store_true',
        help='Run high contrast, native Form/menu pixels and opacity recovery cases')
    parser.add_argument('--contrast-preview', action='store_true',
        help='Also run native resource Save/Cancel and captured editor draft cases')
    parser.add_argument('--palette-preview', action='store_true',
        help='Also run native opaque palette/appearance previews and whole-draft capture')
    parser.add_argument('--profiles', nargs='+', default=['slate-dark', 'dune-light', 'classic-large'],
        choices=['slate-dark', 'dune-light', 'classic-large'])
    options = parser.parse_args()
    if options.complete_bundle_preview:
        if options.execution_profile == 'unspecified':
            parser.error('v3 requires an explicit --execution-profile label')
        options.probe = (options.probe or
            options.build / 'tests/image-owner-probe').resolve()
        if not options.probe.is_file() or not os.access(options.probe, os.X_OK):
            parser.error('v3 requires the actual --probe native subscriber')
        options.worker = (options.worker or
            options.build / 'bin/fvwm-image-svg-worker').resolve()
        if not options.worker.is_file() or not os.access(options.worker, os.X_OK):
            parser.error('v3 requires the actual --worker image helper')
    if (options.contrast_preview or options.palette_preview or
            options.complete_bundle_preview):
        options.contrast = True
    for selected in options.profiles:
        run(options, selected)
