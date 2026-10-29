#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or any later version.
"""Independent startup sender, native observer, helper and activity-form checks.

The lead compiles startup_notification_client.c against generated config.h/X11,
then runs this fixture on Linux. --mode protocol --expiry spends one real expiry
interval; UI themes never repeat it. No candidate parser or reply is substituted.
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
import socket
import stat
import subprocess
import sys
import tempfile
import time

import directory_roles_live as shared
from live_x_harness import ManagedProcess
from native_bindings_live import menu_rows

PROFILES = shared.PROFILES
STATES = {'pending', 'protocol-ended', 'window-observed', 'expired'}
EVIDENCE_FILE_MAX = 4 * 1024 * 1024
EVIDENCE_TOTAL_MAX = 16 * 1024 * 1024
EVIDENCE_ENTRIES_MAX = 512


def pause_until(deadline):
    while time.monotonic() < deadline:
        select.select([], [], [], max(0, min(.05, deadline - time.monotonic())))


def application(directory, arguments):
    """A fixture-owned eventual application, independent of announcement code."""
    root = Path(directory)
    token = arguments[0]
    identity = shared.child_identity(os.getpid(), os.getppid())
    assert identity
    shared.atomic_json(root / ('application-' + token + '-%d.json' % os.getpid()), dict(token=token,
        identity=identity, group=os.getpgrp(), argv_bytes=shared.encoded(arguments),
        cwd_bytes=shared.encoded([os.getcwd()])[0],
        startup_id=os.environ.get('DESKTOP_STARTUP_ID'), started=time.monotonic()))
    deadline = time.monotonic() + 5
    while not (root / ('release-' + token)).exists():
        if time.monotonic() >= deadline:
            return 3
        select.select([], [], [], .005)
    return 0


class XClient:
    def __init__(self, session, name):
        self.session, self.name, self.pending = session, name, bytearray()
        self.log = (session.runtime.root / (name + '.log')).open('wb')
        self.process = subprocess.Popen([str(session.args.client.resolve())],
            stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=self.log,
            env=session.env, start_new_session=True)
        session.clients.append(self)
        os.set_blocking(self.process.stdout.fileno(), False)
        ready = self.line()
        fields = ready.split()
        assert len(fields) == 3 and fields[0] == 'READY', ready
        self.root, self.screen = map(int, fields[1:])
        self.windows = {}
        self.streams, self.messages = {}, []

    def line(self, seconds=5):
        deadline = time.monotonic() + seconds
        while b'\n' not in self.pending:
            remaining = deadline - time.monotonic()
            assert remaining > 0, 'native client response deadline'
            ready, _, _ = select.select([self.process.stdout], [], [], min(.05, remaining))
            if not ready:
                assert self.process.poll() is None
                continue
            block = os.read(self.process.stdout.fileno(), 4096)
            assert block, 'native client exited before response'
            self.pending.extend(block)
            assert len(self.pending) <= 8192
        line, self.pending = self.pending.split(b'\n', 1)
        return line.decode('ascii')

    def send(self, command):
        assert command.isascii() and '\n' not in command and len(command) < 16382
        self.process.stdin.write((command + '\n').encode())
        self.process.stdin.flush()

    def command(self, command, seconds=5):
        self.send(command)
        answer = self.line(seconds)
        fields = answer.split()
        assert len(fields) == 4 and fields[:2] == ['OK', command.split()[0]], answer
        self.session.evidence.setdefault('native_commands', []).append(dict(
            client=self.name, command=command, reply=answer))
        assert len(self.session.evidence['native_commands']) <= 4096
        return int(fields[3])

    def create(self, slot):
        self.windows[slot] = self.command('create %d' % slot)
        return self.windows[slot]

    def fragment(self, slot, data, begin=False, format_=8):
        assert len(data) <= 20
        return self.command('fragment %d %s %d %s' %
            (slot, 'begin' if begin else 'continue', format_, data.hex() or '-'))

    def drain(self):
        self.send('drain 0')
        for _ in range(4097):
            line = self.line()
            if line.startswith('OK drain 0 '):
                return self.messages
            fields = line.split()
            assert len(fields) == 5 and fields[0] == 'EVENT', line
            producer, begin, format_ = map(int, fields[1:4])
            raw = bytes.fromhex(fields[4])
            assert len(raw) == 20 and format_ == 8
            self.session.evidence.setdefault('public_startup_fragments', []).append(line)
            assert len(self.session.evidence['public_startup_fragments']) <= 4096
            if begin:
                self.streams[producer] = bytearray()
            if producer not in self.streams:
                continue
            assert len(self.streams) <= 128
            self.streams[producer].extend(raw)
            assert len(self.streams[producer]) <= 4096
            if b'\0' in self.streams[producer]:
                text = self.streams.pop(producer).split(b'\0', 1)[0].decode('utf-8')
                self.messages.append(dict(producer=producer, text=text))
                assert len(self.messages) <= 512
        raise AssertionError('bounded public event drain exhausted')

    def close(self):
        try:
            if self.process.poll() is None:
                self.send('quit')
                try:
                    self.process.wait(timeout=2)
                except subprocess.TimeoutExpired:
                    os.killpg(self.process.pid, signal.SIGKILL)
                    self.process.wait(timeout=2)
                    raise AssertionError('native fixture client required forced cleanup')
            assert self.process.returncode == 0, self.process.returncode
        finally:
            try:
                self.log.close()
            finally:
                self.session.evidence.setdefault('native_client_waits', {})[self.name] = self.process.returncode


def quoted(value):
    return '"' + value.replace('\\', '\\\\').replace('"', '\\"') + '"'


def message(verb, **fields):
    return (verb + ': ' + ' '.join(key + '=' + quoted(str(value))
        for key, value in fields.items())).encode('utf-8') + b'\0'


def fragments(data):
    return [data[index:index + 20] for index in range(0, len(data), 20)]


def send(client, slot, data):
    for index, part in enumerate(fragments(data)):
        client.fragment(slot, part, begin=index == 0)


class Session:
    def __init__(self, args, phase, profile, out, runtime):
        self.args, self.phase, self.profile, self.out, self.runtime = args, phase, profile, out, runtime
        self.source = Path(__file__).resolve().parents[2]
        self.fixture = Path(__file__).resolve()
        self.clients, self.core, self.peer, self.control = [], None, None, None
        self.client = self.errors = None
        self.serial = 0
        self.evidence = dict(status='running', phase=phase, profile=profile,
            cases={}, screenshots={}, cleanup_failures=[], limits=dict(
                native_cpu_hardware_release=False, exact_kernel_exit_timestamp=False,
                late_unmanaged_group_property_changes='deferred by contract'))

    def script(self, path, arguments, suffix=''):
        path.write_text('#!/bin/sh\nexec ' + shlex.join([sys.executable,
            str(self.fixture), *map(str, arguments)]) + suffix + '\n')
        path.chmod(0o700)

    def start(self):
        build, root = self.args.build.resolve(), self.runtime.root
        self.binaries = dict(core=build / 'fvwm3', Desktop=build / 'bin/FvwmPrompt/FvwmDesktop',
            worker=build / 'bin/FvwmPrompt/fvwm-desktop-worker',
            MFL=build / 'modules/FvwmMFL/FvwmMFL', Form=build / 'modules/FvwmForm/FvwmForm',
            capture=build / 'bin/fvwm-desktop-capture', helper=self.args.startup_helper.resolve(),
            client=self.args.client.resolve())
        self.evidence['binaries'] = {key: dict(path=str(path.resolve()), sha256=shared.digest(path))
            for key, path in self.binaries.items()}
        inputs = ['tests/live_x/startup_notification.py', 'tests/live_x/startup_notification_client.c',
            'tests/live_x/directory_roles_live.py', 'tests/live_x/live_x_harness.py',
            'tests/live_x/native_module_proxy.py', 'tests/live_x/native_bindings_live.py',
            'tests/live_x/inspector_live.py', 'tests/live_x/native_capture.py',
            'dev-docs/decisions/STARTUP-NOTIFICATION-2026-10-28.md']
        self.evidence['source_sha256'] = {name: shared.digest(self.source / name) for name in inputs}
        _, display = shared.start_display_server(self.runtime, 'Xvfb',
            ['Xvfb', '-screen', '0', '1440x1200x24', '-noreset'],
            'xserver.log', time.monotonic() + 10)
        self.native, self.records, self.user, self.apps = [root / name for name in
            ('native-records', 'application-records', 'user', 'data/applications')]
        modules = root / 'modules'
        self.cwd = root / 'literal working directory'
        for path in (self.native, self.records, self.user, self.apps, modules, self.cwd):
            path.mkdir(mode=0o700, parents=True)
        for directory in (build / 'modules').iterdir():
            executable = directory / directory.name
            if executable.is_file() and directory.name not in ('FvwmForm', 'FvwmMFL'):
                (modules / directory.name).symlink_to(executable)
        self.private_helper = root / 'launch-copy' / 'fvwm-startup-launch'
        desktop_image = self.binaries['Desktop']
        if self.phase != 'protocol':
            self.private_helper.parent.mkdir(mode=0o700)
            desktop_image = self.private_helper.parent / 'FvwmDesktop'
            # A hard link gives the actual image an isolated discovery path
            # without copying a large Go executable in the constrained lab.
            os.link(self.binaries['Desktop'].resolve(), desktop_image)
            self.private_helper.symlink_to(self.binaries['helper'])
            self.evidence['owned_desktop_image'] = dict(path=str(desktop_image),
                sha256=shared.digest(desktop_image), hard_link=True)
        for name, key in [('FvwmForm', 'Form'), ('FvwmMFL', 'MFL'), ('FvwmDesktop', 'Desktop')]:
            executable = desktop_image if name == 'FvwmDesktop' else self.binaries[key]
            self.script(modules / name, ['--module-proxy', executable, self.native],
                ' "$@" 2>>' + shlex.quote(str(root / (name + '.log'))))
        self.application = root / 'record application'
        self.script(self.application, ['--application', self.records], ' "$@"')
        self.write_entries()
        assets = self.source / 'default-config'
        palette, appearance = ('dune', 'light') if self.profile == 'dune-light' else ('slate', 'dark')
        (self.user / 'local.config').write_text('*FvwmDesktop: palette ' + palette +
            '\n*FvwmDesktop: appearance ' + appearance +
            '\n*FvwmDesktop: motion Off\n*FvwmDesktop: transparency Off\n')
        body = ''
        if self.phase != 'protocol':
            body = (assets / 'config').read_text().replace('$[FVWM_DATADIR]/default-config', str(assets))
            if self.profile == 'classic-large':
                body = body.replace('Sans:size=10:antialias=True',
                    'DejaVu Sans Mono:size=18:antialias=True')
                body += '\n*FvwmFormDefault: Fore wheat\n*FvwmFormDefault: Back navy\n'
        config = root / 'config.fvwm'
        config.write_text('ModulePath ' + str(modules) + '\nRead ' +
            str(self.source / 'fvwm/ConfigFvwmDefaults') + '\n' + body +
            '\nAddToFunc StartFunction I Module FvwmMFL\n')
        self.env = self.runtime.environment(dict(DISPLAY=display, HOME=str(root),
            FVWM_USERDIR=str(self.user), FVWM_DESKTOP_ROOT=str(assets),
            FVWM_DESKTOP_WORKER=str(self.binaries['worker']), FVWMMFL_SOCKET_PATH=str(root),
            XDG_DATA_HOME=str(root / 'data'), XDG_DATA_DIRS=str(root / 'absent-data'),
            XDG_CONFIG_HOME=str(root / 'config'), XDG_CONFIG_DIRS=str(root / 'absent-config'),
            XDG_CURRENT_DESKTOP='FVWM', LC_ALL='C', DESKTOP_STARTUP_ID='must-clear-inherited-ID'))
        self.evidence['configuration_sha256'] = {name: shared.digest(path) for name, path in
            [('config', config), ('local', self.user / 'local.config')]}
        self.core = self.runtime.start('core', [str(self.binaries['core']), '-d', display,
            '-f', str(config)], 'core.log', self.env)
        endpoint = root / ('fvwm_mfl_' + display.replace(':', '_') + '.sock')
        shared.wait_for_predicate('startup MFL endpoint', endpoint.exists,
            time.monotonic() + 12, process=self.core)
        self.control, self.peer = shared.Connection(endpoint), shared.Connection(endpoint)
        self.control.record(); self.peer.record()
        self.control.send(b'set echo\n')
        self.control.echo('startup-fixture-ready')
        self.client = shared.connect_client(display)
        self.errors = shared.SessionXErrors(self.client, self.evidence).install()
        self.view = shared.NativeView(self.client, self.errors, self.core)
        self.sender = XClient(self, 'sender')
        self.sender.create(0)

    def write_entries(self):
        for token, extra in [('true', 'StartupNotify=true\nStartupWMClass=StartupTrue\n'),
                ('false', 'StartupNotify=false\nStartupWMClass=StartupFalse\n'),
                ('legacy', 'StartupWMClass=StartupLegacy\n'), ('ordinary', '')]:
            entry = self.apps / ('fixture-' + token + '.desktop')
            text = '[Desktop Entry]\nType=Application\nName=Startup ' + token + '\n'
            text += 'Exec=' + shared.desktop_string('"' + str(self.application) + '" ' + token +
                ' "space literal" "semi;literal"') + '\n'
            text += 'Path=' + str(self.cwd).replace(' ', '\\s') + '\nTerminal=false\n' + extra
            entry.write_text(text)

    def query(self, retries=3):
        result, offset, generation, items = None, 0, None, []
        while True:
            self.serial += 1
            token = 'startup-query-%d' % self.serial
            wire = shared.frame(token, resource='startups', offset=offset, limit=16,
                **({'generation': generation} if generation is not None else {}))
            self.peer.send(wire)
            reply = self.peer.record()['reply']
            self.evidence.setdefault('query_receipts', []).append(dict(request=json.loads(
                wire.removeprefix(b'request ')), reply=reply))
            assert len(self.evidence['query_receipts']) <= 4096
            if offset and retries and reply.get('status') == 'error' and \
                    reply.get('error', {}).get('code') == 'stale_generation':
                return self.query(retries - 1)
            assert reply['id'] == token and reply['status'] == 'ok', reply
            page = reply['result']
            assert page['scope'] == 'core-startup-activity'
            assert re.fullmatch(r'[0-9a-f]{48}', page['generation'])
            if generation is None:
                result, generation = page, page['generation']
            assert page['generation'] == generation and page['offset'] == offset
            assert len(page['startups']) <= 16 and page['total'] <= 64
            assert all(item['state'] in STATES and isinstance(item['window'], int)
                and item['age_ms'] >= 0 for item in page['startups'])
            items.extend(page['startups'])
            if page['next_offset'] is None:
                assert len(items) == page['total']
                return dict(result, startups=items)
            assert page['next_offset'] > offset
            offset = page['next_offset']

    def wait(self, label, predicate, seconds=5):
        return shared.wait_for_predicate(label, predicate, time.monotonic() + seconds, process=self.core)

    def state(self, id_, state, seconds=5):
        return self.wait('startup ' + id_ + ' ' + state, lambda: next((item
            for item in self.query()['startups'] if item['id'] == id_ and item['state'] == state), None), seconds)

    def new(self, id_, name=None, slot=0, client=None, **fields):
        send(client or self.sender, slot, message('new', ID=id_, NAME=name or id_,
            SCREEN=self.sender.screen, **fields))
        return self.state(id_, 'pending')

    def managed(self, window):
        queue = [(child, 1) for child in self.view.children(self.client.root)]
        visited = 0
        while queue:
            target, depth = queue.pop()
            visited += 1
            assert visited <= 4096
            if target == window:
                return depth > 1
            if depth < 4:
                queue.extend((child, depth + 1) for child in self.view.children(target))
        return False

    def capture(self, name):
        result = subprocess.run([sys.executable, str(self.fixture), '--capture',
            str(self.binaries['capture']), str(self.out)], env=self.env,
            capture_output=True, text=True, check=True, timeout=40)
        path = Path(json.loads(result.stdout)); target = self.out / name
        path.replace(target)
        self.evidence['screenshots'][name] = shared.digest(target)

    def close(self):
        def clean(name, operation):
            try:
                operation()
            except BaseException as failure:
                self.evidence['status'] = 'failed'
                self.evidence['cleanup_failures'].append(dict(step=name, error=repr(failure)))
        for client in reversed(self.clients):
            clean(client.name, client.close)
        records = getattr(self, 'records', self.runtime.root / 'application-records')
        native = getattr(self, 'native', self.runtime.root / 'native-records')
        for release in records.glob('application-*.json'):
            token = json.loads(release.read_text())['token']
            (records / ('release-' + token)).touch()
        if self.peer: clean('query observer', self.peer.close)
        if self.control: clean('control observer', self.control.close)
        if self.core: clean('core', lambda: self.core.close(time.monotonic() + 5))
        if self.errors: clean('display', self.errors.close_display)
        elif self.client: clean('display', self.client.close)
        for child in reversed(self.runtime.processes):
            clean('stop ' + child.name, lambda child=child: child.close(time.monotonic() + 4))
        for path in records.glob('application-*.json'):
            identity = json.loads(path.read_text())['identity']
            def retire(identity=identity):
                current = shared.process_identity(identity['child'])
                if current is None or current['starttime'] != identity['starttime']:
                    return
                assert current['parent'] == os.getpid(), current
                def exited():
                    try:
                        pid, status = os.waitpid(identity['child'], os.WNOHANG)
                    except ChildProcessError:
                        return shared.process_identity(identity['child']) is None
                    if pid:
                        s = os.waitstatus_to_exitcode(status)
                        self.evidence.setdefault('adopted_application_waits', []).append(dict(
                            identity=identity, exit=s))
                        return True
                    return False
                try:
                    shared.wait_for_predicate('released owned application exit', exited,
                        time.monotonic() + 2)
                except BaseException:
                    current = shared.process_identity(identity['child'])
                    assert current and current['starttime'] == identity['starttime'] and current['parent'] == os.getpid()
                    os.kill(identity['child'], signal.SIGKILL)
                    os.waitpid(identity['child'], 0)
                    raise AssertionError('owned eventual application required forced cleanup')
            clean('adopted owned application Wait', retire)
        self.evidence['owned_process_waits'] = {child.name: child.poll() for child in self.runtime.processes}
        if native.exists():
            clean('native child Wait receipts', lambda: self.evidence.update(
                native_cleanup=shared.cleanup_receipts(native)))
        if self.evidence.get('observer_unexpected_x_errors'):
            self.evidence['status'] = 'failed'

    def preserve_runtime(self):
        """Keep bounded owned evidence before HarnessRuntime deletes its root."""
        root = self.runtime.root
        manifest = dict(status='complete', total_bytes=0, reserved_bytes=0, files={}, issues=[],
            bounds=dict(per_file_bytes=EVIDENCE_FILE_MAX,
                total_bytes=EVIDENCE_TOTAL_MAX, scanned_entries=EVIDENCE_ENTRIES_MAX),
            excluded_scopes=['ELFs', 'launch-copy', 'modules', 'caches', 'SDKs',
                'Xauthority and other unselected runtime files'])
        self.evidence['runtime_evidence'] = manifest
        required = ('config.fvwm', 'user/local.config')
        candidates = {name: root / name for name in required}
        scanned = 0

        def issue(path, reason):
            manifest['status'] = 'incomplete'
            manifest['issues'].append(dict(path=path, reason=reason))
            self.evidence['status'] = 'failed'

        def collect(directory, depth, accepts):
            nonlocal scanned
            try:
                metadata = directory.lstat()
            except FileNotFoundError:
                return
            if not stat.S_ISDIR(metadata.st_mode):
                issue(str(directory.relative_to(root)), 'selected evidence directory is not a real directory')
                return
            with os.scandir(directory) as entries:
                for entry in entries:
                    scanned += 1
                    if scanned > EVIDENCE_ENTRIES_MAX:
                        issue(str(directory.relative_to(root)), 'evidence entry limit reached')
                        return
                    path = Path(entry.path)
                    relative = str(path.relative_to(root))
                    if entry.is_symlink():
                        issue(relative, 'symlink in selected evidence scope; not followed')
                    elif entry.is_dir(follow_symlinks=False):
                        if depth:
                            collect(path, depth - 1, accepts)
                            if scanned > EVIDENCE_ENTRIES_MAX:
                                return
                        else:
                            issue(relative, 'selected evidence depth limit reached')
                    elif entry.is_file(follow_symlinks=False) and accepts(path):
                        candidates[relative] = path

        # Root scans are non-recursive: the private Desktop hard link and any
        # unrelated binaries/cache trees never enter the candidate set.
        with os.scandir(root) as entries:
            for entry in entries:
                scanned += 1
                if scanned > EVIDENCE_ENTRIES_MAX:
                    issue('.', 'evidence entry limit reached')
                    break
                path = Path(entry.path)
                if entry.is_file(follow_symlinks=False) and path.suffix == '.log':
                    candidates[path.name] = path
        scopes = [('application-records', 0, lambda path: (path.name.startswith('application-')
                and path.suffix == '.json') or path.name.startswith('release-')),
            ('native-records', 0, lambda path: path.suffix in ('.json', '.jsonl')),
            ('user', 1, lambda path: path.suffix in ('.config', '.fvwm', '.json')),
            ('data/applications', 0, lambda path: path.suffix == '.desktop'),
            ('config', 1, lambda path: path.suffix in ('.json', '.conf', '.list'))]
        for directory, depth, accepts in scopes:
            if scanned > EVIDENCE_ENTRIES_MAX:
                break
            collect(root / directory, depth, accepts)

        # Required configuration is copied first and never silently omitted.
        ordered = list(required) + sorted(set(candidates) - set(required))
        for relative in ordered:
            path = candidates[relative]
            value = dict(required=relative in required)
            manifest['files'][relative] = value
            target = None
            try:
                metadata = path.lstat()
                value.update(size_bytes=metadata.st_size,
                    mode=format(stat.S_IMODE(metadata.st_mode), '04o'))
                if not stat.S_ISREG(metadata.st_mode):
                    raise RuntimeError('selected evidence is not a regular file')
                if metadata.st_size > EVIDENCE_FILE_MAX:
                    raise RuntimeError('file exceeds evidence per-file byte limit')
                if metadata.st_size > EVIDENCE_TOTAL_MAX - manifest['reserved_bytes']:
                    raise RuntimeError('file exceeds remaining evidence byte budget')
                # Reserve the whole admitted size even if a later write fails,
                # so a partially written file cannot escape the aggregate cap.
                manifest['reserved_bytes'] += metadata.st_size
                target = self.out / relative
                target.parent.mkdir(mode=0o700, parents=True, exist_ok=True)
                copied, digest = 0, hashlib.sha256()
                descriptor = os.open(path, os.O_RDONLY | os.O_NOFOLLOW)
                with os.fdopen(descriptor, 'rb') as source, target.open('wb') as output:
                    opened = os.fstat(source.fileno())
                    if not stat.S_ISREG(opened.st_mode) or (opened.st_dev, opened.st_ino) != \
                            (metadata.st_dev, metadata.st_ino):
                        raise RuntimeError('selected evidence identity changed before copy')
                    while True:
                        block = source.read(min(65536, metadata.st_size - copied + 1))
                        if not block:
                            break
                        if len(block) > metadata.st_size - copied:
                            raise RuntimeError('selected evidence grew during copy')
                        output.write(block)
                        digest.update(block)
                        copied += len(block)
                        manifest['total_bytes'] += len(block)
                    after = os.fstat(source.fileno())
                os.chmod(target, stat.S_IMODE(metadata.st_mode))
                value.update(copied_bytes=copied, sha256=digest.hexdigest(), status='copied')
                if copied != metadata.st_size or (after.st_size, after.st_mtime_ns) != \
                        (metadata.st_size, metadata.st_mtime_ns):
                    raise RuntimeError('selected evidence changed during copy')
            except BaseException as failure:
                value.update(status='failed', error=repr(failure))
                issue(relative, repr(failure))
                if target is not None:
                    try:
                        saved = target.lstat()
                        if not stat.S_ISREG(saved.st_mode) or saved.st_size > EVIDENCE_FILE_MAX:
                            raise RuntimeError('partial evidence output is not a bounded regular file')
                        digest = hashlib.sha256()
                        with target.open('rb') as copied_file:
                            while block := copied_file.read(65536):
                                digest.update(block)
                        value.update(saved_size_bytes=saved.st_size,
                            saved_mode=format(stat.S_IMODE(saved.st_mode), '04o'),
                            sha256=digest.hexdigest())
                    except BaseException as metadata_failure:
                        value['saved_metadata_error'] = repr(metadata_failure)
        manifest['scanned_entries'] = scanned
        manifest['total_bytes'] = sum(value.get('saved_size_bytes', value.get('copied_bytes', 0))
            for value in manifest['files'].values())
        shared.atomic_json(self.out / 'runtime-evidence-manifest.json', manifest)


def protocol_cases(s):
    cases, c = s.evidence['cases'], s.sender
    prefix = 'fixture-' + os.urandom(6).hex() + '-'
    a, b = XClient(s, 'producer-a'), XClient(s, 'producer-b')
    a.create(0); b.create(0)
    id_a, id_b = prefix + 'a', prefix + 'b'
    name_a, name_b = 'Éditeur "quoted" \\ literal', 'Interleaved second'
    parts_a = fragments(message('new', ID=id_a, NAME=name_a, SCREEN=c.screen))
    parts_b = fragments(message('new', ID=id_b, NAME=name_b, SCREEN=c.screen))
    for index in range(max(len(parts_a), len(parts_b))):
        for client, parts in ((a, parts_a), (b, parts_b)):
            if index < len(parts):
                client.fragment(0, parts[index], begin=index == 0)
    item_a, item_b = s.state(id_a, 'pending'), s.state(id_b, 'pending')
    assert item_a['name'] == name_a and item_b['name'] == name_b
    first = s.query(); pause_until(time.monotonic() + .025); later = s.query()
    assert first['generation'] == later['generation']
    cases['fragmented_interleaved_utf8'] = dict(first=item_a, second=item_b,
        stable_age_generation=[first['generation'], later['generation']])
    a.close()
    assert s.state(id_a, 'pending')['name'] == name_a
    send(b, 0, message('remove', ID=id_a))
    ended = s.state(id_a, 'protocol-ended')
    send(b, 0, message('change', ID=id_a, NAME='must not replace terminal'))
    send(b, 0, message('new', ID=id_a, NAME='must not reopen', SCREEN=c.screen))
    assert s.state(id_a, 'protocol-ended')['name'] == ended['name']
    cases['dead_producer_other_remove_terminal_retention'] = ended
    orphan = prefix + 'orphan'
    send(c, 0, message('change', ID=orphan, WMCLASS='OrphanClass'))
    send(c, 0, message('remove', ID=prefix + 'unknown'))
    assert not any(item['id'] in (orphan, prefix + 'unknown') for item in s.query()['startups'])
    merged = s.new(orphan, name='Later new')
    assert merged['wmclass'] == 'OrphanClass'
    cases['invisible_orphan_change_merged'] = merged
    padding_id, padding_name = prefix + 'padding', 'NUL padding'
    raw = message('new', ID=padding_id, NAME=padding_name, SCREEN=c.screen)
    if len(raw) % 20 == 0:
        padding_name += 'x'
        raw = message('new', ID=padding_id, NAME=padding_name, SCREEN=c.screen)
    raw += b'!' * (20 - len(raw) % 20)
    send(c, 0, raw)
    assert s.state(padding_id, 'pending')['name'] == padding_name
    cases['nonzero_padding_after_nul_ignored'] = padding_id

    before = len(s.query()['startups'])
    ordinary = c.create(1); c.command('map 1')
    s.wait('ordinary client managed', lambda: s.managed(ordinary))
    assert len(s.query()['startups']) == before
    cases['ordinary_client_untracked'] = ordinary

    def map_id(slot, id_, direct=True, group=None, class_=None):
        window = c.create(slot)
        if direct:
            c.command('id %d %s' % (slot, id_.encode().hex()))
        if group is not None:
            c.command('group %d %d' % (slot, group))
        if class_ is not None:
            c.command('class %d %s %s' % (slot, b'fixture'.hex(), class_.encode('latin-1').hex()))
        c.command('map %d' % slot)
        s.wait('client managed', lambda: s.managed(window))
        return window

    direct_id = prefix + 'direct'
    s.new(direct_id)
    direct_window = map_id(10, direct_id)
    assert s.state(direct_id, 'window-observed')['window'] == direct_window
    changed_id = prefix + 'property'
    property_window = map_id(11, '', direct=False)
    s.new(changed_id)
    c.command('id 11 ' + changed_id.encode().hex())
    assert s.state(changed_id, 'window-observed')['window'] == property_window
    cases['direct_map_and_property'] = dict(direct=direct_window, property=property_window)
    nul_id = prefix + 'single-final-nul'
    s.new(nul_id)
    nul_window = c.create(12)
    c.command('id 12 ' + (nul_id.encode() + b'\0').hex()); c.command('map 12')
    assert s.state(nul_id, 'window-observed')['window'] == nul_window
    cases['direct_optional_terminal_nul'] = nul_window

    group_id = prefix + 'group'
    s.new(group_id)
    c.create(20); c.command('id 20 ' + group_id.encode().hex())
    group_window = map_id(21, '', direct=False, group=20)
    assert s.state(group_id, 'window-observed')['window'] == group_window
    blocked_id = prefix + 'malformed-direct'
    s.new(blocked_id); c.command('id 20 ' + blocked_id.encode().hex())
    blocked_window = c.create(22)
    c.command('group 22 20'); c.command('bad-id 22'); c.command('map 22')
    s.wait('malformed direct client managed', lambda: s.managed(blocked_window))
    assert s.state(blocked_id, 'pending')['window'] == 0
    leader_window = c.create(23)
    c.command('leader 23 20'); c.command('map 23')
    s.wait('session leader client managed', lambda: s.managed(leader_window))
    assert s.state(blocked_id, 'pending')['window'] == 0
    hints_id = prefix + 'hints-change'
    hints_window = map_id(24, '', direct=False)
    s.new(hints_id); c.command('id 20 ' + hints_id.encode().hex())
    c.command('group 24 20')
    assert s.state(hints_id, 'window-observed')['window'] == hints_window
    cases['group_at_map_hints_change_no_malformed_inheritance'] = dict(
        group=group_window, malformed=blocked_window, session_leader=leader_window,
        hints_change=hints_window, still_pending=blocked_id)
    group_nul_id = prefix + 'group-final-nul'
    s.new(group_nul_id); c.command('id 20 ' + (group_nul_id.encode() + b'\0').hex())
    group_nul_window = map_id(25, '', direct=False, group=20)
    assert s.state(group_nul_id, 'window-observed')['window'] == group_nul_window
    for slot, suffix in [(26, b'\0\0'), (27, b'\0trailing')]:
        bad_id = prefix + 'bad-nul-%d' % slot
        s.new(bad_id); c.command('id 20 ' + bad_id.encode().hex())
        bad_window = c.create(slot)
        c.command('id %d %s' % (slot, (bad_id.encode() + suffix).hex()))
        c.command('group %d 20' % slot); c.command('map %d' % slot)
        s.wait('malformed NUL property client managed', lambda window=bad_window: s.managed(window))
        assert s.state(bad_id, 'pending')['window'] == 0
    cases['group_terminal_nul_and_bad_nul_no_inheritance'] = dict(group=group_nul_window,
        malformed_slots=[26, 27])

    c.command('watch 0')
    def wm_removes(id_):
        known = {window for client in s.clients for window in client.windows.values()}
        return [record for record in c.drain() if record['producer'] not in known
            and record['text'].startswith('remove:') and re.search(
                r'(?:^|\s)ID=(?:"' + re.escape(id_) + r'"|' + re.escape(id_) + r')(?:\s|$)',
                record['text'])]
    unique_id = prefix + 'latin1-class'
    s.new(unique_id, WMCLASS='CaféFixture')
    latin_window = map_id(30, '', direct=False, class_='CaféFixture')
    assert s.state(unique_id, 'window-observed')['window'] == latin_window
    unique_remove = s.wait('WM public legacy class remove', lambda: wm_removes(unique_id))
    competing = [prefix + 'class-a', prefix + 'class-b']
    for id_ in competing:
        s.new(id_, WMCLASS='StartupCompetition')
    competing_window = map_id(31, '', direct=False, class_='StartupCompetition')
    assert all(s.state(id_, 'pending')['window'] == 0 for id_ in competing)
    assert all(wm_removes(id_) == [] for id_ in competing)
    send(c, 0, message('remove', ID=competing[0])); s.state(competing[0], 'protocol-ended')
    resolved_window = map_id(32, '', direct=False, class_='StartupCompetition')
    assert s.state(competing[1], 'window-observed')['window'] == resolved_window
    resolved_remove = s.wait('WM public resolved class remove', lambda: wm_removes(competing[1]))
    cases['exact_latin1_and_competing_class'] = dict(latin1=latin_window,
        ambiguous=competing_window, unique_after_remove=resolved_window,
        unique_public_remove=unique_remove, resolved_public_remove=resolved_remove)

    if s.args.expiry:
        expiry_cases(s, prefix)
    else:
        cases['real_expiry'] = dict(status='skipped', reason='lead selects --expiry once; no theme repetition')

    counters = s.query()['counters']
    malformed = [b'new: ID="bad" SCREEN="0"\0',
        b'new: ID="bad" NAME="unterminated SCREEN=0\0',
        b'new: ID="bad" NAME="\xff" SCREEN=0\0']
    for raw in malformed:
        send(c, 0, raw)
    c.fragment(0, b'invalid format', begin=True, format_=32)
    send(c, 0, b'new: ID="oversize" NAME="' + b'x' * 5000 + b'" SCREEN=0\0')
    after = s.wait('malformed counter advances', lambda:
        (value if (value := s.query())['counters']['malformed'] > counters['malformed'] else None))
    assert not any(item['id'] in ('bad', 'oversize') for item in after['startups'])
    cases['malformed_and_oversize_refused'] = dict(before=counters, after=after['counters'])
    drops = after['counters']['dropped']
    for slot in range(50, 83):
        c.create(slot); c.fragment(slot, b'new: ID="partial-xxx', begin=True)
    saturated = s.wait('partial stream cap reports drop', lambda:
        (value if (value := s.query())['counters']['dropped'] > drops else None))
    expired = s.wait('partial streams expire on actual clock', lambda:
        (value if (value := s.query())['counters']['partial_expired'] >
            saturated['counters']['partial_expired'] else None), 6)
    cases['partial_capacity_and_expiry'] = dict(saturated=saturated['counters'], expired=expired['counters'])
    retained = {item['id'] for item in s.query()['startups']}
    drops = s.query()['counters']['dropped']
    for index in range(80):
        send(c, 0, message('new', ID=prefix + 'capacity-%d' % index,
            NAME='Capacity %d' % index, SCREEN=c.screen))
    full = s.wait('activity capacity reports drop', lambda:
        (value if (value := s.query())['total'] == 64 and
            value['counters']['dropped'] > drops else None))
    assert retained <= {item['id'] for item in full['startups']}
    cases['record_capacity_pagination_no_eviction'] = full


def expiry_cases(s, prefix):
    idle, busy = prefix + 'idle-expiry', prefix + 'busy-expiry'
    start = time.monotonic()
    s.new(idle)
    pause_until(start + 5)
    s.new(busy)
    pause_until(start + 45)
    send(s.sender, 0, message('change', ID=busy, NAME='Changed without extending deadline'))
    s.state(busy, 'pending')
    pause_until(start + 61)
    idle_expired = s.state(idle, 'expired')
    assert s.state(busy, 'pending')['window'] == 0
    s.sender.send('busy 1 6500')
    busy_expired = s.state(busy, 'expired', seconds=6)
    answer = s.sender.line(seconds=7)
    assert answer.startswith('OK busy 1 '), answer
    s.evidence['cases']['real_expiry'] = dict(status='passed', idle=idle_expired,
        busy=busy_expired, changed_did_not_extend=True,
        limit='one combined real monotonic interval; no native CPU/performance claim')


def app_record(s, token):
    paths = list(s.records.glob('application-' + token + '-*.json'))
    assert len(paths) <= 1, 'eventual application launched more than once: ' + token
    return json.loads(paths[0].read_text()) if paths else None


def owned_children(identity):
    current = shared.process_identity(identity['child'])
    assert current and current['starttime'] == identity['starttime']
    tasks = list((Path('/proc') / str(identity['child']) / 'task').iterdir())
    assert len(tasks) <= 128
    children = set()
    for task in tasks:
        try:
            children.update(int(value) for value in (task / 'children').read_text().split())
        except FileNotFoundError:
            pass
    return sorted(children)


def start_at_cwd(s, name, argv, env):
    log_path = s.runtime.root / (name + '.log')
    log = log_path.open('wb')
    child = subprocess.Popen(argv, cwd=s.cwd, env=env, stdin=subprocess.DEVNULL,
        stdout=log, stderr=subprocess.STDOUT, start_new_session=True)
    managed = ManagedProcess(name, list(argv), log_path, child, log)
    s.runtime.processes.append(managed)
    return managed


def helper_launch(s, token, display=None, blocked=None):
    raw = os.fsdecode(os.fsencode(s.runtime.root) + b'/raw space ; %-\xff')
    arguments = [token, 'space literal', raw, 'semi;literal', '$(touch marker)']
    env = dict(s.env)
    if display is not None:
        env['DISPLAY'] = display
    name = 'Helper ' + token
    argv = [str(s.binaries['helper']), '--name', name, '--icon', 'fixture-icon',
        '--application-id', token + '.desktop', '--wmclass', 'HelperFixture', '--',
        str(s.application), *arguments]
    started = time.monotonic()
    child = start_at_cwd(s, token, argv, env)
    accepted = None
    try:
        if blocked is not None:
            blocked.settimeout(3)
            accepted, _ = blocked.accept()
            accepted.settimeout(3)
            assert accepted.recv(4096), 'actual X handshake missing'
        record = s.wait('actual eventual helper application', lambda: app_record(s, token), 4)
        assert record['identity']['child'] == child.process.pid, record
        assert record['group'] == child.process.pid, record
        assert record['argv_bytes'] == shared.encoded(arguments), record
        assert record['cwd_bytes'] == shared.encoded([s.cwd])[0], record
        assert owned_children(record['identity']) == [], 'announcer survived application exec'
        if display is None:
            id_ = record['startup_id']
            assert id_ and id_ != 'must-clear-inherited-ID' and id_.endswith('_TIME0')
            observed = s.state(id_, 'pending')
            assert observed['name'] == name and observed['application_id'] == token + '.desktop'
        else:
            assert record['startup_id'] is None, record
            assert not any(item['name'] == name for item in s.query()['startups'])
            observed = None
        (s.records / ('release-' + token)).touch()
        code = child.wait_until_exit(time.monotonic() + 3)
        assert code == 0
        assert app_record(s, token)['identity'] == record['identity']
        assert not (s.cwd / 'marker').exists()
        return dict(record=record, observed=observed, application_wait=code,
            announcer_children_at_exec=[], actual_handshake=accepted is not None,
            elapsed_seconds=time.monotonic() - started,
            limit='elapsed is a bounded fallback observation, not a performance qualification')
    finally:
        if accepted is not None:
            accepted.close()


def helper_cases(s):
    cases = s.evidence['cases']
    cases['helper_true_literal_exec_and_survival'] = helper_launch(s, 'helper-positive')
    cases['helper_fresh_second_id'] = helper_launch(s, 'helper-second')
    assert cases['helper_true_literal_exec_and_survival']['record']['startup_id'] != \
        cases['helper_fresh_second_id']['record']['startup_id']
    cases['helper_missing_display_fallback'] = helper_launch(s, 'helper-no-display', display=':65534')
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as blocked:
        # A real owned X transport accepts the actual client handshake, then
        # supplies no reply. No provider result or announcement is fabricated.
        while True:
            blocked.bind(('127.0.0.1', 0))
            port = blocked.getsockname()[1]
            if port > 6000:
                break
            raise RuntimeError('ephemeral port cannot form an X display number')
        blocked.listen(1)
        cases['helper_blocked_display_fallback'] = helper_launch(s,
            'helper-blocked-display', display='127.0.0.1:%d' % (port - 6000), blocked=blocked)
    missing = s.runtime.root / 'no-such-application'
    failed = start_at_cwd(s, 'helper-exec-failure', [str(s.binaries['helper']),
        '--name', 'Known exec failure', '--', str(missing)], s.env)
    code = failed.wait_until_exit(time.monotonic() + 5)
    assert code != 0
    diagnostic = failed.log_path.read_text()
    assert diagnostic.strip()
    ended = s.wait('failed exec bounded remove', lambda: next((item for item in s.query()['startups']
        if item['name'] == 'Known exec failure' and item['state'] == 'protocol-ended'), None))
    cases['known_exec_failure'] = dict(exit=code, diagnostic=diagnostic, activity=ended)
    consumer_cases(s)


def consumer_cases(s):
    cases = s.evidence['cases']
    def callback(label):
        candidates = [row for row in menu_rows(s.native, 'SlateLauncherMenu')
            if '"' + label + '" SendToModule FvwmDesktop launch-bound ' in row]
        return candidates[-1].split(' SendToModule FvwmDesktop ', 1)[1] if candidates else None
    for token, enabled in [('true', True), ('false', False), ('legacy', True), ('ordinary', False)]:
        bound = s.wait('committed real application callback', lambda token=token: callback('Startup ' + token))
        s.control.send(('SendToModule FvwmDesktop ' + bound + '\n').encode())
        record = s.wait('actual Desktop eventual application', lambda token=token: app_record(s, token))
        assert record['argv_bytes'] == shared.encoded([token, 'space literal', 'semi;literal'])
        assert record['cwd_bytes'] == shared.encoded([s.cwd])[0]
        assert record['group'] == record['identity']['child']
        assert owned_children(record['identity']) == []
        if enabled:
            assert record['startup_id'] and record['startup_id'].endswith('_TIME0')
            item = s.state(record['startup_id'], 'pending')
            assert item['name'] == 'Startup ' + token
        else:
            assert record['startup_id'] is None
            assert not any(item['name'] == 'Startup ' + token for item in s.query()['startups'])
            item = None
        (s.records / ('release-' + token)).touch()
        s.wait('Desktop waiter reaped its actual application', lambda record=record:
            shared.process_identity(record['identity']['child']) is None)
        cases['desktop_' + token] = dict(record=record, activity=item,
            child_retired_by_actual_parent=True, exit_status='not observed by this non-parent fixture')
    # Discovery uses only the owned image's directory, its parent and the
    # linked module directory. Never rename a shared staged/built helper.
    if s.args.module_directory is None:
        cases['missing_helper'] = dict(status='unavailable',
            reason='lead must pass actual linked --module-directory to exclude fallback installation')
        raise RuntimeError('missing-helper acceptance requires actual linked --module-directory')
    assert not (s.args.module_directory / 'fvwm-startup-launch').exists()
    assert not (s.private_helper.parent.parent / 'fvwm-startup-launch').exists()
    s.private_helper.unlink()
    # Reuse the true entry with a fresh record token via a new owned entry and
    # real rediscovery; the captured old callback is never rewritten.
    entry = s.apps / 'fixture-missing-helper.desktop'
    original = (s.apps / 'fixture-true.desktop').read_text()
    entry.write_text(original.replace('Name=Startup true', 'Name=Startup missing-helper')
        .replace('" true ', '" missing-helper '))
    s.control.send(b'SendToModule FvwmDesktop discover\n')
    bound = s.wait('real rediscovered missing-helper callback', lambda: callback('Startup missing-helper'))
    s.control.send(('SendToModule FvwmDesktop ' + bound + '\n').encode())
    record = s.wait('missing helper ordinary launch', lambda: app_record(s, 'missing-helper'))
    assert record['startup_id'] is None
    assert not any(item['name'] == 'Startup missing-helper' for item in s.query()['startups'])
    assert record['argv_bytes'] == shared.encoded(['missing-helper', 'space literal', 'semi;literal'])
    assert record['cwd_bytes'] == shared.encoded([s.cwd])[0]
    (s.records / 'release-missing-helper').touch()
    s.wait('missing-helper child reaped', lambda: shared.process_identity(record['identity']['child']) is None)
    cases['missing_helper'] = dict(record=record, linked_directory=str(s.args.module_directory),
        helper_removed_only_from_owned_layout=True)


def activity_text(configuration):
    """Decode only bounded literal TextHex rows, retaining raw rows separately."""
    assert len(configuration) <= 256
    texts, total = [], 0
    prefix = '*SlateLaunchActivityForm: TextHex'
    for line in configuration:
        if not line.startswith(prefix):
            continue
        match = re.fullmatch(re.escape(prefix) + r' x([0-9a-fA-F]*)', line)
        assert match, 'malformed native TextHex directive'
        encoded = match.group(1)
        assert len(encoded) <= 1024 and len(encoded) % 2 == 0
        raw = bytes.fromhex(encoded)
        total += len(raw)
        assert total <= 16384
        texts.append(raw.decode('utf-8'))
    return texts


def ui_cases(s):
    prefix = 'ui-' + os.urandom(8).hex()
    s.new(prefix + '-pending', name='Waiting for a window')
    s.new(prefix + '-ended', name='Protocol ended is not success')
    send(s.sender, 0, message('remove', ID=prefix + '-ended'))
    s.state(prefix + '-ended', 'protocol-ended')
    snapshot = s.query()['startups']
    assert len(snapshot) == 2
    pending_index = next(index for index, item in enumerate(snapshot) if item['id'] == prefix + '-pending')
    ended_index = next(index for index, item in enumerate(snapshot) if item['id'] == prefix + '-ended')
    s.control.send(b'SendToModule FvwmDesktop launch-activity\n')
    def configuration():
        commands = [row['text'] for row in shared.rows(s.native) if row['direction'] == 'out']
        starts = [index for index, line in enumerate(commands)
            if line == 'Module FvwmForm SlateLaunchActivityForm']
        if not starts:
            return []
        end = starts[-1]
        titles = [index for index, line in enumerate(commands[:end])
            if line == '*SlateLaunchActivityForm: Title "Launch activity"']
        return [line for line in commands[titles[-1]:end]
            if line.startswith('*SlateLaunchActivityForm: ')] if titles else []
    def page(state, name):
        def current():
            values = configuration()
            texts = activity_text(values)
            if any(' - ' + state in text for text in texts) and \
                    any('Application: ' + name in text for text in texts):
                return dict(configuration=values, decoded_text=texts)
            return None
        return s.wait('actual activity page ' + state + ' ' + name, current)
    def focus():
        window = s.view.wait('Launch activity')
        x, d = s.client.x, s.client.display
        x.XSetInputFocus.argtypes = [C.c_void_p, C.c_ulong, C.c_int, C.c_ulong]
        x.XSetInputFocus.restype = C.c_int
        x.XSetInputFocus(d, window, 1, 0); x.XSync(d, 0)
        return window
    page(snapshot[0]['state'], snapshot[0]['name'])
    focus()
    if pending_index:
        s.view.key('F2')
    published = page('pending', 'Waiting for a window')
    window = focus()
    assert any('Protocol ended is not application success.' in text for text in published['decoded_text'])
    s.evidence['cases']['native_activity_initial'] = dict(window=window,
        configuration=published['configuration'], decoded_text=published['decoded_text'], query=s.query())
    s.capture('startup-activity-initial.png')
    s.view.key('F2' if ended_index > pending_index else 'F1')
    ended_page = page('protocol-ended', 'Protocol ended is not success')
    focus()
    s.evidence['cases']['native_activity_paged_ended'] = ended_page
    s.view.key('F1' if pending_index < ended_index else 'F2')
    page('pending', 'Waiting for a window'); focus()
    send(s.sender, 0, message('remove', ID=prefix + '-pending'))
    s.state(prefix + '-pending', 'protocol-ended')
    s.view.key('F5')
    # The one-record native page must update the same target after Refresh;
    # a query reply or a stale second page alone cannot satisfy this oracle.
    refreshed = page('protocol-ended', 'Waiting for a window')
    focus()
    s.evidence['cases']['native_activity_refresh'] = dict(configuration=refreshed['configuration'],
        decoded_text=refreshed['decoded_text'], query=s.query())
    s.capture('startup-activity-refreshed.png')
    s.view.key('Escape')
    s.wait('native activity Close', lambda: not s.view.named('Launch activity'))
    s.evidence['cases']['native_activity_close'] = True


def run(args, phase, profile, root):
    out = root / (phase + '-' + profile)
    out.mkdir()
    with shared.HarnessRuntime(prefix='fvwm-startup-') as runtime:
        session = Session(args, phase, profile, out, runtime)
        try:
            session.start()
            {'protocol': protocol_cases, 'helper': helper_cases, 'ui': ui_cases}[phase](session)
            session.evidence['status'] = 'passed'
        except BaseException as failure:
            session.evidence.update(status='failed', failure=repr(failure))
            raise
        finally:
            try:
                session.close()
            except BaseException as failure:
                session.evidence['status'] = 'failed'
                session.evidence['cleanup_failures'].append(dict(
                    step='session.close', error=repr(failure)))
            finally:
                # Even a cleanup exception must leave its configuration and
                # original logs available before the context removes runtime.
                try:
                    session.preserve_runtime()
                except BaseException as failure:
                    session.evidence['status'] = 'failed'
                    session.evidence['cleanup_failures'].append(dict(
                        step='preserve runtime evidence', error=repr(failure)))
                shared.atomic_json(out / 'result.json', session.evidence)
    assert session.evidence['status'] == 'passed', session.evidence
    print(json.dumps(dict(phase=phase, profile=profile, status='passed', artifacts=str(out))), flush=True)


def main():
    if len(sys.argv) > 1 and sys.argv[1] == '--module-proxy':
        return shared.module_proxy(sys.argv[2], sys.argv[3], sys.argv[4:])
    if len(sys.argv) > 1 and sys.argv[1] == '--application':
        return application(sys.argv[2], sys.argv[3:])
    if len(sys.argv) == 4 and sys.argv[1] == '--capture':
        print(json.dumps(shared.NativeCapture(sys.argv[2]).screenshot(
            sys.argv[3]).removeprefix('Screenshot saved: ')))
        return 0
    parser = argparse.ArgumentParser(description=__doc__, allow_abbrev=False)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--client', type=Path, required=True)
    parser.add_argument('--startup-helper', type=Path, required=True)
    parser.add_argument('--module-directory', type=Path,
        help='actual linked installation module directory; required for helper-absence case')
    parser.add_argument('--mode', choices=('protocol', 'helper', 'ui', 'all'), default='all')
    parser.add_argument('--expiry', action='store_true', help='one real idle/busy pending-expiry interval')
    parser.add_argument('--profiles', nargs='+', choices=PROFILES, default=list(PROFILES))
    parser.add_argument('--artifacts-dir', type=Path, required=True)
    args = parser.parse_args()
    if sys.platform != 'linux':
        parser.error('run in the owned Linux native-X environment')
    libc = C.CDLL(None, use_errno=True)
    libc.prctl.argtypes = [C.c_int, C.c_ulong, C.c_ulong, C.c_ulong, C.c_ulong]
    if libc.prctl(36, 1, 0, 0, 0) != 0:
        raise RuntimeError('cannot adopt fixture-owned detached application children')
    args.artifacts_dir.mkdir(parents=True, exist_ok=True)
    root = Path(tempfile.mkdtemp(prefix='startup-', dir=args.artifacts_dir.resolve()))
    print(json.dumps(dict(artifacts_dir=str(root))), flush=True)
    phases = ('protocol', 'helper', 'ui') if args.mode == 'all' else (args.mode,)
    for phase in phases:
        for profile in args.profiles if phase == 'ui' else ('slate-dark',):
            run(args, phase, profile, root)
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
