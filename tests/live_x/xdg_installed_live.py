#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
"""Bounded installed XDG acceptance on Linux, FreeBSD and OpenBSD.

The runner supplies an actual Meson --prefix install, current built core/MFL,
xdg_installed_client.c and the unchanged startup_notification_client.c producer.
No DESTDIR relocation, private-helper override, detached application, /proc or
subreaper is used. --execution-profile records the runner's native/emulated
description; it is an assertion by the runner, not detected CPU qualification.
"""
import argparse
import base64
import hashlib
import json
import os
from pathlib import Path
import re
import select
import shlex
import shutil
import stat
import subprocess
import sys
import time
import traceback
from urllib.parse import quote_from_bytes

from live_x_harness import HarnessRuntime, ManagedProcess, start_display_server
from live_x_harness import wait_for_predicate
from mfl_request_live import frame
from mfl_stream_live import Connection


WIRE_MAX = 4 * 1024 * 1024
ERROR_MAX = 64 * 1024
RECORD_MAX = 2048
TEXT = b'An ordinary plain text document for actual MIME observation.\n'


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def identity(path):
    path = path.resolve(strict=True)
    info = path.stat()
    assert stat.S_ISREG(info.st_mode), path
    return dict(path=str(path), sha256=digest(path),
        mode=stat.S_IMODE(info.st_mode), bytes=info.st_size)


def encoded(value):
    return base64.b64encode(os.fsencode(value)).decode('ascii')


def hexed(values):
    return [os.fsencode(value).hex() for value in values]


def write(path, value):
    path.parent.mkdir(mode=0o700, parents=True, exist_ok=True)
    path.write_text(value, encoding='utf-8')
    path.chmod(0o600)


def desktop_string(value):
    return value.replace('\\', '\\\\').replace('\n', '\\n').replace('\t', '\\t')


def exec_word(value):
    value = str(value).replace('%', '%%')
    for char in ('\\', '"', '$', '`'):
        value = value.replace(char, '\\' + char)
    return '"' + value + '"'


def preferences(roots):
    return {str(path): dict(sha256=digest(path),
        mode=stat.S_IMODE(path.stat().st_mode), mtime_ns=path.stat().st_mtime_ns)
        for root in roots for path in sorted(root.rglob('*mimeapps.list'))}


def module_proxy(real, directory, arguments):
    """Keep the shared relay verbatim; distinguish EOF Wait from fallback."""
    import native_module_proxy as relay

    terminate = relay.terminate

    def wait_before_fallback(child):
        forced = False
        try:
            child.wait(timeout=2)
        except subprocess.TimeoutExpired:
            forced = True
            terminate(child)
        receipt = Path(directory) / ('cleanup-%d.json' % os.getpid())
        write(receipt, json.dumps(dict(child=child.pid, returncode=child.returncode,
            forced_cleanup=forced)))

    relay.terminate = wait_before_fallback
    return relay.module_proxy(real, directory, arguments)


class BoundedConnection(Connection):
    """Reuse the real MFL transport with explicit fixture record/byte limits."""
    def record(self):
        deadline = time.monotonic() + 5
        while b'\n' not in self.pending:
            assert len(self.pending) < WIRE_MAX, 'MFL record limit'
            assert time.monotonic() < deadline, 'MFL record deadline'
            ready, _, _ = select.select([self.socket], [], [], .05)
            if not ready:
                continue
            data = self.socket.recv(min(65536, WIRE_MAX - len(self.pending)))
            assert data, 'unexpected MFL EOF'
            self.pending += data
        line, self.pending = self.pending.split(b'\n', 1)
        self.records = getattr(self, 'records', 0) + 1
        assert self.records <= RECORD_MAX, 'MFL record count'
        return json.loads(line)


class Child:
    """Direct child with bounded streamed output, ordinary stdin EOF and Wait."""
    def __init__(self, fixture, label, argv, env=None, cwd=None):
        self.fixture, self.label = fixture, label
        assert len(fixture.children) < 96
        index = len(fixture.children)
        self.stdout_path = fixture.out / ('%02d-%s.stdout' % (index, label))
        stderr_path = fixture.out / ('%02d-%s.stderr' % (index, label))
        self.log = stderr_path.open('wb')
        self.output = self.stdout_path.open('wb')
        try:
            self.process = subprocess.Popen(list(map(str, argv)),
                env=env or fixture.env, cwd=cwd, stdin=subprocess.PIPE,
                stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                start_new_session=True)
        except BaseException:
            self.log.close()
            self.output.close()
            raise
        self.managed = ManagedProcess(label, list(map(str, argv)), stderr_path,
            self.process, self.log)
        fixture.runtime.processes.append(self.managed)
        self.pending = bytearray()
        self.total, self.errors = 0, bytearray()
        self.readers = {self.process.stdout: 'out', self.process.stderr: 'err'}
        for stream in self.readers:
            os.set_blocking(stream.fileno(), False)
        os.set_blocking(self.process.stdin.fileno(), False)
        self.receipt = dict(label=label, command=list(map(str, argv)),
            cwd=str(cwd) if cwd else None, pid=self.process.pid,
            stdout=self.stdout_path.name, stderr=stderr_path.name,
            forced_cleanup=False, exit=None)
        fixture.evidence['children'].append(self.receipt)
        fixture.children.append(self)

    def pump(self, seconds=.05):
        ready, _, _ = select.select(list(self.readers), [], [], seconds)
        for stream in ready:
            data = os.read(stream.fileno(), 65536)
            if not data:
                del self.readers[stream]
                continue
            if self.readers[stream] == 'out':
                self.total += len(data)
                assert self.total <= WIRE_MAX, (self.label, 'stdout limit')
                self.pending.extend(data)
                self.output.write(data)
            else:
                self.errors.extend(data)
                assert len(self.errors) <= ERROR_MAX, (self.label, 'stderr limit')
                self.log.write(data)

    def send(self, data):
        assert len(data) <= 65536, 'fixture input limit'
        deadline = time.monotonic() + 5
        while data:
            assert time.monotonic() < deadline, 'child stdin deadline'
            _, ready, _ = select.select([], [self.process.stdin], [], .05)
            self.pump(0)
            if ready:
                try:
                    written = os.write(self.process.stdin.fileno(), data)
                except BlockingIOError:
                    continue
                assert written > 0, 'child stdin made no progress'
                data = data[written:]

    def line(self):
        deadline = time.monotonic() + 5
        while b'\n' not in self.pending:
            assert time.monotonic() < deadline, (self.label, 'line deadline')
            self.pump()
            assert self.process.poll() is None or b'\n' in self.pending, (
                self.label, bytes(self.errors), self.process.returncode)
        line, self.pending = self.pending.split(b'\n', 1)
        assert len(line) <= 128 * 1024, 'child line limit'
        return bytes(line)

    def eof(self):
        if not self.process.stdin.closed:
            self.process.stdin.close()

    def finish(self, expected=0):
        deadline = time.monotonic() + 8
        while self.readers or self.process.poll() is None:
            assert time.monotonic() < deadline, (self.label, 'Wait deadline')
            self.pump()
        code = self.process.wait(timeout=1)
        self.receipt.update(exit=code, stdout_bytes=self.total,
            stderr_bytes=len(self.errors))
        self.output.flush()
        self.log.flush()
        if expected is not None:
            assert code == expected, (self.receipt, bytes(self.errors))
        return code

    def cleanup(self):
        self.eof()
        if self.process.poll() is None:
            try:
                self.finish(expected=None)
            except BaseException:
                self.receipt['forced_cleanup'] = True
                self.managed.close(time.monotonic() + 2)
        self.receipt.update(exit=self.process.wait(timeout=1),
            stdout_bytes=self.total, stderr_bytes=len(self.errors))
        for stream in (self.process.stdout, self.process.stderr):
            stream.close()
        self.output.close()
        self.managed.close(time.monotonic() + 1)


class Fixture:
    def __init__(self, args, runtime, out, evidence):
        self.args, self.runtime, self.out, self.evidence = args, runtime, out, evidence
        self.source = Path(__file__).resolve().parents[2]
        self.children, self.connections, self.serial = [], [], 0
        self.env = runtime.environment()
        self.core = self.peer = None
        self.roots = []

    def case(self, name, expected, observed):
        self.evidence['cases'][name] = dict(expected=expected, observed=observed,
            status='passed')

    def rpc(self, label, executable, arguments, request=None, env=None):
        before = preferences(self.roots)
        child = Child(self, label, [executable, *arguments], env)
        if request is not None:
            child.send(json.dumps(request).encode())
        child.eof()
        child.finish()
        after = preferences(self.roots)
        child.receipt.update(preferences_before=before, preferences_after=after)
        assert before == after, 'worker modified personal defaults'
        return child, bytes(child.pending)

    def browser_rpc(self, label, request):
        child, output = self.rpc(label, self.browser, ['--worker'], request)
        result = json.loads(output)
        assert result['schema_version'] == 1, result
        self.evidence['worker_results'].append(dict(label=label, result=result))
        assert len(self.evidence['worker_results']) <= 48
        return result, bytes(child.errors).decode('utf-8', errors='replace')

    def apps(self, label='desktop-apps', env=None):
        _, output = self.rpc(label, self.worker, [],
            dict(version=1, operation='apps'), env)
        result = json.loads(output)
        assert result['version'] == 1 and not result.get('error'), result
        return result['result']

    def installed(self):
        prefix = self.args.prefix.resolve(strict=True)
        self.menu, self.browser = [prefix / 'bin' / name for name in
            ('fvwm-menu-desktop', 'fvwm-file-browser')]
        found = {}
        for name in ('fvwm-startup-launch', 'FvwmDesktop', 'fvwm-desktop-worker'):
            matches = list(prefix.rglob(name))
            assert len(matches) == 1, (name, matches)
            found[name] = matches[0]
        self.helper, self.worker = found['fvwm-startup-launch'], found['fvwm-desktop-worker']
        assert len({path.parent for path in found.values()}) == 1, found
        assert self.helper.parent != self.menu.parent
        # Prevent an adjacent test helper from masking compiled ModuleDirectory.
        assert not (self.menu.parent / self.helper.name).exists()
        assert not (self.menu.parent.parent / self.helper.name).exists()
        desktop = prefix / 'share/applications/fvwm-file-browser.desktop'
        original = self.source / 'bin/FvwmPrompt/fvwm-file-browser.desktop'
        assert desktop.read_bytes() == original.read_bytes()
        binaries = dict(menu=self.menu, browser=self.browser, helper=self.helper,
            desktop_controller=found['FvwmDesktop'], worker=self.worker,
            core=self.args.fvwm, mfl=self.args.mfl, client=self.args.client,
            producer=self.args.producer, desktop_metadata=desktop)
        self.evidence['artifacts'] = {name: identity(path) for name, path in binaries.items()}
        for name, path in binaries.items():
            if name != 'desktop_metadata':
                assert os.access(path, os.X_OK), path
        for name in ('menu', 'browser', 'helper', 'desktop_controller', 'worker', 'desktop_metadata'):
            assert Path(self.evidence['artifacts'][name]['path']).is_relative_to(prefix)
        self.env.update(HOME=str(self.runtime.root), LC_ALL='C', LANGUAGE='C',
            XDG_CURRENT_DESKTOP='FVWM', PATH=str(prefix / 'bin') + os.pathsep + os.environ['PATH'],
            XDG_DATA_HOME=str(self.runtime.root / 'initial-empty'),
            XDG_DATA_DIRS=str(prefix / 'share'),
            XDG_CONFIG_HOME=str(self.runtime.root / 'initial-config'),
            XDG_CONFIG_DIRS=str(self.runtime.root / 'initial-lower-config'),
            XDG_CACHE_HOME=str(self.runtime.root / 'cache'))
        self.env.pop('FVWM_DESKTOP_WORKER', None)
        discovered = self.apps('installed-discovery')
        installed = [row for row in discovered if row['name'] ==
            'FVWM-X File Browser (development)']
        assert len(installed) == 1 and installed[0]['argv'] == [
            'fvwm-file-browser', '--reuse', '--'], installed
        assert installed[0]['startup']['notify'] is False, installed
        self.evidence['environment'] = {key: self.env[key] for key in
            ('HOME', 'LC_ALL', 'LANGUAGE', 'XDG_CURRENT_DESKTOP', 'PATH')}
        self.case('installed-identities', 'real prefix/private peers and unchanged Desktop metadata',
            dict(private_directory=str(self.helper.parent), discovered=installed,
                desktop_source_sha256=digest(original)))

    def setup(self):
        root = self.runtime.root
        self.user, self.system = root / 'user-data', root / 'system-data'
        self.config, self.lower_config = root / 'config', root / 'lower-config'
        self.cwd = root / 'working space ; $(literal)'
        for path in (self.user / 'applications', self.system / 'applications',
                     self.config, self.lower_config, self.cwd):
            path.mkdir(mode=0o700, parents=True)
        self.target = os.fsencode(self.cwd) + b'/raw-\xff ; $(literal).txt'
        with open(self.target, 'wb') as stream:
            stream.write(TEXT)
        self.uri = 'file://' + quote_from_bytes(self.target, safe='/')
        self.roots = [self.config, self.lower_config, self.user, self.system]
        self.env.update(XDG_DATA_HOME=str(self.user), XDG_DATA_DIRS=str(self.system),
            XDG_CONFIG_HOME=str(self.config), XDG_CONFIG_DIRS=str(self.lower_config),
            FVWM_USERDIR=str(root), FVWMMFL_SOCKET_PATH=str(root),
            DESKTOP_STARTUP_ID='must-clear-inherited-ID')
        self.evidence['environment'].update({key: self.env[key] for key in
            ('XDG_DATA_HOME', 'XDG_DATA_DIRS', 'XDG_CONFIG_HOME',
             'XDG_CONFIG_DIRS', 'XDG_CACHE_HOME', 'FVWM_USERDIR',
             'DESKTOP_STARTUP_ID')})
        self.defaults(self.config, 'missing.desktop;')
        self.defaults(self.lower_config, 'missing-lower.desktop;')

    def defaults(self, root, value):
        write(root / 'mimeapps.list', '[Default Applications]\n' + ''.join(
            mime + '=' + value + '\n' for mime in ('inode/directory', 'text/plain')))

    def entry(self, root, name, body):
        path = root / 'applications' / name
        write(path, '[Desktop Entry]\nType=Application\n' + body)
        return path

    def start_x(self):
        _, display = start_display_server(self.runtime, 'Xvfb',
            ['Xvfb', '-screen', '0', '1024x768x24', '-noreset'],
            'xserver.log', time.monotonic() + 10)
        self.env['DISPLAY'] = display
        self.evidence['environment']['DISPLAY'] = display
        modules, records = self.runtime.root / 'modules', self.runtime.root / 'module-records'
        modules.mkdir(mode=0o700)
        records.mkdir(mode=0o700)
        wrapper = modules / 'FvwmMFL'
        write(wrapper, '#!/bin/sh\nexec ' + shlex.join([sys.executable, str(Path(__file__).resolve()),
            '--module-proxy',
            str(self.args.mfl.resolve()), str(records)]) + ' "$@"\n')
        wrapper.chmod(0o700)
        config = self.runtime.root / 'fixture.config'
        # The core and MFL are current build artifacts. Only the launch/runtime
        # utilities above are installed identities; do not imply a full install.
        write(config, 'Read ' + str(self.source / 'fvwm/ConfigFvwmDefaults') +
            '\nModulePath ' + str(modules) +
            '\nStyle * TileCascadePlacement\nAddToFunc StartFunction I Module FvwmMFL\n')
        self.evidence['configuration'] = dict(text=config.read_text(), sha256=digest(config),
            defaults=identity(self.source / 'fvwm/ConfigFvwmDefaults'),
            module_wrapper=identity(wrapper))
        self.core = self.runtime.start('core', [str(self.args.fvwm.resolve()),
            '-d', display, '-f', str(config)], 'core.log', self.env)
        self.evidence['core_command'] = self.core.argv
        endpoint = self.runtime.root / ('fvwm_mfl_' + display.replace(':', '_') + '.sock')
        wait_for_predicate('installed acceptance MFL endpoint', endpoint.exists,
            time.monotonic() + 12, process=self.core)
        self.peer, self.events = BoundedConnection(endpoint), BoundedConnection(endpoint)
        self.connections += [self.peer, self.events]
        assert 'connection_profile' in self.peer.record()
        assert 'connection_profile' in self.events.record()
        self.events.send(b'set echo\nset map\n')
        self.events.echo('installed-ready')

    def query(self):
        for _ in range(3):
            items, generation, offset = [], None, 0
            for _ in range(4):
                self.serial += 1
                token = 'installed-%d' % self.serial
                wire = frame(token, resource='startups', offset=offset, limit=16,
                    **({'generation': generation} if generation else {}))
                self.peer.send(wire)
                reply = self.peer.record()['reply']
                assert reply['id'] == token and reply['version'] == 1, reply
                if reply.get('status') == 'error' and reply.get('error', {}).get('code') == 'stale_generation':
                    break
                assert reply['status'] == 'ok', reply
                page = reply['result']
                assert page['scope'] == 'core-startup-activity', page
                assert re.fullmatch('[0-9a-f]{48}', page['generation']), page
                generation = generation or page['generation']
                assert page['generation'] == generation and page['offset'] == offset
                assert len(page['startups']) <= 16 and page['total'] <= 64
                items += page['startups']
                self.evidence['queries'].append(dict(request=json.loads(wire[8:]), reply=reply))
                assert len(self.evidence['queries']) <= 512
                if page['next_offset'] is None:
                    assert len(items) == page['total']
                    return items
                assert page['next_offset'] > offset
                offset = page['next_offset']
        raise AssertionError('startup generation failed to settle')

    def state(self, token, state):
        return wait_for_predicate(token + ' ' + state, lambda: next((row for row
            in self.query() if row['id'] == token and row['state'] == state), None),
            time.monotonic() + 5, process=self.core)

    def mapped(self, window):
        deadline = time.monotonic() + 5
        for _ in range(256):
            assert time.monotonic() < deadline, 'actual M_MAP deadline'
            record = self.events.record()
            if int(record.get('map', {}).get('window', '0'), 0) == window:
                return record
        raise AssertionError('actual M_MAP record limit')

    def launch(self, name, arguments, expected):
        child = Child(self, name, arguments)
        observed = json.loads(child.line())
        assert observed['pid'] == child.process.pid, 'Exec chain changed owned PID'
        assert observed['argv_hex'] == hexed(expected), observed
        assert observed['cwd_hex'] == os.fsencode(self.cwd).hex(), observed
        token_hex = observed['startup_id_hex']
        assert token_hex and token_hex != os.fsencode('must-clear-inherited-ID').hex(), observed
        token = bytes.fromhex(token_hex).decode('utf-8')
        assert token.endswith('_TIME0'), token
        mapped = self.mapped(observed['window'])
        startup = self.state(token, 'window-observed')
        assert startup['window'] == observed['window'] and startup['name'] == name, startup
        assert startup['application_id'] == 'literal.desktop', startup
        child.send(b'quit\n')
        child.eof()
        child.finish()
        return dict(application=observed, mapped=mapped, startup=startup,
            direct_child_wait=child.receipt['exit'])

    def desktop_cases(self):
        before = preferences(self.roots)
        client = self.args.client.resolve()
        main_args = ['main', '', 'two words', 'semi; & literal',
            '$(touch SHELL-WAS-NOT-RUN)', self.uri, '%']
        main_exec = ' '.join(exec_word(value) for value in [client, *main_args]) + ' %c %k'
        action_args = ['action', 'literal; $(not-a-command)']
        action_exec = ' '.join(exec_word(value) for value in [client, *action_args]) + ' %c %k'
        path = self.entry(self.user, 'literal.desktop', 'Name=Literal main\nExec=' +
            desktop_string(main_exec) + '\nPath=' + desktop_string(str(self.cwd)) +
            '\nTerminal=false\nStartupNotify=true\nActions=Inspect;\n'
            '[Desktop Action Inspect]\nName=Inspect\nExec=' + desktop_string(action_exec) + '\n')
        self.entry(self.system, 'literal.desktop', 'Name=Forbidden lower\nExec=/bin/true\n')
        self.entry(self.system, 'hidden.desktop', 'Name=Forbidden hidden lower\nExec=/bin/true\n')
        self.entry(self.user, 'hidden.desktop', 'Hidden=true\n')
        self.entry(self.user, 'missing.desktop', 'Name=Forbidden absent\n'
            'TryExec=fvwm-no-such-fixture-program\nExec=fvwm-no-such-fixture-program\n')
        expected = [('Literal main', main_args), ('Literal main — Inspect', action_args)]
        discovered = self.apps()
        assert {row['name'] for row in discovered} == {name for name, _ in expected}, discovered
        for name, arguments in expected:
            row = next(item for item in discovered if item['name'] == name)
            assert row['argv'] == [str(client), *arguments, 'Literal main', str(path)], row
            assert row['cwd'] == str(self.cwd) and row['terminal'] is False, row
            assert row['startup'] == dict(name=name, application_id='literal.desktop', notify=True), row
        menu = self.runtime.root / 'fixture.menu'
        write(menu, '<Menu><Name>Applications</Name><DefaultAppDirs/>'
            '<Include><All/></Include></Menu>')
        _, output = self.rpc('installed-menu', self.menu,
            ['--set-menus', str(menu), '--include-items', 'none'])
        references = {}
        # Only these two opaque arguments cross into a real argv vector. The
        # FVWM output is never executed, evaluated or parsed as shell syntax.
        pattern = re.compile(r'^\+ "([^"]+)" Exec exec fvwm-menu-desktop --launch '
            r'([A-Za-z0-9_-]{1,8192}) ([0-9a-f]{64})$')
        for line in output.decode('utf-8').splitlines():
            match = pattern.fullmatch(line)
            if match and match[1] in {name for name, _ in expected}:
                assert match[1] not in references, line
                references[match[1]] = [match[2], match[3]]
        assert set(references) == {name for name, _ in expected}, output
        observations = {}
        assert not any(b'Forbidden' in line for line in output.splitlines()), output
        for name, arguments in expected:
            observations[name] = self.launch(name,
                [self.menu, '--launch', *references[name]],
                [*arguments, 'Literal main', str(path)])
        ids = [row['startup']['id'] for row in observations.values()]
        assert len(set(ids)) == 2, ids
        assert not list(self.runtime.root.rglob('SHELL-WAS-NOT-RUN'))
        assert before == preferences(self.roots)
        self.case('desktop-discovery-and-literal-launch',
            'high-root masking; main/action exact bytes/cwd; fresh ID and exact mapped XID; Wait 0',
            dict(discovered=discovered, references=references, launches=observations,
                menu=identity(menu), desktop=identity(path),
                preferences_before=before, preferences_after=preferences(self.roots)))
        for root in (self.user, self.system):
            for entry in (root / 'applications').glob('*.desktop'):
                entry.unlink()

    def roles(self, label, snapshot, file_id, expected, default, diagnostic=None):
        observations = {}
        for kind in ('directory', 'file'):
            request = dict(operation=kind + '-roles', snapshot=snapshot)
            if kind == 'file':
                request['id'] = file_id
            result, stderr = self.browser_rpc(label + '-' + kind, request)
            assert result['status'] == 'roles', result
            roles = result[kind + '_roles']
            wanted = expected[kind] if isinstance(expected, dict) else expected
            assert [row['id'] for row in roles['choices']] == wanted, roles
            assert roles['default_id'] == default, roles
            if kind == 'file':
                assert roles['observed_mime'] == 'text/plain', roles
            if diagnostic:
                assert 'fvwm-file-browser: XDG role ' + diagnostic in stderr, stderr
            else:
                assert not stderr, stderr
            observations[kind] = roles
        return observations

    def browser_cases(self):
        classifier = shutil.which('file', path=self.env['PATH'])
        if classifier is None:
            self.evidence['cases']['browser-roles'] = dict(status='unavailable',
                reason='real file(1) classifier absent; no substituted result')
            return
        self.evidence['artifacts']['classifier'] = identity(Path(classifier))
        initial = preferences(self.roots)
        scanned, _ = self.browser_rpc('scan', dict(operation='scan', path_bytes=encoded(self.cwd)))
        assert scanned['status'] == 'loaded', scanned
        snapshot = scanned['snapshot']
        assert base64.b64decode(snapshot['path_bytes']) == os.fsencode(self.cwd), snapshot
        matches = [row for row in snapshot['items'] if base64.b64decode(row['name_bytes']) == os.path.basename(self.target)]
        assert len(matches) == 1, matches
        file_id = matches[0]['id']
        located, _ = self.browser_rpc('locate-path', dict(operation='locate', target_bytes=[encoded(self.target)]))
        assert located['status'] == 'loaded' and located['selected_ids'] == [file_id], located
        no_display = dict(self.env)
        no_display.pop('DISPLAY', None)
        _, data = self.rpc('inspect-uri', self.browser,
            ['--inspect', '--reveal', self.uri], env=no_display)
        uri_result = json.loads(data)
        assert uri_result['schema_version'] == 1 and uri_result['status'] == 'loaded', uri_result
        assert uri_result['selected_ids'] == [file_id], uri_result
        assert base64.b64decode(uri_result['snapshot']['path_bytes']) == os.fsencode(self.cwd)
        assert [row['name_bytes'] for row in uri_result['snapshot']['items']] == [
            row['name_bytes'] for row in snapshot['items']], uri_result
        empty = self.roles('empty-providers', snapshot, file_id, [], '')
        assert initial == preferences(self.roots)
        command = desktop_string(exec_word(self.args.client.resolve()) + ' %f')

        def entry(root, name, extra='', mime='inode/directory;text/plain'):
            self.entry(root, name, 'Name=' + name + '\nExec=' + command +
                '\nMimeType=' + mime + ';\n' + extra)

        entry(self.user, 'good.desktop')
        entry(self.system, 'broken.desktop')
        self.entry(self.user, 'broken.desktop', 'Name=Broken\nExec=app %Z\n')
        self.defaults(self.config, 'broken.desktop;good.desktop;')
        malformed = self.roles('malformed-masks-lower', snapshot, file_id,
            ['good.desktop'], 'good.desktop', 'invalid-desktop-entry')
        (self.user / 'applications/broken.desktop').unlink()
        (self.system / 'applications/broken.desktop').unlink()
        entry(self.user, 'missing.desktop', 'TryExec=fvwm-no-such-fixture-program\n')
        self.defaults(self.config, 'missing.desktop;good.desktop;')
        missing = self.roles('missing-preferred', snapshot, file_id, ['good.desktop'], 'good.desktop')
        entry(self.system, 'good.desktop', 'Hidden=true\n')
        entry(self.system, 'alt.desktop')
        entry(self.system, 'parent.desktop', mime='application/octet-stream')
        self.defaults(self.config, 'good.desktop;')
        self.defaults(self.lower_config, 'alt.desktop;')
        with (self.lower_config / 'mimeapps.list').open('a') as stream:
            stream.write('application/octet-stream=parent.desktop;\n')
        precedence = self.roles('specificity-and-precedence', snapshot, file_id,
            dict(directory=['good.desktop', 'alt.desktop'],
                file=['good.desktop', 'alt.desktop', 'parent.desktop']), 'good.desktop')
        final = preferences(self.roots)
        # All preference changes above are deliberate fixture setup. Every RPC
        # separately snapshots bytes/mode/mtime around the installed process.
        self.case('browser-roles',
            'raw path/URI locate; empty inventory; malformed/missing preferred fallback; exact MIME before broader defaults; each child preserves defaults',
            dict(raw_target_hex=self.target.hex(), uri=self.uri, item=matches[0],
                located=located, inspected_uri=uri_result, empty=empty,
                malformed=malformed, missing=missing, precedence=precedence,
                initial_preferences=initial, final_preferences=final,
                launches=0))

    def helper_cases(self):
        before = preferences(self.roots)
        name = 'Known installed exec failure'
        child = Child(self, 'missing-exec', [self.helper, '--name', name, '--',
            self.runtime.root / 'no-such-program'])
        child.eof()
        code = child.finish(expected=None)
        assert code != 0 and bytes(child.errors).strip(), child.receipt
        ended = wait_for_predicate('known exec failure protocol removal', lambda: next((row
            for row in self.query() if row['name'] == name and row['state'] == 'protocol-ended'), None),
            time.monotonic() + 5, process=self.core)
        assert ended['window'] == 0, ended
        env = dict(self.env)
        env.pop('DISPLAY', None)
        child = Child(self, 'no-display', [self.helper, '--name', 'No display fallback',
            '--', self.args.client.resolve(), '--no-x', 'literal; $(never)'], env, self.cwd)
        observed = json.loads(child.line())
        assert observed['pid'] == child.process.pid and observed['window'] == 0, observed
        assert observed['argv_hex'] == hexed(['--no-x', 'literal; $(never)']), observed
        assert observed['cwd_hex'] == os.fsencode(self.cwd).hex(), observed
        assert observed['startup_id_hex'] is None, observed
        child.send(b'quit\n')
        child.eof()
        child.finish()
        assert not any(row['name'] == 'No display fallback' for row in self.query())
        assert before == preferences(self.roots)
        self.case('helper-failure-and-display-absence',
            'exec failure nonzero/stderr and protocol-ended; absent DISPLAY executes ordinary app with cleared inherited ID and Wait 0',
            dict(failure_exit=code, failure_activity=ended, fallback=observed,
                fallback_wait=child.receipt['exit'], preferences_before=before,
                preferences_after=preferences(self.roots)))

    def group_cases(self):
        before = preferences(self.roots)
        producer = Child(self, 'independent-producer', [self.args.producer.resolve()])
        ready = producer.line().decode('ascii').split()
        assert len(ready) == 3 and ready[0] == 'READY', ready
        screen = int(ready[2])

        def command(text):
            producer.send(text.encode('ascii') + b'\n')
            answer = producer.line().decode('ascii')
            fields = answer.split()
            assert len(fields) == 4 and fields[:2] == ['OK', text.split()[0]], answer
            self.evidence['producer_commands'].append(dict(command=text, reply=answer))
            assert len(self.evidence['producer_commands']) <= 128
            return int(fields[3])

        command('create 0')

        def announce(token, **extra):
            fields = dict(ID=token, NAME=token, SCREEN=screen, **extra)
            def quoted(value):
                return '"' + str(value).replace('\\', '\\\\').replace('"', '\\"') + '"'
            data = ('new: ' + ' '.join(key + '=' + quoted(value)
                for key, value in fields.items())).encode() + b'\0'
            assert len(data) <= 4096
            for offset in range(0, len(data), 20):
                command('fragment 0 %s 8 %s' %
                    ('begin' if offset == 0 else 'continue', data[offset:offset+20].hex()))
            return self.state(token, 'pending')

        prefix = 'installed-group-' + os.urandom(6).hex() + '-'
        class_name = 'InstalledGroup' + os.urandom(4).hex()
        announce(prefix + 'initial', WMCLASS=class_name)
        leader, dependent = command('create 10'), command('create 11')
        command('group 11 10')
        command('class 11 %s %s' % (b'fixture'.hex(), class_name.encode().hex()))
        command('map 11')
        mapped = self.mapped(dependent)
        initial = self.state(prefix + 'initial', 'window-observed')
        assert initial['window'] == dependent, initial
        assert command('listed 10') == 0, 'leader must remain unmanaged'
        pending = announce(prefix + 'late')
        assert pending['window'] == 0 and not pending.get('wmclass'), pending
        command('id 10 ' + (prefix + 'late').encode().hex())
        observed = self.state(prefix + 'late', 'window-observed')
        assert observed['window'] == dependent, observed
        command('destroy 11')
        released = wait_for_predicate('last group reference release',
            lambda: command('mask 10') == 0, time.monotonic() + 5, process=self.core)
        command('destroy 10')
        producer.send(b'quit\n')
        producer.eof()
        producer.finish()
        assert before == preferences(self.roots)
        self.case('late-unmanaged-group-property',
            'independent class completion AND actual M_MAP precede new no-class pending ID; late leader property selects exact dependent',
            dict(leader=leader, dependent=dependent, initial_class=initial, mapped=mapped,
                pending=pending, observed=observed, last_reference_released=released,
                producer_wait=producer.receipt['exit'],
                preferences_before=before, preferences_after=preferences(self.roots),
                exhaustive_PR300='retained separately; no expiry interval repeated'))

    def close(self):
        failures = []
        for child in reversed(self.children):
            try:
                child.cleanup()
                if child.receipt['forced_cleanup']:
                    failures.append('forced child cleanup: ' + child.label)
            except BaseException as error:
                failures.append('child cleanup ' + child.label + ': ' + repr(error))
        if self.core is not None:
            try:
                if self.core.poll() is None and self.peer is not None:
                    self.peer.send(b'Quit\n')
                code = self.core.wait_until_exit(time.monotonic() + 5)
                self.evidence['core_wait'] = code
                assert code == 0, code
            except BaseException as error:
                failures.append('core normal Quit/Wait failed: ' + repr(error))
        for connection in self.connections:
            connection.close()
        records = self.runtime.root / 'module-records'
        if records.exists():
            started = list(records.glob('module-*.json'))
            assert len(started) == 1, started
            for path in started:
                receipt = json.loads(path.read_text())
                stopped = records / path.name.replace('module-', 'stopped-')
                try:
                    wait_for_predicate('actual MFL child Wait receipt', stopped.exists,
                        time.monotonic() + 3)
                    waited = json.loads(stopped.read_text())
                    assert waited['child'] == receipt['child'] and waited['returncode'] == 0, waited
                    cleanup = records / path.name.replace('module-', 'cleanup-')
                    cleanup_receipt = json.loads(cleanup.read_text())
                    assert cleanup_receipt['child'] == receipt['child'], cleanup_receipt
                    assert cleanup_receipt['forced_cleanup'] is False, cleanup_receipt
                    self.evidence['mfl_wait'] = dict(started=receipt, stopped=waited,
                        cleanup=cleanup_receipt)
                except BaseException as error:
                    failures.append('MFL child Wait: ' + repr(error))
        for path in self.runtime.root.glob('*.log'):
            size = path.stat().st_size
            if size > WIRE_MAX:
                failures.append('infrastructure log exceeded cap: ' + path.name)
            with path.open('rb') as stream:
                (self.out / path.name).write_bytes(stream.read(WIRE_MAX))
        self.evidence['cleanup_failures'] = failures
        assert not failures, failures


def main():
    parser = argparse.ArgumentParser(description=__doc__, allow_abbrev=False)
    for name in ('prefix', 'fvwm', 'mfl', 'client', 'producer', 'artifacts-dir'):
        parser.add_argument('--' + name, type=Path, required=True)
    parser.add_argument('--execution-profile', required=True,
        help='runner-supplied target/CPU native-vs-emulated descriptor')
    args = parser.parse_args()
    out = args.artifacts_dir.resolve()
    out.mkdir(mode=0o700, parents=True, exist_ok=False)
    source = Path(__file__).resolve().parents[2]
    sources = ['tests/live_x/' + name for name in ('xdg_installed_live.py',
        'xdg_installed_client.c', 'startup_notification_client.c', 'live_x_harness.py',
        'native_module_proxy.py', 'mfl_stream_live.py', 'mfl_request_live.py',
        'geometry_transition_live.py')]
    sources += ['bin/startup_launch.c', 'bin/meson.build', 'bin/FvwmPrompt/meson.build',
        'bin/FvwmPrompt/fvwm-file-browser.desktop', 'fvwm/startup_notification.c',
        'fvwm/startup_notification.h', 'libs/startup_group_policy.c',
        'libs/startup_group_policy.h', 'libs/startup_notification_policy.c',
        'libs/startup_notification_policy.h', 'modules/FvwmMFL/FvwmMFL.c']
    for group in ('cmd/fvwmmenudesktop', 'cmd/fvwmfilebrowser', 'cmd/fvwmdesktopworker',
                  'internal/startuplaunch', 'internal/runtimepath', 'internal/desktopworker',
                  'internal/xdgmenu', 'internal/xdgsource', 'internal/filemodel'):
        sources += [str(path.relative_to(source)) for path in sorted(
            (source / 'bin/FvwmPrompt' / group).glob('*.go'))
            if not path.name.endswith('_test.go')]
    evidence = dict(status='running', execution_profile=args.execution_profile,
        uname=list(os.uname()), source_sha256={name: digest(source / name) for name in sources},
        children=[], cases={}, queries=[], worker_results=[], producer_commands=[],
        limits=dict(core_and_mfl='current build artifacts, not fully installed native modules',
            native_cpu='runner descriptor; no automatic native execution inference',
            full_matrix=False, UI=False, screenshot=False, detached_browser_launch=False,
            exhaustive_startup_lifetime='existing PR300 fixture retains its original scope',
            infrastructure='X server uses ordinary owned session SIGTERM during HarnessRuntime teardown'))
    runtime = HarnessRuntime(prefix='fvwm-xdg-installed-')
    fixture = Fixture(args, runtime, out, evidence)
    code = 1
    try:
        fixture.installed()
        fixture.setup()
        fixture.start_x()
        fixture.desktop_cases()
        fixture.browser_cases()
        fixture.helper_cases()
        fixture.group_cases()
        evidence['status'] = ('unavailable' if any(row['status'] == 'unavailable'
            for row in evidence['cases'].values()) else 'passed')
        code = 0 if evidence['status'] == 'passed' else 2
    except BaseException as error:
        evidence.update(status='failed', failure=repr(error), traceback=traceback.format_exc())
    finally:
        try:
            fixture.close()
        except BaseException as error:
            evidence.update(status='failed', cleanup_error=repr(error))
            code = 1
        try:
            runtime.close()
        except BaseException as error:
            evidence.update(status='failed', runtime_cleanup_error=repr(error))
            code = 1
        (out / 'evidence.json').write_text(json.dumps(evidence, indent=2) + '\n')
    print(json.dumps(dict(status=evidence['status'], evidence=str(out / 'evidence.json'))))
    return code


if __name__ == '__main__':
    if len(sys.argv) > 1 and sys.argv[1] == '--module-proxy':
        raise SystemExit(module_proxy(sys.argv[2], sys.argv[3], sys.argv[4:]))
    raise SystemExit(main())
