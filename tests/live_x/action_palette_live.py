#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
"""Exercise the optional native action palette against real core/module pipes.

Run on native Linux with Xvfb and XTest. Receipts contain original module
commands, actual chunked core replies, X observations and native screenshots.
The transparent module and worker proxies do not invent provider responses.
This measures bounded palette work, not physical displays or kernel wakeups.
"""
import argparse
import base64
import ctypes as C
import hashlib
import json
import os
from pathlib import Path
import re
import select
import shlex
import shutil
import signal
import struct
import sys
import tempfile
import time
import traceback

from desktop_live import connect_client
from inspector_live import NativeView, atomic_json
from live_x_harness import HarnessRuntime, start_display_server, wait_for_predicate
from native_bindings_live import module_proxy, rows, worker_proxy
from native_capture import NativeCapture
from session_x_errors import SessionXErrors

PROFILES = ('slate-dark', 'dune-light', 'classic-large')
TITLE = 'Action palette'
WORD = struct.calcsize('@L')
CONFIG, CONFIG_END, STRING = 1 << 18, 1 << 19, 1 << 22
ECHO, REPLY = (1 << 31) | (1 << 9), (1 << 31) | (1 << 10)
CAP = 1024 * 1024
OBSERVER_NODE_BUDGET = 16384
OBSERVER_DEPTH_BUDGET = 5
LITERAL = 'Palette ; $[exec.touch] `literal` 100%'
LOCALIZED = 'Palette Résumé ; $[exec.touch]'
ARGV = ['literal ; argument', '$[exec.touch]', '100%', '`unchanged`']


class PaletteNativeView(NativeView):
    def named(self, title):
        # Our 520 actual clients also have FVWM frames/decorations. This
        # observer-only X tree budget counts those separate X windows; the
        # product's 4096 managed-record scan and 512 retained rows stay fixed.
        queue, visited = [(self.client.root, 0)], 0
        while queue:
            window, depth = queue.pop()
            visited += 1
            assert visited <= OBSERVER_NODE_BUDGET
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
            if depth < OBSERVER_DEPTH_BUDGET:
                queue.extend((child, depth + 1) for child in self.children(window))
        return None


def module_probe(arguments):
    """Independent native module: bounded files in, original directed replies out."""
    output, incoming = map(int, arguments[:2])
    root = Path(arguments[-1])

    def send(command):
        data = command.encode('utf-8')
        assert len(data) <= 1000 and not any(c in command for c in '\0\r\n')
        wire = struct.pack('@LL', 0, len(data)) + data + struct.pack('@L', 1)
        while wire:
            written = os.write(output, wire)
            assert written > 0
            wire = wire[written:]

    send('SET_MASK ' + str(CONFIG | CONFIG_END | STRING))
    send('SET_MASK ' + str(ECHO | REPLY))
    send('NOP FINISHED STARTUP')
    atomic_json(root / 'ready.json', {'pid': os.getpid()})
    previous = signal.getsignal(signal.SIGTERM)

    def stopped(signum, frame):
        raise KeyboardInterrupt('owned native probe stopped')

    signal.signal(signal.SIGTERM, stopped)
    buffer, pending, seen = bytearray(), None, set()
    lines, chunks, diagnostics = [], [], {}
    try:
        while True:
            if pending is None:
                commands = sorted(root.glob('command-*.json'))
                assert len(commands) <= 1024
                for path in commands:
                    if path.name in seen:
                        continue
                    pending = json.loads(path.read_text())
                    seen.add(path.name)
                    assert re.fullmatch('[0-9a-f]{32}', pending['token'])
                    lines, chunks, diagnostics = [], [], {}
                    if pending['mode'] == 'snapshot':
                        send('Send_ConfigInfo')
                    else:
                        assert len(pending['commands']) <= 64
                        for command in pending['commands']:
                            send(command)
                        if pending['mode'] == 'commands':
                            send('SendToModule PaletteProbe done ' + pending['token'])
                    break
            if not select.select([incoming], [], [], .02)[0]:
                continue
            block = os.read(incoming, 65536)
            if not block:
                assert not buffer, 'EOF inside native packet'
                return 0
            buffer.extend(block)
            assert len(buffer) <= CAP
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
                text = (body[3 * WORD:].split(b'\0', 1)[0].decode('utf-8', 'strict')
                        if kind in (CONFIG, STRING, ECHO, REPLY) else '')
                result = None
                if pending['mode'] == 'snapshot':
                    if kind == CONFIG and text.startswith(('*SlatePaletteForm', '*FvwmFormDefault')):
                        lines.append(text)
                        assert len(lines) <= 256
                    if kind == CONFIG_END:
                        result = {'lines': lines}
                elif pending['mode'] == 'commands':
                    if kind == STRING and text == 'done ' + pending['token']:
                        result = {'done': True}
                elif pending['mode'] == 'diagnostic' and kind == ECHO:
                    fields = text.split(' ', 2)
                    if len(fields) == 3 and fields[1] == pending['token']:
                        assert fields[0] in ('FVWM_PALETTE_STATE', 'FVWM_PALETTE_VIEW')
                        assert len(fields[2]) <= 2000
                        diagnostics[fields[0]] = json.loads(bytes.fromhex(fields[2]))
                        state = diagnostics.get('FVWM_PALETTE_STATE')
                        if state is not None and (not state['open'] or
                                'FVWM_PALETTE_VIEW' in diagnostics):
                            result = diagnostics
                elif pending['mode'] == 'query' and kind == REPLY:
                    prefix = 'MFL_CORE_1 ' + pending['cookie'] + ' ' + pending['serial'] + ' '
                    if not text.startswith(prefix):
                        continue
                    fields = text[len(prefix):].split()
                    if len(fields) == 3 and fields[0] == 'part':
                        assert int(fields[1]) == len(chunks)
                        assert 0 < len(fields[2]) <= 512 and len(fields[2]) % 2 == 0
                        chunks.append(bytes.fromhex(fields[2]))
                        assert sum(map(len, chunks)) <= CAP
                    else:
                        assert fields == ['end', str(len(chunks))] and chunks
                        result = {'reply': json.loads(b''.join(chunks)),
                                  'bytes': sum(map(len, chunks)), 'parts': len(chunks)}
                if result is not None:
                    atomic_json(root / (pending['token'] + '.json'), result)
                    pending = None
    except (KeyboardInterrupt, BrokenPipeError):
        return 0
    finally:
        for fd in (output, incoming):
            try:
                os.close(fd)
            except OSError:
                pass
        atomic_json(root / 'stopped.json', {'pid': os.getpid()})
        signal.signal(signal.SIGTERM, previous)


class Probe:
    def __init__(self, directory, core):
        self.root, self.core = directory, core
        self.cookie, self.serial = os.urandom(16).hex(), 0
        self.receipts = []

    def request(self, mode, commands=(), **fields):
        token = os.urandom(16).hex()
        atomic_json(self.root / ('command-' + token + '.json'),
            dict(token=token, mode=mode, commands=list(commands), **fields))
        path = self.root / (token + '.json')
        wait_for_predicate('palette directed probe ' + mode, path.exists,
            time.monotonic() + 7, process=self.core)
        return json.loads(path.read_text())

    def command(self, *commands):
        return self.request('commands', commands)

    def snapshot(self):
        return self.request('snapshot')['lines']

    def diagnostic(self):
        token = os.urandom(16).hex()
        request = dict(token=token, mode='diagnostic', commands=[
            'SendToModule FvwmDesktop palette-state ' + token])
        atomic_json(self.root / ('command-' + token + '.json'), request)
        path = self.root / (token + '.json')
        wait_for_predicate('palette diagnostics', path.exists,
            time.monotonic() + 6, process=self.core)
        value = json.loads(path.read_text())
        state = value['FVWM_PALETTE_STATE']
        if state['open']:
            state['view'] = value['FVWM_PALETTE_VIEW']
        return state

    def query(self, resource='palette', offset=0, limit=16, generation='-'):
        return self.exchange('Send_Query', resource, str(offset), str(limit), generation)

    def exchange(self, command, *arguments):
        self.serial += 1
        serial = '%016x' % self.serial
        text = ' '.join([command, self.cookie, serial, *arguments])
        value = self.request('query', [text], cookie=self.cookie, serial=serial)
        self.receipts.append(dict(command=text, **value))
        return value['reply']

    def release(self):
        value = self.exchange('Send_Palette', '-', '-')
        assert value == {'result': {'applied': False, 'action': 'release', 'target': ''}}, value
        return value

    def inventory(self):
        page = self.query()
        assert set(page) == {'result'}, page
        first, result = page['result'], []
        generation = first['generation']
        assert re.fullmatch('[0-9a-f]{48}', generation), first
        while True:
            p = page['result']
            assert p['state'] == 'observed' and p['scope'] == 'core-palette', p
            assert p['generation'] == generation and p['offset'] == len(result), p
            assert p['total'] == first['total'] <= 640 and p['truncated'] == first['truncated'], p
            assert len(p['rows']) <= 16 and p['coverage'], p
            for row in p['rows']:
                assert row['index'] == len(result) and row['kind'] in ('window', 'desktop'), row
                assert 0 < len(row['id']) <= 128 and row['id'].isascii(), row
                assert 0 < len(row['label'].encode()) <= 160, row
                assert len(row['detail'].encode()) <= 160 and row['enabled'], row
                assert row['icon'] in ('file', 'directory', 'other'), row
                result.append(row)
            offset = p['next_offset']
            if offset is None:
                break
            assert offset == len(result) < first['total'], p
            page = self.query(offset=offset, generation=generation)
            assert set(page) == {'result'}, page
        assert len(result) == first['total'] and len({r['id'] for r in result}) == len(result)
        return dict(first, rows=result)


def source_receipt(source, build):
    paths = ['tests/live_x/action_palette_live.py',
             'bin/FvwmPrompt/internal/desktopcontroller/palette.go',
             'bin/FvwmPrompt/internal/desktopcontroller/palette_native.go',
             'fvwm/module_interface.c', 'fvwm/palette_policy.c']
    return {'source_sha256': {name: hashlib.sha256((source / name).read_bytes()).hexdigest()
                              for name in paths},
            'binary_sha256': {str(path.relative_to(build)): hashlib.sha256(path.read_bytes()).hexdigest()
                              for path in (build / 'fvwm3', build / 'bin/FvwmPrompt/FvwmDesktop')}}


def run(args, profile, run_root):
    source, build = Path(__file__).resolve().parents[2], args.build.resolve()
    out = run_root / profile
    out.mkdir()
    evidence = {'status': 'running', 'profile': profile, 'screenshots': [],
        'failures': [], 'cleanup_failures': [], 'scope': 'native Xvfb/XTest; no physical display, '
        'assistive technology, compositor or kernel wakeup qualification',
        'idle_limits': {'seconds': 3, 'desktop_poll_ms': 250,
                        'desktop_status_sampler_seconds': 5},
        'observer_bounds': {'x_tree_nodes': OBSERVER_NODE_BUDGET,
                            'x_tree_depth': OBSERVER_DEPTH_BUDGET,
                            'fixture_bulk_clients': 520}, **source_receipt(source, build)}
    with HarnessRuntime(prefix='fvwm-action-palette-') as runtime:
        client, errors, core = None, None, None
        windows, probes = [], []

        def module_cleanup_fence(phase):
            started = time.monotonic()
            pending = []

            def actual_wait_receipts():
                pending.clear()
                result, probe_receipts = [], []
                for path in (runtime.root / 'native-records').glob('module-*.json'):
                    record = json.loads(path.read_text())
                    stopped_path = path.with_name('stopped-' + str(record['proxy']) + '.json')
                    if not stopped_path.exists():
                        pending.append(dict(kind='module-child-wait', **record))
                        continue
                    stopped_record = json.loads(stopped_path.read_text())
                    assert stopped_record['child'] == record['child']
                    assert stopped_record['returncode'] is not None
                    result.append(dict(record, reaped=stopped_record))
                for path in runtime.root.glob('probe*/ready.json'):
                    record = json.loads(path.read_text())
                    stopped_path = path.with_name('stopped.json')
                    if not stopped_path.exists():
                        pending.append(dict(kind='native-probe-exit', directory=str(path.parent), **record))
                        continue
                    stopped_record = json.loads(stopped_path.read_text())
                    assert stopped_record['pid'] == record['pid']
                    probe_receipts.append(dict(directory=str(path.parent), **stopped_record))
                return dict(modules=result, probes=probe_receipts) if not pending else None

            # ManagedProcess.close waits for the core, while each transparent
            # proxy separately waits for its own real module child. Keep the
            # private receipt directory alive until those actual waits finish.
            # The already-exited core must not terminate this condition wait.
            try:
                result = wait_for_predicate('actual owned module cleanup receipts: ' + phase,
                    actual_wait_receipts, time.monotonic() + 5)
            except BaseException:
                evidence.setdefault('cleanup_fences', []).append(dict(phase=phase,
                    status='failed', seconds=time.monotonic() - started, pending=list(pending)))
                raise
            evidence.setdefault('cleanup_fences', []).append(dict(phase=phase,
                status='passed', seconds=time.monotonic() - started,
                module_wait_receipts=len(result['modules']), probe_exit_receipts=len(result['probes'])))
            evidence['owned_module_children_reaped'] = result['modules']
            evidence['owned_native_probe_exit_receipts'] = result['probes']
            return result

        try:
            _, display = start_display_server(runtime, 'Xvfb',
                ['Xvfb', '-screen', '0', '1440x1200x24', '-noreset'],
                'xserver.log', time.monotonic() + 10)
            modules, user, native, jobs, directory = [runtime.root / name for name in
                ('modules', 'user', 'native-records', 'desktop-workers', 'probe')]
            for path in (modules, user, native, jobs, directory):
                path.mkdir(mode=0o700)
            for path in (build / 'modules').iterdir():
                executable = path / path.name
                if executable.is_file() and path.name != 'FvwmForm':
                    (modules / path.name).symlink_to(executable)
            fixture = str(Path(__file__).resolve())

            def script(path, arguments, suffix=''):
                path.write_text('#!/bin/sh\nexec ' + shlex.join([sys.executable,
                    fixture, *map(str, arguments)]) + suffix + '\n')
                path.chmod(0o700)

            for name, binary in [('FvwmDesktop', build / 'bin/FvwmPrompt/FvwmDesktop'),
                                 ('FvwmForm', build / 'modules/FvwmForm/FvwmForm')]:
                script(modules / name, ['--module-proxy', binary, native],
                    ' "$@" 2>>' + shlex.quote(str(runtime.root / (name + '.log'))))
            script(modules / 'PaletteProbe', ['--probe'],
                ' "$@" 2>>' + shlex.quote(str(runtime.root / 'probe.log')))
            worker = runtime.root / 'desktop-worker'
            script(worker, ['--desktop-worker', build / 'bin/FvwmPrompt/fvwm-desktop-worker', jobs])
            marker = runtime.root / 'argv.json'
            apps = runtime.root / 'data/applications'
            apps.mkdir(parents=True)

            def desktop_file(path, label, arguments):
                # This existing launcher explicitly supports POSIX word
                # quoting without evaluation. %% is its literal percent code.
                quoted = shlex.join([str(value).replace('%', '%%') for value in arguments])
                path.write_text('[Desktop Entry]\nType=Application\nName=' + label +
                    '\nExec=' + quoted + '\n')

            app = apps / 'palette-argv.desktop'
            desktop_file(app, 'Palette argv fixture',
                [sys.executable, fixture, '--app-record', marker, *ARGV])
            desktop_file(apps / 'literal.desktop', LITERAL,
                [sys.executable, fixture, '--app-record', marker, 'literal-name-activated'])
            desktop_file(apps / 'peer.desktop', 'Palette stable peer',
                [sys.executable, fixture, '--app-record', marker, 'peer-activated'])
            palette, appearance = ('dune', 'light') if profile == 'dune-light' else ('slate', 'dark')
            local = user / 'local.config'
            local.write_text('*FvwmDesktop: palette ' + palette +
                '\n*FvwmDesktop: appearance ' + appearance +
                '\n*FvwmDesktop: motion Off\n*FvwmDesktop: transparency Off\n')
            assets = source / 'default-config'
            body = (assets / 'config').read_text().replace('$[FVWM_DATADIR]/default-config', str(assets))
            if profile == 'classic-large':
                body = body.replace('Sans:size=10:antialias=True',
                    'DejaVu Sans Mono:size=18:antialias=True')
            config = runtime.root / 'config'
            config.write_text('ModulePath ' + str(modules) + '\nRead ' +
                str(source / 'fvwm/ConfigFvwmDefaults') + '\n' + body +
                '\nEwmhNumberOfDesktops 6\nDesktopName 4 Palette desk four\n'
                'DesktopName 5 Palette desk five\nAddToFunc StartFunction I Module PaletteProbe ' +
                str(directory) + '\n')
            env = runtime.environment({'DISPLAY': display, 'HOME': str(runtime.root),
                'FVWM_USERDIR': str(user), 'FVWM_DESKTOP_ROOT': str(assets),
                'FVWM_DESKTOP_WORKER': str(worker), 'FVWMMFL_SOCKET_PATH': str(runtime.root),
                'XDG_DATA_HOME': str(apps.parent), 'XDG_DATA_DIRS': str(runtime.root / 'absent'),
                'LANG': 'C.UTF-8', 'LC_ALL': 'C.UTF-8'})
            evidence.update(display=display, config_sha256=hashlib.sha256(config.read_bytes()).hexdigest(),
                font='DejaVu Sans Mono 18' if profile == 'classic-large' else 'Sans 10')
            core = runtime.start('core', [str(build / 'fvwm3'), '-d', display,
                '-f', str(config), '-v', '-o', '-'], 'core.log', env)
            wait_for_predicate('independent palette native probe', (directory / 'ready.json').exists,
                time.monotonic() + 12, process=core)
            probe = Probe(directory, core)
            probes.append(probe)
            client = connect_client(display)
            errors = SessionXErrors(client, evidence).install()
            view = PaletteNativeView(client, errors, core)

            def make_window(name):
                window = client.x.XCreateSimpleWindow(client.display, client.root,
                    140, 120, 320, 160, 0, 0, 0x384650)
                assert window
                windows.append(window)
                client.x.XStoreName(client.display, window, name.encode())
                client.x.XMapWindow(client.display, window)
                client.x.XSync(client.display, 0)
                wait_for_predicate('managed ' + name[:40],
                    lambda: client.parent(window) != client.root,
                    time.monotonic() + 5, process=core)
                return window

            prior = make_window('Palette prior focus')
            target = make_window('Palette target focus')
            literal_window = make_window(LITERAL)
            localized_window = make_window(LOCALIZED)
            wait_for_predicate('initial application catalog committed',
                lambda: any('AddToMenu SlateLauncherMenu ' in row['text'] and
                    'Palette argv fixture' in row['text'] for row in rows(native)
                    if row['direction'] == 'out'), time.monotonic() + 12, process=core)
            probe.command('WindowId %d Focus' % prior)
            wait_for_predicate('initial client focus', lambda: view.focus() == prior,
                time.monotonic() + 5, process=core)

            def ready():
                value = probe.diagnostic()
                return value if value['open'] and value['native_ready'] and \
                    not value['binding']['in_flight'] and not value['binding']['pending'] else None

            def settled(query=None):
                def matches():
                    value = ready()
                    return value if value and (query is None or
                        value['view']['query'] == query) else None
                # XTest's Form and this directed probe are independent native
                # producers. An old idle diagnostic cannot acknowledge a new
                # Form callback; observe its expected model state explicitly.
                return wait_for_predicate('settled palette native publication' +
                    ('' if query is None else ' for query ' + repr(query)), matches,
                    time.monotonic() + 12, process=core)

            def native_filter_callback(after):
                def observed():
                    callbacks = [r['text'] for r in rows(native)[after:]
                        if r['direction'] == 'in' and
                        r['text'].startswith('palette-control ') and ' filter ' in r['text']]
                    return callbacks[-1] if callbacks else None
                return wait_for_predicate('actual native Form filter callback', observed,
                    time.monotonic() + 5, process=core)

            def focus():
                window = view.wait(TITLE)
                probe.command('WindowId %d Focus' % window)
                wait_for_predicate('native palette keyboard focus', lambda: view.focus() == window,
                    time.monotonic() + 5, process=core)
                return window

            def chord(symbol, modifier):
                code = client.x.XKeysymToKeycode(client.display,
                    client.x.XStringToKeysym(modifier.encode()))
                assert code
                assert view.pointer.x.XTestFakeKeyEvent(client.display, code, 1, 0)
                view.key(symbol)
                assert view.pointer.x.XTestFakeKeyEvent(client.display, code, 0, 0)
                client.x.XSync(client.display, 0)

            def opened(keyboard=False):
                if keyboard:
                    chord('a', 'Super_L')
                else:
                    probe.command('SendToModule FvwmDesktop palette')
                view.wait(TITLE)
                return settled()

            def closed(return_to=None):
                focus()
                view.key('Escape')
                wait_for_predicate('Escape closes native palette',
                    lambda: view.named(TITLE) is None, time.monotonic() + 5, process=core)
                state = probe.diagnostic()
                assert state == {'open': False}, state
                if return_to is not None:
                    wait_for_predicate('Cancel restores managed focus', lambda: view.focus() == return_to,
                        time.monotonic() + 5, process=core)

            def capture(name):
                previous = os.environ.get('DISPLAY')
                os.environ['DISPLAY'] = display
                try:
                    path = Path(NativeCapture(build / 'bin/fvwm-desktop-capture').screenshot(out)
                        .removeprefix('Screenshot saved: '))
                    path.replace(out / name)
                    evidence['screenshots'].append(name)
                finally:
                    if previous is None:
                        os.environ.pop('DISPLAY', None)
                    else:
                        os.environ['DISPLAY'] = previous

            def type_query(text):
                focus()
                # Input is first in the Form. Shift-Tab from an unknown focus
                # is unnecessary: each caller keeps focus in that input.
                view.key('u', control=True)
                symbols = {' ': 'space', ';': 'semicolon', '$': 'dollar',
                           '[': 'bracketleft', ']': 'bracketright', '`': 'grave',
                           '%': 'percent', ':': 'colon'}
                for char in text:
                    if char in '$%:' or char.isupper():
                        chord(symbols.get(char, char.lower()), 'Shift_L')
                    else:
                        view.key(symbols.get(char, char))
                view.key('F3')
                value = settled(query=text)
                assert value['view']['query'] == text, value
                return value

            initial = opened(keyboard=True)
            assert initial['states'] == ['observed'] * 4, initial
            assert initial['counts'][0] >= 4 and initial['counts'][1] >= 6, initial
            assert initial['counts'][2:] == [3, 8], initial
            assert initial['view']['total'] == sum(initial['counts']) <= 1156, initial
            lines = probe.snapshot()
            assert any('InputHex palette_query' in line for line in lines), lines
            assert any('ListViewport 18 1' in line for line in lines), lines
            assert any('ListVersioned' in line for line in lines), lines
            assert any('Font' in line and ('size=18' if profile == 'classic-large' else 'size=10')
                in line for line in lines), lines
            evidence['initial'] = dict(state=initial, form_configuration=lines,
                pixels=view.pixels(view.wait(TITLE)))
            capture('palette-ready.png')
            closed(prior)

            inventory = probe.inventory()
            assert not inventory['truncated'], inventory
            assert any(r['kind'] == 'window' and r['label'] == LITERAL for r in inventory['rows'])
            assert any(r['kind'] == 'window' and r['label'] == LOCALIZED for r in inventory['rows'])
            desks = [r for r in inventory['rows'] if r['kind'] == 'desktop']
            assert len(desks) >= 6, desks
            unchanged = probe.inventory()
            assert {r['id'] for r in inventory['rows']} == {r['id'] for r in unchanged['rows']}
            assert inventory['generation'] != unchanged['generation']
            evidence['core_inventory'] = unchanged
            probe.release()

            opened()
            filtered = type_query('palette target focus')
            assert filtered['visible'] == 1, filtered
            capture('palette-filtered.png')
            view.key('Tab')
            view.key('Home')
            view.key('space')
            view.key('Return')
            wait_for_predicate('window activation closes palette', lambda: not view.named(TITLE),
                time.monotonic() + 5, process=core)
            wait_for_predicate('shared engine focuses selected window', lambda: view.focus() == target,
                time.monotonic() + 5, process=core)
            evidence['native_window_action'] = dict(filtered=filtered, target=target, focus=view.focus())

            opened()
            literal_filter = type_query('palette ;')
            assert literal_filter['visible'] == 2, literal_filter
            assert not marker.exists(), 'literal filter executed application metadata'
            newer_filter = type_query('palette stable peer')
            assert newer_filter['visible'] == 1 and newer_filter['view']['generation'] > \
                literal_filter['view']['generation'], newer_filter
            evidence['literal_filter_supersession'] = dict(first=literal_filter, latest=newer_filter)
            closed(target)

            opened()
            type_query('sum')
            literal_commands = [r['text'] for r in rows(native) if r['direction'] == 'out'
                and 'ListEntry picks' in r['text'] and 'Palette Résumé' in r['text']]
            assert literal_commands, 'localized glyphs were not preserved in native display transport'
            assert all('[U+0024][exec.touch]' in command and '$' not in command
                for command in literal_commands), literal_commands
            metachar_commands = [r['text'] for r in rows(native) if r['direction'] == 'out'
                and 'ListEntry picks' in r['text'] and 'Palette ; ' in r['text']]
            assert metachar_commands and all('[U+0024][exec.touch]' in command and
                '100[U+0025]' in command and '$' not in command and '%' not in command
                for command in metachar_commands), metachar_commands
            before = len(rows(native))
            view.key('F3')
            callback = native_filter_callback(before).split()
            callback[3] = 'x' + base64.urlsafe_b64encode('rés'.encode()).decode().rstrip('=')
            probe.command('SendToModule FvwmDesktop ' + ' '.join(callback))
            localized = settled(query='rés')
            assert localized['view']['query'] == 'rés' and localized['visible'] == 1, localized
            capture('palette-localized.png')
            evidence['localized_literal_filter'] = dict(state=localized,
                native_label_commands=literal_commands,
                escaped_metachar_commands=metachar_commands,
                input_method='ASCII sum via XTest; rés via actual versioned Form callback',
                pixels=view.pixels(view.wait(TITLE)), shaping_or_ime_qualified=False,
                target=localized_window)
            closed()

            opened()
            app_filter = type_query('palette argv fixture')
            assert app_filter['visible'] == 1, app_filter
            view.key('Return')
            wait_for_predicate('fixture application invoked', marker.exists,
                time.monotonic() + 6, process=core)
            argv = json.loads(marker.read_text())
            assert argv['argv'] == ARGV, argv
            evidence['native_application_argv'] = argv
            marker.unlink()

            opened()
            action_filter = type_query('action: settings')
            assert action_filter['visible'] == 1, action_filter
            mapped_before = {window for window in view.children(client.root)
                if (attributes := view.attributes(window)) and attributes.map_state == 2}
            local_before = local.read_bytes()
            saved_path = user / 'desktop-state.json'
            saved_before = saved_path.read_bytes() if saved_path.exists() else None
            action_started = time.monotonic()
            view.key('Return')

            def mapped_settings_menu():
                if not view.pointer.grabbed():
                    return None
                submitted = any(row['direction'] == 'out' and row['time'] >= action_started and
                    row['text'] == 'Menu SlateSettings Monitor c c' for row in rows(native))
                if not submitted:
                    return None
                for window in set(view.children(client.root)) - mapped_before:
                    attributes = view.attributes(window)
                    if attributes and attributes.map_state == 2 and attributes.save_under and \
                            attributes.width > 20 and attributes.height > 20:
                        return window
                return None

            settings = wait_for_predicate('registered Settings native menu mapped',
                mapped_settings_menu, time.monotonic() + 5, process=core)
            assert not view.named(TITLE)
            settings_pixels = view.pixels(settings)
            capture('palette-settings-action.png')
            view.key('Escape')
            wait_for_predicate('registered Settings menu dismissed with Escape',
                lambda: not view.pointer.grabbed(), time.monotonic() + 5, process=core)
            assert local.read_bytes() == local_before
            assert (saved_path.read_bytes() if saved_path.exists() else None) == saved_before
            evidence['native_registered_action'] = dict(state=action_filter,
                submitted_command='Menu SlateSettings Monitor c c', native_pixels=settings_pixels,
                local_configuration_unchanged=True, saved_preferences_unchanged=True)

            # Query the actual advertised mode/names, then activate a specific
            # advertised row; no assumption that all modes expose four desks.
            current = probe.inventory()
            desk = next(r for r in current['rows'] if r['kind'] == 'desktop' and 'five' in r['label'])
            result = probe.exchange('Send_Palette', current['generation'], str(desk['index']))
            assert result == {'result': {'applied': True, 'action': 'desktop', 'target': desk['id']}}, result
            desktop = probe.query(resource='workspaces', limit=8)
            assert any(m['desk'] == 5 for m in desktop['result']['monitors']), desktop
            evidence['core_desktop_activation'] = dict(row=desk, action=result, observed=desktop)
            probe.command('GotoDesk 0 0')
            probe.release()
            opened()
            native_desk = type_query('palette desk five')
            assert native_desk['visible'] == 1, native_desk
            view.key('Return')
            wait_for_predicate('native desktop action closes palette', lambda: not view.named(TITLE),
                time.monotonic() + 5, process=core)
            desktop = probe.query(resource='workspaces', limit=8)
            assert any(m['desk'] == 5 for m in desktop['result']['monitors']), desktop
            evidence['native_desktop_activation'] = dict(state=native_desk, observed=desktop)
            probe.command('GotoDesk 0 0', 'WindowId %d Focus' % target)

            current = probe.inventory()
            stale = next(r for r in current['rows'] if r['label'] == LITERAL and r['kind'] == 'window')
            client.x.XDestroyWindow(client.display, literal_window)
            client.x.XSync(client.display, 0)
            windows.remove(literal_window)
            replacement = make_window(LITERAL)
            result = probe.exchange('Send_Palette', current['generation'], str(stale['index']))
            assert result == {'error': {'code': 'stale_target'}}, result
            evidence['destroyed_window_guard'] = dict(action=result, old_xid=literal_window,
                new_xid=replacement, actual_xid_reuse=literal_window == replacement,
                forced_identity_reuse='separate pure C policy coverage')
            probe.release()

            opened()
            (jobs / 'fail-apps').touch()
            focus()
            view.key('F5')

            def app_lost():
                value = ready()
                return value if value and value['states'][2] == 'unavailable' else None

            lost = wait_for_predicate('actual app worker loss reaches palette', app_lost,
                time.monotonic() + 8, process=core)
            assert lost['counts'][2] == 3 and lost['states'][3] == 'observed', lost
            capture('palette-unavailable.png')
            focus()
            view.key('F5')

            def app_recovered():
                value = ready()
                return value if value and value['states'] == ['observed'] * 4 else None

            recovered = wait_for_predicate('real application discovery recovery', app_recovered,
                time.monotonic() + 8, process=core)
            capture('palette-recovered.png')
            evidence['app_loss_recovery'] = dict(lost=lost, recovered=recovered)
            closed()

            # App callbacks carry both native generation and application
            # publication identity. Replay the real old native callback only.
            opened()
            stale_before = type_query('palette argv fixture')
            before = len(rows(native))
            view.key('F3')
            old_filter = native_filter_callback(before)
            desktop_file(app, 'Palette changed application',
                [sys.executable, fixture, '--app-record', marker, 'replacement-activated'])
            probe.command('SendToModule FvwmDesktop discover')
            def replaced_application():
                value = ready()
                return value if value and value['view']['query'] == 'palette argv fixture' and \
                    value['visible'] == 0 and value['view']['generation'] > \
                    stale_before['view']['generation'] else None

            replaced = wait_for_predicate('application replacement native publication',
                replaced_application, time.monotonic() + 8, process=core)
            # A stale Form generation is rejected even if the old row index
            # happens to identify a different application in the new catalog.
            old_activate = old_filter.replace(' filter ', ' activate ', 1)
            probe.command('SendToModule FvwmDesktop ' + old_activate)
            probe.diagnostic()
            assert not marker.exists(), 'superseded native application callback executed'
            evidence['stale_native_application_callback'] = dict(callback=old_activate,
                before=stale_before, replacement=replaced)
            notice = view.named('FVWM-X')
            if notice:
                probe.command('WindowId %d Close' % notice)
                wait_for_predicate('stale callback notice dismissed', lambda: not view.named('FVWM-X'),
                    time.monotonic() + 5, process=core)
            closed()

            # Three independent native producers plus the live optional view
            # exhaust the four snapshot slots. Closing the view must make its
            # slot immediately reusable, without an expiry timer or stats shim.
            for index in range(3):
                extra = runtime.root / ('probe-%d' % index)
                extra.mkdir(mode=0o700)
                probe.command('Module PaletteProbe ' + str(extra))
                wait_for_predicate('additional directed producer', (extra / 'ready.json').exists,
                    time.monotonic() + 5, process=core)
                probes.append(Probe(extra, core))
            for producer in probes[:3]:
                producer.inventory()
            opened()
            blocked = probes[3].query()
            assert blocked == {'error': {'code': 'palette_busy'}}, blocked
            open_state = probe.diagnostic()
            closed()
            admitted = probes[3].inventory()
            evidence['close_releases_snapshot'] = dict(before=blocked, admitted=admitted,
                retired_lifetime=open_state['view']['token'])
            # An old close from the retired lifetime cannot close its successor.
            for producer in probes:
                producer.release()
            successor = opened()
            probe.command('SendToModule FvwmDesktop palette-control ' +
                open_state['view']['token'] + ' close')
            assert probe.diagnostic()['view']['token'] == successor['view']['token']
            assert view.named(TITLE)
            closed()

            # Exercise the actual native cap and its truthful user-facing
            # omission text with more than 512 eligible managed windows.
            bulk = []
            for index in range(520):
                window = client.x.XCreateSimpleWindow(client.display, client.root,
                    20, 20, 64, 32, 0, 0, 0)
                assert window
                windows.append(window)
                bulk.append(window)
                client.x.XStoreName(client.display, window,
                    ('Palette bulk %03d' % index).encode())
                client.x.XMapWindow(client.display, window)
            client.x.XSync(client.display, 0)

            def bulk_managed():
                value = probe.inventory()
                return value if value['truncated'] and len([r for r in value['rows']
                    if r['kind'] == 'window']) == 512 else None

            bounded = wait_for_predicate('bounded real managed-window inventory', bulk_managed,
                time.monotonic() + 15, process=core)
            probe.release()
            limited = opened()
            assert limited['counts'][0] == 512 and limited['view']['total'] <= 1156, limited
            emitted = [r['text'] for r in rows(native) if r['direction'] == 'out']
            assert any('more omitted' in c and 'SetLabel' in c for c in emitted), emitted[-30:]
            evidence['large_inventory'] = dict(core=bounded, native=limited)
            capture('palette-bounded.png')
            probe.command(*(['SendToModule FvwmDesktop palette'] * 16))
            pressure = probe.diagnostic()
            drained = settled()
            delta = {key: drained['binding'][key] - limited['binding'][key]
                     for key in ('invalidations', 'started', 'published', 'rejected')}
            assert delta['invalidations'] == 16, delta
            assert delta['started'] <= 2 and delta['published'] == 1, delta
            assert delta['rejected'] >= 1, delta
            assert not drained['binding']['pending'] and not drained['binding']['in_flight'], drained
            evidence['query_pressure'] = dict(before=limited, during=pressure, after=drained, delta=delta,
                cause='sixteen real native intents against the paged 512-window snapshot')
            closed()
            for window in bulk:
                client.x.XDestroyWindow(client.display, window)
                windows.remove(window)
            client.x.XSync(client.display, 0)

            # Quiet binding work is separate from the existing controller poll
            # and status sampler. All diagnostics themselves are explicit work.
            stable = opened()
            start = time.monotonic()
            end = start + 3
            wait_for_predicate('three-second palette idle observation',
                lambda: time.monotonic() >= end, end + 1, process=core)
            after = settled()
            for key in ('started', 'published', 'invalidations', 'rejected'):
                assert stable['binding'][key] == after['binding'][key], (stable, after)
            closed()
            start = time.monotonic()
            end = start + 3
            wait_for_predicate('three-second closed palette observation',
                lambda: time.monotonic() >= end, end + 1, process=core)
            activity = [r for r in rows(native) if r['time'] >= start and r['direction'] == 'out'
                and ('SlatePaletteForm' in r['text'] or re.search(r'^Send_(Query|Palette) ', r['text']))]
            assert not activity, activity
            evidence['idle'] = dict(before=stable, after=after, closed_unsolicited=activity)
            commands = [r['text'] for r in rows(native) if r['direction'] == 'out' and
                ('SlatePaletteForm' in r['text'] or r['text'].startswith(('Send_Query ', 'Send_Palette ')))]
            assert commands and max(len(c.encode()) for c in commands) <= 1000
            evidence['native_command_bounds'] = dict(count=len(commands),
                maximum_bytes=max(len(c.encode()) for c in commands), limit=1000)
            assert not marker.exists(), 'metadata or stale action unexpectedly invoked an app'

            # A real replacement WM process gives the same surviving X client
            # a different core incarnation. Old observations remain unusable
            # even when the protocol owner and row index are recreated.
            before_restart = probe.inventory()
            before_row = next(r for r in before_restart['rows'] if r['label'] == 'Palette target focus')
            old_cookie = probe.cookie
            core.close(time.monotonic() + 4)
            module_cleanup_fence('before-core-replacement')
            restart_directory = runtime.root / 'probe-restart'
            restart_directory.mkdir(mode=0o700)
            restart_config = runtime.root / 'config-restart'
            restart_config.write_text(config.read_text().replace(str(directory), str(restart_directory)))
            core = runtime.start('core-restarted', [str(build / 'fvwm3'), '-d', display,
                '-f', str(restart_config), '-v', '-o', '-'], 'core-restarted.log', env)
            wait_for_predicate('replacement core directed native probe',
                (restart_directory / 'ready.json').exists, time.monotonic() + 12, process=core)
            probe = Probe(restart_directory, core)
            probes.append(probe)
            probe.cookie = old_cookie
            view.core = core
            old_action = probe.exchange('Send_Palette', before_restart['generation'], str(before_row['index']))
            assert old_action == {'error': {'code': 'stale_generation'}}, old_action
            after_restart = probe.inventory()
            after_row = next(r for r in after_restart['rows'] if r['label'] == 'Palette target focus')
            assert before_restart['generation'][:32] != after_restart['generation'][:32]
            assert before_row['id'] != after_row['id'] and view.named('Palette target focus') == target
            evidence['core_incarnation_replacement'] = dict(before=before_restart,
                after=after_restart, rejected_old_action=old_action, surviving_client_xid=target)
            probe.release()
            evidence['status'] = 'passed'
        except BaseException as failure:
            evidence['status'] = 'failed'
            evidence['failures'].append({'type': type(failure).__name__, 'message': str(failure),
                                        'traceback': traceback.format_exc()})
            raise
        finally:
            def cleanup(label, action):
                try:
                    action()
                except BaseException as failure:
                    evidence['cleanup_failures'].append(dict(step=label,
                        type=type(failure).__name__, message=str(failure)))

            if client:
                for window in windows:
                    cleanup('destroy fixture window', lambda w=window:
                        client.x.XDestroyWindow(client.display, w))
            if errors:
                cleanup('close observer display', errors.close_display)
            elif client:
                cleanup('close observer display', client.close)
            for process in reversed(runtime.processes):
                cleanup('stop ' + process.name, lambda p=process: p.close(time.monotonic() + 3))
            evidence['children_after_cleanup'] = {p.name: p.poll() for p in runtime.processes}
            evidence['directed_probe_receipts'] = [dict(cookie=p.cookie, receipts=p.receipts) for p in probes]

            cleanup('await actual module child wait receipts',
                lambda: module_cleanup_fence('final-runtime-cleanup'))
            for path in runtime.root.iterdir():
                if path.is_dir() and (path.name in ('native-records', 'desktop-workers', 'user')
                        or path.name.startswith('probe')):
                    cleanup('copy ' + path.name, lambda p=path: shutil.copytree(p, out / p.name))
                elif path.is_file() and (path.suffix == '.log' or path.name.startswith('config')
                        or path.name == 'desktop-worker'):
                    cleanup('copy ' + path.name, lambda p=path: shutil.copy2(p, out / p.name))
            if evidence['cleanup_failures'] or evidence.get('observer_unexpected_x_errors'):
                evidence['status'] = 'failed'
            atomic_json(out / 'evidence.json', evidence)
    assert not evidence['cleanup_failures'], evidence['cleanup_failures']
    assert not evidence.get('observer_unexpected_x_errors'), evidence.get('observer_unexpected_x_errors')
    assert all(code is not None for code in evidence['children_after_cleanup'].values()), evidence
    print('native action palette ' + profile + ' passed: ' + str(out))


def main():
    if len(sys.argv) > 1 and sys.argv[1] == '--probe':
        return module_probe(sys.argv[2:])
    if len(sys.argv) > 1 and sys.argv[1] == '--module-proxy':
        return module_proxy(sys.argv[2], sys.argv[3], sys.argv[4:])
    if len(sys.argv) == 4 and sys.argv[1] == '--desktop-worker':
        return worker_proxy(sys.argv[2], sys.argv[3])
    if len(sys.argv) >= 3 and sys.argv[1] == '--app-record':
        atomic_json(Path(sys.argv[2]), {'argv': sys.argv[3:], 'pid': os.getpid()})
        return 0
    parser = argparse.ArgumentParser(description=__doc__, allow_abbrev=False)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--artifacts-dir', type=Path, required=True)
    parser.add_argument('--profiles', nargs='+', choices=PROFILES, default=list(PROFILES))
    args = parser.parse_args()
    args.artifacts_dir.mkdir(parents=True, exist_ok=True)
    run_root = Path(tempfile.mkdtemp(prefix='action-palette-', dir=args.artifacts_dir.resolve()))
    atomic_json(run_root / 'run.json', {'profiles': args.profiles,
        'build': str(args.build.resolve()), 'source': str(Path(__file__).resolve().parents[2]),
        'started_unix': time.time()})
    for profile in args.profiles:
        run(args, profile, run_root)
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
