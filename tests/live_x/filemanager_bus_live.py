#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or any later version.
"""Opt-in Linux FileManager1 checks on an owned bus and actual nested X.

Use a libdbus-enabled build, dbus-daemon and the independent dbus-send client.
The service is the public browser CLI; the helper override names the actual
built/staged native helper. MFL queries observe real browser providers, selection
bytes and properties. Transparent Form/MFL proxies retain native publications.
No bridge frame, provider response, admission receipt or repaint is fabricated.

Default: all three material theme/font profiles and protocol/browser groups.
--groups protocol omits successful browser requests and captures explicitly.
--disabled-helper optionally qualifies a separately built dependency-free stub.
Browser groups include actual MFL and core Restart retirement, followed by fresh
requests on reconnected transport with the same borrowed-bus service owner.
Native sanitizer/race, lost/late admission faults, hard resource limits and wider
installed/platform acceptance remain separate lead-owned gates.
"""
import argparse
import base64
import hashlib
import json
import os
from pathlib import Path
import re
import shlex
import shutil
import signal
import socket
import struct
import subprocess
import sys
import time
from urllib.parse import quote_from_bytes

from browser_continuity_live import cleanup_receipts
from browser_handoff_live import child_identity, process_identity
from desktop_live import connect_client
from inspector_live import NativeView, atomic_json
from live_x_harness import HarnessRuntime, start_display_server, wait_for_predicate
from mfl_request_live import frame
from mfl_stream_live import Connection
from native_bindings_live import rows
from native_capture import NativeCapture
from native_module_proxy import module_proxy, terminate
from session_x_errors import SessionXErrors

PROFILES = ('slate-dark', 'dune-light', 'classic-large')
BUS_NAME = 'org.freedesktop.FileManager1'
BUS_PATH = '/org/freedesktop/FileManager1'


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def service_runner(directory, binary, arguments):
    """Own one actual CLI child, preserving its stdout and stderr separately."""
    root = Path(directory)
    with (root / 'stdout').open('wb') as output, \
            (root / 'stderr').open('wb') as errors:
        child = subprocess.Popen([binary, *arguments], stdout=output,
            stderr=errors)
        atomic_json(root / 'started.json', dict(pid=child.pid,
            identity=child_identity(child.pid, os.getpid()),
            argv=[binary, *arguments]))
        saved = signal.getsignal(signal.SIGTERM)

        def stopped(signum, stack):
            if child.poll() is None:
                child.terminate()

        signal.signal(signal.SIGTERM, stopped)
        try:
            returncode = child.wait()
        finally:
            signal.signal(signal.SIGTERM, signal.SIG_IGN)
            try:
                terminate(child)
                atomic_json(root / 'waited.json', dict(pid=child.pid,
                    returncode=child.returncode))
            finally:
                signal.signal(signal.SIGTERM, saved)
    return returncode if returncode >= 0 else 128 - returncode


class BusClient:
    """Independent installed client; every exact argv and reply is retained."""
    def __init__(self, executable, address, env, out, evidence):
        self.executable, self.address = executable, address
        self.env, self.out, self.evidence = env, out, evidence
        self.serial = 0

    def call(self, method, arguments=(), destination=BUS_NAME,
            path=BUS_PATH, success=True, empty=True):
        self.serial += 1
        argv = [str(self.executable), '--bus=' + self.address,
            '--type=method_call', '--print-reply', '--reply-timeout=6000',
            '--dest=' + destination, path, method, *arguments]
        record = dict(argv=argv, started=time.monotonic())
        self.evidence['bus_calls'].append(record)
        prefix = 'bus-%03d' % self.serial
        try:
            result = subprocess.run(argv, env=self.env, capture_output=True,
                timeout=8)
        except subprocess.TimeoutExpired as error:
            record.update(status='failed', error=repr(error))
            (self.out / (prefix + '.stdout')).write_bytes(error.stdout or b'')
            (self.out / (prefix + '.stderr')).write_bytes(error.stderr or b'')
            raise
        (self.out / (prefix + '.stdout')).write_bytes(result.stdout)
        (self.out / (prefix + '.stderr')).write_bytes(result.stderr)
        record.update(returncode=result.returncode,
            seconds=time.monotonic() - record['started'],
            stdout=prefix + '.stdout', stderr=prefix + '.stderr')
        assert (result.returncode == 0) == success, record
        text = result.stdout.decode('utf-8', 'strict')
        if success:
            lines = text.strip().splitlines()
            assert lines and lines[0].startswith('method return '), text
            if empty:
                assert len(lines) == 1, 'FileManager1 return must be empty: ' + text
        else:
            assert result.stderr.strip(), 'refusal must report a diagnostic'
        record['status'] = 'passed'
        return text

    def show(self, method, uris, startup='fixture', success=True):
        # dbus-send uses commas as array separators. Every filename comma is
        # percent-encoded by uri(); caller-provided protocol cases have none.
        assert all(',' not in value for value in uris)
        return self.call(BUS_NAME + '.' + method,
            ['array:string:' + ','.join(uris), 'string:' + startup],
            success=success)

    def owner(self):
        text = self.call('org.freedesktop.DBus.GetNameOwner',
            ['string:' + BUS_NAME], 'org.freedesktop.DBus',
            '/org/freedesktop/DBus', empty=False)
        values = re.findall(r'^\s*string "(:[^"]+)"$', text, re.M)
        assert len(values) == 1, text
        return values[0]

    def has_owner(self):
        text = self.call('org.freedesktop.DBus.NameHasOwner',
            ['string:' + BUS_NAME], 'org.freedesktop.DBus',
            '/org/freedesktop/DBus', empty=False)
        assert ('boolean true' in text) != ('boolean false' in text), text
        return 'boolean true' in text


def uri(path):
    return 'file://' + quote_from_bytes(os.fsencode(path), safe='/')


def run(args, profile):
    source, build = Path(__file__).resolve().parents[2], args.build.resolve()
    out = args.artifacts_dir.resolve() / profile
    out.mkdir(parents=True, exist_ok=False)
    evidence = dict(status='running', profile=profile, execution_profile=args.execution_profile,
        groups=args.groups, cases={}, bus_calls=[], screenshots={},
        cleanup_failures=[], limits={
            'native_cpu_hardware_acceptance': False,
            'lost_late_admission_and_backpressure': 'not run; separate policy/fault gate',
            'wm_mfl_restart': 'pending' if 'browser' in args.groups else 'not run; browser group omitted',
            'captures': 'not run' if 'browser' not in args.groups else 'pending',
            'disabled_build': 'not run; supply --disabled-helper'})
    inputs = ['tests/live_x/filemanager_bus_live.py',
        'tests/live_x/live_x_harness.py', 'tests/live_x/browser_handoff_live.py',
        'tests/live_x/native_module_proxy.py', 'tests/live_x/native_bindings_live.py',
        'tests/live_x/inspector_live.py', 'tests/live_x/session_x_errors.py',
        'tests/live_x/native_capture.py', 'tests/live_x/browser_continuity_live.py',
        'tests/live_x/mfl_request_live.py', 'tests/live_x/mfl_stream_live.py']
    inputs += [str(p.relative_to(source)) for directory in (
        'bin/FvwmPrompt/cmd/fvwmfilebrowser', 'bin/FvwmPrompt/internal/filemodel')
        for p in sorted((source / directory).glob('*.go'))]
    inputs += ['bin/file_manager_bus.c', 'bin/file_manager_policy.c']
    evidence['source_sha256'] = {p: digest(source / p) for p in inputs}
    binaries = {name: getattr(args, name) for name in
        ('fvwm', 'mfl', 'form', 'browser', 'helper', 'desktop', 'worker', 'capture')}
    if args.disabled_helper:
        binaries['disabled_helper'] = args.disabled_helper
    evidence['binary_inputs'] = {name: dict(path=str(path), sha256=digest(path))
        for name, path in binaries.items()}
    evidence['external_tools'] = {name: dict(path=str(getattr(args, name)),
        sha256=digest(getattr(args, name))) for name in ('dbus_send', 'dbus_daemon')}
    runtime = HarnessRuntime(prefix='fvwm-filemanager-bus-')
    peer = control = core = client = errors = view = bus = None
    services, owned_browsers = [], {}
    native = runtime.root / 'native-records'
    try:
        _, display = start_display_server(runtime, 'Xvfb',
            ['Xvfb', '-screen', '0', '1440x1200x24', '-noreset'],
            'xserver.log', time.monotonic() + 10)
        busdir = runtime.root / 'bus'
        busdir.mkdir(mode=0o700)
        bus = runtime.start('private-bus', [str(args.dbus_daemon), '--session',
            '--nofork', '--nopidfile', '--print-address=1',
            '--address=unix:path=' + str(busdir / 'socket')], 'bus.log')
        address = wait_for_predicate('owned private bus address', lambda:
            (lambda data: data.splitlines()[0] if data.startswith('unix:')
                else None)(bus.log_path.read_text()), time.monotonic() + 8,
            process=bus)
        evidence['bus_address'] = address
        address = address.split(',guid=', 1)[0]
        modules, user, folder, other = [runtime.root / name for name in
            ('modules', 'user', 'files', 'other')]
        for path in (modules, user, folder, other, native):
            path.mkdir(mode=0o700)
        fixture = Path(__file__).resolve()
        for directory in (build / 'modules').iterdir():
            executable = directory / directory.name
            if executable.is_file() and directory.name not in ('FvwmForm', 'FvwmMFL'):
                (modules / directory.name).symlink_to(executable)
        for name, executable in [('FvwmForm', args.form), ('FvwmMFL', args.mfl)]:
            wrapper = modules / name
            wrapper.write_text('#!/bin/sh\nexec ' + shlex.join([sys.executable,
                str(fixture), '--module-proxy', str(executable), str(native)]) +
                ' "$@" 2>>' + shlex.quote(str(runtime.root / (name + '.log'))) + '\n')
            wrapper.chmod(0o700)
        (modules / 'FvwmDesktop').symlink_to(args.desktop)
        assets = source / 'default-config'
        palette, appearance = ('dune', 'light') if profile == 'dune-light' else ('slate', 'dark')
        local = user / 'local.config'
        local.write_text('*FvwmDesktop: palette ' + palette +
            '\n*FvwmDesktop: appearance ' + appearance +
            '\n*FvwmDesktop: motion Off\n*FvwmDesktop: transparency Off\n')
        body = (assets / 'config').read_text().replace(
            '$[FVWM_DATADIR]/default-config', str(assets))
        if profile == 'classic-large':
            body = body.replace('Sans:size=10:antialias=True',
                'DejaVu Sans Mono:size=18:antialias=True')
            body += ('\n*FvwmFormDefault: Fore wheat\n*FvwmFormDefault: Back navy\n'
                '*FvwmFormDefault: ItemFore black\n*FvwmFormDefault: ItemBack ivory\n')
        config = runtime.root / 'config'
        # SetRCDefaults starts MFL before this ModulePath is read. Retire that
        # unproxied producer on every config read, including real Restart,
        # then let its lock/socket close before starting the observed proxy.
        config.write_text('ModulePath ' + str(modules) +
            '\nKillModule FvwmMFL\nSchedule 1000 Module FvwmMFL\nRead ' +
            str(source / 'fvwm/ConfigFvwmDefaults') + '\n' + body +
            '\nStyle FvwmFiles-* PositionPlacement 20 60\n')
        evidence['font'] = 'DejaVu Sans Mono 18' if profile == 'classic-large' else 'Sans 10'
        for name, path in [('config', config), ('local.config', local),
                ('default.config', assets / 'config'), ('desktop.fvwm', assets / 'desktop.fvwm'),
                ('palettes.json', assets / 'palettes.json')]:
            shutil.copy2(path, out / name)
            evidence.setdefault('configuration_sha256', {})[name] = digest(path)
        env = runtime.environment({'DISPLAY': display, 'FVWM_USERDIR': str(user),
            'FVWM_DESKTOP_ROOT': str(assets), 'FVWMMFL_SOCKET_PATH': str(runtime.root),
            'FVWM_DESKTOP_WORKER': str(args.worker),
            'FVWM_FILEMANAGER_BUS_HELPER': str(args.helper),
            'DBUS_SESSION_BUS_ADDRESS': address,
            'XDG_DATA_HOME': str(runtime.root / 'data'),
            'XDG_DATA_DIRS': str(runtime.root / 'absent')})
        core = runtime.start('core', [str(args.fvwm), '-d', display,
            '-f', str(config)], 'core.log', env)
        endpoint = runtime.root / ('fvwm_mfl_' + display.replace(':', '_') + '.sock')

        def current_mfl():
            records = list(native.glob('module-*.json'))
            assert len(records) <= 128, 'native module record bound exceeded'
            active = []
            for path in records:
                record = json.loads(path.read_text())
                if record['real'] != str(args.mfl):
                    continue
                identity = child_identity(record['child'], record['proxy'])
                if identity:
                    active.append(dict(record, identity=identity))
            assert len(active) <= 1, 'multiple live native MFL modules'
            return active[0] if active else None

        def connect_proxied_mfl(label, old_identity=None):
            def connected():
                actual = current_mfl()
                if actual is None or actual['identity'] == old_identity:
                    return None
                peers, credentials = [], []
                try:
                    for _ in range(2):
                        connection = Connection(endpoint)
                        peers.append(connection)
                        connection.socket.settimeout(.5)
                        pid, uid, gid = struct.unpack('3i', connection.socket.getsockopt(
                            socket.SOL_SOCKET, socket.SO_PEERCRED, struct.calcsize('3i')))
                        assert pid == actual['child'], 'endpoint belongs to a different MFL'
                        credentials.append(dict(pid=pid, uid=uid, gid=gid))
                        assert 'connection_profile' in connection.record()
                    peers[1].send(b'set echo\n')
                    peers[1].echo(label + '-transport-ready')
                    assert child_identity(actual['child'], actual['proxy']) == actual['identity']
                    for connection in peers:
                        connection.socket.settimeout(5)
                    return peers[0], peers[1], dict(actual, peer_credentials=credentials)
                except (OSError, AssertionError):
                    for connection in peers:
                        connection.close()
                    return None
            return wait_for_predicate(label + ' actual proxied MFL and core handshake',
                connected, time.monotonic() + 12, process=core)

        peer, control, initial_mfl = connect_proxied_mfl('filemanager-initial')
        evidence['initial_proxied_mfl'] = initial_mfl
        serial = 0

        def rpc(**fields):
            nonlocal serial
            serial += 1
            token = 'filemanager-%d' % serial
            peer.send(frame(token, resource='files', **fields))
            value = peer.record()['reply']
            assert value['id'] == token, value
            return value

        def providers():
            value = rpc()
            assert value['status'] == 'ok', value
            return value['result']['providers']

        def theme():
            control.send(b'SendToModule FvwmDesktop query-settings filemanager all\n')
            values = {}
            for _ in range(128):
                text = control.record().get('echo', {}).get('message', '')
                if text == 'desktop-settings-filemanager done':
                    return values if all(values.get(k, {}).get('effective') == v
                        for k, v in [('palette', palette), ('appearance', appearance),
                                     ('transparency', False)]) else None
                if text.startswith('desktop-settings-filemanager '):
                    _, key, value = text.split(' ', 2)
                    values[key] = json.loads(value)
            raise AssertionError('theme response bound exceeded')

        evidence['theme_settings'] = wait_for_predicate('actual configured theme',
            theme, time.monotonic() + 15, process=core)
        busclient = BusClient(args.dbus_send, address, env, out, evidence)
        assert providers() == []
        assert not busclient.has_owner()

        def start_service(name, environment=env):
            directory = runtime.root / name
            directory.mkdir(mode=0o700)
            process = runtime.start(name, [sys.executable, str(fixture),
                '--service-run', str(directory), str(args.browser),
                '--filemanager-service', '--socket', str(endpoint)],
                name + '.log', environment)
            services.append((process, directory))
            wait_for_predicate(name + ' actual CLI start',
                (directory / 'started.json').exists, time.monotonic() + 5,
                process=process)
            return process, directory

        def failed_start(name, environment):
            process, directory = start_service(name, environment)
            result = process.wait_until_exit(time.monotonic() + 8)
            assert result != 0, name + ' unexpectedly succeeded'
            assert (directory / 'stdout').read_bytes() == b''
            diagnostic = (directory / 'stderr').read_text()
            assert diagnostic.strip(), name + ' lacked a diagnostic'
            evidence['cases'][name] = dict(returncode=result, diagnostic=diagnostic)

        missing = dict(env)
        missing.pop('DBUS_SESSION_BUS_ADDRESS', None)
        failed_start('no-bus', missing)
        assert not busclient.has_owner() and providers() == []
        if args.disabled_helper:
            disabled = dict(env, FVWM_FILEMANAGER_BUS_HELPER=str(args.disabled_helper))
            failed_start('disabled-helper', disabled)
            assert not busclient.has_owner() and providers() == []
            evidence['limits']['disabled_build'] = 'passed actual stub startup refusal'
        service, service_dir = start_service('service')

        def ready():
            data = (service_dir / 'stdout').read_bytes()
            assert len(data) <= 1024, 'unbounded CLI stdout'
            if not data.endswith(b'\n'):
                return None
            lines = data.splitlines()
            assert len(lines) == 1, 'service stdout must contain one ready JSON only'
            value = json.loads(lines[0])
            assert value['version'] == 1 and value['kind'] == 'ready'
            return value

        receipt = wait_for_predicate('public CLI confirmed native owner', ready,
            time.monotonic() + 8, process=service)
        assert receipt['owner'] == busclient.owner()
        evidence['cases']['actual_owner_ready'] = receipt
        failed_start('competing-owner', env)
        assert busclient.owner() == receipt['owner'] and providers() == []
        if 'protocol' in args.groups:
            for method, arguments in [
                    (BUS_NAME + '.ShowFolders', ['string:wrong-signature']),
                    (BUS_NAME + '.ShowItems', ['array:string:file:///tmp', 'uint32:3']),
                    (BUS_NAME + '.ShowItemProperties', ['array:string:file:///tmp']),
                    (BUS_NAME + '.Unknown', ['array:string:file:///tmp', 'string:'])]:
                busclient.call(method, arguments, success=False)
            invalid = [('remote', ['file://remote.invalid/tmp']),
                ('plain-path', [str(folder)]), ('dot', [uri(folder) + '/../other']),
                ('bad-percent', ['file:///tmp/%GG']), ('root-reveal', ['file:///']),
                ('mixed', [uri(folder / 'good'), 'https://example.invalid/a']),
                ('nine-uris', [uri(folder / str(i)) for i in range(9)]),
                ('uri-byte-limit', ['file:///tmp/' + 'a' * 16384]), ('empty', [])]
            for name, targets in invalid:
                busclient.show('ShowItems', targets, success=False)
                assert providers() == [], name + ' started UI'
            busclient.show('ShowFolders', [uri(folder)], startup='a' * 513,
                success=False)
            assert providers() == []
            for interface, method in [('org.freedesktop.DBus.Introspectable', 'Introspect'),
                    ('org.freedesktop.DBus.Peer', 'Ping')]:
                text = busclient.call(interface + '.' + method,
                    empty=method == 'Ping')
                if method == 'Introspect':
                    assert all(name in text for name in
                        ('ShowFolders', 'ShowItems', 'ShowItemProperties'))
                    assert len(text) < 32768
            evidence['cases']['invalid_requests_no_ui'] = dict(cases=[n for n, _ in invalid],
                signatures=3, startup_id_limit=True, introspection_peer=True)

        def remember_children():
            if service.poll() is not None:
                return
            started = json.loads((service_dir / 'started.json').read_text())
            parent = started['pid']
            identity = process_identity(parent)
            assert identity is not None and identity['starttime'] == started['identity']['starttime']
            tasks = list(Path('/proc/%d/task' % parent).iterdir())
            assert len(tasks) <= 256
            children = set()
            for task in tasks:
                try:
                    text = (task / 'children').read_text()
                except FileNotFoundError:
                    continue
                assert len(text) <= 8192
                children.update(map(int, text.split()))
            assert len(children) <= 64
            for pid in children:
                value = child_identity(pid, parent)
                if value and base64.b64encode(b'--_filemanager-child').decode() in value['argv_bytes']:
                    owned_browsers[pid] = value

        if 'browser' in args.groups:
            marker = runtime.root / 'must-not-execute'
            literal = folder / ('literal " percent% ;$(touch ' +
                marker.name + ') `touch ' + marker.name + '`')
            literal.write_text('#!/bin/sh\ntouch ' + shlex.quote(str(marker)) + '\n')
            literal.chmod(0o700)
            raw = os.fsencode(folder) + b'/raw-\xff'
            fd = os.open(raw, os.O_CREAT | os.O_WRONLY, 0o600)
            os.close(fd)
            percent = folder / 'percent%FF'
            percent.write_text('percent is decoded once\n')
            normal = folder / 'normal'
            normal.write_text('fixture\n')
            for name in ('one', 'two'):
                (other / name).write_text(name)
            client = connect_client(display)
            errors = SessionXErrors(client, evidence).install()
            view = NativeView(client, errors, core)

            def states(count):
                listed = providers()
                if len(listed) != count:
                    return None
                result = {}
                for item in listed:
                    value = rpc(provider=item['provider'], scope='state')
                    if value['status'] != 'ok':
                        assert value['error']['code'] == 'not_ready', value
                        return None
                    state = value['result']
                    if state['busy'] or not state['availability']['ready']:
                        return None
                    result[base64.b64decode(state['path_bytes'])] = state
                return result if len(result) == count else None

            def wait_states(count, selected=None):
                remember_children()
                def observed():
                    result = states(count)
                    if result and (selected is None or all(
                            result.get(path, {}).get('selected') == value
                            for path, value in selected.items())):
                        return result
                    return None
                result = wait_for_predicate('actual ready browser providers',
                    observed, time.monotonic() + 10, process=core)
                remember_children()
                return result

            def capture(name):
                window = view.wait('File browser')
                evidence.setdefault('native_pixels', {})[name] = view.pixels(window)
                result = subprocess.run([sys.executable, str(fixture), '--capture',
                    str(args.capture), str(out)], env=env, capture_output=True,
                    timeout=40, check=True)
                path = Path(json.loads(result.stdout))
                path.replace(out / name)
                evidence['screenshots'][name] = digest(out / name)

            busclient.show('ShowFolders', [uri(folder), uri(folder) + '//'])
            first = wait_states(1)[os.fsencode(folder)]
            window = view.wait('File browser')
            capture('filemanager-folder.png')
            busclient.show('ShowFolders', [uri(folder)])
            reused = wait_states(1)[os.fsencode(folder)]
            assert reused['provider'] == first['provider']
            assert view.wait('File browser') == window
            evidence['cases']['folder_new_and_reuse'] = dict(initial=first,
                reused=reused, same_window=window, normalized_duplicates=True)
            targets = [literal, raw, percent, normal, other / 'one', other / 'two']
            uris = [uri(p) for p in targets] + [uri(normal), uri(other / 'one')]
            evidence['uri_inputs'] = uris
            evidence['target_bytes'] = [base64.b64encode(os.fsencode(p)).decode() for p in targets]
            busclient.show('ShowItems', uris)
            grouped = wait_states(2, {os.fsencode(folder): 4, os.fsencode(other): 2})
            assert set(grouped) == {os.fsencode(folder), os.fsencode(other)}
            for path, wanted in [(folder, targets[:4]), (other, targets[4:])]:
                state = grouped[os.fsencode(path)]
                selected = rpc(provider=state['provider'], scope='selection')['result']['items']
                assert {base64.b64decode(i['name_bytes']) for i in selected} == {
                    os.fsencode(p).rsplit(b'/', 1)[-1] for p in wanted}, selected
                assert state['selected'] == len(wanted)
                evidence['cases'].setdefault('grouped_reveal', []).append(
                    dict(state=state, selection=selected))
            assert grouped[os.fsencode(folder)]['provider'] == first['provider']
            capture('filemanager-grouped-reveal.png')
            before = max((r['time'] for r in rows(native)), default=0)
            busclient.show('ShowItemProperties', [uri(literal), uri(raw), uri(literal)])
            properties_state = wait_states(2, {os.fsencode(folder): 2})[os.fsencode(folder)]
            provider = properties_state['provider']
            assert provider == first['provider'] and properties_state['selected'] == 2
            properties = rpc(provider=provider, scope='properties')['result']['properties']
            assert properties['count'] == 2 and properties['files'] == 2
            assert not properties['links_followed'] and not properties['recursive']
            published = wait_for_predicate('actual native properties publication', lambda:
                [r for r in rows(native) if r['time'] > before and
                    'Showing observed properties for 2 requested entries.' in r['text']],
                time.monotonic() + 6, process=core)
            capture('filemanager-properties.png')
            evidence['cases']['properties_inspector'] = dict(state=properties_state,
                properties=properties, native_publication=published)
            assert len(providers()) == 2
            assert not marker.exists() and not (folder / marker.name).exists()
            assert not (Path.cwd() / marker.name).exists()
            evidence['cases']['literal_bytes_no_execution'] = True
            evidence['limits']['captures'] = 'captured; separate visual inspection required'

            def live_browsers_now():
                remember_children()
                live = []
                for pid, identity in owned_browsers.items():
                    current = process_identity(pid)
                    if current and current['starttime'] == identity['starttime']:
                        value = child_identity(pid, current['parent'])
                        if value and value['argv_bytes'] == identity['argv_bytes']:
                            live.append(identity)
                return live

            def two_live_browsers():
                live = live_browsers_now()
                return live if len(live) == 2 else None

            def restart_phase(name, core_restart):
                nonlocal peer, control
                case = dict(status='running', automatic_browser_recovery=False)
                evidence['cases'][name] = case
                before_states = wait_states(2)
                old_browsers = wait_for_predicate(name + ' exact old browsers',
                    two_live_browsers, time.monotonic() + 5, process=core)
                old_mfl = wait_for_predicate(name + ' actual old MFL',
                    current_mfl, time.monotonic() + 5, process=core)
                old_core = process_identity(core.process.pid)
                assert old_core is not None
                case.update(providers_before=list(before_states.values()),
                    browsers_before=old_browsers, mfl_before=old_mfl,
                    core_before=old_core, owner_before=busclient.owner())
                assert case['owner_before'] == receipt['owner']
                control.echo(name + '-before')
                if core_restart:
                    # Keep the inherited configuration/StartFunction intact;
                    # the real same-process exec Restart starts its new MFL.
                    control.send(b'Restart\n')
                else:
                    # Schedule through the still-live core before retiring its
                    # producer. No synthetic socket EOF or replacement broker.
                    control.send(b'Schedule 1000 Module FvwmMFL\nKillModule FvwmMFL\n')
                peer.expect_closed()
                control.expect_closed()
                peer.close()
                control.close()
                peer = control = None
                case['old_observers_eof'] = True
                waited_path = native / ('stopped-%d.json' % old_mfl['proxy'])
                waited = wait_for_predicate(name + ' old native MFL Wait', lambda:
                    json.loads(waited_path.read_text()) if waited_path.exists() else None,
                    time.monotonic() + 5, process=core)
                assert waited['child'] == old_mfl['child'] and waited['returncode'] is not None
                case['old_mfl_wait'] = waited

                def retired_browsers():
                    remaining = [process_identity(item['child']) for item in old_browsers]
                    if any(now and now['starttime'] == item['starttime']
                            for item, now in zip(old_browsers, remaining)):
                        return None
                    return dict(exact_old_processes_retired=True,
                        remaining_identities=remaining,
                        exit_status='unavailable; actual Wait belongs to the service')
                case['old_browser_retirement'] = wait_for_predicate(
                    name + ' exact old browser retirement', retired_browsers,
                    time.monotonic() + 8, process=core)
                assert service.poll() is None and bus.poll() is None
                assert ready() == receipt and busclient.owner() == receipt['owner']
                peer, control, new_mfl = connect_proxied_mfl(name, old_mfl['identity'])
                after_core = process_identity(core.process.pid)
                assert after_core == old_core, (old_core, after_core)
                case.update(core_after=after_core, mfl_after=new_mfl,
                    owner_after=busclient.owner(), new_transport_ready=True)
                assert case['owner_after'] == receipt['owner']
                # Observe the new registry before making any browser request.
                # Old requests must not repopulate it or launch replacement UI.
                observations = []
                for index in range(3):
                    control.echo('%s-no-replay-%d' % (name, index))
                    listed, live = providers(), live_browsers_now()
                    assert listed == [] and live == [], (listed, live)
                    observations.append(dict(providers=listed,
                        browser_identities=live, time=time.monotonic()))
                case['no_replay_before_explicit_request'] = observations
                fresh = [runtime.root / (name + '-folder-' + str(i)) for i in range(2)]
                for path in fresh:
                    path.mkdir(mode=0o700)
                    (path / 'new-entry').write_text(name + '\n')
                case['fresh_request_after_ready'] = dict(time=time.monotonic(),
                    uris=[uri(path) for path in fresh])
                busclient.show('ShowFolders', case['fresh_request_after_ready']['uris'])
                after = wait_states(2)
                assert set(after) == {os.fsencode(path) for path in fresh}
                assert {value['provider'] for value in after.values()}.isdisjoint(
                    value['provider'] for value in before_states.values())
                case.update(providers_after=list(after.values()),
                    browsers_after=wait_for_predicate(name + ' fresh independent browsers',
                        two_live_browsers, time.monotonic() + 5, process=core),
                    status='passed')
                assert busclient.owner() == receipt['owner'] and ready() == receipt
                return after

            grouped = restart_phase('mfl_restart', False)
            grouped = restart_phase('core_restart', True)
            evidence['limits']['wm_mfl_restart'] = (
                'passed actual transport/provider retirement and explicit fresh requests; '
                'same borrowed-bus service owner; no automatic browser recovery claim')
            live_browsers = wait_for_predicate('two independent live browser children',
                two_live_browsers, time.monotonic() + 5, process=core)
            service.close(time.monotonic() + 4)
            assert not busclient.has_owner() and bus.poll() is None
            after = wait_states(2)
            assert {s['provider'] for s in after.values()} == {s['provider'] for s in grouped.values()}
            assert view.named('File browser')
            evidence['cases']['service_exit_preserves_browser_and_borrowed_bus'] = dict(
                providers=list(after.values()),
                browser_identities=live_browsers, bus_alive=True)
        else:
            service.close(time.monotonic() + 4)
            assert not busclient.has_owner() and providers() == [] and bus.poll() is None
        assert ready() == receipt
        evidence['status'] = 'passed'
    except BaseException as failure:
        evidence.update(status='failed', failure=repr(failure))
        raise
    finally:
        def clean(label, operation):
            try:
                operation()
            except BaseException as failure:
                evidence['status'] = 'failed'
                evidence['cleanup_failures'].append(dict(step=label, error=repr(failure)))

        if 'remember_children' in locals():
            clean('record still owned browser children', remember_children)
        # Independent browsers deliberately leave the service's process group.
        # Signal only observed exact PID/starttime/argv identities, never a name
        # search, broad process group or another harness's provider.
        for pid, identity in owned_browsers.items():
            def stop_browser(pid=pid, identity=identity):
                current = process_identity(pid)
                if current is None:
                    return
                assert current['starttime'] == identity['starttime'], 'browser PID reused'
                try:
                    fields = Path('/proc/%d/stat' % pid).read_text().rsplit(')', 1)[1].split()
                except FileNotFoundError:
                    return
                if fields[0] == 'Z':
                    evidence.setdefault('browser_cleanup', []).append(dict(pid=pid,
                        remaining_identity=current, zombie=True))
                    return
                value = child_identity(pid, current['parent'])
                assert value and value['argv_bytes'] == identity['argv_bytes']
                try:
                    os.kill(pid, signal.SIGTERM)
                except ProcessLookupError:
                    return
                def retired():
                    now = process_identity(pid)
                    if now is None or now['starttime'] != identity['starttime']:
                        return True
                    # An orphan may await the guest init's reap. Record this
                    # separately; a zombie has closed its X/MFL descriptors.
                    try:
                        fields = Path('/proc/%d/stat' % pid).read_text().rsplit(')', 1)[1].split()
                    except FileNotFoundError:
                        return True
                    return fields[0] == 'Z'
                wait_for_predicate('exact independent browser exit', retired,
                    time.monotonic() + 5)
                evidence.setdefault('browser_cleanup', []).append(dict(pid=pid,
                    remaining_identity=process_identity(pid)))
            clean('independent browser %d' % pid, stop_browser)
        for connection in (peer, control):
            if connection:
                clean('MFL observer', connection.close)
        if core:
            clean('core', lambda: core.close(time.monotonic() + 4))
        if errors:
            clean('X observer', errors.close_display)
        elif client:
            clean('X client', client.close)
        for process in reversed(runtime.processes):
            clean('stop ' + process.name, lambda p=process: p.close(time.monotonic() + 4))
        evidence['owned_processes'] = [dict(name=p.name, argv=p.argv,
            pid=p.process.pid, returncode=p.poll()) for p in runtime.processes]
        if native.exists():
            clean('native module actual Wait receipts', lambda:
                evidence.update(module_cleanup=cleanup_receipts(native)))
        for path in runtime.root.iterdir():
            if path.is_dir() and (path == native or any(path == d for _, d in services)):
                clean('copy ' + path.name, lambda p=path: shutil.copytree(p, out / p.name))
            elif path.is_file() and path.suffix == '.log':
                clean('copy ' + path.name, lambda p=path: shutil.copy2(p, out / p.name))
        if evidence.get('observer_unexpected_x_errors'):
            evidence['status'] = 'failed'
        clean('runtime', runtime.close)
        atomic_json(out / 'result.json', evidence)
    assert evidence['status'] == 'passed', evidence
    print(json.dumps(dict(status='passed', profile=profile, artifacts=str(out))))


def main():
    if len(sys.argv) > 1 and sys.argv[1] == '--module-proxy':
        return module_proxy(sys.argv[2], sys.argv[3], sys.argv[4:])
    if len(sys.argv) > 1 and sys.argv[1] == '--service-run':
        return service_runner(sys.argv[2], sys.argv[3], sys.argv[4:])
    if len(sys.argv) == 4 and sys.argv[1] == '--capture':
        print(json.dumps(NativeCapture(sys.argv[2]).screenshot(
            sys.argv[3]).removeprefix('Screenshot saved: ')))
        return 0
    parser = argparse.ArgumentParser(description=__doc__, allow_abbrev=False)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--artifacts-dir', type=Path, required=True)
    parser.add_argument('--execution-profile', required=True,
        choices=('native-linux', 'emulated-linux'), help='record actual execution provenance')
    parser.add_argument('--profiles', nargs='+', choices=PROFILES, default=list(PROFILES))
    parser.add_argument('--groups', nargs='+', choices=('protocol', 'browser'),
        default=['protocol', 'browser'])
    for name in ('fvwm', 'mfl', 'form', 'browser', 'helper', 'desktop', 'worker', 'capture',
            'disabled-helper', 'dbus-send', 'dbus-daemon'):
        parser.add_argument('--' + name, type=Path)
    args = parser.parse_args()
    if sys.platform != 'linux':
        parser.error('explicit opt-in fixture runs only in the owning Linux guest')
    defaults = dict(fvwm='fvwm3', mfl='modules/FvwmMFL/FvwmMFL',
        form='modules/FvwmForm/FvwmForm', browser='bin/FvwmPrompt/fvwm-file-browser',
        helper='bin/fvwm-session-bus', desktop='bin/FvwmPrompt/FvwmDesktop',
        worker='bin/FvwmPrompt/fvwm-desktop-worker', capture='bin/fvwm-desktop-capture')
    for name, relative in defaults.items():
        setattr(args, name, (getattr(args, name) or args.build / relative).resolve())
    for name, executable in [('dbus_send', 'dbus-send'), ('dbus_daemon', 'dbus-daemon')]:
        value = getattr(args, name) or shutil.which(executable)
        if not value:
            parser.error('unavailable actual independent tool: ' + executable)
        setattr(args, name, Path(value).resolve())
    if args.disabled_helper:
        args.disabled_helper = args.disabled_helper.resolve()
    for name in (*defaults, 'dbus_send', 'dbus_daemon', 'disabled_helper'):
        path = getattr(args, name)
        if path and (not path.is_file() or not os.access(path, os.X_OK)):
            parser.error('unavailable executable for ' + name + ': ' + str(path))
    args.artifacts_dir.resolve().mkdir(parents=True, exist_ok=True)
    for profile in args.profiles:
        run(args, profile)
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
