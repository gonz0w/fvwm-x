#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or any later version.
"""Bounded native content-layout checkpoint in the shipped desktop profiles.

Uses actual Buttons rectangle expansion, Form ListStatus and browser provider
receipts. No product probe or diagnostic protocol is added. Capture files show
actual native windows. CPU/RSS and elapsed times are observations, not gates.
"""
import argparse
import base64
import ctypes as C
import hashlib
import json
import os
from pathlib import Path
import shutil
import signal
import sys
import time

from desktop_live import connect_client
from geometry_transition_live import SizeHints
from inspector_live import NativeView
from live_x_harness import HarnessRuntime, start_display_server, wait_for_predicate
from mfl_stream_live import Connection
from native_capture import NativeCapture
from session_x_errors import SessionXErrors

PROFILES = ('slate-dark', 'dune-light', 'classic-large')
ENTRIES = 4096


class ClassHint(C.Structure):
    _fields_ = [('name', C.c_void_p), ('class_', C.c_void_p)]


def process_sample(pid):
    """Read only an owned process's Linux counters; report absence explicitly."""
    try:
        fields = (Path('/proc') / str(pid) / 'stat').read_text().rsplit(')', 1)[1].split()
        status = (Path('/proc') / str(pid) / 'status').read_text().splitlines()
        memory = {line.split(':', 1)[0]: int(line.split()[1])
                  for line in status if line.startswith(('VmRSS:', 'VmHWM:'))}
        return dict(pid=pid, cpu_ticks=int(fields[11]) + int(fields[12]),
                    clock_ticks_per_second=os.sysconf('SC_CLK_TCK'),
                    rss_kib=memory.get('VmRSS'), high_water_kib=memory.get('VmHWM'))
    except (OSError, ValueError, IndexError) as error:
        return dict(pid=pid, unavailable=type(error).__name__)


def owned_module_pid(core_pid, executable, alias):
    """Inspect only direct children owned by this fixture's WM, without pgrep."""
    try:
        children = (Path('/proc') / str(core_pid) / 'task' / str(core_pid) /
                    'children').read_text().split()
        assert len(children) <= 256
        for value in children:
            command = (Path('/proc') / value / 'cmdline').read_bytes().split(b'\0')
            if command and Path(os.fsdecode(command[0])).name == executable and any(
                    item == alias.encode() for item in command[1:]):
                return int(value)
    except (OSError, ValueError):
        pass
    return None


def run(args, profile):
    source, build = Path(__file__).resolve().parents[2], args.build.resolve()
    assets = source / 'default-config'
    out = args.artifacts_dir.resolve() / profile
    out.mkdir(parents=True, exist_ok=False)
    evidence = dict(status='running', profile=profile, entries=ENTRIES,
                    screenshots=[], latency_ms=[], process_observations={})
    started = time.monotonic()
    with HarnessRuntime(prefix='fvwm-native-layout-') as runtime:
        connection = query = client = errors = pointer = core = None
        try:
            _, display = start_display_server(runtime, 'Xvfb',
                ['Xvfb', '-screen', '0', '1440x1200x24', '-noreset'],
                'xserver.log', time.monotonic() + 8)
            modules, user = runtime.root / 'modules', runtime.root / 'user'
            modules.mkdir()
            user.mkdir()
            for directory in (build / 'modules').iterdir():
                executable = directory / directory.name
                if executable.is_file():
                    (modules / directory.name).symlink_to(executable)
            (modules / 'FvwmDesktop').symlink_to(build / 'bin/FvwmPrompt/FvwmDesktop')
            palette, appearance = ('dune', 'light') if profile == 'dune-light' else ('slate', 'dark')
            (user / 'local.config').write_text('*FvwmDesktop: palette ' + palette +
                '\n*FvwmDesktop: appearance ' + appearance +
                '\n*FvwmDesktop: motion Off\n*FvwmDesktop: transparency Off\n')
            desktop = (assets / 'desktop.fvwm').read_text()
            assert '+ I *SlateBar: Layout content' in desktop
            # The shipped bar deliberately cannot be resized/focused by users.
            # Enable those test entry points while retaining all shipped leaves,
            # weights, font, colors, actions and the actual layout declaration.
            desktop = desktop.replace('+ I Module FvwmButtons SlateBar',
                '+ I *SlateBar: KeyboardNavigation 1\n+ I Module FvwmButtons SlateBar')
            body = (assets / 'config').read_text().replace(
                '$[FVWM_DATADIR]/default-config', str(assets)).replace(
                'Read "$[infostore.slate_root]/desktop.fvwm"', desktop)
            if profile == 'classic-large':
                body = body.replace('Sans:size=10:antialias=True',
                    'DejaVu Sans Mono:size=18:antialias=True')
            legacy = ('*FvwmFormDefault: Fore wheat\n*FvwmFormDefault: Back navy\n'
                '*FvwmFormDefault: ItemFore black\n*FvwmFormDefault: ItemBack ivory\n'
                if profile == 'classic-large' else '')
            config = runtime.root / 'config'
            config.write_text('ModulePath ' + str(modules) + '\nRead ' +
                str(source / 'fvwm/ConfigFvwmDefaults') + '\n' + body + '\n' + legacy +
                'Style SlateBar VariableSize, VariablePosition, ClickToFocus\n'
                'Style FvwmFiles-* PositionPlacement 0 80\n'
                'Style LayoutCollection PositionPlacement 0 80\n'
                'AddToFunc StartFunction I Module FvwmMFL\n')
            shutil.copy2(config, out / 'generated.config')
            evidence['configuration_sha256'] = {
                name: hashlib.sha256((assets / name).read_bytes()).hexdigest()
                for name in ('config', 'desktop.fvwm', 'palettes.json')}
            evidence['fixture_overrides'] = ['bar keyboard navigation and variable size/focus',
                'collection placement', 'classic-large: 18pt font and legacy Form colors']
            evidence['font'] = ('xft:DejaVu Sans Mono:size=18:antialias=True'
                if profile == 'classic-large' else 'xft:Sans:size=10:antialias=True')
            env = runtime.environment({'DISPLAY': display, 'HOME': str(runtime.root),
                'FVWM_USERDIR': str(user), 'FVWM_DESKTOP_ROOT': str(assets),
                'FVWMMFL_SOCKET_PATH': str(runtime.root),
                'XDG_DATA_HOME': str(runtime.root / 'data'),
                'XDG_DATA_DIRS': str(runtime.root / 'absent')})
            core = runtime.start('core', [str(build / 'fvwm3'), '-d', display,
                '-f', str(config)], 'core.log', env)
            endpoint = runtime.root / ('fvwm_mfl_' + display.replace(':', '_') + '.sock')
            wait_for_predicate('native layout MFL', endpoint.exists,
                time.monotonic() + 8, process=core)
            connection = Connection(endpoint)
            connection.record()
            connection.send(b'set echo\n')
            connection.echo('layout-started')
            query = Connection(endpoint)
            query.record()
            client = connect_client(display)
            errors = SessionXErrors(client, evidence).install()
            view = NativeView(client, errors, core)
            pointer, x, d = view.pointer, client.x, client.display
            for name, arguments, result in [
                ('XSetInputFocus', [C.c_void_p, C.c_ulong, C.c_int, C.c_ulong], C.c_int),
                ('XGetClassHint', [C.c_void_p, C.c_ulong, C.POINTER(ClassHint)], C.c_int),
                ('XTranslateCoordinates', [C.c_void_p, C.c_ulong, C.c_ulong,
                    C.c_int, C.c_int, C.POINTER(C.c_int), C.POINTER(C.c_int),
                    C.POINTER(C.c_ulong)], C.c_int),
                ('XFlush', [C.c_void_p], C.c_int),
                ('XGetWMNormalHints', [C.c_void_p, C.c_ulong,
                    C.POINTER(SizeHints), C.POINTER(C.c_long)], C.c_int),
            ]:
                getattr(x, name).argtypes = arguments
                getattr(x, name).restype = result
            serial = 0

            def command(text):
                connection.send((text + '\n').encode())
                connection.echo('layout-command')

            def receipt(prefix):
                deadline = time.monotonic() + 5
                while time.monotonic() < deadline:
                    text = connection.record().get('echo', {}).get('message', '')
                    if text.startswith(prefix):
                        return text
                raise AssertionError('missing native receipt ' + prefix)

            def alias(window):
                hint = ClassHint()
                assert errors.read(window, 20, lambda: x.XGetClassHint(d, window, C.byref(hint)))
                try:
                    return C.string_at(hint.name).decode()
                finally:
                    if hint.name: x.XFree(hint.name)
                    if hint.class_: x.XFree(hint.class_)

            def capture(name):
                previous = os.environ.get('DISPLAY')
                os.environ['DISPLAY'] = display
                try:
                    path = Path(NativeCapture(build / 'bin/fvwm-desktop-capture').screenshot(
                        out).removeprefix('Screenshot saved: '))
                    path.rename(out / name)
                    evidence['screenshots'].append(name)
                finally:
                    if previous is None: os.environ.pop('DISPLAY', None)
                    else: os.environ['DISPLAY'] = previous

            def focus(window):
                x.XSetInputFocus(d, window, 1, 0)
                x.XSync(d, 0)
                wait_for_predicate('native input focus', lambda: view.focus() == window,
                    time.monotonic() + 3, process=core)

            def click(window, px, py):
                rx, ry, child = C.c_int(), C.c_int(), C.c_ulong()
                assert x.XTranslateCoordinates(d, window, client.root, px, py,
                    C.byref(rx), C.byref(ry), C.byref(child))
                pointer.motion(rx.value, ry.value)
                pointer.button(True)
                time.sleep(.03)  # Actual press/release interval for Form's grab.
                pointer.button(False)
                x.XSync(d, 0)

            def geometry(window):
                a = view.attributes(window)
                assert a and a.map_state == 2
                return [a.x, a.y, a.width, a.height]

            def resize(window, width, height):
                before = time.monotonic()
                x.XResizeWindow(d, window, width, height)
                x.XSync(d, 0)
                wait_for_predicate('native resize',
                    lambda: geometry(window)[2:] == [width, height],
                    time.monotonic() + 4, process=core)
                evidence['latency_ms'].append(dict(operation='resize', window=window,
                    width=width, height=height, elapsed=(time.monotonic() - before) * 1000))

            def sample(name, executable, instance):
                pid = owned_module_pid(core.process.pid, executable, instance)
                value = process_sample(pid) if pid else {'unavailable': 'owned module PID absent'}
                evidence['process_observations'].setdefault(name, []).append(value)
                return pid

            # Observe the configured palette through the real owner, including
            # startup races, before recording any native presentation evidence.
            deadline = time.monotonic() + 8
            while True:
                connection.send(b'SendToModule FvwmDesktop query-settings layout all\n')
                settings = {}
                while True:
                    line = receipt('desktop-settings-layout ')
                    if line.endswith(' done'): break
                    _, key, value = line.split(' ', 2)
                    settings[key] = json.loads(value)
                if all(settings.get(k, {}).get('effective') == v for k, v in
                       [('palette', palette), ('appearance', appearance), ('transparency', False)]):
                    break
                assert time.monotonic() < deadline, settings
                time.sleep(.02)
            evidence['theme_settings'] = settings
            bar = view.wait('SlateBar')
            bar_pid = sample('buttons', 'FvwmButtons', 'SlateBar')

            def read_rectangles():
                nonlocal serial
                serial += 1
                tag = 'LAYOUT_RECT_%d_' % serial
                for i in range(10):
                    connection.send(('SendToModule SlateBar ExpandButtonVars %d Echo %s%d '
                        '$left $top $width $height\n' % (i, tag, i)).encode())
                result = []
                for i in range(10):
                    text = receipt(tag + str(i) + ' ')
                    result.append(list(map(int, text.split()[1:])))
                evidence['bar_last_observation'] = dict(rectangles=result, window=geometry(bar))
                assert all(w > 0 and h == 36 for _, _, w, h in result), result
                assert all(a[0] + a[2] == b[0] and a[1] == b[1]
                           for a, b in zip(result, result[1:])), result
                assert result[-1][0] + result[-1][2] - result[0][0] == geometry(bar)[2], result
                return result

            def rectangles():
                def prepared():
                    try:
                        return read_rectangles()
                    except AssertionError:
                        return None  # ConfigureNotify can race the pipe query.
                return wait_for_predicate('prepared bar rectangles', prepared,
                    time.monotonic() + 3, process=core)

            evidence['bar_initial'] = rectangles()
            assert geometry(bar)[3] == 36
            capture('native-bar.png')
            # Observe real shared command-engine desk actions without replacing
            # any shipped action. Echo expansion supplies its actual desk result.
            def desk():
                nonlocal serial
                serial += 1
                tag = 'LAYOUT_DESK_%d ' % serial
                connection.send(('Echo ' + tag + '$[desk.n]\n').encode())
                return int(receipt(tag).split()[-1])

            def choose(index):
                rect = rectangles()[index + 1]
                rx, ry, child = C.c_int(), C.c_int(), C.c_ulong()
                assert x.XTranslateCoordinates(d, client.root, bar,
                    rect[0] + rect[2] // 2, rect[1] + rect[3] // 2,
                    C.byref(rx), C.byref(ry), C.byref(child))
                before = time.monotonic()
                click(bar, rx.value, ry.value)
                wait_for_predicate('actual bar desk action', lambda: desk() == index,
                    time.monotonic() + 3, process=core)
                evidence['latency_ms'].append(dict(operation='bar-pointer-action',
                    elapsed=(time.monotonic() - before) * 1000))

            choose(2)
            focus(bar)
            # ButtonPress focused desk2. A title reflow must retain its action
            # identity; Tab advances to desk3 even after its geometry changes.
            command('SendToModule SlateBar ChangeButtons '
                '(clock Title "Layout checkpoint wide title", '
                'ActiveTitle "Longer active caption for native measurement", '
                'PressTitle "Longest pressed caption reserved before activation") '
                '(status Title "System status")')
            evidence['bar_changed'] = rectangles()
            assert evidence['bar_changed'] != evidence['bar_initial']
            view.key('Tab')
            view.key('Return')
            wait_for_predicate('bar keyboard identity after title reflow',
                lambda: desk() == 3, time.monotonic() + 3, process=core)
            accepted = rectangles()
            command('SendToModule SlateBar ChangeButtons '
                '(clock Title "Must remain invisible") (missing Title "Refuse")')
            assert rectangles() == accepted
            command('SendToModule SlateBar ChangeButton clock '
                'Title "Short", ActiveTitle "Hover", PressTitle "Press"')
            assert rectangles() != accepted
            evidence['bar_refused_group_and_single_title_reflow'] = True
            for width in (160, 700, 1440):
                resize(bar, width, 36)
                evidence.setdefault('bar_resized', []).append(rectangles())
                choose(1)
                choose(2)
            command('GotoDesk 0 0')
            evidence['bar_native_click_keyboard_and_title_identity'] = True

            # Controlled native leaves make every action observable, including
            # an icon minimum and all three caption states at a compressed size.
            # The shipped bar above retains its actual configured actions.
            icon = runtime.root / 'layout-icon.xbm'
            icon.write_text('#define icon_width 320\n#define icon_height 8\n'
                'static unsigned char icon_bits[] = {\n' +
                ','.join(['0x5a'] * 320) + '};\n')
            row = 'LayoutTargets'
            lines = ['Geometry 800x36+0+45', 'Rows 1', 'Layout content',
                'Frame 0', 'Padding 6 0', 'Font "$[infostore.slate_font]"',
                '(1x1, Id icon, Icon ' + str(icon) + ', Action "Echo LAYOUT_TARGET_icon")',
                '(2x1, Id active, Title "A", ActiveTitle "Active wider", '
                'Action "Echo LAYOUT_TARGET_active")',
                '(3x1, Id pressed, Title "P", PressTitle "Pressed wider", '
                'Action "Echo LAYOUT_TARGET_pressed")']
            for line in lines:
                connection.send(('*' + row + ': ' + line + '\n').encode())
            command('Module FvwmButtons ' + row)
            targets = view.wait(row)

            def read_target_rectangles():
                nonlocal serial
                values = []
                for target in ('icon', 'active', 'pressed'):
                    serial += 1
                    tag = 'LAYOUT_TARGET_RECT_%d ' % serial
                    connection.send(('SendToModule %s ExpandButtonVars %s Echo %s'
                        '$left $top $width $height\n' % (row, target, tag)).encode())
                    values.append(list(map(int, receipt(tag).split()[1:])))
                assert all(r[2] > 0 and r[3] == 36 for r in values), values
                assert all(a[0] + a[2] == b[0] for a, b in zip(values, values[1:]))
                assert sum(r[2] for r in values) == geometry(targets)[2]
                return values

            def target_rectangles():
                def prepared():
                    try:
                        return read_target_rectangles()
                    except AssertionError:
                        return None
                return wait_for_predicate('prepared controlled rectangles', prepared,
                    time.monotonic() + 3, process=core)

            wide = target_rectangles()
            assert wide[0][2] >= 332, wide
            resize(targets, 920, 36)
            expanded = target_rectangles()
            assert [b[2] - a[2] for a, b in zip(wide, expanded)] == [20, 40, 60], (wide, expanded)
            resize(targets, 40, 36)
            compressed = target_rectangles()
            assert sum(r[2] for r in compressed) == 40, compressed
            for target, rectangle in zip(('icon', 'active', 'pressed'), compressed):
                pointer.motion(rectangle[0] + rectangle[2] // 2,
                    rectangle[1] + rectangle[3] // 2)
                pointer.button(True)
                time.sleep(.03)
                pointer.button(False)
                receipt('LAYOUT_TARGET_' + target)
                assert target_rectangles() == compressed
            evidence['controlled_icon_and_state_caption_rectangles'] = dict(
                wide=wide, expanded=expanded, compressed=compressed,
                weighted_surplus=[20, 40, 60], all_native_click_actions=True)
            command('KillModule FvwmButtons ' + row)

            folder = runtime.root / 'files'
            folder.mkdir()
            for i in range(ENTRIES): (folder / ('entry-%04d' % i)).touch()
            browser = runtime.start('browser', [str(build / 'bin/FvwmPrompt/fvwm-file-browser'),
                '--socket', str(endpoint), '--view', 'grid', '--reveal',
                str(folder / 'entry-4095')], 'browser.log', env)
            window = view.wait('File browser')
            instance = alias(window)
            assert instance.startswith('FvwmFiles-') and instance.endswith('-ready'), instance
            provider = instance.split('-')[1]
            form_pid = sample('browser-form', 'FvwmForm', instance)
            evidence['process_observations']['browser-owner'] = [process_sample(browser.process.pid)]

            def rpc():
                nonlocal serial
                serial += 1
                request = dict(version=1, id='layout-%d' % serial, method='query',
                    resource='files', provider=provider, scope='state')
                query.send(('request ' + json.dumps(request) + '\n').encode())
                response = query.record()['reply']
                assert response['id'] == request['id'] and response['status'] == 'ok', response
                return response['result']

            def list_state(target, name='picks', allow_pending=False):
                nonlocal serial
                serial += 1
                token = 'layout%d' % serial
                connection.send(('SendToModule %s ListStatus %s %s selection\n' %
                    (target, name, token)).encode())
                text = receipt('FVWM_FORM_LIST ' + token + ' ')
                fields = text.split()
                assert fields[4] == '0' and (allow_pending or fields[3] == '0'), text
                packet = base64.urlsafe_b64decode(fields[5][1:] + '==')
                assert int.from_bytes(packet[:4], 'big') == int(fields[2]), text
                return packet

            def child_values(target):
                values = [(child, geometry(child)) for child in view.children(target)]
                assert 1 <= len(values) <= 64, values
                return values

            def children(target):
                values = child_values(target)
                outer = geometry(target)
                assert all(g[0] >= 0 and g[1] >= 0 and g[2] > 0 and g[3] > 0 and
                    g[0] + g[2] <= outer[2] and g[1] + g[3] <= outer[3]
                    for _, g in values), (outer, values)
                return values

            def list_child(target):
                return max(child_values(target), key=lambda value: value[1][3])

            def burst_resize(target, module_alias, owner):
                before = list_state(module_alias)
                identities = sorted(view.children(target))
                outer = geometry(target)
                control, box = list_child(target)
                fixed_height = outer[3] - box[3]
                sizes = [(outer[2] + 40, outer[3] + 30),
                         (outer[2] + 90, outer[3] + 60),
                         (outer[2] + 60, outer[3] + 40)]
                started = time.monotonic()
                # Queue a real burst on one X connection. There is no query,
                # sync or round trip between requests; flush only after all 3.
                for width, height in sizes:
                    x.XResizeWindow(d, target, width, height)
                x.XFlush(d)
                width, height = sizes[-1]

                def final_layout():
                    return (geometry(target)[2:] == [width, height] and
                        geometry(control)[2:] ==
                        [width - 20, height - fixed_height])

                wait_for_predicate('last burst configure owns viewport',
                    final_layout, time.monotonic() + 4, process=owner)
                assert sorted(view.children(target)) == identities
                assert list_state(module_alias) == before
                children(target)
                elapsed = (time.monotonic() - started) * 1000
                evidence['latency_ms'].append(dict(operation='burst-resize',
                    window=target, elapsed=elapsed))
                evidence.setdefault('native_resize_bursts', []).append(dict(
                    alias=module_alias, requests=sizes, final_parent=geometry(target),
                    final_viewport=geometry(control), same_state_and_children=True))

            state = rpc()
            assert state['loaded'] == ENTRIES and state['selected'] == 1, state
            before = list_state(instance)
            assert len(before) == 518 and before[4:6] == bytes([15, 255]) and before[-1] == 128, before
            identities = sorted(view.children(window))
            initial = geometry(window)
            browser_fixed_height = initial[3] - list_child(window)[1][3]
            capture('native-browser-4096.png')
            # Derive legal actual-pixel sizes from native hints. Resize adds
            # viewport rows without changing fixed controls or publishing data.
            hints, supplied = SizeHints(), C.c_long()
            assert x.XGetWMNormalHints(d, window, C.byref(hints), C.byref(supplied))
            sizes = [(max(hints.min_width, initial[2] - 180),
                      max(hints.min_height, initial[3] - 100)),
                     (max(hints.min_width, initial[2] + 100), initial[3] + 100)]
            evidence['browser_initial_geometry'] = initial
            evidence['browser_minimum_size'] = [hints.min_width, hints.min_height]
            for width, height in sizes:
                resize(window, width, height)
                # Wait for the native child reflow, then compare its state.
                wait_for_predicate('browser collection reflow',
                    lambda: list_child(window)[1][2:] ==
                        [width - 20, height - browser_fixed_height],
                    time.monotonic() + 3, process=browser)
                assert list_state(instance) == before
                assert sorted(view.children(window)) == identities
                assert view.named('File browser') == window
                children(window)
                after = rpc()
                assert all(after[field] == state[field] for field in
                    ('generation', 'selected', 'focus', 'loaded')), (state, after)
                evidence.setdefault('browser_resized', []).append(dict(
                    geometry=geometry(window), collection=list_child(window)[1]))
            burst_resize(window, instance, browser)
            after = rpc()
            assert all(after[field] == state[field] for field in
                ('generation', 'selected', 'focus', 'loaded')), (state, after)
            capture('native-browser-resized.png')
            control, box = list_child(window)
            click(control, min(20, box[2] - 1), min(20, box[3] - 1))
            clicked = list_state(instance)
            assert clicked != before and clicked[4:6] != bytes([15, 255]), clicked
            focus(window)
            view.key('End')
            assert list_state(instance)[4:6] == bytes([15, 255])
            # A real refresh publishes once, preserving the selected last entry
            # and native window/children, unlike a resize which publishes none.
            before = list_state(instance)
            # Refresh only publishes a native generation for changed entries.
            # Change metadata in-place so IDs, ordering and selection survive.
            (folder / 'entry-0000').write_text('changed metadata')
            focus(window)
            view.key('F5')
            wait_for_predicate('browser refresh generation',
                lambda: int.from_bytes(list_state(instance, allow_pending=True)[:4], 'big') >
                    int.from_bytes(before[:4], 'big'),
                time.monotonic() + 8, process=browser)
            refreshed = list_state(instance)
            assert refreshed[4:] == before[4:]
            assert sorted(view.children(window)) == identities
            evidence['browser_resize_preserves_state_and_refresh_publishes'] = True
            evidence['browser_native_generation'] = [int.from_bytes(before[:4], 'big'),
                                                    int.from_bytes(refreshed[:4], 'big')]
            sample('browser-form', 'FvwmForm', instance)
            evidence['process_observations']['browser-owner'].append(process_sample(browser.process.pid))
            idle_before = process_sample(form_pid) if form_pid else {'unavailable': 'owned module PID absent'}
            time.sleep(.4)
            idle_after = process_sample(form_pid) if form_pid else {'unavailable': 'owned module PID absent'}
            evidence['form_idle_observation'] = dict(seconds=.4, before=idle_before,
                after=idle_after, stable_cpu_ticks=(idle_before.get('cpu_ticks') ==
                idle_after.get('cpu_ticks')) if 'cpu_ticks' in idle_before else None)
            view.key('Escape')
            assert browser.wait_until_exit(time.monotonic() + 4) == 0

            # Small direct adopter fixes a four-column contract independently
            # of the browser's configurable eight-column grid projection.
            collection = 'LayoutCollection'
            lines = ['Title "Native layout collection"', 'Line left',
                'List picks 40 4', 'ListViewport 12 4', 'ListVersioned',
                *['ListItem off "Native entry %02d" file' % n for n in range(64)],
                'ListFocus 33', 'Line left', 'Selection options single',
                'Choice keep yes on "Fixed choice label survives resize"',
                'Line left', 'Button continue "Observe (F2)" F2',
                'Command Echo LAYOUT_COLLECTION_ACTIVATED x$(picks:Base64)']
            for line in lines: connection.send(('*' + collection + ': ' + line + '\n').encode())
            command('Module FvwmForm ' + collection)
            small = view.wait('Native layout collection')
            focus(small)
            view.key('space')
            original = list_state(collection)
            small_ids = sorted(view.children(small))
            choices = {child: box[2:] for child, box in children(small)
                       if box[2] == box[3]}
            assert len(choices) == 1, choices
            original_size = geometry(small)

            def top_identity():
                saved = list_state(collection)
                child, box = list_child(small)
                click(child, min(10, box[2] - 1), min(10, box[3] - 1))
                observed = list_state(collection)
                top = int.from_bytes(observed[4:6], 'big')
                # Toggle the same entry back and restore only focus through the
                # existing versioned operation; do not publish another model.
                click(child, min(10, box[2] - 1), min(10, box[3] - 1))
                command('SendToModule %s ListFocus picks %d %d' % (collection,
                    int.from_bytes(saved[:4], 'big'),
                    int.from_bytes(saved[4:6], 'big') + 1))
                assert list_state(collection) == saved
                return top

            top = top_identity()
            previous_list_height = list_child(small)[1][3]
            resize(small, original_size[2], original_size[3] + 80)
            wait_for_predicate('collection child height reflow',
                lambda: list_child(small)[1][3] == previous_list_height + 80,
                time.monotonic() + 3, process=core)
            assert top_identity() == top
            evidence['primitive_top_identity_after_height_resize'] = top
            for width, height in [(original_size[2] + 120, original_size[3] + 120),
                                  (original_size[2], original_size[3])]:
                resize(small, width, height)
                wait_for_predicate('small collection final child reflow',
                    lambda: list_child(small)[1][2:] == [width - 20,
                        height - original_size[3] + previous_list_height],
                    time.monotonic() + 3, process=core)
                assert all(geometry(child)[2:] == size
                           for child, size in choices.items())
                assert list_state(collection) == original
                assert sorted(view.children(small)) == small_ids
                children(small)
            burst_resize(small, collection, core)
            assert all(geometry(child)[2:] == size for child, size in choices.items())
            assert list_state(collection) == original
            capture('native-collection-choice-resized.png')
            evidence['fixed_choice_square_after_burst_resize'] = choices
            focus(small)
            view.key('Home')
            view.key('Down')
            moved = list_state(collection)
            assert 1 <= int.from_bytes(moved[4:6], 'big') <= 4, moved
            view.key('F2')
            receipt('LAYOUT_COLLECTION_ACTIVATED ')
            evidence['four_column_primitive_state_and_keyboard'] = True
            sample('buttons', 'FvwmButtons', 'SlateBar')
            if form_pid:
                evidence['process_observations']['browser-form'].append(
                    dict(pid=form_pid, retired_after_owned_browser_close=True))
            idle_before = process_sample(bar_pid) if bar_pid else {'unavailable': 'owned module PID absent'}
            time.sleep(.4)
            idle_after = process_sample(bar_pid) if bar_pid else {'unavailable': 'owned module PID absent'}
            evidence['buttons_idle_observation'] = dict(seconds=.4, before=idle_before,
                after=idle_after, stable_cpu_ticks=(idle_before.get('cpu_ticks') ==
                idle_after.get('cpu_ticks')) if 'cpu_ticks' in idle_before else None)
            command('KillModule FvwmForm ' + collection)
            connection.send(b'Quit\n')
            assert core.wait_until_exit(time.monotonic() + 4) == 0
            logs = '\n'.join(path.read_text(errors='replace') for path in runtime.root.glob('*.log'))
            assert not any(marker in logs for marker in ('ERROR: AddressSanitizer',
                'ERROR: LeakSanitizer', 'runtime error:', 'Assertion', 'BadMatch', 'BadWindow')), logs
            assert not errors.unexpected, errors.unexpected
            evidence['status'] = 'passed'
        finally:
            failure = sys.exc_info()[1]
            if failure is not None:
                evidence['status'] = 'failed'
                evidence['failure'] = repr(failure)
            if connection: connection.close()
            if query: query.close()
            if pointer: pointer.button(False)
            if core: core.close(time.monotonic() + 4)
            if errors:
                errors.close_display()
            elif client:
                client.close()
            for path in runtime.root.glob('*.log'):
                shutil.copy2(path, out / path.name)
            if errors and errors.unexpected:
                evidence['status'] = 'failed'
            evidence['elapsed_seconds'] = time.monotonic() - started
            (out / 'result.json').write_text(json.dumps(evidence, indent=2) + '\n')
    assert not errors.unexpected, errors.unexpected
    print(json.dumps(evidence))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__, allow_abbrev=False)
    parser.add_argument('--build', required=True, type=Path)
    parser.add_argument('--artifacts-dir', required=True, type=Path)
    parser.add_argument('--profiles', nargs='+', choices=PROFILES, default=list(PROFILES))
    options = parser.parse_args()
    # One finite combined checkpoint. Harness SIGTERM cleanup remains active.
    def deadline(signum, frame):
        raise TimeoutError('native layout checkpoint exceeded 120 seconds')
    signal.signal(signal.SIGALRM, deadline)
    signal.alarm(120)
    for selected_profile in options.profiles:
        run(options, selected_profile)
    signal.alarm(0)
