#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
"""Opt-in Linux nested-X Files-menu handoff through the real native browser.

Run only in the owning Linux guest: a fresh 000-prefixed TemporaryDirectory in
its existing Home is entered through the real Home worker's returned index.
HOME and the parent environment are never changed. Native proxies relay original
bytes; the launcher forwards literal argv and waits its actual browser child.
Missing/start/nonzero launcher injections are separate from real browser reuse.
Unknown post-admission outcomes retain the existing Go policy qualification;
this fixture never fabricates MFL responses or claims that fault on real X.
The retained --adjacent-browser flag asserts installed command discovery: only
the owned child's PATH receives the staged bindir, with no browser override.
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
import subprocess
import sys
import tempfile
import time

from browser_continuity_live import cleanup_receipts
from desktop_live import connect_client
from inspector_live import NativeView, atomic_json
from live_x_harness import HarnessRuntime, start_display_server, wait_for_predicate
from mfl_request_live import frame
from mfl_stream_live import Connection
from native_bindings_live import menu_rows, rows, worker_proxy
from native_capture import NativeCapture
from native_module_proxy import module_proxy, terminate
from session_x_errors import SessionXErrors

PROFILES = ('slate-dark', 'dune-light', 'classic-large')
MENU = 'SlateFiles'
ACTION = 'Open this folder in browser'


def launcher(real, directory, arguments):
    """Only the explicit failure mode substitutes for the real executable."""
    root = Path(directory)
    token = str(os.getpid())
    record = dict(launcher=os.getpid(), started=time.monotonic(),
        argv_bytes=[base64.b64encode(os.fsencode(a)).decode() for a in arguments])
    parent = os.getppid()
    record['launcher_identity'] = child_identity(os.getpid(), parent)
    record['parent_identity'] = process_identity(parent)
    assert record['launcher_identity'] and record['parent_identity']
    if (root / 'fail-next').exists():
        (root / 'fail-next').unlink()
        atomic_json(root / ('launch-' + token + '.json'),
            dict(record, injected_failure=True))
        atomic_json(root / ('wait-' + token + '.json'),
            dict(returncode=23, injected_failure=True))
        return 23
    child = subprocess.Popen([real, *arguments])
    record['child'] = child.pid
    atomic_json(root / ('launch-' + token + '.json'), record)
    saved = signal.getsignal(signal.SIGTERM)

    def stop(signum, stack):
        if child.poll() is None:
            child.terminate()

    signal.signal(signal.SIGTERM, stop)
    try:
        returncode = child.wait()
    finally:
        signal.signal(signal.SIGTERM, signal.SIG_IGN)
        try:
            terminate(child)
            atomic_json(root / ('wait-' + token + '.json'),
                dict(child=child.pid, returncode=child.returncode))
        finally:
            signal.signal(signal.SIGTERM, saved)
    return returncode if returncode >= 0 else 128 - returncode


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def process_identity(pid):
    """Read kernel identity without inspecting an unowned command line."""
    try:
        fields = Path('/proc/%d/stat' % pid).read_text().rsplit(')', 1)[1].split()
        return dict(child=pid, parent=int(fields[1]), starttime=int(fields[19]))
    except FileNotFoundError:
        return None


def child_identity(pid, parent):
    """Check ownership before reading only this owned child's command line."""
    try:
        identity = process_identity(pid)
        if identity is None or identity['parent'] != parent:
            return None
        arguments = [base64.b64encode(a).decode() for a in
            Path('/proc/%d/cmdline' % pid).read_bytes().split(b'\0')[:-1]]
        if process_identity(pid) != identity:
            return None
        return dict(identity, argv_bytes=arguments)
    except FileNotFoundError:
        return None


def run(args, profile, run_root):
    source, build = Path(__file__).resolve().parents[2], args.build.resolve()
    out = run_root / profile
    out.mkdir()
    evidence = dict(status='running', profile=profile, screenshots={},
        cleanup_failures=[], cases={}, limits=dict(
            real_unknown_admission_fault='not run; existing Go qualification',
            installed_browser_command=str(args.adjacent_browser) if args.adjacent_browser else 'separate lead-owned check',
            native_cpu_hardware_assistive_acceptance=False))
    source_inputs = ['tests/live_x/browser_handoff_live.py',
        'tests/live_x/native_bindings_live.py', 'tests/live_x/native_module_proxy.py',
        'tests/live_x/inspector_live.py', 'tests/live_x/session_x_errors.py',
        'tests/live_x/live_x_harness.py', 'tests/live_x/browser_continuity_live.py',
        'tests/live_x/native_capture.py']
    source_inputs += [str(p.relative_to(source)) for directory in (
        'bin/FvwmPrompt/internal/desktopcontroller', 'bin/FvwmPrompt/cmd/fvwmdesktop',
        'bin/FvwmPrompt/cmd/fvwmfilebrowser', 'bin/FvwmPrompt/internal/desktopworker')
        for p in sorted((source / directory).glob('*.go'))]
    evidence['source_sha256'] = {p: digest(source / p) for p in source_inputs}
    binaries = ['fvwm3', 'bin/FvwmPrompt/FvwmDesktop',
        'bin/FvwmPrompt/fvwm-desktop-worker', 'bin/FvwmPrompt/fvwm-file-browser',
        'modules/FvwmForm/FvwmForm', 'modules/FvwmMFL/FvwmMFL',
        'bin/fvwm-desktop-capture']
    evidence['binary_sha256'] = {p: digest(build / p) for p in binaries}
    if args.adjacent_browser:
        evidence['installed_browser_binary_sha256'] = digest(args.adjacent_browser.resolve())
        evidence['installed_browser_discovery'] = dict(helper_override=False,
            expected_executable=str(args.adjacent_browser.resolve()),
            child_path_prefix=str(args.adjacent_browser.resolve().parent))
    with HarnessRuntime(prefix='fvwm-browser-handoff-') as runtime, \
            tempfile.TemporaryDirectory(prefix='000-fvwm-browser-handoff-',
                dir=Path.home()) as owned_home:
        core = client = errors = control = rpc_peer = None
        adjacent_child = desktop_pid = None
        launches = runtime.root / 'launches'
        launches.mkdir(mode=0o700)
        try:
            _, display = start_display_server(runtime, 'Xvfb',
                ['Xvfb', '-screen', '0', '1440x1200x24', '-noreset'],
                'xserver.log', time.monotonic() + 10)
            modules, user, native, jobs = [runtime.root / p for p in
                ('modules', 'user', 'native-records', 'desktop-workers')]
            for directory in (modules, user, native, jobs):
                directory.mkdir(mode=0o700)
            for directory in (build / 'modules').iterdir():
                executable = directory / directory.name
                if executable.is_file() and directory.name not in ('FvwmForm', 'FvwmMFL'):
                    (modules / directory.name).symlink_to(executable)
            fixture = Path(__file__).resolve()

            def script(path, arguments, suffix=''):
                path.write_text('#!/bin/sh\nexec ' + shlex.join([sys.executable,
                    str(fixture), *map(str, arguments)]) + suffix + '\n')
                path.chmod(0o700)

            for name, binary in [('FvwmDesktop', build / binaries[1]),
                    ('FvwmForm', build / binaries[4]), ('FvwmMFL', build / binaries[5])]:
                script(modules / name, ['--module-proxy', binary, native],
                    ' "$@" 2>>' + shlex.quote(str(runtime.root / (name + '.log'))))
            worker, browser = runtime.root / 'worker', runtime.root / 'browser'
            script(worker, ['--worker', build / binaries[2], jobs])

            def browser_script():
                script(browser, ['--launcher', build / binaries[3], launches],
                    ' "$@" 2>>' + shlex.quote(str(runtime.root / 'browser.log')))

            browser_script()
            folder = Path(owned_home)
            marker = folder / ('must-not-execute-' + folder.name.rsplit('-', 1)[-1])
            literal = folder / ('literal space ; $(touch ' + marker.name + ') '
                '`touch ' + marker.name + '` $[exec.touch] % " quote')
            literal.mkdir()
            (literal / 'entry.txt').write_text('fixture\n')
            (folder / 'other').mkdir()
            evidence['home'] = dict(inherited=str(Path.home()), owned=str(folder),
                override=False, literal_folder_bytes=base64.b64encode(os.fsencode(literal)).decode())
            assets = source / 'default-config'
            palette, appearance = ('dune', 'light') if profile == 'dune-light' else ('slate', 'dark')
            local = user / 'local.config'
            local.write_text('*FvwmDesktop: palette ' + palette +
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
            for name, path in [('config', config), ('local.config', local),
                    ('default.config', assets / 'config'), ('desktop.fvwm', assets / 'desktop.fvwm'),
                    ('palettes.json', assets / 'palettes.json')]:
                shutil.copy2(path, out / name)
                evidence.setdefault('configuration_sha256', {})[name] = digest(path)
            evidence['font'] = 'DejaVu Sans Mono 18' if profile == 'classic-large' else 'Sans 10'
            env = runtime.environment({'DISPLAY': display, 'FVWM_USERDIR': str(user),
                'FVWM_DESKTOP_ROOT': str(assets), 'FVWMMFL_SOCKET_PATH': str(runtime.root),
                'FVWM_DESKTOP_WORKER': str(worker), 'FVWM_FILE_BROWSER': str(browser),
                'XDG_DATA_HOME': str(runtime.root / 'data'),
                'XDG_DATA_DIRS': str(runtime.root / 'absent')})
            if args.adjacent_browser:
                env.pop('FVWM_FILE_BROWSER', None)
                env['PATH'] = str(args.adjacent_browser.resolve().parent) + os.pathsep + env.get('PATH', os.defpath)
            assert env.get('HOME') == os.environ.get('HOME')
            core = runtime.start('core', [str(build / 'fvwm3'), '-d', display,
                '-f', str(config)], 'core.log', env)
            endpoint = runtime.root / ('fvwm_mfl_' + display.replace(':', '_') + '.sock')
            deadline = time.monotonic() + 12
            wait_for_predicate('handoff MFL endpoint', endpoint.exists, deadline, process=core)

            def provider_notice():
                enrolled = [json.loads(p.read_text()) for p in native.glob('module-*.json')]
                mfl = [r for r in enrolled if r['real'] == str(build / binaries[5])]
                assert len(mfl) <= 1, mfl
                if not mfl:
                    return None
                trace = native / ('module-%d.jsonl' % mfl[0]['proxy'])
                if not trace.exists():
                    return None
                assert trace.stat().st_size <= 8 * 1024 * 1024
                records = [json.loads(line) for line in trace.read_text().split('\n')[:-1]]
                return next((dict(enrollment=mfl[0], notice=r) for r in records if r['direction'] == 'in' and
                    r['kind'] == 1 << 22 and re.fullmatch(
                        r'MFL_PROVIDER_1 [0-9a-f]{32} [0-9a-f]{32}', r['text'])), None)

            evidence['desktop_provider_ready'] = wait_for_predicate(
                'positive real Desktop provider notice', provider_notice, deadline, process=core)
            desktop_records = [json.loads(p.read_text()) for p in native.glob('module-*.json')]
            desktop_record = next(r for r in desktop_records if r['real'] == str(build / binaries[1]))
            desktop_pid = desktop_record['child']
            evidence['desktop_enrollment'] = desktop_record
            if args.adjacent_browser:
                desktop_identity = child_identity(desktop_pid, desktop_record['proxy'])
                assert desktop_identity is not None, 'enrolled Desktop identity unavailable'
                evidence['installed_desktop_identity'] = desktop_identity
                evidence['installed_child_scans'] = []
            control, rpc_peer = Connection(endpoint), Connection(endpoint)
            control.record()
            rpc_peer.record()
            control.send(b'set echo\n')
            control.echo('browser-handoff-ready')
            serial = 0

            def command(text):
                nonlocal serial
                serial += 1
                control.send((text + '\nSendToModule FvwmDesktop barrier handoff-%d\n' % serial).encode())
                control.expect_echo('handoff-%d' % serial)

            def theme():
                nonlocal serial
                serial += 1
                tag = 'handoff-theme-%d' % serial
                control.send(('SendToModule FvwmDesktop query-settings ' + tag + ' all\n').encode())
                values = {}
                for _ in range(128):
                    text = control.record().get('echo', {}).get('message', '')
                    if text == 'desktop-settings-' + tag + ' done':
                        return values if all(values.get(k, {}).get('effective') == v
                            for k, v in [('palette', palette), ('appearance', appearance),
                                         ('transparency', False)]) else None
                    if text.startswith('desktop-settings-' + tag + ' '):
                        _, key, value = text.split(' ', 2)
                        values[key] = json.loads(value)
                raise AssertionError('theme response bound exceeded')

            evidence['theme'] = wait_for_predicate('requested real theme', theme,
                deadline, process=core)
            client = connect_client(display)
            errors = SessionXErrors(client, evidence).install()
            view = NativeView(client, errors, core)

            def capture(name):
                result = subprocess.run([sys.executable, str(fixture), '--capture',
                    str(build / binaries[-1]), str(out)], env=env, capture_output=True,
                    text=True, timeout=40, check=True)
                path = Path(json.loads(result.stdout))
                target = out / name
                path.replace(target)
                evidence['screenshots'][name] = digest(target)

            def mapped_menu():
                return next((w for w in view.children(client.root)
                    if (a := view.attributes(w)) and a.map_state == 2 and
                    a.save_under and a.width > 20 and a.height > 20), None)

            def dismiss():
                view.key('Escape')
                wait_for_predicate('menu pointer released', lambda: not view.pointer.grabbed(),
                    time.monotonic() + 4, process=core)

            def directory_menu(action, path):
                before = max((r['time'] for r in rows(native)), default=0)
                control.send(('SendToModule FvwmDesktop ' + action + '\n').encode())

                def committed():
                    current = menu_rows(native, MENU)
                    matches = [r for r in rows(native) if r['time'] > before and
                        r['direction'] == 'out' and r['text'].startswith(
                            'AddToMenu ' + MENU + ' ') and ' files-browser ' in r['text']]
                    return current if matches and mapped_menu() else None

                current = wait_for_predicate('committed and mapped Files menu', committed,
                    time.monotonic() + 6, process=core)
                records = [json.loads(p.read_text()) for p in jobs.glob('*.json')]
                result = next(r for r in sorted(records, key=lambda r: r['started'], reverse=True)
                    if r['request']['operation'] == 'files' and r['request']['path'] == str(path))
                assert result['returncode'] == 0 and not result['response'].get('error'), result
                return current, result['response']['result']

            home_rows, entries = directory_menu('files', Path.home())
            index = next(i for i, entry in enumerate(entries) if entry['path'] == str(folder))
            assert index < 100
            dismiss()
            _, entries = directory_menu('file %d' % index, folder)
            index = next(i for i, entry in enumerate(entries) if entry['path'] == str(literal))
            dismiss()
            current, _ = directory_menu('file %d' % index, literal)
            old_token = next(re.search(r'files-browser ([0-9a-f]{32})$', row)[1]
                for row in current if ' files-browser ' in row)
            evidence['initial_menu_commands'] = current
            menu_window = mapped_menu()
            geometry = view.attributes(menu_window)
            evidence['menu_geometry'] = dict(window=menu_window,
                x=geometry.x, y=geometry.y, width=geometry.width,
                height=geometry.height, screen=[1440, 1200])
            capture('files-menu.png')
            evidence['menu_pixels'] = view.pixels(menu_window)

            def activate_browser():
                current = menu_rows(native, MENU)
                actions = [row for row in current if ' SendToModule ' in row]
                index = next(i for i, row in enumerate(actions) if '"' + ACTION + '"' in row)
                assert index == 3, current
                view.key('Home')
                for _ in range(index):
                    view.key('Down')
                view.key('Return')
                wait_for_predicate('selected native menu dismissed',
                    lambda: not view.pointer.grabbed(), time.monotonic() + 4, process=core)

            def launch_records():
                return [json.loads(p.read_text()) for p in sorted(launches.glob('launch-*.json'))]

            def adjacent_children():
                # Go may start a process from any of its OS threads. Linux
                # exposes children on the creating thread, while the child's
                # PPID still identifies the enrolled Desktop process.
                assert child_identity(desktop_pid, desktop_record['proxy']) == desktop_identity, \
                    'enrolled Desktop identity changed before child scan'
                tasks = []
                for task in Path('/proc/%d/task' % desktop_pid).iterdir():
                    assert len(tasks) < 256, 'Desktop task count exceeds fixture bound'
                    assert task.name.isdecimal(), task
                    tasks.append(task)
                per_thread, vanished, child_pids = {}, [], set()
                total = 0
                for task in sorted(tasks, key=lambda path: int(path.name)):
                    try:
                        with (task / 'children').open() as stream:
                            raw = stream.read(8193)
                    except FileNotFoundError:
                        vanished.append(int(task.name))
                        continue
                    assert len(raw) <= 8192, 'Desktop thread child record exceeds fixture bound'
                    values = raw.split()
                    assert all(value.isdecimal() for value in values), values
                    total += len(values)
                    assert total <= 512, 'Desktop total child count exceeds fixture bound'
                    children = [int(value) for value in values]
                    per_thread[task.name] = children
                    child_pids.update(children)
                assert child_identity(desktop_pid, desktop_record['proxy']) == desktop_identity, \
                    'enrolled Desktop identity changed during task scan'
                result = [identity for pid in sorted(child_pids)
                    if (identity := child_identity(pid, desktop_pid))]
                assert child_identity(desktop_pid, desktop_record['proxy']) == desktop_identity, \
                    'enrolled Desktop identity changed during child identity scan'
                scans = evidence['installed_child_scans']
                assert len(scans) < 512, 'Desktop child scan count exceeds fixture bound'
                scans.append(dict(observed=time.monotonic(), task_count=len(tasks),
                    per_thread_children=per_thread,
                    main_thread_children=per_thread.get(str(desktop_pid), []),
                    vanished_threads=vanished, children=result))
                return result

            def adjacent_browser():
                expected_binary = base64.b64encode(os.fsencode(args.adjacent_browser.resolve())).decode()
                matches = [r for r in adjacent_children() if r['argv_bytes'] and
                    r['argv_bytes'][0] == expected_binary]
                assert len(matches) <= 1, matches
                return matches[0] if matches else None

            def rpc(**fields):
                nonlocal serial
                serial += 1
                token = 'handoff-rpc-%d' % serial
                rpc_peer.send(frame(token, 'query', resource='files', **fields))
                reply = rpc_peer.record()['reply']
                assert reply['id'] == token
                return reply

            def providers():
                value = rpc()
                assert value['status'] == 'ok', value
                return value['result']['providers']

            activate_browser()
            if args.adjacent_browser:
                adjacent_child = first = wait_for_predicate('real installed browser child argv',
                    adjacent_browser, time.monotonic() + 6, process=core)
            else:
                first = wait_for_predicate('actual browser launch recorded',
                    lambda: launch_records() or None, time.monotonic() + 6, process=core)[0]
            assert not first.get('injected_failure') and first['child'] > 0
            expected = [base64.b64encode(os.fsencode(a)).decode()
                for a in ['--reuse', '--', str(literal)]]
            if args.adjacent_browser:
                expected.insert(0, base64.b64encode(os.fsencode(args.adjacent_browser.resolve())).decode())
            assert first['argv_bytes'] == expected, first
            listed = wait_for_predicate('real file provider', providers,
                time.monotonic() + 8, process=core)
            assert len(listed) == 1, listed
            provider = listed[0]['provider']

            def ready():
                value = rpc(provider=provider, scope='state')
                if value['status'] != 'ok':
                    assert value['error']['code'] == 'not_ready', value
                    return None
                state = value['result']
                return state if not state['busy'] and state['availability']['ready'] else None

            initial = wait_for_predicate('real browser ready', ready,
                time.monotonic() + 8, process=core)
            assert base64.b64decode(initial['path_bytes']) == os.fsencode(literal), initial
            window = view.wait('File browser')
            evidence['cases']['open_literal_folder'] = dict(launch=first,
                provider=provider, state=initial, window=window)
            capture('browser-open.png')
            command('WindowId %#x Iconify on' % window)
            wait_for_predicate('browser iconified', lambda: not view.named('File browser'),
                time.monotonic() + 4, process=core)
            control.send(b'Menu SlateFiles Monitor c c\n')
            wait_for_predicate('Files remapped', mapped_menu, time.monotonic() + 4, process=core)
            activate_browser()
            second = reuse_wait = None
            if not args.adjacent_browser:
                reused = wait_for_predicate('reuse launcher recorded',
                    lambda: launch_records() if len(launch_records()) == 2 else None,
                    time.monotonic() + 6, process=core)
                second = next(r for r in reused if r['launcher'] != first['launcher'])
                assert second['argv_bytes'] == expected, second
                waited = launches / ('wait-%d.json' % second['launcher'])
                wait_for_predicate('actual reuse process Wait', waited.exists,
                    time.monotonic() + 12, process=core)
                reuse_wait = json.loads(waited.read_text())
                assert reuse_wait['returncode'] == 0
            wait_for_predicate('same native browser presented',
                lambda: view.named('File browser') == window, time.monotonic() + 6, process=core)
            after = wait_for_predicate('reused browser idle', ready,
                time.monotonic() + 6, process=core)
            assert [p['provider'] for p in providers()] == [provider]
            assert after['path_bytes'] == initial['path_bytes']
            evidence['cases']['reuse_deiconify'] = dict(launch=second,
                wait=reuse_wait, window=window, state=after)
            if args.adjacent_browser:
                def only_original():
                    children = adjacent_children()
                    browsers = [r for r in children if r['argv_bytes'] == first['argv_bytes']]
                    return children if browsers == [first] else None
                evidence['cases']['reuse_deiconify']['direct_children_after_reuse'] = wait_for_predicate(
                    'installed browser reuse leaves only original browser', only_original,
                    time.monotonic() + 6, process=core)
            capture('browser-reused.png')

            def notice_since(start, fragment):
                text = []
                for row in rows(native):
                    prefix = '*SlateNoticeForm: TextHex x'
                    if row['time'] > start and row['direction'] == 'out' and row['text'].startswith(prefix):
                        text.append(bytes.fromhex(row['text'][len(prefix):]).decode())
                combined = ''.join(text)
                return combined if fragment in combined else None

            def notice_case(name, action, fragment, expected_launches):
                start = time.monotonic()
                action()
                text = wait_for_predicate(name + ' real notice',
                    lambda: notice_since(start, fragment), time.monotonic() + 6, process=core)
                view.wait('FVWM-X')
                capture(name + '.png')
                command('KillModule FvwmForm SlateNoticeForm')
                if args.adjacent_browser:
                    assert adjacent_browser() == first
                else:
                    assert len(launch_records()) == expected_launches, launch_records()
                evidence['cases'][name] = dict(notice=text,
                    launches=expected_launches if not args.adjacent_browser else 'direct owned child unchanged')

            # A later directory invalidates the prior committed callback.
            current, _ = directory_menu('files-up', folder)
            dismiss()
            notice_case('stale-token', lambda: command(
                'SendToModule FvwmDesktop files-browser ' + old_token),
                'Files menu is stale', 2)
            token = next(re.search(r'files-browser ([0-9a-f]{32})$', row)[1]
                for row in current if ' files-browser ' in row)

            def reopen_and_select():
                control.send(b'Menu SlateFiles Monitor c c\n')
                wait_for_predicate('usable native Files menu', mapped_menu,
                    time.monotonic() + 4, process=core)
                activate_browser()

            if args.adjacent_browser:
                evidence['cases']['override_failures'] = dict(status='skipped',
                    reason='installed command discovery has no override; normal three-theme runs own fault injections')
            else:
                browser.rename(browser.with_suffix('.saved'))
                notice_case('missing-browser', reopen_and_select, 'File browser is unavailable', 2)
                browser.write_text('#!/fixture/absent-interpreter\n')
                browser.chmod(0o700)
                notice_case('start-failure', reopen_and_select, 'Could not start file browser', 2)
                browser.unlink()
                browser.with_suffix('.saved').rename(browser)
                (launches / 'fail-next').touch()
                notice_case('injected-nonzero-exit', reopen_and_select,
                    'File browser exited unsuccessfully.', 3)
                assert any(r.get('injected_failure') for r in launch_records())
            # Prove failure leaves the same menu usable without another launch.
            control.send(b'Menu SlateFiles Monitor c c\n')
            wait_for_predicate('Files usable after errors', mapped_menu,
                time.monotonic() + 4, process=core)
            capture('files-after-failures.png')
            dismiss()
            assert token != old_token
            assert not marker.exists() and not (literal / marker.name).exists()
            assert not (Path.cwd() / marker.name).exists(), 'literal argv unexpectedly executed'
            evidence['cases']['literal_bytes_no_execution'] = True
            # Native Escape asks the actual Form/browser to retire, then observe
            # provider removal and parent reaping while Desktop/core remain alive.
            command('WindowId %#x Focus' % window)
            view.key('Escape')
            wait_for_predicate('browser provider retired before core exit',
                lambda: not providers(), time.monotonic() + 5, process=core)
            if args.adjacent_browser:
                wait_for_predicate('installed browser reaped by Desktop',
                    lambda: child_identity(first['child'], desktop_pid) is None,
                    time.monotonic() + 5, process=core)
                evidence['installed_browser_reaped_core_alive'] = core.poll() is None
                os.kill(desktop_pid, 0)
                evidence['installed_browser_parent_desktop_alive'] = True
            else:
                waited = launches / ('wait-%d.json' % first['launcher'])
                wait_for_predicate('native close actual browser Wait', waited.exists,
                    time.monotonic() + 5, process=core)
                assert json.loads(waited.read_text())['returncode'] == 0
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

            # Stop only recorded owned launchers. Their child Wait receipts must
            # exist before the core exits: Desktop deliberately owns no timeout.
            def stop_browsers():
                if adjacent_child:
                    identity = child_identity(adjacent_child['child'], desktop_pid)
                    if identity == adjacent_child:
                        os.kill(adjacent_child['child'], signal.SIGTERM)
                        wait_for_predicate('owned installed browser child reaped',
                            lambda: child_identity(adjacent_child['child'], desktop_pid) is None,
                            time.monotonic() + 5)
                records = [json.loads(p.read_text()) for p in launches.glob('launch-*.json')]
                for record in records:
                    waited = launches / ('wait-%d.json' % record['launcher'])
                    if not waited.exists():
                        identity = child_identity(record['launcher'], desktop_pid)
                        parent_identity = process_identity(desktop_pid)
                        owned = (identity == record['launcher_identity'] and
                            identity is not None and
                            identity['child'] == record['launcher'] and
                            parent_identity == record['parent_identity'] and
                            parent_identity is not None and
                            parent_identity['child'] == desktop_pid and
                            identity['argv_bytes'][-len(record['argv_bytes']):] ==
                            record['argv_bytes'])
                        # A completion can race the identity sample. Its actual
                        # Wait receipt remains authoritative; absent that receipt,
                        # an unavailable or changed identity must never be killed.
                        if not waited.exists():
                            if not owned:
                                raise RuntimeError('launcher ownership unavailable or changed: ' +
                                    str(record['launcher']))
                            os.kill(record['launcher'], signal.SIGTERM)
                    wait_for_predicate('browser exact child Wait', waited.exists,
                        time.monotonic() + 5)
                    result = json.loads(waited.read_text())
                    assert result['returncode'] is not None
                    if 'child' in record:
                        assert result['child'] == record['child']
                evidence['browser_waits_before_core_exit'] = [json.loads(p.read_text())
                    for p in launches.glob('wait-*.json')]

            clean('actual browser Wait before core exit', stop_browsers)
            for peer in (control, rpc_peer):
                if peer:
                    clean('MFL observer', peer.close)
            if core:
                clean('core', lambda: core.close(time.monotonic() + 4))
            if errors:
                clean('X display and handler', errors.close_display)
            elif client:
                clean('X display', client.close)
            for process in reversed(runtime.processes):
                clean('stop ' + process.name, lambda p=process: p.close(time.monotonic() + 4))
            evidence['children_after_cleanup'] = {p.name: p.poll() for p in runtime.processes}
            if (runtime.root / 'native-records').exists():
                def waited_modules():
                    evidence['module_cleanup'] = cleanup_receipts(runtime.root / 'native-records')
                clean('real module child Wait receipts', waited_modules)
            for path in runtime.root.iterdir():
                if path.is_dir() and path.name in ('native-records', 'desktop-workers', 'launches'):
                    clean('copy ' + path.name, lambda p=path: shutil.copytree(p, out / p.name))
                elif path.is_file() and path.suffix == '.log':
                    clean('copy ' + path.name, lambda p=path: shutil.copy2(p, out / p.name))
            if evidence.get('observer_unexpected_x_errors'):
                evidence['status'] = 'failed'
            atomic_json(out / 'result.json', evidence)
    assert evidence['status'] == 'passed', evidence
    print(json.dumps(dict(profile=profile, status=evidence['status'], artifacts=str(out))))


def main():
    if len(sys.argv) > 1 and sys.argv[1] == '--module-proxy':
        return module_proxy(sys.argv[2], sys.argv[3], sys.argv[4:])
    if len(sys.argv) == 4 and sys.argv[1] == '--worker':
        return worker_proxy(sys.argv[2], sys.argv[3])
    if len(sys.argv) > 1 and sys.argv[1] == '--launcher':
        return launcher(sys.argv[2], sys.argv[3], sys.argv[4:])
    if len(sys.argv) == 4 and sys.argv[1] == '--capture':
        print(json.dumps(NativeCapture(sys.argv[2]).screenshot(
            sys.argv[3]).removeprefix('Screenshot saved: ')))
        return 0
    parser = argparse.ArgumentParser(description=__doc__, allow_abbrev=False)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--profiles', nargs='+', choices=PROFILES, default=list(PROFILES))
    parser.add_argument('--artifacts-dir', type=Path, required=True)
    parser.add_argument('--adjacent-browser', type=Path,
        help='expected installed browser discovered through owned child PATH; assertion only, no helper override')
    args = parser.parse_args()
    if sys.platform != 'linux':
        parser.error('run only in the owned Linux guest; Home fixture creation is opt-in')
    args.artifacts_dir.mkdir(parents=True, exist_ok=True)
    run_root = Path(tempfile.mkdtemp(prefix='browser-handoff-', dir=args.artifacts_dir.resolve()))
    print(json.dumps(dict(artifacts_dir=str(run_root))), flush=True)
    for profile in args.profiles:
        run(args, profile, run_root)
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
