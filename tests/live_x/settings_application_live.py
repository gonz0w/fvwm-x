#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the GNU General Public License as published by the Free Software
# Foundation; either version 2, or any later version.
"""Opt-in actual Qt5/Firefox observations on owned X11 and portal resources.

Run only in the lead's Linux executor. Supply existing private Qt/GTK runtime
receipts and actual Firefox; this script installs nothing. Enabled and disabled
portal profiles each observe fresh, running and restarted clients while the real
native publisher changes font/motion/icon/cursor preferences. Its owned stdin is
the input: this establishes native publication, NOT Desktop/CFG provenance.

Application effects and non-effects are observations, not universal compliance
assertions. No Qt style/theme or browser motion/sandbox preference is forced.
Firefox runs as existing uid/gid65534, with private chowned HOME/profile/runtime,
using actual loopback Marionette framing and CSS matchMedia. An unavailable
sandbox/runtime is recorded as unavailable and stops dependent browser cases.
No host/user profile, default browser or security policy is changed.
"""
import argparse
import ast
import base64
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import signal
import socket
import stat
import subprocess
import sys
import time
from urllib.parse import quote

from browser_handoff_live import child_identity, process_identity
from inspector_live import atomic_json
from live_x_harness import HarnessRuntime, start_display_server
from native_settings_live import (SETTINGS, PipeChild, XObserver, apply_line,
    helper_result, wait_snapshot)
from settings_portal_live import (BUS, BUS_PATH, NAME, PUBLIC, PortalProbe,
    portal_arguments, validate_portal_args)

BROWSER_SHA256 = '3e40ae6b0f27a826c9a52522651f3c6f7406c2ee581d2cf5d42eeb668534ed42'
MAX_MESSAGE = 16 * 1024 * 1024
DISTRIBUTION_LIMITS = dict(entries=8192, files=4096, depth=16,
    file_bytes=512 * 1024 * 1024, total_bytes=2 * 1024 * 1024 * 1024)


class Unavailable(RuntimeError):
    """A required real application runtime cannot be exercised safely."""


def digest(path, maximum=None):
    result = hashlib.sha256()
    count = 0
    with Path(path).open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            count += len(block)
            assert maximum is None or count <= maximum, 'file hash byte bound'
            result.update(block)
    return result.hexdigest()


def distribution_manifest(browser):
    """Hash the supplied distribution as inputs, not as observed loaded files."""
    root = browser.resolve().parent
    pending, files, links, entries, total = [(root, 0)], {}, {}, 0, 0
    while pending:
        directory, depth = pending.pop()
        assert depth <= DISTRIBUTION_LIMITS['depth'], 'distribution depth bound'
        with os.scandir(directory) as children:
            for entry in children:
                entries += 1
                assert entries <= DISTRIBUTION_LIMITS['entries']
                path = Path(entry.path)
                relative = str(path.relative_to(root))
                info = entry.stat(follow_symlinks=False)
                if stat.S_ISDIR(info.st_mode):
                    pending.append((path, depth + 1))
                elif stat.S_ISLNK(info.st_mode):
                    links[relative] = os.readlink(path)
                else:
                    assert stat.S_ISREG(info.st_mode), relative
                    assert len(files) < DISTRIBUTION_LIMITS['files']
                    assert info.st_size <= DISTRIBUTION_LIMITS['file_bytes']
                    total += info.st_size
                    assert total <= DISTRIBUTION_LIMITS['total_bytes']
                    files[relative] = dict(bytes=info.st_size,
                        sha256=digest(path, DISTRIBUTION_LIMITS['file_bytes']))
    assert files, 'empty Firefox distribution'
    return dict(root=str(root), kind='distribution inputs; not process maps',
        bounds=DISTRIBUTION_LIMITS, total_regular_file_bytes=total,
        entries=entries, regular_files=files, symlinks=links)


def source_closure(source):
    """Freeze local Python imports transitively without importing candidates."""
    pending = [Path(__file__), Path(__file__).with_name(
        'settings_application_probe.py')]
    found = {}
    while pending:
        path = pending.pop().resolve()
        key = str(path.relative_to(source))
        if key in found:
            continue
        found[key] = digest(path)
        for node in ast.walk(ast.parse(path.read_text())):
            names = ([v.name for v in node.names] if
                isinstance(node, ast.Import) else [node.module] if
                isinstance(node, ast.ImportFrom) and node.module else [])
            for name in names:
                local = path.parent / (name.split('.')[0] + '.py')
                if local.is_file():
                    pending.append(local)
    for name in ('bin/native_settings.c', 'bin/native_settings_policy.c',
            'bin/native_settings_policy.h', 'bin/settings_portal.c',
            'bin/settings_portal_policy.c', 'bin/settings_portal_policy.h'):
        found[name] = digest(source / name)
    return found


def environment(receipt, base, evidence):
    raw = receipt.read_bytes()
    assert len(raw) <= 65536
    item = json.loads(raw)
    values = item.get('environment', item.get('env', item))
    assert isinstance(values, dict) and len(values) <= 64
    assert all(isinstance(k, str) and isinstance(v, str) and
        '\0' not in k + v for k, v in values.items())
    forbidden = {'DISPLAY', 'XAUTHORITY', 'HOME', 'DBUS_SESSION_BUS_ADDRESS',
        'XDG_CONFIG_HOME', 'XDG_RUNTIME_DIR'}
    assert not forbidden.intersection(values), forbidden.intersection(values)
    assert not any(k.startswith('MOZ_DISABLE_') for k in values)
    omitted = {'QT_STYLE_OVERRIDE', 'QT_QPA_PLATFORMTHEME', 'NO_AT_BRIDGE',
        'XCURSOR_THEME', 'XCURSOR_SIZE', 'GTK_THEME', 'GTK_USE_PORTAL',
        'GTK2_RC_FILES'}
    selected = {k: v for k, v in values.items() if k not in omitted}
    evidence.setdefault('runtime_environments', {})[str(receipt)] = dict(
        sha256=digest(receipt), selected=selected,
        omitted={k: v for k, v in values.items() if k in omitted})
    return {**{k: v for k, v in base.items() if k not in omitted}, **selected}


def mapped_files(pid, unavailable=None):
    found = set()
    path = Path('/proc/%d/maps' % pid)
    try:
        text = path.read_text()
    except PermissionError as failure:
        if unavailable is None:
            raise
        assert str(pid) in unavailable or len(unavailable) < 256
        unavailable.setdefault(str(pid), dict(status='unavailable',
            identity=process_identity(pid), path=str(path),
            error_type=type(failure).__name__, errno=failure.errno,
            error=str(failure),
            limit='kernel denied maps; no loaded-file observation for this PID'))
        return found
    for line in text.splitlines():
        candidate = line.rsplit(None, 1)[-1]
        if candidate.startswith('/') and Path(candidate).is_file():
            found.add(candidate)
    assert len(found) <= 1024
    return found


def session_members(leader):
    members = []
    for path in Path('/proc').iterdir():
        if not path.name.isdigit():
            continue
        try:
            fields = (path / 'stat').read_text().rsplit(')', 1)[1].split()
            if int(fields[3]) == leader:
                identity = process_identity(int(path.name))
                if identity:
                    members.append(identity)
        except FileNotFoundError:
            pass
    return members


def surviving_process(identity):
    """Parent changes on reparenting; PID and kernel start time stay stable."""
    current = process_identity(identity['child'])
    if current and current['starttime'] == identity['starttime']:
        return current
    return None


def kill_owned_process(identity):
    """Pin the process before checking reuse, then signal that exact handle."""
    try:
        descriptor = os.pidfd_open(identity['child'])
    except ProcessLookupError:
        return
    try:
        if surviving_process(identity):
            signal.pidfd_send_signal(descriptor, signal.SIGKILL)
    except ProcessLookupError:
        pass
    finally:
        os.close(descriptor)


def freeze_loaded(evidence, paths):
    loaded = evidence.setdefault('loaded_files_sha256', {})
    for path in sorted(paths):
        if path not in loaded:
            loaded[path] = digest(path)


class ApplicationPortal(PortalProbe):
    """Same actual bus/frontend, admitting the one unprivileged browser uid."""
    def start(self, name, argv, env=None):
        if name == 'portal-bus':
            config = Path(next(str(v).split('=', 1)[1] for v in argv
                if str(v).startswith('--config-file=')))
            text = config.read_text().replace('<policy context="default">',
                '<policy context="default"><allow user="65534"/>' +
                '<allow user="%d"/>' % os.geteuid())
            config.write_text(text)
        return super().start(name, argv, env)


class QtClient:
    def __init__(self, env, out, tag, evidence):
        self.child = PipeChild([sys.executable,
            Path(__file__).with_name('settings_application_probe.py')],
            {**env, 'FVWM_SETTINGS_CAPTURE_DIR': str(out)}, out, tag, evidence)
        self.evidence, self.tag = evidence, tag
        # Actual readiness: a mapped native widget, expected Qt/PyQt versions
        # and positive measured text dimensions, not mere process creation.
        try:
            self.state('ready')
        except BaseException:
            self.child.close()
            raise

    def state(self, capture=None):
        deadline = time.monotonic() + 8
        while True:
            self.child.send((json.dumps(dict(verb='state', capture=None)) +
                '\n').encode())
            result = json.loads(self.child.line(timeout=8))
            assert result['pid'] == self.child.process.pid
            assert result['qt'] == '5.15.13' and result['pyqt'] == '5.15.10'
            assert result['platform'] == 'xcb'
            if result['mapped']:
                break
            assert time.monotonic() < deadline, 'Qt widget not mapped'
            time.sleep(.05)
        assert result['xid'] > 0 and result['values']['font_height'] > 0
        assert all(v > 0 for v in result['values']['sample_size'])
        if capture:
            self.child.send((json.dumps(dict(verb='state',
                capture=self.tag + '-' + capture)) + '\n').encode())
            result = json.loads(self.child.line())
        freeze_loaded(self.evidence, mapped_files(self.child.process.pid))
        return result

    def close(self):
        self.child.close()


class Firefox:
    """One real Firefox with serial Marionette requests and owned process group."""
    def __init__(self, args, runtime, env, out, tag, evidence):
        self.process = self.stream = self.log = None
        self.closed = False
        self.evidence, self.out, self.tag, self.sequence = evidence, out, tag, 0
        root = runtime.root / tag
        root.mkdir(mode=0o700)
        for part in ('home', 'runtime', 'config', 'profile', 'cache', 'data', 'tmp'):
            (root / part).mkdir(mode=0o700)
        with socket.socket() as reservation:
            reservation.bind(('127.0.0.1', 0))
            port = reservation.getsockname()[1]
        # Only ordinary offline test preferences and the Marionette port.
        # No motion/media override, portal-forcing or security bypass is used.
        prefs = {'marionette.port': port, 'browser.shell.checkDefaultBrowser': False,
            'browser.startup.homepage': 'about:blank',
            'browser.startup.homepage_override.mstone': 'ignore',
            'browser.aboutwelcome.enabled': False,
            'browser.newtabpage.enabled': False, 'app.update.auto': False,
            'datareporting.policy.dataSubmissionEnabled': False,
            'toolkit.telemetry.enabled': False,
            'network.captive-portal-service.enabled': False,
            'network.connectivity-service.enabled': False}
        (root / 'profile' / 'user.js').write_text(''.join(
            'user_pref(%s, %s);\n' % (json.dumps(k), json.dumps(v))
            for k, v in prefs.items()))
        drop = {}
        if os.geteuid() == 0:
            drop = dict(user=65534, group=65534, extra_groups=[])
            for path in [*root.rglob('*'), root]:
                os.chown(path, 65534, 65534)
        elif os.geteuid() != 65534 or os.getegid() != 65534:
            raise Unavailable('Firefox requires existing uid/gid65534 executor')
        browser_env = {**env, 'HOME': str(root / 'home'),
            'XDG_CONFIG_HOME': str(root / 'config'),
            'XDG_RUNTIME_DIR': str(root / 'runtime'),
            'XDG_CACHE_HOME': str(root / 'cache'),
            'XDG_DATA_HOME': str(root / 'data'), 'GDK_BACKEND': 'x11',
            'MOZ_ENABLE_WAYLAND': '0', 'TMPDIR': str(root / 'tmp')}
        assert not any(k.startswith('MOZ_DISABLE_') for k in browser_env)
        self.log_path = out / (tag + '.log')
        self.log = self.log_path.open('wb')
        argv = [str(args.browser), '--no-remote', '--marionette',
            '--profile', str(root / 'profile'), 'about:blank']
        self.receipt = dict(argv=argv, uid=65534, gid=65534,
            profile_preferences=prefs, samples=[], wait=None)
        evidence.setdefault('browser_children', {})[tag] = self.receipt
        try:
            self.process = subprocess.Popen(argv, env=browser_env,
                stdin=subprocess.DEVNULL, stdout=self.log,
                stderr=subprocess.STDOUT, start_new_session=True, **drop)
            self.receipt.update(pid=self.process.pid,
                identity=child_identity(self.process.pid, os.getpid()))
            deadline = time.monotonic() + 30
            while self.stream is None:
                if self.process.poll() is not None:
                    self.startup_failure('Firefox exited before Marionette')
                try:
                    self.stream = socket.create_connection(
                        ('127.0.0.1', port), timeout=.3)
                except (ConnectionRefusedError, socket.timeout):
                    if time.monotonic() >= deadline:
                        self.startup_failure('Firefox Marionette deadline')
                    time.sleep(.1)
            self.stream.settimeout(8)
            greeting = self.receive()
            assert greeting['applicationType'] == 'gecko'
            assert greeting['marionetteProtocol'] == 3, greeting
            self.receipt['greeting'] = greeting
            session = self.call('WebDriver:NewSession', {'capabilities': {}})
            capabilities = session['capabilities']
            assert capabilities['browserName'] == 'firefox', capabilities
            assert capabilities['browserVersion'] == args.browser_version
            assert capabilities.get('moz:headless') is not True
            self.receipt['capabilities'] = capabilities
            self.evidence.setdefault('browser_runtime_versions', []).append(dict(
                process=self.receipt['identity'],
                version=capabilities['browserVersion'],
                launcher_sha256=digest(args.browser)))
            diagnostics = self.log_path.read_text(errors='replace')[-16384:]
            self.receipt['startup_diagnostics'] = diagnostics
            if re.search(r'CanCreateUserNamespace.*(?:failed|EPERM)|'
                    r'Sandbox.*(?:EPERM|Operation not permitted)', diagnostics):
                raise Unavailable('Firefox reports unavailable user namespace/' +
                    'sandbox: ' + diagnostics)
            browser_pid = capabilities['moz:processID']
            assert browser_pid == self.process.pid, (browser_pid, self.process.pid)
            status = Path('/proc/%d/status' % browser_pid).read_text()
            assert re.search(r'^Uid:\s+65534\s+65534\s+', status, re.M)
            self.receipt['kernel_uid_line'] = re.search(
                r'^Uid:.+$', status, re.M)[0]
            page = ('<!doctype html><title>Actual Firefox preference sample</title>'
                '<p id="sample">Application font sample: Aa Bb 0123456789</p>'
                '<button id="control">Actual browser widget</button>')
            self.call('WebDriver:Navigate', {'url': 'data:text/html;charset=utf-8,' +
                quote(page)})
            self.state('ready')
        except BaseException:
            self.close()
            raise

    def startup_failure(self, reason):
        text = self.log_path.read_text(errors='replace')[-8192:]
        if re.search(r'sandbox|user.?namespace|Operation not permitted|'
                r'error while loading shared libraries|cannot open shared object',
                text, re.I):
            raise Unavailable(reason + ': ' + text)
        raise RuntimeError(reason + ': ' + text)

    def exact(self, size):
        result = bytearray()
        while len(result) < size:
            part = self.stream.recv(min(65536, size - len(result)))
            assert part, 'Marionette EOF'
            result.extend(part)
        return bytes(result)

    def receive(self):
        size = bytearray()
        while True:
            byte = self.exact(1)
            if byte == b':':
                break
            assert byte.isdigit() and len(size) < 8, 'Marionette frame header'
            size.extend(byte)
        length = int(size)
        assert 0 < length <= MAX_MESSAGE
        return json.loads(self.exact(length))

    def call(self, command, parameters):
        self.sequence += 1
        message = json.dumps([0, self.sequence, command, parameters]).encode()
        assert len(message) <= MAX_MESSAGE
        self.stream.sendall(str(len(message)).encode() + b':' + message)
        reply = self.receive()
        assert isinstance(reply, list) and len(reply) == 4
        assert reply[:2] == [1, self.sequence], reply
        assert reply[2] is None, (command, reply[2])
        return reply[3]

    def state(self, capture=None):
        script = """
            const media = matchMedia('(prefers-reduced-motion: reduce)');
            if (!window.__motionEvents) {
                window.__motionEvents = [];
                media.addEventListener('change', e => {
                    if (window.__motionEvents.length < 128)
                        window.__motionEvents.push(e.matches);
                });
                window.__motionQuery = media;
            }
            const label = document.querySelector('#sample');
            const button = document.querySelector('#control');
            const box = label.getBoundingClientRect();
            return {ready: document.readyState, visibility: document.visibilityState,
                user_agent: navigator.userAgent, reduced_motion: media.matches,
                motion_events: window.__motionEvents.slice(),
                font: getComputedStyle(label).font,
                button_font: getComputedStyle(button).font,
                sample_size: [box.width, box.height],
                device_pixel_ratio: devicePixelRatio};
        """
        result = self.call('WebDriver:ExecuteScript', dict(
            script=script, args=[], newSandbox=False))['value']
        assert result['ready'] == 'complete' and result['visibility'] == 'visible'
        assert type(result['reduced_motion']) is bool
        assert 'Firefox/' in result['user_agent']
        assert all(v > 0 for v in result['sample_size'])
        members = session_members(self.process.pid)
        self.receipt['observed_session_members'] = members
        loaded = set()
        for member in members:
            try:
                loaded.update(mapped_files(member['child'], self.receipt.setdefault(
                    'kernel_maps_unavailable', {})))
            except FileNotFoundError:
                pass  # A browser-owned child can exit between independent reads.
        freeze_loaded(self.evidence, loaded)
        if capture:
            encoded = self.call('WebDriver:TakeScreenshot', dict(
                id=None, highlights=[], full=False))['value']
            image = base64.b64decode(encoded, validate=True)
            assert image.startswith(b'\x89PNG\r\n\x1a\n')
            path = self.out / (self.tag + '-' + capture + '.png')
            path.write_bytes(image)
            result['screenshot'] = str(path)
        self.receipt['samples'].append(result)
        assert len(self.receipt['samples']) < 256
        return result

    def close(self):
        if self.closed:
            return
        if self.stream:
            self.stream.close()
            self.stream = None
        if self.process:
            members = session_members(self.process.pid)
            forced = False
            if self.process.poll() is None:
                os.killpg(self.process.pid, signal.SIGTERM)
                try:
                    self.process.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    forced = True
                    os.killpg(self.process.pid, signal.SIGKILL)
            self.process.wait(timeout=3)
            self.receipt['wait'] = dict(returncode=self.process.returncode,
                forced=forced)
            # Direct child Wait is observed above. Browser-owned descendants
            # have a different Wait owner; attest only their kernel retirement.
            deadline = time.monotonic() + 4
            remaining = []
            while True:
                remaining = [current for member in members if
                    (current := surviving_process(member)) is not None]
                if not remaining or time.monotonic() >= deadline:
                    break
                for member in remaining:
                    kill_owned_process(member)
                time.sleep(.05)
            states = []
            for member in remaining:
                try:
                    raw = Path('/proc/%d/stat' % member['child']).read_text()
                    if surviving_process(member):
                        states.append(dict(identity=member,
                            state=raw.rsplit(')', 1)[1].split()[0]))
                except FileNotFoundError:
                    pass
            self.receipt['descendant_retirement'] = dict(
                observed=members, remaining=remaining, remaining_kernel_states=states,
                wait_owner='Firefox; controller observes kernel retirement only')
            assert not remaining, 'owned Firefox descendants still present'
        if self.log:
            self.log.close()
        self.closed = True


def observe(client, capture):
    # Keep the same actual client/event loop alive through a bounded settling
    # interval. Record the result even when this toolkit consumes no mapping.
    samples = [client.state()]
    deadline = time.monotonic() + 2
    while time.monotonic() < deadline:
        time.sleep(.2)
        samples.append(client.state())
    return dict(settle_seconds=2, samples=samples,
        final=client.state(capture))


def delta(before, after):
    old, new = before.get('values', before), after.get('values', after)
    ignored = {'motion_events', 'screenshot'}
    return {key: dict(before=old.get(key), after=new.get(key),
        changed=old.get(key) != new.get(key))
        for key in sorted(set(old) | set(new)) if key not in ignored}


def profile(args, name, runtime, env, out, evidence, browser_available):
    folder = out / name
    folder.mkdir()
    result = dict(status='running', observations={}, publication=[],
        cfg_provenance=False, source='actual native helper with owned stdin',
        portal='enabled' if name == 'enabled' else 'disabled stub/native only')
    evidence['profiles'][name] = result
    observer = portal = publisher = None
    clients, active = [], {}
    try:
        _, display = start_display_server(runtime, 'Xvfb', ['Xvfb', '-screen',
            '0', '1280x900x24', '-noreset', '-ac'], name + '-Xvfb.log',
            time.monotonic() + 10)
        actual_env = {**env, 'DISPLAY': display}
        observer = XObserver(display, evidence)
        if name == 'enabled':
            portal = ApplicationPortal(args, runtime, actual_env, folder,
                evidence, frontend=True)
            portal.start_frontend()
            activated = portal.wait_owner(NAME)
            result['portal_backend'] = activated
            actual_env = portal.env
            # Only these fixture-owned parents become traversable. Private
            # browser HOME/profile remain uid65534 mode0700.
            portal_root = Path(actual_env['HOME']).parent
            portal_root.chmod(0o711)
            (portal_root / 'bus').chmod(0o666)
        else:
            # Same real private bus, with no descriptor/frontend or backend.
            # Observe the installed default-disabled executable's actual refusal.
            disabled = argparse.Namespace(**{**vars(args),
                'helper': args.disabled_helper})
            portal = ApplicationPortal(disabled, runtime, actual_env, folder,
                evidence, frontend=False)
            child = portal.start('disabled-stub', [args.disabled_helper])
            assert child.wait_until_exit(time.monotonic() + 5) != 0
            for name_ in (NAME, PUBLIC):
                portal.call(BUS, BUS + '.GetNameOwner', ['string:' + name_],
                    path=BUS_PATH,
                    error='org.freedesktop.DBus.Error.NameHasNoOwner')
            actual_env = portal.env
            root = Path(actual_env['HOME']).parent
            root.chmod(0o711)
            (root / 'bus').chmod(0o666)
        result['display'] = display
        publisher = PipeChild([args.publisher], actual_env, folder,
            name + '-native-publisher', evidence)
        sequence = 0

        def publish(values):
            nonlocal sequence
            sequence += 1
            publisher.send(apply_line(sequence=sequence, values=values))
            reply = helper_result(publisher)
            assert reply['readiness'] == 'ready'
            snapshot = wait_snapshot(observer, values)
            assert snapshot['serial'] == reply['xserial']
            result['publication'].append(dict(reply=reply, snapshot=snapshot,
                fresh_xrm_lookup=observer.lookup()))
            if name == 'enabled':
                portal.wait_value(1 - values['Gtk/EnableAnimations'], public=True,
                    source=dict(kind='actual-native-pipe-not-CFG',
                        owner=snapshot['owner'], serial=snapshot['serial']))

        initial = {**SETTINGS, 'Gtk/EnableAnimations': 1}
        enlarged = {**SETTINGS, 'Gtk/FontName': 'Sans 22',
            'Gtk/CursorThemeSize': 48, 'Net/IconThemeName': 'hicolor'}
        publish(initial)
        factories = {}
        if 'qt' in args.groups:
            qt_env = environment(args.qt_env, actual_env, evidence)
            qt_env['QT_QPA_PLATFORM'] = 'xcb'
            factories['qt'] = lambda tag: QtClient(qt_env, folder, tag, evidence)
        if 'browser' in args.groups and browser_available:
            browser_env = environment(args.browser_env, actual_env, evidence)
            factories['browser'] = lambda tag: Firefox(args, runtime,
                browser_env, folder, tag, evidence)
        elif 'browser' in args.groups:
            result['observations']['browser'] = dict(status='unavailable',
                reason='prior browser runtime unavailable', dependent_cases='stopped')
        for group, factory in factories.items():
            try:
                current = factory(name + '-' + group + '-fresh')
            except Unavailable as failure:
                result['observations'][group] = dict(status='unavailable',
                    reason=str(failure), dependent_cases='stopped')
                browser_available = False
                continue
            clients.append(current)
            active[group] = current
            result['observations'][group] = dict(status='running',
                initial=observe(current, 'initial'))
        publish(enlarged)
        for group, item in result['observations'].items():
            if item['status'] == 'unavailable':
                continue
            current = active[group]
            item['running_enlarged'] = observe(current, 'enlarged')
            item['running_effects'] = delta(item['initial']['final'],
                item['running_enlarged']['final'])
            try:
                fresh = factories[group](name + '-' + group + '-new-enlarged')
                clients.append(fresh)
                item['fresh_enlarged'] = observe(fresh, 'enlarged')
                fresh.close()
                current.close()
                restarted = factories[group](name + '-' + group + '-restarted')
                clients.append(restarted)
                item['restarted_enlarged'] = observe(restarted, 'enlarged')
                item['restart_effects'] = delta(item['initial']['final'],
                    item['restarted_enlarged']['final'])
                item['status'] = 'observed'
            except Unavailable as failure:
                item.update(status='unavailable', reason=str(failure),
                    dependent_cases='stopped')
                browser_available = False
        publish(initial)
        for group, item in result['observations'].items():
            if item['status'] == 'observed':
                current = next(c for c in reversed(clients) if
                    c.tag == name + '-' + group + '-restarted')
                item['running_restored'] = observe(current, 'restored')
                item['restore_effects'] = delta(item['restarted_enlarged']['final'],
                    item['running_restored']['final'])
        publisher.eof(expected=0)
        assert helper_result(publisher)['readiness'] == 'off'
        assert observer.owner() == 0
        if name == 'enabled':
            portal.wait_value(0, public=True,
                source=dict(kind='actual-native-publisher-parent-eof'))
        else:
            for name_ in (NAME, PUBLIC):
                portal.call(BUS, BUS + '.GetNameOwner', ['string:' + name_],
                    path=BUS_PATH,
                    error='org.freedesktop.DBus.Error.NameHasNoOwner')
        result['status'] = ('unavailable' if any(v['status'] == 'unavailable'
            for v in result['observations'].values()) else 'observed')
        return browser_available
    finally:
        failures = []
        for label, action in [(c.tag, c.close) for c in reversed(clients)] + [
                ('native publisher', publisher.close if publisher else None),
                ('portal', portal.close if portal else None),
                ('observer', observer.close if observer else None)]:
            if action:
                try:
                    action()
                except BaseException as failure:
                    failures.append(dict(step=label, error=repr(failure)))
        result['cleanup_failures'] = failures
        if portal:
            result['portal_receipt'] = evidence.pop('portal')
        if failures:
            raise RuntimeError('application profile cleanup failed: ' +
                repr(failures))


def run(args):
    out = args.artifacts_dir
    out.mkdir(parents=True, exist_ok=False)
    source = Path(__file__).resolve().parents[2]
    evidence = dict(status='running', execution_profile=args.execution_profile,
        profiles={}, source_sha256=source_closure(source),
        limits=['Native publisher pipe input is not Desktop CFG provenance.',
            'Exact Qt5/Firefox versions only; non-effects remain observations.',
            'Denied browser process maps remain unavailable metadata; the full '
            'distribution manifest attests inputs, not actually loaded files.',
            'No physical cursor theme/size or compositor/hardware inference.',
            'Native CPU, sanitizers and six-target acceptance are lead gates.'])
    inputs = {k: getattr(args, k) for k in ('helper', 'publisher', 'frontend',
        'disabled_helper', 'portal_descriptor', 'activation_service')}
    for group in args.groups:
        inputs[group + '_env'] = getattr(args, group + '_env')
    if 'browser' in args.groups:
        inputs['browser'] = args.browser
        assert digest(args.browser) == args.browser_sha256
    if args.portal_env:
        inputs['portal_env'] = args.portal_env
    evidence['inputs'] = {k: dict(path=str(p), sha256=digest(p))
        for k, p in inputs.items() if p is not None}
    evidence['external_tools'] = {k: dict(path=shutil.which(k),
        sha256=digest(shutil.which(k))) for k in
        ('Xvfb', 'dbus-daemon', 'dbus-send', 'dbus-monitor', 'gdbus')}
    runtime = HarnessRuntime(prefix='fvwm-settings-applications-')
    runtime.root.chmod(0o711)
    try:
        if 'browser' in args.groups:
            evidence['browser_distribution'] = distribution_manifest(args.browser)
        env = runtime.environment()
        for key in ('XAUTHORITY', 'NO_AT_BRIDGE', 'QT_STYLE_OVERRIDE',
                'QT_QPA_PLATFORMTHEME', 'QT_QPA_PLATFORM', 'XCURSOR_THEME',
                'XCURSOR_SIZE', 'XENVIRONMENT', 'GTK_THEME', 'GTK_USE_PORTAL',
                'GTK2_RC_FILES', 'GSETTINGS_SCHEMA_DIR', 'MOZ_MARIONETTE'):
            env.pop(key, None)
        browser_available = True
        for name in args.profiles:
            browser_available = profile(args, name, runtime, env, out, evidence,
                browser_available)
        assert evidence['source_sha256'] == source_closure(source)
        assert all(digest(v['path']) == v['sha256'] for v in
            evidence['inputs'].values()), 'input changed during observation'
        assert all(digest(p) == h for p, h in
            evidence.get('loaded_files_sha256', {}).items())
        if 'browser' in args.groups:
            assert evidence['browser_distribution'] == distribution_manifest(
                args.browser), 'Firefox distribution inputs changed during observation'
        assert not evidence.get('observer_unexpected_x_errors')
        evidence['status'] = ('unavailable' if not browser_available else
            'observed')
    except BaseException as failure:
        evidence.update(status='failed', failure=dict(
            type=type(failure).__name__, message=str(failure)[:8192]))
        raise
    finally:
        evidence['cleanup_failures'] = []
        for child in reversed(runtime.processes):
            try:
                child.close(time.monotonic() + 5)
            except BaseException as failure:
                evidence['cleanup_failures'].append(dict(
                    step=child.name, error=repr(failure)))
        evidence['owned_processes'] = [dict(name=c.name, argv=c.argv,
            pid=c.process.pid, identity=getattr(c, 'portal_identity', None),
            wait=dict(returncode=c.process.returncode))
            for c in runtime.processes]
        for path in runtime.root.glob('*.log'):
            shutil.copyfile(path, out / path.name)
        try:
            runtime.close()
        except BaseException as failure:
            evidence['cleanup_failures'].append(dict(
                step='runtime', error=repr(failure)))
        if evidence['cleanup_failures']:
            evidence['status'] = 'failed'
        atomic_json(out / 'result.json', evidence)
    print(json.dumps(dict(status=evidence['status'], artifacts=str(out))))
    return 0 if evidence['status'] == 'observed' else 2


def main():
    parser = argparse.ArgumentParser(description=__doc__, allow_abbrev=False)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--artifacts-dir', type=Path, required=True)
    parser.add_argument('--execution-profile', required=True,
        choices=('native-linux', 'emulated-linux'))
    parser.add_argument('--profiles', nargs='+', choices=('enabled', 'disabled'),
        default=['enabled', 'disabled'])
    parser.add_argument('--groups', nargs='+', choices=('qt', 'browser'),
        default=['qt', 'browser'])
    parser.add_argument('--publisher', type=Path)
    parser.add_argument('--disabled-helper', type=Path, required=True)
    parser.add_argument('--qt-env', type=Path)
    parser.add_argument('--browser-env', type=Path)
    parser.add_argument('--browser', type=Path)
    parser.add_argument('--browser-version', default='159.0')
    parser.add_argument('--browser-sha256', default=BROWSER_SHA256)
    portal_arguments(parser)
    args = parser.parse_args()
    if sys.platform != 'linux':
        parser.error('requires the lead-owned Linux application executor')
    if not shutil.which('Xvfb'):
        parser.error('actual installed Xvfb is unavailable')
    if (len(set(args.profiles)) != len(args.profiles) or
            len(set(args.groups)) != len(args.groups)):
        parser.error('profiles and groups must be unique')
    args.build = args.build.resolve()
    args.artifacts_dir = args.artifacts_dir.resolve()
    args.helper = args.helper or args.build / 'bin/fvwm-settings-portal'
    args.publisher = args.publisher or args.build / 'bin/fvwm-native-settings'
    validate_portal_args(parser, args, frontend='enabled' in args.profiles)
    for name in ('publisher', 'disabled_helper', 'qt_env', 'browser_env', 'browser'):
        path = getattr(args, name)
        required = (name in ('publisher', 'disabled_helper') or
            name == 'qt_env' and 'qt' in args.groups or
            name in ('browser', 'browser_env') and 'browser' in args.groups)
        if required and path is None:
            parser.error('required --' + name.replace('_', '-'))
        if path:
            path = path.resolve()
            if not path.is_file() or (name in ('publisher', 'disabled_helper',
                    'browser') and not os.access(path, os.X_OK)):
                parser.error('unavailable input ' + name + ': ' + str(path))
            setattr(args, name, path)
    return run(args)


if __name__ == '__main__':
    raise SystemExit(main())
