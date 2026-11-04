#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License, version 2 or later.
"""One frozen native drawing comparison case, using existing owned X/input tools.

Run correctness with private observation, and performance with observation off.
A missed active interruption is inconclusive and fails qualification; this script
never retries timing. No input-to-photon or implicit-roundtrip claim is made.
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
import tempfile
import time

from desktop_live import connect_client
from graphics_eval_records import (MAX_LOG_BYTES, TraceError, parse_records,
    qualify_lifecycle, require, parse_layout_record, layout_metadata)
from inspector_live import NativeView
from live_x_harness import HarnessRuntime, start_display_server, wait_for_predicate
from mfl_stream_live import Connection
from native_capture import NativeCapture
from native_layout_live import PROFILES, owned_module_pid, process_sample
from session_x_errors import SessionXErrors

CONTROLS = ('FVWM_GRAPHICS_EVAL_TRACE', 'FVWM_GRAPHICS_EVAL_MOTION',
    'FVWM_GRAPHICS_EVAL_REFUSE', 'FVWM_LAYOUT_EVAL_TRACE',
    'FVWM_LAYOUT_EVAL_REFUSE')
SCENARIOS = ('positive', 'choice-only', 'equal', 'replace', 'press', 'move',
    'resize', 'palette', 'unmap', 'destroy')
SOURCE_INPUTS = ('libs/graphics_eval.c', 'libs/graphics_eval.h',
    'libs/graphics_eval_policy.c', 'libs/graphics_eval_policy.h',
    'tests/test_graphics_eval_x.c',
    'libs/graphics_eval_observer.c', 'libs/graphics_eval_observer.h',
    'modules/FvwmForm/FvwmForm.c', 'modules/FvwmForm/ReadXServer.c',
    'modules/FvwmForm/form_graphics_eval.c',
    'modules/FvwmForm/form_graphics_eval.h', 'modules/FvwmForm/list_widget.c',
    'modules/FvwmButtons/FvwmButtons.c', 'modules/FvwmButtons/keyboard.c',
    'modules/FvwmButtons/draw.c', 'modules/FvwmButtons/buttons_graphics_eval.c',
    'modules/FvwmButtons/buttons_graphics_eval.h',
    'tests/live_x/graphics_eval_live.py', 'tests/live_x/graphics_eval_records.py',
    'tests/live_x/native_layout_live.py', 'tests/live_x/live_x_harness.py',
    'tests/live_x/inspector_live.py', 'tests/live_x/mfl_stream_live.py',
    'tests/live_x/session_x_errors.py', 'tests/live_x/native_capture.py',
    'fvwm/ConfigFvwmDefaults', 'default-config/palettes.json')


def digest(path, limit=32 * 1024 * 1024):
    require(path.is_file() and path.stat().st_size <= limit, 'input byte budget')
    raw = path.read_bytes()
    require(len(raw) <= limit, 'input grew past byte budget')
    return hashlib.sha256(raw).hexdigest()


def input_record(path):
    digest(path, 65536)
    value = json.loads(path.read_text())
    require(isinstance(value, dict) and value.get('schema_version') == 1,
        'comparison input record version')
    for key in ('source_manifest_sha256', 'source_tree'):
        size = 64 if key.endswith('sha256') else 40
        require(isinstance(value.get(key), str) and len(value[key]) == size and
            all(c in '0123456789abcdef' for c in value[key]), 'missing source binding')
    require(value.get('execution_profile') in ('native-cpu', 'emulated-cpu') and
        all(value.get(key) for key in ('compiler', 'libraries', 'features', 'server')),
        'missing executor/tool/library/server profile')
    return value


def form_configuration(font, scenario):
    lines = ['Title "G3 native collection"', f'Font "{font}"',
        'Colorset 200', 'ItemColorset 203', 'Line left',
        'Selection flags multiple', 'Choice flag yes off "Toggle native choice"']
    if scenario != 'choice-only':
        lines += ['Line left', 'List picks 28 2', 'ListViewport 12 2',
            'ListVersioned',
            *['ListItem off "Native item %02d" file' % n for n in range(16)]]
    # Direct $(flag) expands the configured value regardless of selection.
    lines += ['Line left', 'Button continue "Observe (F2)" F2',
        'Command Echo G3_FORM_ACTION x$(flag?yes)']
    return lines


def require_form_action(evidence, name, actual, selected):
    expected = 'G3_FORM_ACTION ' + ('xyes' if selected else 'x')
    evidence[name] = actual
    evidence[name + '_expected'] = expected
    require(actual == expected, 'committed choice state/action ' + name)


def layout_reflow_endpoint(previous_width, requested_width, client_geometry,
        root_position, prepared_rectangles):
    """Pending geometry is false; malformed observations remain hard errors.

    This is the controlled row's endpoint, not a general layout contract. It
    accepts different width distributions only when every action still occupies
    a positive adjacent outer rectangle covering the requested client extent.
    """
    require(type(previous_width) is int and type(requested_width) is int and
        previous_width in (800, 880) and requested_width in (800, 880) and
        previous_width != requested_width, 'layout reflow requires a size change')
    require(isinstance(client_geometry, (list, tuple)) and
        len(client_geometry) == 4 and
        all(type(value) is int for value in client_geometry) and
        all(1 <= value <= 32767 for value in client_geometry[2:]),
        'malformed layout client geometry')
    require(isinstance(root_position, (list, tuple)) and len(root_position) == 2
        and all(type(value) is int for value in root_position),
        'malformed layout root position')
    require(isinstance(prepared_rectangles, (list, tuple)) and
        len(prepared_rectangles) == 3, 'malformed layout rectangle count')
    for box in prepared_rectangles:
        require(isinstance(box, (list, tuple)) and len(box) == 4 and
            all(type(value) is int for value in box) and
            all(1 <= value <= 32767 for value in box[2:]),
            'malformed layout prepared rectangle')
    root_x, root_y = root_position
    return (list(client_geometry[2:]) == [requested_width, 64] and
        all(top == root_y and height == 64
            for _, top, _, height in prepared_rectangles) and
        prepared_rectangles[0][0] == root_x and
        all(left[0] + left[2] == right[0]
            for left, right in zip(prepared_rectangles, prepared_rectangles[1:])) and
        sum(box[2] for box in prepared_rectangles) == requested_width and
        prepared_rectangles[-1][0] + prepared_rectangles[-1][2] ==
            root_x + requested_width)


def _run(args, evidence, out):
    require(sys.platform.startswith('linux'), 'this owned-PID fixture requires Linux')
    source = Path(__file__).resolve().parents[2]
    build = args.build.resolve()
    record = input_record(args.input_record)
    layout = getattr(args, 'layout', None)
    layout_case = getattr(args, 'layout_case', 'fit')
    layout_refuse = getattr(args, 'layout_refuse', False)
    layout_observed = args.mode == 'correctness' and layout is not None
    observed = args.mode == 'correctness' and args.variant != 'ordinary'
    evidence.update(dict(status='running', variant=args.variant, profile=args.profile,
        scenario=args.scenario, mode=args.mode, observed=observed,
        motion=args.motion, injected_refusal=args.refuse, screenshots=[],
        layout=layout, layout_case=layout_case, layout_observed=layout_observed,
        layout_refuse=layout_refuse,
        layout_policy='independent Clay equalization; native weighted surplus retained',
        input_record=dict(sha256=digest(args.input_record), value=record),
        source_sha256={name: digest(source / name) for name in SOURCE_INPUTS},
        generated_config_sha256=None, binaries={}, startup={}, action_latency_ms=[],
        latency_scope='semantic command receipt; includes harness/query overhead',
        implicit_roundtrips=None, process_observations={}, declared_idle_seconds=.4))
    if layout:
        names = ('libs/clay_layout_eval.c', 'libs/clay_layout_eval.h',
            'libs/ui_layout.c', 'libs/ui_layout.h',
            'modules/FvwmButtons/button.c',
            'modules/FvwmButtons/buttons_layout_eval.c',
            'modules/FvwmButtons/buttons_layout_eval.h',
            'tests/third_party/clay/clay.h', 'tests/third_party/clay/LICENSE.md',
            'tests/third_party/clay/SOURCE.md')
        evidence['source_sha256'].update({name: digest(source / name) for name in names})
        if args.mode == 'performance':
            evidence.update(layout_reflow_latency_ms=[],
                layout_reflow_scope='XResizeWindow through changed actual client '
                    'extent plus complete queried outer prepared rectangles; includes '
                    'server/WM/module, IPC, query and polling overhead; '
                    'not solver-only or input-to-photon latency',
                layout_reflow_widths=[800, 880],
                layout_reflow_content_insets=[0, 0, 0, 0])
    started = time.monotonic()
    core = connection = client = errors = view = None
    with HarnessRuntime(prefix='fvwm-g3-') as runtime:
        try:
            server_path = Path(shutil.which('Xvfb') or '/unavailable/Xvfb')
            evidence['server_binary_sha256'] = digest(server_path)
            _, display = start_display_server(runtime, 'Xvfb',
                [str(server_path), '-screen', '0', '1440x900x24', '-noreset'],
                'xserver.log', time.monotonic() + 8)
            if args.shape_probe:
                probe = build / 'tests/test-graphics-eval-x'
                evidence['binaries']['shape_probe'] = dict(path=str(probe), sha256=digest(probe))
                child = runtime.start('shape-probe', [str(probe)], 'shape-probe.log',
                    runtime.environment({'DISPLAY': display}))
                require(child.wait_until_exit(time.monotonic() + 15) == 0,
                    'real-X shape/fallback probe failed')
                result = json.loads(child.log_path.read_text())
                require(result.get('cairo') is True and result.get('x_errors') == 0 and
                    result.get('own_resources_retired') is True, 'shape probe contract')
                evidence['shape_probe'] = result
            modules, user = runtime.root / 'modules', runtime.root / 'user'
            modules.mkdir()
            user.mkdir()
            suffix = '' if args.variant == 'ordinary' else '-' + args.variant + '-eval'
            selected = {}
            for module in ('FvwmForm', 'FvwmButtons', 'FvwmMFL'):
                name = module + (suffix if module != 'FvwmMFL' else '')
                if layout and module == 'FvwmButtons':
                    name = module + '-' + layout + '-layout-eval'
                binary = build / 'modules' / module / name
                evidence['binaries'][module] = dict(path=str(binary), sha256=digest(binary))
                (modules / module).symlink_to(binary)
                selected[module] = binary
            evidence['binaries']['core'] = dict(path=str(build / 'fvwm3'),
                sha256=digest(build / 'fvwm3'))
            evidence['generated_config_header_sha256'] = digest(build / 'config.h')
            palette_data = json.loads((source / 'default-config/palettes.json').read_text())
            palette = palette_data['dune']['light'] if args.profile == 'dune-light' else palette_data['slate']['dark']
            font = ('xft:DejaVu Sans Mono:size=18:antialias=True'
                if args.profile == 'classic-large' else 'xft:Sans:size=10:antialias=True')
            text, surface, accent, border = (palette[k] for k in
                ('text', 'surface', 'accent', 'border'))
            choice_fg, choice_bg, hi, lo = text, surface, accent, border
            if args.profile == 'classic-large':
                choice_fg, choice_bg, hi, lo = 'black', 'ivory', 'white', 'black'
            focus = text if args.scenario == 'equal' else accent
            if args.scenario == 'equal': hi = lo
            config = runtime.root / 'config'
            config.write_text(f'ModulePath {modules}\nRead {source / "fvwm/ConfigFvwmDefaults"}\n'
                f'DefaultFont "{font}"\nColorset 200 fg {text}, bg {surface}\n'
                f'Colorset 201 fg {text}, bg {surface}\n'
                f'Colorset 202 fg {focus}, bg {surface}\n'
                f'Colorset 203 fg {choice_fg}, bg {choice_bg}, hi {hi}, sh {lo}\n'
                'Style * Colorset 200, HilightColorset 202, VariableSize, VariablePosition\n'
                'Style DrawingRow PositionPlacement 20 30\n'
                'Style DrawingForm PositionPlacement 20 180\n'
                'DestroyFunc StartFunction\nAddToFunc StartFunction I Module FvwmMFL\n')
            shutil.copyfile(config, out / 'generated.config')
            evidence['generated_config_sha256'] = digest(config)
            evidence['font_request'] = font
            evidence['palette_controls'] = dict(palette=palette,
                choice=[choice_fg, choice_bg, hi, lo], focus=focus,
                scope='controlled native-layout subset; no application/toolkit theme claim')
            env = runtime.environment({'DISPLAY': display, 'HOME': str(runtime.root),
                'FVWM_USERDIR': str(user), 'FVWMMFL_SOCKET_PATH': str(runtime.root)})
            for name in CONTROLS: env.pop(name, None)
            if args.motion == 'full': env['FVWM_GRAPHICS_EVAL_MOTION'] = 'full'
            if args.refuse: env['FVWM_GRAPHICS_EVAL_REFUSE'] = '1'
            if observed: env['FVWM_GRAPHICS_EVAL_TRACE'] = '1'
            if layout_observed: env['FVWM_LAYOUT_EVAL_TRACE'] = '1'
            if layout_refuse: env['FVWM_LAYOUT_EVAL_REFUSE'] = '1'
            core_started = time.monotonic()
            core = runtime.start('core', [str(build / 'fvwm3'), '-d', display,
                '-f', str(config)], 'core.log', env)
            endpoint = runtime.root / ('fvwm_mfl_' + display.replace(':', '_') + '.sock')
            wait_for_predicate('drawing MFL endpoint', endpoint.exists,
                time.monotonic() + 8, process=core)
            connection = Connection(endpoint)
            connection.record()
            connection.send(b'set echo\n')
            connection.echo('drawing-started')
            evidence['startup']['core_mfl_endpoint_ms'] = (time.monotonic() - core_started) * 1000
            client = connect_client(display)
            errors = SessionXErrors(client, evidence).install()
            view = NativeView(client, errors, core)
            x, d = client.x, client.display
            for name, arguments in (
                ('XSetInputFocus', [C.c_void_p, C.c_ulong, C.c_int, C.c_ulong]),
                ('XTranslateCoordinates', [C.c_void_p, C.c_ulong, C.c_ulong,
                    C.c_int, C.c_int, C.POINTER(C.c_int), C.POINTER(C.c_int), C.POINTER(C.c_ulong)]),
                ('XUnmapWindow', [C.c_void_p, C.c_ulong])):
                getattr(x, name).argtypes = arguments
                getattr(x, name).restype = C.c_int
            x.XFlush.argtypes = [C.c_void_p]
            x.XFlush.restype = C.c_int
            serial = 0

            def command(text):
                connection.send((text + '\n').encode())
                connection.echo('drawing-command')

            def receipt(prefix):
                until = time.monotonic() + 4
                while time.monotonic() < until:
                    message = connection.record().get('echo', {}).get('message', '')
                    if message.startswith(prefix): return message
                raise AssertionError('missing drawing receipt ' + prefix)

            def configure(alias, lines):
                for line in lines:
                    connection.send(('*' + alias + ': ' + line + '\n').encode())
                connection.echo('drawing-configured')

            def geometry(window):
                a = view.attributes(window)
                require(a is not None and a.map_state == 2, 'unmapped native geometry')
                return [a.x, a.y, a.width, a.height]

            def root_position(window):
                rx, ry, child = C.c_int(), C.c_int(), C.c_ulong()
                require(x.XTranslateCoordinates(d, window, client.root, 0, 0,
                    C.byref(rx), C.byref(ry), C.byref(child)), 'native root position')
                return rx.value, ry.value

            def focus_window(window):
                x.XSetInputFocus(d, window, 1, 0)
                x.XSync(d, 0)
                wait_for_predicate('drawing focus', lambda: view.focus() == window,
                    time.monotonic() + 3, process=core)
                time.sleep(.02)  # Trace, not this delay, must prove active work.

            def click(window):
                box = geometry(window)
                rx, ry, child = C.c_int(), C.c_int(), C.c_ulong()
                require(x.XTranslateCoordinates(d, window, client.root,
                    box[2] // 2, box[3] // 2, C.byref(rx), C.byref(ry), C.byref(child)),
                    'native pointer translation')
                view.pointer.motion(rx.value, ry.value)
                view.pointer.button(True)
                time.sleep(.03)
                view.pointer.button(False)
                x.XSync(d, 0)

            def bind_pid(module, alias):
                pid = wait_for_predicate('owned drawing module PID',
                    lambda: owned_module_pid(core.process.pid, module, alias),
                    time.monotonic() + 3, process=core)
                actual = Path('/proc') / str(pid) / 'exe'
                require(digest(actual) == evidence['binaries'][module]['sha256'],
                    'owned executable identity changed')
                evidence['binaries'][module]['pid'] = pid
                return pid

            row_width = 200 if layout and layout_case == 'compressed' else 800
            beta_weight = 2 if layout and layout_case == 'weighted' else 1
            row_lines = [f'Geometry {row_width}x64+20+30', 'Rows 1', 'Layout content',
                'Frame 1', 'Padding 8 3', f'Font "{font}"', 'Colorset 201',
                'FocusColorset 202', 'KeyboardNavigation 1',
                '(1x1, Id alpha, Title "First action", Action "Echo G3_BUTTON_alpha")',
                f'({beta_weight}x1, Id beta, Title "Second action", Action "Echo G3_BUTTON_beta")',
                '(1x1, Id gamma, Title "Third action", Action "Echo G3_BUTTON_gamma")']
            form_lines = form_configuration(font, args.scenario)
            configure('DrawingRow', row_lines)
            configure('DrawingForm', form_lines)
            for module, alias, title in (('FvwmButtons', 'DrawingRow', 'DrawingRow'),
                    ('FvwmForm', 'DrawingForm', 'G3 native collection')):
                before = time.monotonic()
                command('Module ' + module + ' ' + alias)
                window = view.wait(title)
                pid = bind_pid(module, alias)
                evidence['startup'][module + '_mapped_owned_elf_ms'] = (time.monotonic() - before) * 1000
                if module == 'FvwmButtons': row, row_pid = window, pid
                else: form, form_pid = window, pid
            evidence['startup']['scope'] = 'mapped title plus owned ELF; includes harness/IPC/query overhead'
            child_boxes = [(child, geometry(child)) for child in view.children(form)]
            choices = [child for child, box in child_boxes if box[2] == box[3]]
            require(len(choices) == 1, 'one fixed native choice expected')
            choice = choices[0]
            evidence['initial_geometry'] = dict(buttons=geometry(row), form=geometry(form),
                choice=geometry(choice), child_ids=sorted(view.children(form)))

            def query_rectangles():
                nonlocal serial
                result, queries = [], []
                latest = dict(window_xid=row, module_pid=row_pid,
                    client_geometry=None, root_position=None, queries=[],
                    prepared_rectangles=None)
                # Replace this bounded snapshot per query, not a polling log.
                # Save replies before parsing or asserting their geometry.
                evidence['latest_rectangle_query'] = latest
                for target in ('alpha', 'beta', 'gamma'):
                    serial += 1
                    tag = 'G3_RECT_%d ' % serial
                    query = dict(control=target, token=tag.rstrip(),
                        raw_reply=None, reply_bytes=None)
                    latest['queries'].append(query)
                    connection.send(('SendToModule DrawingRow ExpandButtonVars ' + target +
                        ' Echo ' + tag + '$left $top $width $height\n').encode())
                    reply = receipt(tag)
                    encoded = reply.encode('utf-8', 'backslashreplace')
                    query['raw_reply'] = encoded[:1024].decode('utf-8', 'ignore')
                    query['reply_bytes'] = len(encoded)
                    require(len(encoded) <= 1024, 'native rectangle reply byte budget')
                latest['client_geometry'] = geometry(row)
                latest['root_position'] = list(root_position(row))
                for query in latest['queries']:
                    fields = query['raw_reply'].split()
                    require(query['raw_reply'].isascii() and len(fields) == 5 and
                        fields[0] == query['token'], 'malformed native rectangle reply')
                    result.append(list(map(int, fields[1:])))
                    queries.append(dict(control=query['control'], token=query['token']))
                latest['prepared_rectangles'] = result
                return result, queries

            def rectangles():
                result, _ = query_rectangles()
                latest = evidence['latest_rectangle_query']
                root_x, root_y = latest['root_position']
                client_geometry = latest['client_geometry']
                # Global Frame/Padding are child defaults. ExpandButtonVars
                # reports outer buttons; the unflagged root has no own inset.
                require(client_geometry[3] == 64 and
                    all(w > 0 and h == 64 and top == root_y
                    for _, top, w, h in result) and
                    all(a[0] + a[2] == b[0] for a, b in zip(result, result[1:])) and
                    result[0][0] == root_x and
                    result[-1][0] + result[-1][2] == root_x + client_geometry[2] and
                    sum(box[2] for box in result) == client_geometry[2],
                    'native outer draw/hit rectangles cover the client')
                return result

            def reflow_snapshot(previous_width, requested_width):
                before_queries = geometry(row)
                if before_queries[2:] != [requested_width, 64]:
                    return False
                result, queries = query_rectangles()
                latest = evidence['latest_rectangle_query']
                position = latest['root_position']
                after_queries = latest['client_geometry']
                if not (layout_reflow_endpoint(previous_width, requested_width,
                        before_queries, position, result) and
                        layout_reflow_endpoint(previous_width, requested_width,
                        after_queries, position, result)):
                    return False
                return dict(previous_width=previous_width,
                    requested_width=requested_width, requested_height=64,
                    client_before_queries=before_queries,
                    client_after_queries=after_queries, root_position=position,
                    prepared_rectangles=result, queries=queries,
                    window_xid=row, module_pid=row_pid)

            def list_state():
                nonlocal serial
                serial += 1
                token = 'drawing%d' % serial
                connection.send(('SendToModule DrawingForm ListStatus picks ' +
                    token + ' selection\n').encode())
                fields = receipt('FVWM_FORM_LIST ' + token + ' ').split()
                require(len(fields) == 6 and fields[3:5] == ['0', '0'] and
                    fields[5].startswith('x') and len(fields[5]) <= 1024,
                    'native collection status')
                encoded = fields[5][1:]
                packet = base64.b64decode(encoded + '=' * (-len(encoded) % 4),
                    altchars=b'-_', validate=True)
                require(6 <= len(packet) <= 518 and
                    int.from_bytes(packet[:4], 'big') == int(fields[2]),
                    'native collection snapshot generation')
                return packet

            def pixel(window, px, py):
                image = x.XGetImage(d, window, px, py, 1, 1, C.c_ulong(-1).value, 2)
                require(image, 'owned pixel observation')
                try: return x.XGetPixel(image, 0, 0)
                finally: x.XDestroyImage(image)

            evidence['prepared_rectangles'] = rectangles()
            if layout and layout_case == 'update':
                command('SendToModule DrawingRow ChangeButtons '
                    '(alpha Title "Open café", ActiveTitle "Long translated action") '
                    '(gamma Title "Third changed action")')
                evidence['layout_after_update'] = rectangles()
                # Clay may keep equalized widths; semantic identity and a
                # later reflow witness matter, not a forced pixel difference.
            initial_list = list_state() if args.scenario != 'choice-only' else None
            if initial_list is not None:
                evidence['initial_list_status'] = initial_list.hex()
            # Queries happen before admission: G2 intentionally treats M_STRING
            # as a visual interruption. An Echo on the core is not a module query.
            stamp = lambda: time.clock_gettime_ns(time.CLOCK_MONOTONIC) // 1000000
            positive_bounds, stimulus_bounds = {}, {}
            focus_window(row)
            positive_bounds['buttons'] = [stamp()]
            view.key('Tab')
            time.sleep(.2)
            positive_bounds['buttons'].append(stamp())
            view.key('Return')
            require(receipt('G3_BUTTON_') == 'G3_BUTTON_beta', 'positive focus/action identity')
            if layout:
                target = rectangles()[2]
                view.pointer.motion(target[0] + target[2] // 2,
                    target[1] + target[3] // 2)
                before_pointer = time.monotonic()
                view.pointer.button(True)
                time.sleep(.03)
                view.pointer.button(False)
                x.XSync(d, 0)
                require(receipt('G3_BUTTON_') == 'G3_BUTTON_gamma',
                    'private prepared rectangle pointer/action identity')
                evidence['layout_pointer_action_ms'] = (
                    time.monotonic() - before_pointer) * 1000
            focus_window(form)
            positive_bounds['form'] = [stamp()]
            click(choice)
            time.sleep(.2)
            positive_bounds['form'].append(stamp())
            view.key('F2')
            require_form_action(evidence, 'initial_form_action',
                receipt('G3_FORM_ACTION '), True)

            def interrupt(window, module):
                if args.scenario == 'replace':
                    view.key('Tab') if module == 'buttons' else click(choice)
                elif args.scenario == 'press':
                    view.key('Return' if module == 'buttons' else 'F2')
                    receipt('G3_BUTTON_' if module == 'buttons' else 'G3_FORM_ACTION ')
                elif args.scenario == 'move':
                    command('WindowId 0x%x Move 80p %dp' % (window, 90 if module == 'buttons' else 250))
                elif args.scenario == 'resize':
                    box = geometry(window)
                    x.XResizeWindow(d, window, box[2] + 80, box[3])
                    x.XSync(d, 0)
                    wait_for_predicate('drawing resize',
                        lambda: geometry(window)[2] == box[2] + 80,
                        time.monotonic() + 3, process=core)
                elif args.scenario == 'palette':
                    command('Colorset %d fg %s, bg %s, hi %s, sh %s' %
                        (202 if module == 'buttons' else 203, text, surface, text, border))
                elif args.scenario in ('unmap', 'destroy'):
                    (x.XUnmapWindow if args.scenario == 'unmap' else x.XDestroyWindow)(d, window)
                    x.XSync(d, 0)

            flag_on = True
            if args.mode == 'performance':
                evidence['animation_settle_seconds'] = .15
                evidence['process_observations']['workload_before'] = {
                    name: process_sample(pid) for name, pid in
                    (('buttons', row_pid), ('form', form_pid))}
                for index in range(args.samples):
                    if layout:
                        previous_width = geometry(row)[2]
                        require(previous_width in (800, 880),
                            'layout reflow left the declared width pair')
                        requested_width = 880 if previous_width == 800 else 800
                        require(requested_width != previous_width,
                            'layout reflow cannot time a no-op resize')
                        before = time.monotonic()
                        x.XResizeWindow(d, row, requested_width, 64)
                        x.XFlush(d)
                        snapshot = wait_for_predicate('private layout reflow',
                            lambda: reflow_snapshot(previous_width, requested_width),
                            time.monotonic() + 3, process=core)
                        snapshot.update(sample=index,
                            elapsed=(time.monotonic() - before) * 1000)
                        evidence['layout_reflow_latency_ms'].append(snapshot)
                    focus_window(row)
                    view.key('Tab')
                    time.sleep(.15)  # Fixed pacing outside the receipt timer.
                    before = time.monotonic()
                    view.key('Return')
                    receipt('G3_BUTTON_')
                    evidence['action_latency_ms'].append(dict(module='buttons',
                        sample=index, elapsed=(time.monotonic() - before) * 1000))
                    focus_window(form)
                    click(choice)
                    flag_on = not flag_on
                    time.sleep(.15)
                    before = time.monotonic()
                    view.key('F2')
                    receipt('G3_FORM_ACTION ')
                    evidence['action_latency_ms'].append(dict(module='form',
                        sample=index, elapsed=(time.monotonic() - before) * 1000))
                evidence['process_observations']['workload_after'] = {
                    name: process_sample(pid) for name, pid in
                    (('buttons', row_pid), ('form', form_pid))}
            elif args.scenario not in ('positive', 'choice-only', 'equal'):
                focus_window(row)
                stimulus_bounds['buttons'] = [stamp()]
                view.key('Tab')
                interrupt(row, 'buttons')
                time.sleep(.2)
                stimulus_bounds['buttons'].append(stamp())
                if args.scenario == 'palette':
                    boxes = rectangles()
                    rx, ry = root_position(row)
                    actual = pixel(row, boxes[2][0] - rx + 2, boxes[2][1] - ry + 2)
                    require(actual == int(text[1:], 16), 'current Buttons focus palette')
                    evidence['palette_buttons_pixel'] = actual
                focus_window(form)
                stimulus_bounds['form'] = [stamp()]
                click(choice)
                interrupt(form, 'form')
                time.sleep(.2)
                stimulus_bounds['form'].append(stamp())
                if args.scenario == 'palette':
                    actual = pixel(choice, 0, 0)
                    require(actual == int(text[1:], 16), 'current Form bevel palette')
                    evidence['palette_form_pixel'] = actual
            else:
                focus_window(row)
                view.key('Return')
                receipt('G3_BUTTON_')
                focus_window(form)
                view.key('F2')
                receipt('G3_FORM_ACTION ')
            if args.scenario not in ('unmap', 'destroy'):
                require(geometry(choice)[2:] == evidence['initial_geometry']['choice'][2:],
                    'choice square changed')
                evidence['final_geometry'] = dict(buttons=geometry(row), form=geometry(form),
                    choice=geometry(choice))
                require(sorted(view.children(form)) == evidence['initial_geometry']['child_ids'],
                    'native child identity changed')
                evidence['final_rectangles'] = rectangles()
                if initial_list is not None:
                    final_list = list_state()
                    require(final_list == initial_list, 'collection selection/focus generation changed')
                    evidence['final_list_status'] = final_list.hex()
                focus_window(form)
                view.key('F2')
                final_action = receipt('G3_FORM_ACTION ')
                expected_on = flag_on if args.mode == 'performance' else args.scenario in (
                    'positive', 'choice-only', 'equal', 'replace')
                require_form_action(evidence, 'final_form_action', final_action,
                    expected_on)
                evidence['pixels'] = dict(buttons=view.pixels(row), form=view.pixels(form))
            if args.capture:
                require(args.scenario in ('positive', 'choice-only', 'equal'),
                    'capture the stable positive controls')
                helper = build / 'bin/fvwm-desktop-capture'
                evidence['binaries']['capture'] = dict(path=str(helper), sha256=digest(helper))
                previous = os.environ.get('DISPLAY')
                os.environ['DISPLAY'] = display
                try:
                    image = Path(NativeCapture(helper).screenshot(out).removeprefix('Screenshot saved: '))
                    image.rename(out / 'native-drawing.png')
                    evidence['screenshots'].append('native-drawing.png')
                finally:
                    if previous is None: os.environ.pop('DISPLAY', None)
                    else: os.environ['DISPLAY'] = previous
            before = {name: process_sample(pid) for name, pid in
                (('buttons', row_pid), ('form', form_pid))}
            time.sleep(.4)
            after = {name: process_sample(pid) for name, pid in
                (('buttons', row_pid), ('form', form_pid))}
            evidence['process_observations']['idle'] = dict(before=before, after=after,
                scope='Linux owned-process CPU ticks/RSS/HWM; not a zero-wakeup proof')
            evidence['retired_before_kill'] = {name: not (Path('/proc') / str(pid)).exists()
                for name, pid in (('buttons', row_pid), ('form', form_pid))}
            for module, alias, pid in (('FvwmButtons', 'DrawingRow', row_pid),
                    ('FvwmForm', 'DrawingForm', form_pid)):
                command('KillModule ' + module + ' ' + alias)
                wait_for_predicate('owned drawing module exit',
                    lambda: not (Path('/proc') / str(pid)).exists(),
                    time.monotonic() + 4, process=core)
            connection.send(b'Quit\n')
            require(core.wait_until_exit(time.monotonic() + 4) == 0,
                'core did not exit successfully')
            log = runtime.root / 'core.log'
            require(log.stat().st_size <= MAX_LOG_BYTES, 'core log byte budget')
            raw = log.read_bytes()
            expected = {row_pid: ('buttons', args.variant), form_pid: ('form', args.variant)} if observed else {}
            streams = parse_records(raw, expected)
            evidence['observer_streams'] = streams
            evidence['layout_record'] = parse_layout_record(raw,
                row_pid if layout_observed else None,
                layout if layout_observed else None,
                'refuse' if layout_refuse else layout_case,
                layout_metadata([('Open café', 'Long translated action', None),
                    ('Second action', None, None),
                    ('Third changed action', None, None)])
                if layout and layout_case == 'update' else None)
            evidence['stimulus_clock'] = 'CLOCK_MONOTONIC floor milliseconds'
            evidence['positive_bounds_ms'] = positive_bounds
            evidence['stimulus_bounds_ms'] = stimulus_bounds
            evidence['qualification'] = {}
            immediate = args.motion == 'reduced' or args.scenario == 'equal'
            for pid, trace in streams.items():
                module = trace['header']['module']
                reasons = {'replace': {1, 2}, 'press': {2, 9}, 'move': {5},
                    'resize': {5, 7}, 'palette': {2, 7}, 'unmap': {3}, 'destroy': {3, 4}}.get(args.scenario, set())
                if module == 'form' and args.scenario == 'press': reasons = set()
                retirement = args.scenario in ('unmap', 'destroy') and evidence['retired_before_kill'][module]
                result = qualify_lifecycle(trace, require_positive=not immediate,
                    positive_bounds=positive_bounds[module],
                    # The positive Buttons Tab targets beta (2); the later
                    # interruption Tab targets gamma (3). Form has one choice.
                    expected_identity=choice if module == 'form' else 2,
                    immediate=immediate, require_native=args.variant == 'native' or args.refuse,
                    require_cairo=args.variant == 'cairo' and not args.refuse)
                if reasons and not immediate:
                    target = qualify_lifecycle(trace, require_interruption=True,
                        interruption_bounds=stimulus_bounds[module],
                        expected_identity=choice if module == 'form' else 3,
                        interruption_reasons=reasons, retirement_exit=retirement)
                    result['targeted_interruption'] = target
                if args.refuse:
                    require(trace['stats']['refused'] > 0 and trace['stats']['committed'] == 0,
                        'injected refusal did not retain native fallback')
                evidence['qualification'][module] = result
            require(not errors.unexpected, 'unexpected observer X errors')
            text_log = raw.decode('utf-8', 'replace')
            require(not any(marker in text_log for marker in
                ('ERROR: AddressSanitizer', 'ERROR: LeakSanitizer', 'runtime error:',
                 'BadWindow', 'BadMatch', 'Assertion')), 'module/core X or sanitizer error')
            evidence['status'] = 'body passed; runtime cleanup pending'
        except BaseException as error:
            evidence.update(status='failed; retained', error=repr(error))
            raise
        finally:
            failures = []
            if view:
                try: view.pointer.button(False)
                except BaseException as error: failures.append(repr(error))
            if connection:
                try: connection.close()
                except BaseException as error: failures.append(repr(error))
            if core:
                try: core.close(time.monotonic() + 4)
                except BaseException as error: failures.append(repr(error))
            if errors:
                try: errors.close_display()
                except BaseException as error: failures.append(repr(error))
            elif client:
                try: client.close()
                except BaseException as error: failures.append(repr(error))
            for path in runtime.root.glob('*.log'):
                require(path.stat().st_size <= MAX_LOG_BYTES, 'owned log byte budget')
                shutil.copyfile(path, out / path.name)
            if failures or (errors and errors.unexpected):
                evidence.update(status='failed; retained', cleanup_failures=failures)
            evidence['elapsed_seconds'] = time.monotonic() - started
            if failures or (errors and errors.unexpected):
                raise RuntimeError('owned drawing cleanup or X observation failed')
    return evidence


def run(args):
    base = args.artifacts_dir.resolve()
    base.mkdir(parents=True, exist_ok=True)
    out = Path(tempfile.mkdtemp(prefix=args.profile + '-' + args.variant + '-', dir=base))
    evidence = dict(status='running', artifact_directory=str(out))
    started = time.monotonic()
    try:
        _run(args, evidence, out)
        evidence['status'] = 'passed'
    except BaseException as error:
        if evidence.get('error') and evidence['error'] != repr(error):
            evidence['outer_cleanup_error'] = repr(error)
        evidence.setdefault('error', repr(error))
        evidence['status'] = 'failed; retained'
        raise
    finally:
        evidence['elapsed_total_seconds'] = time.monotonic() - started
        (out / 'result.json').write_text(json.dumps(evidence, indent=2) + '\n')
    return evidence


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__, allow_abbrev=False)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--input-record', type=Path, required=True)
    parser.add_argument('--artifacts-dir', type=Path, required=True)
    parser.add_argument('--profile', choices=PROFILES, required=True)
    parser.add_argument('--variant', choices=('ordinary', 'native', 'cairo'), required=True)
    parser.add_argument('--scenario', choices=SCENARIOS, default='positive')
    parser.add_argument('--mode', choices=('correctness', 'performance'), default='correctness')
    parser.add_argument('--motion', choices=('full', 'reduced'), default='full')
    parser.add_argument('--refuse', action='store_true')
    parser.add_argument('--capture', action='store_true')
    parser.add_argument('--shape-probe', action='store_true')
    parser.add_argument('--layout', choices=('native', 'clay'))
    parser.add_argument('--layout-case',
        choices=('fit', 'weighted', 'compressed', 'update'), default='fit')
    parser.add_argument('--layout-refuse', action='store_true')
    parser.add_argument('--samples', type=int, choices=range(1, 33), default=12)
    args = parser.parse_args()
    if args.layout and (args.variant != 'ordinary' or args.shape_probe or args.refuse):
        parser.error('layout comparison uses ordinary drawing without graphics injection')
    if not args.layout and (args.layout_case != 'fit' or args.layout_refuse):
        parser.error('layout controls require a private layout variant')
    if args.layout and args.scenario not in ('positive', 'resize', 'move'):
        parser.error('layout comparison uses stable action/update or live geometry cases')
    if args.layout and args.mode == 'performance' and (
            args.layout_case != 'fit' or args.layout_refuse):
        parser.error('layout performance uses fitted uninjected controls')
    if args.layout_refuse and args.layout_case not in ('fit', 'weighted', 'compressed'):
        parser.error('keep update and explicit refusal evidence separate')
    if args.shape_probe and (args.mode != 'correctness' or args.variant != 'cairo' or
            args.scenario != 'positive'):
        parser.error('shape probe belongs to one Cairo positive correctness case')
    if args.mode == 'performance' and (args.scenario != 'positive' or args.refuse or args.capture):
        parser.error('performance uses the positive controls without refusal/capture')
    def deadline(number, frame):
        raise TimeoutError('drawing comparison exceeded 60 seconds')
    signal.signal(signal.SIGALRM, deadline)
    signal.alarm(60)
    result = run(args)
    signal.alarm(0)
    print(json.dumps({key: result[key] for key in ('status', 'profile', 'variant', 'scenario')}))
