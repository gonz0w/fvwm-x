#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
"""Observe the installed current-folder application chooser through real X11.

The native browser and its actual worker read owned mimeapps/desktop sources.
Only owned record-and-exit applications and the installed browser reuse client
are launched. Raw folder bytes cross the worker protocol unchanged; recordings
use base64 rather than JSON text arguments. A Linux subreaper owns only the exact
recorded detached descendants for Wait. No worker/provider reply is invented.
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
import subprocess
import sys
import tempfile
import time
from urllib.parse import unquote_to_bytes, urlsplit

from browser_continuity_live import ClassHint, cleanup_receipts
from browser_handoff_live import child_identity, process_identity, launcher as browser_launcher
from desktop_applications_live import desktop_string, digest, encoded, record_application
from desktop_live import connect_client
from inspector_live import NativeView, atomic_json
from live_x_harness import HarnessRuntime, start_display_server, wait_for_predicate
from mfl_request_live import frame
from mfl_stream_live import Connection
from native_bindings_live import rows
from native_capture import NativeCapture
from native_module_proxy import module_proxy
from session_x_errors import SessionXErrors

PROFILES = ('slate-dark', 'dune-light', 'classic-large')
CHOOSER_TITLE = 'Open folder with'
DEFAULT_ID = 'fixture-default.desktop'
ALTERNATIVE_ID = 'fixture-alternative.desktop'
SELF_ID = 'fvwm-file-browser.desktop'
DEFAULT_LABEL = 'Dossiers français'
# Provider labels use the existing ASCII presentation contract; launch %c and
# native chooser rows retain the localized name above. Keep this oracle literal.
PUBLIC_DEFAULT_LABEL = 'Dossiers fran[U+00E7]ais'
ALTERNATIVE_LABEL = 'Alternative locale'


def browser_worker(real, directory):
    """Record and exec the original bounded worker request; optional owned gate."""
    root = Path(directory)
    data = sys.stdin.buffer.read(4 * 1024 * 1024 + 1)
    assert len(data) <= 4 * 1024 * 1024
    request = json.loads(data)
    operation = request['operation']
    token = '%d-%d' % (time.monotonic_ns(), os.getpid())
    record = dict(request=request, worker=os.getpid(), started=time.monotonic())
    claim, claimed = root / ('hold-' + operation), root / ('claimed-' + token)
    try:
        claim.rename(claimed)
    except FileNotFoundError:
        pass
    else:
        atomic_json(root / ('held-' + operation + '.json'), dict(token=token,
            worker=os.getpid(), started=time.monotonic()))
        deadline = time.monotonic() + 1.5
        while not (root / ('release-' + token)).exists():
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                atomic_json(root / (token + '.json'), dict(record, gate_timeout=True))
                return 3
            select.select([], [], [], min(.005, remaining))
        record['held'] = True
    atomic_json(root / (token + '.json'), dict(record, forwarded=True))
    # exec and the original stdin leave no intermediate child when cancelled.
    with tempfile.TemporaryFile() as original:
        original.write(data)
        original.seek(0)
        os.dup2(original.fileno(), 0)
        os.execv(real, [real, '--worker'])


def run(args, profile, root):
    source, build = Path(__file__).resolve().parents[2], args.build.resolve()
    fixture, out = Path(__file__).resolve(), root / profile
    out.mkdir()
    evidence = dict(status='running', profile=profile, cases={}, screenshots={},
        cleanup_failures=[], limits=dict(known_mime='inode/directory only',
            terminal='literal terminal coverage belongs to worker tests; no real terminal launched',
            full_platform_release=False, physical_assistive=False))
    inputs = ['tests/live_x/directory_roles_live.py', 'tests/live_x/native_module_proxy.py',
        'tests/live_x/native_bindings_live.py', 'tests/live_x/desktop_applications_live.py',
        'tests/live_x/browser_continuity_live.py', 'tests/live_x/browser_handoff_live.py',
        'tests/live_x/live_x_harness.py', 'tests/live_x/inspector_live.py',
        'tests/live_x/native_capture.py', 'tests/live_x/session_x_errors.py',
        'modules/FvwmMFL/FvwmMFL.c', 'modules/FvwmMFL/request.c',
        'modules/FvwmMFL/request.h']
    inputs += [str(p.relative_to(source)) for group in ('cmd/fvwmfilebrowser',
        'internal/xdgmenu', 'internal/xdgsource') for p in sorted(
        (source / 'bin/FvwmPrompt' / group).glob('*.go'))]
    evidence['source_sha256'] = {name: digest(source / name) for name in inputs}
    binaries = {'core': build / 'fvwm3', 'Desktop': build / 'bin/FvwmPrompt/FvwmDesktop',
        'worker': build / 'bin/FvwmPrompt/fvwm-desktop-worker',
        'browser': build / 'bin/FvwmPrompt/fvwm-file-browser',
        'MFL': build / 'modules/FvwmMFL/FvwmMFL',
        'Form': build / 'modules/FvwmForm/FvwmForm',
        'capture': build / 'bin/fvwm-desktop-capture'}
    evidence['binaries'] = {name: dict(path=str(path.resolve()), sha256=digest(path))
        for name, path in binaries.items()}
    installed_entry = binaries['browser'].resolve().parent.parent / 'share/applications/fvwm-file-browser.desktop'
    assert installed_entry.is_file(), installed_entry
    assert installed_entry.read_bytes() == (source / 'bin/FvwmPrompt/fvwm-file-browser.desktop').read_bytes()
    evidence['installed_entry'] = dict(path=str(installed_entry), sha256=digest(installed_entry))
    with HarnessRuntime(prefix='fvwm-directory-roles-') as runtime:
        core = control = peer = client = errors = browser = None
        records, native, jobs = [runtime.root / name for name in
            ('application-records', 'native-records', 'browser-workers')]
        for path in (records, native, jobs):
            path.mkdir(mode=0o700)
        reuse_launches = runtime.root / 'reuse-launches'
        reuse_launches.mkdir(mode=0o700)
        reaped = {}
        retire_record = None
        try:
            _, display = start_display_server(runtime, 'Xvfb',
                ['Xvfb', '-screen', '0', '1440x1200x24', '-noreset'],
                'xserver.log', time.monotonic() + 10)
            modules, user, data, config_home, tools = [runtime.root / name for name in
                ('modules', 'user', 'data', 'config', 'tools')]
            apps = data / 'applications'
            explicit_cwd = runtime.root / 'explicit working directory'
            for path in (modules, user, apps, config_home, explicit_cwd, tools):
                path.mkdir(mode=0o700, parents=True)
            folder_bytes = os.fsencode(runtime.root) + b'/folder space $(touch marker) ; $[literal] %-\xff'
            os.mkdir(folder_bytes, 0o700)
            folder = os.fsdecode(folder_bytes)
            os.close(os.open(folder_bytes + b'/observed-file', os.O_CREAT | os.O_WRONLY, 0o600))
            marker = folder_bytes + b'/marker'
            evidence['target'] = dict(path_bytes=base64.b64encode(folder_bytes).decode())
            for directory in (build / 'modules').iterdir():
                executable = directory / directory.name
                if executable.is_file() and directory.name not in ('FvwmForm', 'FvwmMFL'):
                    (modules / directory.name).symlink_to(executable)

            def script(path, arguments, suffix=''):
                path.write_text('#!/bin/sh\nexec ' + shlex.join([sys.executable,
                    str(fixture), *map(str, arguments)]) + suffix + '\n')
                path.chmod(0o700)

            for name in ('FvwmDesktop', 'FvwmForm', 'FvwmMFL'):
                key = {'FvwmDesktop': 'Desktop', 'FvwmForm': 'Form', 'FvwmMFL': 'MFL'}[name]
                script(modules / name, ['--module-proxy', binaries[key], native],
                    ' "$@" 2>>' + shlex.quote(str(runtime.root / (name + '.log'))))
            # The unchanged installed desktop entry resolves this owned transparent
            # launcher, which forwards literal argv to the actual staged binary
            # and retains its real Wait result; no reuse reply is synthesized.
            script(tools / 'fvwm-file-browser', ['--reuse-launcher',
                binaries['browser'].resolve(), reuse_launches], ' "$@"')
            evidence['reuse_forwarder'] = dict(actual_installed_executable=str(binaries['browser'].resolve()),
                actual_sha256=digest(binaries['browser']), wrapped_for_owned_wait=True)
            application = runtime.root / 'record application'
            script(application, ['--application', records, application], ' "$@"')
            default_entry, alternative_entry = apps / DEFAULT_ID, apps / ALTERNATIVE_ID

            def write_handler(path, name, mode, field, explicit=False):
                text = '[Desktop Entry]\nType=Application\nName=' + name + '\n'
                localized = {'Default folders': DEFAULT_LABEL, 'Alternative folders': ALTERNATIVE_LABEL}[name]
                text += 'Name[fr]=' + localized + '\n'
                text += 'MimeType=inode/directory;\nTerminal=false\n'
                text += 'Exec=' + desktop_string('"' + str(application) + '" ' + mode +
                    ' ' + field + ' %c %k') + '\n'
                if explicit:
                    text += 'Path=' + str(explicit_cwd).replace(' ', '\\s') + '\n'
                    # Menu presentation restrictions must not remove role/default eligibility.
                    text += 'NoDisplay=true\nOnlyShowIn=Other;\n'
                path.write_text(text)

            write_handler(default_entry, 'Default folders', 'default', '%u', True)
            write_handler(alternative_entry, 'Alternative folders', 'alternative', '%f')
            shutil.copy2(installed_entry, apps / SELF_ID)
            associations = config_home / 'mimeapps.list'

            def write_associations(default=DEFAULT_ID, removed=False):
                text = '[Default Applications]\ninode/directory=' + default + ';\n'
                if removed:
                    text += '[Removed Associations]\ninode/directory=' + ALTERNATIVE_ID + ';\n'
                associations.write_text(text)

            write_associations()
            frozen_inputs = {str(path.relative_to(runtime.root)): digest(path)
                for path in (default_entry, alternative_entry, apps / SELF_ID, associations)}
            evidence['input_sha256'] = frozen_inputs
            for path in (default_entry, alternative_entry, apps / SELF_ID, associations):
                shutil.copy2(path, out / path.name)
            assets = source / 'default-config'
            palette, appearance = ('dune', 'light') if profile == 'dune-light' else ('slate', 'dark')
            (user / 'local.config').write_text('*FvwmDesktop: palette ' + palette +
                '\n*FvwmDesktop: appearance ' + appearance +
                '\n*FvwmDesktop: motion Off\n*FvwmDesktop: transparency Off\n')
            body = (assets / 'config').read_text().replace('$[FVWM_DATADIR]/default-config', str(assets))
            if profile == 'classic-large':
                body = body.replace('Sans:size=10:antialias=True',
                    'DejaVu Sans Mono:size=18:antialias=True')
                body += '\n*FvwmFormDefault: Fore wheat\n*FvwmFormDefault: Back navy\n'
            config = runtime.root / 'config.fvwm'
            config.write_text('ModulePath ' + str(modules) + '\nRead ' +
                str(source / 'fvwm/ConfigFvwmDefaults') + '\n' + body +
                '\nStyle FvwmFiles-* PositionPlacement 20p 60p\n'
                'AddToFunc StartFunction I Module FvwmMFL\n')
            env = runtime.environment(dict(DISPLAY=display, HOME=str(runtime.root),
                FVWM_USERDIR=str(user), FVWM_DESKTOP_ROOT=str(assets),
                FVWMMFL_SOCKET_PATH=str(runtime.root), FVWM_DESKTOP_WORKER=str(binaries['worker']),
                XDG_DATA_HOME=str(data), XDG_DATA_DIRS=str(runtime.root / 'absent-data'),
                XDG_CONFIG_HOME=str(config_home), XDG_CONFIG_DIRS=str(runtime.root / 'absent-config'),
                XDG_CURRENT_DESKTOP='FVWM', LC_ALL='fr_FR.UTF-8', LANGUAGE='fr',
                PATH=str(tools) + os.pathsep + str(binaries['browser'].resolve().parent) +
                os.pathsep + os.environ['PATH']))
            evidence['configuration_sha256'] = {name: digest(path) for name, path in
                [('config', config), ('local', user / 'local.config'), ('default', assets / 'config'),
                 ('desktop', assets / 'desktop.fvwm')]}
            core = runtime.start('core', [str(binaries['core']), '-d', display,
                '-f', str(config)], 'core.log', env)
            endpoint = runtime.root / ('fvwm_mfl_' + display.replace(':', '_') + '.sock')
            deadline = time.monotonic() + 12
            wait_for_predicate('role fixture MFL endpoint', endpoint.exists, deadline, process=core)

            def provider_notice():
                return next((row for row in rows(native) if row['direction'] == 'in' and
                    row['kind'] == 1 << 22 and re.fullmatch(
                        r'MFL_PROVIDER_1 [0-9a-f]{32} [0-9a-f]{32}', row['text'])), None)

            evidence['positive_provider_notice'] = wait_for_predicate('positive Desktop provider',
                provider_notice, deadline, process=core)
            control, peer = Connection(endpoint), Connection(endpoint)
            control.record()
            peer.record()
            peer.socket.settimeout(8)
            control.send(b'set echo\n')
            control.echo('directory-roles-ready')
            serial = 0

            def receipt(prefix):
                until = time.monotonic() + 6
                while time.monotonic() < until:
                    text = control.record().get('echo', {}).get('message', '')
                    if text.startswith(prefix):
                        return text
                raise AssertionError('missing native receipt ' + prefix)

            def command(text):
                control.send((text + '\n').encode())
                control.echo('directory-command-' + os.urandom(4).hex())

            def theme():
                control.send(b'SendToModule FvwmDesktop query-settings directory-theme all\n')
                values = {}
                while True:
                    text = receipt('desktop-settings-directory-theme ')
                    if text.endswith(' done'):
                        break
                    _, name, value = text.split(' ', 2)
                    values[name] = json.loads(value)
                return values if all(values.get(name, {}).get('effective') == value for name, value in
                    [('palette', palette), ('appearance', appearance), ('transparency', False)]) else None

            evidence['theme_settings'] = wait_for_predicate('actual requested theme', theme,
                time.monotonic() + 12, process=core)

            def rpc(scope='state', method='query', expected_error=None, provider_wait=False, **fields):
                nonlocal serial
                serial += 1
                token = 'directory-roles-%d' % serial
                wire = frame(token, method, resource='files', **fields,
                    **({'scope': scope} if 'provider' in fields and method == 'query' else {}))
                peer.send(wire)
                value = peer.record()['reply']
                assert value['id'] == token, value
                evidence.setdefault('rpc_receipts', []).append(dict(
                    request=json.loads(wire.removeprefix(b'request ')), reply=value))
                assert len(evidence['rpc_receipts']) <= 1024
                if expected_error:
                    assert value['status'] == 'error' and value['error']['code'] == expected_error, value
                    return value
                if value['status'] == 'error' and value['error']['code'] == 'not_ready':
                    return None
                if provider_wait and value['status'] == 'error' and value['error']['code'] == 'provider_busy':
                    # FvwmMFL documents one active producer query. Only the
                    # explicit post-reuse wait treats that contention as pending.
                    assert method == 'query' and scope == 'state' and fields.get('provider') == provider
                    return None
                assert value['status'] == 'ok', value
                return value['result']

            private_browser = runtime.root / 'browser-image'
            shutil.copy2(binaries['browser'], private_browser)
            browser = runtime.start('browser', [str(private_browser), '--socket', str(endpoint),
                '--dir', folder, '--view', 'list', '--watch', 'off', '--opener', '/bin/true',
                '--terminal', '/bin/true'], 'browser.log', env)
            listed = wait_for_predicate('browser registered', lambda: (rpc() or {}).get('providers'),
                time.monotonic() + 10, process=browser)
            assert len(listed) == 1, listed
            provider = listed[0]['provider']

            def state():
                return rpc(provider=provider)

            def ready(provider_wait=False):
                value = rpc(provider=provider, provider_wait=provider_wait)
                return value if value and not value['busy'] and value['availability']['ready'] and \
                    value['bindings']['projection']['state'] == 'current' else None

            initial = wait_for_predicate('ready current-folder browser', ready,
                time.monotonic() + 12, process=browser)
            assert base64.b64decode(initial['path_bytes']) == folder_bytes
            wrapper = runtime.root / 'browser-worker-wrapper'
            script(wrapper, ['--browser-worker', binaries['browser'], jobs])
            wrapper.replace(private_browser)
            client = connect_client(display)
            errors = SessionXErrors(client, evidence).install()
            view = NativeView(client, errors, core)
            x, d = client.x, client.display
            x.XSetInputFocus.argtypes = [C.c_void_p, C.c_ulong, C.c_int, C.c_ulong]
            x.XSetInputFocus.restype = C.c_int
            x.XGetClassHint.argtypes = [C.c_void_p, C.c_ulong, C.POINTER(ClassHint)]
            x.XGetClassHint.restype = C.c_int
            window = view.wait('File browser')

            def focus(target):
                x.XSetInputFocus(d, target, 1, 0)
                x.XSync(d, 0)

            def alias_of(target):
                hint = ClassHint()
                try:
                    if not errors.read(target, 20, lambda: x.XGetClassHint(d, target, C.byref(hint))):
                        return None
                    return C.string_at(hint.name).decode() if hint.name else None
                finally:
                    if hint.name: x.XFree(hint.name)
                    if hint.class_: x.XFree(hint.class_)

            browser_alias = alias_of(window)
            assert browser_alias and browser_alias.startswith('FvwmFiles-' + provider + '-')

            def role_query():
                return rpc(scope='directory_roles', provider=provider, offset=0, limit=8)

            def roles_ready():
                value = role_query()
                return value if value and value['status'] == 'ready' else None

            def action(operation, values=None, expected_error=None, generation=None):
                return rpc(method='action', provider=provider,
                    generation=generation or state()['generation'], operation=operation,
                    values=values or {}, expected_error=expected_error)

            def chooser(expected_status='ready'):
                assert expected_status in ('ready', 'failed')
                def current():
                    commands = [row['text'] for row in rows(native)
                        if row['direction'] == 'out']
                    prefix = 'Module FvwmForm FvwmFiles-' + provider + '-roles-'
                    starts = [text for text in commands if text.startswith(prefix)]
                    if not starts:
                        return None
                    alias = starts[-1].removeprefix('Module FvwmForm ')
                    configuration = [text for text in commands
                        if text.startswith('*' + alias + ': ')]
                    # Every status shares a title. Require the latest actually
                    # started alias's own expected status configuration, then find
                    # its mapped window by class rather than any same-title peer.
                    if not any(text.startswith('*' + alias + ': Label status 42 "' + expected_status + ': ')
                            for text in configuration):
                        return None
                    assert '*' + alias + ': Title "' + CHOOSER_TITLE + '"' in configuration
                    queue, visited = [(client.root, 0)], 0
                    while queue:
                        target, depth = queue.pop()
                        visited += 1
                        assert visited <= 4096
                        attributes = view.attributes(target)
                        if attributes and attributes.map_state == 2 and alias_of(target) == alias:
                            forms = evidence.setdefault('native_chooser_forms', {})
                            assert len(forms) < 128
                            forms[alias] = dict(window=target, expected_status=expected_status, configuration=configuration)
                            return target, alias
                        if depth < 5:
                            queue.extend((child, depth + 1) for child in view.children(target))
                    return None
                return wait_for_predicate('latest ' + expected_status + ' folder chooser alias mapped', current,
                    time.monotonic() + 10, process=browser)

            def capture(name):
                result = subprocess.run([sys.executable, str(fixture), '--capture',
                    str(binaries['capture']), str(out)], env=env, capture_output=True,
                    text=True, timeout=40, check=True)
                path = Path(json.loads(result.stdout))
                target = out / name
                path.replace(target)
                evidence['screenshots'][name] = digest(target)

            def native_selection(alias):
                token = 'roles-selection-' + os.urandom(4).hex()
                control.send(('SendToModule %s ListStatus applications %s selection\n' %
                    (alias, token)).encode())
                text = receipt('FVWM_FORM_LIST ' + token + ' ')
                fields = text.split()
                assert len(fields) == 6 and fields[3:5] == ['0', '0'], text
                packet = base64.urlsafe_b64decode(fields[5][1:] + '==')
                return dict(generation=int(fields[2]), focus=int.from_bytes(packet[4:6], 'big'),
                    selected=[i for i in range((len(packet) - 6) * 8)
                        if packet[6 + i // 8] & (1 << (i % 8))], receipt=text)

            def records_now():
                values = [json.loads(p.read_text()) for p in sorted(records.glob('application-*.json'))]
                assert len(values) <= 8
                return values

            def reap_record(value):
                identity = value['identity']
                if identity['child'] in reaped:
                    return reaped[identity['child']]
                def done():
                    current = process_identity(identity['child'])
                    assert current is None or current['starttime'] == identity['starttime']
                    try:
                        pid, status = os.waitpid(identity['child'], os.WNOHANG)
                    except ChildProcessError:
                        return dict(state='already reaped') if current is None else None
                    if pid:
                        assert os.waitstatus_to_exitcode(status) == 0
                        return dict(state='owned adopted child reaped', exit=0)
                    return None
                result = wait_for_predicate('exact detached recording child Wait', done,
                    time.monotonic() + 5, process=browser)
                reaped[identity['child']] = result
                return result

            def retire_record(value):
                # On a failed test, stop only the exact recorded descendant
                # already adopted by this subreaper. The forwarding launcher's
                # own signal handler retires and Waits its retained child.
                try:
                    return reap_record(value)
                except BaseException:
                    identity = value['identity']
                    current = process_identity(identity['child'])
                    if current is None:
                        raise
                    assert current['starttime'] == identity['starttime'] and current['parent'] == os.getpid()
                    os.kill(identity['child'], signal.SIGTERM)
                    deadline = time.monotonic() + 2
                    while True:
                        pid, status = os.waitpid(identity['child'], os.WNOHANG)
                        if pid:
                            break
                        if time.monotonic() >= deadline:
                            current = process_identity(identity['child'])
                            assert current and current['starttime'] == identity['starttime'] and current['parent'] == os.getpid()
                            os.kill(identity['child'], signal.SIGKILL)
                            pid, status = os.waitpid(identity['child'], 0)
                            break
                        select.select([], [], [], .005)
                    evidence.setdefault('forced_owned_cleanup', []).append(dict(
                        identity=identity, exit=os.waitstatus_to_exitcode(status)))
                    raise AssertionError('owned record-and-exit child required forced cleanup')

            def local_uri_argument(raw):
                # RFC-valid local URIs can spell reserved bytes differently.
                # Independently validate semantics, then assert the complete
                # literal argv including that one unmodified URI argument.
                value = raw.decode('ascii')
                parsed = urlsplit(value)
                assert parsed.scheme == 'file' and parsed.netloc == '' and not parsed.query and not parsed.fragment, value
                assert unquote_to_bytes(parsed.path) == folder_bytes, value
                return value

            def observed(expected, cwd, count, uri_index=None):
                values = wait_for_predicate('actual handler record', lambda:
                    records_now() if len(records_now()) == count else None,
                    time.monotonic() + 6, process=browser)
                latest = max(values, key=lambda v: v['recorded'])
                if uri_index is not None:
                    expected = list(expected)
                    expected[uri_index] = local_uri_argument(base64.b64decode(latest['literal_argv_bytes'][uri_index]))
                assert latest['literal_argv_bytes'] == encoded(expected), (expected, latest)
                assert latest['cwd_bytes'] == encoded([cwd])[0], latest
                return dict(record=latest, wait=reap_record(latest))

            focus(window)
            view.key('F12')
            choices = wait_for_predicate('directory choices ready', roles_ready,
                time.monotonic() + 10, process=browser)
            items = choices['items']
            ids = [item['id'] for item in items]
            by_label = {item['label']: item for item in items}
            assert set(by_label) == {PUBLIC_DEFAULT_LABEL, ALTERNATIVE_LABEL,
                'FVWM-X File Browser (development)'}, choices
            assert all(re.fullmatch(r'[0-9a-f]{32}', item['id']) and
                re.fullmatch(r'[0-9a-f]{64}', item['fingerprint']) for item in items), choices
            assert choices['default_id'] == by_label[PUBLIC_DEFAULT_LABEL]['id'], choices
            target, role_alias = chooser()
            selected = native_selection(role_alias)
            assert selected['focus'] == ids.index(by_label[PUBLIC_DEFAULT_LABEL]['id']) and selected['selected'] == [ids.index(by_label[PUBLIC_DEFAULT_LABEL]['id'])], selected
            evidence['cases']['default_native_selection'] = dict(choices=choices, native=selected)
            capture('directory-default-chooser.png')
            focus(target)
            view.key('Return')
            expected = [str(application), 'default', '', DEFAULT_LABEL, str(default_entry)]
            evidence['cases']['native_default_launch'] = observed(expected, str(explicit_cwd), 1, uri_index=2)
            wait_for_predicate('default launch worker completes', ready,
                time.monotonic() + 6, process=browser)

            def open_choices():
                previous = role_query()['choice_generation']
                focus(window)
                view.key('F12')
                result = wait_for_predicate('new directory choices ready', lambda:
                    (value if (value := roles_ready()) and value['choice_generation'] != previous else None),
                    time.monotonic() + 10, process=browser)
                return result, chooser()[0]

            choices, target = open_choices()
            ids = [item['id'] for item in choices['items']]
            alternative_id = next(item['id'] for item in choices['items']
                if item['label'] == ALTERNATIVE_LABEL)
            target, role_alias = chooser()
            focus(target)
            assert view.focus() == target
            view.key('Home')
            for _ in range(ids.index(alternative_id)):
                view.key('Down')
            selected = native_selection(role_alias)
            assert selected['generation'] == 1 and selected['focus'] == ids.index(alternative_id), selected
            assert chooser() == (target, role_alias), 'ready chooser changed during keyboard selection'
            evidence['cases']['alternative_native_focus_before_launch'] = dict(
                alias=role_alias, window=target, intended_index=ids.index(alternative_id), native=selected)
            # The one-time explicit choice must survive an unrelated default
            # change, while the fresh worker still validates association membership.
            write_associations(default=SELF_ID)
            view.key('Return')
            expected = [str(application), 'alternative', folder, ALTERNATIVE_LABEL, str(alternative_entry)]
            evidence['cases']['native_alternative_raw_path'] = observed(expected, folder, 2)
            evidence['cases']['default_change_did_not_substitute_explicit_choice'] = True
            wait_for_predicate('alternative launch completed', ready,
                time.monotonic() + 6, process=browser)
            write_associations()

            choices, target = open_choices()
            old = next(item for item in choices['items'] if item['label'] == ALTERNATIVE_LABEL)
            write_handler(alternative_entry, 'Alternative folders', 'changed', '%f')
            launch = dict(generation=choices['choice_generation'], id=old['id'])
            admitted = action('directory-role-launch', launch)
            failed = wait_for_predicate('changed handler refused', lambda:
                (v if (v := state())['work'] and v['work']['status'] == 'failed' else None),
                time.monotonic() + 8, process=browser)
            assert len(records_now()) == 2
            failed_target, failed_alias = chooser(expected_status='failed')
            evidence['cases']['changed_handler_refused'] = dict(admission=admitted, state=failed,
                native_alias=failed_alias, native_window=failed_target)
            capture('directory-stale-handler.png')
            write_handler(alternative_entry, 'Alternative folders', 'alternative', '%f')
            focus(chooser(expected_status='failed')[0])
            view.key('F5')
            fresh = wait_for_predicate('refreshed directory choices', lambda:
                (v if (v := roles_ready()) and v['choice_generation'] != choices['choice_generation'] else None),
                time.monotonic() + 8, process=browser)
            action('directory-role-launch', launch, expected_error='stale_choices')
            assert len(records_now()) == 2
            evidence['cases']['stale_choice_after_refresh'] = fresh

            alternative = next(item for item in fresh['items'] if item['label'] == ALTERNATIVE_LABEL)
            write_associations(removed=True)
            removed_admission = action('directory-role-launch', dict(
                generation=fresh['choice_generation'], id=alternative['id']))
            removed = wait_for_predicate('removed association refused', lambda:
                (value if (value := state())['work'] and value['work']['status'] == 'failed' else None),
                time.monotonic() + 8, process=browser)
            assert len(records_now()) == 2
            failed_target, failed_alias = chooser(expected_status='failed')
            evidence['cases']['removed_association_refused'] = dict(admission=removed_admission, state=removed,
                native_alias=failed_alias, native_window=failed_target)
            write_associations()
            focus(chooser(expected_status='failed')[0])
            view.key('F5')
            wait_for_predicate('association refresh recovered', lambda:
                (value if (value := roles_ready()) and
                    any(item['label'] == ALTERNATIVE_LABEL for item in value['items']) else None),
                time.monotonic() + 8, process=browser)

            # A real bounded worker gate establishes busy admission deterministically.
            (jobs / 'hold-directory-roles').touch()
            focus(chooser()[0])
            view.key('F5')
            held = wait_for_predicate('real discovery worker held', lambda:
                json.loads((jobs / 'held-directory-roles.json').read_text())
                if (jobs / 'held-directory-roles.json').exists() else None,
                time.monotonic() + 5, process=browser)
            action('directory-roles', expected_error='busy')
            (jobs / ('release-' + held['token'])).touch()
            fresh = wait_for_predicate('held discovery completed', roles_ready,
                time.monotonic() + 6, process=browser)
            evidence['cases']['duplicate_busy_refused'] = held
            focus(chooser()[0])
            view.key('Escape')
            closed = wait_for_predicate('chooser close visible to clients', lambda:
                (v if (v := role_query())['status'] == 'closed' else None),
                time.monotonic() + 5, process=browser)
            evidence['cases']['explicit_close'] = closed

            choices, target = open_choices()
            own = next(item for item in choices['items']
                if item['label'] == 'FVWM-X File Browser (development)')
            before = state()
            action('directory-role-launch', dict(generation=choices['choice_generation'],
                id=own['id']))
            # Let the actual reuse client complete its own provider queries
            # before this observer competes for the producer's bounded slot.
            launches = wait_for_predicate('actual installed reuse child Wait', lambda:
                [json.loads(path.read_text()) for path in reuse_launches.glob('launch-*.json')]
                if list(reuse_launches.glob('wait-*.json')) else None,
                time.monotonic() + 6, process=browser)
            assert len(launches) == 1 and len(launches[0]['argv_bytes']) == 3, launches
            assert launches[0]['argv_bytes'][:2] == encoded(['--reuse', '--']), launches
            local_uri_argument(base64.b64decode(launches[0]['argv_bytes'][2]))
            wait_receipts = [json.loads(path.read_text()) for path in reuse_launches.glob('wait-*.json')]
            evidence['reuse_child_observation'] = dict(launches=launches, waits=wait_receipts)
            assert len(wait_receipts) == 1 and wait_receipts[0]['returncode'] == 0, wait_receipts
            assert wait_receipts[0]['child'] == launches[0]['child'], wait_receipts
            launcher_wait = reap_record(dict(identity=launches[0]['launcher_identity']))

            def reused():
                value = ready(provider_wait=True)
                if not value:
                    return None
                providers = (rpc() or {}).get('providers', [])
                return value if value.get('work') and value['work']['operation'] == 'refresh' and \
                    value['work']['status'] == 'loaded' and len(providers) == 1 and providers[0]['provider'] == provider else None
            reused_state = wait_for_predicate('installed handler reuses existing browser', reused,
                time.monotonic() + 12, process=browser)
            assert view.named('File browser') == window
            assert base64.b64decode(reused_state['path_bytes']) == folder_bytes
            assert reused_state['generation'] != before['generation'], reused_state
            evidence['cases']['installed_browser_reuse'] = dict(before=before, after=reused_state,
                installed_entry_sha256=digest(installed_entry), handler=own,
                actual_launch=launches[0], child_wait=wait_receipts[0], launcher_wait=launcher_wait)
            # Removing all effective handlers is visible as an empty current
            # chooser, rather than an invented default or a launch fallback.
            for path in (default_entry, alternative_entry, apps / SELF_ID):
                path.write_text('[Desktop Entry]\nType=Application\nHidden=true\n')
            empty, _ = open_choices()
            assert empty['items'] == [] and empty['default_id'] == '', empty
            assert len(records_now()) == 2
            evidence['cases']['no_eligible_handlers'] = empty
            for path in (default_entry, alternative_entry, apps / SELF_ID):
                shutil.copy2(out / path.name, path)
            focus(chooser()[0])
            view.key('F5')
            recovered = wait_for_predicate('empty chooser refresh recovered', lambda:
                (value if (value := roles_ready()) and len(value['items']) == 3 else None),
                time.monotonic() + 8, process=browser)
            evidence['cases']['refresh_recovery'] = recovered
            chooser()
            capture('directory-recovered-chooser.png')
            assert not os.path.exists(marker), 'raw folder reached shell expansion'
            assert digest(associations) == frozen_inputs['config/mimeapps.list']
            evidence['cases']['no_default_write_or_shell'] = True
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
                clean('recorded application Wait', lambda value=value: retire_record(value))
            for path in reuse_launches.glob('launch-*.json'):
                value = json.loads(path.read_text())
                clean('recorded reuse launcher Wait', lambda value=value:
                    retire_record(dict(identity=value['launcher_identity'])))
            evidence['recorded_application_waits'] = reaped
            if peer: clean('file RPC observer', peer.close)
            if control: clean('core observer', control.close)
            if browser: clean('browser', lambda: browser.close(time.monotonic() + 5))
            if core: clean('core', lambda: core.close(time.monotonic() + 5))
            if errors: clean('observer display', errors.close_display)
            elif client: clean('observer display', client.close)
            for process in reversed(runtime.processes):
                clean('stop ' + process.name, lambda process=process: process.close(time.monotonic() + 4))
            evidence['children_after_cleanup'] = {p.name: p.poll() for p in runtime.processes}
            if native.exists():
                clean('native child Wait receipts', lambda: evidence.update(module_cleanup=cleanup_receipts(native)))
            for path in runtime.root.iterdir():
                if path.is_dir() and path.name in ('native-records', 'browser-workers', 'application-records', 'reuse-launches'):
                    clean('copy ' + path.name, lambda path=path: shutil.copytree(path, out / path.name))
                elif path.is_file() and path.suffix == '.log':
                    clean('copy ' + path.name, lambda path=path: shutil.copy2(path, out / path.name))
            if evidence.get('observer_unexpected_x_errors'):
                evidence['status'] = 'failed'
            atomic_json(out / 'result.json', evidence)
    assert evidence['status'] == 'passed', evidence
    print(json.dumps(dict(profile=profile, status='passed', artifacts=str(out))))


def main():
    if len(sys.argv) > 1 and sys.argv[1] == '--module-proxy':
        return module_proxy(sys.argv[2], sys.argv[3], sys.argv[4:])
    if len(sys.argv) == 4 and sys.argv[1] == '--browser-worker':
        return browser_worker(sys.argv[2], sys.argv[3])
    if len(sys.argv) > 1 and sys.argv[1] == '--reuse-launcher':
        return browser_launcher(sys.argv[2], sys.argv[3], sys.argv[4:])
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
        parser.error('run in the owning Linux guest; exact detached-child Wait requires subreaping')
    libc = C.CDLL(None, use_errno=True)
    libc.prctl.argtypes = [C.c_int, C.c_ulong, C.c_ulong, C.c_ulong, C.c_ulong]
    if libc.prctl(36, 1, 0, 0, 0) != 0:
        raise RuntimeError('cannot own detached fixture descendants for exact cleanup')
    args.artifacts_dir.mkdir(parents=True, exist_ok=True)
    root = Path(tempfile.mkdtemp(prefix='directory-roles-', dir=args.artifacts_dir.resolve()))
    print(json.dumps(dict(artifacts_dir=str(root))), flush=True)
    for profile in args.profiles:
        run(args, profile, root)
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
