#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the GNU General Public License as published by the Free Software
# Foundation; either version 2, or any later version.
"""Opt-in actual-X/private-bus Settings backend and installed frontend fixture.

The direct group injects explicitly labelled XSETTINGS protocol samples and also
uses the actual native publisher's pipe. Neither establishes CFG provenance.
The optional native_settings_live.py hook observes the real Desktop CFG chain.
The frontend group requires an actual installed xdg-desktop-portal executable,
the installed fvwm .portal/.service files and their matching installed backend.
No frontend, service descriptor, toolkit, package or readiness is emulated.

--portal-env supplies a JSON environment (or an 'environment'/'env' object) for
an existing private frontend runtime. The fixture overrides display, bus, home
and XDG config/data homes with its own disposable resources. Consumer UI,
GTK/Qt/browser effects, serial rollover and wider support gates are separate.
Only explicitly selecting --groups direct permits omitting --frontend.
FreeBSD/OpenBSD support only direct owned children. Bus PID plus actual Popen
ownership/Wait and declared binary hashes are recorded without kernel-image
claims; activation/frontend observations retain their Linux /proc requirement.
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
import time
import xml.etree.ElementTree as ET
from xml.sax.saxutils import escape

from browser_handoff_live import process_identity
from inspector_live import atomic_json
from live_x_harness import (HarnessRuntime, start_display_server,
    wait_for_predicate)
from native_settings_live import (SETTINGS, PipeChild, XObserver, apply_line,
    digest, helper_result, owned_spawn_identity, wait_snapshot)

NAME = 'org.freedesktop.impl.portal.desktop.fvwm'
PUBLIC = 'org.freedesktop.portal.Desktop'
PATH = '/org/freedesktop/portal/desktop'
IMPL = 'org.freedesktop.impl.portal.Settings'
FRONT = 'org.freedesktop.portal.Settings'
NS, KEY = 'org.freedesktop.appearance', 'reduced-motion'
BUS = 'org.freedesktop.DBus'
BUS_PATH = '/org/freedesktop/DBus'


def payload(animations=0, serial=1, big=False, extra=True):
    """Independent protocol input, distinct from native CFG data."""
    order = '>' if big else '<'
    records = []
    if extra:
        records.extend([(1, b'Other/String', b'value'),
            (2, b'Other/Color', (1, 2, 3, 4)), (0, b'Other/Integer', -2)])
    if animations is not None:
        records.append((0, b'Gtk/EnableAnimations', animations))
    raw = bytearray(struct.pack(order + 'B3xII',
        int(big), serial, len(records)))
    for kind, name, value in records:
        raw.extend(struct.pack(order + 'BBH', kind, 0, len(name)))
        raw.extend(name)
        raw.extend(b'\0' * (-len(raw) % 4))
        raw.extend(struct.pack(order + 'I', serial))
        if kind == 0:
            raw.extend(struct.pack(order + 'i', value))
        elif kind == 1:
            raw.extend(struct.pack(order + 'I', len(value)))
            raw.extend(value)
            raw.extend(b'\0' * (-len(raw) % 4))
        else:
            raw.extend(struct.pack(order + 'HHHH', *value))
    return bytes(raw)


def publish(observer, raw, window=None, property_type=None, form=8,
        require_owner=True):
    window = window or observer.contender
    assert window and (not require_owner or observer.owner() == window)
    memory = C.create_string_buffer(raw)
    observer.x.XChangeProperty(observer.d, window,
        observer.atom('_XSETTINGS_SETTINGS'),
        observer.atom(property_type or '_XSETTINGS_SETTINGS'), form, 0,
        memory, len(raw) if form == 8 else len(raw) // (form // 8))
    observer.x.XSync(observer.d, 0)


def reply_tokens(text):
    lines = text.splitlines()
    assert lines and lines[0].startswith('method return '), text
    body = '\n'.join(lines[1:])
    tokens = re.findall(
        r'"[^"\\]*(?:\\.[^"\\]*)*"|[A-Za-z0-9_]+|[\[\]()]', body)
    # Never discard unexpected punctuation while accepting a typed reply.
    assert re.sub(r'\s+', '', body) == ''.join(tokens), body
    return tokens


class PortalProbe:
    """Own real backend/frontend/bus children, including in the CFG run."""
    def __init__(self, args, runtime, env, out, evidence, frontend=False):
        assert not frontend or sys.platform == 'linux', (
            'activated/frontend ownership requires the Linux observation path')
        self.args, self.runtime, self.out = args, runtime, out
        self.evidence, self.frontend_enabled = evidence, frontend
        self.env = dict(env)
        if args.portal_env:
            supplied = json.loads(args.portal_env.read_text())
            supplied = supplied.get('environment',
                supplied.get('env', supplied))
            assert isinstance(supplied, dict) and all(isinstance(k, str) and
                isinstance(v, str) and '\0' not in k + v
                for k, v in supplied.items())
            self.env.update(supplied)
        self.children = []
        self.activated = None
        self.closed = False
        self.tag = str(len(runtime.processes))
        root = runtime.root / ('portal-' + self.tag)
        root.mkdir(mode=0o700)
        for part in ('home', 'config', 'data', 'runtime', 'services'):
            (root / part).mkdir(mode=0o700)
        self.env.update(DISPLAY=env['DISPLAY'], HOME=str(root / 'home'),
            XDG_CONFIG_HOME=str(root / 'config'),
            XDG_CONFIG_DIRS=str(root / 'config'),
            XDG_DATA_HOME=str(root / 'data'),
            XDG_RUNTIME_DIR=str(root / 'runtime'),
            XDG_CURRENT_DESKTOP='FVWM', DBUS_STARTER_BUS_TYPE='session')
        for key in ('DBUS_STARTER_ADDRESS', 'DBUS_SESSION_BUS_ADDRESS'):
            self.env.pop(key, None)
        self.env.pop('AT_SPI_BUS_ADDRESS', None)
        self.env.pop('XDG_DESKTOP_PORTAL_CONFIG_DIR', None)
        self.env['XDG_DESKTOP_PORTAL_DIR'] = str(
            root / 'data' / 'xdg-desktop-portal' / 'portals')
        if frontend:
            self.install_metadata(root)
        config = root / 'bus.conf'
        config.write_text('<busconfig><type>session</type><listen>unix:path=' +
            escape(str(root / 'bus')) + '</listen><auth>EXTERNAL</auth>' +
            '<servicedir>' + escape(str(root / 'services')) + '</servicedir>' +
            '<policy context="default"><allow send_destination="*"/>' +
            '<allow receive_sender="*"/><allow own="*"/>' +
            '<allow eavesdrop="true"/></policy></busconfig>\n')
        self.bus = self.start('portal-bus', ['dbus-daemon', '--nofork',
            '--nopidfile', '--print-address=1', '--config-file=' + str(config)])
        def bus_address():
            text = self.bus.log_path.read_text()
            return text.splitlines()[0] if text.startswith('unix:') else None
        self.address = wait_for_predicate('actual owned portal bus address',
            bus_address, time.monotonic() + 8, process=self.bus)
        self.env['DBUS_SESSION_BUS_ADDRESS'] = self.address
        self.env['DBUS_STARTER_ADDRESS'] = self.address
        self.backend = self.frontend = None
        self.monitor_count = 0
        evidence['portal'] = dict(address=self.address,
            display=self.env['DISPLAY'],
            source_provenance='caller must identify CFG or injected publisher',
            calls=[], owners=[], changes=[],
            metadata=getattr(self, 'metadata', None))
        evidence['portal'].update(bus_config=config.read_text(),
            environment={key: self.env.get(key) for key in ('DISPLAY',
                'XAUTHORITY', 'XDG_DATA_HOME', 'XDG_DATA_DIRS',
                'XDG_CONFIG_HOME',
                'XDG_CURRENT_DESKTOP', 'XDG_DESKTOP_PORTAL_DIR')},
            portal_environment_receipt=(dict(path=str(args.portal_env),
                sha256=digest(args.portal_env)) if args.portal_env else None))
        self.receipt = evidence['portal']

    def install_metadata(self, root):
        descriptor = self.args.portal_descriptor
        service = self.args.activation_service
        text = descriptor.read_text()
        assert 'DBusName=' + NAME in text and IMPL in text, text
        service_text = service.read_text()
        assert 'Name=' + NAME in service_text, service_text
        command = re.findall(r'^Exec=(.+)$', service_text, re.M)
        assert len(command) == 1
        argv = shlex.split(command[0])
        assert len(argv) == 1 and Path(argv[0]).resolve() == (
            self.args.helper.resolve()), argv
        data = root / 'data' / 'xdg-desktop-portal' / 'portals'
        data.mkdir(parents=True)
        shutil.copyfile(descriptor, data / 'fvwm.portal')
        shutil.copyfile(service, root / 'services' / service.name)
        # XDG_DESKTOP_PORTAL_DIR overrides both descriptor and configuration
        # lookup in the real frontend. Keep its private selection in that same
        # directory; XDG_CONFIG_HOME is deliberately ignored by this override.
        config = data
        # This private selection names only Settings. No borrowed configuration
        # or activation environment is changed by this fixture.
        (config / 'portals.conf').write_text('[preferred]\ndefault=none\n' +
            IMPL + '=fvwm\n')
        self.metadata = dict(descriptor=dict(path=str(descriptor),
            sha256=digest(descriptor)),
            activation_service=dict(path=str(service), sha256=digest(service)),
            installed_exec=argv,
            selection=(config / 'portals.conf').read_text())

    def start(self, name, argv, env=None):
        name += '-' + self.tag
        child = self.runtime.start(name, [str(v) for v in argv], name + '.log',
            self.env if env is None else env)
        child.portal_identity = owned_spawn_identity(
            child.process, child.argv, self.env if env is None else env)
        self.children.append(child)
        return child

    def call(self, name, method, arguments=(), error=None,
            path=PATH, record=True):
        argv = ['dbus-send', '--session', '--print-reply',
            '--reply-timeout=3000',
            '--dest=' + name, path, method, *arguments]
        result = subprocess.run(argv, env=self.env, stdout=subprocess.PIPE,
            stderr=subprocess.PIPE, timeout=5, text=True)
        assert len(result.stdout) + len(result.stderr) < 131072
        if record:
            self.evidence['portal']['calls'].append(dict(argv=argv,
                returncode=result.returncode, stdout=result.stdout,
                stderr=result.stderr))
        if error:
            assert result.returncode != 0 and error in result.stderr, result
            return None
        assert result.returncode == 0, (argv, result.stdout, result.stderr)
        return reply_tokens(result.stdout)

    def owner(self, name):
        reply = self.call(BUS, BUS + '.GetNameOwner', ['string:' + name],
            path=BUS_PATH, record=False)
        assert len(reply) == 2 and reply[0] == 'string' and re.fullmatch(
            r'":\d+\.\d+"', reply[1])
        unique = reply[1][1:-1]
        pid = self.call(BUS, BUS + '.GetConnectionUnixProcessID',
            ['string:' + unique],
            path=BUS_PATH, record=False)
        assert len(pid) == 2 and pid[0] == 'uint32'
        return dict(name=name, unique=unique, pid=int(pid[1]))

    def wait_owner(self, name, child=None):
        def current():
            try:
                result = self.owner(name)
            except AssertionError:
                return None
            if child:
                assert result['pid'] == child.process.pid, (result, child.argv)
                assert child.process.poll() is None, (result, child.argv)
            return result
        if sys.platform != 'linux':
            assert child is not None and child in self.children, (
                'BSD bus owner requires this probe\'s actual owned Popen child')
        result = wait_for_predicate('actual bus owner ' + name, current,
            time.monotonic() + 8, process=child)
        if sys.platform == 'linux':
            exe = Path('/proc/%d/exe' % result['pid'])
            result.update(identity=process_identity(result['pid']),
                executable=str(exe.resolve()), executable_sha256=digest(exe))
            if name == NAME and child is None:
                assert result['executable_sha256'] == (
                    digest(self.args.helper)), result
                self.activated = result
        else:
            identity = child.portal_identity
            assert identity and identity['kind'] == 'owned-spawn'
            assert identity['child'] == result['pid']
            assert child.process.poll() is None
            result.update(identity=identity, declared_argv=list(child.argv),
                executable_input=identity['executable_input'],
                ownership_observation='owned spawn plus live bus PID',
                kernel_image='not observed', wait_method=identity['wait_method'])
        self.evidence['portal']['owners'].append(result)
        return result

    def start_backend(self):
        self.backend = self.start(
            'portal-backend-%d' % len(self.runtime.processes),
            [self.args.helper])
        owner = self.wait_owner(NAME, self.backend)
        if sys.platform == 'linux':
            assert owner['executable_sha256'] == digest(self.args.helper)
        else:
            assert self.backend.argv == [str(self.args.helper)]
            assert owner['executable_input']['sha256'] == digest(self.args.helper)
        return owner

    def start_frontend(self):
        assert sys.platform == 'linux', 'frontend observations require Linux'
        self.frontend = self.start(
            'portal-frontend-%d' % len(self.runtime.processes),
            [self.args.frontend])
        owner = self.wait_owner(PUBLIC, self.frontend)
        assert owner['executable_sha256'] == digest(self.args.frontend)
        # Real readiness is the public method returning the exact expected type,
        # after frontend selection and backend initialization have completed.
        self.wait_value(0, public=True)
        return owner

    def read(self, public=False, legacy=False):
        interface, name = (FRONT, PUBLIC) if public else (IMPL, NAME)
        method = 'Read' if not public or legacy else 'ReadOne'
        value = self.call(name, interface + '.' + method,
            ['string:' + NS, 'string:' + KEY], record=False)
        prefix = ['variant', 'variant'] if public and legacy else ['variant']
        assert value[:-1] == prefix + ['uint32'] and value[-1] in (
            '0', '1'), value
        return int(value[-1])

    def wait_value(self, value, public=False, source=None):
        def observed():
            try:
                actual = self.read(public=public)
            except AssertionError:
                return None
            return dict(value=actual) if actual == value else None
        result = wait_for_predicate('typed portal value %d' % value, observed,
            time.monotonic() + 8,
            process=self.frontend if public else self.backend)
        result.update(public=public, source=source, at=time.monotonic())
        self.evidence['portal']['changes'].append(result)
        return result

    def read_all(self, patterns, value=0, public=False, empty=False):
        interface, name = (FRONT, PUBLIC) if public else (IMPL, NAME)
        if patterns in ([], ['']):
            # dbus-send cannot express an empty container. GDBus introspects
            # the actual as argument and sends [] (or one empty string) on wire.
            argv = ['gdbus', 'call', '--session', '--dest', name,
                '--object-path', PATH, '--method', interface + '.ReadAll',
                json.dumps(patterns)]
            result = subprocess.run(argv, env=self.env, capture_output=True,
                text=True, timeout=5)
            self.evidence['portal']['calls'].append(dict(argv=argv,
                returncode=result.returncode, stdout=result.stdout,
                stderr=result.stderr))
            assert result.returncode == 0, result.stderr
            expected = "({'%s': {'%s': <uint32 %d>}},)" % (NS, KEY, value)
            assert re.sub(r'\s+', '', result.stdout) == re.sub(
                r'\s+', '', expected), result.stdout
            return
        result = self.call(name, interface + '.ReadAll',
            ['array:string:' + ','.join(patterns)])
        expected = ['array', '[', ']'] if empty else ['array', '[', 'dict',
            'entry', '(', 'string', '"' + NS + '"', 'array', '[',
            'dict', 'entry',
            '(', 'string', '"' + KEY + '"', 'variant', 'uint32', str(value),
            ')', ']', ')', ']']
        assert result == expected, (patterns, result, expected)

    def monitor(self, public=False):
        self.monitor_count += 1
        interface, name = (FRONT, PUBLIC) if public else (IMPL, NAME)
        owner = self.owner(name)
        child = self.start('portal-monitor-%d' % self.monitor_count,
            ['dbus-monitor', '--session', '--monitor',
                ("type='signal',sender='%s',path='%s',interface='%s',"
                    "member='SettingChanged'") %
                (owner['unique'], PATH, interface)])
        wait_for_predicate('actual monitor BecomeMonitor receipt', lambda:
            'member=NameLost' in child.log_path.read_text(),
            time.monotonic() + 5, process=child)
        return child, owner['unique'], interface

    def signals(self, monitor):
        child, sender, interface = monitor
        text = child.log_path.read_text()
        assert len(text) < 1048576, 'bounded monitor evidence'
        blocks = re.split(r'(?m)^(?=signal |method |error )', text)
        values = []
        for block in blocks:
            if 'member=SettingChanged' not in block:
                continue
            header, body = block.split('\n', 1)
            assert 'sender=' + sender + ' ' in header, header
            assert 'path=' + PATH + ';' in header
            assert 'interface=' + interface + ';' in header
            tokens = re.findall(r'"[^"\\]*"|[A-Za-z0-9_]+', body)
            assert re.sub(r'\s+', '', body) == ''.join(tokens), body
            assert tokens[:-1] == ['string', '"' + NS + '"', 'string',
                '"' + KEY + '"', 'variant', 'uint32'] and tokens[-1] in (
                    '0', '1'), tokens
            values.append(int(tokens[-1]))
        return values

    def wait_signals(self, monitor, expected):
        wait_for_predicate('exact typed SettingChanged sequence', lambda:
            self.signals(monitor) == expected, time.monotonic() + 6,
            process=monitor[0])

    def close(self):
        # A CFG caller shares the runtime; close only this probe's processes.
        if self.closed:
            return
        if self.activated:
            owner = self.activated
            identity = owner['identity']
            if process_identity(owner['pid']) == identity:
                os.kill(owner['pid'], signal.SIGTERM)
                wait_for_predicate('owned activated backend cleanup retirement',
                    lambda: process_identity(owner['pid']) != identity,
                    time.monotonic() + 5)
                self.evidence.setdefault('activated_cleanup', []).append(dict(
                    identity=identity, wait_owner=(
                        'private dbus-daemon; kernel retirement observed')))
        for child in reversed(self.children):
            if child is not self.bus:
                child.close(time.monotonic() + 4)
        self.bus.close(time.monotonic() + 4)
        self.receipt['owned_children'] = [dict(name=p.name, argv=p.argv,
            identity=p.portal_identity,
            wait=dict(returncode=p.process.returncode))
            for p in self.children]
        self.closed = True


def protocol_cases(probe):
    probe.wait_value(0)
    for patterns in ([], [NS], ['org.freedesktop.*'], ['*'], ['']):
        probe.read_all(patterns)
    for patterns in (['org.example.*'], [NS + '.*']):
        probe.read_all(patterns, empty=True)
    for patterns in (['org.*.appearance'], ['x' * 129], [NS] * 17):
        probe.call(NAME, IMPL + '.ReadAll',
            ['array:string:' + ','.join(patterns)],
            error='org.freedesktop.DBus.Error.InvalidArgs')
    for method, args in [('ReadAll', []), ('ReadAll', ['string:' + NS]),
            ('Read', ['string:' + NS]), ('Read', ['string:' + NS, 'uint32:0']),
            ('Read', ['string:' + NS, 'string:' + KEY, 'string:extra'])]:
        probe.call(NAME, IMPL + '.' + method, args,
            error='org.freedesktop.DBus.Error.InvalidArgs')
    for namespace, key in [(NS, 'unknown'), ('org.example', KEY)]:
        probe.call(NAME, IMPL + '.Read',
            ['string:' + namespace, 'string:' + key],
            error='org.freedesktop.portal.Error.NotFound')
    probe.call(NAME, IMPL + '.ReadOne', ['string:' + NS, 'string:' + KEY],
        error='org.freedesktop.DBus.Error.UnknownMethod')
    probe.call(NAME, IMPL + '.Write',
        ['string:' + NS, 'string:' + KEY, 'uint32:1'],
        error='org.freedesktop.DBus.Error.UnknownMethod')
    properties = 'org.freedesktop.DBus.Properties.'
    assert probe.call(NAME, properties + 'Get',
        ['string:' + IMPL, 'string:version']) == [
        'variant', 'uint32', '1']
    assert probe.call(NAME, properties + 'GetAll', ['string:' + IMPL]) == [
        'array', '[', 'dict', 'entry', '(', 'string', '"version"',
        'variant', 'uint32', '1', ')', ']']
    probe.call(NAME, properties + 'Set', ['string:' + IMPL,
        'string:version', 'variant:uint32:2'],
        error='org.freedesktop.DBus.Error.PropertyReadOnly')
    for method, arguments, error in [
            ('Get', ['string:' + IMPL, 'string:missing'], 'UnknownProperty'),
            ('Get', ['string:org.example', 'string:version'],
                'UnknownInterface'),
            ('Get', ['string:' + IMPL], 'InvalidArgs'),
            ('GetAll', ['string:' + IMPL, 'string:extra'], 'InvalidArgs')]:
        probe.call(NAME, properties + method, arguments,
            error='org.freedesktop.DBus.Error.' + error)
    result = subprocess.run(['gdbus', 'introspect', '--session', '--xml',
        '--dest', NAME, '--object-path', PATH], env=probe.env,
        capture_output=True, text=True, timeout=5)
    assert result.returncode == 0, result.stderr
    interface = ET.fromstring(result.stdout).find(
        "interface[@name='%s']" % IMPL)
    assert interface is not None
    signatures = {method.attrib['name']: (
        ''.join(arg.attrib['type'] for arg in method
            if arg.attrib.get('direction') == 'in'),
        ''.join(arg.attrib['type'] for arg in method
            if arg.attrib.get('direction') == 'out'))
        for method in interface.findall('method')}
    assert signatures == {'ReadAll': ('as', 'a{sa{sv}}'),
        'Read': ('ss', 'v')}, signatures
    changed = interface.find("signal[@name='SettingChanged']")
    assert [arg.attrib['type'] for arg in changed] == ['s', 's', 'v']
    version = interface.find("property[@name='version']")
    assert version.attrib['type'] == 'u' and version.attrib['access'] == 'read'
    probe.evidence['cases'][
        'backend_methods_signatures_filters_properties'] = 'passed'


def source_cases(probe, observer, args, out, evidence):
    monitor = probe.monitor()
    first = observer.claim()
    publish(observer, payload(0, 5))
    probe.wait_value(1, source=dict(kind='injected-XSETTINGS',
        owner=first, serial=5))
    probe.wait_signals(monitor, [1])
    for raw in (payload(0, 5), payload(0, 6), payload(0, 7, big=True)):
        publish(observer, raw)
        probe.wait_value(1)
    select.select([], [], [], .4)
    assert probe.signals(monitor) == [1], (
        'same-value serial/byte-order signal churn')
    publish(observer, payload(1, 8, big=True))
    probe.wait_value(0)
    probe.wait_signals(monitor, [1, 0])
    good = payload(0, 9, extra=False)
    # Full malformed data must clear an earlier valid value, then recover.
    malformed = [b'', good[:11], good[:-1], good + b'\0',
        good[:8] + struct.pack('<I', 257) + good[12:],
        good[:12] + bytes([3]) + good[13:], payload(2, 10), payload(None, 11),
        good[:8] + struct.pack('<I', 2) + good[12:] + good[12:]]
    for index, raw in enumerate(malformed):
        publish(observer, good)
        probe.wait_value(1)
        publish(observer, raw)
        probe.wait_value(0, source=dict(kind='injected-malformed', index=index,
            owner=first, sha256=hashlib.sha256(raw).hexdigest()))
    publish(observer, good)
    probe.wait_value(1)
    publish(observer, good, property_type='STRING')
    probe.wait_value(0)
    publish(observer, good)
    probe.wait_value(1)
    publish(observer, b'x' * 65540)
    probe.wait_value(0)
    publish(observer, good)
    probe.wait_value(1)
    publish(observer, good, form=16)
    probe.wait_value(0)
    publish(observer, good)
    probe.wait_value(1)
    replacement_monitor = probe.monitor()
    # Replace authority while keeping the old owner window and property alive.
    observer.contender = None
    second = observer.claim()
    publish(observer, payload(0, 0))
    probe.wait_value(1, source=dict(kind='injected-replacement',
        owner=second, serial=0))
    publish(observer, payload(1, 100), window=first, require_owner=False)
    select.select([], [], [], .4)
    assert probe.signals(replacement_monitor) == [], (
        'same-value replacement/old-owner churn')
    probe.wait_value(1)
    publish(observer, payload(1, 100), window=second)
    probe.wait_value(0)
    probe.wait_signals(replacement_monitor, [0])
    observer.x.XDestroyWindow(observer.d, first)
    observer.x.XSync(observer.d, 0)
    publish(observer, payload(0, 1), window=second)
    probe.wait_value(1)
    observer.x.XSetSelectionOwner(observer.d, observer.selection, 0, 0)
    observer.x.XSync(observer.d, 0)
    probe.wait_value(0, source=dict(kind='injected-selection-loss', owner=0))
    observer.release()
    observer.claim()
    publish(observer, good)
    probe.wait_value(1)
    observer.release()
    probe.wait_value(0, source=dict(kind='injected-owner-destruction', owner=0))
    peer = PipeChild([args.publisher], probe.env, out,
        'portal-native-publisher', evidence)
    try:
        peer.send(apply_line())
        result = helper_result(peer)
        assert result['readiness'] == 'ready', result
        snapshot = wait_snapshot(observer, SETTINGS)
        probe.wait_value(1, source=dict(
            kind='actual-native-pipe-publisher-not-CFG',
            owner=snapshot['owner'], serial=snapshot['serial'],
            child=peer.receipt['identity']))
        peer.send(apply_line(sequence=2,
            values={**SETTINGS, 'Gtk/EnableAnimations': 1}))
        assert helper_result(peer)['readiness'] == 'ready'
        probe.wait_value(0)
        peer.eof(expected=0)
        assert helper_result(peer)['readiness'] == 'off'
        assert observer.owner() == 0
    finally:
        peer.close()
    evidence['cases'][
        'injected_source_and_real_native_pipe_publication'] = dict(
        status='passed', malformed_samples=len(malformed),
        byte_orders=['little', 'big'],
        cfg_provenance=False, pure_rollover='separate lead-owned policy gate')


def frontend_cases(probe, observer):
    # Initialization consumes the installed descriptor and actually activates
    # its installed service Exec on the private bus.
    probe.start_frontend()
    activated = probe.wait_owner(NAME)
    assert activated['executable_sha256'] == digest(probe.args.helper)
    probe.evidence['portal']['activated_backend'] = activated
    monitor = probe.monitor(public=True)
    owner = observer.claim()
    publish(observer, payload(0))
    probe.wait_value(1, public=True,
        source=dict(kind='injected-XSETTINGS', owner=owner))
    assert probe.read(public=True, legacy=True) == 1
    probe.read_all([], value=1, public=True)
    probe.read_all(['org.example.*'], public=True, empty=True)
    probe.wait_signals(monitor, [1])
    publish(observer, payload(1, 2))
    probe.wait_value(0, public=True)
    probe.wait_signals(monitor, [1, 0])
    observer.release()
    # Retire only the exact activated executable identified above. dbus-daemon
    # owns its Wait; this controller cannot claim a direct-child Wait.
    identity = activated['identity']
    assert process_identity(activated['pid']) == identity
    os.kill(activated['pid'], signal.SIGTERM)
    wait_for_predicate('exact activated backend retired', lambda:
        process_identity(activated['pid']) != identity, time.monotonic() + 6)
    probe.evidence['portal']['activation_retirement'] = dict(identity=identity,
        wait_owner=('private dbus-daemon; kernel retirement observed, '
            'Wait not observed'))
    replacement = probe.start_backend()
    # The existing public owner must recover independently of frontend restart.
    frontend_owner = probe.owner(PUBLIC)
    probe.wait_value(0, public=True)
    observer.claim()
    publish(observer, payload(0, 3))
    probe.wait_value(1, public=True)
    assert probe.owner(PUBLIC) == frontend_owner
    observer.release()
    probe.wait_value(0, public=True)
    probe.evidence['cases']['backend_restart_existing_frontend'] = 'passed'
    probe.frontend.close(time.monotonic() + 5)
    probe.start_frontend()
    probe.wait_value(0, public=True)
    observer.claim()
    publish(observer, payload(0, 3))
    probe.wait_value(1, public=True)
    assert probe.read(public=True, legacy=True) == 1
    probe.read_all([NS], value=1, public=True)
    observer.release()
    probe.wait_value(0, public=True)
    probe.evidence['cases'][
        'actual_installed_frontend_activation_restarts'] = dict(
        status='passed', replacement_backend=replacement,
        ReadOne='single variant uint32', Read='double variant uint32',
        application_effects='not run; frontend protocol observations only')


def pressure_cases(probe, observer):
    monitor = probe.monitor()
    child = monitor[0]
    owner = observer.claim()
    pid = probe.backend.process.pid
    def rss():
        if sys.platform == 'linux':
            status = Path('/proc/%d/status' % pid).read_text()
            return int(re.search(r'^VmRSS:\s+(\d+) kB$', status, re.M)[1])
        assert sys.platform.startswith(('freebsd', 'openbsd'))
        assert probe.backend.process.poll() is None
        argv = [shutil.which('ps'), '-o', 'rss=', '-p', str(pid)]
        assert argv[0] is not None, 'BSD ps is unavailable'
        result = subprocess.run(argv, capture_output=True, text=True, timeout=2)
        assert probe.backend.process.poll() is None
        assert result.returncode == 0, (argv, result.stderr)
        assert len(result.stdout) <= 1024 and len(result.stderr) <= 4096
        assert re.fullmatch(r'\s*[0-9]+\s*', result.stdout), result.stdout
        value = int(result.stdout)
        assert value > 0
        probe.evidence['portal'].setdefault('rss_observations', []).append(dict(
            argv=argv, returncode=result.returncode, stdout=result.stdout,
            stderr=result.stderr, rss_kib=value, timeout_seconds=2,
            ownership='backend Popen alive before and after bounded ps'))
        return value
    before = rss()
    os.kill(child.process.pid, signal.SIGSTOP)
    try:
        for index in range(128):
            publish(observer, payload(index % 2, index + 1))
            probe.wait_value(1 - index % 2)
        after = rss()
        assert after - before < 16384, (before, after)
        probe.read_all([], value=0)
    finally:
        os.kill(child.process.pid, signal.SIGCONT)
        observer.release()
    probe.wait_value(0)
    probe.evidence['cases']['bounded_slow_monitor_pressure'] = dict(
        status='passed',
        owner=owner, updates=128, rss_before_kib=before, rss_after_kib=after,
        limit='finite stopped-reader observation; not every bus queue limit')


def lifecycle_cases(probe, args):
    competing = probe.start('portal-competing', [args.helper])
    assert competing.wait_until_exit(time.monotonic() + 5) != 0
    probe.wait_owner(NAME, probe.backend)
    arguments = probe.start('portal-invalid-arguments', [args.helper, '--help'])
    assert arguments.wait_until_exit(time.monotonic() + 5) != 0
    # No incumbent name may mask an invalid DISPLAY/address refusal.
    probe.backend.close(time.monotonic() + 5)
    for index, changes in enumerate(({'DISPLAY': ''},
            {'DBUS_SESSION_BUS_ADDRESS': ''},
            {'DISPLAY': ':65530'},
            {'DBUS_SESSION_BUS_ADDRESS':
                'unix:path=/nonexistent/fvwm-portal-bus'})):
        env = {**probe.env, **changes}
        child = probe.start('portal-missing-env-%d' % index, [args.helper], env)
        assert child.wait_until_exit(time.monotonic() + 5) != 0
        probe.call(BUS, BUS + '.GetNameOwner', ['string:' + NAME],
            path=BUS_PATH,
            error='org.freedesktop.DBus.Error.NameHasNoOwner')
    if args.disabled_helper:
        child = probe.start('portal-disabled', [args.disabled_helper])
        assert child.wait_until_exit(time.monotonic() + 5) != 0
        probe.call(BUS, BUS + '.GetNameOwner', ['string:' + NAME],
            path=BUS_PATH,
            error='org.freedesktop.DBus.Error.NameHasNoOwner')
        probe.evidence['cases']['disabled_stub'] = 'passed'
    else:
        probe.evidence['cases']['disabled_stub'] = (
            'skipped; no --disabled-helper')
    probe.start_backend()
    probe.bus.close(time.monotonic() + 5)
    assert probe.backend.wait_until_exit(time.monotonic() + 5) != 0
    probe.evidence['cases'][
        'name_conflict_missing_environment_bus_loss'] = 'passed'


def run(args):
    assert sys.platform == 'linux' or (
        sys.platform.startswith(('freebsd', 'openbsd')) and args.groups == ['direct'])
    out = args.artifacts_dir
    out.mkdir(parents=True, exist_ok=False)
    source = Path(__file__).resolve().parents[2]
    evidence = dict(status='running', execution_profile=args.execution_profile,
        platform=sys.platform, observed_os=dict(zip(
            ('sysname', 'nodename', 'release', 'version', 'machine'), os.uname())),
        groups=args.groups, cases={}, cleanup_failures=[],
        group_results={g: 'pending' if g in args.groups else (
            'skipped by --groups')
            for g in ('direct', 'frontend')},
        limits=['No application UI/effects, hardware or native CPU inference.',
            'Rollover, sanitizer and six-target gates: separate lead work.',
            'Unrelated portal preservation needs a supplied working provider.',
            'Injected source and raw native pipe are not CFG evidence.'])
    files = ['tests/live_x/settings_portal_live.py',
        'tests/live_x/native_settings_live.py',
        'tests/live_x/live_x_harness.py', 'tests/live_x/session_x_errors.py',
        'bin/settings_portal.c', 'bin/settings_portal_policy.c',
        'bin/settings_portal_policy.h',
        'bin/native_settings.c', 'bin/native_settings_policy.c',
        'bin/meson.build']
    evidence['source_sha256'] = {p: digest(source / p) for p in files}
    names = ['helper', 'publisher']
    if 'frontend' in args.groups:
        names.append('frontend')
    if args.disabled_helper:
        names.append('disabled_helper')
    evidence['binary_inputs'] = {n: dict(path=str(getattr(args, n)),
        sha256=digest(getattr(args, n))) for n in names}
    evidence['external_tools'] = {n: dict(path=shutil.which(n),
        sha256=digest(shutil.which(n)))
        for n in ('dbus-daemon', 'dbus-send', 'dbus-monitor', 'gdbus', 'Xvfb')}
    if sys.platform.startswith(('freebsd', 'openbsd')):
        evidence['external_tools']['ps'] = dict(path=shutil.which('ps'),
            sha256=digest(shutil.which('ps')))
    runtime = HarnessRuntime(prefix='fvwm-settings-portal-')
    observer = probe = None
    try:
        server, display = start_display_server(runtime, 'Xvfb',
            ['Xvfb', '-screen', '0', '800x600x24', '-noreset'], 'xserver.log',
            time.monotonic() + 10)
        env = runtime.environment({'DISPLAY': display})
        observer = XObserver(display, evidence)
        if 'direct' in args.groups:
            probe = PortalProbe(args, runtime, env, out, evidence)
            probe.start_backend()
            protocol_cases(probe)
            source_cases(probe, observer, args, out, evidence)
            pressure_cases(probe, observer)
            lifecycle_cases(probe, args)
            evidence['direct_portal'] = evidence.pop('portal')
            probe.close()
            evidence['group_results']['direct'] = 'passed'
        if 'frontend' in args.groups:
            probe = PortalProbe(args, runtime, env, out, evidence,
                frontend=True)
            frontend_cases(probe, observer)
            evidence['group_results']['frontend'] = 'passed'
            probe.close()
            evidence['frontend_portal'] = evidence.pop('portal')
        # X death has its own connection. Close the Python observer first since
        # Xlib's process-global fatal I/O handler cannot be caught safely.
        observer.close()
        observer = None
        if 'direct' in args.groups:
            death_args = argparse.Namespace(**vars(args))
            probe = PortalProbe(death_args, runtime, env, out, evidence)
            probe.start_backend()
            server.close(time.monotonic() + 5)
            assert probe.backend.wait_until_exit(time.monotonic() + 5) != 0
            evidence['cases']['actual_X_connection_loss_retires'] = 'passed'
        assert evidence['source_sha256'] == {
            path: digest(source / path) for path in files}, (
                'source changed during run')
        assert all(digest(item['path']) == item['sha256'] for item in
            evidence['binary_inputs'].values()), 'binary changed during run'
        assert not evidence.get('observer_unexpected_x_errors')
        evidence['status'] = 'passed'
    except BaseException as failure:
        evidence.update(status='failed', failure=dict(
            type=type(failure).__name__,
            message=str(failure)[:2048]))
        raise
    finally:
        def clean(label, action):
            try:
                action()
            except BaseException as failure:
                evidence['status'] = 'failed'
                evidence['cleanup_failures'].append(dict(
                    step=label, error=repr(failure)))
        if probe:
            clean('portal children', probe.close)
        if observer:
            clean('X observer', observer.close)
        for child in reversed(runtime.processes):
            clean(child.name, lambda p=child: p.close(time.monotonic() + 5))
        evidence['owned_processes'] = [dict(name=p.name, argv=p.argv,
            pid=p.process.pid,
            identity=getattr(p, 'portal_identity', None),
            wait=dict(returncode=p.process.returncode), log=p.log_path.name)
            for p in runtime.processes]
        for path in runtime.root.glob('*.log'):
            clean('copy ' + path.name,
                lambda p=path: shutil.copy2(p, out / p.name))
        clean('runtime', runtime.close)
        atomic_json(out / 'result.json', evidence)
    assert evidence['status'] == 'passed', evidence['cleanup_failures']
    print(json.dumps(dict(status='passed', artifacts=str(out))))


def portal_arguments(parser, prefix=''):
    """Shared opt-in arguments; native fixture keeps its existing --helper."""
    parser.add_argument('--' + prefix + 'helper', type=Path)
    parser.add_argument('--frontend', type=Path)
    parser.add_argument('--portal-env', type=Path)
    parser.add_argument('--portal-descriptor', type=Path)
    parser.add_argument('--activation-service', type=Path)


def start_cfg_probe(args, runtime, env, out, evidence):
    """Observe only the existing real Desktop run; never inject a CFG source."""
    if not args.portal_helper:
        return None
    options = argparse.Namespace(**{**vars(args), 'helper': args.portal_helper})
    probe = PortalProbe(options, runtime, env, out, evidence, frontend=True)
    probe.start_frontend()
    backend = probe.wait_owner(NAME)
    assert backend['executable_sha256'] == digest(options.helper)
    evidence['portal']['activated_backend'] = backend
    evidence['portal']['source_provenance'] = (
        'actual Desktop CFG through direct native publisher')
    evidence['portal']['binary_inputs'] = {name: dict(
        path=str(getattr(options, name)), sha256=digest(getattr(options, name)))
        for name in ('helper', 'frontend')}
    evidence['portal']['external_tools'] = {name: dict(path=shutil.which(name),
        sha256=digest(shutil.which(name))) for name in (
            'dbus-daemon', 'dbus-send', 'dbus-monitor', 'gdbus')}
    return probe


def validate_portal_args(parser, args, frontend=False):
    for name in ('helper', 'frontend', 'portal_descriptor',
            'activation_service', 'portal_env'):
        path = getattr(args, name, None)
        required = name == 'helper' or frontend and name in (
            'frontend', 'portal_descriptor', 'activation_service')
        if required and path is None:
            parser.error('--' + name.replace('_', '-') +
                ' is required for this group')
        if path:
            path = path.resolve()
            if not path.is_file() or (name in ('helper', 'frontend') and
                    not os.access(path, os.X_OK)):
                parser.error('unavailable input ' + name + ': ' + str(path))
            setattr(args, name, path)
    for tool in ('dbus-daemon', 'dbus-send', 'dbus-monitor', 'gdbus'):
        if not shutil.which(tool):
            parser.error('unavailable required installed tool ' + tool)


def main():
    parser = argparse.ArgumentParser(description=__doc__, allow_abbrev=False)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--artifacts-dir', type=Path, required=True)
    parser.add_argument('--execution-profile',
        choices=('native-linux', 'emulated-linux', 'native-cpu', 'emulated-cpu'),
        required=True)
    parser.add_argument('--groups', nargs='+', choices=('direct', 'frontend'),
        default=['direct', 'frontend'])
    parser.add_argument('--publisher', type=Path)
    parser.add_argument('--disabled-helper', type=Path)
    portal_arguments(parser)
    args = parser.parse_args()
    if sys.platform.startswith(('freebsd', 'openbsd')):
        if args.groups != ['direct']:
            parser.error('BSD supports only --groups direct; frontend requires Linux')
        if args.execution_profile not in ('native-cpu', 'emulated-cpu'):
            parser.error('BSD requires --execution-profile native-cpu or emulated-cpu')
        if not shutil.which('ps'):
            parser.error('unavailable required BSD ps for backend RSS observations')
    elif sys.platform != 'linux':
        parser.error('requires a lead-owned Linux, FreeBSD or OpenBSD executor')
    if not shutil.which('Xvfb'):
        parser.error('unavailable required actual Xvfb server')
    if len(args.groups) != len(set(args.groups)):
        parser.error('group names must be unique')
    args.build = args.build.resolve()
    args.artifacts_dir = args.artifacts_dir.resolve()
    args.helper = args.helper or args.build / 'bin/fvwm-settings-portal'
    args.publisher = (args.publisher or
        args.build / 'bin/fvwm-native-settings').resolve()
    for name in ('publisher', 'disabled_helper'):
        path = getattr(args, name)
        if path and (not path.is_file() or not os.access(path, os.X_OK)):
            parser.error('unavailable executable ' + name + ': ' + str(path))
    validate_portal_args(parser, args, frontend='frontend' in args.groups)
    run(args)
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
