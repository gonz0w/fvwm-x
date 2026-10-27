#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
"""Observe in-place browser list/grid continuity on real X11 and native pipes.

The existing transparent proxies record original commands and real Form ACKs.
The existing worker proxy forwards original bytes, with one bounded private
gate/failure. No reply, selection packet, action or publication ACK is invented.
This focused checkpoint does not qualify physical displays, IMEs or assistive
technology; existing broader browser/action receipts retain their own scope.
"""
import argparse
import base64
import ctypes as C
import hashlib
import json
import os
from pathlib import Path
import shlex
import shutil
import sys
import tempfile
import time
import traceback

from desktop_live import connect_client
from geometry_transition_live import SizeHints
from inspector_live import NativeView, atomic_json
from live_x_harness import HarnessRuntime, start_display_server, wait_for_predicate
from mfl_request_live import frame
from mfl_stream_live import Connection
from native_bindings_live import module_proxy, rows, worker_proxy
from native_capture import NativeCapture
from selection_live import Event
from session_x_errors import SessionXErrors

PROFILES = ('slate-dark', 'dune-light', 'classic-large')
ENTRIES = 4096


class ClassHint(C.Structure):
    _fields_ = [('name', C.c_void_p), ('class_', C.c_void_p)]


def cleanup_receipts(directory):
    """Wait for actual proxy-owned child Wait receipts before deleting files."""
    started, pending = time.monotonic(), []

    def finished():
        pending.clear()
        result = []
        for path in directory.glob('module-*.json'):
            record = json.loads(path.read_text())
            stopped = directory / ('stopped-%d.json' % record['proxy'])
            if not stopped.exists():
                pending.append(record)
                continue
            waited = json.loads(stopped.read_text())
            assert waited['child'] == record['child'] and waited['returncode'] is not None
            result.append(dict(record, waited=waited))
        return {'modules': result, 'seconds': time.monotonic() - started} if not pending else None

    return wait_for_predicate('real module child wait receipts', finished, started + 5)


def run(args, profile, run_root):
    source, build = Path(__file__).resolve().parents[2], args.build.resolve()
    out = run_root / profile
    out.mkdir()
    inputs = ['tests/live_x/browser_continuity_live.py', 'tests/live_x/native_bindings_live.py',
        'modules/FvwmForm/FvwmForm.c',
        'modules/FvwmForm/form_list.c', 'bin/FvwmPrompt/cmd/fvwmfilebrowser/native.go',
        'bin/FvwmPrompt/cmd/fvwmfilebrowser/view.go']
    evidence = dict(status='running', profile=profile, screenshots=[], failures=[], cleanup_failures=[],
        source_sha256={name: hashlib.sha256((source / name).read_bytes()).hexdigest() for name in inputs},
        binary_sha256={str(p.relative_to(build)): hashlib.sha256(p.read_bytes()).hexdigest()
            for p in (build / 'fvwm3', build / 'modules/FvwmForm/FvwmForm',
                      build / 'bin/FvwmPrompt/fvwm-file-browser')},
        limits={'entries': ENTRIES, 'native_children': 64, 'worker_gate_seconds': 1.5,
                'physical_display_or_assistive_technology_qualified': False,
                'automatic_module_scaling_qualified': False}, display_geometry='1920x1800x24')
    with HarnessRuntime(prefix='fvwm-browser-continuity-') as runtime:
        client = errors = core = browser = control = rpc_peer = None
        try:
            _, display = start_display_server(runtime, 'Xvfb',
                ['Xvfb', '-screen', '0', '1920x1800x24', '-noreset'],
                'xserver.log', time.monotonic() + 10)
            modules, user, native, jobs, folder = [runtime.root / name for name in
                ('modules', 'user', 'native-records', 'browser-workers', 'files')]
            for path in (modules, user, native, jobs, folder):
                path.mkdir(mode=0o700)
            for index in range(ENTRIES):
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
                '\nStyle FvwmFiles-* PositionPlacement 20 60\n'
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
            wait_for_predicate('browser continuity MFL', endpoint.exists, time.monotonic() + 12, process=core)
            control, rpc_peer = Connection(endpoint), Connection(endpoint)
            control.record()
            rpc_peer.record()
            control.send(b'set echo\n')
            control.echo('browser-continuity-ready')
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
            private_browser = runtime.root / 'browser-image'
            missing_program = runtime.root / 'missing-custom-program'
            marker = runtime.root / 'must-not-launch'

            def start_browser(name):
                replacement = runtime.root / 'browser-replacement'
                shutil.copy2(real_browser, replacement)
                replacement.replace(private_browser)
                arguments = [str(private_browser), '--socket', str(endpoint),
                    '--dir', str(folder), '--view', 'list', '--watch', 'off',
                    '--opener', '/bin/true', '--terminal', '/bin/true']
                for index in range(1, 5):
                    arguments.extend(['--action', 'Missing %d=%s' % (index, missing_program)])
                return runtime.start(name, arguments, name + '.log', env)

            browser = start_browser('browser')
            listed = wait_for_predicate('initial browser provider registered',
                lambda: rpc()['providers'], time.monotonic() + 8, process=browser)
            provider = listed[0]['provider']
            client = connect_client(display)
            errors = SessionXErrors(client, evidence).install()
            view = NativeView(client, errors, core)
            x, d, pointer = client.x, client.display, view.pointer
            for name, arguments, result in [
                ('XSetInputFocus', [C.c_void_p, C.c_ulong, C.c_int, C.c_ulong], C.c_int),
                ('XGetClassHint', [C.c_void_p, C.c_ulong, C.POINTER(ClassHint)], C.c_int),
                ('XTranslateCoordinates', [C.c_void_p, C.c_ulong, C.c_ulong, C.c_int, C.c_int,
                    C.POINTER(C.c_int), C.POINTER(C.c_int), C.POINTER(C.c_ulong)], C.c_int),
                ('XGetWMNormalHints', [C.c_void_p, C.c_ulong, C.POINTER(SizeHints),
                    C.POINTER(C.c_long)], C.c_int),
                ('XCheckTypedWindowEvent', [C.c_void_p, C.c_ulong, C.c_int, C.POINTER(Event)], C.c_int),
            ]:
                getattr(x, name).argtypes, getattr(x, name).restype = arguments, result

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
            wrapper = runtime.root / 'browser-worker-wrapper'
            script(wrapper, ['--browser-worker', real_browser, jobs])
            wrapper.replace(private_browser)

            def geometry(target):
                a = view.attributes(target)
                assert a and a.map_state == 2
                return [a.x, a.y, a.width, a.height]

            children = sorted(view.children(window))
            assert 3 <= len(children) <= 64, children
            boxes = {child: geometry(child) for child in children}
            list_window = max(children, key=lambda child: boxes[child][3])
            # Input widths 24/32 exceed every configured button caption here;
            # custom-action rows can follow Path, so last-row guessing is wrong.
            inputs = sorted((child for child in children if child != list_window),
                key=lambda child: boxes[child][2], reverse=True)[:2]
            filter_y, path_y = sorted(boxes[child][1] for child in inputs)
            filter_row = sorted((g[0], child) for child, g in boxes.items() if g[1] == filter_y)
            path_row = sorted((g[0], child) for child, g in boxes.items() if g[1] == path_y)
            filter_input, path_input = filter_row[0][1], path_row[0][1]
            evidence['native_identity'] = dict(window=window, alias=alias, children=children,
                filter_input=filter_input, path_input=path_input, list=list_window)
            button_lines = [r['text'] for r in rows(native) if r['direction'] == 'out' and
                r['text'].startswith('*' + alias + ': Button continue ')]
            shortcuts = [line.rsplit(' ', 1)[-1] for line in button_lines]
            expected = {'Alt-F', 'Alt-C', 'Alt-S', 'Alt-R', 'Alt-A', 'Alt-D',
                'Alt-H', 'Alt-U', 'Alt-B', 'Alt-G', 'Alt-X', 'Alt-1', 'Alt-2', 'Alt-3', 'Alt-4',
                'F2', 'F3', 'F4', 'F5', 'F6', 'F7', 'F8', '^M', '^['}
            assert len(shortcuts) == len(set(shortcuts)) and set(shortcuts) == expected, button_lines
            evidence['configured_unique_keyboard_actions'] = button_lines
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

            def status(target=alias, pending=False):
                def observed():
                    token = 'bc' + os.urandom(8).hex()
                    control.send(('SendToModule %s ListStatus picks %s selection\n' % (target, token)).encode())
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

            def focused():
                x.XSetInputFocus(d, window, 1, 0)
                x.XSync(d, 0)
                assert view.focus() == window

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
                box = geometry(target)
                rx, ry, child = C.c_int(), C.c_int(), C.c_ulong()
                assert x.XTranslateCoordinates(d, target, client.root,
                    box[2]//2 if px is None else px, box[3]//2 if py is None else py,
                    C.byref(rx), C.byref(ry), C.byref(child))
                pointer.motion(rx.value, ry.value)
                assert pointer.x.XTestFakeButtonEvent(d, button, 1, 0)
                assert pointer.x.XTestFakeButtonEvent(d, button, 0, 0)
                x.XSync(d, 0)

            def capture(name):
                previous = os.environ.get('DISPLAY')
                os.environ['DISPLAY'] = display
                try:
                    path = Path(NativeCapture(build / 'bin/fvwm-desktop-capture').screenshot(out)
                        .removeprefix('Screenshot saved: '))
                    path.replace(out / name)
                    evidence['screenshots'].append(name)
                finally:
                    if previous is None: os.environ.pop('DISPLAY', None)
                    else: os.environ['DISPLAY'] = previous

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

            def toggle(allow_busy=False, on_admitted=None):
                before, before_state, started = status(), state(), time.monotonic()
                wanted = 'grid' if before_state['view'] == 'list' else 'list'
                focused()
                key('F7')

                def admitted():
                    value = state()
                    return value if value['view'] == wanted and (allow_busy or
                        value['availability']['ready']) and \
                        value['bindings']['projection']['state'] == 'current' else None

                after_state = wait_for_predicate('real ACK before browser publishes ' + wanted,
                    admitted, time.monotonic() + 8, process=browser)
                after = status()
                assert after['generation'] == before['generation'] + 1, (before, after)
                assert after['packet'][8:] == before['packet'][8:], (before, after)
                if on_admitted is not None:
                    on_admitted()
                same_identity()
                actual = [r['text'] for r in rows(native) if r['time'] >= started and r['direction'] == 'out']
                layout = 'SendToModule %s ListLayout picks %d %d 18 %d' % (
                    alias, before['generation'], after['generation'], 8 if wanted == 'grid' else 1)
                assert actual.count(layout) == 1, actual
                assert not any(c.startswith(('Module FvwmForm ' + alias, '*' + alias + ':')) or
                    c.startswith('SendToModule ' + alias + ' List' + verb)
                    for c in actual for verb in ('Begin', 'Entry', 'Commit')), actual
                assert any(c.startswith('Echo FVWM_FORM_LIST ' + provider + '_') and
                    (' %d 0 0 x' % after['generation']) in c for c in actual), actual
                result = dict(before=before, after=after, state=after_state,
                    command=layout, same_form_inputs_list_and_child_set=True, remap_events=[])
                evidence.setdefault('switches', []).append(result)
                return result

            def wait_packet(focus, selected=None):
                def observed():
                    value = status()
                    return value if value['focus'] == focus and (selected is None or
                        value['selected'] == selected) else None
                return wait_for_predicate('native focus/selection packet', observed,
                    time.monotonic() + 5, process=browser)

            capture('continuity-list-4096.png')
            click(filter_input)
            focused()
            key('Tab')
            key('Home')
            for _ in range(3): key('Down', shift=True)
            key('space')
            range_before = wait_packet(3, [0, 1, 2])
            toggle()
            key('Left', shift=True)
            range_after = wait_packet(2, [0, 1, 2])
            evidence['range_anchor_survives'] = dict(before=range_before, after=range_after,
                distinguishing_invariant='index 3 remains unselected after Shift-Left from focus 3')
            key('d', alt=True)
            wait_packet(2, [])
            evidence['alt_clear_selection_from_list'] = True
            capture('continuity-grid-range.png')
            key('Home')
            key('Down')
            columns = wait_for_predicate('grid keyboard column stride',
                lambda: (s if 2 <= s['focus'] <= 8 else None) if (s := status()) else None,
                time.monotonic() + 5, process=browser)['focus']
            key('Home')
            for _ in range(6): click(list_window, 8, 8, button=5)
            offscreen = toggle()
            assert offscreen['after']['focus'] == 0
            click(list_window, 8, 8)
            top_list = wait_packet(18 * columns)
            evidence['top_grid_to_list'] = dict(old_columns=columns, wheel_visual_rows=18,
                expected_top=18*columns, observed=top_list,
                observation='first-cell click after switch changes focus/selection and range anchor')
            key('Home')
            for _ in range(6): click(list_window, 8, 8, button=5)
            offscreen = toggle()
            assert offscreen['after']['focus'] == 0
            click(list_window, 8, 8)
            def top_in_first_row():
                value = status()
                return value if 0 < value['focus'] <= 18 and 18-value['focus'] < columns and \
                    value['focus'] % columns == 0 else None
            top_grid = wait_for_predicate('old top identity remains in first grid row',
                top_in_first_row, time.monotonic() + 5, process=browser)
            evidence['top_list_to_grid'] = dict(old_top=18, columns=columns, observed=top_grid,
                observation='first-cell click after switch changes focus/selection and range anchor')

            def action(operation, values):
                return rpc(method='action', provider=provider, generation=state()['generation'],
                    operation=operation, values=values)

            first_page = rpc(scope='items', provider=provider, offset=0, limit=1)
            known_pages = [first_page]
            for offset in (50, 100):
                page = rpc(scope='items', provider=provider, generation=first_page['generation'],
                    offset=offset, limit=1)
                assert page['generation'] == first_page['generation'] and page['offset'] == offset
                known_pages.append(page)
            known = [page['items'][0] for page in known_pages]
            assert [base64.b64decode(item['name_bytes']) for item in known] == \
                [b'entry-0000', b'entry-0050', b'entry-0100']
            evidence['generation_bound_target_pages'] = known_pages
            action('selection', {'mode': 'replace', 'ids': [i['id'] for i in known]})
            action('filter', {'text': 'entry-00'})
            hidden_before = rpc(scope='selection', provider=provider)
            assert hidden_before['total'] == 3 and state()['visible'] == 100
            for _ in range(4):
                toggle()
                selected = rpc(scope='selection', provider=provider)
                assert {i['id'] for i in selected['items']} == {i['id'] for i in known}, selected
                assert len(status()['selected']) == 2
            evidence['hidden_multiple_selection'] = hidden_before

            outer = geometry(window)
            minimum, supplied = SizeHints(), C.c_long()
            assert x.XGetWMNormalHints(d, window, C.byref(minimum), C.byref(supplied))
            width, height = max(minimum.min_width, outer[2]+80), max(minimum.min_height, outer[3]+60)
            x.XResizeWindow(d, window, width, height)
            x.XSync(d, 0)
            wait_for_predicate('native resized Form', lambda: geometry(window)[2:] == [width, height],
                time.monotonic() + 5, process=browser)
            resized_switch = toggle()
            evidence['resize_then_f7'] = dict(size=[width, height], switch=resized_switch)
            capture('continuity-resized.png')

            def type_text(target, text):
                click(target)
                focused()
                view.key('u', control=True)
                symbols = {' ': 'space', '/': 'slash', '-': 'minus', '_': 'underscore',
                    '.': 'period', ';': 'semicolon', '$': 'dollar', '%': 'percent',
                    '[': 'bracketleft', ']': 'bracketright'}
                for char in text:
                    key(symbols.get(char, char.lower()), shift=char in '_$%' or char.isupper())

            def callback(operation, after):
                prefix = 'Echo FVWM_FILES_3 ' + provider + ' '
                def observed():
                    found = [r['text'] for r in rows(native) if r['time'] >= after and
                        r['direction'] == 'out' and r['text'].startswith(prefix) and
                        r['text'].split()[4] == operation]
                    return found[-1] if found else None
                text = wait_for_predicate('actual browser ' + operation + ' callback', observed,
                    time.monotonic() + 5, process=browser)
                return text, text.split()[5]

            path_draft = str(runtime.root / 'unsent folder ; $[literal] 100%')
            type_text(path_input, path_draft)
            type_text(filter_input, 'entry-00')
            if state()['view'] != 'list':
                toggle()
            previous_geometry = geometry(window)
            fixed_height = previous_geometry[3] - geometry(list_window)[3]
            hints, supplied = SizeHints(), C.c_long()
            assert x.XGetWMNormalHints(d, window, C.byref(hints), C.byref(supplied))
            x.XResizeWindow(d, window, hints.min_width, hints.min_height)
            x.XSync(d, 0)
            wait_for_predicate('one-list-row legal minimum',
                lambda: geometry(window)[2:] == [hints.min_width, hints.min_height],
                time.monotonic() + 5, process=browser)
            wait_for_predicate('native children reflow at legal minimum',
                lambda: geometry(list_window)[2:] ==
                    [hints.min_width-20, hints.min_height-fixed_height],
                time.monotonic() + 5, process=browser)
            before_refusal, before_boxes = status(), contained()
            started = time.monotonic()
            key('F7')

            def refused_layout_observed():
                actual = [r['text'] for r in rows(native) if r['time'] >= started and r['direction'] == 'out']
                layout = 'SendToModule %s ListLayout picks %d %d 18 8' % (
                    alias, before_refusal['generation'], before_refusal['generation']+1)
                unchanged_ack = any(c.startswith('Echo FVWM_FORM_LIST ' + provider + '_') and
                    c.split()[2].endswith('_layout') and
                    c.split()[3:6] == [str(before_refusal['generation']), '0', '0'] for c in actual)
                guidance = [c for c in actual if c.startswith('SendToModule ' + alias + ' SetLabel status ')
                    and 'enlarge' in c.lower() and 'retry' in c.lower()]
                return dict(command=layout, status_commands=guidance) if layout in actual and \
                    unchanged_ack and guidance else None

            refused_layout = wait_for_predicate('real unchanged layout ACK and enlarge/retry guidance',
                refused_layout_observed, time.monotonic() + 8, process=browser)
            assert state()['view'] == 'list' and status()['packet'] == before_refusal['packet']
            assert contained() == before_boxes
            same_identity()
            capture('continuity-small-layout-refused.png')
            x.XResizeWindow(d, window, previous_geometry[2], previous_geometry[3])
            x.XSync(d, 0)
            wait_for_predicate('real enlargement before layout retry',
                lambda: geometry(window)[2:] == previous_geometry[2:],
                time.monotonic() + 5, process=browser)
            retried = toggle()
            contained()
            evidence['small_layout_refusal_retry'] = dict(before=before_refusal,
                minimum=[hints.min_width, hints.min_height], refusal=refused_layout, retry=retried,
                both_drafts_observed_later=True)
            for _ in range(4): toggle()
            key('1')
            started = time.monotonic()
            key('f', alt=True)
            filter_callback, field = callback('filter', started)
            assert base64.urlsafe_b64decode(field[1:] + '==') == b'entry-001', filter_callback
            wait_for_predicate('submitted filter draft after repeated switches',
                lambda: (s if s['filter'] == 'entry-001' and s['visible'] == 10 else None)
                if (s := state()) else None, time.monotonic() + 8, process=browser)
            click(path_input)
            before_reverse = state()['reverse']
            key('r', alt=True)
            wait_for_predicate('Alt-R from Path preserves its unsubmitted draft',
                lambda: (s if s['reverse'] != before_reverse else None) if (s := state()) else None,
                time.monotonic() + 6, process=browser)
            started = time.monotonic()
            key('g', alt=True)
            path_callback, field = callback('path', started)
            assert base64.urlsafe_b64decode(field[1:] + '==') == path_draft.encode(), path_callback
            evidence['unsubmitted_drafts_and_input_focus'] = dict(filter_callback=filter_callback,
                path_callback=path_callback, filter_bytes=b'entry-001'.hex(), path_bytes=path_draft.encode().hex(),
                focus_proof='typing 1 after four F7 operations edits the retained filter input',
                alt_actions=['Alt-F from filter', 'Alt-R and Alt-G from Path'])

            def unavailable():
                value = state()
                return value if not value['busy'] and not value['availability']['ready'] else None

            lost = wait_for_predicate('missing draft folder reports failure honestly', unavailable,
                time.monotonic() + 8, process=browser)
            assert lost['loaded'] == ENTRIES and not lost['progress']['known'], lost
            same_identity()
            capture('continuity-unavailable.png')
            focused()
            key('F5')
            recovered = wait_for_predicate('F5 recovers retained browser', ready_state,
                time.monotonic() + 8, process=browser)
            same_identity()
            evidence['failure_recovery'] = dict(failed=lost, recovered=recovered)
            capture('continuity-recovered.png')

            # These selected targets still exist. The configured executable is
            # absent, so the real worker must report a correlated failure at
            # the launch boundary rather than claiming a started handoff.
            assert not missing_program.exists() and all((folder / os.fsdecode(
                base64.b64decode(item['name_bytes']))).is_file() for item in known)
            custom = action('custom', {'index': 0})
            work_id = custom['work']['id']

            def custom_failed():
                value = state()
                return value if value.get('work') and value['work']['id'] == work_id and \
                    value['work']['status'] == 'failed' and not value['busy'] else None

            failed_custom = wait_for_predicate('correlated missing-program launch failure', custom_failed,
                time.monotonic() + 8, process=browser)
            assert failed_custom['work']['operation'] == 'custom' and not marker.exists()
            focused()
            key('F5')
            wait_for_predicate('refresh after correlated custom failure', ready_state,
                time.monotonic() + 8, process=browser)
            click(filter_input)
            key('Tab')
            started = time.monotonic()
            key('1', alt=True)
            native_custom, _ = callback('custom', started)
            assert native_custom.split()[5] == '0', native_custom
            alt_failed = wait_for_predicate('Alt-1 from list reaches real unavailable application',
                unavailable, time.monotonic() + 8, process=browser)
            assert not marker.exists()
            focused()
            key('F5')
            wait_for_predicate('refresh after Alt-1 custom failure', ready_state,
                time.monotonic() + 8, process=browser)
            evidence['missing_custom_program'] = dict(admitted=custom, correlated_failure=failed_custom,
                native_alt_callback=native_custom, native_failure=alt_failed, output_marker_absent=True)

            action('filter', {'text': ''})
            (folder / 'entry-0000').unlink()
            (folder / 'entry-0001').write_text('changed metadata')
            focused()
            key('F5')
            updated = wait_for_predicate('vanished entry reconciled',
                lambda: (s if s and s['loaded'] == ENTRIES-1 and s['selected'] == 2 else None)
                if (s := ready_state()) else None, time.monotonic() + 10, process=browser)
            same_identity()
            assert known[0]['id'] not in {i['id'] for i in rpc(scope='selection', provider=provider)['items']}
            evidence['incremental_vanished_entry'] = updated

            # Malformed/stale layouts on the real browser must be atomic.
            before = status()
            bad = [f"{before['generation']-1} {before['generation']+1} 18 8",
                f"{before['generation']} {before['generation']} 18 8",
                f"{before['generation']} {before['generation']+1} 512 8",
                f"{before['generation']} {before['generation']+1} 4097 8",
                f"{before['generation']} {before['generation']+1} 18 9",
                f"{before['generation']} {before['generation']+1} 18 8 trailing"]
            before_boxes = {child: geometry(child) for child in children}
            for arguments in bad:
                command('SendToModule ' + alias + ' ListLayout picks ' + arguments)
                assert status()['packet'] == before['packet']
                assert {child: geometry(child) for child in children} == before_boxes
            evidence['refused_browser_layouts'] = bad
            same_identity()

            # Pending-update refusal uses an independent prepared Form, since
            # consuming a browser-owned transaction serial would desynchronize
            # that real owner. It still uses the production command and ACK.
            primitive = 'ContinuityCollection'
            for line in ['Title "Continuity collection"', 'Line left', 'List picks 40 4',
                    'ListViewport 18 1', 'ListVersioned', *['ListItem off "entry %d" file' % i for i in range(32)]]:
                control.send(('*' + primitive + ': ' + line + '\n').encode())
            command('Module FvwmForm ' + primitive)
            small = view.wait('Continuity collection')
            original = status(primitive)
            command('SendToModule ' + primitive + ' ListBegin picks 1 2')
            pending = status(primitive, pending=True)
            command('SendToModule ' + primitive + ' ListLayout picks 1 3 18 8')
            refused = status(primitive, pending=True)
            assert pending['pending'] == refused['pending'] == 2
            assert refused['packet'] == original['packet']
            command('SendToModule ' + primitive + ' ListAbort picks 2')
            command('SendToModule ' + primitive + ' ListLayout picks 1 3 18 8')
            admitted = status(primitive)
            assert admitted['generation'] == 3 and admitted['packet'][8:] == original['packet'][8:]
            assert view.named('Continuity collection') == small
            evidence['pending_layout_refusal'] = dict(before=original, pending=pending, refused=refused, admitted=admitted)
            command('KillModule FvwmForm ' + primitive)

            # Delay one genuine worker request while F7 changes only layout.
            (jobs / 'hold-scan').touch()
            (jobs / 'held-scan.json').unlink(missing_ok=True)
            focused()
            key('F5')
            wait_for_predicate('owned actual scan reaches bounded gate',
                (jobs / 'held-scan.json').exists, time.monotonic() + 1, process=browser)
            held = json.loads((jobs / 'held-scan.json').read_text())
            released = {}

            def release_actual_worker():
                released['seconds'] = time.monotonic()-held['started']
                (jobs / ('release-' + held['token'])).touch()
                assert released['seconds'] < 1.5, held

            busy_switch = toggle(allow_busy=True, on_admitted=release_actual_worker)
            wait_for_predicate('genuine scan completes after layout switch', ready_state,
                time.monotonic() + 8, process=browser)
            evidence['layout_during_refresh'] = dict(held=held, release=released, switch=busy_switch)
            same_identity()

            old_provider, old_alias = provider, alias
            old_callback = next(r['text'] for r in reversed(rows(native)) if r['direction'] == 'out'
                and r['text'].startswith('Echo FVWM_FILES_3 ' + provider + ' ') and ' view ' in r['text'])
            focused()
            key('Escape')
            assert browser.wait_until_exit(time.monotonic() + 5) == 0
            wait_for_predicate('retired browser Form closes', lambda: not view.named('File browser'),
                time.monotonic() + 5, process=core)
            wait_for_predicate('retired browser provider removed', lambda: not rpc()['providers'],
                time.monotonic() + 5, process=core)
            browser = start_browser('browser-successor')
            successor_window = view.wait('File browser')
            listed = wait_for_predicate('replacement provider registered',
                lambda: rpc()['providers'], time.monotonic() + 8, process=browser)
            provider = listed[0]['provider']
            assert provider != old_provider
            successor = wait_for_predicate('replacement browser full data', ready_state,
                time.monotonic() + 10, process=browser)
            assert successor['loaded'] == ENTRIES-1
            command(old_callback)
            still_current = ready_state()
            assert still_current and still_current['view'] == successor['view']
            assert view.named('File browser') == successor_window
            evidence['producer_teardown_restart'] = dict(old_provider=old_provider, old_alias=old_alias,
                successor=successor, old_callback_refused=old_callback)
            capture('continuity-successor.png')
            x.XSetInputFocus(d, successor_window, 1, 0)
            key('Escape')
            assert browser.wait_until_exit(time.monotonic() + 5) == 0
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
            cleanup('wait actual module child receipts', waits)
            for path in runtime.root.iterdir():
                if path.is_dir() and path.name in ('native-records', 'browser-workers', 'user'):
                    cleanup('copy ' + path.name, lambda p=path: shutil.copytree(p, out / p.name))
                elif path.is_file() and (path.suffix == '.log' or path.name == 'config'):
                    cleanup('copy ' + path.name, lambda p=path: shutil.copy2(p, out / p.name))
            if evidence['cleanup_failures'] or evidence.get('observer_unexpected_x_errors'):
                evidence['status'] = 'failed'
            atomic_json(out / 'evidence.json', evidence)
    assert not evidence['cleanup_failures'], evidence['cleanup_failures']
    assert not evidence.get('observer_unexpected_x_errors'), evidence.get('observer_unexpected_x_errors')
    assert all(code is not None for code in evidence['children_after_cleanup'].values())
    print('native browser continuity ' + profile + ' passed: ' + str(out))


def main():
    if len(sys.argv) > 1 and sys.argv[1] == '--module-proxy':
        return module_proxy(sys.argv[2], sys.argv[3], sys.argv[4:])
    if len(sys.argv) == 4 and sys.argv[1] == '--browser-worker':
        return worker_proxy(sys.argv[2], sys.argv[3], browser=True)
    parser = argparse.ArgumentParser(description=__doc__, allow_abbrev=False)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--artifacts-dir', type=Path, required=True)
    parser.add_argument('--profiles', nargs='+', choices=PROFILES, default=list(PROFILES))
    args = parser.parse_args()
    args.artifacts_dir.mkdir(parents=True, exist_ok=True)
    run_root = Path(tempfile.mkdtemp(prefix='browser-continuity-', dir=args.artifacts_dir.resolve()))
    for profile in args.profiles:
        run(args, profile, run_root)
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
