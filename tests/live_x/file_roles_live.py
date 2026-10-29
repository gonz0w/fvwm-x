#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
"""Independent selected-regular-file Open with checks on an actual Linux X server.

Run with the lead's installed build view and an owned artifacts directory.
The classifier wrapper records then execs the discovered real file(1). It never
supplies a MIME answer. Native Form/protocol/session helpers are shared with the
current-folder fixture; this scenario does not run that unrelated suite.
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

import directory_roles_live as shared

PROFILES = shared.PROFILES
DEFAULT_ID = 'file-default.desktop'
ALTERNATIVE_ID = 'file-alternative.desktop'
DEFAULT_LABEL = 'Selected text default'
ALTERNATIVE_LABEL = 'Selected text alternative'
TEXT = b'An ordinary plain text document for independent MIME observation.\n'


def identity_state(identity):
    """Inspect only a recorded process, checking start identity on both reads."""
    current = shared.process_identity(identity['child'])
    if current is None or current['starttime'] != identity['starttime']:
        return None
    try:
        fields = Path('/proc/%d/stat' % identity['child']).read_text().rsplit(')', 1)[1].split()
    except FileNotFoundError:
        return None
    after = shared.process_identity(identity['child'])
    if after is None or after['starttime'] != identity['starttime']:
        return None
    return dict(after, state=fields[0], group=int(fields[2]))


def direct_children(identity):
    """Aggregate only this recorded process's tasks, including Go exec threads."""
    assert identity_state(identity)
    tasks = list((Path('/proc') / str(identity['child']) / 'task').iterdir())
    assert len(tasks) <= 128
    children = set()
    for task in tasks:
        try:
            children.update(int(value) for value in (task / 'children').read_text().split())
        except FileNotFoundError:
            pass  # A retiring thread cannot create a new surviving child here.
    assert identity_state(identity)
    assert len(children) <= 64
    return sorted(children)


def hold_classifier(root, identity):
    """One explicitly armed cancellation control; never emits a MIME answer."""
    claim = root / 'hold-next.json'
    claimed = root / ('hold-claimed-%d.json' % os.getpid())
    try:
        claim.rename(claimed)
    except FileNotFoundError:
        return
    control = json.loads(claimed.read_text())
    token, browser = control['token'], control['browser_identity']
    assert re.fullmatch(r'[0-9a-f]{16}', token)
    worker = shared.process_identity(os.getppid())
    assert worker and worker['parent'] == browser['child']
    assert identity_state(browser) and identity_state(worker)
    assert os.getpgrp() == worker['child'], 'classifier must inherit owned worker group'
    started = time.monotonic_ns()
    heartbeat = root / ('heartbeat-' + token + '.jsonl')
    with heartbeat.open('wb', buffering=0) as stream:
        # Write the first heartbeat before readiness, giving the observer a
        # complete frozen identity plus heartbeat boundary before it triggers.
        def tick():
            state = identity_state(browser)
            row = dict(time_ns=time.monotonic_ns(),
                browser_state=state['state'] if state else 'absent')
            stream.write((json.dumps(row) + '\n').encode())
        tick()
        shared.atomic_json(root / ('held-' + token + '.json'), dict(
            token=token, classifier_identity=identity, worker_identity=worker,
            browser_identity=browser, group=os.getpgrp(), started_ns=started,
            heartbeat=heartbeat.name, controlled_before_real_provider=True))
        while time.monotonic_ns() - started < 3_000_000_000:
            select.select([], [], [], .005)
            tick()
    # The product's two-second classifier deadline must win even when the
    # intended exit is broken. This guard provides no positive provider result.
    shared.atomic_json(root / ('hold-expired-' + token + '.json'), dict(token=token))
    raise RuntimeError('controlled classifier hold exceeded cancellation boundary')


def classifier(real, directory, arguments):
    """Transparent instrumentation: preserve arguments and actual MIME output."""
    identity = shared.child_identity(os.getpid(), os.getppid())
    assert identity
    shared.atomic_json(Path(directory) / ('classifier-%d.json' % os.getpid()),
        dict(identity=identity, executable=str(Path(real).resolve()),
            executable_sha256=shared.digest(Path(real)),
            argv_bytes=shared.encoded(arguments)))
    hold_classifier(Path(directory), identity)
    os.execv(real, [real, *arguments])


class Session:
    """One owned server/browser and one observer per protocol channel."""
    def __init__(self, args, profile, out, runtime, evidence):
        self.args, self.profile, self.out = args, profile, out
        self.runtime, self.evidence = runtime, evidence
        self.source = Path(__file__).resolve().parents[2]
        self.fixture = Path(__file__).resolve()
        self.core = self.browser = self.peer = self.control = None
        self.client = self.errors = None
        self.serial, self.reaped = 0, {}
        self.root = runtime.root
        self.held_children = []
        self.records, self.native, self.jobs, self.classifiers = [self.root / name
            for name in ('application-records', 'native-records', 'browser-workers',
                'classifier-records')]
        for path in (self.records, self.native, self.jobs, self.classifiers):
            path.mkdir(mode=0o700)

    def script(self, path, arguments, suffix=''):
        path.write_text('#!/bin/sh\nexec ' + shlex.join([sys.executable,
            str(self.fixture), *map(str, arguments)]) + suffix + '\n')
        path.chmod(0o700)

    def start(self):
        build = self.args.build.resolve()
        binary_paths = dict(core='fvwm3', Desktop='bin/FvwmPrompt/FvwmDesktop',
            worker='bin/FvwmPrompt/fvwm-desktop-worker',
            browser='bin/FvwmPrompt/fvwm-file-browser',
            MFL='modules/FvwmMFL/FvwmMFL', Form='modules/FvwmForm/FvwmForm',
            capture='bin/fvwm-desktop-capture')
        self.binaries = {key: build / value for key, value in binary_paths.items()}
        self.evidence['binaries'] = {key: dict(path=str(path.resolve()),
            sha256=shared.digest(path)) for key, path in self.binaries.items()}
        # Resolve before installing the transparent PATH forwarder. An absent
        # classifier is unavailable evidence, never a fabricated positive case.
        real_file = shutil.which('file')
        assert real_file and Path(real_file).is_file(), 'real file(1) unavailable'
        self.evidence['real_classifier'] = dict(path=str(Path(real_file).resolve()),
            sha256=shared.digest(Path(real_file)))
        _, display = shared.start_display_server(self.runtime, 'Xvfb',
            ['Xvfb', '-screen', '0', '1440x1200x24', '-noreset'],
            'xserver.log', time.monotonic() + 10)
        modules, user, data, config_home, tools = [self.root / name
            for name in ('modules', 'user', 'data', 'config', 'tools')]
        self.apps = data / 'applications'
        self.cwd = self.root / 'explicit working directory'
        self.folder = self.root / 'selected files'
        for path in (modules, user, self.apps, config_home, tools, self.cwd, self.folder):
            path.mkdir(mode=0o700, parents=True)
        self.target = os.fsencode(self.folder) + b'/text $(touch marker) ; $[literal] %-\xff'
        self.other = os.fsencode(self.folder) + b'/other-text'
        self.link = os.fsencode(self.folder) + b'/linked-text'
        for path in (self.target, self.other):
            with open(path, 'wb') as stream:
                stream.write(TEXT)
        os.symlink(self.target, self.link)
        self.marker = self.folder / 'marker'
        self.evidence['target'] = dict(path_bytes=shared.encoded([self.target])[0],
            content_sha256=hashlib.sha256(TEXT).hexdigest())
        for directory in (build / 'modules').iterdir():
            executable = directory / directory.name
            if executable.is_file() and directory.name not in ('FvwmForm', 'FvwmMFL'):
                (modules / directory.name).symlink_to(executable)
        for name in ('FvwmDesktop', 'FvwmForm', 'FvwmMFL'):
            key = dict(FvwmDesktop='Desktop', FvwmForm='Form', FvwmMFL='MFL')[name]
            self.script(modules / name, ['--module-proxy', self.binaries[key], self.native],
                ' "$@" 2>>' + shlex.quote(str(self.root / (name + '.log'))))
        self.script(tools / 'file', ['--classifier', real_file, self.classifiers], ' "$@"')
        self.application = self.root / 'record application'
        self.script(self.application, ['--application', self.records, self.application], ' "$@"')
        self.default_entry, self.alternative_entry = [self.apps / name
            for name in (DEFAULT_ID, ALTERNATIVE_ID)]
        self.write_handler(self.default_entry, DEFAULT_LABEL, 'default', '%u', True)
        self.write_handler(self.alternative_entry, ALTERNATIVE_LABEL, 'alternative', '%f')
        self.associations = config_home / 'mimeapps.list'
        self.write_associations()
        self.original_inputs = {path: path.read_bytes() for path in
            (self.default_entry, self.alternative_entry, self.associations)}
        self.evidence['input_sha256'] = {str(path.relative_to(self.root)): shared.digest(path)
            for path in self.original_inputs}
        for path in self.original_inputs:
            shutil.copy2(path, self.out / path.name)
        assets = self.source / 'default-config'
        palette, appearance = ('dune', 'light') if self.profile == 'dune-light' else ('slate', 'dark')
        self.palette, self.appearance = palette, appearance
        (user / 'local.config').write_text('*FvwmDesktop: palette ' + palette +
            '\n*FvwmDesktop: appearance ' + appearance +
            '\n*FvwmDesktop: motion Off\n*FvwmDesktop: transparency Off\n')
        body = (assets / 'config').read_text().replace('$[FVWM_DATADIR]/default-config', str(assets))
        if self.profile == 'classic-large':
            body = body.replace('Sans:size=10:antialias=True',
                'DejaVu Sans Mono:size=18:antialias=True')
            body += '\n*FvwmFormDefault: Fore wheat\n*FvwmFormDefault: Back navy\n'
        config = self.root / 'config.fvwm'
        config.write_text('ModulePath ' + str(modules) + '\nRead ' +
            str(self.source / 'fvwm/ConfigFvwmDefaults') + '\n' + body +
            '\nStyle FvwmFiles-* PositionPlacement 20p 60p\n'
            'AddToFunc StartFunction I Module FvwmMFL\n')
        self.env = self.runtime.environment(dict(DISPLAY=display, HOME=str(self.root),
            FVWM_USERDIR=str(user), FVWM_DESKTOP_ROOT=str(assets),
            FVWMMFL_SOCKET_PATH=str(self.root),
            FVWM_DESKTOP_WORKER=str(self.binaries['worker']),
            XDG_DATA_HOME=str(data), XDG_DATA_DIRS=str(self.root / 'absent-data'),
            XDG_CONFIG_HOME=str(config_home), XDG_CONFIG_DIRS=str(self.root / 'absent-config'),
            XDG_CURRENT_DESKTOP='FVWM', LC_ALL='C',
            PATH=str(tools) + os.pathsep + os.environ['PATH']))
        self.evidence['configuration_sha256'] = {name: shared.digest(path) for name, path in
            [('config', config), ('local', user / 'local.config'),
             ('default', assets / 'config'), ('desktop', assets / 'desktop.fvwm')]}
        self.core = self.runtime.start('core', [str(self.binaries['core']), '-d', display,
            '-f', str(config)], 'core.log', self.env)
        endpoint = self.root / ('fvwm_mfl_' + display.replace(':', '_') + '.sock')
        shared.wait_for_predicate('file roles endpoint', endpoint.exists,
            time.monotonic() + 12, process=self.core)
        self.evidence['provider_notice'] = shared.wait_for_predicate('Desktop provider registered',
            lambda: next((row for row in shared.rows(self.native)
                if row['direction'] == 'in' and row['kind'] == 1 << 22 and re.fullmatch(
                    r'MFL_PROVIDER_1 [0-9a-f]{32} [0-9a-f]{32}', row['text'])), None),
            time.monotonic() + 12, process=self.core)
        self.control, self.peer = shared.Connection(endpoint), shared.Connection(endpoint)
        self.control.record()
        self.peer.record()
        self.peer.socket.settimeout(8)
        self.control.send(b'set echo\n')
        self.control.echo('file-roles-ready')
        self.evidence['theme_settings'] = shared.wait_for_predicate('actual theme settings',
            self.theme, time.monotonic() + 12, process=self.core)
        private_browser = self.root / 'browser-image'
        shutil.copy2(self.binaries['browser'], private_browser)
        self.browser = self.runtime.start('browser', [str(private_browser), '--socket', str(endpoint),
            '--dir', str(self.folder), '--view', 'list', '--watch', 'off',
            '--opener', '/bin/true', '--terminal', '/bin/true'], 'browser.log', self.env)
        listed = self.wait('browser registered', lambda: (self.rpc() or {}).get('providers'))
        assert len(listed) == 1, listed
        self.provider = listed[0]['provider']
        initial = self.wait('browser current', self.ready)
        assert base64.b64decode(initial['path_bytes']) == os.fsencode(self.folder)
        wrapper = self.root / 'browser-worker-wrapper'
        self.script(wrapper, ['--browser-worker', self.binaries['browser'], self.jobs])
        wrapper.replace(private_browser)
        self.client = shared.connect_client(display)
        self.errors = shared.SessionXErrors(self.client, self.evidence).install()
        self.view = shared.NativeView(self.client, self.errors, self.core)
        self.x, self.d = self.client.x, self.client.display
        self.x.XSetInputFocus.argtypes = [C.c_void_p, C.c_ulong, C.c_int, C.c_ulong]
        self.x.XSetInputFocus.restype = C.c_int
        self.x.XGetClassHint.argtypes = [C.c_void_p, C.c_ulong, C.POINTER(shared.ClassHint)]
        self.x.XGetClassHint.restype = C.c_int
        self.window = self.view.wait('File browser')
        self.browser_alias = self.alias(self.window)
        assert self.browser_alias.startswith('FvwmFiles-' + self.provider + '-')

    def fresh_browser(self, name):
        """Reuse this owned server/core, with a new actual browser process."""
        assert self.browser.poll() is not None
        shared.wait_for_predicate('previous browser provider retired',
            lambda: not (self.rpc() or {}).get('providers'),
            time.monotonic() + 5, process=self.core)
        shared.wait_for_predicate('previous browser Form retired',
            lambda: not self.view.named('File browser'),
            time.monotonic() + 5, process=self.core)
        private_browser = self.root / ('browser-image-' + name)
        shutil.copy2(self.binaries['browser'], private_browser)
        endpoint = self.root / ('fvwm_mfl_' + self.env['DISPLAY'].replace(':', '_') + '.sock')
        self.browser = self.runtime.start('browser-' + name,
            [str(private_browser), '--socket', str(endpoint), '--dir', str(self.folder),
             '--view', 'list', '--watch', 'off', '--opener', '/bin/true',
             '--terminal', '/bin/true'], 'browser-' + name + '.log', self.env)
        listed = self.wait('fresh browser registered', lambda: (self.rpc() or {}).get('providers'))
        assert len(listed) == 1, listed
        self.provider = listed[0]['provider']
        self.wait('fresh browser current', self.ready)
        wrapper = self.root / ('browser-worker-wrapper-' + name)
        self.script(wrapper, ['--browser-worker', self.binaries['browser'], self.jobs])
        wrapper.replace(private_browser)
        self.window = self.view.wait('File browser')
        self.browser_alias = self.alias(self.window)
        assert self.browser_alias.startswith('FvwmFiles-' + self.provider + '-')

    def write_handler(self, path, label, mode, field, explicit=False):
        text = '[Desktop Entry]\nType=Application\nName=' + label + '\n'
        text += 'MimeType=text/plain;inode/directory;\nTerminal=false\n'
        text += 'Exec=' + shared.desktop_string('"' + str(self.application) + '" ' +
            mode + ' ' + field + ' %c %k') + '\n'
        if explicit:
            text += 'Path=' + str(self.cwd).replace(' ', '\\s') + '\n'
            text += 'NoDisplay=true\nOnlyShowIn=Other;\n'
        path.write_text(text)

    def write_associations(self, default=DEFAULT_ID, removed=False):
        text = '[Default Applications]\ntext/plain=' + default + ';\n'
        text += 'inode/directory=' + DEFAULT_ID + ';\n'
        if removed:
            text += '[Removed Associations]\ntext/plain=' + ALTERNATIVE_ID + ';\n'
        self.associations.write_text(text)

    def wait(self, label, predicate, seconds=10):
        return shared.wait_for_predicate(label, predicate,
            time.monotonic() + seconds, process=self.browser)

    def receipt(self, prefix):
        until = time.monotonic() + 6
        while time.monotonic() < until:
            text = self.control.record().get('echo', {}).get('message', '')
            if text.startswith(prefix):
                return text
        raise AssertionError('missing native receipt ' + prefix)

    def theme(self):
        self.control.send(b'SendToModule FvwmDesktop query-settings file-theme all\n')
        values = {}
        while True:
            text = self.receipt('desktop-settings-file-theme ')
            if text.endswith(' done'):
                break
            _, name, value = text.split(' ', 2)
            values[name] = json.loads(value)
        return values if all(values.get(name, {}).get('effective') == value
            for name, value in [('palette', self.palette),
                ('appearance', self.appearance), ('transparency', False)]) else None

    def rpc(self, scope='state', method='query', expected_error=None, **fields):
        self.serial += 1
        token = 'file-roles-%d' % self.serial
        wire = shared.frame(token, method, resource='files', **fields,
            **({'scope': scope} if 'provider' in fields and method == 'query' else {}))
        self.peer.send(wire)
        value = self.peer.record()['reply']
        assert value['id'] == token, value
        self.evidence.setdefault('rpc_receipts', []).append(dict(
            request=json.loads(wire.removeprefix(b'request ')), reply=value))
        assert len(self.evidence['rpc_receipts']) <= 1024
        if expected_error:
            assert value['status'] == 'error' and value['error']['code'] == expected_error, value
            return value
        if value['status'] == 'error' and value['error']['code'] == 'not_ready':
            return None
        assert value['status'] == 'ok', value
        return value['result']

    def state(self):
        return self.rpc(provider=self.provider)

    def ready(self):
        value = self.state()
        return value if value and not value['busy'] and value['availability']['ready'] and \
            value['bindings']['projection']['state'] == 'current' else None

    def action(self, operation, values=None, expected_error=None):
        return self.rpc(method='action', provider=self.provider,
            generation=self.state()['generation'], operation=operation,
            values=values or {}, expected_error=expected_error)

    def roles(self, scope='file_roles'):
        return self.rpc(scope=scope, provider=self.provider, offset=0, limit=8)

    def roles_ready(self):
        value = self.roles()
        return value if value and value['status'] == 'ready' else None

    def focus(self, target):
        self.x.XSetInputFocus(self.d, target, 1, 0)
        self.x.XSync(self.d, 0)

    def alt_o(self):
        self.focus(self.window)
        code = self.x.XKeysymToKeycode(self.d, self.x.XStringToKeysym(b'Alt_L'))
        assert self.view.pointer.x.XTestFakeKeyEvent(self.d, code, 1, 0)
        try:
            self.view.key('o')
        finally:
            assert self.view.pointer.x.XTestFakeKeyEvent(self.d, code, 0, 0)
            self.x.XSync(self.d, 0)

    def alias(self, target):
        hint = shared.ClassHint()
        try:
            if not self.errors.read(target, 20,
                    lambda: self.x.XGetClassHint(self.d, target, C.byref(hint))):
                return None
            return C.string_at(hint.name).decode() if hint.name else None
        finally:
            if hint.name: self.x.XFree(hint.name)
            if hint.class_: self.x.XFree(hint.class_)

    def chooser(self, status='ready', folder=False):
        title = 'Open folder with' if folder else 'Open file with'
        def current():
            commands = [row['text'] for row in shared.rows(self.native)
                if row['direction'] == 'out']
            prefix = 'Module FvwmForm FvwmFiles-' + self.provider + ('-roles-' if folder else '-file-roles-')
            starts = [text for text in commands if text.startswith(prefix)]
            if not starts:
                return None
            alias = starts[-1].removeprefix('Module FvwmForm ')
            configuration = [text for text in commands if text.startswith('*' + alias + ': ')]
            if not any(re.match(re.escape('*' + alias + ': Label status ') + r'\d+ "' +
                    re.escape(status) + ': ', text) for text in configuration):
                return None
            assert '*' + alias + ': Title "' + title + '"' in configuration
            queue, visited = [(self.client.root, 0)], 0
            while queue:
                target, depth = queue.pop()
                visited += 1
                assert visited <= 4096
                attributes = self.view.attributes(target)
                if attributes and attributes.map_state == 2 and self.alias(target) == alias:
                    forms = self.evidence.setdefault('native_chooser_forms', {})
                    assert len(forms) < 128
                    forms[alias] = dict(window=target, status=status, configuration=configuration)
                    return target, alias
                if depth < 5:
                    queue.extend((child, depth + 1) for child in self.view.children(target))
            return None
        return self.wait('latest ' + status + ' ' + title + ' alias mapped', current)

    def selection_snapshot(self, alias, name='applications'):
        token = 'file-selection-' + os.urandom(4).hex()
        self.control.send(('SendToModule %s ListStatus %s %s selection\n' %
            (alias, name, token)).encode())
        text = self.receipt('FVWM_FORM_LIST ' + token + ' ')
        fields = text.split()
        assert len(fields) == 6 and fields[3:5] == ['0', '0'], text
        packet = base64.urlsafe_b64decode(fields[5][1:] + '==')
        assert len(packet) >= 6
        return dict(generation=int(fields[2]), focus=int.from_bytes(packet[4:6], 'big'),
            selected=[i for i in range((len(packet) - 6) * 8)
                if packet[6 + i // 8] & (1 << (i % 8))], receipt=text)

    def capture(self, name):
        result = subprocess.run([sys.executable, str(self.fixture), '--capture',
            str(self.binaries['capture']), str(self.out)], env=self.env,
            capture_output=True, text=True, timeout=40, check=True)
        path = Path(json.loads(result.stdout))
        target = self.out / name
        path.replace(target)
        self.evidence['screenshots'][name] = shared.digest(target)

    def records_now(self):
        values = [json.loads(path.read_text()) for path in self.records.glob('application-*.json')]
        assert len(values) <= 8
        return values

    def reap(self, value):
        identity = value['identity']
        if identity['child'] in self.reaped:
            return self.reaped[identity['child']]
        def done():
            current = shared.process_identity(identity['child'])
            assert current is None or current['starttime'] == identity['starttime']
            try:
                pid, status = os.waitpid(identity['child'], os.WNOHANG)
            except ChildProcessError:
                return dict(state='already reaped') if current is None else None
            if pid:
                assert os.waitstatus_to_exitcode(status) == 0
                return dict(state='owned adopted child reaped', exit=0)
            return None
        result = self.wait('exact record-and-exit child Wait', done, 5)
        self.reaped[identity['child']] = result
        return result

    def observed(self, mode, count, uri=False):
        values = self.wait('literal application record', lambda:
            self.records_now() if len(self.records_now()) == count else None, 6)
        latest = max(values, key=lambda value: value['recorded'])
        target, label, entry, cwd = (self.target, DEFAULT_LABEL, self.default_entry,
            str(self.cwd)) if mode == 'default' else (self.target, ALTERNATIVE_LABEL,
                self.alternative_entry, str(self.folder))
        if uri:
            raw = base64.b64decode(latest['literal_argv_bytes'][2])
            parsed = urlsplit(raw.decode('ascii'))
            assert parsed.scheme == 'file' and parsed.netloc == '' and not parsed.query and not parsed.fragment
            assert unquote_to_bytes(parsed.path) == self.target
            target = raw
        expected = [str(self.application), mode, target, label, str(entry)]
        assert latest['literal_argv_bytes'] == shared.encoded(expected), latest
        assert latest['cwd_bytes'] == shared.encoded([cwd])[0], latest
        waited = self.reap(latest)
        assert waited.get('exit') == 0, waited
        return dict(record=latest, wait=waited)

    def close(self):
        def clean(name, operation):
            try:
                operation()
            except BaseException as failure:
                self.evidence['status'] = 'failed'
                self.evidence['cleanup_failures'].append(dict(step=name, error=repr(failure)))
        # Stop the exact owned worker tree first so pending classifiers cannot
        # outlive a failed observation. Successful applications are separate
        # adopted children and retain their lifetime until their own exit/Wait.
        if self.browser:
            clean('browser and owned workers', lambda: self.browser.close(time.monotonic() + 5))
        for value in self.records_now():
            def retire(value=value):
                try:
                    self.reap(value)
                except BaseException:
                    identity = value['identity']
                    current = shared.process_identity(identity['child'])
                    assert current and current['starttime'] == identity['starttime'] and current['parent'] == os.getpid()
                    os.kill(identity['child'], signal.SIGKILL)
                    os.waitpid(identity['child'], 0)
                    raise AssertionError('record-and-exit application required forced cleanup')
            clean('recorded application Wait', retire)
        if self.peer: clean('RPC observer', self.peer.close)
        if self.control: clean('core observer', self.control.close)
        if self.core: clean('core', lambda: self.core.close(time.monotonic() + 5))
        if self.errors: clean('observer display', self.errors.close_display)
        elif self.client: clean('observer display', self.client.close)
        for process in reversed(self.runtime.processes):
            clean('stop ' + process.name, lambda process=process: process.close(time.monotonic() + 4))
        for path in self.classifiers.glob('held-*.json'):
            held = json.loads(path.read_text())
            for key in ('worker_identity', 'classifier_identity'):
                if held[key] not in self.held_children:
                    self.held_children.append(held[key])
        for identity in self.held_children:
            def retire_held(identity=identity):
                state = identity_state(identity)
                if state is None:
                    return
                assert state['parent'] == os.getpid(), state
                if state['state'] != 'Z':
                    os.kill(identity['child'], signal.SIGKILL)
                    os.waitpid(identity['child'], 0)
                    raise AssertionError('owned held descendant survived browser exit')
                pid, status = os.waitpid(identity['child'], os.WNOHANG)
                assert pid == identity['child']
                self.evidence.setdefault('held_cleanup_waits', []).append(dict(
                    identity=identity, exit=os.waitstatus_to_exitcode(status)))
            clean('recorded held descendant Wait', retire_held)
        for path in self.classifiers.glob('classifier-*.json'):
            value = json.loads(path.read_text())
            identity = value['identity']
            def classifier_retired(identity=identity):
                current = shared.process_identity(identity['child'])
                if current is None or current['starttime'] != identity['starttime']:
                    return
                assert current['parent'] == os.getpid(), current
                # A surviving adopted owned classifier is a failed cleanup,
                # even when this last-resort exact-PID retirement succeeds.
                os.kill(identity['child'], signal.SIGKILL)
                os.waitpid(identity['child'], 0)
                raise AssertionError('classifier survived browser/worker cleanup')
            clean('classifier descendant retired', classifier_retired)
        self.evidence['recorded_application_waits'] = self.reaped
        self.evidence['children_after_cleanup'] = {p.name: p.poll() for p in self.runtime.processes}
        clean('native child Wait receipts', lambda:
            self.evidence.update(module_cleanup=shared.cleanup_receipts(self.native)))
        for path in self.root.iterdir():
            if path in (self.records, self.native, self.jobs, self.classifiers):
                clean('copy ' + path.name, lambda path=path: shutil.copytree(path, self.out / path.name))
            elif path.is_file() and path.suffix == '.log':
                clean('copy ' + path.name, lambda path=path: shutil.copy2(path, self.out / path.name))
        if self.evidence.get('observer_unexpected_x_errors'):
            self.evidence['status'] = 'failed'


def run(args, profile, root):
    out = root / profile
    out.mkdir()
    evidence = dict(status='running', profile=profile, cases={}, screenshots={},
        cleanup_failures=[], exit_cases_only=args.exit_cases_only,
        transport_loss_only=args.transport_loss_only,
        limits=dict(full_platform_release=False,
            native_cpu=False, classifier_cancellation='separate lead-owned unit cases',
            physical_assistive=False, broader_mime_database=False))
    source = Path(__file__).resolve().parents[2]
    inputs = ['tests/live_x/file_roles_live.py', 'tests/live_x/directory_roles_live.py',
        'tests/live_x/native_module_proxy.py', 'tests/live_x/native_bindings_live.py',
        'tests/live_x/desktop_applications_live.py', 'tests/live_x/browser_handoff_live.py',
        'tests/live_x/browser_continuity_live.py', 'tests/live_x/live_x_harness.py',
        'tests/live_x/inspector_live.py', 'tests/live_x/native_capture.py',
        'tests/live_x/session_x_errors.py', 'modules/FvwmMFL/FvwmMFL.c',
        'modules/FvwmMFL/request.c', 'modules/FvwmMFL/request.h']
    inputs += [str(path.relative_to(source)) for group in ('cmd/fvwmfilebrowser',
        'internal/xdgmenu', 'internal/xdgsource') for path in sorted(
        (source / 'bin/FvwmPrompt' / group).glob('*.go'))]
    evidence['source_sha256'] = {name: shared.digest(source / name) for name in inputs}
    with shared.HarnessRuntime(prefix='fvwm-file-roles-') as runtime:
        session = Session(args, profile, out, runtime, evidence)
        try:
            session.start()
            if not args.exit_cases_only:
                exercise(session, evidence)
            if profile == 'slate-dark':
                exercise_worker_exit(session, evidence, fresh_first=not args.exit_cases_only)
            evidence['status'] = 'passed'
        except BaseException as failure:
            evidence.update(status='failed', failure=repr(failure))
            raise
        finally:
            session.close()
            shared.atomic_json(out / 'result.json', evidence)
    assert evidence['status'] == 'passed', evidence
    print(json.dumps(dict(profile=profile, status='passed', artifacts=str(out))))


def exercise(s, evidence):
    cases = evidence['cases']
    page = s.rpc(scope='items', provider=s.provider, offset=0, limit=8)
    assert page['total'] == 3
    by_name = {base64.b64decode(item['name_bytes']): item for item in page['items']}
    target = by_name[os.path.basename(s.target)]
    other = by_name[os.path.basename(s.other)]
    link = by_name[os.path.basename(s.link)]

    def choose(items):
        values = dict(mode='replace', ids=[item['id'] for item in items]) if items else dict(mode='clear')
        s.action('selection', values)
        observed = s.rpc(scope='selection', provider=s.provider, offset=0, limit=8)
        assert {item['id'] for item in observed['items']} == {item['id'] for item in items}
        return observed

    def open_choices(native=False):
        previous = s.roles()['choice_generation']
        if native:
            s.alt_o()
        else:
            s.action('file-roles')
        value = s.wait('fresh selected-file choices', lambda:
            (v if (v := s.roles_ready()) and v['choice_generation'] != previous else None))
        assert value['mime'] == 'text/plain' and value['canonical_mime'] == 'text/plain', value
        assert value['observed'] and value['file_content'] is False, value
        assert re.fullmatch(r'[0-9a-f]{32}', value['choice_generation']), value
        assert len(value['items']) == 2 and value['total'] == 2 and value['next_offset'] is None
        assert {item['label'] for item in value['items']} == {DEFAULT_LABEL, ALTERNATIVE_LABEL}
        assert all(re.fullmatch(r'[0-9a-f]{32}', item['id']) and
            re.fullmatch(r'[0-9a-f]{64}', item['fingerprint']) for item in value['items'])
        return value

    def launch_values(choices, label=ALTERNATIVE_LABEL):
        return dict(generation=choices['choice_generation'],
            id=next(item['id'] for item in choices['items'] if item['label'] == label))

    def refresh_collection():
        nonlocal target, other, link
        s.action('refresh')
        s.wait('restored collection refreshed', s.ready)
        page = s.rpc(scope='items', provider=s.provider, offset=0, limit=8)
        by_name = {base64.b64decode(item['name_bytes']): item for item in page['items']}
        target, other, link = [by_name[os.path.basename(path)]
            for path in (s.target, s.other, s.link)]
        choose([target])

    def failed_launch(name, mutate, restore, screenshot=False, collection=False):
        choices = open_choices(native=True)
        values = launch_values(choices)
        count = len(s.records_now())
        mutate()
        admitted = s.action('file-role-launch', values)
        failed = s.wait(name + ' refused before launch', lambda:
            (value if (value := s.state())['work'] and value['work']['status'] == 'failed' else None))
        assert len(s.records_now()) == count
        window, alias = s.chooser('failed')
        cases[name] = dict(admission=admitted, state=failed, alias=alias, window=window)
        if screenshot:
            s.capture('file-refused-handler.png')
        restore()
        if collection:
            # A chooser must never silently adopt a replaced target. Renew
            # collection authority and explicitly reselect before reopening.
            refresh_collection()
            fresh = open_choices(native=True)
        else:
            s.focus(s.chooser('failed')[0])
            s.view.key('F5')
            fresh = s.wait(name + ' F5 recovered', lambda:
                (value if (value := s.roles_ready()) and
                    value['choice_generation'] != choices['choice_generation'] else None))
        s.action('file-role-launch', values, expected_error='stale_choices')
        assert len(s.records_now()) == count
        cases[name]['recovered'] = fresh
        return fresh

    choose([target])
    choices = open_choices(native=True)
    window, alias = s.chooser()
    native = s.selection_snapshot(alias)
    default_index = next(index for index, item in enumerate(choices['items']) if item['default'])
    assert choices['default_id'] == choices['items'][default_index]['id']
    assert native['focus'] == default_index and native['selected'] == [default_index], native
    cases['native_default_selection'] = dict(choices=choices, native=native)
    s.capture('file-initial-chooser.png')
    s.focus(window)
    s.view.key('Return')
    cases['native_default_literal_uri_and_entry_cwd'] = s.observed('default', 1, uri=True)
    s.wait('native launch worker completed', s.ready)
    classifier_records = [json.loads(path.read_text()) for path in s.classifiers.glob('*.json')]
    assert len(classifier_records) >= 2, classifier_records
    for record in classifier_records:
        assert record['argv_bytes'] == shared.encoded(['-b', '--mime-type', s.target]), record
        assert record['executable_sha256'] == evidence['real_classifier']['sha256']
    cases['actual_file_classifier'] = classifier_records

    choices = open_choices()
    s.write_associations(ALTERNATIVE_ID)
    # Captured default launches the originally selected handler even after the
    # default changes. An alternative launch below proves raw %f separately.
    s.action('file-role-launch', launch_values(choices, DEFAULT_LABEL))
    cases['default_change_cannot_redirect_explicit_choice'] = s.observed('default', 2, uri=True)
    s.wait('default change launch completed', s.ready)
    s.write_associations()
    choices = open_choices()
    s.action('file-role-launch', launch_values(choices))
    cases['typed_literal_path_and_parent_cwd'] = s.observed('alternative', 3)
    s.wait('typed launch completed', s.ready)

    failed_launch('changed_handler', lambda: s.write_handler(s.alternative_entry,
        ALTERNATIVE_LABEL, 'changed', '%f'), lambda: s.alternative_entry.write_bytes(
            s.original_inputs[s.alternative_entry]), screenshot=True)
    failed_launch('removed_association', lambda: s.write_associations(removed=True), s.write_associations)

    def write_target(content):
        with open(s.target, 'wb') as stream:
            stream.write(content)

    def replace_target():
        replacement = os.fsencode(s.folder) + b'/replacement'
        with open(replacement, 'wb') as stream:
            stream.write(TEXT)
        os.replace(replacement, s.target)

    # Preserve size/mtime for the content-only case: MIME must be refreshed,
    # rather than relying solely on snapshot identity/metadata comparisons.
    before = os.stat(s.target)
    def change_type():
        write_target(b'\0' * len(TEXT))
        os.utime(s.target, ns=(before.st_atime_ns, before.st_mtime_ns))
        after = os.stat(s.target)
        fields = ('st_dev', 'st_ino', 'st_mode', 'st_size', 'st_mtime_ns')
        assert all(getattr(after, field) == getattr(before, field) for field in fields)
        cases['changed_content_identity_preserved'] = {field: getattr(after, field) for field in fields}
    failed_launch('changed_content_mime', change_type, lambda: write_target(TEXT), collection=True)
    failed_launch('replaced_identity', replace_target, lambda: None, collection=True)
    failed_launch('deleted_target', lambda: os.unlink(s.target),
        lambda: write_target(TEXT), collection=True)

    for name, items in [('empty_selection', []), ('multiple_selection', [target, other]),
            ('symlink_selection', [link])]:
        selected = choose(items)
        before_count = len(s.records_now())
        refusal = s.action('file-roles', expected_error='selection_unavailable')
        assert len(s.records_now()) == before_count
        cases[name + '_refused'] = dict(selection=selected, refusal=refusal)

    choose([target])
    choices = open_choices(native=True)
    old = launch_values(choices)
    choose([other])
    invalidated = s.roles()
    assert invalidated['status'] == 'closed', invalidated
    s.action('file-role-launch', old, expected_error='choices_unavailable')
    cases['selection_change_invalidates_capture'] = invalidated
    choose([target])
    choices = open_choices(native=True)
    (s.jobs / 'hold-file-roles').touch()
    s.focus(s.chooser()[0])
    s.view.key('F5')
    held = s.wait('actual discovery worker held', lambda:
        json.loads((s.jobs / 'held-file-roles.json').read_text())
        if (s.jobs / 'held-file-roles.json').exists() else None, 5)
    s.action('file-roles', expected_error='busy')
    (s.jobs / ('release-' + held['token'])).touch()
    recovered = s.wait('held worker actual result', s.roles_ready, 6)
    cases['duplicate_busy_refused'] = held
    s.chooser()
    s.capture('file-recovered-chooser.png')
    s.focus(s.chooser()[0])
    s.view.key('Escape')
    cases['native_escape_closes'] = s.wait('Escape publishes closed state', lambda:
        (value if (value := s.roles())['status'] == 'closed' else None), 5)
    open_choices()
    s.action('file-roles-close')
    assert s.roles()['status'] == 'closed'
    cases['typed_close'] = True
    s.focus(s.window)
    s.view.key('F12')
    directory = s.wait('F12 preserves folder chooser', lambda:
        (value if (value := s.roles('directory_roles'))['status'] == 'ready' else None))
    s.chooser(folder=True)
    assert len(directory['items']) == 2
    cases['F12_current_folder_unchanged'] = directory
    assert len(s.records_now()) == 3
    assert not any((path / 'marker').exists() for path in
        (s.folder, s.cwd, s.root)), 'raw file reached shell expansion'
    for path, original in s.original_inputs.items():
        assert path.read_bytes() == original, path
    cases['no_default_write_or_shell'] = True


def exercise_worker_exit(s, evidence, fresh_first=True):
    """Two fresh Slate browser instances share the already owned core/server."""
    if fresh_first:
        s.action('directory-roles-close')
        s.focus(s.window)
        s.view.key('Escape')
        predecessor_exit = s.browser.wait_until_exit(time.monotonic() + 5)
        assert predecessor_exit == 0
        evidence['exit_case_predecessor_wait'] = dict(exit=predecessor_exit)
    modes = ('transport-loss',) if s.args.transport_loss_only else ('native-close', 'transport-loss')
    for index, mode in enumerate(modes):
        name = 'held_classifier_' + mode.replace('-', '_')
        case = dict(status='running', mode=mode)
        evidence['cases'][name] = case
        if fresh_first or index > 0:
            s.fresh_browser(mode)
        page = s.rpc(scope='items', provider=s.provider, offset=0, limit=8)
        target = next(item for item in page['items']
            if base64.b64decode(item['name_bytes']) == os.path.basename(s.target))
        s.action('selection', dict(mode='replace', ids=[target['id']]))
        browser = shared.process_identity(s.browser.process.pid)
        assert browser and browser['parent'] == os.getpid()
        token = os.urandom(8).hex()
        shared.atomic_json(s.classifiers / 'hold-next.json', dict(
            token=token, browser_identity=browser))
        mfl = None
        if mode == 'transport-loss':
            modules = [json.loads(path.read_text()) for path in s.native.glob('module-*.json')]
            matching = [value for value in modules
                if value['real'] == str(s.binaries['MFL'])]
            assert len(matching) == 1, matching
            mfl = matching[0]
            mfl_identity = shared.child_identity(mfl['child'], mfl['proxy'])
            assert mfl_identity, mfl
            case['mfl_before'] = dict(record=mfl, identity=mfl_identity)
        s.alt_o()
        held_path = s.classifiers / ('held-' + token + '.json')
        held = s.wait('real classifier held before provider exec', lambda:
            json.loads(held_path.read_text()) if held_path.exists() else None, 1)
        worker, classifier_identity = held['worker_identity'], held['classifier_identity']
        s.held_children.extend((worker, classifier_identity))
        case['held'] = held
        forwarded = [json.loads(path.read_text()) for path in s.jobs.glob('*.json')]
        forwarded = [value for value in forwarded if value.get('worker') == worker['child']
            and value.get('forwarded') and value.get('request', {}).get('operation') == 'file-roles']
        assert forwarded
        request = max(forwarded, key=lambda value: value['started'])
        case['worker_forwarded_request'] = request
        assert held['browser_identity'] == browser
        assert held['controlled_before_real_provider'] is True
        assert worker['parent'] == browser['child']
        assert classifier_identity['parent'] == worker['child']
        worker_state, classifier_state = identity_state(worker), identity_state(classifier_identity)
        assert worker_state and worker_state['state'] != 'Z', worker_state
        assert classifier_state and classifier_state['state'] != 'Z', classifier_state
        assert worker_state['group'] == worker['child'] == classifier_state['group'] == held['group']
        # The held Python classifier creates no children. Record the complete
        # known worker subtree before cancellation instead of signalling a
        # possibly recycled process-group number after its leader has exited.
        worker_children = direct_children(worker)
        classifier_children = direct_children(classifier_identity)
        assert worker_children == [classifier_identity['child']], worker_children
        assert classifier_children == [], classifier_children
        case['owned_tree_before_exit'] = dict(worker=worker_state,
            classifier=classifier_state, worker_children=worker_children,
            classifier_children=classifier_children)
        heartbeat = s.classifiers / held['heartbeat']
        assert heartbeat.is_file()
        if mode == 'transport-loss':
            # Complete a pure provider round trip before closing its producer:
            # the browser has finished native publication and is receiving
            # events, rather than still awaiting a native configuration ACK.
            case['browser_before_transport_loss'] = s.state()
            assert case['browser_before_transport_loss']['busy'] is True
        case['trigger_ns'] = time.monotonic_ns()
        if mode == 'native-close':
            s.focus(s.window)
            s.view.key('Escape')
        else:
            # Core consumes this existing native module command and closes
            # only this session's MFL producer. No synthetic socket EOF or new
            # proxy service supplies the observed transport failure.
            s.control.send(b'KillModule FvwmMFL\n')
        returncode = s.browser.wait_until_exit(time.monotonic() + 5)
        exited_ns = time.monotonic_ns()
        case['browser_wait'] = dict(identity=browser, exit=returncode,
            observed_exit_ns=exited_ns)
        # Session.Read returns scanner.Err(): clean producer EOF is nil. run
        # returns that result through finished; main exits zero, just as for
        # native Close. Truncated/malformed records remain error paths.
        assert returncode == 0, case
        if mode == 'native-close':
            callbacks = [row for row in shared.rows(s.native) if row['direction'] == 'out'
                and row['text'].startswith('Echo FVWM_FILES_3 ' + s.provider + ' ')
                and len(row['text'].split()) == 8
                and row['text'].split()[4:6] == ['close', '-']]
            assert len(callbacks) == 1, callbacks
            case['actual_native_close_callback'] = callbacks[0]
        # This guard distinguishes prompt cancellation from the independent
        # two-second classifier timeout. It is causal test setup, not a speed
        # or native-CPU measurement.
        assert exited_ns - int(request['started'] * 1_000_000_000) < 1_800_000_000, case
        assert s.core.poll() is None, 'transport fixture stopped the WM core'
        states = dict(worker=identity_state(worker), classifier=identity_state(classifier_identity))
        case['identities_at_browser_wait'] = states
        assert states['worker'] is None, 'browser exited before reaping its owned worker'
        assert states['classifier'] is None or states['classifier']['state'] == 'Z', states
        first_heartbeat = heartbeat.read_bytes()
        samples = [json.loads(line) for line in first_heartbeat.splitlines()]
        assert samples and len(samples) <= 1024
        assert all(row['time_ns'] <= exited_ns and row['browser_state'] not in ('Z', 'X', 'absent')
            for row in samples), samples
        select.select([], [], [], .05)
        assert heartbeat.read_bytes() == first_heartbeat, 'classifier heartbeat continued after browser Wait'
        assert not (s.classifiers / ('hold-expired-' + token + '.json')).exists()
        case['heartbeat'] = dict(sha256=hashlib.sha256(first_heartbeat).hexdigest(),
            samples=len(samples), last_ns=samples[-1]['time_ns'],
            unchanged_after_browser_wait=True,
            limit='exit ordering is observed at actual Wait plus kernel state, not an exact kernel exit timestamp')
        waits = []
        for identity in (worker, classifier_identity):
            current = identity_state(identity)
            try:
                pid, status = os.waitpid(identity['child'], os.WNOHANG)
            except ChildProcessError:
                assert current is None, current
                waits.append(dict(identity=identity, state='reaped by original parent before browser exit'))
            else:
                assert pid == identity['child'] and current and current['state'] == 'Z', (identity, current, pid)
                assert current['parent'] == os.getpid(), current
                waits.append(dict(identity=identity, state='owned adopted child reaped',
                    exit=os.waitstatus_to_exitcode(status)))
            assert identity_state(identity) is None
        case['descendant_waits'] = waits
        if mode == 'transport-loss':
            waited_path = s.native / ('stopped-%d.json' % mfl['proxy'])
            waited = shared.wait_for_predicate('actual MFL proxy child Wait', lambda:
                json.loads(waited_path.read_text()) if waited_path.exists() else None,
                time.monotonic() + 5, process=s.core)
            assert waited['child'] == mfl['child'] and waited['returncode'] is not None
            assert identity_state(mfl_identity) is None
            s.peer.expect_closed()
            assert any(row['direction'] == 'out' and row['text'] == 'KillModule FvwmMFL'
                for row in shared.rows(s.native))
            case['transport_eof'] = dict(module_wait=waited, observer_eof=True,
                browser_log=s.browser.log_path.read_text()[-8192:])
        case['status'] = 'passed'


def main():
    if len(sys.argv) > 1 and sys.argv[1] == '--module-proxy':
        return shared.module_proxy(sys.argv[2], sys.argv[3], sys.argv[4:])
    if len(sys.argv) == 4 and sys.argv[1] == '--browser-worker':
        return shared.browser_worker(sys.argv[2], sys.argv[3])
    if len(sys.argv) > 1 and sys.argv[1] == '--classifier':
        return classifier(sys.argv[2], sys.argv[3], sys.argv[4:])
    if len(sys.argv) > 1 and sys.argv[1] == '--application':
        return shared.record_application(sys.argv[2], sys.argv[3], sys.argv[4:])
    if len(sys.argv) == 4 and sys.argv[1] == '--capture':
        print(json.dumps(shared.NativeCapture(sys.argv[2]).screenshot(
            sys.argv[3]).removeprefix('Screenshot saved: ')))
        return 0
    parser = argparse.ArgumentParser(description=__doc__, allow_abbrev=False)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--profiles', nargs='+', choices=PROFILES, default=list(PROFILES))
    parser.add_argument('--artifacts-dir', type=Path, required=True)
    parser.add_argument('--exit-cases-only', action='store_true',
        help='run only the two Slate held-classifier browser exit cases')
    parser.add_argument('--transport-loss-only', action='store_true',
        help='run only the Slate held-classifier producer EOF case')
    args = parser.parse_args()
    if args.transport_loss_only:
        args.exit_cases_only = True
    if args.exit_cases_only:
        args.profiles = ['slate-dark']
    if sys.platform != 'linux':
        parser.error('run in the owning Linux guest; exact child Wait needs subreaping')
    libc = C.CDLL(None, use_errno=True)
    libc.prctl.argtypes = [C.c_int, C.c_ulong, C.c_ulong, C.c_ulong, C.c_ulong]
    if libc.prctl(36, 1, 0, 0, 0) != 0:
        raise RuntimeError('cannot own detached fixture descendants for cleanup')
    args.artifacts_dir.mkdir(parents=True, exist_ok=True)
    root = Path(tempfile.mkdtemp(prefix='file-roles-', dir=args.artifacts_dir.resolve()))
    print(json.dumps(dict(artifacts_dir=str(root))), flush=True)
    for profile in args.profiles:
        run(args, profile, root)
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
