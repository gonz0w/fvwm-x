#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
"""Native inspector inventory, keyboard, privacy and closed-worker lifetimes.

The private module probe observes the core's actual Form configuration. The
worker wrapper records and forwards the actual worker protocol; its one-shot
file gate delays only an inspect request, with a one-second hard bound. Neither
helper invents inventory rows or runs inspected FVWM commands.
"""
import argparse
from collections import Counter
import ctypes as C
import hashlib
import json
import os
from pathlib import Path
import re
import select
import shlex
import shutil
import struct
import subprocess
import sys
import time

from colorset_live import WindowAttributes
from desktop_live import connect_client
from geometry_transition_live import Pointer
from live_x_harness import HarnessRuntime, start_display_server, wait_for_predicate
from mfl_request_live import frame, reply
from mfl_stream_live import Connection
from session_x_errors import SessionXErrors

WORD = struct.calcsize('@L')
TITLE = 'Configuration inspector'
SCOPES = ('settings', 'theme', 'bindings', 'layout', 'sources', 'commands')
PROFILES = ('slate-dark', 'dune-light', 'classic-large')
SECRET_ARGUMENT = 'INSPECTOR_PRIVATE_ACTION_ARGUMENT'
SECRET_WINDOW = 'INSPECTOR_PRIVATE_WINDOW_MATCH'


def atomic_json(path, value):
    temporary = path.with_suffix('.tmp')
    temporary.write_text(json.dumps(value, indent=2) + '\n')
    temporary.replace(path)


def module_probe(arguments):
    """Own only our module pipes and bounded, test-owned command receipts."""
    output, incoming = map(int, arguments[:2])
    root = Path(arguments[-1])
    root.mkdir(mode=0o700, exist_ok=True)

    def send(command):
        encoded = command.encode()
        assert len(encoded) <= 4096 and not any(c in command for c in '\0\n\r')
        wire = struct.pack('@LL', 0, len(encoded)) + encoded + struct.pack('@L', 1)
        while wire:
            wire = wire[os.write(output, wire):]

    send('SET_MASK ' + str((1 << 18) | (1 << 19) | (1 << 22)))
    send('SET_MASK ' + str((1 << 31) | (1 << 9)))  # MX_ECHO
    send('NOP FINISHED STARTUP')
    atomic_json(root / 'ready.json', {'pid': os.getpid()})
    buffer, pending, rows = bytearray(), None, []
    seen = set()
    while True:
        if pending is None:
            commands = sorted(root.glob('command-*.json'))
            assert len(commands) <= 1024
            for path in commands:
                if path.name in seen:
                    continue
                request = json.loads(path.read_text())
                token = request['token']
                assert re.fullmatch(r'p[0-9]{5}', token)
                pending = request
                seen.add(path.name)
                rows = []
                if request['mode'] == 'snapshot':
                    send('Send_ConfigInfo')
                elif request['mode'] == 'barrier':
                    send('SendToModule FvwmDesktop barrier ' + token)
                else:
                    assert request['mode'] == 'commands'
                    assert len(request['commands']) <= 64
                    for command in request['commands']:
                        send(command)
                    send('SendToModule InspectorProbe done ' + token)
                break
        if not select.select([incoming], [], [], .02)[0]:
            continue
        block = os.read(incoming, 65536)
        if not block:
            return
        buffer.extend(block)
        assert len(buffer) <= 1024 * 1024
        while len(buffer) >= 4 * WORD:
            start, kind, count, _ = struct.unpack('@LLLL', buffer[:4 * WORD])
            assert start == 0xffffffff and 4 <= count <= 4096
            size = count * WORD
            if len(buffer) < size:
                break
            body = bytes(buffer[4 * WORD:size])
            del buffer[:size]
            if pending is None:
                continue
            text = body[3 * WORD:].split(b'\0', 1)[0].decode('utf-8', 'strict')
            if pending['mode'] == 'snapshot':
                if kind == 1 << 18:
                    if text.startswith(('*SlateInspectorForm', '*FvwmFormDefault')):
                        rows.append(text)
                        assert len(rows) <= 512
                if kind == 1 << 19:
                    atomic_json(root / (pending['token'] + '.json'), {'lines': rows})
                    pending = None
            elif (kind == 1 << 22 and text == 'done ' + pending['token'] or
                    kind == (1 << 31) | (1 << 9) and text == pending['token']):
                atomic_json(root / (pending['token'] + '.json'), {'done': True})
                pending = None


def worker_probe(worker, directory):
    """Forward private worker bytes unchanged; gate/fail only test-owned jobs."""
    root = Path(directory)
    data = sys.stdin.buffer.read(65537)
    if len(data) > 65536:
        return 2
    request = json.loads(data)
    inspect = request.get('operation') == 'inspect'
    token = 'w' + str(time.monotonic_ns()) + '-' + str(os.getpid())
    started = time.monotonic()
    held = False
    if inspect and (root / 'hold-next').exists():
        try:
            (root / 'hold-next').rename(root / ('held-' + token))
            held = True
        except FileNotFoundError:
            pass
        if held:
            atomic_json(root / 'held.json', {'token': token, 'started': started})
            deadline = time.monotonic() + 1
            while not (root / ('release-' + token)).exists():
                if time.monotonic() >= deadline:
                    atomic_json(root / (token + '.json'), {'request': request,
                        'gate_timeout': True, 'started': started})
                    return 3
                # This is a bounded file condition, not an assumed worker delay.
                select.select([], [], [], max(0, min(.005, deadline - time.monotonic())))
    if inspect and (root / 'fail-next').exists():
        try:
            (root / 'fail-next').unlink()
        except FileNotFoundError:
            pass
        atomic_json(root / (token + '.json'), {'request': request,
            'injected_failure': True, 'started': started})
        return 4
    completed = subprocess.run([str(worker)], input=data, capture_output=True,
        timeout=4.8, check=False)
    if inspect:
        observed = {'request': request, 'returncode': completed.returncode,
            'started': started, 'finished': time.monotonic(), 'held': held}
        try:
            observed['response'] = json.loads(completed.stdout)
        except (ValueError, UnicodeError):
            observed['malformed_response'] = True
        atomic_json(root / (token + '.json'), observed)
    sys.stdout.buffer.write(completed.stdout)
    sys.stderr.buffer.write(completed.stderr)
    return completed.returncode


class Probe:
    def __init__(self, directory, core):
        self.root, self.core, self.serial = directory, core, 0

    def request(self, mode, commands=()):
        self.serial += 1
        token = 'p%05d' % self.serial
        atomic_json(self.root / ('command-' + token + '.json'),
            {'token': token, 'mode': mode, 'commands': list(commands)})
        path = self.root / (token + '.json')
        wait_for_predicate('inspector module probe ' + token, path.exists,
            time.monotonic() + 4, process=self.core)
        return json.loads(path.read_text())

    def command(self, *commands):
        return self.request('commands', commands)

    def barrier(self):
        return self.request('barrier')

    def snapshot(self):
        return self.request('snapshot')['lines']


def inspector_frame(lines):
    found = []
    for line in lines:
        match = re.search(r'inspect-control ([a-zA-Z0-9_-]+) refresh$', line)
        if match:
            found.append(match.group(1))
    assert not found or len(set(found)) == 1, found
    return found[0] if found else None


def form_text(lines):
    """Decode the Form's literal transport for assertions; retain raw evidence."""
    decoded = []
    for line in lines:
        match = re.fullmatch(
            r'(?:\*SlateInspectorForm:?[ \t]*)?TextHex x([0-9a-f]*)', line)
        if match:
            decoded.append(bytes.fromhex(match.group(1)).decode('utf-8', 'strict'))
        else:
            decoded.append(line)
    return '\n'.join(decoded)


class NativeView:
    def __init__(self, client, errors, core):
        self.client, self.errors, self.core = client, errors, core
        self.x, self.d = client.x, client.display
        self.pointer = Pointer(client)
        signatures = {
            'XFetchName': ([C.c_void_p, C.c_ulong, C.POINTER(C.c_void_p)], C.c_int),
            'XGetWindowAttributes': ([C.c_void_p, C.c_ulong,
                C.POINTER(WindowAttributes)], C.c_int),
            'XGetInputFocus': ([C.c_void_p, C.POINTER(C.c_ulong),
                C.POINTER(C.c_int)], C.c_int),
            'XStringToKeysym': ([C.c_char_p], C.c_ulong),
            'XKeysymToKeycode': ([C.c_void_p, C.c_ulong], C.c_ubyte),
            'XGetImage': ([C.c_void_p, C.c_ulong, C.c_int, C.c_int,
                C.c_uint, C.c_uint, C.c_ulong, C.c_int], C.c_void_p),
            'XGetPixel': ([C.c_void_p, C.c_int, C.c_int], C.c_ulong),
            'XDestroyImage': ([C.c_void_p], C.c_int),
        }
        for name, (arguments, result) in signatures.items():
            function = getattr(self.x, name)
            function.argtypes, function.restype = arguments, result
        self.pointer.x.XTestFakeKeyEvent.argtypes = [C.c_void_p, C.c_uint,
            C.c_int, C.c_ulong]
        self.pointer.x.XTestFakeKeyEvent.restype = C.c_int

    def attributes(self, window):
        attributes = WindowAttributes()
        if self.errors.read(window, 3, lambda: self.x.XGetWindowAttributes(
                self.d, window, C.byref(attributes))):
            return attributes
        return None

    def children(self, window):
        root, parent, count = C.c_ulong(), C.c_ulong(), C.c_uint()
        items = C.POINTER(C.c_ulong)()
        try:
            if self.errors.read(window, 15, lambda: self.x.XQueryTree(self.d,
                    window, C.byref(root), C.byref(parent), C.byref(items), C.byref(count))):
                assert count.value <= 2048
                return list(items[:count.value])
            return []
        finally:
            if items:
                self.x.XFree(items)

    def named(self, title):
        queue, visited = [(self.client.root, 0)], 0
        while queue:
            window, depth = queue.pop()
            visited += 1
            assert visited <= 4096
            value = C.c_void_p()
            try:
                ok = self.errors.read(window, 20, lambda: self.x.XFetchName(
                    self.d, window, C.byref(value)))
                if ok and value.value and C.string_at(value).decode('utf-8', 'replace') == title:
                    attributes = self.attributes(window)
                    if attributes and attributes.map_state == 2:
                        return window
            finally:
                if value.value:
                    self.x.XFree(value)
            if depth < 5:
                queue.extend((child, depth + 1) for child in self.children(window))
        return None

    def wait(self, title=TITLE):
        return wait_for_predicate('native ' + title, lambda: self.named(title),
            time.monotonic() + 10, process=self.core)

    def focus(self):
        window, revert = C.c_ulong(), C.c_int()
        self.x.XGetInputFocus(self.d, C.byref(window), C.byref(revert))
        self.x.XSync(self.d, 0)
        return window.value

    def key(self, symbol, control=False):
        code = self.x.XKeysymToKeycode(self.d,
            self.x.XStringToKeysym(symbol.encode()))
        assert code, symbol
        modifier = self.x.XKeysymToKeycode(self.d,
            self.x.XStringToKeysym(b'Control_L'))
        if control:
            assert self.pointer.x.XTestFakeKeyEvent(self.d, modifier, 1, 0)
        for pressed in (1, 0):
            assert self.pointer.x.XTestFakeKeyEvent(self.d, code, pressed, 0)
        if control:
            assert self.pointer.x.XTestFakeKeyEvent(self.d, modifier, 0, 0)
        self.x.XSync(self.d, 0)

    def pixels(self, window):
        attributes = self.attributes(window)
        assert attributes and attributes.map_state == 2
        assert 0 < attributes.width <= 1440 and 0 < attributes.height <= 1200
        image = self.x.XGetImage(self.d, window, 0, 0, attributes.width,
            attributes.height, C.c_ulong(-1).value, 2)
        assert image
        colors = Counter()
        try:
            for y in range(2, attributes.height - 2, 3):
                for x in range(2, attributes.width - 2, 3):
                    colors[self.x.XGetPixel(image, x, y)] += 1
        finally:
            self.x.XDestroyImage(image)
        return {'window': window, 'width': attributes.width,
            'height': attributes.height, 'samples': sum(colors.values()),
            'colors': {str(color): count for color, count in colors.items()}}


def run(args, profile):
    source, build = Path(__file__).resolve().parents[2], args.build.resolve()
    out = args.artifacts_dir.resolve() / profile
    out.mkdir(parents=True, exist_ok=False)
    evidence = {'status': 'running', 'profile': profile, 'sections': {},
        'screenshots': [], 'failures': []}
    with HarnessRuntime(prefix='fvwm-inspector-') as runtime:
        connections, client, errors = [], None, None
        try:
            _, display = start_display_server(runtime, 'Xvfb',
                ['Xvfb', '-screen', '0', '1440x1200x24', '-noreset'],
                'xserver.log', time.monotonic() + 10)
            user, modules = runtime.root / 'user', runtime.root / 'modules'
            records, probe_directory = runtime.root / 'worker-records', runtime.root / 'probe'
            for path in (user, modules, records, probe_directory):
                path.mkdir(mode=0o700)
            for directory in (build / 'modules').iterdir():
                executable = directory / directory.name
                if executable.is_file():
                    (modules / directory.name).symlink_to(executable)
            (modules / 'FvwmDesktop').symlink_to(build / 'bin/FvwmPrompt/FvwmDesktop')
            helper = build / 'bin/FvwmPrompt/fvwm-desktop-worker'
            assert helper.is_file(), helper
            wrapper = runtime.root / 'worker-probe'
            wrapper.write_text('#!/bin/sh\nexec ' + shlex.join([sys.executable,
                str(Path(__file__).resolve()), '--worker', str(helper), str(records)]) + '\n')
            wrapper.chmod(0o700)
            module = modules / 'InspectorProbe'
            module.write_text('#!/bin/sh\nexec ' + shlex.join([sys.executable,
                str(Path(__file__).resolve()), '--probe']) + ' "$@" 2>' +
                shlex.quote(str(runtime.root / 'probe.log')) + '\n')
            module.chmod(0o700)
            palette, appearance = ('dune', 'light') if profile == 'dune-light' else ('slate', 'dark')
            included = user / 'inspector-included.config'
            included.write_text('*FvwmDesktop: palette ' + palette + '\n'
                '*FvwmDesktop: motion Off\n')
            local = user / 'local.config'
            local.write_text('*FvwmDesktop: palette ink\nRead "' + str(included) +
                '"\n*FvwmDesktop: appearance ' + appearance + '\n'
                '*FvwmDesktop: transparency Off\n')
            root = source / 'default-config'
            body = (root / 'config').read_text().replace(
                '$[FVWM_DATADIR]/default-config', str(root))
            if profile == 'classic-large':
                body = body.replace('Sans:size=10:antialias=True',
                    'DejaVu Sans Mono:size=18:antialias=True')
            marker = runtime.root / 'must-not-execute'
            bindings = []
            for key in range(13, 36):
                for modifier in ('N', 'S', 'C', 'M', 'SC', 'SM', 'CM', 'SCM'):
                    bindings.append('Key F%d A %s Nop %s' % (key, modifier, SECRET_ARGUMENT))
            bindings.append('Key (' + SECRET_WINDOW + '*) F12 A N Exec touch ' + str(marker))
            legacy = ('*FvwmFormDefault: Fore wheat\n*FvwmFormDefault: Back navy\n'
                '*FvwmFormDefault: ItemFore black\n*FvwmFormDefault: ItemBack gray\n'
                if profile == 'classic-large' else '')
            config = runtime.root / 'config'
            config.write_text('ModulePath ' + str(modules) + '\nRead ' +
                str(source / 'fvwm/ConfigFvwmDefaults') + '\n' + body + '\n' + legacy +
                '\n'.join(bindings) + '\nAddToFunc StartFunction I Module InspectorProbe ' +
                str(probe_directory) + '\n')
            env = runtime.environment({'DISPLAY': display, 'HOME': str(runtime.root),
                'FVWM_USERDIR': str(user), 'FVWM_DESKTOP_ROOT': str(root),
                'FVWM_DESKTOP_WORKER': str(wrapper), 'FVWMMFL_SOCKET_PATH': str(runtime.root),
                'XDG_DATA_HOME': str(runtime.root / 'data'),
                'XDG_DATA_DIRS': str(runtime.root / 'absent')})
            core = runtime.start('core', [str(build / 'fvwm3'), '-d', display,
                '-f', str(config)], 'core.log', env)
            wait_for_predicate('inspector observer module',
                (probe_directory / 'ready.json').exists,
                time.monotonic() + 10, process=core)
            probe = Probe(probe_directory, core)
            probe.barrier()
            client = connect_client(display)
            errors = SessionXErrors(client, evidence).install()
            view = NativeView(client, errors, core)
            assert view.named(TITLE) is None, 'inspector must be opt-in'
            client.create()
            client.x.XStoreName(client.display, client.window, b'Inspector focus return probe')
            client.x.XSync(client.display, 0)
            view.wait('Inspector focus return probe')
            probe.command('WindowId ' + str(client.window) + ' Focus')
            wait_for_predicate('managed focus baseline', lambda: view.focus() == client.window,
                time.monotonic() + 5, process=core)
            endpoint = runtime.root / ('fvwm_mfl_' + display.replace(':', '_') + '.sock')
            assert not endpoint.exists(), 'test configuration must begin without MFL'
            seen_records = set()

            def record(scope=None, filter_=None, failed=False):
                def candidate():
                    for path in sorted(records.glob('w*.json')):
                        if path.name in seen_records:
                            continue
                        value = json.loads(path.read_text())
                        request = value['request']['inspect']
                        if scope is not None and request['scope'] != scope:
                            continue
                        if filter_ is not None and request['filter'] != filter_:
                            continue
                        seen_records.add(path.name)
                        assert not value.get('gate_timeout'), value
                        if not failed:
                            assert value.get('returncode') == 0, value
                            assert value['response']['version'] == 1, value
                        return value
                    return None
                return wait_for_predicate('actual inspector worker ' + str(scope), candidate,
                    time.monotonic() + 10, process=core)

            def native_page(scope, changed=None):
                result = {}
                def matches():
                    lines = probe.snapshot()
                    token = inspector_frame(lines)
                    if not token or token == changed:
                        return None
                    text = form_text(lines)
                    if ('Snapshot scope: ' + scope not in text or
                            'Separate point-in-time observations' not in text):
                        return None
                    window = view.named(TITLE)
                    if window is None:
                        return None
                    result.update(window=window, token=token, lines=lines)
                    return result
                return wait_for_predicate('published native inspector ' + scope, matches,
                    time.monotonic() + 10, process=core)

            def open_scope(scope):
                probe.command('SendToModule FvwmDesktop inspect ' + scope)
                work = record(scope, '')
                page = native_page(scope)
                return work['response']['result'], page

            def focus_form(page):
                probe.command('WindowId ' + str(page['window']) + ' Focus')
                wait_for_predicate('inspector keyboard focus',
                    lambda: view.focus() == page['window'], time.monotonic() + 5, process=core)

            def close_escape(page):
                focus_form(page)
                view.key('Escape')
                wait_for_predicate('Escape closes inspector', lambda: view.named(TITLE) is None,
                    time.monotonic() + 5, process=core)
                wait_for_predicate('focus returns to prior managed client',
                    lambda: view.focus() == client.window, time.monotonic() + 5, process=core)

            absent, absent_page = open_scope('all')
            by_id = {section['id']: section for section in absent['sections']}
            assert set(by_id) == set(SCOPES), absent
            assert by_id['settings']['state'] == 'provider-policy', by_id
            assert by_id['sources']['state'] == 'literal-observations', by_id
            assert by_id['bindings']['state'] == 'unavailable', by_id
            assert by_id['layout']['state'] == 'unavailable', by_id
            assert not endpoint.exists(), 'inspection started MFL implicitly'
            close_escape(absent_page)
            evidence['mfl_initially_absent'] = absent

            def connect():
                connection = Connection(endpoint)
                connection.socket.settimeout(8)
                assert 'connection_profile' in connection.record()
                connections.append(connection)
                connection.send(frame('readonly', 'set_mode', mode='read-only'))
                assert reply(connection, 'readonly')['read_only']
                return connection

            probe.command('Module FvwmMFL')
            wait_for_predicate('explicit MFL start', endpoint.exists,
                time.monotonic() + 10, process=core)
            rpc, serial = connect(), 0

            def query(resource='settings', **fields):
                nonlocal serial
                serial += 1
                token = 'inspector-' + str(serial)
                rpc.send(frame(token, resource=resource, **fields))
                return reply(rpc, token)

            def settings_ready():
                nonlocal serial
                serial += 1
                token = 'inspector-ready-' + str(serial)
                rpc.send(frame(token, resource='settings', scope='all'))
                response = rpc.record()['reply']
                if response['status'] != 'ok':
                    return None
                value = response['result']
                return value if value['settings']['palette']['application'] == 'submitted-to-fvwm' else None

            settings_before = wait_for_predicate('desktop provider settings', settings_ready,
                time.monotonic() + 12, process=core)
            assert settings_before['settings']['palette']['effective'] == palette
            assert settings_before['settings']['palette']['source'] == 'module-configuration'

            def binding_snapshot():
                snapshots = {}
                for scope in ('global', 'menu', 'menu-fallback'):
                    page = query('bindings', scope=scope, limit=16)
                    generation = page['generation']
                    entries = page['bindings'][:]
                    while page['next_offset'] is not None:
                        assert len(entries) < 4096
                        page = query('bindings', scope=scope, offset=page['next_offset'],
                            generation=generation, limit=16)
                        assert page['generation'] == generation
                        entries.extend(page['bindings'])
                    snapshots[scope] = {'generation': generation, 'entries': entries}
                return snapshots

            bindings_before = binding_snapshot()
            state = user / 'desktop-state.json'
            saved_before = state.read_bytes() if state.exists() else None
            local_before, included_before = local.read_bytes(), included.read_bytes()
            all_result, all_page = open_scope('all')
            sections = {section['id']: section for section in all_result['sections']}
            assert set(sections) == set(SCOPES)
            assert sections['bindings']['truncated'], sections['bindings']
            assert sections['commands']['truncated'], sections['commands']
            assert any(row['label'] == 'palette metadata' and 'type=enum' in row['value']
                for row in sections['settings']['rows'])
            source_rows = sections['sources']['rows']
            assert any('inspector-included.config:' in row['value'] and 'includes=1' in row['value']
                for row in source_rows), source_rows
            assert any('palette' in row['label'] and 'last in checked literals' in row['value']
                for row in source_rows), source_rows
            assert any('actual pixels' in row['value'] for row in sections['layout']['rows'])
            serialized = json.dumps(all_result)
            for hidden in (SECRET_ARGUMENT, SECRET_WINDOW, str(runtime.root), str(marker)):
                assert hidden not in serialized, (hidden, all_result)
            topology = query('topology')
            assert topology['units'] == 'actual-pixels'
            assert client.geometry(client.root)[2:] == (1440, 1200)
            for monitor in topology['topology']['monitors']:
                geometry = monitor['geometry']
                expected = ('x={x} y={y} width={width} height={height} actual pixels'
                    .format(**geometry))
                assert any(row['label'] == 'Monitor ' + monitor['name'] and
                    row['value'].startswith(expected)
                    for row in sections['layout']['rows']), sections['layout']
            evidence['core_layout'] = {'topology': topology,
                'workspaces': query('workspaces'), 'desktop': query('desktop')}
            evidence['all_sections'] = all_result
            close_escape(all_page)

            def capture(name):
                captured = subprocess.run([str(build / 'bin/fvwm-desktop-capture'), str(out)],
                    env=env, capture_output=True, text=True, timeout=35, check=False)
                assert captured.returncode == 0, captured.stderr
                filename = Path(captured.stdout.strip().removeprefix('Screenshot saved: '))
                filename.replace(out / name)
                evidence['screenshots'].append(name)

            for scope in SCOPES:
                result, page = open_scope(scope)
                assert result['scope'] == scope and len(result['sections']) == 1
                assert result['sections'][0]['id'] == scope
                assert any('Colorset 2' in line for line in page['lines'])
                assert any('ItemColorset 12' in line for line in page['lines'])
                font = 'size=18' if profile == 'classic-large' else 'size=10'
                assert any('FvwmFormDefault' in line and 'Font' in line and font in line
                    for line in page['lines']), page
                observation = view.pixels(page['window'])
                colorsets = query('colorsets', limit=16)['colorsets']
                expected = next(item for item in colorsets if item['id'] == 2)
                background = int(expected['pixels']['background'], 16)
                foreground = int(expected['pixels']['foreground'], 16)
                assert observation['colors'].get(str(background), 0) > observation['samples'] // 4
                assert observation['colors'].get(str(foreground), 0) > 0
                for hidden in (SECRET_ARGUMENT, SECRET_WINDOW, str(runtime.root), str(marker)):
                    assert hidden not in form_text(page['lines']), (scope, hidden)
                evidence['sections'][scope] = {'result': result,
                    'form_configuration': page['lines'], 'native_pixels': observation}
                if scope in ('settings', 'theme', 'layout'):
                    capture('inspector-' + scope + '.png')
                if scope == 'settings':
                    focus_form(page)
                    view.key('F2')
                    next_page = native_page(scope, page['token'])
                    assert 'page 2/' in form_text(next_page['lines']), next_page
                    wait_for_predicate('replacement inspector receives keyboard focus',
                        lambda: view.focus() == next_page['window'],
                        time.monotonic() + 5, process=core)
                    view.key('F1')
                    first = native_page(scope, next_page['token'])
                    assert 'page 1/' in form_text(first['lines'])
                    probe.command('SendToModule FvwmDesktop inspect-control ' + page['token'] + ' page 1')
                    probe.barrier()
                    assert view.named(TITLE), 'retired page callback closed current view'
                    page = first
                close_escape(page)

            # Search the full generated inventory, including commands beyond its
            # first 128 rows, using actual native input and Return transport.
            catalog = json.loads((source / 'bin/FvwmPrompt/internal/commandmeta/commands.json').read_text())
            chosen = sorted(item['name'] for item in catalog if item['name'].isalpha())[-1]
            _, commands_page = open_scope('commands')
            focus_form(commands_page)
            view.key('u', control=True)
            for letter in chosen.lower():
                view.key(letter)
            view.key('Return')
            filtered = record('commands', chosen.lower())['response']['result']
            filtered_page = native_page('commands', commands_page['token'])
            assert any(row['label'] == chosen for row in filtered['sections'][0]['rows']), filtered
            assert not filtered['sections'][0]['truncated'], filtered
            evidence['native_filter'] = filtered
            close_escape(filtered_page)

            # Change only the literal file after startup. Executing it now would
            # create the marker; observation must retain candidate provenance.
            local.write_bytes(local_before + ('Exec touch ' + str(marker) + '\n' +
                ''.join('*FvwmDesktop: launcher_opacity %d\n' % (20 + i % 70)
                    for i in range(150))).encode())
            large, large_page = open_scope('sources')
            large_section = large['sections'][0]
            assert large_section['truncated'] and len(large_section['rows']) <= 128
            assert not any('last in checked literals' in row['value']
                for row in large_section['rows']), large_section
            assert 'runtime origin' in large_section['reason'], large_section
            assert not marker.exists(), 'literal inspector executed configuration'
            evidence['large_literal_sources'] = large
            close_escape(large_page)
            local.write_bytes(local_before)

            _, page = open_scope('settings')
            (records / 'fail-next').touch()
            focus_form(page)
            view.key('F5')
            record('settings', '', failed=True)
            def failed_refresh():
                lines = probe.snapshot()
                return lines if 'Refresh failed' in form_text(lines) else None
            failure_lines = wait_for_predicate('native stale refresh failure', failed_refresh,
                time.monotonic() + 5, process=core)
            assert 'Snapshot scope: settings' in form_text(failure_lines)
            evidence['failed_refresh_retains_snapshot'] = failure_lines
            page = {'window': view.wait(), 'token': inspector_frame(failure_lines)}

            def gated_close_refresh():
                (records / 'held.json').unlink(missing_ok=True)
                (records / 'hold-next').touch()
                focus_form(page)
                view.key('F5')
                held_path = records / 'held.json'
                wait_for_predicate('worker reaches owned inspect gate', held_path.exists,
                    time.monotonic() + 3, process=core)
                held = json.loads(held_path.read_text())
                current = view.wait()
                # Core Close sends the ordinary WM_DELETE protocol. The Form's
                # stable view token must retire its owner before worker release.
                probe.command('WindowId ' + str(current) + ' Close')
                wait_for_predicate('WM_DELETE removes inspector before worker release',
                    lambda: view.named(TITLE) is None, time.monotonic() + .6, process=core)
                (records / ('release-' + held['token'])).touch()
                assert time.monotonic() - held['started'] < 1, 'test held inspect gate too long'
                completed = record('settings', '')
                probe.barrier()
                quiet_until = time.monotonic() + .5
                def remains_closed():
                    assert view.named(TITLE) is None, 'late worker resurrected closed view'
                    return time.monotonic() >= quiet_until
                # Observe absence after real worker completion, without assuming
                # how long the worker takes or sleeping before sending Close.
                wait_for_predicate('closed view stays retired after completion', remains_closed,
                    quiet_until + 1, process=core)
                assert view.focus() == client.window
                return completed

            evidence['wm_delete_during_refresh'] = gated_close_refresh()
            _, page = open_scope('settings')
            probe.command('KillModule FvwmMFL')
            wait_for_predicate('MFL loss endpoint', lambda: not endpoint.exists(),
                time.monotonic() + 5, process=core)
            for connection in connections:
                connection.close()
            connections.clear()
            lost, lost_page = open_scope('all')
            lost_sections = {section['id']: section for section in lost['sections']}
            assert lost_sections['settings']['state'] == 'provider-policy'
            assert lost_sections['bindings']['state'] == 'unavailable'
            assert lost_sections['layout']['state'] == 'unavailable'
            close_escape(lost_page)
            evidence['mfl_lost'] = lost
            probe.command('Module FvwmMFL')
            wait_for_predicate('MFL replacement endpoint', endpoint.exists,
                time.monotonic() + 10, process=core)
            rpc = connect()
            wait_for_predicate('same desktop provider after MFL restart', settings_ready,
                time.monotonic() + 12, process=core)
            recovered, recovered_page = open_scope('layout')
            assert recovered['sections'][0]['state'] == 'observed', recovered
            close_escape(recovered_page)
            evidence['mfl_restarted'] = recovered
            assert query(scope='all') == settings_before, 'inspection changed owner settings'
            assert binding_snapshot() == bindings_before, 'inspection changed core bindings'
            assert (state.read_bytes() if state.exists() else None) == saved_before
            assert local.read_bytes() == local_before and included.read_bytes() == included_before
            assert not marker.exists()
            evidence['unchanged'] = {'settings': settings_before,
                'bindings_sha256': hashlib.sha256(json.dumps(bindings_before,
                    sort_keys=True).encode()).hexdigest(), 'saved_preferences': True,
                'literal_files': True, 'sentinel_absent': True}
            evidence['status'] = 'passed'
        except BaseException as failure:
            evidence['status'] = 'failed'
            evidence['failures'].append({'type': type(failure).__name__, 'message': str(failure)})
            raise
        finally:
            for connection in connections:
                connection.close()
            if errors:
                errors.close_display()
            elif client:
                client.close()
            for process in reversed(runtime.processes):
                process.close(time.monotonic() + 3)
            evidence['children_after_cleanup'] = {process.name: process.poll()
                for process in runtime.processes}
            for directory in (runtime.root / 'probe', runtime.root / 'worker-records',
                    runtime.root / 'user'):
                if directory.exists():
                    shutil.copytree(directory, out / directory.name)
            for path in runtime.root.glob('*'):
                if path.is_file() and (path.suffix == '.log' or
                        path.name in ('config', 'worker-probe')):
                    shutil.copy2(path, out / path.name)
            (out / 'evidence.json').write_text(json.dumps(evidence, indent=2) + '\n')
    assert not evidence['observer_unexpected_x_errors'], evidence
    print('native inspector ' + profile + ' passed')


def main():
    if len(sys.argv) > 1 and sys.argv[1] == '--probe':
        module_probe(sys.argv[2:])
        return 0
    if len(sys.argv) == 4 and sys.argv[1] == '--worker':
        return worker_probe(Path(sys.argv[2]), Path(sys.argv[3]))
    parser = argparse.ArgumentParser(description=__doc__, allow_abbrev=False)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--artifacts-dir', type=Path, required=True)
    parser.add_argument('--profiles', nargs='+', choices=PROFILES, default=list(PROFILES))
    args = parser.parse_args()
    for profile in args.profiles:
        run(args, profile)
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
