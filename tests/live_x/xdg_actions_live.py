#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
"""Observe installed XDG Desktop Actions through actual themed native menus.

Use --build INSTALLED_VIEW --provider INSTALLED_PROVIDER --artifacts-dir OUT.
Only owned record-and-exit applications run. The provider creates real private
launch records and native commands; the fixture never invents a provider result.
Old exported menu references intentionally retain their catalog records.
"""
import argparse
import json
import os
from pathlib import Path
import re
import shlex
import shutil
import struct
import subprocess
import sys
import tempfile
import time
import zlib

from browser_continuity_live import cleanup_receipts
from browser_handoff_live import process_identity
from desktop_applications_live import desktop_string, digest, encoded, record_application
from desktop_live import connect_client
from inspector_live import NativeView, atomic_json
from live_x_harness import HarnessRuntime, start_display_server, wait_for_predicate
from mfl_stream_live import Connection
from native_bindings_live import rows
from native_capture import NativeCapture
from native_module_proxy import module_proxy
from session_x_errors import SessionXErrors

PROFILES = ('slate-dark', 'dune-light', 'classic-large')
TITLE = 'XDGActionDemo'
NAME = 'Éditeur démo'
ACTION = NAME + ' — Inspecter'
COLORS = {'parent': (24, 166, 224), 'action': (232, 166, 43),
          'directory': (158, 73, 198)}


def icon_png(path, color):
    """Write a small deterministic fixture icon with an independent color oracle."""
    def chunk(kind, data):
        return struct.pack('!I', len(data)) + kind + data + struct.pack(
            '!I', zlib.crc32(kind + data) & 0xffffffff)
    row = b'\0' + bytes((*color, 255)) * 24
    path.write_bytes(b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack(
        '!IIBBBBB', 24, 24, 8, 6, 0, 0, 0)) + chunk(b'IDAT', zlib.compress(row * 24)) +
        chunk(b'IEND', b''))


def root_rows(text):
    result, active = [], False
    for line in text.splitlines():
        if line.startswith('AddToMenu "' + TITLE + '"'):
            active = True
        elif active and not line:
            break
        elif active and line.startswith('+ '):
            result.append(line)
    return result


def labelled_row(text, label):
    pattern = re.compile(r'^\+ "' + re.escape(label) + r'(?:%[^"%]+%)?" ')
    selected = [row for row in root_rows(text) if pattern.match(row)]
    assert len(selected) == 1, (label, selected, root_rows(text))
    return selected[0]


def generate(provider, output, arguments):
    # The surrounding HarnessRuntime session owns this wrapper, the provider
    # and its children. Stdout is data; diagnostics never become native config.
    with Path(output).open('xb') as commands:
        os.chmod(output, 0o600)
        return subprocess.run([provider, *arguments], stdout=commands,
            stderr=sys.stderr).returncode


def run(args, profile, root):
    source = Path(__file__).resolve().parents[2]
    build, provider = args.build.resolve(), args.provider.resolve()
    fixture, out = Path(__file__).resolve(), root / profile
    out.mkdir()
    evidence = dict(status='running', profile=profile, cases={}, screenshots={},
        cleanup_failures=[], limits=dict(full_install_release=False,
            terminal='inherited Terminal covered in independent CLI catalog test',
            old_records='stable standalone catalog references; no controller stale-action policy'))
    inputs = ['tests/live_x/xdg_actions_live.py', 'tests/live_x/desktop_applications_live.py',
        'tests/live_x/native_module_proxy.py', 'tests/live_x/native_bindings_live.py',
        'tests/live_x/live_x_harness.py', 'tests/live_x/inspector_live.py',
        'tests/live_x/native_capture.py', 'tests/live_x/session_x_errors.py']
    inputs += [str(p.relative_to(source)) for group in ('internal/xdgmenu',
        'internal/xdgsource', 'cmd/fvwmmenudesktop') for p in sorted(
        (source / 'bin/FvwmPrompt' / group).glob('*.go'))]
    evidence['source_sha256'] = {name: digest(source / name) for name in inputs}
    binaries = {'core': build / 'fvwm3', 'Desktop': build / 'bin/FvwmPrompt/FvwmDesktop',
        'worker': build / 'bin/FvwmPrompt/fvwm-desktop-worker',
        'MFL': build / 'modules/FvwmMFL/FvwmMFL',
        'Form': build / 'modules/FvwmForm/FvwmForm',
        'capture': build / 'bin/fvwm-desktop-capture', 'provider': provider}
    evidence['binaries'] = {name: dict(path=str(path.resolve()), sha256=digest(path))
        for name, path in binaries.items()}
    with HarnessRuntime(prefix='fvwm-xdg-actions-') as runtime:
        core = control = client = errors = None
        native = runtime.root / 'native-records'
        records = runtime.root / 'application-records'
        records.mkdir(mode=0o700)
        try:
            _, display = start_display_server(runtime, 'Xvfb',
                ['Xvfb', '-screen', '0', '1440x1200x24', '-noreset'],
                'xserver.log', time.monotonic() + 10)
            user, modules, native, tools, cwd = [runtime.root / name for name in
                ('user', 'modules', 'native-records', 'tools', 'working space')]
            apps = runtime.root / 'data/applications'
            directories = runtime.root / 'data/desktop-directories'
            menus, icons = runtime.root / 'config/menus', runtime.root / 'icons'
            for path in (user, modules, native, tools, cwd, apps, directories, menus, icons):
                path.mkdir(mode=0o700, parents=True)
            for directory in (build / 'modules').iterdir():
                binary = directory / directory.name
                if binary.is_file() and directory.name not in ('FvwmMFL', 'FvwmForm'):
                    (modules / directory.name).symlink_to(binary)

            def script(path, arguments, suffix=''):
                path.write_text('#!/bin/sh\nexec ' + shlex.join([
                    sys.executable, str(fixture), *map(str, arguments)]) + suffix + '\n')
                path.chmod(0o700)

            for name in ('FvwmDesktop', 'FvwmMFL', 'FvwmForm'):
                real = binaries[{'FvwmDesktop': 'Desktop', 'FvwmMFL': 'MFL', 'FvwmForm': 'Form'}[name]]
                script(modules / name, ['--module-proxy', real, native], ' "$@" 2>>' +
                    shlex.quote(str(runtime.root / (name + '.log'))))
            (tools / 'fvwm-menu-desktop').symlink_to(provider)
            application = tools / 'record application'
            script(application, ['--application', records, application], ' "$@"')
            icon_paths = {}
            for key, color in dict(COLORS, base=(37, 171, 71)).items():
                path = icons / (key + '.png')
                icon_png(path, color)
                icon_paths[key] = path
            evidence['fixture_icons'] = {key: dict(sha256=digest(path),
                rgb=list(dict(COLORS, base=(37, 171, 71))[key])) for key, path in icon_paths.items()}
            entry, marker = apps / 'editor.desktop', runtime.root / 'must-not-execute'

            def editor(verb='inspect', label='Inspecter'):
                command = ('"' + str(application) + '" main "" "two words" '
                    '"semi; & literal" "\\$(touch ' + str(marker) + ')" %% %c %i %k %F')
                body = ('[Desktop Entry]\nType=Application\nName=English editor\nName[fr]=' +
                    NAME + '\nIcon=' + str(icon_paths['base']) + '\nIcon[fr]=' +
                    str(icon_paths['parent']) + '\nPath=' + str(cwd).replace(' ', '\\s') +
                    '\nTerminal=false\nOnlyShowIn=FVWM;\nExec=' + desktop_string(command) +
                    '\nActions=Inspect;Recent;Bad;Inspect;Missing;\n' +
                    '[Desktop Action Inspect]\nName=Inspect\nName[fr]=' + label + '\nIcon=' +
                    str(icon_paths['base']) + '\nIcon[fr]=' + str(icon_paths['action']) +
                    '\nExec=' + desktop_string('"' + str(application) + '" ' + verb + ' %c %i %k') +
                    '\nTerminal=true\nPath=/not-parent\n' +
                    '[Desktop Action Recent]\nName=Recent\nName[fr]=Récent\nExec=' +
                    desktop_string('"' + str(application) + '" recent %c %i %k') +
                    '\n[Desktop Action Bad]\nName=Forbidden bad action\nExec=bad %Z\n' +
                    '[Desktop Action Unlisted]\nName=Forbidden unlisted\nExec=never\n')
                entry.write_text(body)

            editor()
            shutil.copy2(entry, out / 'editor-original.desktop')
            (apps / 'compat.desktop').write_text('[Desktop Entry]\nType=Application\n'
                'Name=Compatibilité\nOnlyShowIn=Compat;\nExec="' + str(application) + '" compat\n')
            (apps / 'excluded.desktop').write_text('[Desktop Entry]\nType=Application\n'
                'Name=Forbidden excluded\nNotShowIn=Compat;\nExec="' + str(application) + '" excluded\n')
            (apps / 'nested.desktop').write_text('[Desktop Entry]\nType=Application\n'
                'Name=Outil local\nExec="' + str(application) + '" nested\n')
            (directories / 'tools.directory').write_text('[Desktop Entry]\nType=Directory\n'
                'Name=Tools\nName[fr]=Atelier local\nIcon=' + str(icon_paths['base']) +
                '\nIcon[fr]=' + str(icon_paths['directory']) + '\n')
            menu = menus / 'applications.menu'
            menu.write_text('<Menu><Name>Applications</Name><DefaultAppDirs/>'
                '<DefaultDirectoryDirs/><Include><Filename>editor.desktop</Filename>'
                '<Filename>compat.desktop</Filename><Filename>excluded.desktop</Filename></Include>'
                '<Menu><Name>Tools</Name><Directory>tools.directory</Directory>'
                '<Include><Filename>nested.desktop</Filename></Include></Menu>'
                '<Layout><Filename>editor.desktop</Filename><Merge type="files"/>'
                '<Merge type="menus"/></Layout></Menu>')
            assets = source / 'default-config'
            palette, appearance = ('dune', 'light') if profile == 'dune-light' else ('slate', 'dark')
            local = user / 'local.config'
            local.write_text('*FvwmDesktop: palette ' + palette + '\n*FvwmDesktop: appearance ' +
                appearance + '\n*FvwmDesktop: transparency Off\n*FvwmDesktop: motion Off\n')
            body = (assets / 'config').read_text().replace('$[FVWM_DATADIR]/default-config', str(assets))
            if profile == 'classic-large':
                body = body.replace('Sans:size=10:antialias=True',
                    'DejaVu Sans Mono:size=18:antialias=True')
                body += '\n*FvwmFormDefault: Fore wheat\n*FvwmFormDefault: Back navy\n'
            config = runtime.root / 'config.fvwm'
            config.write_text('ModulePath ' + str(modules) + '\nRead ' +
                str(source / 'fvwm/ConfigFvwmDefaults') + '\n' + body +
                '\nAddToFunc StartFunction I Module FvwmMFL\n')
            env = runtime.environment(dict(DISPLAY=display, FVWM_USERDIR=str(user),
                FVWM_DESKTOP_ROOT=str(assets), FVWMMFL_SOCKET_PATH=str(runtime.root),
                FVWM_DESKTOP_WORKER=str(binaries['worker']), HOME=str(runtime.root),
                XDG_DATA_HOME=str(apps.parent), XDG_DATA_DIRS=str(runtime.root / 'absent-data'),
                XDG_CONFIG_HOME=str(menus.parent), XDG_CONFIG_DIRS=str(runtime.root / 'absent-config'),
                XDG_MENU_PREFIX='', XDG_CURRENT_DESKTOP='FVWM:Compat',
                LC_ALL='fr_FR.UTF-8', LANGUAGE='fr',
                PATH=str(tools) + os.pathsep + os.environ['PATH']))
            evidence['configuration_sha256'] = {name: digest(path) for name, path in
                [('config', config), ('local', local), ('default', assets / 'config'),
                 ('desktop', assets / 'desktop.fvwm')]}
            evidence['environment_inputs'] = {key: env[key] for key in ('LC_ALL', 'LANGUAGE',
                'XDG_CURRENT_DESKTOP', 'XDG_DATA_HOME', 'XDG_CONFIG_HOME')}
            core = runtime.start('core', [str(binaries['core']), '-d', display,
                '-f', str(config)], 'core.log', env)
            endpoint = runtime.root / ('fvwm_mfl_' + display.replace(':', '_') + '.sock')
            deadline = time.monotonic() + 12
            wait_for_predicate('XDG menu MFL endpoint', endpoint.exists, deadline, process=core)

            def provider_ready():
                for row in rows(native):
                    if row['direction'] == 'in' and row['kind'] == 1 << 22 and re.fullmatch(
                            r'MFL_PROVIDER_1 [0-9a-f]{32} [0-9a-f]{32}', row['text']):
                        return row
                return None

            evidence['positive_provider_notice'] = wait_for_predicate('positive provider notice',
                provider_ready, deadline, process=core)
            control = Connection(endpoint)
            control.record()
            control.send(b'set echo\n')
            control.echo('xdg-actions-ready')
            serial = 0

            def command(text):
                nonlocal serial
                serial += 1
                control.send((text + '\nSendToModule FvwmDesktop barrier xdg-%d\n' % serial).encode())
                control.expect_echo('xdg-%d' % serial)

            def theme():
                control.send(b'SendToModule FvwmDesktop query-settings xdg-theme all\n')
                values = {}
                for _ in range(128):
                    text = control.record().get('echo', {}).get('message', '')
                    if text == 'desktop-settings-xdg-theme done':
                        return values if all(values.get(k, {}).get('effective') == v for k, v in
                            [('palette', palette), ('appearance', appearance), ('transparency', False)]) else None
                    if text.startswith('desktop-settings-xdg-theme '):
                        _, key, value = text.split(' ', 2)
                        values[key] = json.loads(value)
                raise AssertionError('theme query exceeds bound')

            evidence['theme'] = wait_for_predicate('actual requested theme', theme, deadline, process=core)
            client = connect_client(display)
            errors = SessionXErrors(client, evidence).install()
            view = NativeView(client, errors, core)

            def produce(name):
                path = runtime.root / name
                arguments = ['--title', TITLE, '--enable-mini-icons',
                    '--size', '24', '--mini-icon-dir', str(runtime.root / 'icon-cache'),
                    '--include-items', 'none', '--desktop', 'applications', '--timeout', '8s']
                process = runtime.start('provider-' + name, [sys.executable, str(fixture),
                    '--generate', str(provider), str(path), *arguments], name + '.stderr', env)
                identity = process_identity(process.process.pid)
                try:
                    status = process.wait_until_exit(time.monotonic() + 15)
                    assert status == 0, process.log_path.read_text()
                finally:
                    process.close(time.monotonic() + 4)
                assert path.stat().st_size <= 4 * 1024 * 1024
                text = path.read_text()
                shutil.copy2(path, out / name)
                shutil.copy2(process.log_path, out / process.log_path.name)
                evidence.setdefault('provider_generations', []).append(dict(
                    arguments=arguments, wrapper_identity=identity, status=status,
                    commands_sha256=digest(path), diagnostics_sha256=digest(process.log_path)))
                assert 'Forbidden' not in text
                return path, text

            original_path, original = produce('menu-original.fvwm')
            main_row, action_row = labelled_row(original, NAME), labelled_row(original, ACTION)
            recent_row = labelled_row(original, NAME + ' — Récent')
            directory_row = labelled_row(original, 'Atelier local')
            compatibility = labelled_row(original, 'Compatibilité')
            original_rows = root_rows(original)
            assert original_rows.index(main_row) + 1 == original_rows.index(action_row)
            assert original_rows.index(action_row) + 1 == original_rows.index(recent_row)
            assert original_rows.index(recent_row) < original_rows.index(compatibility)
            assert original_rows.index(compatibility) < original_rows.index(directory_row)
            assert ' --launch ' in main_row and ' --launch ' in action_row
            assert main_row.split(' --launch ', 1)[1] != action_row.split(' --launch ', 1)[1]
            evidence['cases']['actual_provider_rows'] = dict(rows=original_rows,
                configuration_sha256=digest(menu), desktop_sha256=digest(entry))
            command('Read ' + json.dumps(str(original_path)))

            def mapped():
                return next((window for window in view.children(client.root) if
                    (a := view.attributes(window)) and a.map_state == 2 and a.save_under and
                    a.width > 20 and a.height > 20), None)

            def open_menu():
                view.pointer.motion(1000, 80)
                control.send(('Menu ' + TITLE + ' Monitor c c\n').encode())
                return wait_for_predicate('mapped XDG menu', mapped, time.monotonic() + 4, process=core)

            def capture(name):
                result = subprocess.run([sys.executable, str(fixture), '--capture',
                    str(binaries['capture']), str(out)], env=env, capture_output=True,
                    text=True, timeout=40, check=True)
                path = Path(json.loads(result.stdout))
                target = out / name
                path.replace(target)
                evidence['screenshots'][name] = digest(target)

            def dismiss():
                view.key('Escape')
                wait_for_predicate('menu released', lambda: not view.pointer.grabbed(),
                    time.monotonic() + 4, process=core)

            def activate(text, label):
                open_menu()
                selectable = [row for row in root_rows(text) if not row.endswith(' Nop') and
                    not row.endswith(' Title')]
                index = selectable.index(labelled_row(text, label))
                view.key('Home')
                for _ in range(index):
                    view.key('Down')
                view.key('Return')
                wait_for_predicate('native XDG action selected', lambda: not view.pointer.grabbed(),
                    time.monotonic() + 4, process=core)

            def observed(expected, count):
                def records_ready():
                    paths = sorted(records.glob('application-*.json'))
                    assert len(paths) <= 4
                    return [json.loads(path.read_text()) for path in paths] if len(paths) == count else None
                values = wait_for_predicate('actual owned application result', records_ready,
                    time.monotonic() + 5, process=core)
                value = max(values, key=lambda v: v['recorded'])
                assert value['literal_argv_bytes'] == encoded(expected), (expected, values)
                assert value['cwd_bytes'] == encoded([str(cwd)])[0]
                identity = value['identity']
                wait_for_predicate('owned application exit before core retirement', lambda:
                    (current := process_identity(identity['child'])) is None or
                    current['starttime'] != identity['starttime'], time.monotonic() + 5, process=core)
                return value

            main_argv = [str(application), 'main', '', 'two words', 'semi; & literal',
                '$(touch ' + str(marker) + ')', '%', NAME, '--icon', str(icon_paths['parent']), str(entry)]
            action_argv = [str(application), 'inspect', NAME, '--icon', str(icon_paths['action']), str(entry)]
            window = open_menu()
            pixels = view.pixels(window)
            for color in COLORS.values():
                value = (color[0] << 16) | (color[1] << 8) | color[2]
                assert pixels['colors'].get(str(value), 0) >= 4, (color, pixels)
            evidence['cases']['localized_native_icons'] = pixels
            capture('xdg-localized-actions.png')
            dismiss()
            activate(original, NAME)
            evidence['cases']['native_primary'] = observed(main_argv, 1)
            activate(original, ACTION)
            evidence['cases']['native_action'] = observed(action_argv, 2)
            editor('renewed', 'Inspection renouvelée')
            regenerated_path, regenerated = produce('menu-regenerated.fvwm')
            renewed = NAME + ' — Inspection renouvelée'
            labelled_row(regenerated, renewed)
            command('Read ' + json.dumps(str(regenerated_path)))
            open_menu()
            capture('xdg-regenerated-actions.png')
            dismiss()
            activate(regenerated, renewed)
            renewed_argv = list(action_argv)
            renewed_argv[1] = 'renewed'
            evidence['cases']['native_current_action'] = observed(renewed_argv, 3)
            command('Read ' + json.dumps(str(original_path)))
            activate(original, ACTION)
            evidence['cases']['native_old_exported_action'] = observed(action_argv, 4)
            open_menu()
            capture('xdg-retained-export.png')
            dismiss()
            assert not marker.exists(), 'Desktop metadata reached a shell expansion'
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
            for path in records.glob('application-*.json'):
                value = json.loads(path.read_text())
                identity = value['identity']
                clean('owned application exit', lambda identity=identity: wait_for_predicate(
                    'record-and-exit application gone', lambda:
                    (current := process_identity(identity['child'])) is None or
                    current['starttime'] != identity['starttime'], time.monotonic() + 5))
            if control:
                clean('MFL observer', control.close)
            if core:
                clean('core', lambda: core.close(time.monotonic() + 4))
            if errors:
                clean('observer display', errors.close_display)
            elif client:
                clean('observer display', client.close)
            for process in reversed(runtime.processes):
                clean('stop ' + process.name, lambda p=process: p.close(time.monotonic() + 4))
            evidence['children_after_cleanup'] = {p.name: p.poll() for p in runtime.processes}
            if native.exists():
                clean('native child Wait receipts', lambda: evidence.update(module_cleanup=cleanup_receipts(native)))
            for path in runtime.root.iterdir():
                if path.is_dir() and path.name in ('application-records', 'native-records', 'icons', 'icon-cache', 'data', 'config'):
                    clean('copy ' + path.name, lambda p=path: shutil.copytree(p, out / p.name))
                elif path.is_file():
                    clean('copy ' + path.name, lambda p=path: shutil.copy2(p, out / p.name)
                        if not (out / p.name).exists() else None)
            if evidence.get('observer_unexpected_x_errors'):
                evidence['status'] = 'failed'
            atomic_json(out / 'result.json', evidence)
    assert evidence['status'] == 'passed', evidence
    print(json.dumps(dict(profile=profile, status=evidence['status'], artifacts=str(out))))


def main():
    if len(sys.argv) > 1 and sys.argv[1] == '--module-proxy':
        return module_proxy(sys.argv[2], sys.argv[3], sys.argv[4:])
    if len(sys.argv) > 1 and sys.argv[1] == '--application':
        return record_application(sys.argv[2], sys.argv[3], sys.argv[4:])
    if len(sys.argv) > 1 and sys.argv[1] == '--generate':
        return generate(sys.argv[2], sys.argv[3], sys.argv[4:])
    if len(sys.argv) == 4 and sys.argv[1] == '--capture':
        print(json.dumps(NativeCapture(sys.argv[2]).screenshot(
            sys.argv[3]).removeprefix('Screenshot saved: ')))
        return 0
    parser = argparse.ArgumentParser(description=__doc__, allow_abbrev=False)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--provider', type=Path, required=True)
    parser.add_argument('--artifacts-dir', type=Path, required=True)
    parser.add_argument('--profiles', nargs='+', choices=PROFILES, default=list(PROFILES))
    args = parser.parse_args()
    if sys.platform != 'linux':
        parser.error('run only in the owning Linux guest')
    args.artifacts_dir.mkdir(parents=True, exist_ok=True)
    root = Path(tempfile.mkdtemp(prefix='xdg-actions-', dir=args.artifacts_dir.resolve()))
    print(json.dumps(dict(artifacts_dir=str(root))), flush=True)
    for profile in args.profiles:
        run(args, profile, root)
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
