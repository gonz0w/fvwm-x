#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
"""Observe complete frozen browser values using real X11 and native Forms.

Transparent native proxies retain original commands and actual Form callbacks.
No module packets, worker data or metadata replies are synthesized. Runs the
three representative themes; optional real RandR DPI selects one focused scale.
--startup-workarea uses stable initial core Monitor reservations. Optional
secondary-monitor and oversized cases use genuine RandR/EwmhBaseStruts inputs;
malformed metadata and atomic strut/hotplug freshness are outside native scope.
"""
import argparse
import base64
from collections import Counter
import ctypes as C
import hashlib
import json
import os
import re
from pathlib import Path
import shlex
import shutil
import struct
import sys
import tempfile
import time
import traceback

from desktop_live import connect_client
from geometry_transition_live import SizeHints
from legacy_property_live import ClientMessage
from inspector_live import NativeView, atomic_json
from live_x_harness import (HarnessRuntime, start_display_server, start_xorg_dummy,
    wait_for_randr, run_required_xrandr, wait_for_authoritative_state, wait_for_predicate)
from mfl_request_live import frame
from mfl_stream_live import Connection
from native_bindings_live import module_proxy, rows
from browser_continuity_live import cleanup_receipts
from native_capture import NativeCapture
from selection_live import Event
from session_x_errors import SessionXErrors

PROFILES = ('slate-dark', 'dune-light', 'classic-large')
ENTRIES = 128


class ClassHint(C.Structure):
    _fields_ = [('name', C.c_void_p), ('class_', C.c_void_p)]



def run(args, profile, run_root):
    source, build = Path(__file__).resolve().parents[2], args.build.resolve()
    out = run_root / profile
    out.mkdir()
    inputs = ['tests/live_x/browser_disclosure_live.py', 'tests/live_x/native_bindings_live.py', 'bin/FvwmPrompt/cmd/fvwmfilebrowser/details.go',
        'modules/FvwmForm/FvwmForm.c', 'modules/FvwmForm/ReadXServer.c',
        'modules/FvwmForm/form_list.c', 'modules/FvwmForm/form_scale.c',
        'modules/FvwmForm/form_scale_policy.c', 'modules/FvwmForm/form_scale.h',
        'modules/FvwmForm/form_workarea.h',
        'libs/module_monitor.c', 'libs/module_ui_scale.c',
        'bin/FvwmPrompt/cmd/fvwmfilebrowser/native.go',
        'bin/FvwmPrompt/cmd/fvwmfilebrowser/view.go']
    evidence = dict(status='running', profile=profile, screenshots=[], failures=[], cleanup_failures=[],
        source_sha256={name: hashlib.sha256((source / name).read_bytes()).hexdigest() for name in inputs},
        binary_sha256={str(p.relative_to(build)): hashlib.sha256(p.read_bytes()).hexdigest()
            for p in (build / 'fvwm3', build / 'modules/FvwmForm/FvwmForm',
                      build / 'bin/FvwmPrompt/fvwm-file-browser',
                      build / 'bin/fvwm-desktop-capture')},
        operations=[], requested_scale=args.scale, startup_continuity=args.startup_continuity,
        editable_continuity_only=args.editable_continuity_only,
        startup_workarea=args.startup_workarea, startup_monitor=args.startup_monitor,
        startup_oversized=args.startup_oversized,
        limits={'entries': ENTRIES, 'native_children': 64,
                'physical_display_or_assistive_technology_qualified': False,
                'automatic_module_scaling_qualified': bool(args.xorg_config)}, display_geometry='1920x1800x24')
    with HarnessRuntime(prefix='fvwm-browser-disclosure-') as runtime:
        client = errors = core = browser = control = rpc_peer = None
        try:
            if args.xorg_config:
                # Reuse the accepted viewport-scale dummy-server mode and
                # millimetre policy; this driver has no fault/scale matrix.
                original = args.xorg_config.resolve().read_text()
                original = original.replace('    Modeline "1920x1080"',
                    '    Modeline "2560x2160" 225.00 2560 2720 2992 3424 2160 2163 2173 2190\n'
                    '    Modeline "1920x1080"')
                original = original.replace('Modes "1920x1080"', 'Modes "2560x2160" "1920x1080"')
                xorg = runtime.root / 'xorg.conf'
                xorg.write_text(original)
                _, display = start_xorg_dummy(runtime, xorg, time.monotonic() + 10)
                wait_for_randr(runtime, display, time.monotonic() + 10)
                width_mm, height_mm = {'1': (677, 572), '1.25': (542, 458),
                    '2': (339, 286)}[args.scale]
                run_required_xrandr(runtime, display, ['--output', 'DUMMY0',
                    '--mode', '2560x2160', '--pos', '0x0', '--primary',
                    '--set', 'WIDTH_MM', str(width_mm), '--set', 'HEIGHT_MM', str(height_mm)],
                    'disclosure-real-randr-scale', evidence['operations'])
                evidence['randr'] = wait_for_authoritative_state(runtime, display,
                    'disclosure monitor millimetres', lambda v:
                        v['monitors'].get('DUMMY0', {}).get('mm_width') == width_mm and
                        v['monitors'].get('DUMMY0', {}).get('mm_height') == height_mm,
                    time.monotonic() + 6, 'disclosure-scale')
                evidence['display_geometry'] = '2560x2160x24'
                if args.startup_monitor == 'DUMMY1':
                    # Keep DUMMY0 primary, at a different scale. Selection
                    # must follow the real current monitor at a nonzero origin.
                    primary_mm = (339, 271) if args.scale != '1' else (170, 135)
                    run_required_xrandr(runtime, display,
                        ['--addmode', 'DUMMY1', '2560x2160'],
                        'startup-second-mode', evidence['operations'])
                    run_required_xrandr(runtime, display, ['--output', 'DUMMY0',
                        '--mode', '1280x1024', '--pos', '0x0', '--primary',
                        '--set', 'WIDTH_MM', str(primary_mm[0]),
                        '--set', 'HEIGHT_MM', str(primary_mm[1]),
                        '--output', 'DUMMY1', '--mode', '2560x2160', '--pos', '1280x0',
                        '--set', 'WIDTH_MM', str(width_mm),
                        '--set', 'HEIGHT_MM', str(height_mm), '--fb', '3840x2160'],
                        'startup-mixed-monitors', evidence['operations'])
                    evidence['randr'] = wait_for_authoritative_state(runtime, display,
                        'real nonzero mixed-scale startup monitor', lambda v:
                            v['monitors'].get('DUMMY1', {}).get('x') == 1280 and
                            v['monitors']['DUMMY1']['mm_width'] == width_mm and
                            v['monitors']['DUMMY1']['mm_height'] == height_mm and
                            v['monitors'].get('DUMMY0', {}).get('width') == 1280 and
                            v['monitors']['DUMMY0']['mm_width'] == primary_mm[0],
                        time.monotonic() + 6, 'startup-mixed-monitors')
                    evidence['display_geometry'] = '3840x2160x24'
            else:
                _, display = start_display_server(runtime, 'Xvfb',
                    ['Xvfb', '-screen', '0', '1920x1800x24', '-noreset'],
                    'xserver.log', time.monotonic() + 10)
            modules, user, native, folder = [runtime.root / name for name in
                ('modules', 'user', 'native-records', 'files')]
            for path in (modules, user, native, folder):
                path.mkdir(mode=0o700)
            # Real nested directory components keep the path below PATH_MAX,
            # while requiring several native disclosure parts.
            for index in range(7):
                folder = folder / ('component-%d-' % index + 'p' * 140)
                folder.mkdir()
            assert 1000 < len(os.fsencode(folder)) < 3000
            special_name = b'000-observed-' + b'n' * 180 + b'-$[%]"\\&-\xff-\n-\t-' + 'é'.encode()
            special_path = os.fsencode(folder) + b'/' + special_name
            descriptor = os.open(special_path, os.O_CREAT | os.O_WRONLY, 0o600)
            os.close(descriptor)
            for index in range(ENTRIES-1):
                (folder / ('entry-%04d' % index)).touch()
            for path in (build / 'modules').iterdir():
                if (path / path.name).is_file() and path.name not in ('FvwmForm', 'FvwmMFL'):
                    (modules / path.name).symlink_to(path / path.name)
            (modules / 'FvwmDesktop').symlink_to(build / 'bin/FvwmPrompt/FvwmDesktop')
            fixture = Path(__file__).resolve()

            def script(path, arguments, suffix=''):
                path.write_text('#!/bin/sh\nexec ' + shlex.join([sys.executable,
                    str(fixture), *map(str, arguments)]) + suffix + '\n')
                path.chmod(0o700)

            for name in ('FvwmForm', 'FvwmMFL'):
                script(modules / name, ['--module-proxy', build / 'modules' / name / name, native],
                    ' "$@" 2>>' + shlex.quote(str(runtime.root / (name + '.log'))))
            assets = source / 'default-config'
            palette, appearance = ('dune', 'light') if profile == 'dune-light' else ('slate', 'dark')
            (user / 'local.config').write_text('*FvwmDesktop: palette ' + palette +
                '\n*FvwmDesktop: appearance ' + appearance +
                '\n*FvwmDesktop: motion Off\n*FvwmDesktop: transparency Off\n')
            body = (assets / 'config').read_text().replace('$[FVWM_DATADIR]/default-config', str(assets))
            if profile == 'classic-large':
                body = body.replace('Sans:size=10:antialias=True',
                    'DejaVu Sans Mono:size=18:antialias=True')
                body += ('\n*FvwmFormDefault: Fore wheat\n*FvwmFormDefault: Back navy\n'
                         '*FvwmFormDefault: ItemFore black\n*FvwmFormDefault: ItemBack ivory\n')
            config = runtime.root / 'config'
            config.write_text('ModulePath ' + str(modules) + '\nRead ' +
                str(source / 'fvwm/ConfigFvwmDefaults') + '\n' + body +
                '\nStyle FvwmFiles-* PositionPlacement 0p 0p\n'
                'AddToFunc StartFunction I Module FvwmMFL\n')
            evidence['config_sha256'] = hashlib.sha256(config.read_bytes()).hexdigest()
            evidence['font'] = 'DejaVu Sans Mono 18' if profile == 'classic-large' else 'Sans 10'
            env = runtime.environment({'DISPLAY': display, 'HOME': str(runtime.root),
                'FVWM_USERDIR': str(user), 'FVWM_DESKTOP_ROOT': str(assets),
                'FVWMMFL_SOCKET_PATH': str(runtime.root), 'LANG': 'C.UTF-8', 'LC_ALL': 'C.UTF-8',
                'XDG_DATA_HOME': str(runtime.root / 'data'), 'XDG_DATA_DIRS': str(runtime.root / 'absent')})
            core = runtime.start('core', [str(build / 'fvwm3'), '-d', display,
                '-f', str(config)], 'core.log', env)
            endpoint = runtime.root / ('fvwm_mfl_' + display.replace(':', '_') + '.sock')
            wait_for_predicate('browser disclosure MFL', endpoint.exists, time.monotonic() + 12, process=core)
            control, rpc_peer = Connection(endpoint), Connection(endpoint)
            control.record()
            rpc_peer.record()
            control.send(b'set echo\n')
            control.echo('browser-disclosure-ready')
            rpc_peer.socket.settimeout(8)
            serial = 0

            def command(text):
                control.send((text + '\n').encode())
                control.echo('continuity-command-' + os.urandom(4).hex())

            def receipt(prefix):
                deadline = time.monotonic() + 6
                while time.monotonic() < deadline:
                    text = control.record().get('echo', {}).get('message', '')
                    if text.startswith(prefix):
                        return text
                raise AssertionError('missing real native receipt ' + prefix)

            def configured_theme():
                control.send(b'SendToModule FvwmDesktop query-settings continuity all\n')
                values = {}
                while True:
                    text = receipt('desktop-settings-continuity ')
                    if text.endswith(' done'):
                        break
                    _, name, value = text.split(' ', 2)
                    values[name] = json.loads(value)
                return values if all(values.get(name, {}).get('effective') == value for name, value in
                    [('palette', palette), ('appearance', appearance), ('transparency', False)]) else None

            evidence['theme_settings'] = wait_for_predicate('requested real desktop palette applied',
                configured_theme, time.monotonic() + 12, process=core)
            if args.startup_monitor == 'DUMMY1':
                # A deliberately large genuine reservation makes the usable
                # top binding even for a centered Details Form. This is a
                # configuration case, not an observed physical panel height.
                command('EwmhBaseStruts screen DUMMY1 11 13 1200 19')
                evidence['secondary_reservation_command'] = \
                    'EwmhBaseStruts screen DUMMY1 11 13 1200 19'

            def rpc(scope='state', method='query', **fields):
                nonlocal serial
                serial += 1
                token = 'continuity-%d' % serial
                wire = frame(token, method, resource='files', **fields,
                    **({'scope': scope} if 'provider' in fields and method == 'query' else {}))
                rpc_peer.send(wire)
                value = rpc_peer.record()['reply']
                receipts = evidence.setdefault('rpc_receipts', [])
                assert len(receipts) < 4096
                receipts.append(dict(request=json.loads(wire.removeprefix(b'request ')),
                    status=value.get('status'), error=value.get('error'),
                    generation=(value.get('result') or {}).get('generation')))
                if value['id'] == token and value['status'] == 'error' and \
                        value['error']['code'] == 'not_ready' and method == 'query' and scope == 'state':
                    return None
                assert value['id'] == token and value['status'] == 'ok', value
                return value['result']

            real_browser = build / 'bin/FvwmPrompt/fvwm-file-browser'

            def start_browser(name):
                arguments = [str(real_browser), '--socket', str(endpoint),
                    '--dir', str(folder), '--view', 'list', '--watch', 'off',
                    '--opener', '/bin/true', '--terminal', '/bin/true']
                return runtime.start(name, arguments, name + '.log', env)

            client = connect_client(display)
            errors = SessionXErrors(client, evidence).install()
            view = NativeView(client, errors, core)
            x, d, pointer = client.x, client.display, view.pointer
            for name, arguments, result in [
                ('XSendEvent', [C.c_void_p, C.c_ulong, C.c_int, C.c_long, C.POINTER(Event)], C.c_int),
                ('XSetInputFocus', [C.c_void_p, C.c_ulong, C.c_int, C.c_ulong], C.c_int),
                ('XGetClassHint', [C.c_void_p, C.c_ulong, C.POINTER(ClassHint)], C.c_int),
                ('XTranslateCoordinates', [C.c_void_p, C.c_ulong, C.c_ulong, C.c_int, C.c_int,
                    C.POINTER(C.c_int), C.POINTER(C.c_int), C.POINTER(C.c_ulong)], C.c_int),
                ('XGetWMNormalHints', [C.c_void_p, C.c_ulong, C.POINTER(SizeHints),
                    C.POINTER(C.c_long)], C.c_int),
                ('XCheckTypedWindowEvent', [C.c_void_p, C.c_ulong, C.c_int, C.POINTER(Event)], C.c_int),
            ]:
                getattr(x, name).argtypes, getattr(x, name).restype = arguments, result

            if args.startup_monitor == 'DUMMY1':
                pointer.motion(1400, 200)
            browser = start_browser('browser')
            listed = wait_for_predicate('initial browser provider registered',
                lambda: rpc()['providers'], time.monotonic() + 8, process=browser)
            provider = listed[0]['provider']

            def state():
                return rpc(provider=provider)

            def ready_state():
                value = state()
                return value if value and not value['busy'] and value['availability']['ready'] and \
                    value['bindings']['projection']['state'] == 'current' else None

            initial = wait_for_predicate('initial full browser projection', ready_state,
                time.monotonic() + 12, process=browser)
            assert initial['loaded'] == ENTRIES and initial['visible'] == ENTRIES, initial
            window = view.wait('File browser')
            hint = ClassHint()
            assert x.XGetClassHint(d, window, C.byref(hint))
            try:
                alias = C.string_at(hint.name).decode()
            finally:
                if hint.name: x.XFree(hint.name)
                if hint.class_: x.XFree(hint.class_)
            assert alias.startswith('FvwmFiles-' + provider + '-') and alias.endswith('-ready'), alias
            capture_titles = {'File browser', 'File browser details'}

            def geometry(target):
                a = view.attributes(target)
                assert a and a.map_state == 2
                return [a.x, a.y, a.width, a.height]

            def root_rectangle(target):
                a = view.attributes(target)
                assert a and a.map_state == 2
                rx, ry, child = C.c_int(), C.c_int(), C.c_ulong()
                assert x.XTranslateCoordinates(d, target, client.root, 0, 0,
                    C.byref(rx), C.byref(ry), C.byref(child))
                return [rx.value-a.border_width, ry.value-a.border_width,
                    a.width+2*a.border_width, a.height+2*a.border_width]

            def frame_origin(target):
                outer = target
                for _ in range(8):
                    parent = client.parent(outer)
                    if parent == client.root:
                        return root_rectangle(outer)[:2]
                    outer = parent
                raise AssertionError('explicit Form parent depth')

            def scale_ready(target, name, expected_q=None):
                def observed():
                    token = 'startup' + os.urandom(8).hex()
                    control.send(('SendToModule ' + name + ' UIScaleStatus ' + token + '\n').encode())
                    raw = receipt('FVWM_FORM_UI_SCALE ' + token + ' ')
                    fields = raw.split()
                    assert len(fields) == 15, raw
                    value = dict(raw=raw, window=target, alias=name,
                        state=fields[3], current_q=int(fields[4]),
                        desired_q=int(fields[5]), current_epoch=fields[6], desired_epoch=fields[7],
                        current_atom=int(fields[8]), desired_atom=int(fields[9]),
                        resource_generation=int(fields[10]),
                        minimum=[int(fields[11]), int(fields[12])],
                        required=[int(fields[13]), int(fields[14])],
                        client=root_rectangle(target))
                    samples = evidence.setdefault('scale_samples', [])
                    assert len(samples) < 4096
                    samples.append(value)
                    if value['state'] != 'ready':
                        return None
                    if expected_q is not None and (value['current_q'] != expected_q or
                            value['desired_q'] != expected_q):
                        return None
                    assert value['current_q'] == value['desired_q'] == int(float(args.scale)*4), value
                    assert value['current_epoch'] == value['desired_epoch'] and int(value['current_epoch'], 16) > 0, value
                    assert value['current_atom'] == value['desired_atom'] != 0, value
                    assert all(actual >= minimum > 0 for actual, minimum in
                        zip(value['client'][2:], value['minimum'])), value
                    return value
                return wait_for_predicate('first actual native ready scale', observed,
                    time.monotonic() + 8, process=browser)

            def native_capture(name):
                previous = os.environ.get('DISPLAY')
                os.environ['DISPLAY'] = display
                try:
                    path = Path(NativeCapture(build / 'bin/fvwm-desktop-capture').screenshot(out)
                        .removeprefix('Screenshot saved: '))
                    path.replace(out / name)
                    evidence['screenshots'].append(name)
                    image = (out / name).read_bytes()
                    assert image[:8] == b'\x89PNG\r\n\x1a\n'
                    manifest = evidence.setdefault('screenshot_manifest', [])
                    assert len(manifest) < 64
                    actual = {}
                    for title in sorted(capture_titles):
                        target = view.named(title)
                        if not target:
                            continue
                        outer = target
                        for _ in range(8):
                            parent = client.parent(outer)
                            if parent == client.root:
                                break
                            outer = parent
                        else:
                            raise AssertionError('captured Form parent depth')
                        actual[title] = dict(window=target,
                            client=root_rectangle(target), frame=root_rectangle(outer))
                    manifest.append(dict(path=str(out / name), windows=actual,
                        sha256=hashlib.sha256(image).hexdigest(),
                        pixels=list(struct.unpack('!II', image[16:24])),
                        published_rectangles=dict(evidence.get('published_view_rectangles', {}))))
                    atomic_json(out / 'screenshot-manifest.json', manifest)
                finally:
                    if previous is None: os.environ.pop('DISPLAY', None)
                    else: os.environ['DISPLAY'] = previous

            def published_view_contained(target, name, oversized=False):
                monitor = evidence.get('randr', {}).get('monitors', {}).get(args.startup_monitor)
                if monitor is None:
                    root_attributes = view.attributes(client.root)
                    bounds = [0, 0, root_attributes.width, root_attributes.height]
                else:
                    bounds = [monitor[key] for key in ('x', 'y', 'width', 'height')]
                reservation = None
                if args.startup_workarea:
                    # Observe original core packets from the transparent
                    # native relay. Root _NET_WORKAREA is not per-monitor
                    # policy, and a MonitorUI row alone has no reservations.
                    samples = [s for s in evidence['scale_samples']
                        if s['state'] == 'ready' and s['alias'] == name]
                    current = samples[-1] if samples else scale_ready(target, name)
                    atom = current['current_atom']
                    incoming = [r['text'] for r in rows(native) if r['direction'] == 'in']
                    ui = [line.split() for line in incoming if line.startswith('MonitorUI 1 ROW ')]
                    selected = [fields for fields in ui if int(fields[5]) == atom][-1]
                    monitor_name = bytes.fromhex(selected[12]).decode()
                    bounds = list(map(int, selected[8:12]))
                    if args.xorg_config:
                        assert monitor_name == args.startup_monitor, (name, selected)
                        fact = evidence['randr']['monitors'][monitor_name]
                        assert bounds == [fact[k] for k in ('x', 'y', 'width', 'height')], \
                            (selected, fact)
                    if args.startup_monitor == 'DUMMY1':
                        primary = [fields for fields in ui
                            if bytes.fromhex(fields[12]).decode() == 'DUMMY0'][-1]
                        assert int(primary[7]) == 1 and int(selected[7]) == 0 and \
                            int(primary[6]) != int(selected[6]), (primary, selected)
                        evidence['mixed_monitor_selection'] = dict(primary=primary,
                            selected=selected, selected_name=monitor_name,
                            real_randr=evidence['randr'],
                            nonzero_origin=bounds[:2], different_scale=True)
                    legacy = [shlex.split(line) for line in incoming if line.startswith('Monitor ')]
                    matching = [fields for fields in legacy if fields[1] == monitor_name]
                    assert matching, (monitor_name, legacy)
                    raw = matching[-1]
                    assert len(raw) == 16, raw
                    left, right, top, bottom = map(int, raw[12:16])
                    assert min(left, right, top, bottom) >= 0, raw
                    if args.startup_monitor == 'DUMMY1' and not oversized:
                        assert [left, right, top, bottom] == [11, 13, 1200, 19], raw
                    usable = [bounds[0] + left, bounds[1] + top,
                        bounds[2] - left - right, bounds[3] - top - bottom]
                    assert min(usable[2:]) > 0, (bounds, raw)
                    reservation = dict(name=monitor_name, physical=bounds,
                        usable=usable, insets=[left, right, top, bottom],
                        core_monitor_record=raw, core_ui_record=selected,
                        stable_initial_config_only=True)
                    bounds = usable
                outer = target
                for _ in range(8):
                    parent = client.parent(outer)
                    if parent == client.root:
                        break
                    outer = parent
                else:
                    raise AssertionError('owned Form frame exceeds parent depth')
                rectangles = dict(client=root_rectangle(target), frame=root_rectangle(outer),
                    monitor=bounds, reservation=reservation, window=target, alias=name)
                if args.startup_workarea:
                    hints, supplied = SizeHints(), C.c_long()
                    assert x.XGetWMNormalHints(d, target, C.byref(hints), C.byref(supplied))
                    rectangles['minimum'] = [hints.min_width, hints.min_height]
                    rectangles['oversized_axes'] = []

                def contained(value):
                    if oversized:
                        # A readable minimum may exceed one or both axes.
                        # Every such axis must anchor at the usable start;
                        # an ordinary fitting frame must remain contained.
                        for axis in range(2):
                            actual = value['frame'][axis + 2]
                            available = bounds[axis + 2]
                            if actual > available:
                                decoration = actual - value['client'][axis + 2]
                                minimum = value['minimum'][axis]
                                if not (minimum + decoration > available and
                                        value['client'][axis + 2] == minimum and
                                        value['frame'][axis] == bounds[axis]):
                                    return False
                                value['oversized_axes'].append(axis)
                            elif not (value['frame'][axis] >= bounds[axis] and
                                    value['frame'][axis] + actual <= bounds[axis] + available):
                                return False
                        return bool(value['oversized_axes'])
                    return all(box[0] >= bounds[0] and box[1] >= bounds[1] and
                        box[0]+box[2] <= bounds[0]+bounds[2] and
                        box[1]+box[3] <= bounds[1]+bounds[3]
                        for box in (value['client'], value['frame']))

                # Capture the actual automatic placement before asserting it.
                # No fixture Move can qualify product startup containment.
                published = evidence.setdefault('published_view_rectangles', {})
                assert len(published) < 64 or name in published
                accepted = contained(rectangles)
                published[name] = rectangles
                atomic_json(out / 'published-view-rectangles.json', published)
                if not accepted:
                    native_capture('automatic-containment-failed.png')
                assert accepted, rectangles
                return rectangles

            evidence['browser_first_ready'] = scale_ready(window, alias)
            first_bounds = published_view_contained(window, alias)
            if args.startup_workarea:
                assert first_bounds['reservation']['insets'][2] > 0, first_bounds
                evidence['reservation_negative_native_qualified'] = False
                evidence['reservation_negative_coverage'] = \
                    'Malformed/missing core metadata belongs to pure admission tests; no native packets injected.'
            ordered_children = view.children(window)
            children = sorted(ordered_children)
            assert 3 <= len(children) <= 64, children
            boxes = {child: geometry(child) for child in children}
            # A legal viewport can have only one row, whose height can tie
            # an Input at fractional scale. Native controls are created in
            # configuration order; use that actual producer schema and the
            # corresponding XIDs rather than guessing from height/width.
            configuration = [r['text'].removeprefix('*' + alias + ': ') for r in rows(native)
                if r['direction'] == 'out' and r['text'].startswith('*' + alias + ': ')]
            controls = [line.split()[:2] for line in configuration if
                line.split()[0] in ('Input', 'InputHex', 'Button', 'List')]
            assert len(controls) == len(children), (controls, children)
            named_controls = {parts[1]: target for parts, target in zip(controls, ordered_children)
                if parts[0] in ('Input', 'InputHex', 'List')}
            list_window = named_controls['picks']
            filter_input, path_input = named_controls['filter'], named_controls['path']
            evidence['native_identity'] = dict(window=window, alias=alias, children=children,
                filter_input=filter_input, path_input=path_input, list=list_window, boxes=boxes)
            for target in [window, *children]:
                x.XSelectInput(d, target, 1 << 17)
            x.XSync(d, 0)

            def remaps():
                events = []
                for target in [window, *children]:
                    for kind in (17, 18, 19):
                        event = Event()
                        while x.XCheckTypedWindowEvent(d, target, kind, C.byref(event)):
                            events.append(dict(window=target, type=kind))
                return events

            remaps()

            def status(target=alias, pending=False, list_name="picks"):
                def observed():
                    token = 'bc' + os.urandom(8).hex()
                    control.send(('SendToModule %s ListStatus %s %s selection\n' %
                        (target, list_name, token)).encode())
                    text = receipt('FVWM_FORM_LIST ' + token + ' ')
                    fields = text.split()
                    assert len(fields) == 6 and fields[4] == '0', text
                    packet = base64.urlsafe_b64decode(fields[5][1:] + '==')
                    assert len(packet) >= 6 and int.from_bytes(packet[:4], 'big') == int(fields[2]), text
                    value = dict(generation=int(fields[2]), pending=int(fields[3]), packet=packet.hex(),
                        focus=int.from_bytes(packet[4:6], 'big'),
                        selected=[i for i in range((len(packet)-6)*8) if packet[6+i//8] & (1 << (i % 8))],
                        receipt=text)
                    if not pending and value['pending'] != 0:
                        observations = evidence.setdefault('native_pending_observations', [])
                        assert len(observations) < 4096
                        observations.append(dict(alias=target, generation=value['generation'],
                            pending=value['pending']))
                        return None
                    return value

                # An explicit native action can legitimately stage thousands
                # of entries while this independent observer asks ListStatus.
                # Only a real pending=0 reply can satisfy ordinary assertions.
                return observed() if pending else wait_for_predicate(
                    'quiescent native collection status', observed,
                    time.monotonic() + 8, process=browser)

            owners = {child: window for child in children}

            def focused(target=None):
                target = window if target is None else target
                x.XSetInputFocus(d, target, 1, 0)
                x.XSync(d, 0)
                assert view.focus() == target

            def key(symbol, shift=False, alt=False):
                modifiers = [name for name, active in [('Shift_L', shift), ('Alt_L', alt)] if active]
                codes = [x.XKeysymToKeycode(d, x.XStringToKeysym(name.encode())) for name in modifiers]
                for code in codes:
                    assert code
                    assert pointer.x.XTestFakeKeyEvent(d, code, 1, 0)
                view.key(symbol)
                for code in reversed(codes):
                    assert pointer.x.XTestFakeKeyEvent(d, code, 0, 0)
                x.XSync(d, 0)

            def click(target, px=None, py=None, button=1):
                owner = owners.get(target, target)
                command('WindowId %d Raise' % owner)
                command('WindowId %d Focus' % owner)
                wait_for_predicate('managed click owner focused', lambda: view.focus() == owner,
                    time.monotonic() + 5, process=core)
                box = geometry(target)
                rx, ry, child = C.c_int(), C.c_int(), C.c_ulong()
                assert x.XTranslateCoordinates(d, target, client.root,
                    box[2]//2 if px is None else px, box[3]//2 if py is None else py,
                    C.byref(rx), C.byref(ry), C.byref(child))
                pointer.motion(rx.value, ry.value)
                leaf, path = client.root, []
                for _ in range(16):
                    pointer_root, pointer_child = C.c_ulong(), C.c_ulong()
                    root_x, root_y, win_x, win_y = [C.c_int() for _ in range(4)]
                    mask = C.c_uint()
                    assert x.XQueryPointer(d, leaf, C.byref(pointer_root), C.byref(pointer_child),
                        C.byref(root_x), C.byref(root_y), C.byref(win_x), C.byref(win_y),
                        C.byref(mask))
                    path.append(leaf)
                    if not pointer_child.value:
                        break
                    leaf = pointer_child.value
                else:
                    raise AssertionError('owned pointer hit-test exceeded tree depth')
                assert leaf == target, (target, owner, path)
                evidence.setdefault('pointer_hits', []).append(dict(target=target, owner=owner,
                    path=path, root_point=[rx.value, ry.value], focus=view.focus()))
                assert len(evidence['pointer_hits']) <= 256
                assert pointer.x.XTestFakeButtonEvent(d, button, 1, 0)
                assert pointer.x.XTestFakeButtonEvent(d, button, 0, 0)
                x.XSync(d, 0)
                wait_for_predicate('owned pointer grab released', lambda: not pointer.grabbed(),
                    time.monotonic() + 5, process=core)

            def producer_quiet():
                command('Nop disclosure-producer-barrier')
                wait_for_predicate('quiet after real producer round trip',
                    lambda: time.monotonic() - max([r['time'] for r in rows(native)] or [0]) > .12,
                    time.monotonic() + 3, process=browser)

            def capture(name):
                # Quiet output is diagnostic only. Neither core Nop nor the
                # observer's XSync is a cross-channel native paint fence.
                producer_quiet()
                native_capture(name)

            if args.startup_workarea:
                capture('startup-browser-usable-first-ready.png')

            def same_identity():
                assert view.named('File browser') == window
                assert sorted(view.children(window)) == children
                assert not remaps(), 'live layout remapped or replaced a native control'

            def contained():
                outer = geometry(window)
                actual = {child: geometry(child) for child in children}
                assert all(g[0] >= 0 and g[1] >= 0 and g[2] > 0 and g[3] > 0 and
                    g[0]+g[2] <= outer[2] and g[1]+g[3] <= outer[3] for g in actual.values()), actual
                return actual

            def module_alias(target):
                hint = ClassHint()
                assert x.XGetClassHint(d, target, C.byref(hint))
                try:
                    return C.string_at(hint.name).decode()
                finally:
                    if hint.name: x.XFree(hint.name)
                    if hint.class_: x.XFree(hint.class_)

            def detail_page(previous=None):
                def current():
                    commands = [r['text'] for r in rows(native) if r['direction'] == 'out']
                    aliases = [c.split()[-1] for c in commands if
                        c.startswith('Module FvwmForm FvwmFiles-' + provider + '-details-')]
                    if not aliases or aliases[-1] == previous:
                        return None
                    target = view.named('File browser details')
                    if not target or module_alias(target) != aliases[-1]:
                        return None
                    name = aliases[-1]
                    lines = [c.removeprefix('*' + name + ': ') for c in commands
                        if c.startswith('*' + name + ': ')]
                    values = [c.split()[-1] for c in lines if c.startswith('InputHex value 40 x')]
                    if len(values) != 1:
                        return None
                    value = bytes.fromhex(values[0][1:]).decode('ascii')
                    assert len(value) <= 512
                    assert 'InputReadonly value' in lines
                    assert 'List sections 40 4' in lines and 'ListViewport 18 1' in lines
                    label = next(c for c in lines if c.startswith('Text "Field: '))
                    matched = re.fullmatch(r'Text "Field: (.*); Part (\d+)/(\d+)"', label)
                    assert matched, label
                    ready = scale_ready(target, name)
                    controls = view.children(target)
                    assert 3 <= len(controls) <= 10
                    owners.update({child: target for child in controls})
                    schema = [line.split()[:2] for line in lines if
                        line.split()[0] in ('Input', 'InputHex', 'Button', 'List')]
                    assert len(schema) == len(controls), (schema, controls)
                    names = {parts[1]: child for parts, child in zip(schema, controls)
                        if parts[0] in ('Input', 'InputHex', 'List')}
                    collection, value_input = names['sections'], names['value']
                    sections = [c[len('ListItem off "'):-1] for c in lines
                        if c.startswith('ListItem off "')]
                    published_view_contained(target, name)
                    return dict(alias=name, window=target, value=value,
                        field=matched[1], part=int(matched[2]), parts=int(matched[3]),
                        sections=sections, list=collection, input=value_input, lines=lines, ready=ready)
                return wait_for_predicate('actual disclosure page', current,
                    time.monotonic() + 8, process=browser)

            def open_details():
                focused()
                key('F1')
                page = detail_page()
                assert page['field'] == 'Path' and page['part'] == 1
                return page

            def change_page(page, symbol):
                focused(page['window'])
                key(symbol)
                return detail_page(page['alias'])

            def choose(page, field):
                click(page['list'], 8, 8)
                focused(page['window'])
                key('Home')
                for _ in range(page['sections'].index(field)):
                    key('Down')
                selected = change_page(page, 'Return')
                assert selected['field'] == field and selected['part'] == 1, selected
                return selected

            def full_value(page):
                pieces = [page['value']]
                for number in range(2, page['parts'] + 1):
                    page = change_page(page, 'F10')
                    assert page['part'] == number
                    pieces.append(page['value'])
                return ''.join(pieces), page

            def escaped(raw):
                result = []
                for char in raw.decode('utf-8', 'surrogateescape'):
                    code = ord(char)
                    if 0xdc80 <= code <= 0xdcff:
                        result.append('[BYTE+%02X]' % (code-0xdc00))
                    elif code < 32 or code > 126 or char in '$%\\"&[]':
                        result.append('[U+%04X]' % code)
                    else:
                        result.append(char)
                return ''.join(result)

            input_rasters = {}

            def input_pixels(page, expected=None, different=None, label='input'):
                # Two real native status replies support the loop/flush
                # observation after XTest's server sync. They are not a
                # universal ordering fence between independent channels.
                for _ in range(2):
                    status(page['alias'], list_name=page.get('list_name', 'sections'))
                target = page['input']
                previous, since, latest_raster = None, time.monotonic(), None

                def observed():
                    nonlocal previous, since, latest_raster
                    a = view.attributes(target)
                    assert a and a.map_state == 2
                    assert 0 < a.width <= 1440 and 0 < a.height <= 1200
                    image = x.XGetImage(d, target, 0, 0, a.width, a.height,
                        C.c_ulong(-1).value, 2)
                    assert image
                    try:
                        raster = [x.XGetPixel(image, px, py)
                            for py in range(2, a.height-2)
                            for px in range(2, a.width-2)]
                    finally:
                        x.XDestroyImage(image)
                    latest_raster = raster
                    colors = dict(Counter(raster))
                    actual = dict(window=target, width=a.width, height=a.height,
                        sha256=hashlib.sha256(struct.pack('>%dI' % len(raster), *raster)).hexdigest(),
                        colors=colors)
                    if actual != previous:
                        previous, since = actual, time.monotonic()
                        return None
                    if expected is not None and actual != expected or \
                            different is not None and actual == different:
                        return None
                    return actual if time.monotonic() - since >= .05 else None

                try:
                    actual = wait_for_predicate('expected stable positional input raster', observed,
                        time.monotonic() + 2, process=browser)
                finally:
                    diagnostic = dict(label=label, observed=previous, expected=expected,
                        different=different)
                    if previous is not None and latest_raster is not None:
                        width, height = previous['width']-4, previous['height']-4
                        # Encode the actual spatial XImage interior, without
                        # compositing, resizing, clipping away differences or
                        # touching the producer's value/layout.
                        image_path = out / (label + '.ppm')
                        rgb = bytearray()
                        for pixel in latest_raster:
                            rgb.extend(((pixel >> 16) & 255, (pixel >> 8) & 255, pixel & 255))
                        image_path.write_bytes(('P6\n%d %d\n255\n' % (width, height)).encode() + rgb)
                        diagnostic['raw_image'] = image_path.name
                        if expected is not None:
                            reference = input_rasters[expected['sha256']]
                            assert len(reference) == len(latest_raster)
                            changes = [(index % width + 2, index // width + 2, old, new)
                                for index, (old, new) in enumerate(zip(reference, latest_raster))
                                if old != new]
                            diagnostic['difference'] = dict(count=len(changes),
                                first_pixels=[dict(x=px, y=py, before=old, after=new)
                                    for px, py, old, new in changes[:256]],
                                bounding_box=([min(p[0] for p in changes), min(p[1] for p in changes),
                                    max(p[0] for p in changes), max(p[1] for p in changes)]
                                    if changes else None))
                        input_rasters[previous['sha256']] = latest_raster
                    evidence.setdefault('input_pixel_samples', []).append(diagnostic)
                    atomic_json(out / 'input-pixel-samples.json', evidence['input_pixel_samples'])
                assert len(actual['colors']) >= 2, 'native value has no contrasting pixels'
                return actual

            def close_details(page):
                focused(page['window'])
                key('Escape')
                wait_for_predicate('details closes', lambda: not view.named('File browser details'),
                    time.monotonic() + 5, process=browser)

            def detail_callbacks(after=0):
                return [r['text'] for r in rows(native) if r['time'] >= after and
                    r['direction'] == 'out' and r['text'].startswith('Echo FVWM_FILES_DETAILS_1 ')]

            if args.editable_continuity_only:
                # This mode shares the actual browser/session setup but runs
                # only this checkpoint's editing and reflow scenes.
                draft_started = time.monotonic()
                path_draft = 'head-path-' + 'middle-' * 24 + 'tail-path'
                filter_draft = 'head-filter-' + 'middle-' * 10 + 'tail-filter'

                def type_draft(target, text):
                    click(target)
                    focused()
                    view.key('u', control=True)
                    for char in text:
                        key('minus' if char == '-' else char)
                    key('Home')
                    key('Right')
                    key('Right')

                def parent_pixels(target, label, expected=None):
                    return input_pixels(dict(alias=alias, input=target, list_name='picks'),
                        expected=expected, label=label)

                type_draft(path_input, path_draft)
                type_draft(filter_input, filter_draft)
                packet_before = status()
                before_scale = scale_ready(window, alias)
                original_size = geometry(window)[2:]
                filter_head = parent_pixels(filter_input, 'editable-filter-head')
                path_head = parent_pixels(path_input, 'editable-path-unfocused-head')
                capture('editable-long-drafts-home.png')

                def identity_and_selection(before, label, generation_delta=0):
                    after = status()
                    same_identity()
                    assert after['generation'] == before['generation'] + generation_delta, (label, before, after)
                    assert after['packet'][8:] == before['packet'][8:], (label, before, after)
                    evidence.setdefault('editable_native_continuity', []).append(
                        dict(label=label, before=before, after=after, children=view.children(window)))
                    return after

                # Resize away and back without touching either input. A real
                # native status plus positional samples observes the paint.
                enlarged = [original_size[0] + 40, original_size[1] + 40]
                x.XResizeWindow(d, window, *enlarged)
                x.XSync(d, 0)
                wait_for_predicate('editable browser enlarged',
                    lambda: geometry(window)[2:] == enlarged,
                    time.monotonic() + 5, process=browser)
                identity_and_selection(packet_before, 'resize-enlarged')
                x.XResizeWindow(d, window, *original_size)
                x.XSync(d, 0)
                wait_for_predicate('editable browser size restored',
                    lambda: geometry(window)[2:] == original_size,
                    time.monotonic() + 5, process=browser)
                parent_pixels(filter_input, 'editable-filter-after-resize', filter_head)
                parent_pixels(path_input, 'editable-path-after-resize', path_head)

                packet = status()
                for index in range(2):
                    wanted = 'grid' if state()['view'] == 'list' else 'list'
                    focused()
                    key('F7')
                    wait_for_predicate('editable draft real ' + wanted + ' view',
                        lambda: (v if v['view'] == wanted else None)
                            if (v := ready_state()) else None,
                        time.monotonic() + 8, process=browser)
                    packet = identity_and_selection(packet, 'view-' + wanted, 1)
                    parent_pixels(filter_input, 'editable-filter-view-' + str(index), filter_head)
                    parent_pixels(path_input, 'editable-path-view-' + str(index), path_head)

                if args.xorg_config:
                    original_scale = args.scale
                    alternate = '1.25' if original_scale != '1.25' else '1'
                    previous_scale = before_scale
                    transitions = []
                    evidence['editable_scale_continuity'] = dict(before=before_scale,
                        transitions=transitions)
                    for requested in (alternate, original_scale):
                        mm = {'1': (677, 572), '1.25': (542, 458), '2': (339, 286)}[requested]
                        run_required_xrandr(runtime, display, ['--output', 'DUMMY0',
                            '--set', 'WIDTH_MM', str(mm[0]), '--set', 'HEIGHT_MM', str(mm[1])],
                            'editable-scale-' + requested, evidence['operations'])
                        randr = wait_for_authoritative_state(runtime, display, 'editable actual scale metadata',
                            lambda v: v['monitors'].get('DUMMY0', {}).get('mm_width') == mm[0] and
                                v['monitors']['DUMMY0']['mm_height'] == mm[1],
                            time.monotonic() + 6, 'editable-scale-' + requested)
                        args.scale = requested
                        try:
                            observed_scale = scale_ready(window, alias, int(float(requested)*4))
                        finally:
                            args.scale = original_scale
                        transitions.append(dict(requested_q=int(float(requested)*4),
                            requested_mm=list(mm), randr_monitor=randr['monitors']['DUMMY0'],
                            before=previous_scale, after=observed_scale))
                        assert observed_scale['current_q'] == observed_scale['desired_q'] == int(float(requested)*4)
                        assert observed_scale['current_q'] != previous_scale['current_q']
                        assert int(observed_scale['current_epoch'], 16) > int(previous_scale['current_epoch'], 16)
                        assert int(observed_scale['desired_epoch'], 16) > int(previous_scale['desired_epoch'], 16)
                        assert observed_scale['current_atom'] == previous_scale['current_atom']
                        # Quarter factors and topology epochs are distinct
                        # from resource invalidation. This scene changes only
                        # RandR millimetres; the font cache key includes its
                        # factor separately from the retained resource owner.
                        assert observed_scale['resource_generation'] == previous_scale['resource_generation'] >= 1
                        identity_and_selection(packet, 'scale-' + requested)
                        previous_scale = observed_scale
                    evidence['editable_scale_continuity']['after'] = observed_scale
                else:
                    evidence['editable_scale_continuity'] = 'unavailable: real RandR configuration not supplied'

                # Preserve the non-tail cursor through all of the above. Read
                # bytes only through actual configured native callbacks.
                focused()
                key('x')
                submitted = time.monotonic()
                key('f', alt=True)

                def draft_callback(operation):
                    def observed():
                        found = [r['text'] for r in rows(native) if r['time'] >= submitted and
                            r['direction'] == 'out' and r['text'].startswith('Echo FVWM_FILES_3 ' + provider + ' ')
                            and r['text'].split()[4] == operation]
                        return found[-1] if found else None
                    return wait_for_predicate('editable native ' + operation + ' bytes', observed,
                        time.monotonic() + 6, process=browser)

                filter_callback = draft_callback('filter')
                expected_filter = filter_draft[:2] + 'x' + filter_draft[2:]
                assert base64.urlsafe_b64decode(filter_callback.split()[5][1:] + '==') == expected_filter.encode()
                wait_for_predicate('real long filter draft applied',
                    lambda: (v if v['filter'] == expected_filter else None)
                        if (v := ready_state()) else None,
                    time.monotonic() + 8, process=browser)
                click(path_input)
                focused()
                # Browser Return may activate Open. Down traverses from the
                # last field to the first without firing that action.
                key('End')
                key('Down')
                key('BackSpace')
                key('Home')
                key('x')
                submitted = time.monotonic()
                key('f', alt=True)
                wrap_callback = draft_callback('filter')
                assert base64.urlsafe_b64decode(wrap_callback.split()[5][1:] + '==') == ('x' + expected_filter).encode()
                click(path_input)
                focused()
                key('Home')
                key('Right')
                key('Right')
                key('x')
                submitted = time.monotonic()
                key('g', alt=True)
                path_callback = draft_callback('path')
                expected_path = path_draft[:2] + 'x' + path_draft[2:]
                assert base64.urlsafe_b64decode(path_callback.split()[5][1:] + '==') == expected_path.encode()
                same_identity()
                actual_callbacks = [r['text'].split()[4] for r in rows(native) if r['time'] >= draft_started and
                    r['direction'] == 'out' and r['text'].startswith('Echo FVWM_FILES_3 ' + provider + ' ')]
                assert actual_callbacks == ['view', 'view', 'filter', 'filter', 'path'], actual_callbacks
                evidence['editable_draft_callbacks'] = dict(filter=filter_callback,
                    wrap_filter=wrap_callback, path=path_callback, operations=actual_callbacks,
                    filter_bytes=expected_filter.encode().hex(), path_bytes=expected_path.encode().hex())
                # The preceding exact-byte probe deliberately submitted a
                # nonexistent Path. The provider retains the admitted snapshot;
                # normal F5 Refresh reloads that exact real directory before
                # the next scenes require a loaded/current projection. Its path
                # exceeds the native callback payload bound, so do not retype
                # the whole disclosure-path fixture through a Path callback.
                submitted = time.monotonic()
                focused()
                key('F5')
                restore_callback = draft_callback('refresh')
                restored = wait_for_predicate('real directory restored before pointer/paste',
                    lambda: (v if base64.b64decode(v['path_bytes']) == str(folder).encode()
                        and v['loaded'] == ENTRIES else None)
                        if (v := ready_state()) else None,
                    time.monotonic() + 8, process=browser)
                same_identity()
                evidence['pointer_paste_ready_restore'] = dict(method='native-F5-refresh',
                    callback=restore_callback,
                    path_bytes=restored['path_bytes'], loaded=restored['loaded'],
                    busy=restored['busy'], availability=restored['availability'],
                    projection=restored['bindings']['projection'])
                # Pointer mapping is measured from the two actual painted
                # carets, rather than guessed from a requested font/backend.
                type_draft(filter_input, 'abc')
                key('Home')
                pointer_home = parent_pixels(filter_input, 'pointer-filter-home')
                key('Right')
                pointer_first = parent_pixels(filter_input, 'pointer-filter-first')
                old_pixels = input_rasters[pointer_home['sha256']]
                new_pixels = input_rasters[pointer_first['sha256']]
                assert pointer_home['width'] == pointer_first['width']
                inner_width = pointer_home['width'] - 4
                columns = sorted({i % inner_width + 2
                    for i, (old, new) in enumerate(zip(old_pixels, new_pixels)) if old != new})
                groups = []
                for px in columns:
                    if not groups or px != groups[-1][-1] + 1:
                        groups.append([])
                    groups[-1].append(px)
                assert len(groups) == 2 and len(groups[0]) == len(groups[1]), groups
                assert 1 <= len(groups[1]) <= 4, groups
                # The painter's filled caret starts one pixel before the
                # measured text boundary, independent of its stroke width.
                boundary_x = groups[1][0] + 1
                click(filter_input, px=boundary_x)
                focused()
                key('x')
                submitted = time.monotonic()
                key('f', alt=True)
                pointer_callback = draft_callback('filter')
                assert base64.urlsafe_b64decode(pointer_callback.split()[5][1:] + '==') == b'axbc'
                wait_for_predicate('pointer-edited real filter applied',
                    lambda: (v if v['filter'] == 'axbc' else None)
                        if (v := ready_state()) else None,
                    time.monotonic() + 8, process=browser)
                evidence['pointer_input_boundary'] = dict(home=pointer_home,
                    first=pointer_first, changed_column_groups=groups,
                    clicked_x=boundary_x, expected_index=1, callback=pointer_callback)

                type_draft(filter_input, 'def')
                type_draft(path_input, path_draft)
                key('Home')
                x.XStoreBuffer.argtypes = [C.c_void_p, C.c_char_p, C.c_int, C.c_int]
                paste_bytes = b'A\tB'
                x.XStoreBuffer(d, paste_bytes, len(paste_bytes), 0)
                x.XSync(d, 0)
                # Path is the last Input. One real pasted Tab must wrap once
                # into filter, without firing a configured action or returning
                # to Path. The left border click clamps to the visible start.
                click(path_input, px=2, button=2)
                focused()
                pasted_filter = parent_pixels(filter_input, 'paste-filter-immediate')
                pasted_path = parent_pixels(path_input, 'paste-path-immediate')
                capture('pointer-paste-drafts-after-reflow.png')
                x.XClearArea.argtypes = [C.c_void_p, C.c_ulong, C.c_int,
                    C.c_int, C.c_uint, C.c_uint, C.c_int]
                for target in (filter_input, path_input):
                    x.XClearArea(d, target, 0, 0, 0, 0, 1)
                x.XSync(d, 0)
                parent_pixels(filter_input, 'paste-filter-after-expose', pasted_filter)
                parent_pixels(path_input, 'paste-path-after-expose', pasted_path)
                submitted = time.monotonic()
                key('f', alt=True)
                paste_filter_callback = draft_callback('filter')
                assert base64.urlsafe_b64decode(paste_filter_callback.split()[5][1:] + '==') == b'Bdef'
                wait_for_predicate('pasted real filter applied',
                    lambda: (v if v['filter'] == 'Bdef' else None)
                        if (v := ready_state()) else None,
                    time.monotonic() + 8, process=browser)
                submitted = time.monotonic()
                key('g', alt=True)
                paste_path_callback = draft_callback('path')
                assert base64.urlsafe_b64decode(paste_path_callback.split()[5][1:] + '==') == ('A' + path_draft).encode()
                same_identity()
                all_callbacks = [r['text'].split()[4] for r in rows(native)
                    if r['time'] >= draft_started and r['direction'] == 'out'
                    and r['text'].startswith('Echo FVWM_FILES_3 ' + provider + ' ')]
                assert all_callbacks == ['view', 'view', 'filter', 'filter', 'path',
                    'refresh', 'filter', 'filter', 'path'], all_callbacks
                evidence['pointer_paste_callbacks'] = dict(filter=pointer_callback,
                    paste_filter=paste_filter_callback, paste_path=paste_path_callback,
                    operations=all_callbacks, pasted_bytes=paste_bytes.hex(),
                    expected_filter='Bdef', expected_path='A' + path_draft,
                    immediate_paint_matches_expose=True)
                evidence['status'] = 'passed'
                return

            if args.startup_continuity:
                page = open_details()
                first = dict(page['ready'], list=geometry(page['list']),
                    input=geometry(page['input']), children=view.children(page['window']))
                evidence['details_first_ready'] = first
                atomic_json(out / 'startup-geometry.json', first)
                capture('startup-details-first-ready.png')
                hints, supplied = SizeHints(), C.c_long()
                assert x.XGetWMNormalHints(d, page['window'], C.byref(hints), C.byref(supplied))
                minimum = [hints.min_width, hints.min_height]
                x.XResizeWindow(d, page['window'], *minimum)
                x.XSync(d, 0)
                wait_for_predicate('actual legal one-row minimum Configure',
                    lambda: geometry(page['window'])[2:] == minimum and
                        geometry(page['list'])[3] == first['list'][3] + minimum[1] - first['client'][3],
                    time.monotonic() + 6, process=browser)
                minimum_list = geometry(page['list'])
                box = (3 * int(float(args.scale)*4) + 2) // 4
                row_height = minimum_list[3] - 2 * box
                expected_height = 4 * row_height + 2 * box
                evidence['startup_preferred_rows'] = dict(first=first,
                    minimum=minimum, minimum_list=minimum_list,
                    scaled_box=box, row_height=row_height,
                    configured_rows=4, expected_list_height=expected_height)
                atomic_json(out / 'startup-geometry.json', evidence['startup_preferred_rows'])
                assert row_height > 0 and (first['list'][3] - 2 * box) // row_height == 4, \
                    evidence['startup_preferred_rows']
                assert first['list'][2] >= first['input'][2], first
                capture('startup-details-one-row-minimum.png')
                parent_before, packet_before = ready_state(), status()
                close_details(page)
                assert ready_state()['bindings'] == parent_before['bindings']
                assert status()['packet'] == packet_before['packet']
                same_identity()

                def configured_form(name, placement=(), enabled=True):
                    lines = ['Title "' + name + '"',
                        'UIScale ' + ('monitor' if enabled else 'off'), *placement,
                        'Line left', 'Input draft 40 "draft"',
                        'Button continue "Observe" F12',
                        'Command Echo STARTUP_DRAFT ' + name + ' x$(draft:Base64) x$(picks:Base64)',
                        'Button quit "Close" F11', 'Line left',
                        'List picks 40 4', 'ListViewport 18 1', 'ListVersioned',
                        *['ListItem ' + ('on' if i == 7 else 'off') +
                            ' "row-%02d"' % i for i in range(32)], 'ListFocus 8']
                    for line in lines:
                        control.send(('*' + name + ': ' + line + '\n').encode())
                    if args.startup_monitor == 'DUMMY1':
                        pointer.motion(1400, 200)
                    command('Module FvwmForm ' + name)
                    target = view.wait(name)
                    capture_titles.add(name)
                    ids = view.children(target)
                    schema = [line.split()[:2] for line in lines if
                        line.split()[0] in ('Input', 'Button', 'List')]
                    assert len(ids) == len(schema) == 4, (ids, schema)
                    mapping = [dict(kind=parts[0], name=parts[1], window=child)
                        for parts, child in zip(schema, ids)]
                    named = {item['name']: item['window'] for item in mapping
                        if item['kind'] in ('Input', 'List')}
                    owners.update({child: target for child in ids})
                    value = scale_ready(target, name) if enabled else None
                    return dict(alias=name, window=target, children=ids,
                        input=named['draft'], list=named['picks'], ready=value,
                        control_mapping=mapping, placement=list(placement))

                def form_status(form):
                    return status(form['alias'])

                def resize_form(form, size):
                    before = geometry(form['window'])
                    before_list = geometry(form['list'])
                    x.XResizeWindow(d, form['window'], *size)
                    x.XSync(d, 0)
                    def reflowed():
                        actual = geometry(form['window'])
                        collection = geometry(form['list'])
                        if actual[2:] != size or collection[3] != before_list[3] + size[1] - before[3]:
                            return None
                        return dict(client=root_rectangle(form['window']), list=collection)
                    return wait_for_predicate('actual Configure and child viewport reflow',
                        reflowed, time.monotonic() + 6, process=core)

                def settled_packet(form, predicate):
                    return wait_for_predicate('actual native input result',
                        lambda: (v if predicate(v) else None) if (v := form_status(form)) else None,
                        time.monotonic() + 6, process=core)

                def close_form(form):
                    focused(form['window'])
                    key('F11')
                    wait_for_predicate('owned startup Form closed',
                        lambda: not view.named(form['alias']), time.monotonic() + 5, process=core)
                    capture_titles.discard(form['alias'])

                probe = configured_form('StartupContinuity')
                published_view_contained(probe['window'], probe['alias'])
                original = dict(packet=form_status(probe),
                    client=geometry(probe['window']), list=geometry(probe['list']),
                    scale=probe['ready'], children=probe['children'])
                assert original['packet']['focus'] == 7 and original['packet']['selected'] == [7], original
                capture('startup-nonzero-focus-first-ready.png')
                assert (original['list'][3] - 2 * box) // row_height == 4, original
                click(probe['list'], box + 2, box + 3 * row_height + 2)
                initial_hit = settled_packet(probe, lambda v: v['focus'] == 7 and not v['selected'])
                click(probe['list'], box + 2, box + 3 * row_height + 2)
                settled_packet(probe, lambda v: v['packet'] == original['packet']['packet'])
                click(probe['input'])
                focused(probe['window'])
                key('Home')
                key('Right')
                key('Right')
                minimum = probe['ready']['minimum']
                shrunk = resize_form(probe, minimum)
                unchanged = form_status(probe)
                assert unchanged['packet'] == original['packet']['packet'], (original, unchanged)
                assert view.children(probe['window']) == probe['children']
                assert scale_ready(probe['window'], probe['alias'])['resource_generation'] == \
                    original['scale']['resource_generation']
                # No field traversal or click occurs between the cursor setup,
                # native Configure/reflow and inserted byte.
                key('x')
                key('F12')
                draft = receipt('STARTUP_DRAFT ' + probe['alias'] + ' ')
                assert base64.urlsafe_b64decode(draft.split()[2][1:] + '==') == b'drxaft', draft
                assert base64.urlsafe_b64decode(draft.split()[3][1:] + '==').hex() == unchanged['packet'], draft
                # A one-row actual hit proves the preserved focus is visible;
                # toggling the same identity twice restores the exact packet.
                click(probe['list'], box + 2, box + 2)
                hit = settled_packet(probe, lambda v: v['focus'] == 7 and not v['selected'])
                click(probe['list'], box + 2, box + 2)
                restored = settled_packet(probe, lambda v: v['packet'] == unchanged['packet'])
                capture('startup-visible-focus-after-shrink.png')
                def first_row_pixel():
                    width, height = geometry(probe['list'])[2:]
                    image = x.XGetImage(d, probe['list'], width // 2,
                        box + row_height // 2, 1, 1, C.c_ulong(-1).value, 2)
                    assert image and height > box + row_height // 2
                    try:
                        return x.XGetPixel(image, 0, 0)
                    finally:
                        x.XDestroyImage(image)

                # A changed actual selected-row background proves the wheel
                # reached and painted the Form before the independent resize.
                selected_pixel = first_row_pixel()
                # One real wheel event scrolls three rows without moving focus.
                click(probe['list'], box + 2, box + 2, button=5)
                wheel_pixel = wait_for_predicate('actual wheel row background changes',
                    lambda: (dict(pixel=p) if (p := first_row_pixel()) != selected_pixel else None),
                    time.monotonic() + 6, process=core)
                wheel = form_status(probe)
                assert wheel['packet'] == restored['packet'], (wheel, restored)
                reflow = resize_form(probe, [original['client'][2], minimum[1] + row_height])
                after_wheel_reflow = form_status(probe)
                assert after_wheel_reflow['packet'] == restored['packet']
                capture('startup-wheel-focus-remains-offscreen.png')
                click(probe['list'], box + 2, box + 2)
                offscreen = settled_packet(probe, lambda v: v['focus'] == 10 and v['selected'] == [7, 10])
                evidence['viewport_continuity'] = dict(form=probe, before=original,
                    shrunk=shrunk, unchanged=unchanged, draft_callback=draft, initial_visible_hit=initial_hit,
                    visible_hit=hit, restored=restored, wheel=wheel,
                    wheel_pixels=dict(before=selected_pixel, after=wheel_pixel),
                    wheel_reflow=reflow, after_wheel_reflow=after_wheel_reflow,
                    first_visible_after_wheel_reflow=offscreen,
                    visible_focus_retained=True, deliberate_offscreen_focus_retained=True)
                # Startup containment and every screenshot above have already
                # passed. This explicit command is intentional user movement,
                # never a placement correction for a startup overhang.
                monitor_x = first_bounds['reservation']['physical'][0] if args.startup_workarea else 0
                # Normal Move honors the working area. Keep this secondary
                # scene's requested destination below its large reservation.
                user_y = first_bounds['reservation']['usable'][1] + 120 \
                    if args.startup_monitor == 'DUMMY1' else 120
                user_origin = [monitor_x + 100, user_y]
                user_move = 'Move %dp %dp' % tuple(user_origin)
                command('WindowId %d %s' % (probe['window'], user_move))
                moved_origin = wait_for_predicate('intentional managed user movement',
                    lambda: (p if (p := frame_origin(probe['window'])) == user_origin else None),
                    time.monotonic() + 6, process=core)
                user_resize = resize_form(probe,
                    [original['client'][2], minimum[1] + 2 * row_height])
                assert frame_origin(probe['window']) == moved_origin
                assert form_status(probe)['packet'] == offscreen['packet']
                assert view.children(probe['window']) == probe['children']
                evidence['post_startup_user_ownership'] = dict(command=user_move,
                    moved_origin=moved_origin, after_resize=user_resize,
                    after_resize_origin=frame_origin(probe['window']))
                close_form(probe)

                # Compare actual managed frame origins against an unscaled
                # twin with the identical explicit directive. Decorations are
                # measured, rather than inferred from core/theme constants.
                explicit = []
                for index, directive in enumerate(('Position 2400 1800', 'Geometry +2400+1800')):
                    reference = configured_form('StartupExplicitOff%d' % index,
                        [directive], enabled=False)
                    origin = frame_origin(reference['window'])
                    reference_rect = root_rectangle(reference['window'])
                    close_form(reference)
                    placed = configured_form('StartupExplicitOn%d' % index, [directive])
                    actual = frame_origin(placed['window'])
                    record = dict(directive=directive, reference_origin=origin,
                        reference_client=reference_rect, scaled_origin=actual,
                        scaled_client=root_rectangle(placed['window']), scale=placed['ready'])
                    explicit.append(record)
                    assert actual == origin, record
                    initial_size = geometry(placed['window'])[2:]
                    resize_form(placed, [initial_size[0], initial_size[1] + row_height])
                    record['after_resize_origin'] = frame_origin(placed['window'])
                    assert record['after_resize_origin'] == origin, record
                    close_form(placed)
                evidence['explicit_placement'] = explicit
                evidence['negative_explicit_placement_freshly_qualified'] = False
                if args.startup_oversized:
                    # Exercise genuine producer facts before this Form's
                    # first configuration replay. Existing ready Forms retain
                    # user geometry; no live atomic-freshness claim is made.
                    reservation = first_bounds['reservation']
                    physical = reservation['physical']
                    before = dict(client=root_rectangle(window),
                        origin=frame_origin(window), packet=status(),
                        scale=scale_ready(window, alias), children=view.children(window))
                    left, top = 31, 97
                    width = max(1, probe['ready']['minimum'][0] // 2)
                    height = max(1, probe['ready']['minimum'][1] // 2)
                    insets = [left, physical[2] - left - width,
                        top, physical[3] - top - height]
                    assert min(insets) >= 0, (physical, insets)
                    reserve = 'EwmhBaseStruts screen %s %d %d %d %d' % \
                        (reservation['name'], *insets)
                    restore = 'EwmhBaseStruts screen %s %d %d %d %d' % \
                        (reservation['name'], *reservation['insets'])
                    command(reserve)
                    try:
                        large = configured_form('StartupOversized')
                        actual = published_view_contained(large['window'], large['alias'], oversized=True)
                        assert actual['reservation']['insets'] == insets, actual
                        assert actual['oversized_axes'] == [0, 1], actual
                        capture('startup-readable-minimum-usable-anchor.png')
                        evidence['oversized_startup'] = dict(command=reserve,
                            form=large, actual=actual, restore_command=restore)
                        close_form(large)
                    finally:
                        command(restore)
                    after = dict(client=root_rectangle(window),
                        origin=frame_origin(window), packet=status(),
                        scale=scale_ready(window, alias), children=view.children(window))
                    assert after['client'] == before['client'] and \
                        after['origin'] == before['origin'] and \
                        after['packet']['packet'] == before['packet']['packet'] and \
                        after['children'] == before['children'] and \
                        after['scale']['resource_generation'] == before['scale']['resource_generation'], \
                        (before, after)
                    evidence['oversized_startup']['ready_parent_preserved'] = dict(before=before, after=after)
                # Continue the existing complete disclosure contract below
                # on the retained original browser and native child identities.
                same_identity()

            # The actual item contains bytes the normal 120-byte label omits.
            # Properties takes the real selected/focused filename through its
            # ordinary worker, rather than a fixture-built metadata response.
            click(list_window, 8, 8)
            focused()
            key('Home')
            # The verified real click selects this initial row. A status
            # reply can precede processing its queued X event, so wait for the
            # required packet without sending a second selection toggle.
            wait_for_predicate('actual native first filename selected',
                lambda: (v if v['focus'] == 0 and v['selected'] == [0] else None)
                    if (v := status()) else None,
                time.monotonic() + 5, process=browser)
            before_inspect = state()
            key('F2')
            wait_for_predicate('real browser inspector completes',
                lambda: (v if v and v['generation'] != before_inspect['generation'] and
                    v['availability']['ready'] else None) if (v := state()) else None,
                time.monotonic() + 8, process=browser)
            for _ in range(2): click(list_window, 8, 8, button=5)
            # Leave two unsubmitted drafts and a non-tail filter cursor.
            click(path_input)
            focused()
            view.key('u', control=True)
            for char in 'unsent-path': key('minus' if char == '-' else char)
            click(filter_input)
            focused()
            view.key('u', control=True)
            for char in 'entry': key(char)
            key('Home')
            key('Right')
            key('Right')
            command('Nop disclosure-before-observation')
            wait_for_predicate('parent publication quiet', lambda: time.monotonic() -
                max([r['time'] for r in rows(native)] or [0]) > .12,
                time.monotonic() + 3, process=browser)
            baseline, native_before = ready_state(), status()
            started = time.monotonic()
            page = open_details()
            capture('details-path-first.png')
            first = page
            page = change_page(page, 'F9')
            assert page['part'] == 1 and page['value'] == first['value']
            value, page = full_value(page)
            assert value == escaped(os.fsencode(folder)), (value, folder)
            evidence['complete_long_path'] = dict(bytes=len(os.fsencode(folder)),
                parts=page['parts'], reconstructed=value)
            at_end = page
            page = change_page(page, 'F10')
            assert page['part'] == at_end['part'] and page['value'] == at_end['value']
            # Replay a real obsolete page callback; its native FormJob ticket,
            # snapshot and page are preserved exactly.
            stale_page = detail_callbacks(started)[0]
            command(stale_page)
            producer_quiet()
            assert detail_page()['alias'] == page['alias']
            page = change_page(page, 'F9')
            assert page['part'] == at_end['part'] - 1
            page = choose(page, 'Detail 1')
            observed, page = full_value(page)
            assert observed == escaped(b'Observed name: ' + special_name), observed
            evidence['complete_observed_filename'] = observed
            capture('details-observed-filename.png')
            page = choose(page, 'Inspected name')
            observed, page = full_value(page)
            assert observed == escaped(special_name), observed
            # Last chooser field and first field both travel in actual native
            # packets. This catches focus off-by-one/selection confusion.
            page = choose(page, page['sections'][-1])
            assert page['field'] == page['sections'][-1]
            page = choose(page, 'Path')
            click(page['input'])
            focused(page['window'])
            key('Home')
            command('Nop readonly-home')
            head = input_pixels(page, label='value-home-initial')
            key('End')
            command('Nop readonly-end')
            tail = input_pixels(page, different=head, label='value-end')
            assert head != tail
            key('Left')
            key('Right')
            for char in 'dhku': view.key(char, control=True)
            key('Delete')
            key('BackSpace')
            key('x')
            key('Home')
            command('Nop readonly-edits')
            assert input_pixels(page, expected=head, label='value-home-final') == head
            key('End')
            command('Nop readonly-tail-capture')
            capture('details-path-horizontal-end.png')
            after = state()
            assert after['generation'] == baseline['generation']
            assert after['bindings'] == baseline['bindings'], (baseline, after)
            assert status()['packet'] == native_before['packet']
            same_identity()
            parent_commands = [r['text'] for r in rows(native) if r['time'] >= started and
                r['direction'] == 'out']
            assert not any(c.startswith('Echo FVWM_FILES_RENDER ' + provider + ' ') or
                c.startswith('*' + alias + ':') or any(c.startswith(
                    'SendToModule ' + alias + ' ' + verb) for verb in
                    ('SetLabel', 'ListBegin', 'ListEntry', 'ListCommit', 'ListLayout'))
                for c in parent_commands), parent_commands
            evidence['observational_parent'] = dict(before=baseline, after=after,
                native_before=native_before, native_after=status(), no_parent_commands=True)
            close_details(page)
            # The first click after closing observes the preserved visual top.
            click(list_window, 8, 8)
            top = status()
            assert top['focus'] == 6, top
            evidence['preserved_visual_top'] = top
            # Input cursor and both drafts are read through real predefined
            # browser callbacks after the observational assertions complete.
            click(filter_input)
            focused()
            key('Home')
            key('Right')
            key('Right')
            page = open_details()
            close_details(page)
            focused()
            key('x')
            submitted = time.monotonic()
            key('f', alt=True)
            def callback(operation):
                def found():
                    values = [r['text'] for r in rows(native) if r['time'] >= submitted and
                        r['direction'] == 'out' and r['text'].startswith('Echo FVWM_FILES_3 ' + provider + ' ')
                        and r['text'].split()[4] == operation]
                    return values[-1] if values else None
                return wait_for_predicate('real draft ' + operation + ' callback', found,
                    time.monotonic() + 5, process=browser)
            actual = callback('filter')
            assert base64.urlsafe_b64decode(actual.split()[5][1:] + '==') == b'enxtry', actual
            wait_for_predicate('filter submitted', lambda: state()['filter'] == 'enxtry',
                time.monotonic() + 5, process=browser)
            # Restore the real model through its public action, retaining Path.
            rpc(method='action', provider=provider, generation=state()['generation'],
                operation='filter', values={'text': ''})
            wait_for_predicate('unfiltered native list', lambda: (v['visible'] == ENTRIES if (v := ready_state()) else False),
                time.monotonic() + 8, process=browser)
            evidence['filter_cursor_and_draft'] = actual
            page = open_details()
            page = choose(page, 'Status')
            frozen_status = page['value']
            frozen_alias = page['alias']
            before_refresh = state()
            focused()
            key('F5')
            wait_for_predicate('background refresh completes', lambda: (v if v['bindings']['intent']['started'] >
                    before_refresh['bindings']['intent']['started'] else None)
                    if (v := ready_state()) else None,
                time.monotonic() + 8, process=browser)
            command('Nop frozen-details-after-refresh')
            assert detail_page()['alias'] == frozen_alias and detail_page()['value'] == frozen_status
            close_details(page)
            page = open_details()
            page = choose(page, 'Status')
            assert page['value'] != frozen_status, (frozen_status, page)
            evidence['immutable_snapshot_then_reopen'] = dict(old=frozen_status, fresh=page['value'],
                before=before_refresh, after=state())
            # Normal browse controls remain functional with a snapshot open;
            # parent replacement retires the owned disclosure.
            stale_lifetime = detail_callbacks()[-1]
            close_details(page)
            page = open_details()
            command(stale_lifetime)
            producer_quiet()
            assert detail_page()['alias'] == page['alias']
            command('SendToModule ' + page['alias'] + ' UnMap')
            command('SendToModule ' + page['alias'] + ' Map')
            # Deliver WM_DELETE_WINDOW using the same real native close path.
            close = ClientMessage()
            close.type, close.display, close.window = 33, d, page['window']
            x.XInternAtom.argtypes = [C.c_void_p, C.c_char_p, C.c_int]
            x.XInternAtom.restype = C.c_ulong
            close.message_type = x.XInternAtom(d, b'WM_PROTOCOLS', 0)
            close.format = 32
            close.data[0] = x.XInternAtom(d, b'WM_DELETE_WINDOW', 0)
            close.data[1] = 0
            assert x.XSendEvent(d, page['window'], 0, 0, C.cast(C.byref(close), C.POINTER(Event)))
            x.XSync(d, 0)
            wait_for_predicate('WM close retires details', lambda: not view.named('File browser details'),
                time.monotonic() + 5, process=browser)
            page = open_details()
            focused()
            # Reading the retained Path draft proves it survived all snapshots.
            submitted = time.monotonic()
            key('g', alt=True)
            path_callback = callback('path')
            assert base64.urlsafe_b64decode(path_callback.split()[5][1:] + '==') == b'unsent-path'
            focused()
            key('u', alt=True)
            old_parent, old_alias = window, alias
            alias_pattern = re.compile(r'FvwmFiles-' + re.escape(provider) + r'-([1-9][0-9]*)-ready')
            old_lifetime = int(alias_pattern.fullmatch(old_alias)[1])

            def form_receipt(name, require_wait=False):
                records = [json.loads(path.read_text()) for path in native.glob('module-*.json')]
                matching = [record for record in records if record['real'].endswith('/FvwmForm')
                    and record['arguments'][-1] == name]
                assert len(matching) <= 1, matching
                if not matching:
                    return None
                record = matching[0]
                stopped = native / ('stopped-%d.json' % record['proxy'])
                if not stopped.exists():
                    return None if require_wait else record
                waited = json.loads(stopped.read_text())
                assert waited['child'] == record['child'] and waited['returncode'] == 0, waited
                return dict(record, waited=waited)

            def replaced_parent():
                if view.named('File browser details'):
                    return None
                target = view.named('File browser')
                if not target:
                    return None
                name = module_alias(target)
                matched = alias_pattern.fullmatch(name)
                if matched is None or int(matched[1]) <= old_lifetime:
                    return None
                retired, created = form_receipt(old_alias, True), form_receipt(name)
                if retired is None or created is None:
                    return None
                published_view_contained(target, name)
                return dict(old=dict(window=old_parent, alias=old_alias, module=retired),
                    new=dict(window=target, alias=name, module=created),
                    numeric_xid_reused=target == old_parent)

            replacement = wait_for_predicate('new parent Form incarnation and old module Wait0',
                replaced_parent, time.monotonic() + 8, process=browser)
            evidence['parent_replacement'] = replacement
            window, alias = replacement['new']['window'], replacement['new']['alias']
            page = open_details()
            focused()
            key('Escape')
            assert browser.wait_until_exit(time.monotonic() + 5) == 0
            wait_for_predicate('parent exit retires owned details',
                lambda: not view.named('File browser details') and not view.named('File browser'),
                time.monotonic() + 5, process=core)
            replacement['new']['module'] = wait_for_predicate('new parent module Wait0 after exit',
                lambda: form_receipt(alias, True), time.monotonic() + 5, process=core)
            evidence['path_draft'] = path_callback
            evidence['stale_page_and_lifetime_callbacks'] = [stale_page, stale_lifetime]
            evidence['status'] = 'passed'
        except BaseException as failure:
            evidence['status'] = 'failed'
            evidence['failures'].append(dict(type=type(failure).__name__, message=str(failure),
                traceback=traceback.format_exc()))
            raise
        finally:
            def cleanup(label, action):
                try:
                    action()
                except BaseException as failure:
                    evidence['cleanup_failures'].append(dict(step=label,
                        type=type(failure).__name__, message=str(failure)))
            for name, peer in [('control', control), ('rpc', rpc_peer)]:
                if peer: cleanup('close ' + name, peer.close)
            if errors: cleanup('close observer', errors.close_display)
            elif client: cleanup('close observer', client.close)
            for process in reversed(runtime.processes):
                cleanup('stop ' + process.name, lambda p=process: p.close(time.monotonic() + 4))
            evidence['children_after_cleanup'] = {p.name: p.poll() for p in runtime.processes}

            def waits():
                evidence['module_cleanup'] = cleanup_receipts(runtime.root / 'native-records')
                form_waits = [m for m in evidence['module_cleanup']['modules']
                    if m['real'].endswith('/FvwmForm')]
                assert form_waits and all(m['waited']['returncode'] == 0 for m in form_waits), form_waits
            cleanup('wait actual module child receipts', waits)
            for path in runtime.root.iterdir():
                if path.is_dir() and path.name in ('native-records', 'browser-workers', 'user'):
                    cleanup('copy ' + path.name, lambda p=path: shutil.copytree(p, out / p.name))
                elif path.is_file() and (path.suffix == '.log' or path.name in ('config', 'xorg.conf')):
                    cleanup('copy ' + path.name, lambda p=path: shutil.copy2(p, out / p.name))
            if evidence['cleanup_failures'] or evidence.get('observer_unexpected_x_errors'):
                evidence['status'] = 'failed'
            atomic_json(out / 'evidence.json', evidence)
            if args.editable_continuity_only and evidence['status'] == 'passed':
                assert not evidence['cleanup_failures'], evidence['cleanup_failures']
                assert not evidence.get('observer_unexpected_x_errors'), evidence.get('observer_unexpected_x_errors')
                assert all(code is not None for code in evidence['children_after_cleanup'].values())
                print('native browser editable continuity ' + profile + ' passed: ' + str(out))
    assert not evidence['cleanup_failures'], evidence['cleanup_failures']
    assert not evidence.get('observer_unexpected_x_errors'), evidence.get('observer_unexpected_x_errors')
    assert all(code is not None for code in evidence['children_after_cleanup'].values())
    print('native browser disclosure ' + profile + ' passed: ' + str(out))


def main():
    if len(sys.argv) > 1 and sys.argv[1] == '--module-proxy':
        return module_proxy(sys.argv[2], sys.argv[3], sys.argv[4:])
    parser = argparse.ArgumentParser(description=__doc__, allow_abbrev=False)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--artifacts-dir', type=Path, required=True)
    parser.add_argument('--profiles', nargs='+', choices=PROFILES, default=list(PROFILES))
    parser.add_argument('--xorg-config', type=Path)
    parser.add_argument('--scale', choices=('1', '1.25', '2'), default='1')
    parser.add_argument('--editable-continuity-only', action='store_true',
        help='run editable drafts, pointer/paste, resize/view/scale and native byte callbacks')
    parser.add_argument('--startup-continuity', action='store_true',
        help='also check startup sizing, containment and viewport continuity')
    parser.add_argument('--startup-workarea', action='store_true',
        help='assert automatic startup against real per-monitor reservation packets')
    parser.add_argument('--startup-monitor', choices=('DUMMY0', 'DUMMY1'), default='DUMMY0',
        help='DUMMY1 adds a nonzero secondary monitor with a different primary scale')
    parser.add_argument('--startup-oversized', action='store_true',
        help='also check a genuine reserved area smaller than the readable Form minimum')
    args = parser.parse_args()
    if args.editable_continuity_only and (args.startup_continuity or args.startup_workarea or
            args.startup_monitor != 'DUMMY0' or args.startup_oversized):
        parser.error('--editable-continuity-only cannot combine with startup scenarios')
    if args.scale != '1' and not args.xorg_config:
        parser.error('--scale requires --xorg-config for real RandR millimetres')
    if args.startup_monitor == 'DUMMY1' and not (args.xorg_config and args.startup_workarea):
        parser.error('--startup-monitor DUMMY1 requires --xorg-config and --startup-workarea')
    if args.startup_oversized and not (args.startup_workarea and args.startup_continuity):
        parser.error('--startup-oversized requires --startup-workarea and --startup-continuity')
    args.artifacts_dir.mkdir(parents=True, exist_ok=True)
    run_root = Path(tempfile.mkdtemp(prefix='browser-disclosure-', dir=args.artifacts_dir.resolve()))
    for profile in args.profiles:
        run(args, profile, run_root)
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
