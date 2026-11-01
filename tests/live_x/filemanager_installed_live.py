#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or any later version.
"""Actual installed FileManager1 service on a private bus and owned X11.

Enabled mode has eight groups; disabled mode has two. The optional Linux-only
file-transfer propagation group observes startup/awareness, not transfer jobs.
Installed browser/helper paths are explicit identities, never helper overrides.
Core/MFL/Form may come from a separately qualified enabled installed prefix.
No image, theme, protocol matrix, whole desktop, #77 or native-CPU qualification
is inferred. Independent browsers are closed with actual Form Escape input;
their exit codes are unavailable after the original service owner exits.
"""
import argparse
import base64
import ctypes as C
import hashlib
import json
import os
import platform
from pathlib import Path
import re
import shlex
import shutil
import stat
import subprocess
import sys
import time

from browser_continuity_live import ClassHint, cleanup_receipts
from file_transfer_browser_live import process_records
from filemanager_bus_live import BUS_NAME, BusClient, uri
from geometry_transition_live import Client
from inspector_live import NativeView, atomic_json
from live_x_harness import HarnessRuntime, start_display_server, wait_for_predicate
from mfl_request_live import frame
from mfl_stream_live import Connection
from native_bindings_live import rows
from session_x_errors import SessionXErrors

MAX_BYTES = 8 * 1024 * 1024


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def bounded(path, maximum=MAX_BYTES):
    with path.open('rb') as stream:
        value = stream.read(maximum + 1)
    assert len(value) <= maximum, 'fixture output byte bound exceeded'
    return value


def defaults_identity(path):
    value = path.stat()
    return dict(sha256=digest(path), mode=stat.S_IMODE(value.st_mode),
        mtime_ns=value.st_mtime_ns)


class Service:
    """Own the actual public CLI directly; signal no browser descendants."""
    def __init__(self, name, binary, arguments, environment, out, evidence, cwd):
        self.name, self.out = name, out
        self.stdout = out / (name + '.stdout')
        self.stderr = out / (name + '.stderr')
        self.streams = [self.stdout.open('wb'), self.stderr.open('wb')]
        try:
            self.process = subprocess.Popen([str(binary), *arguments],
                stdin=subprocess.DEVNULL, stdout=self.streams[0],
                stderr=self.streams[1], env=environment, cwd=cwd,
                start_new_session=True)
        except BaseException:
            for stream in self.streams:
                stream.close()
            raise
        self.record = dict(name=name, pid=self.process.pid,
            binary=str(binary), started=time.monotonic(),
            stdout=self.stdout.name, stderr=self.stderr.name, wait=None)
        evidence.setdefault('owned_cli_processes', []).append(self.record)

    def poll(self):
        bounded(self.stdout)
        bounded(self.stderr)
        return self.process.poll()

    def wait(self, seconds=5):
        self.record['wait'] = self.process.wait(timeout=seconds)
        self.record.setdefault('waited', time.monotonic())
        bounded(self.stdout)
        bounded(self.stderr)
        return self.record['wait']

    def stop(self):
        if self.poll() is None:
            self.process.terminate()
        try:
            return self.wait()
        except subprocess.TimeoutExpired:
            # Cleanup remains finite, and a forced retirement is never a pass.
            self.record['forced_kill'] = True
            self.process.kill()
            self.record['wait'] = self.process.wait(timeout=2)
            raise

    def close_streams(self):
        for stream in self.streams:
            stream.close()


class Probe:
    def __init__(self, args, runtime, display, endpoint, native, core, evidence):
        self.args, self.runtime, self.native = args, runtime, native
        self.core, self.evidence, self.serial = core, evidence, 0
        self.known = set()
        self.peer = self.control = self.client = self.errors = self.view = None
        self.client = Client(display)
        self.errors = SessionXErrors(self.client, evidence).install()
        self.view = NativeView(self.client, self.errors, core)
        p, w, i = C.c_void_p, C.c_ulong, C.c_int
        signatures = {
            'XGetClassHint': ([p, w, C.POINTER(ClassHint)], i),
            'XInternAtom': ([p, C.c_char_p, i], w),
            'XSetInputFocus': ([p, w, i, w], i),
            'XGetWindowProperty': ([p, w, w, C.c_long, C.c_long, i, w,
                C.POINTER(w), C.POINTER(i), C.POINTER(w), C.POINTER(w),
                C.POINTER(p)], i),
        }
        for name, (arguments, result) in signatures.items():
            getattr(self.client.x, name).argtypes = arguments
            getattr(self.client.x, name).restype = result
        self.peer, self.control = Connection(endpoint), Connection(endpoint)
        for connection in (self.peer, self.control):
            connection.socket.settimeout(5)
            assert 'connection_profile' in connection.record()
        self.control.send(b'set echo\n')
        self.command('Nop')

    def wait(self, label, predicate, seconds=10):
        return wait_for_predicate(label, predicate, time.monotonic() + seconds,
            process=self.core)

    def command(self, text):
        self.serial += 1
        self.control.send((text + '\n').encode())
        self.control.echo('installed-filemanager-%d' % self.serial)

    def rpc(self, **fields):
        self.serial += 1
        token = 'installed-filemanager-%d' % self.serial
        self.peer.send(frame(token, resource='files', **fields))
        value = self.peer.record()['reply']
        assert value['id'] == token
        if value['status'] == 'error' and value['error']['code'] == 'not_ready':
            return None
        assert value['status'] == 'ok', value
        return value['result']

    def providers(self):
        value = self.rpc()
        assert value is not None
        result = value['providers']
        assert len(result) <= 8
        self.known.update(item['provider'] for item in result)
        return result

    def states(self, paths, selections=None):
        wanted = {os.fsencode(path) for path in paths}

        def observed():
            listed = self.providers()
            if len(listed) != len(wanted):
                return None
            result = {}
            for item in listed:
                state = self.rpc(provider=item['provider'], scope='state')
                if state is None or state['busy'] or \
                        not state['availability']['ready'] or \
                        state['bindings']['projection']['state'] != 'current':
                    return None
                path = base64.b64decode(state['path_bytes'], validate=True)
                result[path] = state
            if set(result) != wanted:
                return None
            if selections is not None and any(result[path]['selected'] != count
                    for path, count in selections.items()):
                return None
            return result
        return self.wait('actual ready installed browser providers', observed)

    def selection(self, provider):
        selected, generation, offset = [], None, 0
        for _ in range(8):
            fields = dict(provider=provider, scope='selection', limit=8,
                offset=offset)
            if generation is not None:
                fields['generation'] = generation
            value = self.rpc(**fields)
            assert value is not None
            if generation is not None:
                assert value['generation'] == generation
            generation = value['generation']
            selected.extend(value['items'])
            if value['next_offset'] is None:
                return selected
            offset = value['next_offset']
        raise AssertionError('installed selection pagination exceeds bound')

    def windows(self):
        result, queue, visited = {}, [(self.client.root, 0)], 0
        while queue:
            window, depth = queue.pop()
            visited += 1
            assert visited <= 4096
            hint = ClassHint()
            try:
                ok = self.errors.read(window, 20, lambda:
                    self.client.x.XGetClassHint(self.client.display, window,
                        C.byref(hint)))
                if ok and hint.name:
                    alias = C.string_at(hint.name).decode('ascii', 'replace')
                    match = re.fullmatch(r'FvwmFiles-([0-9a-f]{32})-'
                        r'([1-9][0-9]*)-ready', alias)
                    if match:
                        result.setdefault(match[1], []).append(dict(
                            window=window, alias=alias))
            finally:
                for pointer in (hint.name, hint.class_):
                    if pointer:
                        self.client.x.XFree(pointer)
            if depth < 5:
                queue.extend((child, depth + 1)
                    for child in self.view.children(window))
        return result

    def property(self, window, name):
        kind, count, remaining = C.c_ulong(), C.c_ulong(), C.c_ulong()
        fmt, pointer = C.c_int(), C.c_void_p()
        atom = self.client.x.XInternAtom(self.client.display, name.encode(), 0)
        try:
            status = self.errors.read(window, 20, lambda:
                self.client.x.XGetWindowProperty(self.client.display, window,
                    atom, 0, 8, 0, 0, C.byref(kind), C.byref(fmt),
                    C.byref(count), C.byref(remaining), C.byref(pointer)))
            if status is None:
                return None
            assert status == 0 and remaining.value == 0 and count.value <= 8
            if fmt.value == 0:
                return None
            assert fmt.value == 32
            expected = 'ATOM' if name == 'XdndAware' else name
            expected_atom = self.client.x.XInternAtom(self.client.display,
                expected.encode(), 0)
            assert kind.value == expected_atom
            values = C.cast(pointer, C.POINTER(C.c_ulong))
            return [values[index] & 0xffffffff for index in range(count.value)]
        finally:
            if pointer:
                self.client.x.XFree(pointer)

    def window(self, provider):
        def observed():
            values = self.windows().get(provider, [])
            if len(values) != 1:
                return None
            value = values[0]
            box = self.view.attributes(value['window'])
            state = self.property(value['window'], 'WM_STATE')
            return value if box and box.map_state == 2 and state and \
                state[0] == 1 else None
        return self.wait('actual ready installed Form window', observed)

    def close_provider(self, provider):
        value = self.window(provider)
        window = value['window']
        self.command('WindowId %d Raise\nWindowId %d Focus' % (window, window))
        self.client.x.XSetInputFocus(self.client.display, window, 1, 0)
        self.client.x.XSync(self.client.display, 0)
        assert self.view.focus() == window
        self.view.key('Escape')
        self.wait('provider unregister after actual Form Escape', lambda:
            provider not in {item['provider'] for item in self.providers()})
        self.wait('actual native Form window retirement', lambda:
            provider not in self.windows())
        return dict(provider=provider, **value, input='actual Form Escape',
            provider_unregistered=True, native_window_retired=True,
            browser_exit_code='not observed here')

    def close_browsers(self):
        result = []
        if self.peer is None:
            return result
        for item in list(self.providers()):
            result.append(self.close_provider(item['provider']))
        return result

    def close(self):
        for connection in (self.peer, self.control):
            if connection is not None:
                connection.close()
        if self.errors is not None:
            self.errors.close_display()
            assert not self.errors.unexpected, self.errors.unexpected
        elif self.client is not None:
            self.client.close()


def run(args):
    source, out = Path(__file__).resolve().parents[2], args.artifacts_dir
    out.mkdir(parents=True, exist_ok=False)
    evidence = dict(status='running', mode=args.mode,
        execution_profile=args.execution_profile, platform=sys.platform,
        machine=platform.machine(),
        cases={}, bus_calls=[], cleanup_failures=[], limits=dict(
            whole_installed_desktop=False, filemanager77=False,
            native_cpu='execution label only; not inferred from architecture',
            screenshots='omitted; no new UI rendering acceptance',
            protocol_theme_race_sanitizers='existing separate qualification',
            helper_image='expected installed hash/path contract; comm is not '
                'immutable-image proof',
            independent_browser_exit='not observed after service parent exits; '
                'provider/window retirement and PID absence are separate'))
    binaries = {name: getattr(args, name) for name in
        ('fvwm', 'mfl', 'form', 'browser', 'helper', 'dbus_send', 'dbus_daemon')}
    evidence['binaries'] = {name: dict(path=str(path), sha256=digest(path))
        for name, path in binaries.items()}
    evidence['config_header'] = dict(path=str(args.config_header),
        sha256=digest(args.config_header))
    evidence['installed_prefix'] = str(args.installed_prefix)
    evidence['borrowed_native_actors'] = {name: not path.is_relative_to(
        args.installed_prefix) for name, path in binaries.items()
        if name in ('fvwm', 'mfl', 'form')}
    inputs = [Path(__file__).resolve(), source / 'bin/file_manager_bus.c',
        source / 'bin/file_manager_policy.c', source / 'fvwm/ConfigFvwmDefaults',
        source / 'bin/FvwmPrompt/meson.build']
    for directory in ('cmd/fvwmfilebrowser', 'internal/filemodel',
            'internal/runtimepath'):
        inputs.extend(sorted((source / 'bin/FvwmPrompt' / directory).glob('*.go')))
    inputs.extend(Path(__file__).with_name(name) for name in (
        'filemanager_bus_live.py', 'file_transfer_browser_live.py',
        'browser_handoff_live.py',
        'native_module_proxy.py', 'live_x_harness.py', 'mfl_request_live.py',
        'mfl_stream_live.py', 'native_bindings_live.py', 'inspector_live.py',
        'browser_continuity_live.py', 'geometry_transition_live.py',
        'session_x_errors.py'))
    evidence['source_sha256'] = {str(path.relative_to(source)): digest(path)
        for path in inputs}
    runtime = HarnessRuntime(prefix='fvwm-fm-installed-')
    services, probe, core, bus = [], None, None, None
    observed_pids = set()  # Absence observations only; never signal these PIDs.
    native = runtime.root / 'native-records'
    failure = None

    def case(name, operation):
        record = dict(status='running', started=time.monotonic())
        evidence['cases'][name] = record
        try:
            record['observed'] = operation()
        except BaseException as error:
            record.update(status='failed', failure=repr(error))
            raise
        record.update(status='passed', seconds=time.monotonic() - record['started'])

    try:
        _, display = start_display_server(runtime, 'Xvfb',
            ['Xvfb', '-screen', '0', '1920x1800x24', '-noreset'],
            'xserver.log', time.monotonic() + 10)
        modules, user, folder, other, busdir = [runtime.root / name for name in
            ('modules', 'user', 'files', 'other', 'bus')]
        for path in (modules, user, folder, other, busdir, native):
            path.mkdir(mode=0o700)
        defaults = runtime.root / 'config-home/mimeapps.list'
        defaults.parent.mkdir(mode=0o700)
        defaults.write_text('[Default Applications]\n'
            'inode/directory=fixture-existing.desktop;\n')
        defaults.chmod(0o600)
        expected_defaults = defaults_identity(defaults)
        marker = runtime.root / 'must-not-execute'
        literal = folder / 'literal ;$(touch must-not-execute) `touch must-not-execute`'
        literal.write_text('#!/bin/sh\ntouch ' + shlex.quote(str(marker)) + '\n')
        literal.chmod(0o700)
        raw = folder / os.fsdecode(b'raw-\xff')
        raw.write_bytes(b'raw byte name\n')
        percent = folder / 'literal%FF,comma'
        percent.write_bytes(b'percent decoded once\n')
        plain = other / 'plain'
        plain.write_bytes(b'other lexical parent\n')
        for name, executable in (('FvwmForm', args.form), ('FvwmMFL', args.mfl)):
            wrapper = modules / name
            wrapper.write_text('#!/bin/sh\nexec ' + shlex.join([sys.executable,
                str(Path(__file__).with_name('native_module_proxy.py')),
                str(executable), str(native)]) + ' "$@" 2>>' +
                shlex.quote(str(runtime.root / (name + '.log'))) + '\n')
            wrapper.chmod(0o700)
        config = runtime.root / 'config'
        config.write_text('ModulePath ' + str(modules) +
            '\nKillModule FvwmMFL\nSchedule 1000 Module FvwmMFL\nRead ' +
            str(source / 'fvwm/ConfigFvwmDefaults') + '\n' +
            'DefaultFont fixed\nColorset 0 fg black, bg gray90\n'
            'Colorset 1 fg black, bg gray80\n'
            '*FvwmFormDefault: Font fixed\n*FvwmFormDefault: ButtonFont fixed\n'
            '*FvwmFormDefault: InputFont fixed\n*FvwmFormDefault: Fore black\n'
            '*FvwmFormDefault: Back gray90\n*FvwmFormDefault: ItemFore black\n'
            '*FvwmFormDefault: ItemBack white\n'
            'Style FvwmFiles-* PositionPlacement 20 60\n')
        shutil.copyfile(config, out / 'session.fvwm')
        evidence['configuration'] = dict(path='session.fvwm', sha256=digest(config))
        env = runtime.environment(dict(DISPLAY=display, HOME=str(runtime.root),
            FVWM_USERDIR=str(user), FVWMMFL_SOCKET_PATH=str(runtime.root),
            XDG_CONFIG_HOME=str(defaults.parent),
            XDG_DATA_HOME=str(runtime.root / 'data'),
            XDG_DATA_DIRS=str(runtime.root / 'absent')))
        for name in ('DBUS_SESSION_BUS_ADDRESS', 'DBUS_STARTER_ADDRESS',
                'DBUS_STARTER_BUS_TYPE', 'FVWM_FILEMANAGER_BUS_HELPER',
                'FVWM_FILE_TRANSFER_HELPER', 'FVWM_SESSION_BUS_HELPER',
                'FVWM_DESKTOP_ROOT', 'FVWM_DESKTOP_WORKER'):
            env.pop(name, None)
        evidence['helper_overrides_absent'] = True
        bus = runtime.start('private-bus', [str(args.dbus_daemon), '--session',
            '--nofork', '--nopidfile', '--print-address=1',
            '--address=unix:path=' + str(busdir / 'socket')], 'bus.log', env)

        def bus_address():
            value = bounded(bus.log_path).decode('ascii')
            return value.splitlines()[0] if value.startswith('unix:') else None
        address = wait_for_predicate('owned private bus address', bus_address,
            time.monotonic() + 5, process=bus).split(',guid=', 1)[0]
        env['DBUS_SESSION_BUS_ADDRESS'] = address
        evidence['bus'] = dict(address=address, pid=bus.process.pid,
            borrowed_by_service=True)
        core = runtime.start('core', [str(args.fvwm), '-d', display,
            '-f', str(config)], 'core.log', env)
        endpoint = runtime.root / ('fvwm_mfl_' + display.replace(':', '_') + '.sock')

        def actual_mfl():
            result = [json.loads(path.read_text())
                for path in native.glob('module-*.json')]
            result = [value for value in result if value['real'] == str(args.mfl)
                and not (native / ('stopped-%d.json' % value['proxy'])).exists()]
            assert len(result) <= 1
            if not result or not endpoint.exists():
                return None
            value = result[0]
            log = native / ('module-%d.jsonl' % value['proxy'])
            if not log.exists():
                return None
            # MFL emits this actual command only after binding its listener.
            records = [json.loads(line) for line in bounded(log).split(b'\n')[:-1]]
            bound = [record for record in records if record.get('direction') == 'out'
                and record['text'] == 'SetEnv FVWMMFL_SOCKET ' + str(endpoint)]
            return dict(value, bound_receipt=bound[-1]) if bound else None
        evidence['actual_mfl'] = wait_for_predicate('actual proxy-owned MFL',
            actual_mfl, time.monotonic() + 10, process=core)
        probe = Probe.__new__(Probe)
        probe.__init__(args, runtime, display, endpoint, native, core, evidence)
        client = BusClient(args.dbus_send, address, env, out, evidence)
        assert not client.has_owner() and probe.providers() == []

        def start(name, environment=None, transfer=False, ordinary=False):
            arguments = ['--socket', str(endpoint)]
            if ordinary:
                arguments += ['--dir', str(folder), '--view', 'list', '--watch',
                    'off', '--opener', '/bin/true', '--terminal', '/bin/true']
            else:
                arguments += ['--filemanager-service']
                if transfer:
                    arguments += ['--file-transfer']
            value = Service(name, args.browser, arguments,
                env if environment is None else environment, out, evidence,
                runtime.root)
            services.append(value)
            return value

        def ready(service):
            def observed():
                data = bounded(service.stdout, 1024)
                if not data.endswith(b'\n'):
                    return None
                assert len(data.splitlines()) == 1
                result = json.loads(data)
                assert result['version'] == 1 and result['kind'] == 'ready'
                assert result['owner'] == client.owner()
                return result
            return wait_for_predicate('actual installed service ownership ready',
                observed, time.monotonic() + 10, process=service)

        def identity(service, receipt):
            text = client.call('org.freedesktop.DBus.GetConnectionUnixProcessID',
                ['string:' + receipt['owner']], 'org.freedesktop.DBus',
                '/org/freedesktop/DBus', empty=False)
            values = re.findall(r'^\s*uint32 ([0-9]+)$', text, re.M)
            assert len(values) == 1
            pid = int(values[0])
            children = process_records(parent=service.process.pid)
            matches = [value for value in children if value['pid'] == pid]
            assert len(matches) == 1
            assert matches[0]['comm'] in (args.helper.name, args.helper.name[:15])
            return dict(service_pid=service.process.pid, bus_owner=receipt['owner'],
                native_bridge=matches[0], service_children=children,
                helper_exit_code='owned by Go service, not independently observed')

        def failed_start(name, environment, disabled=False):
            before = {item['provider'] for item in probe.providers()}
            windows = probe.windows()
            owner = client.owner() if client.has_owner() else None
            value = start(name, environment)
            assert value.wait(seconds=10) != 0
            assert bounded(value.stdout) == b''
            diagnostic = bounded(value.stderr, 4096).decode('utf-8', 'replace')
            assert diagnostic.strip()
            if disabled:
                assert 'optional_bus_support_disabled' in diagnostic
            assert {item['provider'] for item in probe.providers()} == before
            assert probe.windows() == windows
            assert (client.owner() if client.has_owner() else None) == owner
            assert bus.poll() is None
            return dict(wait=value.record['wait'], diagnostic=diagnostic,
                no_ready=True, owner_unchanged=True, providers_unchanged=True,
                windows_unchanged=True)

        if args.mode == 'disabled':
            case('D1-disabled-installed-service-explicit-refusal', lambda:
                failed_start('disabled-service', env, disabled=True))

            def ordinary():
                environment = dict(env)
                environment.pop('DBUS_SESSION_BUS_ADDRESS', None)
                value = start('ordinary-no-bus', environment, ordinary=True)
                state = probe.states([folder])[os.fsencode(folder)]
                window = probe.window(state['provider'])
                assert state['file_transfer'] is False
                closed = probe.close_provider(state['provider'])
                assert value.wait() == 0 and not client.has_owner()
                return dict(state=state, window=window, closed=closed,
                    actual_owned_browser_wait=value.record['wait'])
            case('D2-ordinary-installed-browser-without-bus', ordinary)
        else:
            service = start('service')
            receipt = None

            def startup():
                nonlocal receipt
                receipt = ready(service)
                return dict(ready=receipt, identity=identity(service, receipt))
            case('E1-installed-owner-and-direct-parent-identity', startup)
            first = None
            first_window = None

            def show_folder():
                nonlocal first, first_window
                client.show('ShowFolders', [uri(folder)])
                first = probe.states([folder])[os.fsencode(folder)]
                first_window = probe.window(first['provider'])
                return dict(state=first, window=first_window)
            case('E2-independent-ShowFolders-actual-provider', show_folder)

            def reuse():
                window = first_window['window']
                probe.command('WindowId %d Iconify on' % window)
                def iconified():
                    state = probe.property(window, 'WM_STATE')
                    box = probe.view.attributes(window)
                    return bool(state and state[0] == 3 and box and box.map_state != 2)
                probe.wait('actual iconified browser', iconified)
                client.show('ShowFolders', [uri(folder)])
                state = probe.states([folder])[os.fsencode(folder)]
                assert state['provider'] == first['provider']
                assert probe.window(state['provider']) == first_window
                return dict(state=state, window=first_window,
                    iconified_before_request=True, actual_deiconified=True)
            case('E3-repeat-reuses-provider-window-and-deiconifies', reuse)

            def grouped():
                targets = [literal, raw, percent, plain]
                client.show('ShowItems', [uri(path) for path in targets] +
                    [uri(raw), uri(plain)])
                result = probe.states([folder, other],
                    {os.fsencode(folder): 3, os.fsencode(other): 1})
                selections = {}
                for path, wanted in ((folder, targets[:3]), (other, targets[3:])):
                    selected = probe.selection(result[os.fsencode(path)]['provider'])
                    selections[base64.b64encode(os.fsencode(path)).decode()] = selected
                    assert {base64.b64decode(item['name_bytes'], validate=True)
                        for item in selected} == {os.fsencode(item.name)
                            for item in wanted}
                assert not marker.exists() and not (folder / marker.name).exists()
                assert result[os.fsencode(folder)]['provider'] == first['provider']
                return dict(states=list(result.values()), selections=selections,
                    exact_name_bytes=True,
                    duplicate_normalization=True, subject_not_executed=True)
            case('E4-two-parent-ShowItems-literal-and-byte-names', grouped)

            def properties():
                after = time.monotonic()
                client.show('ShowItemProperties', [uri(literal), uri(raw), uri(raw)])
                result = probe.states([folder, other], {os.fsencode(folder): 2})
                provider = result[os.fsencode(folder)]['provider']
                value = probe.rpc(provider=provider, scope='properties')['properties']
                assert value == dict(count=2, files=2, directories=0, links=0,
                    special=0, regular_file_bytes=str(literal.stat().st_size +
                        raw.stat().st_size), recursive=False, links_followed=False)
                publication = probe.wait('actual native properties publication', lambda:
                    [record for record in rows(native) if record['direction'] == 'out'
                        and record['time'] > after and
                        'Showing observed properties for 2 requested entries.'
                        in record['text']])
                return dict(properties=value, native_publication=publication)
            case('E5-ShowItemProperties-observed-native-summary', properties)
            case('E6-competing-installed-owner-refused', lambda:
                failed_start('competing', env))

            def no_bus():
                missing = dict(env)
                missing.pop('DBUS_SESSION_BUS_ADDRESS', None)
                absent = dict(env, DBUS_SESSION_BUS_ADDRESS='unix:path=' +
                    str(runtime.root / 'nonexistent-bus'))
                return dict(unset=failed_start('unset-bus', missing),
                    nonexistent=failed_start('nonexistent-bus', absent),
                    explicit_local_bus_only=True)
            case('E7-unset-and-nonexistent-bus-no-autolaunch', no_bus)

            def service_lifetime():
                before = probe.states([folder, other])
                windows = {state['provider']: probe.window(state['provider'])
                    for state in before.values()}
                def two_browser_children():
                    children = process_records(parent=service.process.pid)
                    pids = [value['pid'] for value in children if value['comm'] in
                        (args.browser.name, args.browser.name[:15])]
                    return pids if len(pids) == 2 else None
                browser_pids = probe.wait('two direct admitted browser children',
                    two_browser_children)
                observed_pids.update(browser_pids)
                bridge = identity(service, receipt)['native_bridge']['pid']
                observed_pids.add(bridge)
                assert service.stop() == 0
                probe.wait('actual name release after owned service Wait', lambda:
                    not client.has_owner(), seconds=5)
                probe.wait('old native bridge PID absence', lambda:
                    not process_records(pid=bridge), seconds=5)
                after = probe.states([folder, other])
                assert {state['provider'] for state in before.values()} == \
                    {state['provider'] for state in after.values()}
                assert {provider: probe.window(provider)
                    for provider in windows} == windows
                assert bus.poll() is None
                replacement = start('service-restarted')
                second = ready(replacement)
                assert second['owner'] != receipt['owner']
                client.show('ShowFolders', [uri(folder)])
                reused = probe.states([folder, other])
                assert reused[os.fsencode(folder)]['provider'] == first['provider']
                assert probe.window(first['provider']) == first_window
                replacement_identity = identity(replacement, second)
                assert replacement.stop() == 0
                probe.wait('replacement owner released', lambda:
                    not client.has_owner(), seconds=5)
                replacement_bridge = replacement_identity['native_bridge']['pid']
                observed_pids.add(replacement_bridge)
                probe.wait('replacement native bridge PID absence', lambda:
                    not process_records(pid=replacement_bridge), seconds=5)
                after_second_stop = probe.states([folder, other])
                assert {state['provider'] for state in after_second_stop.values()} == \
                    set(windows)
                assert {provider: probe.window(provider)
                    for provider in windows} == windows
                assert bus.poll() is None
                closed = probe.close_browsers()
                probe.wait('previously observed browser PID absence', lambda:
                    not any(process_records(pid=pid) for pid in browser_pids))
                return dict(before=list(before.values()), after=list(after.values()),
                    original_wait=service.record['wait'], bus_survived=True,
                    original_bridge_pid_absent=True, restart=second,
                    replacement_identity=replacement_identity, reuse_same_window=True,
                    replacement_wait=replacement.record['wait'],
                    after_second_stop=list(after_second_stop.values()),
                    replacement_bridge_pid_absent=True, native_closes=closed,
                    observed_browser_pids=browser_pids, browser_pids_absent=True,
                    orphan_browser_exit_status='unavailable; no PID signals sent')
            case('E8-service-Wait-name-release-browser-continuity-restart',
                service_lifetime)

            if args.check_file_transfer_propagation:
                def propagation():
                    transfer_helper = args.helper.parent / 'fvwm-file-transfer'
                    assert transfer_helper.is_file() and \
                        os.access(transfer_helper, os.X_OK)
                    assert os.fsencode(transfer_helper) in args.browser.read_bytes()
                    fresh = runtime.root / 'transfer-folder'
                    fresh.mkdir(mode=0o700)
                    value = start('service-transfer', transfer=True)
                    ready_value = ready(value)
                    client.show('ShowFolders', [uri(fresh)])
                    state = probe.states([fresh])[os.fsencode(fresh)]
                    assert state['file_transfer'] is True
                    window = probe.window(state['provider'])
                    probe.wait('actual helper frame XdndAware version5', lambda:
                        probe.property(window['window'], 'XdndAware') == [5])
                    service_identity = identity(value, ready_value)
                    browsers = [child for child in process_records(
                        parent=value.process.pid) if child['comm'] in
                        (args.browser.name, args.browser.name[:15])]
                    assert len(browsers) == 1
                    helpers = [child for child in process_records(
                        parent=browsers[0]['pid']) if child['comm'] in
                        (transfer_helper.name, transfer_helper.name[:15])]
                    assert len(helpers) == 1
                    retired_pids = [browsers[0]['pid'], helpers[0]['pid']]
                    observed_pids.update(retired_pids)
                    closed = probe.close_provider(state['provider'])
                    probe.wait('E9 browser and transfer helper PID absence', lambda:
                        not any(process_records(pid=pid) for pid in retired_pids))
                    assert value.stop() == 0
                    probe.wait('propagation owner released', lambda:
                        not client.has_owner(), seconds=5)
                    return dict(service=ready_value, state=state, window=window,
                        service_identity=service_identity,
                        direct_transfer_helper=helpers[0], helper=dict(
                            path=str(transfer_helper), sha256=digest(transfer_helper)),
                        native_awareness=5, closed=closed,
                        observed_browser_helper_pids=retired_pids,
                        browser_helper_pids_absent=True,
                        browser_helper_exit_status='unavailable; not waited here',
                        service_wait=value.record['wait'],
                        transfer_operations='not exercised')
                case('E9-file-transfer-propagation-startup-only', propagation)
        assert defaults_identity(defaults) == expected_defaults
        evidence['controlled_defaults_unchanged'] = expected_defaults
        assert not marker.exists()
        assert bus.poll() is None
        evidence['status'] = 'passed'
    except BaseException as error:
        failure = error
        evidence.update(status='failed', failure=repr(error))
    finally:
        def clean(name, operation):
            nonlocal failure
            try:
                return operation()
            except BaseException as error:
                evidence['cleanup_failures'].append(dict(component=name,
                    failure=repr(error)))
                evidence['status'] = 'failed'
                if failure is None:
                    failure = error
        def observe_live_descendants():
            result = []
            for value in services:
                if value.process.poll() is not None:
                    continue
                for child in process_records(parent=value.process.pid):
                    browser = child['comm'] in (args.browser.name,
                        args.browser.name[:15])
                    bridge = child['comm'] in (args.helper.name,
                        args.helper.name[:15])
                    if not browser and not bridge:
                        continue
                    observed_pids.add(child['pid'])
                    result.append(dict(child, service_pid=value.process.pid))
                    if browser:
                        for helper in process_records(parent=child['pid']):
                            if helper['comm'] in ('fvwm-file-transfer',
                                    'fvwm-file-transfer'[:15]):
                                observed_pids.add(helper['pid'])
                                result.append(dict(helper,
                                    service_pid=value.process.pid))
            return result
        evidence['cleanup_current_descendants'] = clean(
            'observe live owned service descendants', observe_live_descendants)
        for value in reversed(services):
            clean(value.name + ' actual Wait', value.stop)
            clean(value.name + ' streams', value.close_streams)
        if probe is not None:
            closed = clean('actual native browser closes', probe.close_browsers)
            evidence['cleanup_native_closes'] = closed
        if observed_pids:
            absent = clean('observed browser/helper PID absence', lambda:
                wait_for_predicate('observed browser/helper PID absence', lambda:
                    not any(process_records(pid=pid) for pid in observed_pids),
                    time.monotonic() + 10))
            evidence['cleanup_pid_retirement'] = dict(
                observed_pids=sorted(observed_pids), absent=absent is True,
                exit_status='unavailable; read-only absence, no orphan PID signals')
        if probe is not None:
            clean('independent observers', probe.close)
        if core is not None:
            clean('actual core Wait', lambda: core.close(time.monotonic() + 5))
        if native.exists():
            evidence['native_child_waits'] = clean('native module actual Wait receipts',
                lambda: cleanup_receipts(native))
            clean('retain native receipts', lambda: shutil.copytree(native,
                out / 'native-records'))
        if bus is not None:
            clean('owned private bus Wait', lambda: bus.close(time.monotonic() + 5))
        for process in reversed(runtime.processes):
            clean(process.name + ' Wait', lambda process=process:
                process.close(time.monotonic() + 3))
        for path in runtime.root.glob('*.log'):
            clean('retain ' + path.name, lambda path=path:
                shutil.copyfile(path, out / path.name))
        evidence['owned_process_waits'] = [dict(name=value.name,
            pid=value.process.pid, returncode=value.process.returncode)
            for value in runtime.processes]
        evidence['passed_groups'] = sum(value['status'] == 'passed'
            for value in evidence['cases'].values())
        evidence['expected_groups'] = (8 if args.mode == 'enabled' else 2) + \
            int(args.check_file_transfer_propagation)
        evidence['mode_omissions'] = ('disabled D1/D2 not selected'
            if args.mode == 'enabled' else 'enabled E1-E9 not selected')
        evidence['file_transfer_propagation'] = ('selected' if
            args.check_file_transfer_propagation else 'not selected; omitted')
        if evidence['status'] == 'passed' and evidence['passed_groups'] != \
                evidence['expected_groups']:
            failure = AssertionError('installed fixture group count mismatch')
            evidence.update(status='failed', failure=repr(failure))
        clean('private runtime cleanup', runtime.close)
        atomic_json(out / 'results.json', evidence)
    if failure is not None:
        raise failure
    print(json.dumps(dict(status='passed', mode=args.mode,
        groups=evidence['passed_groups'], artifacts=str(out))))


def main():
    parser = argparse.ArgumentParser(description=__doc__, allow_abbrev=False)
    parser.add_argument('--mode', choices=('enabled', 'disabled'), required=True)
    parser.add_argument('--installed-prefix', type=Path, required=True)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--config-header', type=Path)
    parser.add_argument('--execution-profile', required=True)
    parser.add_argument('--artifacts-dir', type=Path, required=True)
    for name in ('fvwm', 'mfl', 'form', 'browser', 'helper',
            'dbus_send', 'dbus_daemon'):
        parser.add_argument('--' + name.replace('_', '-'), type=Path, required=True)
    parser.add_argument('--check-file-transfer-propagation', action='store_true')
    args = parser.parse_args()
    if not sys.platform.startswith(('linux', 'freebsd', 'openbsd')):
        parser.error('installed service fixture supports Linux/FreeBSD/OpenBSD')
    if args.check_file_transfer_propagation and (args.mode != 'enabled' or
            not sys.platform.startswith('linux') or
            platform.machine().lower() not in ('aarch64', 'arm64')):
        parser.error('propagation is selected only for enabled Linux ARM64; '
            'this does not establish native CPU execution')
    args.installed_prefix = args.installed_prefix.resolve(strict=True)
    args.build = args.build.resolve(strict=True)
    args.config_header = (args.config_header or args.build /
        'config.h').resolve(strict=True)
    for name in ('fvwm', 'mfl', 'form', 'browser', 'helper',
            'dbus_send', 'dbus_daemon'):
        path = getattr(args, name).resolve(strict=True)
        assert path.is_file() and os.access(path, os.X_OK)
        setattr(args, name, path)
    for name in ('browser', 'helper'):
        assert getattr(args, name).is_relative_to(args.installed_prefix)
    assert args.helper.name == 'fvwm-session-bus'
    assert os.fsencode(args.helper) in args.browser.read_bytes(), \
        'installed browser lacks expected compiled absolute helper path literal'
    args.artifacts_dir = args.artifacts_dir.resolve()
    run(args)


if __name__ == '__main__':
    main()
