#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the GNU General Public License, version 2 or any later version.
"""Observe the standalone tray with independent persistent native X clients.

Use only a caller-owned disposable display or the fixture's private server.
The helper owns foreign windows; their actual parents/maps and received XEmbed
messages are independent of module Status. This is a focused subset, not full
XEmbed, toolkit, compositor, hardware or six-target qualification.
"""
import argparse
import ctypes as C
import hashlib
import json
import os
from pathlib import Path
import select
import shutil
import signal
import socket
import subprocess
import time
import traceback

from desktop_live import connect_client
from geometry_transition_live import Pointer
from live_x_harness import (HarnessRuntime, start_display_server,
    start_xorg_dummy, wait_for_randr, run_required_xrandr,
    wait_for_authoritative_state, wait_for_predicate)
from mfl_request_live import frame, reply
from mfl_stream_live import Connection
from session_services_live import Session
from session_x_errors import SessionXErrors


PROFILES = {
    'slate-dark': ('#e5edf5', '#253140', 'xft:Sans:size=11', 2),
    'dune-light': ('#322a20', '#eee4cb', 'xft:Serif:size=12', 1),
    'classic-large': ('#ffffe0', '#252550', 'xft:DejaVu Sans Mono:size=18', 5),
}
STATUS_FIELDS = ('count', 'mapped', 'limit', 'quarters', 'claimed', 'pending',
    'epoch', 'host', 'owner', 'proxy', 'font_height')


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


class Peer:
    def __init__(self, executable, environment, out, name):
        self.log = (out / (name + '.stderr')).open('wb')
        self.process = None
        self.pending = b''
        self.records, self.events = [], []
        try:
            self.process = subprocess.Popen([str(executable)],
                stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=self.log,
                env=environment, start_new_session=True)
            self.identity = self.request('identity')
        except BaseException:
            self.close()
            raise

    def request(self, verb, **values):
        command = dict(verb=verb, **values)
        wire = (json.dumps(command) + '\n').encode()
        assert len(wire) <= 8192 and len(self.records) < 4096
        self.process.stdin.write(wire)
        self.process.stdin.flush()
        deadline = time.monotonic() + 5
        while b'\n' not in self.pending:
            assert time.monotonic() < deadline, 'tray peer reply deadline'
            assert self.process.poll() is None, self.process.returncode
            ready = select.select([self.process.stdout], [], [],
                max(0, deadline - time.monotonic()))[0]
            assert ready, 'tray peer reply timed out'
            data = os.read(self.process.stdout.fileno(), 65536)
            assert data, 'tray peer disconnected'
            self.pending += data
            assert len(self.pending) <= 262144
        line, self.pending = self.pending.split(b'\n', 1)
        result = json.loads(line)
        self.records.append({'request': command, 'reply': result})
        assert 'error' not in result, result
        assert result.get('unexpected_x_errors', 0) == 0, result
        self.events.extend(result.get('events', []))
        assert len(self.events) <= 8192
        return result

    def create(self, **fields):
        return self.request('create', **fields)['id']

    def state(self, identifier):
        return self.request('state', id=identifier)

    def message(self, identifier, opcode, **fields):
        def observed():
            self.request('events')
            for index, event in enumerate(self.events):
                if (event.get('kind') == 'xembed' and
                        event['window'] == identifier and
                        event['opcode'] == opcode and all(
                            event.get(k) == v for k, v in fields.items())):
                    return self.events.pop(index)
            return None
        return wait_for_predicate('actual XEmbed message ' + str(opcode),
            observed, time.monotonic() + 5)

    def key(self, identifier, keysym):
        def observed():
            self.request('events')
            for index, event in enumerate(self.events):
                if (event.get('kind') == 'key' and event['type'] == 2 and
                        event['window'] == identifier and
                        event['keysym'] == keysym):
                    assert event['synthetic'], event
                    return self.events.pop(index)
            return None
        return wait_for_predicate('actual forwarded key', observed,
            time.monotonic() + 5)

    def close(self):
        forced = False
        if self.process:
            try:
                if self.process.poll() is None:
                    self.process.stdin.close()
                    try:
                        self.process.wait(timeout=3)
                    except subprocess.TimeoutExpired:
                        forced = True
                        self.process.terminate()
                        try:
                            self.process.wait(timeout=2)
                        except subprocess.TimeoutExpired:
                            self.process.kill()
                            self.process.wait(timeout=2)
            finally:
                self.process.stdout.close()
        self.log.close()
        return dict(exit=self.process.returncode if self.process else None,
            forced_cleanup=forced)


class Native(Session):
    """Reuse independent X/capture methods without session or D-Bus startup."""
    def __init__(self, runtime, options, out, evidence):
        self.runtime, self.options, self.out = runtime, options, out
        self.evidence = evidence
        self.connections, self.probes, self.peers, self.handles = [], [], [], []
        self.client = self.errors = None
        self.dock = None
        self.serial = 0
        self.home = runtime.root / 'standalone-tray'
        self.home.mkdir(mode=0o700)
        self.env = runtime.environment({'DISPLAY': options.display,
            'HOME': str(self.home), 'FVWM_USERDIR': str(self.home),
            'FVWMMFL_SOCKET_PATH': str(self.home)})
        for name in ('DBUS_SESSION_BUS_ADDRESS', 'SESSION_MANAGER',
                'FVWM_SESSION_CONTROL_SOCKET', 'FVWM_SESSION_READY_FD'):
            self.env.pop(name, None)
        modules = self.home / 'modules'
        modules.mkdir()
        for name, executable in [('FvwmMFL', options.mfl),
                ('FvwmTray', options.tray)]:
            (modules / name).symlink_to(executable)
        source = Path(__file__).resolve().parents[2]
        self.config = self.home / 'base.fvwm'
        self.config.write_text('Read ' + str(source / 'fvwm/ConfigFvwmDefaults') +
            '\nModulePath ' + str(modules) +
            '\nStyle * TileCascadePlacement\n'
            'Style "FVWM transition probe" ClickToFocus\n'
            'DesktopConfiguration per-monitor\n'
            'Colorset 42 fg #e5edf5, bg #253140\n'
            '*FvwmTray Colorset 42\n'
            'AddToFunc StartFunction I Module FvwmMFL\n')
        shutil.copyfile(self.config, out / 'base.fvwm')
        try:
            self.process = runtime.start('tray-core', [str(options.fvwm), '-d',
                options.display, '-f', str(self.config)], 'tray-core.log', self.env)
            self.endpoint = self.home / ('fvwm_mfl_' +
                options.display.replace(':', '_') + '.sock')
            wait_for_predicate('tray MFL endpoint', self.endpoint.exists,
                time.monotonic() + 10, process=self.process)
            self.control = Connection(self.endpoint)
            self.connections.append(self.control)
            assert 'connection_profile' in self.control.record()
            self.control.send(b'set echo\n')
            self.control.echo('tray-core-ready')
            self.rpc = Connection(self.endpoint)
            self.connections.append(self.rpc)
            assert 'connection_profile' in self.rpc.record()
            self.client = connect_client(options.display)
            self.errors = SessionXErrors(self.client, evidence).install()
            self.x, self.d = self.client.x, self.client.display
            self.configure_xlib()
            self.x.XGetWindowProperty.argtypes = [C.c_void_p, C.c_ulong,
                C.c_ulong, C.c_long, C.c_long, C.c_int, C.c_ulong,
                C.POINTER(C.c_ulong), C.POINTER(C.c_int), C.POINTER(C.c_ulong),
                C.POINTER(C.c_ulong), C.POINTER(C.POINTER(C.c_ubyte))]
            self.x.XGetWindowProperty.restype = C.c_int
            self.x.XChangeProperty.argtypes = [C.c_void_p, C.c_ulong, C.c_ulong,
                C.c_ulong, C.c_int, C.c_int, C.c_void_p, C.c_int]
            self.x.XChangeProperty.restype = C.c_int
            self.pointer = Pointer(self.client)
            self.pointer.x.XTestFakeKeyEvent.argtypes = [C.c_void_p, C.c_uint,
                C.c_int, C.c_ulong]
            self.pointer.x.XTestFakeKeyEvent.restype = C.c_int
            self.client.create()
            wait_for_predicate('managed focus probe frame', self.client.frame,
                time.monotonic() + 5, process=self.process)
            self.app = self.client.window
            self.focus_app()
        except BaseException:
            self.close()
            raise

    def peer(self, name):
        peer = Peer(self.options.client, self.env, self.out, name)
        self.peers.append((name, peer))
        return peer

    def command(self, text):
        assert len(text.encode()) < 1000 and '\0' not in text
        self.control.send((text + '\n').encode())
        self.serial += 1
        self.control.echo('tray-core-barrier-' + str(self.serial))

    def wait_status(self, predicate=lambda value: True):
        deadline = time.monotonic() + 8
        previous = self.control.socket.gettimeout()
        records = 0
        try:
            for _ in range(32):
                if time.monotonic() >= deadline or records >= 256:
                    break
                attempt_deadline = min(deadline, time.monotonic() + .25)
                self.control.socket.settimeout(max(.001,
                    attempt_deadline - time.monotonic()))
                self.control.send(b'SendToModule FvwmTray Status\n')
                while time.monotonic() < attempt_deadline and records < 256:
                    if b'\n' not in self.control.pending:
                        self.control.socket.settimeout(max(.001,
                            attempt_deadline - time.monotonic()))
                        try:
                            data = self.control.socket.recv(65536)
                        except (TimeoutError, socket.timeout):
                            break
                        assert data, 'tray control disconnected'
                        self.control.pending += data
                        assert len(self.control.pending) <= 65536
                        continue
                    line, self.control.pending = self.control.pending.split(
                        b'\n', 1)
                    records += 1
                    record = json.loads(line)
                    message = record.get('echo', {}).get('message', '')
                    if not message.startswith('FVWM_TRAY_STATUS '):
                        continue
                    words = message.split()
                    assert len(words) == len(STATUS_FIELDS) + 1, message
                    state = dict(zip(STATUS_FIELDS, map(int, words[1:])))
                    if predicate(state):
                        return state
            raise AssertionError('tray Status deadline/record limit/predicate')
        finally:
            self.control.socket.settimeout(previous)

    def status(self):
        return self.wait_status()

    def start(self, peer):
        self.command('Module FvwmTray')
        owner = wait_for_predicate('actual tray selection owner',
            lambda: peer.request('owner')['owner'], time.monotonic() + 8,
            process=self.process)
        state = self.wait_status(lambda s: s['owner'] == owner and
            s['claimed'] == 1 and s['pending'] == 0 and s['epoch'] > 0)
        assert self.property(owner, '_FVWM_TRAY_HOST', 'WINDOW') == [state['host']]
        assert self.attrs(state['host']).override_redirect
        self.state = state
        self.capture_module_handle(owner)
        return state

    def capture_module_handle(self, owner):
        if not Path('/proc').is_dir() or not hasattr(os, 'pidfd_open'):
            return None
        from session_bus_lifetime_live import OwnedProcess
        values = self.property(owner, '_NET_WM_PID', 'CARDINAL')
        assert len(values) == 1 and values[0] > 0, values
        # The property is only a locator. PPID/birth/executable/pidfd still
        # authenticate the exact fixture core child before any signal.
        handle = OwnedProcess(values[0], self.process.process.pid,
            self.options.tray)
        self.handles.append(handle)
        self.module = handle
        return handle

    def property(self, window, name, expected_type=None):
        actual, count, remaining = C.c_ulong(), C.c_ulong(), C.c_ulong()
        format_ = C.c_int()
        data = C.POINTER(C.c_ubyte)()
        atom = self.x.XInternAtom(self.d, name.encode(), 0)
        try:
            result = self.errors.read(window, 20, lambda:
                self.x.XGetWindowProperty(self.d, window, atom, 0, 128,
                    0, 0, C.byref(actual), C.byref(format_), C.byref(count),
                    C.byref(remaining), C.byref(data)))
            if result == 0 and data and format_.value == 32:
                assert remaining.value == 0 and count.value <= 128
                if expected_type:
                    assert actual.value == self.x.XInternAtom(self.d,
                        expected_type.encode(), 0)
                return list(C.cast(data, C.POINTER(C.c_ulong))[:count.value])
            return []
        finally:
            if data:
                self.x.XFree(data)

    def refusals(self):
        values = self.property(self.state['owner'], '_FVWM_TRAY_REFUSALS',
            'CARDINAL')
        assert len(values) == 1
        return values[0]

    def rejected_after(self, before, count=1):
        return wait_for_predicate('processed tray refusal',
            lambda: (lambda value: value if value >= before + count else None)(
                self.refusals()), time.monotonic() + 5, process=self.process)

    def focus(self):
        current, revert = C.c_ulong(), C.c_int()
        self.x.XGetInputFocus(self.d, C.byref(current), C.byref(revert))
        return current.value

    def focus_app(self):
        self.command('Silent WindowId 0x%x Focus' % self.app)
        wait_for_predicate('probe application focus',
            lambda: self.focus() == self.app, time.monotonic() + 5,
            process=self.process)

    def press(self, symbol, shift=False):
        code = self.x.XKeysymToKeycode(self.d,
            self.x.XStringToKeysym(symbol.encode()))
        modifier = self.x.XKeysymToKeycode(self.d,
            self.x.XStringToKeysym(b'Shift_L')) if shift else 0
        assert code and (not shift or modifier)
        if modifier:
            assert self.pointer.x.XTestFakeKeyEvent(self.d, modifier, 1, 0)
        try:
            for pressed in (1, 0):
                assert self.pointer.x.XTestFakeKeyEvent(self.d, code, pressed, 0)
        finally:
            if modifier:
                assert self.pointer.x.XTestFakeKeyEvent(self.d, modifier, 0, 0)
            self.x.XSync(self.d, 0)

    def query(self, resource):
        self.serial += 1
        token = 'tray-query-' + str(self.serial)
        self.rpc.send(frame(token, resource=resource))
        return reply(self.rpc, token)

    def rect(self, window):
        attributes = self.attrs(window)
        if not attributes or attributes.map_state != 2:
            return None
        x, y, child = C.c_int(), C.c_int(), C.c_ulong()
        assert self.x.XTranslateCoordinates(self.d, window, self.client.root,
            0, 0, C.byref(x), C.byref(y), C.byref(child))
        return dict(x=x.value, y=y.value, width=attributes.width,
            height=attributes.height, border=attributes.border_width)

    def settled(self, primary, quarters, after=None, minimum_top=None):
        last = {}
        def observed():
            topology = self.query('topology')['topology']
            workspace = self.query('workspaces')
            monitors = {m['name']: m for m in topology['monitors']}
            areas = {m['name']: m for m in workspace['monitors']}
            state = self.status()
            rect = self.rect(state['host'])
            sample = dict(topology=topology, workspace=workspace,
                native=state, host_rect=rect)
            last.clear()
            last.update(sample)
            if primary not in monitors or primary not in areas:
                return None
            epoch = int(topology['epoch'], 16)
            area = areas[primary]['work_area']
            if (not monitors[primary]['primary'] or state['pending'] or
                    state['epoch'] != epoch or state['quarters'] != quarters or
                    (after is not None and epoch <= after) or not area or
                    not rect or (minimum_top is not None and
                        area['y'] < minimum_top)):
                return None
            if not (area['x'] <= rect['x'] and area['y'] <= rect['y'] and
                    rect['x'] + rect['width'] <= area['x'] + area['width'] and
                    rect['y'] + rect['height'] <= area['y'] + area['height']):
                return None
            return sample
        try:
            return wait_for_predicate('committed tray topology ' + primary,
                observed, time.monotonic() + 8, process=self.process)
        finally:
            self.evidence.setdefault('last_topology_observations', {})[
                primary + '-' + str(quarters)] = last

    def reserve_top(self, height):
        self.command('Style TrayFixtureDock Sticky, NoTitle, BorderWidth 0, '
            'HandleWidth 0, EWMHUseStrutHints')
        self.dock = self.x.XCreateSimpleWindow(self.d, self.client.root, 0, 0,
            3200, height, 0, 0, 0x445566)
        self.x.XStoreName(self.d, self.dock, b'TrayFixtureDock')
        cardinal = self.x.XInternAtom(self.d, b'CARDINAL', 0)
        atom_type = self.x.XInternAtom(self.d, b'ATOM', 0)
        for name, values, type_ in [('_NET_WM_WINDOW_TYPE',
                [self.x.XInternAtom(self.d, b'_NET_WM_WINDOW_TYPE_DOCK', 0)], atom_type),
                ('_NET_WM_DESKTOP', [0xffffffff], cardinal),
                ('_NET_WM_STRUT', [0, 0, height, 0], cardinal),
                ('_NET_WM_STRUT_PARTIAL',
                    [0, 0, height, 0, 0, 0, 0, 0, 0, 3199, 0, 0], cardinal)]:
            data = (C.c_ulong * len(values))(*values)
            self.x.XChangeProperty(self.d, self.dock,
                self.x.XInternAtom(self.d, name.encode(), 0), type_, 32, 0,
                C.cast(data, C.c_void_p), len(values))
        self.x.XMapWindow(self.d, self.dock)
        self.x.XSync(self.d, 0)

    def close(self):
        for name, peer in self.peers:
            try:
                outcome = peer.close()
            except BaseException as error:
                outcome = {'error': repr(error)}
            self.evidence.setdefault('peer_cleanup', {})[name] = outcome
            if outcome.get('exit') != 0 or outcome.get('forced_cleanup'):
                self.evidence['status'] = 'failed'
        for handle in self.handles:
            handle.close()
        for connection in self.connections:
            connection.close()
        if self.dock and self.client:
            self.x.XDestroyWindow(self.d, self.dock)
            self.dock = None
        if self.errors:
            self.errors.close_display()
        elif self.client:
            self.client.close()


def competing_start(native, peer):
    """Wait for one actual refused process while a controlled owner exists."""
    assert peer.request('owner')['owner'] == 0, (
        'Caller display already has a tray owner; preserve it.')
    claimed = peer.request('claim')['owner']
    assert claimed
    to_parent_read, to_parent_write = os.pipe()
    from_parent_read, from_parent_write = os.pipe()
    descriptors = {to_parent_read, to_parent_write,
        from_parent_read, from_parent_write}
    process = None
    path = native.out / 'competing-start.log'
    arguments = [str(native.options.tray), str(to_parent_write),
        str(from_parent_read), '/dev/null', '0', '0']
    try:
        with path.open('wb') as log:
            process = subprocess.Popen(arguments, env=native.env,
                stdout=log, stderr=log, start_new_session=True,
                pass_fds=(to_parent_write, from_parent_read))
            for descriptor in (to_parent_write, from_parent_read):
                os.close(descriptor)
                descriptors.remove(descriptor)
            # Keep both parent endpoints open: startup must reach actual owner
            # refusal rather than fail because a fake core pipe was closed.
            result = process.wait(timeout=10)
        assert result == 1, (result, path.read_text(errors='replace')[-2000:])
        assert peer.request('owner')['owner'] == claimed
        return dict(argv=arguments, exit=result, owner=claimed,
            log=path.name, log_sha256=digest(path),
            proof='completed process refusal; selected owner retained')
    finally:
        if process and process.poll() is None:
            process.terminate()
            try:
                process.wait(timeout=2)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait(timeout=2)
        for descriptor in descriptors:
            os.close(descriptor)
        peer.request('release')
        assert peer.request('owner')['owner'] == 0


def wait_state(native, peer, identifier, predicate, label):
    return wait_for_predicate(label,
        lambda: (lambda state: state if predicate(state) else None)(
            peer.state(identifier)), time.monotonic() + 5,
        process=native.process)


def dock(native, peer, identifier):
    peer.request('dock', id=identifier)
    state = wait_state(native, peer, identifier,
        lambda s: s['exists'] and s['parent'] != peer.identity['root'] and
            s['map'] == 2, 'actual docked foreign window')
    message = peer.message(identifier, 0, data1=state['parent'], data2=0)
    return state, message


def released(native, peer, identifier):
    return wait_state(native, peer, identifier,
        lambda s: s['exists'] and s['parent'] == peer.identity['root'] and
            s['map'] == 0, 'surviving unmapped foreign root client')


def run(options):
    runtime = HarnessRuntime(prefix='fvwm-tray-')
    out = options.artifacts_dir.resolve()
    out.mkdir(parents=True, exist_ok=True)
    native = None
    evidence = dict(status='running', cases={}, captures=[], operations=[],
        executor=dict(uname=list(os.uname()),
            execution_profile=options.execution_profile,
            cpu_execution='caller declaration, not inferred from uname'),
        limits=['Standalone default-visual XEmbed subset only; no StatusNotifier, arbitrary containment, ARGB, accelerator or XDND claim.',
            'Synthetic clients on a disposable X server; not physical devices, toolkit interoperability or full support acceptance.',
            'Abrupt-exit signaling requires Linux pidfd/executable/birth checks; other platforms report it unavailable.',
            'Event/count limits do not preempt a blocking Xlib call.',
            'Server XID reuse is accepted across observed owner absence/restart; this fixture does not force the server resource allocator.'])
    try:
        if options.xorg_config:
            assert not options.display, '--display and --xorg-config are exclusive'
            _, options.display = start_xorg_dummy(runtime,
                options.xorg_config.resolve(), time.monotonic() + 10)
            wait_for_randr(runtime, options.display, time.monotonic() + 10)
            run_required_xrandr(runtime, options.display,
                ['--addmode', 'DUMMY1', '1280x1024'], 'second-mode',
                evidence['operations'])
            run_required_xrandr(runtime, options.display,
                ['--output', 'DUMMY0', '--mode', '1920x1080', '--pos', '0x0',
                '--primary', '--set', 'WIDTH_MM', '508', '--set', 'HEIGHT_MM',
                '286', '--output', 'DUMMY1', '--mode', '1280x1024', '--pos',
                '1920x0', '--set', 'WIDTH_MM', '169', '--set', 'HEIGHT_MM',
                '135', '--fb', '3200x1080', '--fbmm', '846x286'],
                'mixed-dpi-baseline', evidence['operations'])
            evidence['baseline_randr'] = wait_for_authoritative_state(runtime,
                options.display, 'baseline', lambda s:
                    set(s['monitors']) == {'DUMMY0', 'DUMMY1'} and
                    s['monitors']['DUMMY0']['primary'],
                time.monotonic() + 8, 'baseline')
            origin = 'fixture-owned Xorg-dummy'
        elif options.display:
            origin = 'caller-owned disposable display'
        else:
            _, options.display = start_display_server(runtime, 'Xvfb',
                ['Xvfb', '-screen', '0', '1200x800x24', '-noreset'],
                'xserver.log', time.monotonic() + 10)
            origin = 'fixture-owned Xvfb'
        evidence['display'] = dict(name=options.display, ownership=origin)
        evidence['inputs'] = {name: dict(path=str(getattr(options, name)),
            sha256=digest(getattr(options, name))) for name in
            ('fvwm', 'tray', 'mfl', 'client', 'capture')}
        if options.xorg_config:
            evidence['inputs']['xorg_config'] = dict(
                path=str(options.xorg_config), sha256=digest(options.xorg_config))
        native = Native(runtime, options, out, evidence)
        first, second = native.peer('first'), native.peer('second')
        competitor = competing_start(native, first)
        initial = native.start(first)
        owner, host = initial['owner'], initial['host']
        first.request('events')
        announced = [e for e in first.events if e.get('kind') == 'manager' and
            e['owner'] == owner and e['selection'] == first.identity['selection']]
        assert announced and all(e['timestamp'] > 0 for e in announced)
        assert native.property(owner, '_NET_SYSTEM_TRAY_ORIENTATION', 'CARDINAL') == [0]
        assert len(native.property(owner, '_NET_SYSTEM_TRAY_VISUAL', 'VISUALID')) == 1
        a = first.create(width=24, height=20, name='Mail status')
        first.request('info', id=a, version=7)
        before = native.focus()
        a_state, embedded = dock(native, first, a)
        assert native.focus() == before == native.app
        assert first.request('owner')['owner'] == owner
        assert native.property(owner, '_FVWM_TRAY_HOST', 'WINDOW') == [host]
        evidence['cases']['owner_discovery_and_competing_start'] = dict(
            owner=owner, host=host, announcement=announced, embedded=embedded,
            completed_competing_start=competitor,
            actual_client=a_state, preserved_application_focus=before)
        b = second.create(width=30, height=26, name='Network status')
        second.request('map', id=b)
        dock(native, second, b)
        native.wait_status(lambda s: s['count'] == 2 and s['mapped'] == 2)
        refusal_before = native.refusals()
        first.request('dock', id=a)
        assert native.wait_status(lambda s: s['count'] == 2)['owner'] == owner
        invalid_messages = [(first.identity['root'], {}), (owner, {}),
            (host, {}), (initial['proxy'], {}), (a, {'format': 8}),
            (a, {'opcode': 99})]
        for identifier, fields in invalid_messages:
            first.request('dock', id=identifier, **fields)
        bad = []
        for mode in ('missing', 'wrong-type', 'oversize'):
            identifier = first.create(name='Malformed fixture ' + mode)
            first.request('info', id=identifier, mode=mode)
            first.request('dock', id=identifier)
            bad.append(dict(mode=mode, id=identifier))
        invalid_flags = first.create(name='Unknown XEmbed flags')
        first.request('info', id=invalid_flags, flags=2)
        first.request('dock', id=invalid_flags)
        bad.append(dict(mode='unknown flags', id=invalid_flags))
        unsupported = []
        input_only = first.create(name='Input-only refusal', kind='input-only')
        first.request('dock', id=input_only)
        unsupported.append(dict(reason='InputOnly', id=input_only))
        other = first.request('create', name='Other-visual refusal', visual='other')
        if 'unavailable' in other:
            evidence.setdefault('unavailable', []).append(other['unavailable'])
        else:
            first.request('dock', id=other['id'])
            unsupported.append(dict(reason='Other visual/depth', id=other['id']))
        parent = first.create(name='Foreign parent fixture')
        nested = first.create(name='Already embedded foreign child')
        first.request('reparent', id=nested, parent=parent)
        first.request('dock', id=nested)
        refusal_after = native.rejected_after(refusal_before,
            2 + len(invalid_messages) + len(bad) + len(unsupported))
        for item in bad + unsupported:
            item['state'] = first.state(item['id'])
            assert item['state']['exists'] and (
                item['state']['parent'] == first.identity['root'])
        assert first.state(a)['parent'] == a_state['parent']
        nested_state = first.state(nested)
        assert nested_state['exists'] and nested_state['parent'] == parent
        assert native.wait_status(lambda s: s['count'] == 2)['count'] == 2
        evidence['cases']['bounded_dock_validation'] = dict(
            duplicate_id=a, malformed=bad, unsupported=unsupported,
            nonroot=nested_state, owner=owner,
            refusal_counter_before=refusal_before,
            refusal_counter_after=refusal_after)
        changing = second.create(name='Property mutation recovery')
        dock(native, second, changing)
        native.wait_status(lambda s: s['count'] == 3)
        second.request('info', id=changing, mode='oversize')
        mutated = released(native, second, changing)
        native.wait_status(lambda s: s['count'] == 2)
        assert first.state(a)['parent'] == a_state['parent']
        second.request('info', id=changing, mode='valid')
        dock(native, second, changing)
        second.request('destroy', id=changing)
        native.wait_status(lambda s: s['count'] == 2)
        evidence['cases']['malformed_property_detach_and_redock'] = dict(
            actual_survivor=mutated, unrelated_client=first.state(a))
        first.request('info', id=a, flags=0)
        wait_state(native, first, a, lambda s: s['map'] != 2,
            'XEmbed mapped flag clears actual mapping')
        map_refusal = native.refusals()
        first.request('map', id=a)
        # Same X sender queues MapRequest before this duplicate dock. The
        # observed refusal is a positive processing fence for that sequence.
        first.request('dock', id=a)
        native.rejected_after(map_refusal)
        assert first.state(a)['map'] != 2
        first.request('info', id=a, flags=1)
        wait_state(native, first, a, lambda s: s['map'] == 2,
            'XEmbed mapped flag restores actual mapping')
        first.request('resize', id=a, width=43, height=31)
        resized = wait_state(native, first, a,
            lambda s: (s['width'], s['height']) == (43, 31),
            'actual foreign configure request')
        native.command('SendToModule FvwmTray Hide')
        hidden_status = native.wait_status(lambda s: s['mapped'] == 2)
        hidden_host = wait_for_predicate('actual hidden host window',
            lambda: (lambda attrs: attrs if attrs and attrs.map_state == 0
                else None)(native.attrs(host)), time.monotonic() + 5,
            process=native.process)
        wait_state(native, first, a, lambda s: s['map'] != 2,
            'hidden first client not viewable')
        wait_state(native, second, b, lambda s: s['map'] != 2,
            'hidden second client not viewable')
        native.command('SendToModule FvwmTray Show')
        wait_state(native, first, a, lambda s: s['map'] == 2, 'shown tray client')
        evidence['cases']['mapping_resize_and_visibility'] = dict(
            actual_resized=resized, hidden_mapped_flags=hidden_status['mapped'],
            actual_hidden_host_map=hidden_host.map_state,
            dimensions_are_actual_pixels=True)
        normal = first.create(width=28, height=18,
            name='Normal unmapped root plug', override_redirect=False)
        normal_root = first.state(normal)
        assert not normal_root['override'] and normal_root['map'] == 0
        assert normal_root['parent'] == first.identity['root']
        normal_embedded, _ = dock(native, first, normal)
        slot = native.attrs(normal_embedded['parent'])
        assert slot and slot.all_event_masks & (1 << 20)
        native.wait_status(lambda s: s['count'] == 3 and s['mapped'] == 3)
        first.request('resize', id=normal, width=47, height=29)
        normal_visible = wait_state(native, first, normal,
            lambda s: not s['override'] and s['map'] == 2 and
                (s['width'], s['height']) == (47, 29),
            'normal plug actual visible ConfigureRequest allocation')
        native.command('SendToModule FvwmTray Hide')
        hidden_flags = native.wait_status(lambda s: s['mapped'] == 3)
        wait_for_predicate('normal ConfigureRequest test host hidden',
            lambda: (lambda attrs: attrs and attrs.map_state == 0)(
                native.attrs(host)), time.monotonic() + 5,
            process=native.process)
        wait_state(native, first, normal, lambda s: s['map'] != 2,
            'normal plug not viewable while host hidden')
        first.request('resize', id=normal, width=37, height=23)
        normal_hidden = wait_state(native, first, normal,
            lambda s: not s['override'] and s['map'] != 2 and
                (s['width'], s['height']) == (37, 23),
            'normal hidden plug actual ConfigureRequest allocation')
        oversize_before = native.refusals()
        first.request('resize', id=normal, width=257, height=23)
        oversize_after = native.rejected_after(oversize_before)
        refused_normal = first.state(normal)
        assert (refused_normal['width'], refused_normal['height']) == (37, 23)
        assert refused_normal['map'] != 2
        native.command('SendToModule FvwmTray Show')
        normal_shown = wait_state(native, first, normal,
            lambda s: s['map'] == 2 and
                (s['width'], s['height']) == (37, 23),
            'normal resized plug visible after host Show')
        first.request('destroy', id=normal)
        native.wait_status(lambda s: s['count'] == 2 and s['mapped'] == 2)
        assert not first.state(normal)['exists']
        evidence['cases']['normal_visible_and_hidden_configure_requests'] = dict(
            initial_root=normal_root, embedded=normal_embedded,
            socket_substructure_redirect=bool(slot.all_event_masks & (1 << 20)),
            actual_visible=normal_visible, actual_hidden=normal_hidden,
            hidden_mapped_flags=hidden_flags['mapped'],
            oversize_counter_before=oversize_before,
            oversize_counter_after=oversize_after,
            actual_oversize_refusal=refused_normal, actual_shown=normal_shown,
            ordinary_override_configure_notify=resized)
        # A repeated explicit Focus preserves a passive application's child;
        # Escape must still restore its top-level after an unmanaged proxy.
        child = first.create(width=16, height=16, name='Application child')
        first.request('reparent', id=child, parent=native.app)
        first.request('map', id=child)
        wait_state(native, first, child, lambda s: s['map'] == 2,
            'application child is viewable')
        native.x.XSetInputFocus.argtypes = [C.c_void_p, C.c_ulong,
            C.c_int, C.c_ulong]
        native.x.XSetInputFocus(native.d, child, 2, 0)
        native.x.XSync(native.d, 0)
        assert native.focus() == child
        native.command('Silent WindowId 0x%x Focus' % native.app)
        assert native.focus() == child
        first.request('destroy', id=child)
        native.focus_app()
        native.command('SendToModule FvwmTray Focus')
        first.message(a, 4)
        proxy = native.status()['proxy']
        wait_for_predicate('own tray focus proxy', lambda: native.focus() == proxy,
            time.monotonic() + 5, process=native.process)
        native.press('a')
        forwarded = first.key(a, 0x61)
        native.press('Tab')
        tab_next = first.key(a, 0xff09)
        first.request('xembed', id=a, opcode=6)
        first.message(a, 5)
        second.message(b, 4, detail=1)
        native.press('Tab', shift=True)
        shift_prev = second.key(b, 0xffe1)
        tab_prev = second.key(b, 0xff09)
        second.request('xembed', id=b, opcode=7)
        second.message(b, 5)
        first.message(a, 4, detail=2)
        native.press('Escape')
        wait_for_predicate('core-routed application focus return',
            lambda: native.focus() == native.app, time.monotonic() + 5,
            process=native.process)
        evidence['cases']['keyboard_proxy_and_traversal'] = dict(
            proxy=proxy, forwarded_key=forwarded, focus_return=native.app,
            actual_tab_before_next=tab_next, actual_shift_before_prev=shift_prev,
            actual_tab_before_prev=tab_prev,
            application_policy='ClickToFocus', preserved_application_child=child)
        first.request('resize', id=a, width=24, height=20)
        wait_state(native, first, a,
            lambda s: (s['width'], s['height']) == (24, 20), 'restored client size')
        first.request('resize', id=a, width=1, height=1)
        wait_state(native, first, a,
            lambda s: (s['width'], s['height']) == (1, 1),
            'one-pixel foreign client retained')
        second.request('info', id=b, flags=0)
        wait_state(native, second, b, lambda s: s['map'] != 2,
            'second client hidden by its mapped flag')
        native.command('*FvwmTray Padding 0')
        native.command('*FvwmTray Gap 0')
        native.command('SendToModule FvwmTray Reload')
        tiny_status = native.wait_status(lambda s: not s['pending'] and
            s['mapped'] == 1)
        def zero_padding_projection():
            foreign = first.state(a)
            socket = native.attrs(foreign['parent'])
            return dict(foreign=foreign,
                socket={k: getattr(socket, k) for k in
                    ('x', 'y', 'width', 'height')}) if (socket and
                    socket.x == 0 and foreign['map'] == 2 and
                    (foreign['width'], foreign['height']) == (1, 1)) else None
        tiny = wait_for_predicate('actual zero-padding tiny-client projection',
            zero_padding_projection, time.monotonic() + 5,
            process=native.process)
        target = 24 * tiny_status['quarters'] // 4
        host_rect = native.rect(host)
        assert host_rect and host_rect['width'] >= target
        assert host_rect['height'] >= target
        assert tiny['socket']['y'] >= target
        evidence['cases']['host_target_floor_with_one_pixel_client'] = dict(
            source_reviewed_edge='added before first native gate; no executed old baseline claimed',
            native=tiny_status, host_rect=host_rect, minimum_target=target,
            actual_foreign=tiny['foreign'], actual_socket=tiny['socket'])
        first.request('resize', id=a, width=24, height=20)
        second.request('info', id=b, flags=1)
        native.command('*FvwmTray Padding 4')
        native.command('*FvwmTray Gap 4')
        native.command('SendToModule FvwmTray Reload')
        native.wait_status(lambda s: not s['pending'] and s['mapped'] == 2)
        wait_state(native, first, a,
            lambda s: (s['width'], s['height']) == (24, 20),
            'foreign size restored after minimum-target edge')
        wait_state(native, second, b, lambda s: s['map'] == 2,
            'second client visible after minimum-target edge')
        font_heights = {}
        for profile, (fg, bg, font, border) in PROFILES.items():
            overlay = out / (profile + '.fvwm')
            overlay.write_text('Colorset 42 fg ' + fg + ', bg ' + bg + '\n'
                'Style * BorderWidth ' + str(border) + '\n'
                '*FvwmTray Font "' + font + '"\n'
                '*FvwmTray Gap 6\n*FvwmTray Position TopRight\n'
                'SendToModule FvwmTray Reload\n')
            assert not any(c in str(overlay) for c in '\n\r\0"')
            native.command('Read "' + str(overlay) + '"')
            state = native.wait_status(lambda s: not s['pending'] and
                s['font_height'] > 13 and s['count'] == 2)
            if profile == 'classic-large':
                state = native.wait_status(lambda s: not s['pending'] and
                    s['font_height'] > max(font_heights.values()))
            font_heights[profile] = state['font_height']
            assert (first.state(a)['width'], first.state(a)['height']) == (24, 20)
            native.capture(profile + '.png')
            evidence.setdefault('theme_configs', []).append(dict(profile=profile,
                file=overlay.name, sha256=digest(overlay), font=font,
                font_height=state['font_height'], foreground=fg, background=bg,
                border=border, native=state, actual_client=first.state(a)))
        evidence['cases']['three_actual_font_palette_profiles'] = font_heights
        def placement(vertical):
            first_state, second_state = first.state(a), second.state(b)
            left = native.attrs(first_state['parent'])
            right = native.attrs(second_state['parent'])
            if not left or not right:
                return None
            correct = (left.x == right.x and left.y < right.y) if vertical else (
                left.y == right.y and left.x < right.x)
            return dict(first_socket={k: getattr(left, k) for k in
                ('x', 'y', 'width', 'height')},
                second_socket={k: getattr(right, k) for k in
                ('x', 'y', 'width', 'height')}) if correct else None
        native.command('*FvwmTray Orientation Vertical')
        native.command('SendToModule FvwmTray Reload')
        wait_for_predicate('published vertical orientation',
            lambda: native.property(owner, '_NET_SYSTEM_TRAY_ORIENTATION',
                'CARDINAL') == [1], time.monotonic() + 5,
            process=native.process)
        vertical = wait_for_predicate('actual vertical socket layout',
            lambda: placement(True), time.monotonic() + 5,
            process=native.process)
        native.command('*FvwmTray Orientation Horizontal')
        native.command('SendToModule FvwmTray Reload')
        wait_for_predicate('published horizontal orientation',
            lambda: native.property(owner, '_NET_SYSTEM_TRAY_ORIENTATION',
                'CARDINAL') == [0], time.monotonic() + 5,
            process=native.process)
        horizontal = wait_for_predicate('actual horizontal socket layout',
            lambda: placement(False), time.monotonic() + 5,
            process=native.process)
        native.command('*FvwmTray Gap 999')
        native.command('SendToModule FvwmTray Reload')
        native.wait_status(lambda state: not state['pending'])
        rejected_gap = placement(False)
        assert rejected_gap == horizontal
        evidence['cases']['orientation_and_invalid_layout_config'] = dict(
            vertical=vertical, horizontal=horizontal,
            invalid_gap_preserved=rejected_gap,
            actual_client_dimensions=first.state(a))
        if options.xorg_config:
            original = native.settled('DUMMY0', 4)
            native.reserve_top(96)
            reserved = native.settled('DUMMY0', 4, minimum_top=96)
            assert reserved['host_rect']['y'] > original['host_rect']['y']
            run_required_xrandr(runtime, options.display,
                ['--output', 'DUMMY1', '--primary'], 'second-primary',
                evidence['operations'])
            wait_for_authoritative_state(runtime, options.display, 'primary2',
                lambda s: s['monitors'].get('DUMMY1', {}).get('primary'),
                time.monotonic() + 8, 'second-primary')
            scaled = native.settled('DUMMY1', 8,
                after=reserved['native']['epoch'])
            assert scaled['native']['font_height'] > reserved['native']['font_height']
            assert scaled['native']['host'] == host
            actual = first.state(a)
            assert (actual['width'], actual['height']) == (24, 20)
            assert actual['parent'] == a_state['parent']
            assert native.focus() == native.app
            native.capture('primary-2x.png')
            evidence['cases']['actual_primary_scale_and_work_area'] = dict(
                initial=original, reserved=reserved, scaled=scaled,
                unchanged_foreign_pixels=actual)
            # Restore the first primary for the later overflow test's dimensions.
            run_required_xrandr(runtime, options.display,
                ['--output', 'DUMMY0', '--primary'], 'restore-primary',
                evidence['operations'])
            native.settled('DUMMY0', 4, after=scaled['native']['epoch'])
        else:
            evidence.setdefault('unavailable', []).append(
                'Actual dual-output RandR transition requires --xorg-config.')
        second.request('reparent', id=b)
        native.wait_status(lambda s: s['count'] == 1)
        assert second.state(b)['parent'] == second.identity['root']
        first.request('destroy', id=a)
        native.wait_status(lambda s: s['count'] == 0)
        assert not first.state(a)['exists']
        evidence['cases']['external_reparent_and_destruction'] = 'passed'
        capacity = [first.create(width=128, height=24,
            name='Capacity icon ' + str(i)) for i in range(33)]
        capacity_before = native.refusals()
        for identifier in capacity:
            first.request('dock', id=identifier)
        capacity_after = native.rejected_after(capacity_before)
        full = native.wait_status(lambda s: s['count'] == 32 and
            s['mapped'] == 32)
        assert full['limit'] == 32
        def actual_capacity():
            values = [first.state(i) for i in capacity]
            return values if (all(s['exists'] and
                s['parent'] != first.identity['root'] for s in values[:32]) and
                values[32]['parent'] == first.identity['root'] and
                0 < sum(s['map'] == 2 for s in values[:32]) < 32) else None
        states = wait_for_predicate('actual bounded capacity projection',
            actual_capacity, time.monotonic() + 5, process=native.process)
        for identifier in capacity:
            first.request('destroy', id=identifier)
        native.wait_status(lambda s: s['count'] == 0)
        evidence['cases']['capacity_overflow_and_recovery'] = dict(
            native=full, actual_clients=states,
            actual_viewable_count=sum(s['map'] == 2 for s in states[:32]),
            refusal_counter_before=capacity_before,
            refusal_counter_after=capacity_after)
        anchor = first.create(name='Retained across peer disconnect')
        dock(native, first, anchor)
        third = native.peer('disconnect')
        disconnected = third.create(name='Producer disconnect')
        dock(native, third, disconnected)
        native.wait_status(lambda s: s['count'] == 2)
        disconnect_outcome = third.close()
        assert disconnect_outcome == {'exit': 0, 'forced_cleanup': False}
        native.wait_status(lambda s: s['count'] == 1)
        assert native.attrs(disconnected) is None
        assert first.state(anchor)['map'] == 2
        first.request('destroy', id=anchor)
        native.wait_status(lambda s: s['count'] == 0)
        evidence['cases']['producer_disconnect_preserves_other_client'] = dict(
            producer=disconnect_outcome, destroyed_window=disconnected,
            retained_client=anchor)
        survivor = second.create(name='Survives orderly tray shutdown')
        dock(native, second, survivor)
        native.command('KillModule FvwmTray')
        wait_for_predicate('orderly tray owner exit',
            lambda: first.request('owner')['owner'] == 0,
            time.monotonic() + 5, process=native.process)
        orderly = released(native, second, survivor)
        restarted = native.start(first)
        # X may legitimately reuse the old owner's XID after disconnect.
        # Observed no-owner separation and a new verified process lifetime
        # are the restart boundary, not numeric XID inequality.
        assert native.status()['count'] == 0
        evidence['cases']['orderly_release_and_explicit_restart'] = dict(
            actual_survivor=orderly, old_owner=owner,
            new_owner=restarted['owner'],
            owner_xid_reused=restarted['owner'] == owner, autoreplay=False)
        dock(native, second, survivor)
        replacement = first.request('claim')['owner']
        assert replacement != restarted['owner']
        lost = released(native, second, survivor)
        assert first.request('owner')['owner'] == replacement
        first.request('release')
        wait_for_predicate('controlled owner released',
            lambda: first.request('owner')['owner'] == 0,
            time.monotonic() + 5, process=native.process)
        evidence['cases']['selection_loss_releases_clients'] = dict(
            replacement_owner=replacement, actual_survivor=lost)
        native.start(first)
        dock(native, second, survivor)
        if hasattr(native, 'module'):
            native.module.send(signal.SIGKILL)
            exited = native.module.exited(seconds=5)
            wait_for_predicate('abrupt tray exit releases owner',
                lambda: first.request('owner')['owner'] == 0,
                time.monotonic() + 5, process=native.process)
            saved = wait_state(native, second, survivor,
                lambda s: s['exists'] and s['parent'] == second.identity['root'],
                'save-set preserves client at persistent root after abrupt exit')
            recovery = native.start(first)
            assert native.status()['count'] == 0
            redocked, new_embedded = dock(native, second, survivor)
            native.command('KillModule FvwmTray')
            final_survivor = released(native, second, survivor)
            evidence['cases']['abrupt_exit_save_set_survival'] = dict(
                exact_process=exited, actual_survivor=saved,
                restarted_native=recovery, actual_redock=redocked,
                embedded=new_embedded, orderly_final=final_survivor,
                map_state='base save set may map the saved client')
        else:
            native.command('KillModule FvwmTray')
            evidence.setdefault('unavailable', []).append(
                'Linux pidfd abrupt-exit process observation unavailable.')
        native.control.echo('core-responsive-after-tray-lifetime-cases')
        evidence['status'] = 'passed'
    except BaseException as failure:
        evidence.update(status='failed', error=repr(failure),
            traceback=traceback.format_exc())
        raise
    finally:
        if native:
            for name, peer in native.peers:
                (out / (name + '-wire.json')).write_text(
                    json.dumps(peer.records, indent=2) + '\n')
            try:
                native.close()
            except BaseException as failure:
                evidence.update(status='failed', cleanup_error=repr(failure))
            if native.errors and native.errors.unexpected:
                evidence['status'] = 'failed'
                evidence['cleanup_x_errors'] = native.errors.unexpected
        for process in runtime.processes:
            if process.log_path.exists():
                shutil.copyfile(process.log_path, out / process.log_path.name)
        try:
            runtime.close()
        except BaseException as failure:
            evidence.update(status='failed', runtime_cleanup_error=repr(failure))
        (out / 'result.json').write_text(json.dumps(evidence, indent=2) + '\n')
    assert evidence['status'] == 'passed', evidence


def main():
    parser = argparse.ArgumentParser(allow_abbrev=False)
    parser.add_argument('--build', type=Path, required=True)
    for name in ('fvwm', 'tray', 'client', 'mfl', 'capture'):
        parser.add_argument('--' + name, type=Path)
    parser.add_argument('--display')
    parser.add_argument('--xorg-config', type=Path)
    parser.add_argument('--execution-profile', default='unspecified',
        choices=('unspecified', 'native-cpu', 'emulated-cpu'))
    parser.add_argument('--artifacts-dir', type=Path, required=True)
    options = parser.parse_args()
    defaults = dict(fvwm=options.build / 'fvwm3',
        tray=options.build / 'modules/FvwmTray/FvwmTray',
        client=options.build / 'tests/tray-client',
        mfl=options.build / 'modules/FvwmMFL/FvwmMFL',
        capture=options.build / 'bin/fvwm-desktop-capture')
    for name, default in defaults.items():
        setattr(options, name, (getattr(options, name) or default).resolve())
        assert getattr(options, name).is_file(), getattr(options, name)
    run(options)


if __name__ == '__main__':
    main()
