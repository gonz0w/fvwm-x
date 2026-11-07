#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or any later version.
"""Focused semantic-color reload with surviving editor/browser identities.

Uses the existing native module relay, X witnesses and settings/files IPC.
Preview failure/race policy remains with the affected Go tests; this workload
covers actual recoloring, source refusal, saved state and an unsubmitted draft.
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
import subprocess
import sys
import time
import traceback

from desktop_live import connect_client
from inspector_live import NativeView, PROFILES, form_text, inspector_frame
from live_x_harness import HarnessRuntime, start_display_server, wait_for_predicate
from mfl_request_live import frame
from mfl_stream_live import Connection
from settings_preview_live import PreviewWitness
from settings_snapshot import expanded_settings
from session_x_errors import SessionXErrors

PENDING = 'FvwmDesktop: theme colors reload pending.'
SUCCESS = ('FvwmDesktop: theme colors reloaded; wallpaper and other '
    'components unchanged.')


def run(args, profile):
    source, build = Path(__file__).resolve().parents[2], args.build.resolve()
    backer = build / 'modules/FvwmBacker/FvwmBacker'
    image_worker = build / 'bin/fvwm-image-svg-worker'
    browser_executable = build / 'bin/FvwmPrompt/fvwm-file-browser'
    out = args.artifacts_dir.resolve() / profile
    out.mkdir(parents=True, exist_ok=False)
    evidence = dict(status='running', profile=profile, screenshots=[],
        scope='semantic color reload; preview race policy uses Go tests')
    with HarnessRuntime(prefix='fvwm-theme-reload-') as runtime:
        control = rpc = client = errors = pointer = core = browser = server = None
        native, config = runtime.root / 'native', runtime.root / 'config'
        try:
            evidence['stage'] = 'required fixture executables'
            evidence['required_executables'] = {}
            for name, executable in (('backer', backer),
                    ('image_worker', image_worker),
                    ('file_browser', browser_executable)):
                assert executable.is_file() and os.access(executable, os.X_OK), (
                    'required executable unavailable: ' + str(executable))
                evidence['required_executables'][name] = str(executable)
            server, display = start_display_server(runtime, 'Xvfb',
                ['Xvfb', '-screen', '0', '1440x1200x24', '-noreset'],
                'xserver.log', time.monotonic() + 10)
            user, modules = runtime.root / 'user', runtime.root / 'modules'
            native, root = runtime.root / 'native', runtime.root / 'theme'
            for directory in (user, modules, native):
                directory.mkdir(mode=0o700)
            shutil.copytree(source / 'default-config', root)
            for directory in (build / 'modules').iterdir():
                executable = directory / directory.name
                if executable.is_file():
                    (modules / directory.name).symlink_to(executable)
            desktop = build / 'bin/FvwmPrompt/FvwmDesktop'
            for name, executable in (('FvwmDesktop', desktop),
                    ('FvwmForm', build / 'modules/FvwmForm/FvwmForm')):
                wrapper = modules / name
                wrapper.unlink(missing_ok=True)
                wrapper.write_text('#!/bin/sh\nexec ' + shlex.join([
                    sys.executable, '-E', '-S', '-B',
                    str(source / 'tests/live_x/native_module_proxy.py'),
                    str(executable), str(native)]) + ' "$@"\n')
                wrapper.chmod(0o700)
            palette, phase = ('dune', 'light') if profile == 'dune-light' else ('slate', 'dark')
            # Independently inspected shipped PNG corner samples distinguish
            # the selected asset from a valid old root or solid placeholder.
            wallpaper_samples = {
                ('slate', 'dark'): (
                    '662a472394348a9a0c717035e770109a7'
                    '84991d7149a03146a7e76aa2a59ff87',
                    [0x1c2c34, 0x2c4c53, 0x1c2c34, 0x2c4c53]),
                ('dune', 'light'): (
                    '3bdc776292b00122746304285ac113568'
                    '66675dcccd787029aea6e30adb9163a',
                    [0xe4d9ca, 0xd5c5b3, 0xe4d9ca, 0xd5c5b3]),
            }
            asset_hash, expected_corners = wallpaper_samples[palette, phase]
            wallpaper = root / 'wallpapers' / (palette + '-' + phase + '.png')
            assert hashlib.sha256(wallpaper.read_bytes()).hexdigest() == asset_hash
            evidence['wallpaper_fixture'] = dict(
                asset=str(wallpaper.relative_to(root)), sha256=asset_hash,
                expected_corner_pixels=expected_corners)
            included = user / 'theme-reload-included.config'
            included.write_text('*FvwmDesktop: motion Off\n')
            (user / 'local.config').write_text('*FvwmDesktop: palette ' + palette +
                '\nRead "' + str(included) + '"\n*FvwmDesktop: appearance ' + phase +
                '\n*FvwmDesktop: transparency Off\n')
            body = (root / 'config').read_text().replace(
                '$[FVWM_DATADIR]/default-config', str(root))
            if profile == 'classic-large':
                body = body.replace('Sans:size=10:antialias=True',
                    'DejaVu Sans Mono:size=18:antialias=True')
            legacy = ('*FvwmFormDefault: Fore wheat\n*FvwmFormDefault: Back navy\n'
                '*FvwmFormDefault: ItemFore black\n*FvwmFormDefault: ItemBack ivory\n'
                if profile == 'classic-large' else '')
            config = runtime.root / 'config'
            config.write_text('ModulePath ' + str(modules) + '\nRead ' +
                str(source / 'fvwm/ConfigFvwmDefaults') + '\n' + body + legacy +
                '\nStyle SlateSettingsForm PositionPlacement 20 80\n'
                'Style FvwmFiles-* PositionPlacement 650 100\n'
                'AddToFunc StartFunction I Module FvwmMFL\n')
            env = runtime.environment({'DISPLAY': display,
                'HOME': str(runtime.root), 'FVWM_USERDIR': str(user),
                'FVWM_DESKTOP_ROOT': str(root),
                'FVWM_DESKTOP_WORKER': str(build / 'bin/FvwmPrompt/fvwm-desktop-worker'),
                'FVWM_IMAGE_SVG_WORKER': str(image_worker),
                'FVWMMFL_SOCKET_PATH': str(runtime.root),
                'XDG_DATA_HOME': str(runtime.root / 'data'),
                'XDG_DATA_DIRS': str(runtime.root / 'absent')})
            core = runtime.start('core', [str(build / 'fvwm3'), '-d', display,
                '-f', str(config)], 'core.log', env)
            endpoint = runtime.root / ('fvwm_mfl_' + display.replace(':', '_') + '.sock')
            wait_for_predicate('theme reload MFL', endpoint.exists,
                time.monotonic() + 10, process=core)
            control, rpc = Connection(endpoint), Connection(endpoint)
            for connection in (control, rpc):
                connection.record()
            control.send(b'set echo\n')
            serial = 0

            def wait(label, predicate):
                evidence['stage'] = label
                try:
                    return wait_for_predicate(label, predicate,
                        time.monotonic() + 10, process=core)
                except BaseException:
                    evidence['wait_failure'] = dict(stage=label,
                        core_poll=core.poll(),
                        browser_poll=browser.poll() if browser else None)
                    raise

            def request(resource='settings', **fields):
                nonlocal serial
                serial += 1
                token = 'theme-reload-%d' % serial
                rpc.send(frame(token, resource=resource, **fields))
                value = rpc.record()['reply']
                assert value['id'] == token, value
                if value['status'] == 'error' and value['error']['code'] == 'not_ready':
                    return None
                assert value['status'] == 'ok', value
                return expanded_settings(value['result']) if resource == 'settings' else value['result']

            def barrier():
                nonlocal serial
                serial += 1
                token = 'theme-reload-barrier-' + str(serial)
                control.send(('SendToModule FvwmDesktop barrier ' + token +
                    '\n').encode())
                control.expect_echo(token)

            def ready():
                value = request(scope='all')
                return value if value and value['settings']['palette']['application'] == 'submitted-to-fvwm' else None

            before = wait('desktop theme ready', ready)
            assert before['settings']['palette']['effective'] == palette
            assert before['settings']['appearance']['effective'] == phase
            assert before['settings']['desktop_font']['saved'] == ''
            client = connect_client(display)
            errors = SessionXErrors(client, evidence).install()
            view = NativeView(client, errors, core)
            pointer, x, d = view.pointer, client.x, client.display
            witness = PreviewWitness(client)
            bar = view.wait('SlateBar')

            def pixel():
                image = x.XGetImage(d, bar, 5, 5, 1, 1,
                    C.c_ulong(-1).value, 2)
                assert image
                try:
                    actual = int(x.XGetPixel(image, 0, 0))
                    evidence['last_panel_pixel'] = actual
                    return actual
                finally:
                    x.XDestroyImage(image)

            def root_state():
                # Valid old roots can coexist with a pending replacement.
                # Query the composition fixture's submitted-resource gates,
                # then require the actual selected asset before the baseline.
                appearance = request('appearance')
                colorsets = request('colorsets', target='20')
                if appearance is None or colorsets is None:
                    return None
                capability = appearance['complete_bundle_preview']
                colorset = colorsets['colorsets'][0]
                diagnostic = dict(appearance=appearance, colorset20=colorset,
                    expected_corner_pixels=expected_corners)
                evidence['last_root_readiness'] = diagnostic
                if (not capability['root_owned'] or
                        colorset['update_state'] != 'idle'):
                    return None
                try:
                    observed = witness.root_state()
                except AssertionError as error:
                    diagnostic['root_error'] = repr(error)
                    return None
                diagnostic['observed_root'] = observed
                if observed['image']['corner_pixels'] != expected_corners:
                    return None
                return observed

            palettes_path = root / 'palettes.json'
            original = json.loads(palettes_path.read_text())
            normal_surface = int(original[palette][phase]['surface'][1:], 16)
            evidence['expected_initial_panel_pixel'] = normal_surface
            wait('initial actual panel surface', lambda: pixel() == normal_surface)
            folder = runtime.root / 'files'
            folder.mkdir()
            for name in ('alpha', 'beta', 'gamma'):
                (folder / name).touch()
            browser = runtime.start('browser', [str(browser_executable),
                '--socket', str(endpoint),
                '--view', 'grid', '--reveal', str(folder / 'beta')],
                'browser.log', env)
            providers = wait('browser provider', lambda:
                (value['providers'] if (value := request('files')) else None))
            assert len(providers) == 1, providers
            provider = providers[0]['provider']

            def browser_state():
                value = request('files', provider=provider, scope='state')
                return value if value and not value['busy'] and value['availability']['ready'] and value['bindings']['projection']['state'] == 'current' else None

            browser_before = wait('browser native projection', browser_state)
            assert browser_before['loaded'] == 3 and browser_before['selected'] == 1
            browser_window = view.wait('File browser')
            browser_children = sorted(view.children(browser_window))
            control.send(b'SendToModule FvwmDesktop edit-settings group:desktop\n')
            editor = view.wait('Desktop component settings')
            editor_children = sorted(view.children(editor))
            control.send(('WindowId %d Raise\nWindowId %d Focus NoWarp\n' %
                (editor, editor)).encode())
            wait('actual component editor focus', lambda: view.focus() == editor)
            # The first component input is the initially empty desktop font.
            # A space is an unsubmitted draft invalid under FontCommandSafe.
            view.key('space')
            saved = user / 'desktop-state.json'
            saved_before = saved.read_bytes() if saved.exists() else None
            before = request(scope='all')

            def outgoing(real, since):
                rows = []
                identities = list(native.glob('module-*.json'))
                assert len(identities) <= 64
                for identity in identities:
                    assert identity.stat().st_size <= 16384
                    metadata = json.loads(identity.read_text())
                    if metadata['real'] != str(real):
                        continue
                    log = identity.with_suffix('.jsonl')
                    if not log.exists():
                        continue
                    assert log.stat().st_size <= 8388608
                    for line in log.read_text().splitlines(keepends=True):
                        if not line.endswith('\n'):
                            continue
                        row = json.loads(line)
                        if row.get('direction') == 'out' and row['time'] >= since:
                            rows.append(row)
                            assert len(rows) <= 16384
                return sorted(rows, key=lambda value: value['time'])

            def retained():
                current = request(scope='all')
                assert current['provider'] == before['provider']
                assert current['generation'] == before['generation']
                assert current['saved_revision'] == before['saved_revision']
                for key, old in before['settings'].items():
                    for field in ('saved', 'effective', 'source'):
                        assert current['settings'][key][field] == old[field], (key, field)
                assert (saved.read_bytes() if saved.exists() else None) == saved_before
                assert view.named('Desktop component settings') == editor
                assert sorted(view.children(editor)) == editor_children
                assert view.named('File browser') == browser_window
                assert sorted(view.children(browser_window)) == browser_children
                current_browser = wait('retained browser projection', browser_state)
                for key in ('provider', 'generation', 'loaded', 'visible', 'selected'):
                    assert current_browser[key] == browser_before[key], key
                assert browser.poll() is None
                return current

            barrier()
            initial_root = wait('initial selected owned wallpaper', root_state)
            evidence['initial_root'] = initial_root
            changed = json.loads(json.dumps(original))
            selected = changed[palette][phase]
            selected['surface'] = '#ded4ec' if phase == 'light' else '#34304d'
            selected['raised'] = '#cec2e0' if phase == 'light' else '#494064'
            selected['background'] = '#ff00ff'  # Explicitly outside reload scope.
            palettes_path.write_text(json.dumps(changed) + '\n')
            since = time.monotonic()
            control.send(b'SendToModule FvwmDesktop reload-theme\n')
            control.expect_echo(PENDING)
            control.expect_echo(SUCCESS)
            changed_surface = int(selected['surface'][1:], 16)
            evidence['expected_changed_panel_pixel'] = changed_surface
            wait('same-name semantic surface changed', lambda: pixel() == changed_surface)
            after = retained()
            evidence['stage'] = 'unchanged wallpaper after valid reload'
            current_root = witness.root_state()
            evidence['root_after_reload'] = current_root
            assert current_root == initial_root
            roles = [row['text'] for row in outgoing(desktop, since)
                if row.get('text', '').startswith('Colorset ')]
            assert [int(row.split()[1]) for row in roles] == list(range(9)) + list(range(10, 15)), roles
            assert view.focus() == editor, 'reload stole editor focus'
            evidence['success'] = dict(before_surface=normal_surface,
                after_surface=changed_surface, roles=roles,
                settings_generation=after['generation'],
                saved_revision=after['saved_revision'],
                editor=editor, editor_children=editor_children,
                browser=browser_window, browser_children=browser_children,
                browser_provider=provider, browser_generation=browser_before['generation'],
                root_state=initial_root)

            def capture(name):
                completed = subprocess.run([str(build / 'bin/fvwm-desktop-capture'),
                    str(out)], env=env, capture_output=True, text=True, timeout=35)
                assert completed.returncode == 0, completed.stderr
                path = Path(completed.stdout.strip().removeprefix('Screenshot saved: '))
                path.replace(out / name)
                evidence['screenshots'].append(name)

            capture('theme-reload-' + profile + '.png')
            log = runtime.root / 'core.log'
            log_offset = log.stat().st_size
            palettes_path.write_text('{"slate":{}}\n')
            since = time.monotonic()
            control.send(b'SendToModule FvwmDesktop reload-theme\n')
            control.expect_echo(PENDING)
            def refused():
                assert log.stat().st_size <= 16777216
                text = log.read_bytes()[log_offset:].decode('utf-8', 'replace')
                return text if 'Theme reload refused: ' in text and str(palettes_path) in text else None
            diagnostic = wait('source-aware palette refusal', refused)
            assert pixel() == changed_surface
            evidence['stage'] = 'unchanged wallpaper after refused reload'
            current_root = witness.root_state()
            evidence['root_after_refusal'] = current_root
            assert current_root == initial_root
            retained()
            assert not any(row.get('text', '').startswith('Colorset ')
                for row in outgoing(desktop, since))
            assert view.focus() == editor, 'refusal stole editor focus'
            evidence['refusal'] = dict(diagnostic=diagnostic,
                surface_retained=changed_surface, editor_retained=editor)
            # Apply the invalid draft only after both reload outcomes. Observe
            # the real Form callback, then confirm validation retains all state.
            since = time.monotonic()
            view.key('F3')
            def submitted():
                return next((row for row in outgoing(build /
                    'modules/FvwmForm/FvwmForm', since) if row.get('text', '').startswith(
                    'SendToModule FvwmDesktop apply-settings ')), None)
            callback = wait('actual surviving editor draft callback', submitted)
            field = callback['text'].split()[4]
            assert field.startswith('x') and base64.urlsafe_b64decode(field[1:] + '===') == b' ', callback
            def draft_refused():
                for row in outgoing(desktop, since):
                    text = row.get('text', '')
                    prefix = '*SlateNoticeForm: TextHex x'
                    if text.startswith(prefix) and bytes.fromhex(text[len(prefix):]).decode() == 'Invalid value for desktop_font':
                        return True
                return None
            wait('actual invalid draft validation', draft_refused)
            retained()
            evidence['draft'] = dict(callback=callback['text'],
                bytes_hex='20', window=editor, rejected_without_persistence=True)
            palettes_path.write_text(json.dumps(changed) + '\n')
            if profile == 'slate-dark':
                notice = view.named('FVWM-X')
                if notice:
                    control.send(('WindowId %d Close\n' % notice).encode())
                    wait('fixture validation notice retired',
                        lambda: view.named('FVWM-X') is None)
                inspection_since = time.monotonic()
                control.send(b'SendToModule FvwmDesktop inspect sources\n')

                def inspection_page(required):
                    lines, published = [], None
                    for row in outgoing(desktop, inspection_since):
                        text = row.get('text', '')
                        if text == 'DestroyModuleConfig SlateInspectorForm:*':
                            lines = []
                        elif text.startswith('*SlateInspectorForm:'):
                            lines.append(text)
                        elif text == 'Module FvwmForm SlateInspectorForm':
                            decoded = form_text(lines)
                            if 'Snapshot scope: sources' in decoded and required in decoded:
                                published = dict(lines=list(lines), text=decoded,
                                    frame=inspector_frame(lines), time=row['time'])
                    if not published:
                        return None
                    active = []
                    for path in native.glob('module-*.json'):
                        metadata = json.loads(path.read_text())
                        if metadata['real'] != str(build / 'modules/FvwmForm/FvwmForm') or 'SlateInspectorForm' not in metadata['arguments']:
                            continue
                        token = path.stem.removeprefix('module-')
                        if not (native / ('stopped-' + token + '.json')).exists():
                            active.append(metadata)
                    if len(active) != 1 or active[0]['started'] < published['time']:
                        return None
                    window = view.named('Configuration inspector')
                    if window is None:
                        return None
                    return dict(published, window=window, module=active[0])

                first = wait('actual sources worker and initial native page',
                    lambda: inspection_page('Coverage'))
                control.send(('WindowId %d Raise\nWindowId %d Focus NoWarp\n' %
                    (first['window'], first['window'])).encode())
                wait('actual inspector page focus',
                    lambda: view.focus() == first['window'])
                page_since = time.monotonic()
                view.key('F2')
                expected_callback = ('SendToModule FvwmDesktop inspect-control ' +
                    first['frame'] + ' page 1')
                wait('real inspector Next callback', lambda: next((row for row in
                    outgoing(build / 'modules/FvwmForm/FvwmForm', page_since)
                    if row.get('text') == expected_callback), None))
                ancestry = wait('actual include ancestry native page',
                    lambda: inspection_page('user:local.config:2'))
                assert 'include 1/1' in ancestry['text']
                capture('theme-reload-include-ancestry.png')
                evidence['include_ancestry'] = ancestry
            control.send(b'Quit\n')
            assert core.wait_until_exit(time.monotonic() + 5) == 0
            evidence['status'] = 'passed'
        finally:
            failure = sys.exc_info()[1]
            if failure is not None:
                evidence.update(status='failed', failure=repr(failure),
                    failure_traceback=traceback.format_exception(
                        type(failure), failure, failure.__traceback__))
            cleanup_errors = evidence.setdefault('cleanup_errors', [])

            def attempt(label, operation):
                try:
                    operation()
                except BaseException as error:
                    cleanup_errors.append(dict(operation=label,
                        error=repr(error)))

            def retain(phase):
                # Save the original Python failure before any native X call
                # or process retirement can terminate this interpreter.
                evidence['cleanup_phase'] = phase
                temporary = out / 'evidence.json.tmp'
                temporary.write_text(json.dumps(evidence, indent=2) + '\n')
                temporary.replace(out / 'evidence.json')
                for path in runtime.root.glob('*.log'):
                    shutil.copy2(path, out / path.name)
                if native.exists():
                    shutil.copytree(native, out / 'native', dirs_exist_ok=True)
                if config.exists():
                    (out / 'configuration.fvwm').write_bytes(config.read_bytes())

            evidence['children_before_cleanup'] = {
                process.name: process.poll() for process in runtime.processes}
            attempt('retain before teardown', lambda: retain('before-teardown'))
            # The observer borrows this server. Closing Xlib after Xvfb exits
            # invokes its fatal I/O path, bypassing Python's exception cleanup.
            server_alive = server is not None and server.poll() is None
            if server_alive:
                if pointer:
                    attempt('release observer pointer', lambda: pointer.button(False))
                if errors:
                    attempt('close observer display', errors.close_display)
                elif client:
                    attempt('close observer client', client.close)
            elif client:
                cleanup_errors.append(dict(operation='close observer display',
                    error='skipped native X calls: owned server already exited'))
            if core and core.poll() is None and control:
                attempt('request owned core shutdown', lambda: control.send(b'Quit\n'))
                attempt('wait for owned core shutdown',
                    lambda: core.wait_until_exit(time.monotonic() + 4))
            for connection in (control, rpc):
                if connection:
                    attempt('close observer IPC connection', connection.close)
            # Retire the owned clients/modules while their server remains
            # available. ManagedProcess.close owns bounded fallback teardown.
            for process in reversed(runtime.processes):
                if process is not server:
                    attempt('close ' + process.name,
                        lambda process=process: process.close(time.monotonic() + 4))
            if errors and errors.unexpected:
                evidence['status'] = 'failed'
            attempt('retain before server teardown',
                lambda: retain('before-server-teardown'))
            if server:
                attempt('close owned X server',
                    lambda: server.close(time.monotonic() + 4))
            evidence['children_after_cleanup'] = {
                process.name: process.poll() for process in runtime.processes}
            if cleanup_errors:
                evidence['status'] = 'failed'
            attempt('retain final diagnostics', lambda: retain('complete'))
            if failure is None and cleanup_errors:
                raise AssertionError('theme reload cleanup failed: ' +
                    repr(cleanup_errors))
    assert not evidence['observer_unexpected_x_errors'], evidence
    print('focused theme reload ' + profile + ' passed')


def main():
    parser = argparse.ArgumentParser(description=__doc__, allow_abbrev=False)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--artifacts-dir', type=Path, required=True)
    parser.add_argument('--profiles', nargs='+', choices=PROFILES,
        default=list(PROFILES))
    args = parser.parse_args()
    def deadline(signum, frame_):
        raise TimeoutError('focused theme reload exceeded 180 seconds')
    signal.signal(signal.SIGALRM, deadline)
    signal.alarm(180)
    for profile in args.profiles:
        run(args, profile)
    signal.alarm(0)


if __name__ == '__main__':
    main()
