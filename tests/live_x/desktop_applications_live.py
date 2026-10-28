#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
"""Observe installed Desktop Applications discovery and native launch actions.

Run in the owned Linux guest with --build INSTALLED_VIEW --artifacts-dir OUT.
The view retains the normal build layout, with Desktop, worker and capture
symlinks pointing to their actual staged installations. All three profiles run
by default. French metadata selection uses the Go policy's locale inputs; no
OS locale generation, external terminal, association or real user app is needed.
Only fixture-owned record-and-exit programs launch. Transparent native/worker
proxies forward actual bytes; no worker result or MFL reply is manufactured.
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
import subprocess
import sys
import tempfile
import time

from browser_continuity_live import cleanup_receipts
from browser_handoff_live import child_identity, process_identity
from desktop_live import connect_client
from inspector_live import NativeView, atomic_json
from live_x_harness import HarnessRuntime, start_display_server, wait_for_predicate
from mfl_stream_live import Connection
from native_bindings_live import menu_rows, rows, worker_proxy
from native_capture import NativeCapture
from native_module_proxy import MAX_RECORD_BYTES, module_proxy
from session_x_errors import SessionXErrors

PROFILES = ('slate-dark', 'dune-light', 'classic-large')
MENU = 'SlateLauncherMenu'
NAME = 'Éditeur démo'
ACTION = NAME + ' — Inspecter'


def encoded(values):
    return [base64.b64encode(os.fsencode(value)).decode() for value in values]


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def record_application(directory, executable, arguments):
    """This owned program records only its own identity and exits immediately."""
    parent = os.getppid()
    identity = child_identity(os.getpid(), parent)
    parent_identity = process_identity(parent)
    assert identity and parent_identity
    atomic_json(Path(directory) / ('application-%d.json' % os.getpid()), dict(
        identity=identity, parent_identity=parent_identity,
        literal_argv_bytes=encoded([executable, *arguments]),
        cwd_bytes=encoded([os.getcwd()])[0], recorded=time.monotonic()))
    return 0


def desktop_string(text):
    return text.replace('\\', '\\\\').replace('\n', '\\n').replace('\t', '\\t')


def run(args, profile, run_root):
    source, build = Path(__file__).resolve().parents[2], args.build.resolve()
    fixture = Path(__file__).resolve()
    out = run_root / profile
    out.mkdir()
    evidence = dict(status='running', profile=profile, cases={}, screenshots={},
        cleanup_failures=[], limits=dict(
            installed_scope='Desktop/worker/capture via lead-supplied view; full install acceptance separate',
            terminal='typed worker result only; no external terminal launched',
            locale='metadata inputs only; actual font rendering requires capture inspection',
            broader_xdg_mime_release_acceptance=False))
    binaries = ['fvwm3', 'bin/FvwmPrompt/FvwmDesktop',
        'bin/FvwmPrompt/fvwm-desktop-worker', 'modules/FvwmForm/FvwmForm',
        'modules/FvwmMFL/FvwmMFL', 'bin/fvwm-desktop-capture']
    evidence['binaries'] = {name: dict(path=str((build / name).resolve()),
        sha256=digest(build / name)) for name in binaries}
    inputs = ['tests/live_x/desktop_applications_live.py',
        'tests/live_x/native_bindings_live.py', 'tests/live_x/native_module_proxy.py',
        'tests/live_x/browser_handoff_live.py', 'tests/live_x/browser_continuity_live.py',
        'tests/live_x/inspector_live.py', 'tests/live_x/live_x_harness.py',
        'tests/live_x/session_x_errors.py', 'tests/live_x/native_capture.py']
    inputs += [str(path.relative_to(source)) for directory in (
        'bin/FvwmPrompt/internal/desktopworker', 'bin/FvwmPrompt/internal/desktopcontroller',
        'bin/FvwmPrompt/internal/xdgmenu', 'bin/FvwmPrompt/cmd/fvwmdesktop')
        for path in sorted((source / directory).glob('*.go'))]
    evidence['source_sha256'] = {name: digest(source / name) for name in inputs}
    with HarnessRuntime(prefix='fvwm-desktop-applications-') as runtime:
        core = client = errors = control = None
        desktop_identity = None
        records = runtime.root / 'application-records'
        records.mkdir(mode=0o700)
        try:
            _, display = start_display_server(runtime, 'Xvfb',
                ['Xvfb', '-screen', '0', '1440x1200x24', '-noreset'],
                'xserver.log', time.monotonic() + 10)
            modules, user, native, jobs = [runtime.root / name for name in
                ('modules', 'user', 'native-records', 'desktop-workers')]
            data, system, lower, relative = [runtime.root / name for name in
                ('data', 'system', 'lower', 'relative-data')]
            cwd = runtime.root / 'working space'
            for directory in (modules, user, native, jobs, cwd, data / 'applications',
                    system / 'applications', lower / 'applications', relative / 'applications'):
                directory.mkdir(mode=0o700, parents=True)
            for directory in (build / 'modules').iterdir():
                executable = directory / directory.name
                if executable.is_file() and directory.name not in ('FvwmForm', 'FvwmMFL'):
                    (modules / directory.name).symlink_to(executable)

            def script(path, arguments, suffix=''):
                path.write_text('#!/bin/sh\nexec ' + shlex.join([sys.executable,
                    str(fixture), *map(str, arguments)]) + suffix + '\n')
                path.chmod(0o700)

            for name, binary in [('FvwmDesktop', binaries[1]),
                    ('FvwmForm', binaries[3]), ('FvwmMFL', binaries[4])]:
                script(modules / name, ['--module-proxy', build / binary, native],
                    ' "$@" 2>>' + shlex.quote(str(runtime.root / (name + '.log'))))
            worker = runtime.root / 'desktop-worker'
            script(worker, ['--worker', build / binaries[2], jobs])
            application, missing = runtime.root / 'application recorder', runtime.root / 'missing-later'
            for path in (application, missing):
                script(path, ['--application', records, path], ' "$@"')
            marker = runtime.root / 'must-not-execute'
            entry = data / 'applications/editor.desktop'

            def write(root, name, body):
                path = root / 'applications' / name
                path.write_text('[Desktop Entry]\nType=Application\n' + body)
                return path

            def simple(name, executable=application):
                return 'Name=' + name + '\nExec="' + str(executable) + '" plain\n'

            def editor(action_name='Inspecter'):
                # Desktop string escaping and Exec escaping are distinct passes.
                command = ('"' + str(application) + '" main "" "two words" '
                    '"semi; & literal" "\\$(touch ' + str(marker) + ')" '
                    '"\\`touch ' + str(marker) + '\\`" %% %c %i %k %F %d')
                text = ('Name=English editor\nName[fr]=Éditeur démo\n'
                    'Name[fr_FR]=Wrong LANGUAGE priority\nIcon=parent\\sicon\n'
                    'Path=' + str(cwd).replace(' ', '\\s') + '\nTerminal=false\n'
                    'OnlyShowIn=FVWM;\nExec=' + desktop_string(command) + '\n'
                    'Actions=Inspect;Recent;Bad;' + ''.join('Extra%d;' % i for i in range(8)) + '\n'
                    '[Desktop Action Inspect]\nName=Inspect\nName[fr]=' + action_name + '\n'
                    'Icon=action\\sicon\nExec=' + desktop_string(
                        '"' + str(application) + '" inspect %c %i %k') + '\n'
                    '[Desktop Action Recent]\nName=Recent\nName[fr]=Récent\nExec=' +
                    desktop_string('"' + str(application) + '" recent %c %i %k') + '\n'
                    '[Desktop Action Bad]\nName=Bad action\nExec="' + str(application) + '" %Z\n')
                for index in range(8):
                    text += ('[Desktop Action Extra%d]\nName=Extra%d\nName[fr]=Supplément%d\n'
                        'Exec="%s" extra%d\n' % (index, index, index, application, index))
                text += ('[Desktop Action Unlisted]\nName=Unlisted forbidden\n'
                    'Exec="' + str(application) + '" unlisted\n')
                entry.write_text('[Desktop Entry]\nType=Application\n' + text)

            editor()
            write(system, 'editor.desktop', simple('Lower editor forbidden'))
            write(system, 'override.desktop', simple('System override forbidden'))
            write(data, 'override.desktop', simple('Priorité utilisateur'))
            write(system, 'hidden.desktop', simple('Hidden lower forbidden'))
            write(data, 'hidden.desktop', 'Hidden=true\n')
            write(system, 'invalid.desktop', simple('Invalid lower forbidden'))
            write(data, 'invalid.desktop', 'Name=Duplicate\nName=Rejected\nExec="' + str(application) + '"\n')
            write(lower, 'override.desktop', simple('Second system forbidden'))
            write(relative, 'relative.desktop', simple('Relative environment forbidden'))
            write(data, 'terminal.desktop', simple('Terminal maîtrisé') + 'Terminal=true\n')
            write(data, 'missing.desktop', simple('Programme disparu', missing))
            for name, fields in {
                    'bad-code': 'Exec="' + str(application) + '" %Z',
                    'quoted-code': 'Exec="' + str(application) + '" "%c"',
                    'single-quote': 'Exec="' + str(application) + '" \'single quoted\'',
                    'typed-bool': 'Exec="' + str(application) + '"\nTerminal=True',
                    'try-missing': 'Exec="' + str(application) + '"\nTryExec=fixture-program-absent',
                    'other-desktop': 'Exec="' + str(application) + '"\nOnlyShowIn=Other;',
                    'no-display': 'Exec="' + str(application) + '"\nNoDisplay=true',
                    'absent-exec': 'Exec=fixture-program-absent'}.items():
                write(data, name + '.desktop', 'Name=Rejected ' + name + '\n' + fields + '\n')
            for name, path in [('user-data', data), ('system-data', system),
                    ('lower-data', lower), ('relative-data', relative)]:
                shutil.copytree(path, out / name)
            evidence['desktop_source_sha256'] = {str(path.relative_to(runtime.root)): digest(path)
                for root in (data, system, lower, relative) for path in sorted(root.rglob('*.desktop'))}
            evidence['application_wrapper_sha256'] = {str(path): digest(path)
                for path in (application, missing)}
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
                '\nAddToFunc StartFunction I Module FvwmMFL\n')
            for name, path in [('config', config), ('local.config', local),
                    ('default.config', assets / 'config'), ('desktop.fvwm', assets / 'desktop.fvwm'),
                    ('palettes.json', assets / 'palettes.json')]:
                shutil.copy2(path, out / name)
                evidence.setdefault('configuration_sha256', {})[name] = digest(path)
            relative_root = os.path.relpath(relative, Path.cwd())
            env = runtime.environment({'DISPLAY': display, 'FVWM_USERDIR': str(user),
                'FVWM_DESKTOP_ROOT': str(assets), 'FVWMMFL_SOCKET_PATH': str(runtime.root),
                'FVWM_DESKTOP_WORKER': str(worker), 'XDG_DATA_HOME': str(data),
                'XDG_DATA_DIRS': os.pathsep.join([relative_root, str(system), str(lower)]),
                'XDG_CURRENT_DESKTOP': 'FVWM', 'LC_ALL': 'fr_FR.UTF-8', 'LANGUAGE': 'fr'})
            evidence['environment_inputs'] = {name: env[name] for name in
                ('XDG_DATA_HOME', 'XDG_DATA_DIRS', 'XDG_CURRENT_DESKTOP', 'LC_ALL', 'LANGUAGE')}
            evidence['font'] = 'DejaVu Sans Mono 18' if profile == 'classic-large' else 'Sans 10'
            # Explicit request roots are caller-selected and retain relative
            # paths. They are distinct from environment-derived root policy.
            request = dict(version=1, operation='apps', directories=[
                os.path.relpath(relative / 'applications', Path.cwd())])
            explicit = subprocess.run([str(build / binaries[2])], input=json.dumps(request).encode(),
                env=env, capture_output=True, timeout=5, check=True)
            actual = json.loads(explicit.stdout)
            assert not actual.get('error')
            assert [item['name'] for item in actual['result']] == ['Relative environment forbidden'], actual
            evidence['cases']['explicit_relative_root'] = dict(request=request, response=actual)
            core = runtime.start('core', [str(build / 'fvwm3'), '-d', display,
                '-f', str(config)], 'core.log', env)
            endpoint = runtime.root / ('fvwm_mfl_' + display.replace(':', '_') + '.sock')
            startup = time.monotonic() + 12
            wait_for_predicate('Applications MFL endpoint', endpoint.exists, startup, process=core)

            def provider_ready():
                enrollments = [json.loads(p.read_text()) for p in native.glob('module-*.json')]
                mfl = [r for r in enrollments if r['real'] == str(build / binaries[4])]
                assert len(mfl) <= 1
                if not mfl:
                    return None
                trace = native / ('module-%d.jsonl' % mfl[0]['proxy'])
                if not trace.exists():
                    return None
                assert trace.stat().st_size <= MAX_RECORD_BYTES
                for line in trace.read_text().split('\n')[:-1]:
                    row = json.loads(line)
                    if row['direction'] == 'in' and row['kind'] == 1 << 22 and re.fullmatch(
                            r'MFL_PROVIDER_1 [0-9a-f]{32} [0-9a-f]{32}', row['text']):
                        return dict(enrollment=mfl[0], notice=row)
                return None

            evidence['desktop_provider_ready'] = wait_for_predicate('positive Desktop provider notice',
                provider_ready, startup, process=core)
            enrollment = next(json.loads(p.read_text()) for p in native.glob('module-*.json')
                if json.loads(p.read_text())['real'] == str(build / binaries[1]))
            desktop_identity = child_identity(enrollment['child'], enrollment['proxy'])
            assert desktop_identity
            evidence['desktop_identity'] = desktop_identity
            control = Connection(endpoint)
            control.record()
            control.send(b'set echo\n')
            control.echo('desktop-applications-ready')
            serial = 0

            def command(text):
                nonlocal serial
                serial += 1
                control.send((text + '\nSendToModule FvwmDesktop barrier apps-%d\n' % serial).encode())
                control.expect_echo('apps-%d' % serial)

            def settings():
                control.send(b'SendToModule FvwmDesktop query-settings apps-theme all\n')
                values = {}
                for _ in range(128):
                    text = control.record().get('echo', {}).get('message', '')
                    if text == 'desktop-settings-apps-theme done':
                        return values if all(values.get(k, {}).get('effective') == v
                            for k, v in [('palette', palette), ('appearance', appearance),
                                         ('transparency', False)]) else None
                    if text.startswith('desktop-settings-apps-theme '):
                        _, key, value = text.split(' ', 2)
                        values[key] = json.loads(value)
                raise AssertionError('theme settings response exceeds bound')

            evidence['theme'] = wait_for_predicate('actual requested theme', settings, startup, process=core)
            client = connect_client(display)
            errors = SessionXErrors(client, evidence).install()
            view = NativeView(client, errors, core)

            def apps_result(after=0):
                candidates = [json.loads(path.read_text()) for path in jobs.glob('*.json')]
                candidates = [r for r in candidates if r['request']['operation'] == 'apps' and
                    r['started'] >= after and r.get('returncode') == 0]
                if not candidates:
                    return None
                result = max(candidates, key=lambda r: r['started'])
                assert not result['response'].get('error'), result
                return result

            initial = wait_for_predicate('actual Applications worker result', apps_result,
                time.monotonic() + 6, process=core)
            applications = initial['response']['result']
            names = [item['name'] for item in applications]
            assert NAME in names and ACTION in names and NAME + ' — Récent' in names, names
            assert 'Priorité utilisateur' in names and 'Programme disparu' in names
            assert all(not any(word in name for word in ('forbidden', 'Rejected', 'Wrong', 'Unlisted', 'Bad action'))
                for name in names), names
            actions = [item for item in applications if item['name'].startswith(NAME + ' — ')]
            assert 2 < len(actions) <= 8, actions
            primary = next(item for item in applications if item['name'] == NAME)
            action = next(item for item in applications if item['name'] == ACTION)
            expected_main = [str(application), 'main', '', 'two words', 'semi; & literal',
                '$(touch ' + str(marker) + ')', '`touch ' + str(marker) + '`', '%', NAME,
                '--icon', 'parent icon', str(entry)]
            expected_action = [str(application), 'inspect', NAME, '--icon', 'action icon', str(entry)]
            assert primary == dict(name=NAME, argv=expected_main, cwd=str(cwd), terminal=False), primary
            assert action == dict(name=ACTION, argv=expected_action, cwd=str(cwd), terminal=False), action
            assert next(item for item in applications if item['name'] == 'Terminal maîtrisé')['terminal'] is True
            recent = next(item for item in applications if item['name'] == NAME + ' — Récent')
            assert recent['argv'] == [str(application), 'recent', NAME, '--icon', 'parent icon', str(entry)]
            evidence['cases']['actual_worker_policy'] = dict(record=initial, expected_main=expected_main,
                expected_action=expected_action, action_count=len(actions))

            def committed(label):
                current = menu_rows(native, MENU)
                return current if any('"' + label + '" SendToModule FvwmDesktop launch-bound ' in row
                    for row in current) else None

            current = wait_for_predicate('committed localized Applications', lambda: committed(ACTION),
                time.monotonic() + 6, process=core)

            def callback(label, commands=None):
                commands = menu_rows(native, MENU) if commands is None else commands
                row = next(row for row in commands if '"' + label + '" SendToModule ' in row)
                return row.split(' SendToModule FvwmDesktop ', 1)[1]

            def mapped_menu():
                return next((w for w in view.children(client.root) if
                    (a := view.attributes(w)) and a.map_state == 2 and a.save_under and
                    a.width > 20 and a.height > 20), None)

            def open_menu():
                view.pointer.motion(1000, 80)
                control.send(b'SlateLauncher\n')
                return wait_for_predicate('mapped localized Applications menu', mapped_menu,
                    time.monotonic() + 4, process=core)

            def capture(name):
                result = subprocess.run([sys.executable, str(fixture), '--capture',
                    str(build / binaries[-1]), str(out)], env=env, capture_output=True,
                    text=True, timeout=40, check=True)
                path = Path(json.loads(result.stdout))
                target = out / name
                path.replace(target)
                evidence['screenshots'][name] = digest(target)

            def dismiss():
                view.key('Escape')
                wait_for_predicate('Applications menu released', lambda: not view.pointer.grabbed(),
                    time.monotonic() + 4, process=core)

            def activate(label):
                window = open_menu()
                selectable = [row for row in menu_rows(native, MENU) if
                    ' Title' not in row and not row.endswith(' Nop') and ' Greyed ' not in row]
                index = next(i for i, row in enumerate(selectable) if '"' + label + '" SendToModule ' in row)
                view.key('Home')
                for _ in range(index):
                    view.key('Down')
                view.key('Return')
                wait_for_predicate('native application row selected', lambda: not view.pointer.grabbed(),
                    time.monotonic() + 4, process=core)
                return window

            def application_records():
                paths = sorted(records.glob('application-*.json'))
                assert len(paths) <= 8
                return [json.loads(path.read_text()) for path in paths]

            def observed_launch(expected, count, matches=1):
                value = wait_for_predicate('owned application record',
                    lambda: application_records() if len(application_records()) == count else None,
                    time.monotonic() + 5, process=core)
                matching = [r for r in value if r['literal_argv_bytes'] == encoded(expected)]
                assert len(matching) == matches, value
                record = max(matching, key=lambda r: r['recorded'])
                assert record['cwd_bytes'] == encoded([str(cwd)])[0], record
                assert record['identity']['parent'] == desktop_identity['child']
                assert record['parent_identity'] == {k: desktop_identity[k] for k in ('child', 'parent', 'starttime')}
                assert child_identity(enrollment['child'], enrollment['proxy']) == desktop_identity

                def reaped():
                    identity = process_identity(record['identity']['child'])
                    return identity is None or identity['starttime'] != record['identity']['starttime']

                wait_for_predicate('application reaped while Desktop alive', reaped,
                    time.monotonic() + 5, process=core)
                return record

            window = open_menu()
            attributes = view.attributes(window)
            evidence['menu_geometry'] = dict(width=attributes.width, height=attributes.height)
            capture('applications-localized.png')
            assert attributes.width <= 1440 and attributes.height <= 1200
            evidence['menu_pixels'] = view.pixels(window)
            dismiss()
            activate(NAME)
            evidence['cases']['native_main_launch'] = observed_launch(expected_main, 1)
            activate(ACTION)
            evidence['cases']['native_action_launch'] = observed_launch(expected_action, 2)
            old_callback = callback(ACTION, current)
            refresh_started = time.monotonic()
            editor('Inspection renouvelée')
            shutil.copy2(entry, out / 'editor-refreshed.desktop')
            evidence['refreshed_desktop_sha256'] = digest(entry)
            command('SendToModule FvwmDesktop discover')
            refreshed = NAME + ' — Inspection renouvelée'
            result = wait_for_predicate('refreshed actual Applications worker',
                lambda: apps_result(refresh_started), time.monotonic() + 6, process=core)
            new_rows = wait_for_predicate('new committed action generation', lambda: committed(refreshed),
                time.monotonic() + 6, process=core)
            assert callback(refreshed, new_rows).split()[1:3] != old_callback.split()[1:3]

            def notice(fragment, after):
                prefix = '*SlateNoticeForm: TextHex x'
                text = ''.join(bytes.fromhex(r['text'][len(prefix):]).decode() for r in rows(native)
                    if r['direction'] == 'out' and r['time'] >= after and r['text'].startswith(prefix))
                return text if fragment in text else None

            stale_started = time.monotonic()
            command('SendToModule FvwmDesktop ' + old_callback)
            stale = wait_for_predicate('stale action notice',
                lambda: notice('Application menu is stale', stale_started), time.monotonic() + 5, process=core)
            view.wait('FVWM-X')
            capture('applications-stale-action.png')
            command('KillModule FvwmForm SlateNoticeForm')
            assert len(application_records()) == 2
            window = open_menu()
            capture('applications-refreshed.png')
            dismiss()
            activate(refreshed)
            # The action label changed; its parent %c context and argv did not.
            current_record = observed_launch(expected_action, 3, matches=2)
            evidence['cases']['stale_then_current'] = dict(stale_notice=stale,
                old_callback=old_callback, current_callback=callback(refreshed), worker=result,
                current_record=current_record)
            missing.unlink()
            failed_started = time.monotonic()
            activate('Programme disparu')
            failed = wait_for_predicate('real missing executable notice',
                lambda: notice('Could not start program:', failed_started), time.monotonic() + 5, process=core)
            view.wait('FVWM-X')
            capture('applications-missing-executable.png')
            command('KillModule FvwmForm SlateNoticeForm')
            assert len(application_records()) == 3
            evidence['cases']['missing_executable_after_commit'] = dict(notice=failed,
                executable=str(missing), application_records=3)
            window = open_menu()
            capture('applications-after-failure.png')
            dismiss()
            assert not marker.exists(), 'Desktop Exec unexpectedly invoked a shell'
            evidence['cases']['literal_no_shell_execution'] = True
            evidence['status'] = 'passed'
        except BaseException as failure:
            evidence.update(status='failed', failure=repr(failure))
            raise
        finally:
            def clean(name, operation):
                try:
                    operation()
                except BaseException as failure:
                    evidence['status'] = 'failed'
                    evidence['cleanup_failures'].append(dict(step=name, error=repr(failure)))

            def reaped_applications():
                receipts = [json.loads(path.read_text()) for path in records.glob('application-*.json')]
                for record in receipts:
                    def gone(record=record):
                        current = process_identity(record['identity']['child'])
                        return current is None or current['starttime'] != record['identity']['starttime']
                    wait_for_predicate('owned record-and-exit program reaped', gone, time.monotonic() + 5)
                evidence['applications_before_core_exit'] = receipts

            clean('owned application cleanup before core exit', reaped_applications)
            if control:
                clean('MFL observer', control.close)
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
                def module_waits():
                    evidence['module_cleanup'] = cleanup_receipts(runtime.root / 'native-records')
                clean('actual native child Wait receipts', module_waits)
            for path in runtime.root.iterdir():
                if path.is_dir() and path.name in ('native-records', 'desktop-workers', 'application-records'):
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
    if len(sys.argv) > 1 and sys.argv[1] == '--application':
        return record_application(sys.argv[2], sys.argv[3], sys.argv[4:])
    if len(sys.argv) == 4 and sys.argv[1] == '--capture':
        print(json.dumps(NativeCapture(sys.argv[2]).screenshot(
            sys.argv[3]).removeprefix('Screenshot saved: ')))
        return 0
    parser = argparse.ArgumentParser(description=__doc__, allow_abbrev=False)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--profiles', nargs='+', choices=PROFILES, default=list(PROFILES))
    parser.add_argument('--artifacts-dir', type=Path, required=True)
    args = parser.parse_args()
    if sys.platform != 'linux':
        parser.error('run only in the owning Linux guest')
    args.artifacts_dir.mkdir(parents=True, exist_ok=True)
    run_root = Path(tempfile.mkdtemp(prefix='desktop-applications-', dir=args.artifacts_dir.resolve()))
    print(json.dumps(dict(artifacts_dir=str(run_root))), flush=True)
    for profile in args.profiles:
        run(args, profile, run_root)
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
