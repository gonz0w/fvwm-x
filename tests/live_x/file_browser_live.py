#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or any later version.
"""Exercise the real Go owner, native Form and isolated filesystem worker."""
import argparse
import base64
import ctypes as C
import hashlib
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
from live_x_harness import HarnessError, HarnessRuntime, start_display_server, wait_for_predicate
from mfl_stream_live import Connection


def run(args):
    artifact = args.artifacts_dir.resolve()
    artifact.mkdir(parents=True, exist_ok=True)
    evidence = {}
    previous_handler = None
    unexpected_errors = []
    observed_windows = set()
    retired_errors = {}
    with HarnessRuntime(prefix='file-browser-') as runtime:
        connection = query_connection = client = pointer = None
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
                'from pathlib import Path\np=Path(' + repr(str(report)) + ');t=p.with_suffix(".part");t.write_text(json.dumps('
                '{"argv":[base64.b64encode(os.fsencode(v)).decode() for v in sys.argv[1:]],'
                '"cwd":base64.b64encode(os.fsencode(os.getcwd())).decode()}));t.replace(p)\n')
            opener.chmod(0o700)
            config = runtime.root / 'config'
            modules = runtime.root / 'modules'
            modules.mkdir()
            build = args.fvwm.resolve().parent
            for directory in (build / 'modules').iterdir():
                executable = directory / directory.name
                if executable.is_file():
                    (modules / directory.name).symlink_to(executable)
            for name, executable in [('FvwmMFL', args.mfl), ('FvwmForm', args.form)]:
                alias = modules / name
                if alias.is_symlink():
                    alias.unlink()
                alias.symlink_to(executable.resolve())
            source = Path(__file__).resolve().parents[2]
            theme_root = source / 'default-config'
            inputs = {}
            if args.theme in ('slate-dark', 'dune-light'):
                palette, appearance = args.theme.split('-')
                local = runtime.root / 'local.config'
                local.write_text('*FvwmDesktop: palette ' + palette + '\n'
                    '*FvwmDesktop: appearance ' + appearance + '\n'
                    '*FvwmDesktop: transparency false\n')
                inputs['local.config'] = local.read_bytes()
                for name in ('config', 'desktop.fvwm', 'desktop.py', 'palettes.json'):
                    inputs[name] = (theme_root / name).read_bytes()
                (modules / 'FvwmDesktop').symlink_to(build / 'default-config/FvwmDesktop')
                config.write_text('ModulePath ' + str(modules) + '\nRead ' +
                    str(source / 'fvwm/ConfigFvwmDefaults') + '\n' +
                    (theme_root / 'config').read_text().replace(
                        '$[FVWM_DATADIR]/default-config', str(theme_root)) +
                    '\nAddToFunc StartFunction I Module FvwmMFL\n')
                evidence['font'] = 'xft:Sans:size=10:antialias=True'
            elif args.theme == 'classic-large':
                config.write_text('DefaultFont 9x15\n'
                    'Style * TileCascadePlacement, MouseFocus, MouseFocusClickRaisesOff, ClickToFocusPassesClick\n'
                    'Style * BorderWidth 3, HandleWidth 6\n'
                    '*FvwmFormDefault: Fore black\n*FvwmFormDefault: Back wheat\n'
                    '*FvwmFormDefault: ItemFore navy\n*FvwmFormDefault: ItemBack ivory\n'
                    '*FvwmFormDefault: Font 9x15\n*FvwmFormDefault: ButtonFont 9x15\n'
                    '*FvwmFormDefault: InputFont 9x15\n'
                    'ModulePath ' + str(modules) + '\n'
                    'AddToFunc StartFunction I Module FvwmMFL\n')
                evidence['font'] = '9x15'
            else:
                config.write_text('DefaultFont fixed\nStyle * TileCascadePlacement, MouseFocus, MouseFocusClickRaisesOff, ClickToFocusPassesClick\n'
                    'Colorset 0 fg #e6ebef, bg #24343e\n'
                    '*FvwmFormDefault: Colorset 0\n*FvwmFormDefault: ItemColorset 0\n'
                    '*FvwmFormDefault: Font fixed\n*FvwmFormDefault: ButtonFont fixed\n'
                    '*FvwmFormDefault: InputFont fixed\n'
                    'ModulePath ' + str(args.mfl.parent) + ':' + str(args.form.parent) + '\n'
                    'AddToFunc StartFunction I Module FvwmMFL\n')
                evidence['font'] = 'fixed'
            inputs['generated.config'] = config.read_bytes()
            for name, data in inputs.items():
                (artifact / name).write_bytes(data)
            evidence['theme'] = args.theme
            evidence['configuration_sha256'] = {
                name: hashlib.sha256(data).hexdigest() for name, data in inputs.items()}
            evidence['server_geometry'] = '1440x1000x24'
            evidence['scale'] = 1
            env = runtime.environment({'DISPLAY': display,
                'FVWMMFL_SOCKET_PATH': str(runtime.root), 'FVWM_USERDIR': str(runtime.root),
                'FVWM_DESKTOP_ROOT': str(theme_root), 'HOME': str(runtime.root)})
            core = runtime.start('core', [str(args.fvwm), '-d', display, '-f', str(config)], 'core.log', env)
            endpoint = runtime.root / ('fvwm_mfl_' + display.replace(':', '_') + '.sock')
            wait_for_predicate('MFL endpoint', endpoint.exists, time.monotonic() + 10, process=core)
            connection = Connection(endpoint)
            connection.socket.settimeout(15)
            connection.record()
            connection.send(b'set echo\n')
            connection.echo('browser-probe-ready')
            if args.theme in ('slate-dark', 'dune-light'):
                # A query may overtake the owner's initial configuration.
                # Wait for the requested real configuration, not the default
                # saved palette returned while module overrides are arriving.
                deadline = time.monotonic() + 15
                attempts = 0
                while time.monotonic() < deadline:
                    attempts += 1
                    connection.send(b'SendToModule FvwmDesktop query-settings theme all\n')
                    settings = {}
                    while time.monotonic() < deadline:
                        text = connection.record().get('echo', {}).get('message', '')
                        if text == 'desktop-settings-theme done':
                            break
                        if text.startswith('desktop-settings-theme '):
                            _, key_, value = text.split(' ', 2)
                            settings[key_] = json.loads(value)
                    expected = {'palette': palette, 'appearance': appearance,
                                'transparency': False}
                    if all(settings.get(k, {}).get('effective') == v and
                           settings[k]['source'] == 'module-configuration' and
                           settings[k]['application'] == 'submitted-to-fvwm'
                           for k, v in expected.items()):
                        evidence['theme_settings'] = {k: settings[k] for k in expected}
                        evidence['theme_readiness_queries'] = attempts
                        break
                    time.sleep(0.02)
                else:
                    raise AssertionError('Configured desktop theme not ready: ' + repr(settings))
            snapshot = subprocess.run([str(args.browser), '--inspect', '--dir', str(root)],
                env=env, capture_output=True, timeout=15)
            assert snapshot.returncode == 0, snapshot
            snapshot = json.loads(snapshot.stdout)['snapshot']
            assert len(snapshot['items']) == 19
            assert any(base64.b64decode(i['name_bytes']) == b'raw-\xff' for i in snapshot['items'])
            app = runtime.start('browser', [str(args.browser), '--socket', str(endpoint),
                '--dir', str(root), '--opener', str(opener), '--terminal', str(opener),
                '--action', 'Review selection=' + str(opener),
                '--action', 'Second configured action=' + str(opener),
                '--action', 'Third configured action=' + str(opener),
                '--action', 'Fourth configured action=' + str(opener)], 'browser.log', env)
            action_count = 4
            expected_children = 23 + action_count
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
                if event.code == 3 and event.resource in observed_windows and event.request in (3, 14, 15, 20):
                    key_ = (event.resource, event.request)
                    retired_errors[key_] = retired_errors.get(key_, 0) + 1
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

            def borrowed_geometry(window):
                before = retired_errors.get((window, 14), 0)
                try:
                    return observe(window, lambda: client.geometry(window))
                except HarnessError:
                    # A retiring Form connection can close while the server is
                    # grabbed. Retry only its observed BadWindow/GetGeometry;
                    # all other X failures keep their original failure path.
                    if retired_errors.get((window, 14), 0) > before:
                        return None
                    raise

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
                    values = []
                    for child in children(window):
                        geometry = borrowed_geometry(child)
                        if geometry is None:
                            return None, [], []
                        values.append((geometry, child))
                    squares = sorted((g[1], w) for g, w in values if g[2] == g[3])
                    return window, squares, values
                finally:
                    x.XUngrabServer(d)
                    x.XSync(d, 0)

            class ClassHint(C.Structure):
                _fields_ = [('name', C.c_void_p), ('class_', C.c_void_p)]
            x.XGetClassHint.argtypes = [C.c_void_p, C.c_ulong, C.POINTER(ClassHint)]
            seen_alias = None

            primitive = False
            seen_renders = {}
            seen_generations = {}
            retired_instances = set()

            def render_state(active):
                deadline = time.monotonic() + 15
                while time.monotonic() < deadline:
                    text = connection.record().get('echo', {}).get('message', '')
                    fields = text.split()
                    if len(fields) != 6 or fields[0] != 'FVWM_FILES_RENDER':
                        continue
                    instance, form, serial, generation, state = fields[1:]
                    if instance in retired_instances:
                        continue
                    serial = int(serial)
                    if serial <= seen_renders.get(instance, 0):
                        continue
                    seen_renders[instance] = serial
                    seen_generations[instance] = int(generation)
                    if int(state) == active:
                        return 'FvwmFiles-%s-%s-ready' % (instance, form)
                raise AssertionError('missing native publication observation')

            def ready(old=None, choices=None):
                target = None if primitive else render_state(0)
                def mapped():
                    window, squares, values = inventory()
                    if not window:
                        return None
                    alias = last_inventory_alias
                    if alias.endswith('-ready') and (target is None or alias == target) and len(values) == expected_children:
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
                    if text.startswith('FVWM_FILES_3 '):
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

            def widget():
                _, _, values = inventory()
                return max(values, key=lambda value: value[0][3])

            def row_groups():
                _, _, values = inventory()
                box, _ = widget()
                ys = sorted({g[1] for g, w in values if g[1] >= box[1] + box[3]})
                return [sorted((g[0], w) for g, w in values if g[1] == y) for y in ys]

            def packet(text):
                value = base64.urlsafe_b64decode(text.split()[5][1:] + '==')
                assert len(value) >= 6
                return value[4:]

            window = ready()
            assert not report.exists(),'configured action ran on startup'
            query_connection = Connection(endpoint)
            query_connection.record()
            query_connection.send(b'request {"version":1,"id":"ro","method":"set_mode","mode":"read-only"}\n')
            assert query_connection.record()['reply']['status'] == 'ok'
            query_serial = 0

            def rpc(**fields):
                nonlocal query_serial
                query_serial += 1
                request = dict(version=1, id='browser-' + str(query_serial),
                               method='query', resource='files', **fields)
                query_connection.send(('request ' + json.dumps(request) + '\n').encode())
                reply = query_connection.record()['reply']
                assert reply['id'] == request['id'], reply
                return reply

            provider = seen_alias.split('-')[1]
            providers = rpc()['result']['providers']
            assert [p['provider'] for p in providers] == [provider], providers
            state = rpc(provider=provider, scope='state')['result']
            assert state['loaded'] == 19 and not state['actions'] and not state['file_content'], state
            assert base64.b64decode(state['path_bytes']) == os.fsencode(root)
            entries, offset = [], 0
            while offset is not None:
                page = rpc(provider=provider, scope='items',
                           generation=state['generation'], offset=offset, limit=8)['result']
                entries.extend(page['items'])
                offset = page['next_offset']
            assert {i['id'] for i in entries} == {i['id'] for i in snapshot['items']}
            assert any(base64.b64decode(i['name_bytes']) == b'raw-\xff' for i in entries)
            assert all(isinstance(i['size_bytes'], str) and isinstance(i['mtime_ns'], str) for i in entries)
            if args.command_client:
                query = json.dumps(dict(version=1, id='cli-files', method='query', resource='files'))
                command = subprocess.run([str(args.command_client), '-f', str(endpoint),
                    '--request', query], env=env, capture_output=True, timeout=10)
                assert command.returncode == 0, command
                assert json.loads(command.stdout)['reply']['result']['providers'][0]['provider'] == provider
            evidence['file_provider_discovery_and_pages'] = True
            # A horizontally scrolled text field must reset its viewport when
            # traversing through the list back to that field.
            for _ in range(100): key(window,b'x')
            key(window,b'Tab')
            shift=x.XKeysymToKeycode(d,x.XStringToKeysym(b'Shift_L'))
            pointer.x.XTestFakeKeyEvent(d,shift,1,0);key(window,b'Tab');pointer.x.XTestFakeKeyEvent(d,shift,0,0)
            key(window,b'a');time.sleep(0.05)
            capture('file-browser-input-focus-return.png')
            _,_,values=inventory()
            field=min(((g,w) for g,w in values if g[2]>140 and g[3]<50),key=lambda v:v[0][1]);fg=field[0]
            controls=sorted((g[0],w) for g,w in values if g[0]>fg[0]+fg[2] and abs(g[1]-fg[1])<3)
            click(controls[0][1]);text=callback()
            assert base64.urlsafe_b64decode(text.split()[4][1:]+'==')==b'a'+b'x'*100
            window=ready(window);capture('file-browser-empty-filter.png')
            _,_,values=inventory()
            field=min(((g,w) for g,w in values if g[2]>140 and g[3]<50),key=lambda v:v[0][1]);fg=field[0]
            controls=sorted((g[0],w) for g,w in values if g[0]>fg[0]+fg[2] and abs(g[1]-fg[1])<3)
            click(controls[1][1]);callback();window=ready(window)
            evidence['mixed_focus_scrolled_input_and_empty_list']=True
            capture('file-browser-collection.png')
            def enter_list():
                _, _, values = inventory()
                field = min(((g,w) for g,w in values if g[2]>140 and g[3]<50), key=lambda v:v[0][1])
                click(field[1], False)
                key(window, b'Tab')

            def clear_input():
                control = x.XKeysymToKeycode(d, x.XStringToKeysym(b'Control_L'))
                pointer.x.XTestFakeKeyEvent(d, control, 1, 0)
                key(window, b'u')
                pointer.x.XTestFakeKeyEvent(d, control, 0, 0)
                x.XSync(d,0)

            identity = window, sorted(children(window))
            state = rpc(provider=provider, scope='state')['result']
            enter_list();key(window,b'Home');key(window,b'space')
            selected = rpc(provider=provider, scope='selection')['result']
            assert len(selected['items']) == 1, selected
            assert base64.b64decode(selected['items'][0]['name_bytes']) == b'00-folder'
            stale = rpc(provider=provider, scope='items', generation=state['generation'], offset=8)
            assert stale['error']['code'] == 'stale_generation', stale
            properties = rpc(provider=provider, scope='properties')['result']['properties']
            assert properties['directories'] == 1 and properties['regular_file_bytes'] == '0', properties
            assert identity == (window, sorted(children(window)))
            capture('file-browser-provider-selection.png')
            key(window,b'space')
            evidence['file_provider_current_native_selection_and_stale_page'] = True

            # Both presentations share indices/IDs, focus and selection. Shift
            # navigation adds a bounded anchor range before switching views.
            key(window,b'F7');callback();window=ready(window)
            capture('file-browser-grid.png')
            enter_list();key(window,b'Home')
            for _ in range(2):
                pointer.x.XTestFakeKeyEvent(d,shift,1,0);key(window,b'Right');pointer.x.XTestFakeKeyEvent(d,shift,0,0)
            box,control=widget()
            cx,cy,child=C.c_int(),C.c_int(),C.c_ulong()
            cell_width=(box[2]-22)//4;cell_height=(box[3]-6)//4
            assert x.XTranslateCoordinates(d,control,client.root,3+2*cell_width+cell_width//2,3+cell_height+cell_height//2,C.byref(cx),C.byref(cy),C.byref(child))
            pointer.x.XTestFakeKeyEvent(d,shift,1,0);pointer.motion(cx.value,cy.value)
            pointer.button(True);time.sleep(0.05);pointer.button(False);pointer.x.XTestFakeKeyEvent(d,shift,0,0)
            key(window,b'F7');bits=packet(callback())
            assert bits[:2]==bytes([0,6]) and bits[2]==127,bits
            window=ready(window);capture('file-browser-range-list.png')
            enter_list()
            for _ in range(2):
                pointer.x.XTestFakeKeyEvent(d,shift,1,0);key(window,b'Down');pointer.x.XTestFakeKeyEvent(d,shift,0,0)
            key(window,b'F2');bits=packet(callback());assert bits[:2]==bytes([0,8]) and bits[2:4]==bytes([255,1]),bits
            window=ready(window)
            click(row_groups()[0][1][1]);callback();window=ready(window)
            key(window,b'F7');callback();window=ready(window)
            enter_list();key(window,b'Home');key(window,b'Down');key(window,b'Right')
            key(window,b'F7');bits=packet(callback());assert bits[:2]==bytes([0,5]) and not any(bits[2:]),bits
            window=ready(window)
            evidence['grid_icons_range_selection_and_view_identity']=True
            # Tab enters the list from the filter. Local motion/selection needs
            # neither a helper round trip nor a new form generation.
            enter_list()
            key(window, b'End')
            key(window, b'space')
            stable_frame = (window, sorted(children(window)))
            key(window, b'F2')
            first = callback()
            bits = packet(first)
            assert bits[:2] == bytes([0,18]) and bits[4] == 4, bits
            window = ready(window)
            assert stable_frame == (window, sorted(children(window)))
            capture('file-browser-scrolled-properties.png')
            evidence['end_space_selection_and_properties'] = True
            # The literal byte name is owned by the worker, never the label.
            enter_list()
            key(window, b'Return')
            callback()
            wait_for_predicate('raw file dispatched', report.exists, time.monotonic()+5, process=app)
            opened = json.loads(report.read_text())
            assert base64.b64decode(opened['argv'][0]) == invalid
            window = ready(window)
            evidence['focused_literal_byte_activation'] = True
            connection.send(('Echo '+first+'\n').encode())
            connection.expect_echo(first)
            window = ready(window)
            evidence['stale_control_refused'] = True
            # Configured actions use the current selection's original paths,
            # including non-UTF-8 and command-looking names, in one invocation.
            click(row_groups()[0][1][1]);callback();window=ready(window)
            enter_list();key(window,b'End');key(window,b'space')
            key(window,b'Up');key(window,b'space');key(window,b'F2')
            picks=packet(callback());assert picks[:2]==bytes([0,17]) and picks[4]==6,picks
            window=ready(window)
            capture('file-browser-selection-properties.png')
            report.unlink()
            click(row_groups()[-action_count][0][1]);action_callback=callback()
            assert action_callback.split()[3:5] == ['custom','0']
            wait_for_predicate('configured action report',report.exists,time.monotonic()+5,process=app)
            opened=json.loads(report.read_text())
            assert [base64.b64decode(v) for v in opened['argv']] == [os.fsencode(root/literal),invalid],opened
            assert base64.b64decode(opened['cwd']) == os.fsencode(root)
            window=ready(window);capture('file-browser-custom-action.png')
            report.unlink()
            connection.send(('Echo '+action_callback+'\n').encode())
            connection.expect_echo(action_callback);window=ready(window)
            assert not report.exists(),'stale configured action launched'
            evidence['configured_action_literal_arguments_and_stale_control'] = True
            # Sort/reverse publish in the same frame while an unrelated path
            # draft remains untouched. Restore the named order for later cases.
            _, _, values = inventory()
            path_field = max(((g,w) for g,w in values if g[2]>200 and g[3]<50), key=lambda v:v[0][2])
            click(path_field[1], False); clear_input()
            for char in 'unsent-draft': key(window, b'minus' if char=='-' else char.encode())
            sort_frame = (window, sorted(children(window)))
            def sort_buttons():
                _, _, values = inventory(); box, _ = widget()
                field = min((g for g,w in values if g[2]>140 and g[3]<50), key=lambda g:g[1])
                return sorted((g[0],w) for g,w in values if g[1]>field[1]+field[3] and g[1]<box[1])
            click(sort_buttons()[0][1]); callback(); window=ready(window)
            click(sort_buttons()[1][1]); callback(); window=ready(window)
            assert sort_frame == (window, sorted(children(window)))
            capture('file-browser-live-sort.png')
            click(sort_buttons()[1][1]); callback(); window=ready(window)
            for _ in range(3):
                click(sort_buttons()[0][1]); callback(); window=ready(window)
            # Go's callback proves the draft survived all updates; cancel the
            # failed/nonexistent navigation and retain this directory.
            _, _, values = inventory(); fg=path_field[0]
            controls=sorted((g[0],w) for g,w in values if g[0]>fg[0]+fg[2] and abs(g[1]-fg[1])<3)
            click(controls[0][1]); text=callback()
            assert base64.urlsafe_b64decode(text.split()[4][1:]+'==')==b'unsent-draft'
            window=ready(window)
            assert sort_frame == (window, sorted(children(window)))
            evidence['live_sort_preserves_path_draft_and_windows'] = True
            # Clear the entire native selection, navigate to the first directory
            # using focus alone, then use the native Up button.
            click(row_groups()[0][1][1]); callback(); window = ready(window)
            enter_list(); key(window, b'Home'); key(window, b'Return')
            callback(); window = ready(window)
            click(row_groups()[1][1][1]); callback(); window = ready(window)
            evidence['focused_folder_activation_and_parent'] = True
            # Select the directory, then filter it away. Hidden selection survives
            # the new generation without admitting nonexistent bitmap bits.
            enter_list(); key(window, b'Home'); key(window, b'space')
            _, _, values = inventory()
            field = min(((g,w) for g,w in values if g[2] > 140 and g[3] < 50), key=lambda v:v[0][1])
            click(field[1], wait_grab=False)
            symbols = {'/':b'slash','-':b'minus','_':b'underscore','.':b'period'}
            clear_input()
            for char in 'file-14': key(window, symbols.get(char,char.encode()))
            fg=field[0]
            controls=sorted((g[0],w) for g,w in values if g[0]>fg[0]+fg[2] and abs(g[1]-fg[1])<3)
            click(controls[0][1]); callback(); window=ready(window)
            enter_list(); key(window,b'space'); key(window,b'F2')
            both=packet(callback()); assert both == bytes([0,0,1]), both
            window=ready(window)
            filtered_frame = (window, sorted(children(window)))
            capture('file-browser-filtered-selection.png')
            evidence['filter_preserves_hidden_selection'] = True
            # The 40-column path input is wider than every configured action
            # button (labels <=32 columns). Actions can sit below it, so Y order
            # alone cannot identify the field in a larger-font configuration.
            # Cancel a genuinely blocked FIFO worker while the core keeps serving.
            _,_,values=inventory()
            field=max(((g,w) for g,w in values if g[2]>200 and g[3]<50),key=lambda v:v[0][2])
            click(field[1],wait_grab=False)
            clear_input()
            for char in str(blocked): key(window,symbols.get(char,char.encode()))
            fg=field[0]
            controls=sorted((g[0],w) for g,w in values if g[0]>fg[0]+fg[2] and abs(g[1]-fg[1])<3)
            click(controls[0][1]); assert base64.urlsafe_b64decode(callback().split()[4][1:]+'==')==os.fsencode(blocked)
            render_state(1)
            connection.echo('core-responsive-during-blocked-worker')
            _,_,values=inventory()
            field=max(((g,w) for g,w in values if g[2]>200 and g[3]<50),key=lambda v:v[0][2]);fg=field[0]
            controls=sorted((g[0],w) for g,w in values if g[0]>fg[0]+fg[2] and abs(g[1]-fg[1])<3)
            click(controls[1][1]);callback();window=ready(window)
            key(window,b'F5');callback();window=ready(window)
            evidence['blocked_worker_cancel_reap_refresh'] = True
            # Refresh a filtered collection after the focused selected entry
            # disappears. The old hidden directory selection must survive;
            # the new matching entry receives focus but not selection.
            (root / 'file-14').unlink()
            (root / 'file-14-new').write_text('new entry')
            (root / 'file-13').write_text('changed observed size')
            key(window,b'F5');callback();window=ready(window)
            enter_list();key(window,b'F2')
            assert packet(callback()) == bytes([0,0,0])
            window=ready(window)
            assert filtered_frame == (window, sorted(children(window)))
            capture('file-browser-refresh-changes.png')
            _,_,values=inventory()
            field=min(((g,w) for g,w in values if g[2]>140 and g[3]<50),key=lambda v:v[0][1]);fg=field[0]
            controls=sorted((g[0],w) for g,w in values if g[0]>fg[0]+fg[2] and abs(g[1]-fg[1])<3)
            click(controls[1][1]);callback();window=ready(window)
            key(window,b'F2');bits=packet(callback())
            assert bits[:2] == bytes([0,15]) and bits[2:] == bytes([1,0,0]),bits
            window=ready(window)
            key(window,b'F5');callback();window=ready(window)
            assert filtered_frame == (window, sorted(children(window)))
            capture('file-browser-refresh-unchanged.png')
            evidence['browser_live_window_and_draft_lifecycle'] = True
            evidence['refresh_focus_fallback_and_hidden_selection'] = True
            # Opt-in watching reconciles native invalidations through the same
            # window, preserving drafts and surviving selection. Loss is an
            # explicit manual fallback; a successful F5 re-arms the observer.
            def watch_state(want):
                deadline = time.monotonic() + 15
                while time.monotonic() < deadline:
                    text = connection.record().get('echo', {}).get('message', '')
                    if text == 'FVWM_FILES_WATCH %s %s' % (seen_alias.split('-')[1], want):
                        return
                raise AssertionError('missing watch state ' + want)

            key(window, b'F8'); callback(); window = ready(window)
            watch_state('native'); window = ready(window)
            watching_frame = (window, sorted(children(window)))
            watch_instance = seen_alias.split('-')[1]
            watch_generation = seen_generations[watch_instance]
            (root / 'watched-added').write_text('native event')
            while seen_generations[watch_instance] <= watch_generation:
                window = ready(window)
            enter_list(); key(window, b'F2')
            watched = packet(callback())
            assert watched[2:] == bytes([1, 0, 0]), watched
            window = ready(window)
            assert watching_frame == (window, sorted(children(window)))
            capture('file-browser-watch-refresh.png')
            moved = runtime.root / 'renamed-files'
            root.rename(moved)
            watch_state('manual')
            assert watching_frame == (window, sorted(children(window)))
            capture('file-browser-watch-fallback.png')
            moved.rename(root)
            key(window, b'F5'); callback(); window = ready(window)
            watch_state('native'); window = ready(window)
            capture('file-browser-watch-recovered.png')
            key(window, b'F8'); callback(); window = ready(window)
            watch_state('off')
            evidence['watch_native_refresh_loss_manual_recovery'] = True
            evidence['watch_preserves_input_selection_and_windows'] = True
            key(window,b'Escape');callback()
            wait_for_predicate('browser exit',lambda:app.poll() is not None,time.monotonic()+5)
            assert app.poll()==0
            retired_instances.add(seen_alias.split('-')[1])
            # Exercise the maximum row count and highest bitmap bit through real
            # Form parsing, drawing and MFL callback transport (under 1000 bytes).
            large=runtime.root/'large';large.mkdir()
            for i in range(4096):(large/('entry-%04d'%i)).touch()
            expected_children = 23
            app=runtime.start('large-browser',[str(args.browser),'--socket',str(endpoint),'--view','grid','--reveal',str(large/'entry-4095')],'large-browser.log',env)
            window=ready();capture('file-browser-4096-grid.png')
            large_frame=(window,sorted(children(window)))
            click(sort_buttons()[1][1]);callback();window=ready(window)
            capture('file-browser-4096-live-sort.png')
            click(sort_buttons()[1][1]);callback();window=ready(window)
            assert large_frame==(window,sorted(children(window)))
            evidence['4096_live_update_native_backpressure']=True
            enter_list();key(window,b'End')
            pointer.x.XTestFakeKeyEvent(d,shift,1,0);key(window,b'Home');pointer.x.XTestFakeKeyEvent(d,shift,0,0)
            key(window,b'F2');bits=packet(callback())
            assert bits[:2]==bytes([15,255]) and sum(v.bit_count() for v in bits[2:])==1 and bits[-1]==128
            window=ready(window)
            evidence['over_limit_grid_range_is_atomic']=True
            key(window,b'F7');callback();window=ready(window);capture('file-browser-4096-rows.png')
            enter_list();key(window,b'End');key(window,b'F2')
            text=callback();bits=packet(text)
            assert len(text)<1000 and len(bits)==514 and bits[:2]==bytes([15,255]) and bits[-1]==128,(len(text),bits)
            window=ready(window)
            enter_list();key(window,b'Home');key(window,b'space');key(window,b'F2')
            bits=packet(callback());assert bits[2]==1 and bits[-1]==128
            window=ready(window)
            enter_list();key(window,b'Home');key(window,b'Next');key(window,b'space');key(window,b'F2')
            bits=packet(callback());assert bits[:2]==bytes([0,12]) and bits[2]==1 and bits[3]==16 and bits[-1]==128
            window=ready(window)
            enter_list();key(window,b'End')
            box,control=widget()
            cx,cy,child=C.c_int(),C.c_int(),C.c_ulong()
            assert x.XTranslateCoordinates(d,control,client.root,box[2]//2,box[3]//2,C.byref(cx),C.byref(cy),C.byref(child))
            pointer.motion(cx.value,cy.value)
            capture('file-browser-wheel-before.png')
            for down in (1,0):assert pointer.x.XTestFakeButtonEvent(d,4,down,0)
            x.XSync(d,0)
            # A server/core barrier establishes input delivery; wheel scroll is
            # local and does not redraw the form through the helper.
            connection.echo('wheel-delivered')
            time.sleep(0.05)
            capture('file-browser-wheel-after.png')
            assert (artifact/'file-browser-wheel-before.png').read_bytes() != (artifact/'file-browser-wheel-after.png').read_bytes()
            key(window,b'F2');bits=packet(callback());assert bits[:2]==bytes([15,255])
            window=ready(window)
            evidence['keyboard_page_and_wheel_preserve_focus']=True
            evidence['4096_rows_two_offscreen_selections_bounded_callback'] = True
            key(window,b'Escape');callback()
            wait_for_predicate('large browser exit',lambda:app.poll() is not None,time.monotonic()+5)
            assert app.poll()==0 and not unexpected_errors,unexpected_errors
            # Opt-in live primitive, in the same actual desktop theme. The
            # browser keeps its current replacement lifecycle until its owner
            # adopts the native publication/status handshake.
            primitive = True
            alias = 'FvwmLiveCollection-ready'
            expected_children = 3
            commands = ['*' + alias + ': ' + value for value in [
                'Title "File browser"', 'Line left',
                'Label live-label 64 "Live collection: atomic reorder, insert and removal"',
                'Line left', 'Input draft 40 ""', 'Line left',
                'List live 64 6', 'ListVersioned',
                *['ListItem ' + ('on' if n == 11 else 'off') +
                  ' "Original entry %02d" file' % n for n in range(32)],
                'ListFocus 12', 'Line left', 'Button continue "Inspect state (F2)" F2',
                'Command Echo FVWM_FORM_UPDATED x$(live:Base64) x$(draft:Base64)',
                'Timeout 10 \"Echo FVWM_FORM_LEASE_EXPIRED\" \"\"']]
            for n, command in enumerate(commands):
                connection.send((command + '\n').encode())
                if n % 32 == 31:
                    connection.echo('live-config-%d' % n)
            connection.send(('Module FvwmForm ' + alias + '\n').encode())
            app = core
            window = ready()
            native_window = window
            native_children = sorted(children(window))
            _, _, values = inventory()
            field = min(values, key=lambda v: v[0][1])[1]
            click(field, False)
            for letter in b'keepdraft': key(window, bytes([letter]))
            key(window, b'Tab'); key(window, b'End'); key(window, b'space')

            def live_state():
                key(window, b'F2')
                while True:
                    text = connection.record().get('echo', {}).get('message', '')
                    if text.startswith('FVWM_FORM_UPDATED '):
                        fields = text.split()
                        result = [base64.urlsafe_b64decode(v[1:] + '==') for v in fields[1:]]
                        assert result[1] == b'keepdraft', result
                        return result[0]

            def update(command):
                if not command.startswith('RenewTimeout '):
                    connection.send(('SendToModule ' + alias + ' RenewTimeout 20\n').encode())
                connection.send(('SendToModule ' + alias + ' ' + command + '\n').encode())

            def native_status(token, published, pending=0, refused=0):
                update('ListStatus live ' + token)
                connection.expect_echo('FVWM_FORM_LIST %s %d %d %d' %
                    (token, published, pending, refused))

            connection.send(('SendToModule ' + alias + ' RenewTimeout 20\n').encode())
            connection.send(('SendToModule ' + alias + ' ListStatus live renewed\n').encode())
            connection.expect_echo('FVWM_FORM_LIST renewed 1 0 0')
            time.sleep(1.1)
            native_status('beyond-original-timeout',1)
            before = live_state()
            assert before[:6] == bytes([0, 0, 0, 1, 0, 31]), before
            capture('collection-update-before.png')
            update('ListBegin live 1 2')
            update('ListEntry live 2 0 "New unselected entry" directory')
            for old in range(32, 0, -1):
                update('ListEntry live 2 %d "Updated entry %02d" file' % (old, old - 1))
                if old == 16: connection.echo('staging-backpressure')
            native_status('staged', 1, 2)
            update('ListStatus live selected-state selection')
            while True:
                text=connection.record().get('echo',{}).get('message','')
                if text.startswith('FVWM_FORM_LIST selected-state '):
                    fields=text.split();assert fields[:5]==['FVWM_FORM_LIST','selected-state','1','2','0']
                    assert base64.urlsafe_b64decode(fields[5][1:]+'==')==before
                    break
            assert live_state() == before
            capture('collection-update-staged.png')
            # A selection edit made after Begin must survive publication.
            key(window, b'Home'); key(window, b'space')
            update('ListCommit live 2'); native_status('committed', 2)
            update('SetLabel live-label \"Published in the same native window\"')
            native_status('label-painted', 2)
            after = live_state()
            assert after[:6] == bytes([0, 0, 0, 2, 0, 32]), after
            assert after[6:] == bytes([2, 0, 32, 0, 1]), after
            assert inventory()[0] == native_window and sorted(children(window)) == native_children
            capture('collection-update-committed.png')
            # Direct updates leave fixed geometry intact and clip/clear labels.
            update('SetLabel live-label "' + 'W'*400 + '"')
            native_status('long-label', 2)
            assert inventory()[0]==native_window and sorted(children(window))==native_children
            capture('collection-label-clipped.png')
            update('SetLabel live-label "Short label"');native_status('short-label',2)
            capture('collection-label-cleared.png')
            connection.send(('*'+alias+': ListStatus live legacy-status\n').encode())
            connection.expect_echo('FVWM_FORM_LIST legacy-status 2 0 0')
            # Duplicate source refuses the entire candidate, including labels.
            update('ListBegin live 2 3')
            update('ListEntry live 3 1 "Never published" file')
            update('ListEntry live 3 1 "Duplicate source" file')
            update('ListCommit live 3'); native_status('poisoned', 2, 3, 1)
            assert live_state() == after
            update('ListAbort live 3'); native_status('aborted', 2)
            update('ListBegin live 2 3'); native_status('reuse-refused', 2)
            # Stale records cannot mutate or poison a newer transaction.
            update('ListBegin live 2 4')
            update('ListEntry live 3 999 "Stale record" file')
            update('ListCommit live 3'); update('ListAbort live 3')
            native_status('stale-refused', 2, 4)
            update('ListEntry live 4 2 "Surviving selection" file')
            update('ListCommit live 4'); native_status('removed', 4)
            result = live_state()
            assert result == bytes([0, 0, 0, 4, 0, 0, 1]), result
            capture('collection-update-removed.png')
            update('ListBegin live 4 5'); update('ListCommit live 5')
            native_status('empty', 5)
            assert live_state() == bytes([0, 0, 0, 5, 0, 0])
            capture('collection-update-empty.png')
            # Leave owned staging resources for normal module-exit cleanup.
            update('ListBegin live 5 6')
            update('ListEntry live 6 0 "Pending at teardown" other')
            native_status('closing', 5, 6)
            update('RenewTimeout 3')
            connection.expect_echo('FVWM_FORM_LEASE_EXPIRED')
            wait_for_predicate('expired primitive retired',lambda:inventory()[0] is None,time.monotonic()+5)
            evidence['renewed_timeout_and_orphan_expiry']=True
            connection.echo('live-update-finished')
            evidence['live_collection_updates'] = {
                'atomic_staging': True, 'current_input_at_commit': True,
                'identity_mapping': True, 'same_native_windows': True,
                'draft_preserved': True, 'duplicate_atomic_refusal': True,
                'stale_refusal': True, 'aborted_generation_reuse_refused': True,
                'abort_empty_remove_teardown': True,
                'versioned_callback_max_bytes': 518}
            evidence['status']='passed'
            evidence['screenshots']=['file-browser-collection.png','file-browser-scrolled-properties.png','file-browser-filtered-selection.png','file-browser-4096-rows.png']
            evidence['scope']='browser live updates and read-only paged metadata/current selection; typed actions, protected content and file mutation remain open'
        finally:
            if sys.exc_info()[0] is not None and 'capture' in locals():
                capture('file-browser-failure.png')
                if 'inventory' in locals():
                    (artifact / 'failure-inventory.json').write_text(json.dumps(inventory()))
            if connection:
                connection.close()
            if query_connection:
                query_connection.close()
            if pointer:
                pointer.button(False)
            if client:
                client.x.XSync(client.display, 0)
                if previous_handler is not None:
                    client.x.XSetErrorHandler.argtypes = [C.c_void_p]
                    client.x.XSetErrorHandler(previous_handler)
                client.close()
            for name in ['core.log', 'browser.log', 'large-browser.log', 'xserver.log']:
                path = runtime.root / name
                if path.exists():
                    shutil.copy2(path, artifact / name)
            (artifact / 'result.json').write_text(json.dumps(evidence, indent=2) + '\n')
    print(json.dumps(evidence))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('fvwm', 'mfl', 'form', 'browser', 'artifacts-dir'):
        parser.add_argument('--' + name, type=Path, required=True)
    parser.add_argument('--theme', choices=('fixture', 'slate-dark', 'dune-light',
                        'classic-large'), default='fixture')
    parser.add_argument('--command-client', type=Path)
    run(parser.parse_args())
