#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
"""Observe fixed native menu/browser bindings, pressure and owned lifetimes.

Private proxies relay original native bytes and record real commands/packets.
Worker gates delay only fixture-owned work for at most 1.5 seconds, forward real
worker requests unchanged, or fail exactly one request. No response is invented.
Three-second quiet samples measure binding activity, not kernel wakeups: the
desktop's existing 250 ms poll and five-second status sampler remain explicit.
"""
import argparse
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
import subprocess
import sys
import tempfile
import time

from desktop_live import connect_client
from inspector_live import NativeView, atomic_json
from live_x_harness import HarnessRuntime, start_display_server, wait_for_predicate
from mfl_request_live import frame
from mfl_stream_live import Connection
from native_capture import NativeCapture
from selection_live import Event
from session_x_errors import SessionXErrors

PROFILES = ('slate-dark', 'dune-light', 'classic-large')
WORD = struct.calcsize('@L')
COUNTERS = ('invalidations', 'started', 'published', 'rejected')
MAX_RECORD_BYTES = 8 * 1024 * 1024


def terminate(child):
    if child.poll() is None:
        child.terminate()
        try:
            child.wait(timeout=.7)
        except subprocess.TimeoutExpired:
            child.kill()
            child.wait(timeout=.7)


def module_proxy(real, directory, arguments):
    """Relay both pipes verbatim; own/reap the one real native module child."""
    outgoing, incoming = map(int, arguments[:2])
    from_child, child_out = os.pipe()
    child_in, to_child = os.pipe()
    child = subprocess.Popen([real, str(child_out), str(child_in), *arguments[2:]],
        pass_fds=(child_out, child_in))
    os.close(child_out)
    os.close(child_in)
    root = Path(directory)
    token = str(os.getpid())
    atomic_json(root / ('module-' + token + '.json'),
        {'proxy': os.getpid(), 'child': child.pid, 'real': real,
         'arguments': arguments[2:], 'started': time.monotonic()})
    saved = signal.getsignal(signal.SIGTERM)

    def stopped(signum, stack):
        raise KeyboardInterrupt('owned module proxy stopped')

    signal.signal(signal.SIGTERM, stopped)
    # Each direction has a fixed 1 MiB relay/parser cap. Never reorder, modify,
    # replay or generate module commands, including during shutdown.
    channels = {from_child: [outgoing, bytearray(), bytearray(), 'out', False],
                incoming: [to_child, bytearray(), bytearray(), 'in', False]}
    recorded = 0
    try:
        for fd in (from_child, incoming, outgoing, to_child):
            os.set_blocking(fd, False)
        with (root / ('module-' + token + '.jsonl')).open('w', buffering=1) as log:
            while channels:
                readers = [fd for fd, (_, pending, _, _, eof) in channels.items()
                           if not eof and len(pending) < 1024 * 1024 - 65536]
                writers = [v[0] for v in channels.values() if v[1]]
                ready, writable, _ = select.select(readers, writers, [], .05)
                for fd in ready:
                    target, pending, parsing, direction, _ = channels[fd]
                    data = os.read(fd, 65536)
                    if not data:
                        channels[fd][4] = True
                        continue
                    pending.extend(data)
                    parsing.extend(data)
                    assert len(parsing) <= 1024 * 1024
                    while True:
                        if direction == 'out':
                            if len(parsing) < 2 * WORD:
                                break
                            _, size = struct.unpack('@LL', parsing[:2 * WORD])
                            assert size <= 4096
                            length = 3 * WORD + size
                            if len(parsing) < length:
                                break
                            assert struct.unpack('@L', parsing[length-WORD:length])[0] == 1
                            text = bytes(parsing[2 * WORD:2 * WORD + size])
                            kind = None
                        else:
                            if len(parsing) < 4 * WORD:
                                break
                            start, kind, words, _ = struct.unpack('@LLLL', parsing[:4 * WORD])
                            assert start == 0xffffffff and 4 <= words <= 4096
                            length = words * WORD
                            if len(parsing) < length:
                                break
                            text = (bytes(parsing[7 * WORD:length]).split(b'\0', 1)[0]
                                    if kind in (1 << 18, 1 << 22) else None)
                        del parsing[:length]
                        if text is not None:
                            row = json.dumps({'time': time.monotonic(), 'direction': direction,
                                'kind': kind, 'text': text.decode('utf-8', 'replace')}) + '\n'
                            recorded += len(row.encode())
                            assert recorded <= MAX_RECORD_BYTES
                            log.write(row)
                for target in writable:
                    pending = next(v[1] for v in channels.values() if v[0] == target)
                    del pending[:os.write(target, pending)]
                # Preserve the final partial write before propagating EOF.
                # Closing a relay endpoint releases the opposite reader too.
                for fd, values in list(channels.items()):
                    if values[4] and not values[1]:
                        os.close(values[0])
                        os.close(fd)
                        del channels[fd]
            return 0
    except (KeyboardInterrupt, BrokenPipeError):
        return 0
    finally:
        # KillModule can begin teardown before the fixture stops the core's
        # whole process group. A repeated TERM must not interrupt the real
        # child Wait or its receipt after ownership has entered this unwind.
        signal.signal(signal.SIGTERM, signal.SIG_IGN)
        try:
            terminate(child)
            for fd in (outgoing, incoming, from_child, to_child):
                try:
                    os.close(fd)
                except OSError:
                    pass
            atomic_json(root / ('stopped-' + token + '.json'),
                {'child': child.pid, 'returncode': child.returncode})
        finally:
            signal.signal(signal.SIGTERM, saved)


def worker_proxy(real, directory, browser=False):
    """Gate original bytes, then execute the real isolated filesystem worker."""
    root = Path(directory)
    limit = 4 * 1024 * 1024 if browser else 65536
    data = sys.stdin.buffer.read(limit + 1)
    assert len(data) <= limit
    request = json.loads(data)
    operation = request['operation']
    token = '%d-%d' % (time.monotonic_ns(), os.getpid())
    record = {'request': request, 'pid': os.getpid(), 'started': time.monotonic()}
    claim = root / ('hold-' + operation)
    claimed = root / ('claimed-' + token)
    try:
        claim.rename(claimed)
    except FileNotFoundError:
        pass
    else:
        atomic_json(root / ('held-' + operation + '.json'),
            {'token': token, 'pid': os.getpid(), 'started': record['started']})
        deadline = time.monotonic() + 1.5
        while not (root / ('release-' + token)).exists():
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                atomic_json(root / (token + '.json'), dict(record, gate_timeout=True))
                return 3
            select.select([], [], [], min(.005, remaining))
        record['held'] = True
    try:
        (root / ('fail-' + operation)).unlink()
    except FileNotFoundError:
        pass
    else:
        atomic_json(root / (token + '.json'), dict(record, injected_failure=True))
        return 4
    if browser:
        # exec leaves no child to orphan if the browser cancels this worker.
        # Recreate stdin from exactly the consumed bounded request bytes.
        atomic_json(root / (token + '.json'), dict(record, forwarded=True))
        with tempfile.TemporaryFile() as original:
            original.write(data)
            original.seek(0)
            os.dup2(original.fileno(), 0)
            os.execv(real, [real, '--worker'])
    result = subprocess.run([real], input=data, capture_output=True, timeout=3.8)
    assert len(result.stdout) <= 1024 * 1024
    record.update(returncode=result.returncode, finished=time.monotonic())
    try:
        record['response'] = json.loads(result.stdout)
    except (ValueError, UnicodeError):
        record['malformed_response'] = True
    atomic_json(root / (token + '.json'), record)
    sys.stdout.buffer.write(result.stdout)
    return result.returncode


def rows(directory):
    result = []
    for path in sorted(directory.glob('module-*.jsonl')):
        assert path.stat().st_size <= MAX_RECORD_BYTES
        data = path.read_text()
        result.extend(json.loads(line) for line in data.split('\n')[:-1])
    return sorted(result, key=lambda row: row['time'])


def menu_rows(directory, name):
    commands = []
    for row in rows(directory):
        if row['direction'] != 'out':
            continue
        text = row['text']
        if text == 'DestroyMenu recreate ' + name:
            commands = []
        elif text.startswith('AddToMenu ' + name + ' '):
            commands.append(text)
    return commands


def process_sample(pid):
    try:
        fields = Path('/proc/%d/stat' % pid).read_text().rsplit(')', 1)[1].split()
        status = Path('/proc/%d/status' % pid).read_text().splitlines()
        return {'pid': pid, 'cpu_ticks': int(fields[11]) + int(fields[12]),
                'rss': next((line for line in status if line.startswith('VmRSS:')), None)}
    except (FileNotFoundError, ProcessLookupError):
        return {'pid': pid, 'exited': True}


def run(args, profile):
    source, build = Path(__file__).resolve().parents[2], args.build.resolve()
    out = args.artifacts_dir.resolve() / profile
    out.mkdir()
    evidence = {'status': 'running', 'profile': profile, 'screenshots': [],
        'idle_limits': {'quiet_seconds': 3, 'desktop_poll_ms': 250,
                        'desktop_status_sampler_seconds': 5,
                        'kernel_wakeups_measured': False}}
    started = time.monotonic()
    with HarnessRuntime(prefix='fvwm-native-bindings-') as runtime:
        connections, client, errors, core, app = [], None, None, None, None
        try:
            _, display = start_display_server(runtime, 'Xvfb',
                ['Xvfb', '-screen', '0', '1440x1200x24', '-noreset'],
                'xserver.log', time.monotonic() + 10)
            modules, user = runtime.root / 'modules', runtime.root / 'user'
            native, desktop_jobs, browser_jobs = [runtime.root / name for name in
                ('native-records', 'desktop-workers', 'browser-workers')]
            for directory in (modules, user, native, desktop_jobs, browser_jobs):
                directory.mkdir(mode=0o700)
            for directory in (build / 'modules').iterdir():
                executable = directory / directory.name
                if executable.is_file() and directory.name != 'FvwmForm':
                    (modules / directory.name).symlink_to(executable)
            fixture = str(Path(__file__).resolve())

            def script(path, arguments, suffix=''):
                path.write_text('#!/bin/sh\nexec ' + shlex.join([sys.executable,
                    fixture, *map(str, arguments)]) + suffix + '\n')
                path.chmod(0o700)

            for name, binary in [('FvwmDesktop', build / 'bin/FvwmPrompt/FvwmDesktop'),
                                 ('FvwmForm', build / 'modules/FvwmForm/FvwmForm')]:
                script(modules / name, ['--module-proxy', binary, native],
                    ' "$@" 2>>' + shlex.quote(str(runtime.root / (name + '.log'))))
            worker = runtime.root / 'desktop-worker'
            script(worker, ['--desktop-worker', build / 'bin/FvwmPrompt/fvwm-desktop-worker', desktop_jobs])
            files = runtime.root / 'files'
            files.mkdir()
            for i in range(8):
                (files / ('entry-%02d' % i)).write_text('fixture\n')
            marker = runtime.root / 'must-not-execute'
            apps = runtime.root / 'data/applications'
            apps.mkdir(parents=True)
            application = apps / 'binding.desktop'
            literal = 'Binding ; $[exec.touch] 100% `literal`'
            application.write_text('[Desktop Entry]\nType=Application\nName=' + literal +
                '\nExec=/usr/bin/touch ' + str(marker) + '\n')
            (apps / 'peer.desktop').write_text('[Desktop Entry]\nType=Application\n'
                'Name=Z stable peer\nExec=/usr/bin/touch ' + str(marker) + '\n')
            palette, appearance = ('dune', 'light') if profile == 'dune-light' else ('slate', 'dark')
            (user / 'local.config').write_text('*FvwmDesktop: palette ' + palette +
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
                '\nAddToFunc StartFunction I Module FvwmMFL\n')
            shutil.copy2(config, out / 'config')
            evidence['config_sha256'] = hashlib.sha256(config.read_bytes()).hexdigest()
            evidence['font'] = 'DejaVu Sans Mono 18' if profile == 'classic-large' else 'Sans 10'
            env = runtime.environment({'DISPLAY': display, 'HOME': str(runtime.root),
                'FVWM_USERDIR': str(user), 'FVWM_DESKTOP_ROOT': str(assets),
                'FVWMMFL_SOCKET_PATH': str(runtime.root), 'FVWM_DESKTOP_WORKER': str(worker),
                'XDG_DATA_HOME': str(apps.parent), 'XDG_DATA_DIRS': str(runtime.root / 'absent')})
            core = runtime.start('core', [str(build / 'fvwm3'), '-d', display,
                '-f', str(config), '-v', '-o', '-'], 'core.log', env)
            endpoint = runtime.root / ('fvwm_mfl_' + display.replace(':', '_') + '.sock')
            wait_for_predicate('binding MFL endpoint', endpoint.exists, time.monotonic() + 12, process=core)

            def connect(echo=False):
                peer = Connection(endpoint)
                peer.record()
                peer.socket.settimeout(5)
                connections.append(peer)
                if echo:
                    peer.send(b'set echo\n')
                    peer.echo('binding-observer-ready')
                return peer

            control, rpc_peer = connect(True), connect()
            serial = 0

            def command(text):
                nonlocal serial
                serial += 1
                control.send((text + '\nSendToModule FvwmDesktop barrier binding-%d\n' % serial).encode())
                control.expect_echo('binding-%d' % serial)

            def rpc(resource='files', method='query', **fields):
                nonlocal serial
                serial += 1
                token = 'bind-%d' % serial
                rpc_peer.send(frame(token, method, resource=resource, **fields))
                result = rpc_peer.record()['reply']
                assert result['id'] == token, result
                return result

            def desktop():
                nonlocal serial
                serial += 1
                token = 'binding-stat-%d' % serial
                control.send(('SendToModule FvwmDesktop bindings ' + token + '\n').encode())
                deadline = time.monotonic() + 5
                result = {}
                while time.monotonic() < deadline:
                    message = control.record().get('echo', {}).get('message', '')
                    if message.startswith('FVWM_BINDINGS_1 ' + token + ' '):
                        _, _, domain, encoded = message.split()
                        assert domain in ('settings', 'apps', 'system')
                        assert domain not in result
                        entry = json.loads(bytes.fromhex(encoded))
                        assert entry['policy']['sources'] <= 8
                        assert entry['policy']['identity_bytes'] <= 2560
                        result[domain] = entry
                        if len(result) == 3:
                            return result
                raise AssertionError('desktop binding diagnostics unavailable')

            def settled_desktop():
                state = desktop()
                return state if all(state[k]['known'] and not state[k]['lost'] and
                    not state[k]['policy']['in_flight'] and not state[k]['policy']['pending']
                    for k in state) else None

            initial = wait_for_predicate('all desktop binding sources', settled_desktop,
                time.monotonic() + 12, process=core)
            client = connect_client(display)
            errors = SessionXErrors(client, evidence).install()
            view = NativeView(client, errors, core)

            def capture(name):
                previous = os.environ.get('DISPLAY')
                os.environ['DISPLAY'] = display
                try:
                    path = Path(NativeCapture(build / 'bin/fvwm-desktop-capture').screenshot(out).removeprefix('Screenshot saved: '))
                    path.replace(out / name)
                    evidence['screenshots'].append(name)
                finally:
                    if previous is None:
                        os.environ.pop('DISPLAY', None)
                    else:
                        os.environ['DISPLAY'] = previous

            def open_menu():
                before = {window for window in view.children(client.root)
                          if (attributes := view.attributes(window)) and attributes.map_state == 2}
                view.pointer.motion(1050, 100)
                control.send(b'SlateLauncher\n')
                def mapped_menu():
                    for window in set(view.children(client.root)) - before:
                        attributes = view.attributes(window)
                        if (attributes and attributes.map_state == 2 and
                                attributes.save_under and attributes.width > 20 and
                                attributes.height > 20):
                            return window
                    return None
                return wait_for_predicate('mapped native launcher menu', mapped_menu,
                    time.monotonic() + 4, process=core)

            def menu_capture(name):
                window = open_menu()
                pixels = view.pixels(window)
                capture(name)
                view.key('Escape')
                wait_for_predicate('launcher dismissed',
                    lambda: not view.pointer.grabbed(), time.monotonic() + 4,
                    process=core)
                command('Nop')
                return pixels

            def hold(directory, operation):
                (directory / ('held-' + operation + '.json')).unlink(missing_ok=True)
                (directory / ('hold-' + operation)).touch()

            def held(directory, operation):
                path = directory / ('held-' + operation + '.json')
                wait_for_predicate('held ' + operation, path.exists,
                    time.monotonic() + 1, process=core)
                return json.loads(path.read_text())

            def release(directory, record):
                (directory / ('release-' + record['token'])).touch()

            # The active apps worker remains held while sixteen explicit intents
            # replace one pending bit. Old successful data must not be published.
            hold(desktop_jobs, 'apps')
            command('SendToModule FvwmDesktop discover')
            gate = held(desktop_jobs, 'apps')
            control.send(b'SendToModule FvwmDesktop discover\n' * 16)
            command('Nop')
            burst = desktop()
            assert burst['apps']['policy']['in_flight'] and burst['apps']['policy']['pending'], burst
            release(desktop_jobs, gate)
            after = wait_for_predicate('latest coalesced apps publication', settled_desktop,
                time.monotonic() + 6, process=core)
            delta = {key: after['apps']['policy'][key] - initial['apps']['policy'][key] for key in COUNTERS}
            assert delta == dict(invalidations=17, started=2, published=1, rejected=1), delta
            good_rows = menu_rows(native, 'SlateLauncherMenu')
            assert any('Binding ; [exec.touch] 100 `literal`' in row and
                       re.search(r'SendToModule FvwmDesktop launch-bound [0-9a-f]+ [0-9a-f]+ 0$', row)
                       for row in good_rows), good_rows
            # A genuine worker failure retains the known application label and
            # replaces its action with native Greyed until a fresh discovery.
            (desktop_jobs / 'fail-apps').touch()
            command('SendToModule FvwmDesktop discover')
            def lost_apps():
                value = desktop()
                return value if value['apps']['lost'] and not value['apps']['policy']['in_flight'] else None
            lost = wait_for_predicate('native app producer loss', lost_apps,
                time.monotonic() + 5, process=core)
            failed_rows = menu_rows(native, 'SlateLauncherMenu')
            assert any(' Greyed "Binding ; [exec.touch] 100 `literal`"' in row for row in failed_rows), failed_rows
            assert not any(' launch-bound ' in row for row in failed_rows), failed_rows
            evidence['menu_failure_pixels'] = menu_capture('menu-unavailable.png')
            command('SendToModule FvwmDesktop discover')
            recovered = wait_for_predicate('app source recovery', settled_desktop,
                time.monotonic() + 6, process=core)
            assert recovered['apps']['known'] and not recovered['apps']['lost']
            evidence['menu_recovery_pixels'] = menu_capture('menu-recovered.png')
            evidence['desktop_pressure_loss_recovery'] = dict(initial=initial, burst=burst,
                delta=delta, lost=lost, recovered=recovered,
                available_commands=good_rows, unavailable_commands=failed_rows)
            # A mapped menu retains its old row while ordered updates defer.
            # Its callback must reject the newer application publication, then
            # converge to the latest desired menu after unmapping.
            old_rows = menu_rows(native, 'SlateLauncherMenu')
            old_row = next(row for row in old_rows if ' launch-bound ' in row)
            old_callback = 'launch-bound ' + old_row.split(' launch-bound ', 1)[1]
            hold(desktop_jobs, 'apps')
            command('SendToModule FvwmDesktop discover')
            gate = held(desktop_jobs, 'apps')
            mapped_window = open_menu()
            application.write_text('[Desktop Entry]\nType=Application\n'
                'Name=ZZ Binding replacement\nExec=/usr/bin/touch ' + str(marker) + '\n')
            release(desktop_jobs, gate)
            # The core preserves producer ordering: unsafe menu mutations can
            # defer later diagnostics behind them during interaction. Observe
            # the real controller's emitted committed projection directly;
            # request counters only after the menu unwinds.
            changed_rows = wait_for_predicate('new app projection while mapped',
                lambda: (observed if any('ZZ Binding replacement' in row for row in observed)
                         else None) if (observed := menu_rows(native, 'SlateLauncherMenu')) else None,
                time.monotonic() + 5, process=core)
            attributes = view.attributes(mapped_window)
            assert attributes and attributes.map_state == 2
            capture('menu-mapped-old-publication.png')
            view.key('Home')
            view.key('Return')
            wait_for_predicate('selected launcher unmapped',
                lambda: not view.pointer.grabbed(), time.monotonic() + 4, process=core)
            command('Nop')
            changed = wait_for_predicate('mapped update drained', settled_desktop,
                time.monotonic() + 5, process=core)
            received = [row['text'] for row in rows(native) if row['direction'] == 'in']
            assert old_callback in received, (old_callback, received[-32:])
            assert not marker.exists(), 'old mapped application action used a new publication'
            notice = view.wait('FVWM-X')
            command('WindowId ' + str(notice) + ' Close')
            wait_for_predicate('stale-action notice dismissed',
                lambda: not view.named('FVWM-X'), time.monotonic() + 4,
                process=core)
            hook_rows = [row['text'] for row in rows(native) if
                row['direction'] == 'out' and 'DynamicPopDownAction' in row['text']]
            assert hook_rows, 'native popdown invalidation hook was not installed'
            command('Nop')
            replacement_rows = menu_rows(native, 'SlateLauncherMenu')
            assert any('"ZZ Binding replacement"' in row and ' launch-bound ' in row and row.endswith(' 1')
                       for row in replacement_rows), replacement_rows
            evidence['menu_reopened_latest_pixels'] = menu_capture('menu-reopened-latest.png')
            evidence['mapped_menu_stale_action'] = dict(old_callback=old_callback,
                emitted_during_mapping=changed_rows, state_after_unmapping=changed, hooks=hook_rows, replacement_commands=replacement_rows)
            # Reload while a real request is outstanding closes that source
            # lifetime. Its delayed old result cannot complete the successor.
            hold(desktop_jobs, 'apps')
            command('SendToModule FvwmDesktop discover')
            gate = held(desktop_jobs, 'apps')
            command('SendToModule FvwmDesktop reload-settings')
            release(desktop_jobs, gate)
            reloaded = wait_for_predicate('fresh desktop source lifetimes', settled_desktop,
                time.monotonic() + 6, process=core)
            assert reloaded['apps']['policy']['started'] == 1 and reloaded['apps']['policy']['published'] == 1, reloaded
            evidence['desktop_reload_fresh_lifetimes'] = reloaded

            real_browser = build / 'bin/FvwmPrompt/fvwm-file-browser'
            private_browser = runtime.root / 'browser-image'

            def start_browser(name):
                replacement = runtime.root / 'replacement-image'
                shutil.copy2(real_browser, replacement)
                replacement.replace(private_browser)
                return runtime.start(name, [str(private_browser), '--socket', str(endpoint),
                    '--dir', str(files), '--watch', 'off', '--opener', '/bin/true',
                    '--terminal', '/bin/true'], name + '.log', env)

            app = start_browser('browser')

            def provider_list():
                response = rpc()
                assert response['status'] == 'ok', response
                return response['result']['providers']

            providers = wait_for_predicate('file provider registered', provider_list,
                time.monotonic() + 8, process=app)
            provider = providers[0]['provider']

            def browser_state():
                response = rpc(provider=provider, scope='state')
                if response['status'] != 'ok':
                    assert response['error']['code'] == 'not_ready', response
                    return None
                result = response['result']
                for domain in ('intent', 'projection'):
                    assert result['bindings'][domain]['sources'] <= 8
                    assert result['bindings'][domain]['identity_bytes'] <= 2560
                return result

            def ready_browser():
                state = browser_state()
                return state if state and not state['busy'] and state['availability']['ready'] else None

            current = wait_for_predicate('browser full native snapshot', ready_browser,
                time.monotonic() + 8, process=app)
            window = view.wait('File browser')
            initial_browser = current
            wrapper = runtime.root / 'worker-replacement'
            script(wrapper, ['--browser-worker', real_browser, browser_jobs], ' "$@"')
            # os.Executable owns this private pathname, rather than argv[0].
            # Atomic replacement preserves the running mapped real image while
            # only its subsequent self-worker invocations pass through our gate.
            wrapper.replace(private_browser)

            def work(operation='refresh', values=None):
                state = browser_state()
                response = rpc(method='action', provider=provider, generation=state['generation'],
                    operation=operation, values={} if values is None else values)
                assert response['status'] == 'ok' and response['result']['applied'], response
                return response['result']

            def native_label(name, fragment):
                result = [row['text'] for row in rows(native) if row['direction'] == 'in' and
                    ('SetLabel ' + name + ' "') in row['text'] and fragment in row['text']]
                assert result, (name, fragment)
                return result[-1]

            hold(browser_jobs, 'scan')
            work()
            gate = held(browser_jobs, 'scan')
            loading = browser_state()
            assert loading['busy'] and not loading['progress']['known'] and loading['progress']['total'] is None, loading
            assert not loading['availability']['ready'] and loading['availability']['cancel'], loading
            native_label('progress', 'total unknown')
            native_label('readiness', 'unavailable')
            capture('browser-progress-unknown.png')
            release(browser_jobs, gate)
            current = wait_for_predicate('browser refresh recovery', ready_browser,
                time.monotonic() + 6, process=app)
            assert current['progress']['known'] and current['progress']['total'] == 8, current
            assert current['availability_generation'] == current['generation']
            native_label('progress', '8 entries loaded')
            native_label('readiness', 'Open 8')
            capture('browser-ready.png')
            # Native F5, unlike the provider's intentional busy refusal, admits
            # replacement scans while a previous native refresh is active.
            def form_key(symbol, count=1):
                event = Event()
                event.key.type, event.key.display = 2, client.display
                event.key.window, event.key.root = window, client.root
                event.key.same_screen = 1
                event.key.keycode = client.x.XKeysymToKeycode(client.display,
                    client.x.XStringToKeysym(symbol.encode()))
                assert event.key.keycode
                client.x.XSendEvent.argtypes = [C.c_void_p, C.c_ulong, C.c_int, C.c_long, C.POINTER(Event)]
                for _ in range(count):
                    assert client.x.XSendEvent(client.display, window, 0, 1, C.byref(event))
                client.x.XSync(client.display, 0)

            hold(browser_jobs, 'scan')
            form_key('F5')
            gate = held(browser_jobs, 'scan')
            before_burst = browser_state()
            form_key('F5', 16)
            # Cancellation kills only the gated wrapper; it has no child before
            # exec, and the original worker's late completion cannot publish.
            release(browser_jobs, gate)
            def burst_drained():
                state = ready_browser()
                return state if state and state['bindings']['intent']['invalidations'] >= \
                    before_burst['bindings']['intent']['invalidations'] + 16 else None
            current = wait_for_predicate('latest native browser refresh', burst_drained,
                time.monotonic() + 8, process=app)
            intent = current['bindings']['intent']
            assert intent['rejected'] > before_burst['bindings']['intent']['rejected'], current
            assert intent['started'] <= intent['invalidations'] and not intent['pending'] and not intent['in_flight'], current
            evidence['browser_pressure'] = dict(before=before_burst, after=current)
            # Failed refresh keeps eight last-good entries while readiness and
            # progress stop claiming that the retained snapshot is current.
            (browser_jobs / 'fail-scan').touch()
            work()
            def failed_browser():
                state = browser_state()
                return state if state and not state['busy'] and state['bindings']['intent']['state'] == 'failed' else None
            failed = wait_for_predicate('browser worker failure', failed_browser,
                time.monotonic() + 6, process=app)
            assert failed['loaded'] == 8 and not failed['availability']['ready'] and not failed['progress']['known'], failed
            native_label('readiness', 'unavailable')
            capture('browser-unavailable.png')
            work()
            current = wait_for_predicate('browser failed source retry', ready_browser,
                time.monotonic() + 6, process=app)
            evidence['browser_progress_failure_recovery'] = dict(initial=initial_browser,
                loading=loading, failed=failed, recovered=current)
            # Destroy with work outstanding, then start an independent provider.
            hold(browser_jobs, 'scan')
            work()
            gate = held(browser_jobs, 'scan')
            old_provider = provider
            app.close(time.monotonic() + 3)
            release(browser_jobs, gate)
            wait_for_predicate('old file provider removed', lambda: not provider_list(),
                time.monotonic() + 5, process=core)
            wait_for_predicate('destroyed native browser removed', lambda: not view.named('File browser'),
                time.monotonic() + 5, process=core)
            app = start_browser('browser-successor')
            providers = wait_for_predicate('replacement provider', provider_list,
                time.monotonic() + 8, process=app)
            provider = providers[0]['provider']
            assert provider != old_provider
            successor = wait_for_predicate('replacement full snapshot', ready_browser,
                time.monotonic() + 8, process=app)
            assert successor['bindings']['intent']['started'] == 1 and successor['bindings']['intent']['published'] == 1, successor
            evidence['browser_destroy_reconnect'] = dict(old_provider=old_provider,
                new_provider=provider, successor=successor)
            # Anchor quiet sampling immediately after an observed five-second
            # status completion. No fabricated idle/wakeup instrumentation.
            baseline = desktop()['system']['policy']['started']
            def status_completed():
                state = desktop()
                policy = state['system']['policy']
                return state if policy['started'] > baseline and not policy['in_flight'] and not policy['pending'] else None
            quiet_desktop = wait_for_predicate('actual periodic status completion', status_completed,
                time.monotonic() + 7, process=core)
            quiet_browser = browser_state()
            assert not quiet_browser['bindings']['watch_timer_active'], quiet_browser
            desktop_pids = [json.loads(path.read_text())['child'] for path in native.glob('module-*.json')
                if json.loads(path.read_text())['real'].endswith('/FvwmDesktop')]
            cpu_before = [process_sample(pid) for pid in [core.process.pid, app.process.pid, *desktop_pids]]
            quiet_start = time.monotonic()
            select.select([], [], [], 3)
            quiet_end = time.monotonic()
            after_desktop, after_browser = desktop(), browser_state()
            assert quiet_end - quiet_start >= 3
            for domain in quiet_desktop:
                for key in COUNTERS:
                    assert quiet_desktop[domain]['policy'][key] == after_desktop[domain]['policy'][key], (domain, key, quiet_desktop, after_desktop)
            for domain in ('intent', 'projection'):
                for key in COUNTERS:
                    assert quiet_browser['bindings'][domain][key] == after_browser['bindings'][domain][key], (domain, key, quiet_browser, after_browser)
            assert not after_browser['bindings']['watch_timer_active']
            evidence['idle_observation'] = dict(seconds=quiet_end - quiet_start,
                desktop_before=quiet_desktop, desktop_after=after_desktop,
                browser_before=quiet_browser, browser_after=after_browser,
                proc_before=cpu_before, proc_after=[process_sample(row['pid']) for row in cpu_before])
            assert not marker.exists(), 'literal application entry executed during observation'
            evidence['literal_application_label_did_not_execute'] = True
            app.close(time.monotonic() + 3)
            control.send(b'Quit\n')
            assert core.wait_until_exit(time.monotonic() + 4) == 0
            evidence['status'] = 'passed'
        finally:
            failure = sys.exc_info()[1]
            if failure:
                evidence.update(status='failed', failure=repr(failure))
            def clean(label, operation):
                try:
                    operation()
                except BaseException as error:
                    evidence['status'] = 'failed'
                    evidence.setdefault('cleanup_failures', []).append({
                        'operation': label, 'error': repr(error)})
            if app:
                clean('browser', lambda: app.close(time.monotonic() + 3))
            for peer in connections:
                clean('MFL observer', peer.close)
            if core:
                clean('core', lambda: core.close(time.monotonic() + 4))
            if errors:
                clean('X display and handler', errors.close_display)
                if errors.unexpected:
                    evidence.update(status='failed', cleanup_x_errors=errors.unexpected)
            elif client:
                clean('X display', client.close)
            for path in runtime.root.glob('*.log'):
                shutil.copy2(path, out / path.name)
            for name in ('native-records', 'desktop-workers', 'browser-workers'):
                if (runtime.root / name).exists():
                    shutil.copytree(runtime.root / name, out / name)
            evidence['elapsed_seconds'] = time.monotonic() - started
            atomic_json(out / 'result.json', evidence)
            if not failure and evidence['status'] == 'failed':
                raise AssertionError(evidence.get('cleanup_failures', errors.unexpected if errors else []))
    print(json.dumps({'profile': profile, 'status': evidence['status']}))


if __name__ == '__main__':
    if len(sys.argv) > 1 and sys.argv[1] == '--module-proxy':
        raise SystemExit(module_proxy(sys.argv[2], sys.argv[3], sys.argv[4:]))
    if len(sys.argv) > 1 and sys.argv[1] in ('--desktop-worker', '--browser-worker'):
        raise SystemExit(worker_proxy(sys.argv[2], sys.argv[3], sys.argv[1] == '--browser-worker'))
    parser = argparse.ArgumentParser(description=__doc__, allow_abbrev=False)
    parser.add_argument('--build', required=True, type=Path)
    parser.add_argument('--artifacts-dir', required=True, type=Path)
    parser.add_argument('--profiles', nargs='+', choices=PROFILES, default=list(PROFILES))
    options = parser.parse_args()
    options.artifacts_dir.resolve().mkdir(parents=True, exist_ok=True)
    options.artifacts_dir = Path(tempfile.mkdtemp(
        prefix='run-', dir=options.artifacts_dir.resolve()))
    print(json.dumps({'artifacts_dir': str(options.artifacts_dir)}), flush=True)

    def timeout(signum, stack):
        raise TimeoutError('native binding checkpoint exceeded 150 seconds')

    signal.signal(signal.SIGALRM, timeout)
    for selected_profile in options.profiles:
        signal.alarm(150)
        run(options, selected_profile)
    signal.alarm(0)
