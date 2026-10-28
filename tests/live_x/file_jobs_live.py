#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
"""Exercise native file-job controls against a marked private filesystem.

Real Forms, MFL replies and filesystem results are the oracles. Transparent
module proxies retain their original packets. A private executable-path wrapper
records job-worker PIDs, then execs the immutable browser ELF without changing
stdin/stdout; only such a verified fixture-owned worker may be killed. Existing
destination conflicts provide deterministic decision/cancel/lifetime pauses.
No response, progress event or filesystem failure is manufactured.
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
import signal
import struct
import sys
import tempfile
import time
import traceback
from urllib.parse import unquote_to_bytes

from browser_continuity_live import cleanup_receipts
from desktop_live import connect_client
from inspector_live import NativeView, atomic_json
from live_x_harness import (HarnessRuntime, start_display_server, start_xorg_dummy,
    wait_for_authoritative_state, wait_for_predicate, wait_for_randr,
    run_required_xrandr)
from mfl_request_live import frame
from mfl_stream_live import Connection
from native_bindings_live import module_proxy, rows
from native_capture import NativeCapture
from session_x_errors import SessionXErrors

PROFILES = ('slate-dark', 'dune-light', 'classic-large')
JOB_TITLE = 'File jobs'
MARKER = b'FVWM-X synthetic file-job fixture\n'
TERMINAL = ('completed', 'skipped', 'failed', 'cancelled', 'partial', 'interrupted')
OPERATION_KEYS = dict(mkdir='F1', rename='F2', copy='F3', move='F4',
    link='F5', trash='F6', restore='F7', delete='F8')


class ClassHint(C.Structure):
    _fields_ = [('name', C.c_void_p), ('class_', C.c_void_p)]


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def safe_child(root, path):
    """Admit only fixture paths, preserving leaf symlinks as leaf objects."""
    root = root.resolve(strict=True)
    path = Path(path)
    parent = path.parent.resolve(strict=True)
    assert parent == root or root in parent.parents, (root, path)
    assert (root / '.file-jobs-fixture').read_bytes() == MARKER
    return path


def write_fixture(root, relative, data):
    path = safe_child(root, root / relative)
    assert not path.exists() and not path.is_symlink(), path
    path.write_bytes(data)
    return path


def run(args, profile, run_root):
    source, build = Path(__file__).resolve().parents[2], args.build.resolve()
    out = run_root / profile
    out.mkdir()
    binaries = [build / 'fvwm3', build / 'modules/FvwmForm/FvwmForm',
        build / 'bin/FvwmPrompt/fvwm-file-browser', build / 'bin/fvwm-desktop-capture']
    inputs = [Path(__file__).relative_to(source), Path('tests/live_x/native_bindings_live.py'),
        Path('tests/live_x/browser_continuity_live.py')]
    inputs += sorted(path.relative_to(source) for path in
        (source / 'bin/FvwmPrompt/internal/filejobs').glob('*.go'))
    inputs += sorted(path.relative_to(source) for path in
        (source / 'bin/FvwmPrompt/cmd/fvwmfilebrowser').glob('*.go')
        if not path.name.endswith('_test.go'))
    evidence = dict(status='running', profile=profile, screenshots=[], checks=[],
        jobs=[], rpc_receipts=[], failures=[], cleanup_failures=[], operations=[],
        source_sha256={str(name): digest(source / name) for name in inputs},
        binary_sha256={str(path.relative_to(build)): digest(path) for path in binaries},
        requested_scale=args.scale,
        limits=dict(top_level_fixture_files=24, runtime_records=128,
            physical_display_or_assistive_technology_qualified=False,
            automatic_module_scaling_qualified=bool(args.xorg_config),
            timing_gate=False, worker_pause='real existing-destination decision'))
    with HarnessRuntime(prefix='fvwm-jobs-') as runtime:
        client = errors = core = browser = control = rpc_peer = None
        fixture_root = runtime.root / 'workspace'
        worker_records = runtime.root / 'job-workers'
        browser_exits = {}
        try:
            for path in (fixture_root, worker_records):
                path.mkdir(mode=0o700)
            (fixture_root / '.file-jobs-fixture').write_bytes(MARKER)
            folder, destination, copies = [fixture_root / name for name in
                ('source', 'destination', 'copies')]
            for path in (folder, destination, copies):
                safe_child(fixture_root, path).mkdir(mode=0o700)
            original = b'literal file bytes\x00\xff\n'
            write_fixture(fixture_root, 'source/source.txt', original)
            write_fixture(fixture_root, 'source/move.txt', b'move contents\n')
            write_fixture(fixture_root, 'source/trash-me.txt', b'recover me\n')
            write_fixture(fixture_root, 'source/delete-me.txt', b'explicit delete\n')
            raw_name = b'raw-\xff-$[]-name'
            descriptor = os.open(os.fsencode(folder) + b'/' + raw_name,
                os.O_CREAT | os.O_EXCL | os.O_WRONLY, 0o600)
            with os.fdopen(descriptor, 'wb') as file:
                file.write(b'byte name\n')
            tree = folder / 'tree'
            safe_child(fixture_root, tree).mkdir(mode=0o700)
            safe_child(fixture_root, tree / 'nested').mkdir(mode=0o700)
            write_fixture(fixture_root, 'source/tree/leaf.txt', b'tree leaf\n')
            write_fixture(fixture_root, 'source/tree/nested/body.txt', b'tree body\n')
            os.symlink('../leaf.txt', safe_child(fixture_root, tree / 'nested/alias'))
            private_data = runtime.root / 'private-data'
            private_data.mkdir(mode=0o700)
            for name in ('private-config', 'private-cache', 'private-state'):
                (runtime.root / name).mkdir(mode=0o700)
            evidence['fixture'] = dict(root=str(fixture_root),
                marker_sha256=digest(fixture_root / '.file-jobs-fixture'),
                data_home=str(private_data), original_sha256=hashlib.sha256(original).hexdigest())

            if args.xorg_config:
                original_config = args.xorg_config.resolve().read_text()
                original_config = original_config.replace('    Modeline "1920x1080"',
                    '    Modeline "2560x2160" 225.00 2560 2720 2992 3424 2160 2163 2173 2190\n'
                    '    Modeline "1920x1080"')
                original_config = original_config.replace('Modes "1920x1080"',
                    'Modes "2560x2160" "1920x1080"')
                xorg = runtime.root / 'xorg.conf'
                xorg.write_text(original_config)
                _, display = start_xorg_dummy(runtime, xorg, time.monotonic() + 10)
                wait_for_randr(runtime, display, time.monotonic() + 10)
                mm = {'1': (677, 572), '1.25': (542, 458), '2': (339, 286)}[args.scale]
                run_required_xrandr(runtime, display, ['--output', 'DUMMY0',
                    '--mode', '2560x2160', '--pos', '0x0', '--primary',
                    '--set', 'WIDTH_MM', str(mm[0]), '--set', 'HEIGHT_MM', str(mm[1])],
                    'file-jobs-real-scale', evidence['operations'])
                evidence['randr'] = wait_for_authoritative_state(runtime, display,
                    'file jobs real monitor scale', lambda value:
                        value['monitors'].get('DUMMY0', {}).get('mm_width') == mm[0] and
                        value['monitors']['DUMMY0']['mm_height'] == mm[1],
                    time.monotonic() + 8, 'file-jobs-scale')
            else:
                _, display = start_display_server(runtime, 'Xvfb',
                    ['Xvfb', '-screen', '0', '2560x2160x24', '-noreset'],
                    'xserver.log', time.monotonic() + 10)
            modules, user, native = [runtime.root / name for name in
                ('modules', 'user', 'native-records')]
            for path in (modules, user, native):
                path.mkdir(mode=0o700)
            for path in (build / 'modules').iterdir():
                if (path / path.name).is_file() and path.name not in ('FvwmForm', 'FvwmMFL'):
                    (modules / path.name).symlink_to(path / path.name)
            (modules / 'FvwmDesktop').symlink_to(build / 'bin/FvwmPrompt/FvwmDesktop')
            for name in ('FvwmForm', 'FvwmMFL'):
                wrapper = modules / name
                wrapper.write_text('#!/bin/sh\nexec ' + shlex.join([sys.executable,
                    str(Path(__file__).resolve()), '--module-proxy',
                    str(build / 'modules' / name / name), str(native)]) +
                    ' "$@" 2>>' + shlex.quote(str(runtime.root / (name + '.log'))) + '\n')
                wrapper.chmod(0o700)
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
            evidence['config_sha256'] = digest(config)
            evidence['font'] = 'DejaVu Sans Mono 18' if profile == 'classic-large' else 'Sans 10'
            # Only the owned children receive these private locations. In
            # particular, no HOME or machine-wide XDG preference is reassigned.
            env = runtime.environment({'DISPLAY': display, 'FVWM_USERDIR': str(user),
                'FVWM_DESKTOP_ROOT': str(assets), 'FVWMMFL_SOCKET_PATH': str(runtime.root),
                'LANG': 'C.UTF-8', 'LC_ALL': 'C.UTF-8',
                'XDG_DATA_HOME': str(private_data),
                'XDG_CONFIG_HOME': str(runtime.root / 'private-config'),
                'XDG_CACHE_HOME': str(runtime.root / 'private-cache'),
                'XDG_STATE_HOME': str(runtime.root / 'private-state'),
                'XDG_DATA_DIRS': str(runtime.root / 'absent')})
            core_argv = [str(build / 'fvwm3'), '-d', display, '-f', str(config)]
            core = runtime.start('core', core_argv, 'core.log', env)
            endpoint = runtime.root / ('fvwm_mfl_' + display.replace(':', '_') + '.sock')

            def connect():
                def opened():
                    if not endpoint.exists():
                        return None
                    try:
                        peer = Connection(endpoint)
                        peer.record()
                        return peer
                    except (OSError, AssertionError):
                        return None
                return wait_for_predicate('file jobs MFL endpoint', opened,
                    time.monotonic() + 12, process=core)

            control, rpc_peer = connect(), connect()
            control.send(b'set echo\n')
            control.echo('file-jobs-ready')
            serial = 0

            def command(text):
                control.send((text + '\n').encode())
                control.echo('file-jobs-command-' + os.urandom(4).hex())

            def receipt(prefix):
                deadline = time.monotonic() + 5
                while time.monotonic() < deadline:
                    text = control.record().get('echo', {}).get('message', '')
                    if text.startswith(prefix):
                        return text
                raise AssertionError('missing actual native receipt ' + prefix)

            def rpc(scope='state', method='query', **fields):
                nonlocal serial
                serial += 1
                assert serial <= 2048
                token = 'jobs-%d' % serial
                wire = frame(token, method, resource='files', **fields,
                    **({'scope': scope} if 'provider' in fields and method == 'query' else {}))
                rpc_peer.send(wire)
                value = rpc_peer.record()['reply']
                evidence['rpc_receipts'].append(dict(request=json.loads(wire.removeprefix(b'request ')),
                    status=value.get('status'), error=value.get('error')))
                assert value['id'] == token, value
                if method == 'query' and value.get('status') == 'error' and \
                    value.get('error', {}).get('code') in ('not_ready', 'stale_generation'):
                    return None
                assert value['status'] == 'ok', value
                return value['result']

            def configured_theme():
                control.send(b'SendToModule FvwmDesktop query-settings jobs all\n')
                values = {}
                deadline = time.monotonic() + 5
                while time.monotonic() < deadline:
                    text = control.record().get('echo', {}).get('message', '')
                    if not text.startswith('desktop-settings-jobs '):
                        continue
                    if text.endswith(' done'):
                        break
                    _, name, value = text.split(' ', 2)
                    values[name] = json.loads(value)
                return values if all(values.get(name, {}).get('effective') == value for name, value in
                    [('palette', palette), ('appearance', appearance), ('transparency', False)]) else None
            evidence['theme_settings'] = wait_for_predicate('file jobs requested theme',
                configured_theme, time.monotonic() + 12, process=core)

            real_browser = build / 'bin/FvwmPrompt/fvwm-file-browser'
            private_browser = runtime.root / 'browser-image'
            shutil.copy2(real_browser, private_browser)
            browser_args = [str(private_browser), '--socket', str(endpoint),
                '--dir', str(folder), '--view', 'list', '--watch', 'off',
                '--opener', '/bin/true', '--terminal', '/bin/true']
            browser = runtime.start('browser', browser_args, 'browser.log', env)
            listed = wait_for_predicate('file jobs browser provider',
                lambda: rpc()['providers'], time.monotonic() + 10, process=browser)
            provider = listed[0]['provider']

            def state():
                return rpc(provider=provider)

            def ready_state():
                value = state()
                return value if value and not value['busy'] and value['availability']['ready'] and \
                    value['bindings']['projection']['state'] == 'current' else None

            wait_for_predicate('file jobs initial projection', ready_state,
                time.monotonic() + 12, process=browser)
            # The running browser keeps its original mapped ELF. Only future
            # self-exec lookups of this fixture-owned pathname see the wrapper.
            wrapper = ('#!' + sys.executable + '\nimport json,os,pathlib,sys,time\n' +
                'real=' + repr(str(real_browser)) + '\nroot=pathlib.Path(' + repr(str(worker_records)) + ')\n' +
                'if sys.argv[1:]==["--job-worker"]:\n' +
                ' token="job-%d-%d"%(os.getpid(),time.monotonic_ns())\n' +
                ' data={"pid":os.getpid(),"argv":[real,*sys.argv[1:]],"started":time.monotonic()}\n' +
                ' temporary=root/(token+".tmp")\n temporary.write_text(json.dumps(data)+"\\n")\n' +
                ' temporary.replace(root/(token+".json"))\n' +
                'os.execv(real,[real,*sys.argv[1:]])\n')
            replacement = runtime.root / 'worker-wrapper'
            replacement.write_text(wrapper)
            replacement.chmod(0o700)
            replacement.replace(private_browser)
            evidence['exec_wrapper_sha256'] = digest(private_browser)

            client = connect_client(display)
            errors = SessionXErrors(client, evidence).install()
            view = NativeView(client, errors, core)
            x, d = client.x, client.display
            x.XGetClassHint.argtypes, x.XGetClassHint.restype = [C.c_void_p, C.c_ulong,
                C.POINTER(ClassHint)], C.c_int
            x.XTranslateCoordinates.argtypes = [C.c_void_p, C.c_ulong, C.c_ulong,
                C.c_int, C.c_int, C.POINTER(C.c_int), C.POINTER(C.c_int),
                C.POINTER(C.c_ulong)]
            x.XSetInputFocus.argtypes = [C.c_void_p, C.c_ulong, C.c_int, C.c_ulong]
            x.XStoreBuffer.argtypes = [C.c_void_p, C.c_char_p, C.c_int, C.c_int]
            window = view.wait('File browser')

            def focus(target):
                x.XSetInputFocus(d, target, 1, 0)
                x.XSync(d, 0)
                wait_for_predicate('file jobs focus', lambda: view.focus() == target,
                    time.monotonic() + 3, process=browser)

            def key(symbol, alt=False):
                modifier = x.XKeysymToKeycode(d, x.XStringToKeysym(b'Alt_L'))
                if alt:
                    assert view.pointer.x.XTestFakeKeyEvent(d, modifier, 1, 0)
                view.key(symbol)
                if alt:
                    assert view.pointer.x.XTestFakeKeyEvent(d, modifier, 0, 0)
                x.XSync(d, 0)

            def click(target, px=4, button=1):
                attributes = view.attributes(target)
                assert attributes and attributes.map_state == 2
                rx, ry, child = C.c_int(), C.c_int(), C.c_ulong()
                assert x.XTranslateCoordinates(d, target, client.root, px,
                    attributes.height // 2, C.byref(rx), C.byref(ry), C.byref(child))
                view.pointer.motion(rx.value, ry.value)
                for pressed in (1, 0):
                    assert view.pointer.x.XTestFakeButtonEvent(d, button, pressed, 0)
                x.XSync(d, 0)

            def module_alias(target):
                hint = ClassHint()
                assert x.XGetClassHint(d, target, C.byref(hint))
                try:
                    return C.string_at(hint.name).decode()
                finally:
                    if hint.name: x.XFree(hint.name)
                    if hint.class_: x.XFree(hint.class_)

            def job_controls():
                target = view.named(JOB_TITLE)
                if target is None:
                    return None
                alias = module_alias(target)
                lines = [row['text'][len(alias)+1:].lstrip(': \t') for row in rows(native)
                    if row['direction'] == 'out' and row['text'].startswith('*' + alias + ': ')]
                schema = [line.split()[:2] for line in lines if line.split() and
                    line.split()[0] in ('Input', 'InputHex', 'Button', 'List')]
                children = view.children(target)
                if not schema or len(children) != len(schema):
                    return None
                inputs = {parts[1]: child for parts, child in zip(schema, children)
                    if parts[0] in ('Input', 'InputHex')}
                if set(inputs) != {'destination', 'name'}:
                    return None
                return dict(window=target, alias=alias, inputs=inputs, children=children)

            def scale_ready(page):
                def observed():
                    token = 'jobs' + os.urandom(8).hex()
                    control.send(('SendToModule ' + page['alias'] + ' UIScaleStatus ' + token + '\n').encode())
                    raw = receipt('FVWM_FORM_UI_SCALE ' + token + ' ')
                    fields = raw.split()
                    assert len(fields) == 15, raw
                    attributes = view.attributes(page['window'])
                    assert attributes and attributes.map_state == 2
                    value = dict(raw=raw, window=page['window'], alias=page['alias'],
                        state=fields[3], current_q=int(fields[4]), desired_q=int(fields[5]),
                        current_epoch=fields[6], desired_epoch=fields[7],
                        current_atom=int(fields[8]), desired_atom=int(fields[9]),
                        resource_generation=int(fields[10]),
                        minimum=[int(fields[11]), int(fields[12])],
                        required=[int(fields[13]), int(fields[14])],
                        client=[attributes.x, attributes.y, attributes.width, attributes.height])
                    samples = evidence.setdefault('scale_samples', [])
                    assert len(samples) < 256
                    samples.append(value)
                    if value['state'] != 'ready':
                        return None
                    assert value['current_q'] == value['desired_q']
                    if args.xorg_config:
                        assert value['current_q'] == int(float(args.scale) * 4), value
                    assert value['current_epoch'] == value['desired_epoch'] and int(value['current_epoch'], 16) > 0
                    assert value['current_atom'] == value['desired_atom'] != 0
                    assert all(actual >= minimum > 0 for actual, minimum in
                        zip(value['client'][2:], value['minimum'])), value
                    return value
                return wait_for_predicate('actual ready file jobs scale', observed,
                    time.monotonic() + 8, process=browser)

            def clear_field(page, name):
                click(page['inputs'][name])
                focus(page['window'])
                view.key('u', control=True)

            def paste_field(page, name, value):
                assert not page.get('paste_pending'), 'previous paste lacks actual callback readback'
                x.XStoreBuffer(d, value, len(value), 0)
                x.XSync(d, 0)
                click(page['inputs'][name], px=2, button=2)
                focus(page['window'])
                page['paste_pending'] = True

            def set_fields(page, destination, name):
                for value in (destination, name):
                    assert len(value) <= 512 and b'\0' not in value and b'\t' not in value
                # Server synchronization does not mean Form has read CUT_BUFFER0.
                # One paste uses Form's single-Tab traversal and stays immutable
                # until the actual operation callback proves the submitted bytes.
                clear_field(page, 'destination')
                clear_field(page, 'name')
                page['planned_fields'] = dict(destination=destination, name=name)
                paste_field(page, 'destination', destination + b'\t' + name)

            def set_field(page, name, value):
                assert name == 'name' and len(value) <= 512 and b'\0' not in value and b'\t' not in value
                assert not page.get('paste_pending'), 'cannot replace an unobserved paste'
                clear_field(page, name)
                page['planned_fields'][name] = value
                paste_field(page, name, value)

            def submit_job_control(page, action, symbol):
                submitted = time.monotonic()
                focus(page['window'])
                key(symbol)

                def observed():
                    for row in rows(native):
                        if row['direction'] != 'out' or row['time'] < submitted or \
                            not row['text'].startswith('Echo FVWM_FILES_JOBS_1 '):
                            continue
                        fields = row['text'].removeprefix('Echo ').split()
                        if len(fields) != 8 or fields[1] != provider or \
                            fields[2] != str(page['lifetime']) or fields[3] != action:
                            continue
                        expected = page['planned_fields']
                        actual = []
                        for field in fields[4:6]:
                            assert field.startswith('x'), fields
                            actual.append(base64.b64decode(field[1:] + '=' * (-len(field[1:]) % 4),
                                altchars=b'-_', validate=True))
                        wanted = [expected['destination'] if action in ('copy', 'move', 'link') else b'',
                            expected['name'] if action in ('mkdir', 'rename', 'copy', 'move', 'link', 'new-name') else b'']
                        assert actual == wanted, dict(callback=row['text'], expected=[v.hex() for v in wanted])
                        page['paste_pending'] = False
                        evidence.setdefault('submitted_fields', []).append(dict(action=action,
                            callback=row['text'], destination_hex=actual[0].hex(), name_hex=actual[1].hex()))
                        return row
                    return None

                return wait_for_predicate('actual file job callback fields for ' + action,
                    observed, time.monotonic() + 8, process=browser)

            def jobs():
                value = state()
                return value.get('jobs', {}) if value else {}

            def records():
                paths = sorted(worker_records.glob('job-*.json'))
                assert len(paths) <= 128
                return {path.name: json.loads(path.read_text()) for path in paths}

            def wait_job(previous=None, terminal=True, expected=None):
                def observed():
                    value = jobs()
                    samples = evidence.setdefault('job_samples', [])
                    assert len(samples) < 2048
                    samples.append(dict(time=time.monotonic(), **value))
                    if not value.get('id') or value['id'] == previous:
                        return None
                    if terminal:
                        if value.get('active') or value.get('state') not in TERMINAL:
                            return None
                    elif not value.get('active') or value.get('state') != 'decision' or not value.get('decision'):
                        return None
                    return value
                value = wait_for_predicate('file job terminal' if terminal else 'file job decision',
                    observed, time.monotonic() + 12, process=browser)
                if expected:
                    assert value['state'] in expected, value
                evidence['jobs'].append(value)
                return value

            def catalog(with_state=False):
                value = ready_state()
                if not value:
                    return None
                limit, total = value['page_max'], value['visible']
                maximum = evidence['limits']['top_level_fixture_files']
                assert isinstance(limit, int) and 1 <= limit <= maximum, value
                assert isinstance(total, int) and 0 <= total <= maximum, value
                entries, ids, offset = {}, set(), 0
                while True:
                    result = rpc('items', provider=provider, generation=value['generation'],
                        offset=offset, limit=limit)
                    if result is None:
                        return None
                    assert result['generation'] == value['generation'] and \
                        result['provider'] == provider and result['scope'] == 'items', result
                    assert result['offset'] == offset and result['total'] == total, result
                    end = min(offset + limit, total)
                    assert len(result['items']) == end - offset, result
                    for item in result['items']:
                        name = base64.b64decode(item['name_bytes'], validate=True)
                        assert name not in entries and item['id'] not in ids, item
                        entries[name] = item
                        ids.add(item['id'])
                    assert result['next_offset'] == (end if end < total else None), result
                    if end == total:
                        assert len(entries) == total
                        return (value, entries) if with_state else entries
                    assert offset < end <= maximum
                    offset = end

            def expect_catalog(present=(), absent=()):
                def observed():
                    entries = catalog()
                    return entries if entries is not None and all(name in entries for name in present) and \
                        all(name not in entries for name in absent) else None
                return wait_for_predicate('file jobs automatic current-folder refresh', observed,
                    time.monotonic() + 12, process=browser)

            def select(names=()):
                value, entries = wait_for_predicate('file jobs selectable current entries',
                    lambda: catalog(with_state=True),
                    time.monotonic() + 8, process=browser)
                chosen = [entries[name]['id'] for name in names]
                assert len(chosen) <= value['selection_edit_max']
                rpc(method='action', provider=provider, generation=value['generation'],
                    operation='selection', values=({'mode': 'replace', 'ids': chosen} if chosen else {'mode': 'clear'}))
                wait_for_predicate('file jobs exact selection',
                    lambda: state()['selected'] == len(chosen), time.monotonic() + 8, process=browser)

            def navigate(path):
                safe_child(fixture_root, path)
                current = wait_for_predicate('ready folder before navigation', ready_state,
                    time.monotonic() + 8, process=browser)
                assert 0 < len(os.fsencode(path)) <= current['path_request_max']
                rpc(method='action', provider=provider, generation=current['generation'], operation='navigate',
                    values={'mode': 'path', 'path_bytes': base64.b64encode(os.fsencode(path)).decode()})
                wait_for_predicate('file jobs navigation', lambda:
                    (value if base64.b64decode(value['path_bytes']) == os.fsencode(path) else None)
                    if (value := ready_state()) else None,
                    time.monotonic() + 12, process=browser)

            def open_jobs(names=(), dest=None, name=b''):
                if view.named(JOB_TITLE):
                    close_jobs()
                select(names)
                focus(view.named('File browser'))
                key('F9')
                page = wait_for_predicate('ready native file job controls', job_controls,
                    time.monotonic() + 8, process=browser)
                if 'first_jobs_scale' not in evidence:
                    evidence['first_jobs_scale'] = scale_ready(page)
                page['lifetime'] = jobs()['dialog_lifetime']
                destination = b''
                if dest is not None:
                    safe_child(fixture_root, dest)
                    destination = os.fsencode(dest)
                set_fields(page, destination, name)
                return page

            def close_jobs():
                page = job_controls()
                if page:
                    focus(page['window'])
                    key('Escape')
                    wait_for_predicate('closed native file jobs', lambda: not view.named(JOB_TITLE),
                        time.monotonic() + 8, process=browser)
                    wait_for_predicate('closed jobs released owned worker', lambda: not jobs().get('active'),
                        time.monotonic() + 8, process=browser)

            def start(page, operation, decision=False):
                before = jobs().get('id')
                prior = set(records())
                submit_job_control(page, operation, OPERATION_KEYS[operation])
                result = wait_job(before, terminal=not decision,
                    expected=None if decision else ('completed',))
                created = {name: item for name, item in records().items() if name not in prior}
                assert len(created) == 1, created
                result['worker'] = next(iter(created.values()))
                return result

            def capture(name):
                page = job_controls()
                assert page
                observed_scale = scale_ready(page)
                previous = os.environ.get('DISPLAY')
                os.environ['DISPLAY'] = display
                try:
                    image = Path(NativeCapture(build / 'bin/fvwm-desktop-capture').screenshot(out)
                        .removeprefix('Screenshot saved: '))
                    image.rename(out / name)
                finally:
                    if previous is None: os.environ.pop('DISPLAY', None)
                    else: os.environ['DISPLAY'] = previous
                evidence['screenshots'].append(name)
                manifest = evidence.setdefault('screenshot_manifest', [])
                manifest.append(dict(path=name, sha256=digest(out / name),
                    pixels=list(struct.unpack('>II', (out / name).read_bytes()[16:24])),
                    jobs=jobs(), native_window=view.named(JOB_TITLE), scale=observed_scale))

            # All eight operations use actual native controls, independently
            # observed directory data and complete byte content checks.
            page = open_jobs(name=b'new-folder')
            capture('jobs-editor.png')
            start(page, 'mkdir')
            assert safe_child(fixture_root, folder / 'new-folder').is_dir()
            expect_catalog(present=(b'new-folder',))
            page = open_jobs((b'source.txt',), name=b'renamed.txt')
            start(page, 'rename')
            assert not (folder / 'source.txt').exists()
            assert (folder / 'renamed.txt').read_bytes() == original
            expect_catalog(present=(b'renamed.txt',), absent=(b'source.txt',))
            page = open_jobs((b'renamed.txt',), dest=destination)
            copied = start(page, 'copy')
            assert copied['bytes'] >= len(original), copied
            assert (destination / 'renamed.txt').read_bytes() == original
            page = open_jobs((raw_name,), dest=destination)
            start(page, 'copy')
            with open(os.fsencode(destination) + b'/' + raw_name, 'rb') as file:
                assert file.read() == b'byte name\n'
            evidence['checks'].append('copy preserves invalid-UTF8 filename bytes and file contents')
            page = open_jobs((b'tree',), dest=destination)
            start(page, 'copy')
            assert (destination / 'tree/leaf.txt').read_bytes() == b'tree leaf\n'
            assert (destination / 'tree/nested/body.txt').read_bytes() == b'tree body\n'
            assert os.readlink(destination / 'tree/nested/alias') == '../leaf.txt'
            evidence['checks'].append('recursive copy retains nested contents and literal symlink')

            page = open_jobs((b'renamed.txt',), dest=destination)
            conflicted = start(page, 'copy', decision=True)
            capture('jobs-conflict.png')
            before_workers = records()
            focus(page['window'])
            key('F3')
            command('Nop file-jobs-zero-queue')
            assert jobs()['id'] == conflicted['id'] and records() == before_workers
            parent = view.named('File browser')
            before_view = ready_state()['view']
            focus(parent)
            key('F7')
            wait_for_predicate('unrelated browser input during pending file decision',
                lambda: (value if value['view'] != before_view else None)
                    if (value := ready_state()) else None,
                time.monotonic() + 8, process=browser)
            assert jobs()['id'] == conflicted['id'] and jobs()['decision'] == conflicted['decision']
            focus(page['window'])
            key('F11')
            wait_job(terminal=True, expected=('skipped', 'completed'))
            assert (destination / 'renamed.txt').read_bytes() == original
            evidence['checks'].append('real conflict keeps zero queued jobs and browser input responsive')

            page = open_jobs((b'renamed.txt',), dest=destination)
            start(page, 'copy', decision=True)
            set_field(page, 'name', b'copy-$[]-literal.txt')
            submit_job_control(page, 'new-name', 'F12')
            wait_job(terminal=True, expected=('completed',))
            assert (destination / 'copy-$[]-literal.txt').read_bytes() == original
            page = open_jobs((b'renamed.txt',), dest=destination)
            paused = start(page, 'copy', decision=True)
            focus(page['window'])
            key('x', alt=True)
            wait_job(terminal=True, expected=('cancelled',))
            assert (folder / 'renamed.txt').read_bytes() == original
            assert (destination / 'renamed.txt').read_bytes() == original
            evidence['checks'].append('conflict new-name is literal and cancellation preserves source/destination')

            page = open_jobs((b'move.txt',), dest=destination)
            start(page, 'move')
            assert not (folder / 'move.txt').exists()
            assert (destination / 'move.txt').read_bytes() == b'move contents\n'
            expect_catalog(absent=(b'move.txt',))
            page = open_jobs((b'renamed.txt',), dest=destination, name=b'literal-link')
            start(page, 'link')
            assert (destination / 'literal-link').is_symlink()
            assert os.readlink(os.fsencode(destination / 'literal-link')) == os.fsencode(folder / 'renamed.txt')
            close_jobs()
            navigate(destination)
            page = open_jobs((b'literal-link',), dest=copies)
            start(page, 'copy')
            assert (copies / 'literal-link').is_symlink()
            assert os.readlink(os.fsencode(copies / 'literal-link')) == os.fsencode(folder / 'renamed.txt')
            close_jobs()
            navigate(folder)

            page = open_jobs((b'trash-me.txt',))
            start(page, 'trash')
            assert not (folder / 'trash-me.txt').exists()
            expect_catalog(absent=(b'trash-me.txt',))
            trash_files = private_data / 'Trash/files'
            trash_info = private_data / 'Trash/info'
            trashed = list(trash_files.iterdir())
            assert len(trashed) == 1 and trashed[0].read_bytes() == b'recover me\n', trashed
            info = (trash_info / (trashed[0].name + '.trashinfo')).read_bytes()
            assert info.startswith(b'[Trash Info]\n')
            info_path = next(line[5:] for line in info.splitlines() if line.startswith(b'Path='))
            assert unquote_to_bytes(info_path.decode('ascii')) == os.fsencode(folder / 'trash-me.txt')
            # This private trash directory is intentionally outside workspace;
            # admission is exact, marker-guarded and never points at user data.
            assert private_data.resolve().parent == runtime.root.resolve()
            focus(page['window'])
            key('t', alt=True)
            wait_for_predicate('native Open trash retires job editor',
                lambda: not view.named(JOB_TITLE), time.monotonic() + 8, process=browser)
            wait_for_predicate('private trash folder ready', lambda:
                (value if base64.b64decode(value['path_bytes']) == os.fsencode(trash_files) else None)
                    if (value := ready_state()) else None,
                time.monotonic() + 12, process=browser)
            page = open_jobs((os.fsencode(trashed[0].name),))
            start(page, 'restore')
            assert (folder / 'trash-me.txt').read_bytes() == b'recover me\n'
            assert not trashed[0].exists() and not (trash_info / (trashed[0].name + '.trashinfo')).exists()
            close_jobs()
            navigate(folder)
            evidence['checks'].append('private desktop trash metadata and restore preserve exact source path/content')

            page = open_jobs((b'delete-me.txt', b'tree'))
            before = jobs().get('id')
            before_delete_generation = jobs()['dialog_generation']
            before_delete_workers = records()
            focus(page['window'])
            submit_job_control(page, 'delete', 'F8')
            command('Nop file-jobs-delete-confirmation')

            def delete_confirmation():
                value = jobs()
                generation = value.get('dialog_generation', 0)
                if generation <= before_delete_generation:
                    return None
                prefix = 'SendToModule %s ListEntry results %d 0 ' % (page['alias'], generation)
                actual = [row['text'] for row in rows(native) if row['direction'] == 'out' and
                    row['text'].startswith(prefix)]
                expected = {prefix + '"Delete target: %s" other off' % name
                    for name in ('delete-me.txt', 'tree')}
                if set(actual) != expected:
                    return None
                return dict(jobs=value, native_rows=actual)

            evidence['delete_confirmation'] = wait_for_predicate(
                'armed native delete lists both captured targets', delete_confirmation,
                time.monotonic() + 8, process=browser)
            assert (folder / 'delete-me.txt').read_bytes() == b'explicit delete\n'
            assert (folder / 'tree/nested/body.txt').read_bytes() == b'tree body\n'
            assert jobs().get('id') == before and records() == before_delete_workers
            capture('delete-confirmation.png')
            submit_job_control(page, 'confirm-delete', 'F10')
            deleted = wait_job(before, expected=('completed',))
            assert len(records()) == len(before_delete_workers) + 1
            assert not (folder / 'delete-me.txt').exists() and not (folder / 'tree').exists()
            assert (destination / 'tree/nested/body.txt').read_bytes() == b'tree body\n'
            expect_catalog(absent=(b'delete-me.txt', b'tree'))
            capture('jobs-completed.png')
            evidence['checks'].append('permanent delete requires distinct native confirmation')

            # Stale actual callback replay cannot admit another mutation.
            stale = next(row['text'] for row in rows(native) if row['direction'] == 'out' and
                row['text'].startswith('Echo FVWM_FILES_JOBS_1 ') and ' mkdir ' in row['text'])
            page = open_jobs((b'renamed.txt',), dest=destination)
            before, before_workers = jobs(), records()
            command(stale)
            assert jobs()['id'] == before['id'] and records() == before_workers
            evidence['checks'].append('retired native job callback cannot create a new job')

            # The immutable ELF and exact recorded argv identify this one child.
            # There is no name-based matching or signalling of an arbitrary PID.
            lost = start(page, 'copy', decision=True)
            worker = lost['worker']
            pid = worker['pid']
            assert pid > 1 and worker['argv'] == [str(real_browser), '--job-worker']
            process_root = Path('/proc') / str(pid)
            assert process_root.is_dir() and os.path.samefile(process_root / 'exe', real_browser)
            argv = (process_root / 'cmdline').read_bytes().rstrip(b'\0').split(b'\0')
            assert argv == [os.fsencode(real_browser), b'--job-worker'], argv
            os.kill(pid, signal.SIGKILL)
            interrupted = wait_job(terminal=True, expected=('interrupted',))
            wait_for_predicate('killed owned worker exited and reaped', lambda: not process_root.exists(),
                time.monotonic() + 5, process=browser)
            assert (folder / 'renamed.txt').read_bytes() == original
            count = records()
            command('Nop file-jobs-no-implicit-replay')
            assert records() == count and jobs()['id'] == interrupted['id']
            evidence['worker_loss'] = dict(worker=worker, observed_argv=[os.fsdecode(x) for x in argv],
                kernel_executable_sha256=digest(real_browser), terminal=interrupted, no_replay=True)

            page = open_jobs((b'renamed.txt',), dest=destination)
            closing = start(page, 'copy', decision=True)
            close_jobs()
            wait_for_predicate('closed dialog worker reaped',
                lambda: not (Path('/proc') / str(closing['worker']['pid'])).exists(),
                time.monotonic() + 5, process=browser)
            assert not jobs()['active'] and jobs()['id'] == closing['id']
            assert jobs()['state'] in ('cancelled', 'interrupted'), jobs()
            evidence['close_during_decision'] = dict(before=closing, after=jobs())

            page = open_jobs((b'renamed.txt',), dest=destination)
            restarting = start(page, 'copy', decision=True)
            # Restart uses the same actual executable/config and the ordinary
            # module-transport loss path; no result is synthesized or replayed.
            control.send(b'Restart\n')
            control.expect_closed()
            control.close()
            control = None
            rpc_peer.close()
            rpc_peer = None
            browser_exits['browser'] = browser.wait_until_exit(time.monotonic() + 10)
            assert browser_exits['browser'] in (0, 1), browser_exits
            wait_for_predicate('WM restart retires its owned job worker',
                lambda: not (Path('/proc') / str(restarting['worker']['pid'])).exists(),
                time.monotonic() + 5, process=core)
            control, rpc_peer = connect(), connect()
            control.send(b'set echo\n')
            control.echo('file-jobs-restarted')
            assert not rpc()['providers']
            assert (folder / 'renamed.txt').read_bytes() == original
            evidence['wm_restart'] = dict(job=restarting, browser_returncode=browser_exits['browser'],
                worker_gone=True, providers_empty=True, source_preserved=True)
            evidence['checks'].append('dialog close and actual WM restart cancel/reap instead of replaying jobs')
            evidence['observed_copy_progress'] = [sample for sample in evidence.get('job_samples', [])
                if sample.get('active') and sample.get('operation') in ('copy', 'move') and
                sample.get('bytes', 0) > 0 and sample.get('state') != 'decision']
            evidence['progress_observation_limit'] = ('Fast synthetic copies may finish between state samples; '
                'the real conflict pause independently qualifies native decision/cancel/input servicing.')
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
                if peer:
                    cleanup('close ' + name, peer.close)
            if errors:
                cleanup('close observer', errors.close_display)
            elif client:
                cleanup('close observer', client.close)
            for process in reversed(runtime.processes):
                cleanup('stop ' + process.name, lambda p=process: p.close(time.monotonic() + 5))
            evidence['children_after_cleanup'] = {p.name: p.poll() for p in runtime.processes}
            evidence['expected_browser_exits'] = browser_exits
            cleanup('actual module child Wait receipts',
                lambda: evidence.update(module_cleanup=cleanup_receipts(runtime.root / 'native-records')))
            for path in runtime.root.iterdir():
                if path.is_dir() and path.name in ('native-records', 'job-workers', 'user'):
                    cleanup('copy ' + path.name, lambda p=path: shutil.copytree(p, out / p.name))
                elif path.is_file() and (path.suffix == '.log' or path.name in ('config', 'xorg.conf')):
                    cleanup('copy ' + path.name, lambda p=path: shutil.copy2(p, out / p.name))
            # Temp filesystem contents are deliberately not copied into the
            # evidence archive. Only small identities, outcomes and captures stay.
            if evidence['cleanup_failures'] or evidence.get('observer_unexpected_x_errors'):
                evidence['status'] = 'failed'
            atomic_json(out / 'evidence.json', evidence)
    assert evidence['status'] == 'passed', evidence['failures']
    assert not evidence['cleanup_failures']
    assert all(value is not None for value in evidence['children_after_cleanup'].values())
    print('native file jobs ' + profile + ' passed: ' + str(out))


def main():
    if len(sys.argv) > 1 and sys.argv[1] == '--module-proxy':
        return module_proxy(sys.argv[2], sys.argv[3], sys.argv[4:])
    parser = argparse.ArgumentParser(description=__doc__, allow_abbrev=False)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--artifacts-dir', type=Path, required=True)
    parser.add_argument('--profiles', nargs='+', choices=PROFILES, default=list(PROFILES))
    parser.add_argument('--xorg-config', type=Path)
    parser.add_argument('--scale', choices=('1', '1.25', '2'), default='1')
    args = parser.parse_args()
    if args.scale != '1' and not args.xorg_config:
        parser.error('--scale requires --xorg-config for real RandR millimetres')
    args.artifacts_dir.mkdir(parents=True, exist_ok=True)
    root = Path(tempfile.mkdtemp(prefix='file-jobs-', dir=args.artifacts_dir.resolve()))
    for profile in args.profiles:
        run(args, profile, root)
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
