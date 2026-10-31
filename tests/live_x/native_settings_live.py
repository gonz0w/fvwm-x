#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the GNU General Public License as published by the Free Software
# Foundation; either version 2 of the License, or any later version.
"""Opt-in owned actual-X/native-helper/GTK3 application preference fixture.

Run in the lead's executor against one coherent build. No compiler, package
installer, provider emulator or synthetic XSETTINGS publisher is used. Direct
pipe tests invoke the actual C helper with no arguments; the controller group
uses its direct real child. Transparent module proxies only record actual
Desktop/Form/MFL commands and Wait. Three profiles capture the native editor.

Default --groups wire controller runs raw pipe checks once on the first profile
and controller/GTK/editor checks on every selected profile. --groups wire runs
only the first profile, without Desktop, GTK, Xcursor consumers or captures.
--groups controller omits the raw helper cases explicitly.
FreeBSD/OpenBSD permit only --groups wire, using actual owned child Wait and
declared executable input hashes; Linux /proc identity is not inferred there.
--installed-helper-lookup removes the helper override from the owned Desktop
process environment; supply explicit absolute --desktop and --helper installed
paths. The latter remains the expected real child identity and hash input.

--toolkit-env accepts a JSON object of environment overrides, or an object with
an 'environment'/'env' object. The caller supplies an existing private GTK bundle;
this fixture never installs one. GTK is required for a successful full run.
Cached Xcursor clients, non-GTK toolkits, browsers, physical/native-CPU hardware,
sanitisers and the wider platform/release gates remain separate qualifications.
--portal-helper additionally observes the actual installed Settings frontend
through the existing Desktop motion changes and native-owner retirement. Supply
--frontend, --portal-descriptor, --activation-service and optional --portal-env
for an already installed private runtime; this probe does not infer application
effects from a public protocol value.
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
import socket
import struct
import subprocess
import sys
import time

from browser_continuity_live import cleanup_receipts
from browser_handoff_live import child_identity, process_identity
from colorset_live import WindowAttributes
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
SETTINGS = {
    'Gtk/FontName': 'Sans 11', 'Net/IconThemeName': 'Adwaita',
    'Gtk/CursorThemeName': 'Adwaita', 'Gtk/CursorThemeSize': 24,
    'Gtk/EnableAnimations': 0,
}
GTK_KEYS = {
    'Gtk/FontName': 'gtk-font-name',
    'Net/IconThemeName': 'gtk-icon-theme-name',
    'Gtk/CursorThemeName': 'gtk-cursor-theme-name',
    'Gtk/CursorThemeSize': 'gtk-cursor-theme-size',
    'Gtk/EnableAnimations': 'gtk-enable-animations',
}


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def owned_spawn_identity(process, argv, env):
    """Keep Linux kernel evidence; BSD records only the owned Popen contract."""
    if sys.platform == 'linux':
        return child_identity(process.pid, os.getpid())
    assert sys.platform.startswith(('freebsd', 'openbsd'))
    arguments = [str(v) for v in argv]
    executable = (arguments[0] if '/' in arguments[0] else
        shutil.which(arguments[0], path=env.get('PATH', os.defpath)))
    assert executable is not None, arguments
    path = Path(executable).resolve()
    assert path.is_file() and os.access(path, os.X_OK), path
    return dict(kind='owned-spawn', child=process.pid, parent=os.getpid(),
        platform=sys.platform, declared_argv=arguments,
        executable_input=dict(path=str(path), sha256=digest(path)),
        kernel_image='not observed; declared executable input only',
        wait_method='owned Popen.wait; actual returncode recorded on teardown')


def decode_xsettings(data):
    """Independent strict decoder of the public XSETTINGS 0.5 wire format."""
    assert 12 <= len(data) <= 65536
    assert data[0] in (0, 1) and data[1:4] == b'\0\0\0'
    order = '<' if data[0] == 0 else '>'
    serial, count = struct.unpack_from(order + 'II', data, 4)
    assert count <= 256
    offset, values = 12, {}

    def take(size):
        nonlocal offset
        assert 0 <= size <= len(data) - offset
        result = data[offset:offset + size]
        offset += size
        return result

    def aligned():
        padding = (-offset) % 4
        assert take(padding) == b'\0' * padding

    for _ in range(count):
        kind, unused, length = struct.unpack(order + 'BBH', take(4))
        assert unused == 0 and kind in (0, 1, 2) and 0 < length <= 256
        name = take(length).decode('ascii')
        assert name not in values
        aligned()
        changed, = struct.unpack(order + 'I', take(4))
        if kind == 0:
            value, = struct.unpack(order + 'i', take(4))
        elif kind == 1:
            size, = struct.unpack(order + 'I', take(4))
            assert size <= 65536
            value = take(size).decode('utf-8', 'strict')
            aligned()
        else:
            value = list(struct.unpack(order + 'HHHH', take(8)))
        values[name] = dict(type=kind, value=value, last_change_serial=changed)
    assert offset == len(data), 'trailing XSETTINGS bytes'
    return dict(serial=serial, settings=values, sha256=hashlib.sha256(data).hexdigest())


class XObserver:
    """Read real X properties through Xlib; never constructs provider bytes."""
    def __init__(self, display, evidence):
        self.client = connect_client(display)
        self.errors = SessionXErrors(self.client, evidence).install()
        self.x, self.d, self.root = self.client.x, self.client.display, self.client.root
        signatures = {
            'XInternAtom': ([C.c_void_p, C.c_char_p, C.c_int], C.c_ulong),
            'XGetSelectionOwner': ([C.c_void_p, C.c_ulong], C.c_ulong),
            'XSetSelectionOwner': ([C.c_void_p, C.c_ulong, C.c_ulong, C.c_ulong], C.c_int),
            'XGetWindowProperty': ([C.c_void_p, C.c_ulong, C.c_ulong,
                C.c_long, C.c_long, C.c_int, C.c_ulong, C.POINTER(C.c_ulong),
                C.POINTER(C.c_int), C.POINTER(C.c_ulong), C.POINTER(C.c_ulong),
                C.POINTER(C.c_void_p)], C.c_int),
            'XChangeProperty': ([C.c_void_p, C.c_ulong, C.c_ulong, C.c_ulong,
                C.c_int, C.c_int, C.c_void_p, C.c_int], C.c_int),
            'XDeleteProperty': ([C.c_void_p, C.c_ulong, C.c_ulong], C.c_int),
            'XDefaultScreen': ([C.c_void_p], C.c_int),
            'XGetWindowAttributes': ([C.c_void_p, C.c_ulong,
                C.POINTER(WindowAttributes)], C.c_int),
            'XrmInitialize': ([], None),
            'XrmGetStringDatabase': ([C.c_char_p], C.c_void_p),
            'XrmGetResource': ([C.c_void_p, C.c_char_p, C.c_char_p,
                C.POINTER(C.c_char_p), C.c_void_p], C.c_int),
            'XrmDestroyDatabase': ([C.c_void_p], None),
        }
        for name, (args, result) in signatures.items():
            function = getattr(self.x, name)
            function.argtypes, function.restype = args, result
        self.selection = self.atom('_XSETTINGS_S' + str(self.x.XDefaultScreen(self.d)))
        self.contender = None

    def atom(self, name):
        return self.x.XInternAtom(self.d, name.encode(), 0)

    def owner(self):
        return int(self.x.XGetSelectionOwner(self.d, self.selection))

    def property(self, window, name, maximum=65536):
        kind, count, after = C.c_ulong(), C.c_ulong(), C.c_ulong()
        form, result = C.c_int(), C.c_void_p()
        try:
            status = self.errors.read(window, 20, lambda:
                self.x.XGetWindowProperty(self.d, window, self.atom(name),
                    0, (maximum + 3) // 4, 0, 0, C.byref(kind), C.byref(form),
                    C.byref(count), C.byref(after), C.byref(result)))
            if status is None:
                return None
            assert status == 0 and after.value == 0, (name, status, after.value)
            if kind.value == 0:
                return None
            assert form.value == 8 and count.value <= maximum, (name, form.value, count.value)
            assert kind.value == self.atom('STRING' if name == 'RESOURCE_MANAGER'
                else '_XSETTINGS_SETTINGS'), (name, kind.value)
            return C.string_at(result, count.value)
        finally:
            if result.value:
                self.x.XFree(result)

    def snapshot(self):
        owner = self.owner()
        if not owner:
            return None
        data = self.property(owner, '_XSETTINGS_SETTINGS')
        return dict(owner=owner, **decode_xsettings(data)) if data is not None else None

    def window_exists(self, window):
        attributes = WindowAttributes()
        return bool(self.errors.read(window, 3, lambda:
            self.x.XGetWindowAttributes(self.d, window, C.byref(attributes))))

    def resources(self, maximum=65536):
        return self.property(self.root, 'RESOURCE_MANAGER', maximum)

    def write_resources(self, value):
        # This writer changes only the fixture-owned disposable server root.
        if value is None:
            self.x.XDeleteProperty(self.d, self.root, self.atom('RESOURCE_MANAGER'))
        else:
            assert len(value) <= 65540
            memory = C.create_string_buffer(value)
            self.x.XChangeProperty(self.d, self.root, self.atom('RESOURCE_MANAGER'),
                self.atom('STRING'), 8, 0, memory, len(value))
        self.x.XSync(self.d, 0)

    def lookup(self):
        class Value(C.Structure):
            _fields_ = [('size', C.c_uint), ('addr', C.c_void_p)]
        raw = self.resources() or b''
        assert b'\0' not in raw
        self.x.XrmInitialize()
        database = self.x.XrmGetStringDatabase(raw)
        assert database
        found = {}
        try:
            for key in ('Xcursor.theme', 'Xcursor.size', 'Fixture.keep'):
                value, kind = Value(), C.c_char_p()
                if self.x.XrmGetResource(database, key.encode(), key.encode(),
                        C.byref(kind), C.byref(value)):
                    found[key] = C.string_at(value.addr, value.size).rstrip(b'\0').decode()
        finally:
            self.x.XrmDestroyDatabase(database)
        return found

    def claim(self):
        assert self.contender is None
        self.contender = self.x.XCreateSimpleWindow(self.d, self.root,
            0, 0, 1, 1, 0, 0, 0)
        self.x.XSetSelectionOwner(self.d, self.selection, self.contender, 0)
        self.x.XSync(self.d, 0)
        assert self.owner() == self.contender
        return self.contender

    def release(self):
        if self.contender:
            self.x.XDestroyWindow(self.d, self.contender)
            self.x.XSync(self.d, 0)
            self.contender = None

    def close(self):
        self.release()
        self.errors.close_display()


def apply_line(desktop='1' * 32, run='2' * 32, sequence=1, values=None):
    values = SETTINGS if values is None else values
    def encoded(key):
        return str(values.get(key, '')).encode().hex() or '-'
    return ('FNS1 APPLY %s %s %d %d %d %s %s %s\n' % (desktop, run,
        sequence, values.get('Gtk/EnableAnimations', 0),
        values.get('Gtk/CursorThemeSize', 0), encoded('Gtk/FontName'),
        encoded('Net/IconThemeName'), encoded('Gtk/CursorThemeName'))).encode()


class PipeChild:
    """Own one exact process and bounded response pipe, with actual Wait."""
    def __init__(self, argv, env, out, name, evidence):
        self.log = (out / (name + '.stderr')).open('wb')
        self.process = subprocess.Popen([str(p) for p in argv], env=env,
            stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=self.log,
            start_new_session=True)
        self.pending = bytearray()
        self.receipt = dict(argv=[str(p) for p in argv], pid=self.process.pid,
            identity=owned_spawn_identity(self.process, argv, env), wire=[])
        evidence.setdefault('pipe_children', {})[name] = self.receipt
        self.closed = False

    def send(self, raw):
        assert len(raw) <= 8192 and len(self.receipt['wire']) < 256
        self.process.stdin.write(raw)
        self.process.stdin.flush()
        self.receipt['wire'].append(dict(direction='in', hex=raw.hex()))

    def line(self, timeout=4):
        deadline = time.monotonic() + timeout
        while b'\n' not in self.pending:
            remaining = deadline - time.monotonic()
            assert remaining > 0, 'actual child response deadline'
            assert select.select([self.process.stdout], [], [], remaining)[0]
            data = os.read(self.process.stdout.fileno(), 4096)
            assert data, ('actual child EOF', self.process.poll())
            self.pending.extend(data)
            assert len(self.pending) <= 16384
        raw, _, rest = self.pending.partition(b'\n')
        self.pending[:] = rest
        self.receipt['wire'].append(dict(direction='out', hex=bytes(raw).hex()))
        return bytes(raw)

    def eof(self, expected=None):
        if not self.process.stdin.closed:
            self.process.stdin.close()
        code = self.process.wait(timeout=3)
        self.receipt['wait'] = dict(returncode=code, forced=False)
        if expected is not None:
            assert code == expected, self.receipt
        return code

    def close(self):
        if self.closed:
            return
        try:
            try:
                self.eof()
            except (subprocess.TimeoutExpired, BrokenPipeError):
                self.process.terminate()
                try:
                    self.process.wait(timeout=1)
                except subprocess.TimeoutExpired:
                    self.process.kill()
                    self.process.wait(timeout=1)
                self.receipt['wait'] = dict(returncode=self.process.returncode, forced=True)
        finally:
            self.process.stdout.close()
            self.log.close()
            self.closed = True


def helper_result(child):
    fields = child.line().decode('ascii').split()
    assert len(fields) == 9 and fields[:2] == ['FNS1', 'RESULT'], fields
    assert all(re.fullmatch('[0-9a-f]{32}', f) for f in fields[2:4])
    assert fields[5] in ('ready', 'conflict', 'lost', 'failed', 'off')
    assert fields[6] in ('applied', 'inherited', 'conflict', 'unavailable')
    seq, serial = int(fields[4]), int(fields[7])
    assert 0 <= seq <= 2**64 - 1 and 0 <= serial <= 2**32 - 1
    assert fields[8] in ('none', 'owner-conflict', 'invalid-input', 'stale-input',
        'x-error', 'resource-conflict', 'resource-limit', 'parent-eof')
    return dict(desktop=fields[2], run=fields[3], sequence=seq,
        readiness=fields[5], resource_state=fields[6], xserial=serial, reason=fields[8])


def wait_snapshot(observer, values, core=None):
    def observed():
        snapshot = observer.snapshot()
        if snapshot and {k: v['value'] for k, v in snapshot['settings'].items()} == values:
            return snapshot
        return None
    return wait_for_predicate('independent complete XSETTINGS snapshot', observed,
        time.monotonic() + 6, process=core)


def wire_cases(args, env, out, evidence, observer):
    children, reservations = [], []
    def child(name):
        value = PipeChild([args.helper], env, out, name, evidence)
        children.append(value)
        return value
    baseline = b'Fixture.keep: exact-prefix\nXcursor.size: 17\n'
    try:
        observer.write_resources(baseline)
        peer = child('wire-main')
        first = apply_line()
        peer.send(first[:37])
        assert not select.select([peer.process.stdout], [], [], .05)[0]
        assert observer.owner() == 0 and observer.resources() == baseline
        peer.send(first[37:])
        ready = helper_result(peer)
        assert ready['readiness'] == 'ready' and ready['resource_state'] == 'applied', ready
        before = wait_snapshot(observer, SETTINGS)
        assert ready['xserial'] == before['serial']
        owned = observer.resources()
        assert owned.startswith(baseline) and b'! FVWM_NATIVE_SETTINGS FNS1 BEGIN ' in owned
        assert observer.lookup() == {'Fixture.keep': 'exact-prefix',
            'Xcursor.theme': 'Adwaita', 'Xcursor.size': '24'}
        peer.send(first)
        duplicate = helper_result(peer)
        assert duplicate['xserial'] == before['serial'] and observer.snapshot() == before
        peer.send(apply_line(sequence=2))
        noop = helper_result(peer)
        assert noop['xserial'] == before['serial'] and observer.snapshot() == before
        for name, raw in [('old-sequence', first),
                ('changed-duplicate', apply_line(sequence=2,
                    values={**SETTINGS, 'Gtk/FontName': 'Sans 12'})),
                ('wrong-run', apply_line(run='3' * 32, sequence=3)),
                ('wrong-desktop', apply_line(desktop='4' * 32, sequence=3))]:
            peer.send(raw)
            refused = helper_result(peer)
            assert refused['reason'] == 'stale-input', (name, refused)
            assert observer.snapshot() == before and observer.resources() == owned
            evidence['cases'][name] = refused
        changed = {**SETTINGS, 'Gtk/FontName': 'Sans 18'}
        peer.send(apply_line(sequence=3, values=changed))
        updated = helper_result(peer)
        after = wait_snapshot(observer, changed)
        assert updated['readiness'] == 'ready' and after['serial'] != before['serial']
        for key in SETTINGS:
            old, new = before['settings'][key], after['settings'][key]
            assert (old['last_change_serial'] != new['last_change_serial']) == (
                key == 'Gtk/FontName')
        peer.send(apply_line(sequence=4, values={'Gtk/EnableAnimations': 1}))
        inherited = helper_result(peer)
        assert inherited['resource_state'] == 'inherited'
        empty = wait_snapshot(observer, {'Gtk/EnableAnimations': 1})
        assert observer.resources() == baseline
        peer.eof(expected=0)
        terminal = helper_result(peer)
        assert terminal['readiness'] == 'off' and terminal['reason'] == 'parent-eof'
        assert observer.owner() == 0 and observer.resources() == baseline
        evidence['cases']['partial_noop_serial_per_key_empty_eof'] = dict(
            initial=ready, duplicate=duplicate, noop=noop, before=before,
            changed=after, inherited=inherited, empty=empty, parent_eof=terminal)

        # Malformed initial frames must never bind/acquire or change resources.
        for index, raw in enumerate((b'FNS1 APPLY broken\n',
                apply_line().replace(b'53616e73203131', b'c080'),
                apply_line().replace(b' 24 ', b' 129 '),
                b'x' * 2048 + b'\n', apply_line().rstrip(b'\n'))):
            bad = child('wire-invalid-%d' % index)
            bad.send(raw)
            if index < 4:
                # Let the actual parser refuse complete frames before EOF;
                # closing immediately could bypass parsing on POLLHUP.
                assert bad.process.wait(timeout=3) != 0
            bad.eof()
            assert observer.owner() == 0 and observer.resources() == baseline
        evidence['cases']['invalid_frames_no_mutation'] = dict(count=5)

        contender = observer.claim()
        refused = child('wire-owner-conflict')
        refused.send(apply_line(run='5' * 32))
        conflict = helper_result(refused)
        assert conflict['readiness'] == 'conflict' and conflict['reason'] == 'owner-conflict'
        assert observer.owner() == contender and observer.resources() == baseline
        refused.eof()
        acquiring = child('wire-parent-eof-during-acquisition')
        acquiring.send(apply_line(run='c' * 32))
        assert not select.select([acquiring.process.stdout], [], [], .1)[0]
        stop_started = time.monotonic()
        acquiring.eof(expected=0)
        stop_seconds = time.monotonic() - stop_started
        assert stop_seconds < 1, 'EOF waited for the two-second acquisition grace'
        assert observer.owner() == contender and observer.resources() == baseline
        evidence['cases']['parent_eof_during_owner_conflict_grace'] = dict(
            seconds=stop_seconds, owner=contender)
        observer.release()
        lost = child('wire-owner-loss')
        lost.send(apply_line(run='6' * 32))
        assert helper_result(lost)['readiness'] == 'ready'
        observer.claim()
        loss = helper_result(lost)
        assert loss['readiness'] == 'lost'
        lost.eof()
        assert observer.owner() == observer.contender and observer.resources() == baseline
        observer.release()
        evidence['cases']['owner_conflict_loss'] = dict(conflict=conflict, loss=loss)

        edited = child('wire-resource-edit')
        edited.send(apply_line(run='7' * 32))
        assert helper_result(edited)['resource_state'] == 'applied'
        original = observer.resources()
        external = b'Fixture.external: before\n' + original + b'Fixture.tail: after\n'
        observer.write_resources(external)
        edited.send(apply_line(run='7' * 32, sequence=2,
            values={**SETTINGS, 'Gtk/CursorThemeSize': 32}))
        edit = helper_result(edited)
        assert edit['readiness'] == 'ready' and edit['resource_state'] == 'conflict'
        assert observer.resources() == external
        edited.eof(expected=0)
        assert observer.resources() == (b'Fixture.external: before\n' +
            baseline + b'Fixture.tail: after\n')
        evidence['cases']['external_edit_latched_cleanup_preserves_prefix_tail'] = edit

        observer.write_resources(baseline)
        tampered = child('wire-resource-tamper')
        tampered.send(apply_line(run='b' * 32))
        assert helper_result(tampered)['resource_state'] == 'applied'
        altered = observer.resources().replace(b'Xcursor.theme: Adwaita',
            b'Xcursor.theme: external')
        assert altered != observer.resources()
        observer.write_resources(altered)
        tampered.send(apply_line(run='b' * 32, sequence=2))
        tamper = helper_result(tampered)
        assert tamper['resource_state'] == 'conflict'
        tampered.eof(expected=0)
        assert observer.resources() == altered
        evidence['cases']['modified_owned_bytes_never_removed_or_restored'] = tamper

        # Force death leaves an actual helper-generated orphan, never forged bytes.
        observer.write_resources(baseline)
        orphan = child('wire-orphan-original')
        orphan.send(apply_line(run='8' * 32))
        assert helper_result(orphan)['resource_state'] == 'applied'
        orphan_bytes = observer.resources()
        old_owner = observer.owner()
        assert old_owner and observer.window_exists(old_owner)
        orphan.process.kill()
        code = orphan.process.wait(timeout=2)
        orphan.receipt['wait'] = dict(returncode=code, forced=True, intentional_orphan=True)
        assert observer.owner() == 0 and observer.resources() == orphan_bytes
        assert not observer.window_exists(old_owner)

        # X servers recycle a disconnected client's resource base. A freshly
        # opened helper can therefore recreate the orphan's owner XID itself;
        # the helper correctly refuses that now-live ID. Reserve the old base
        # with an independent connection and prove both branches explicitly.
        # Every connection/window here belongs to this disposable X server.
        reserved, created = None, []
        for connection_index in range(8):
            holder = connect_client(env['DISPLAY'])
            reservations.append(holder)
            windows = []
            created.append(dict(connection=connection_index, windows=windows))
            for _ in range(8):
                window = int(holder.x.XCreateSimpleWindow(holder.display,
                    holder.root, 0, 0, 1, 1, 0, 0, 0))
                windows.append(window)
                if window == old_owner:
                    reserved = holder
                    break
            holder.x.XSync(holder.display, 0)
            if reserved is not None:
                break
        assert reserved is not None, ('old resource base was not reserved',
            old_owner, created)
        assert observer.window_exists(old_owner)
        refused_orphan = child('wire-orphan-live-xid-refusal')
        refused_orphan.send(apply_line(run='d' * 32))
        live_refusal = helper_result(refused_orphan)
        assert live_refusal['readiness'] == 'ready'
        assert live_refusal['resource_state'] == 'conflict'
        assert live_refusal['reason'] == 'resource-conflict'
        assert observer.owner() != old_owner and observer.window_exists(old_owner)
        assert observer.resources() == orphan_bytes
        refused_orphan.eof(expected=0)
        assert observer.owner() == 0 and observer.resources() == orphan_bytes
        evidence['cases']['reused_live_orphan_owner_xid_refused'] = dict(
            result=live_refusal, old_owner=old_owner, reservations=created,
            exact_orphan_bytes_preserved=True)

        # Keep the connection open after destroying its exact reused window.
        # Its resource base remains allocated, preventing the next helper from
        # making the old owner XID live again during the adoption transaction.
        reserved.x.XDestroyWindow(reserved.display, old_owner)
        reserved.x.XSync(reserved.display, 0)
        assert not observer.window_exists(old_owner)
        adopted = child('wire-orphan-adopt')
        adopted.send(apply_line(run='9' * 32))
        adoption = helper_result(adopted)
        assert adoption['readiness'] == 'ready' and adoption['resource_state'] == 'applied'
        new_owner = observer.owner()
        assert new_owner and new_owner != old_owner
        assert not observer.window_exists(old_owner)
        assert b'8' * 32 not in observer.resources()
        adopted.eof(expected=0)
        assert observer.resources() == baseline
        evidence['cases']['actual_dead_owner_orphan_adoption'] = dict(
            result=adoption, old_owner=old_owner, new_owner=new_owner,
            old_owner_dead_and_resource_base_reserved=True)

        huge = b'! bounded external resource\n' + b'a' * 65510
        assert len(huge) == 65538 and len(huge) > 65536
        observer.write_resources(huge)
        limited = child('wire-resource-limit')
        limited.send(apply_line(run='a' * 32))
        limit = helper_result(limited)
        assert limit['readiness'] == 'ready' and limit['resource_state'] == 'conflict'
        assert limit['reason'] == 'resource-limit'
        assert observer.resources(65540) == huge
        limited.eof(expected=0)
        assert observer.resources(65540) == huge
        evidence['cases']['resource_size_limit_independent_xsettings'] = dict(
            result=limit, root_bytes=len(huge), maximum_bytes=65536)
    finally:
        try:
            for peer in reversed(children):
                peer.close()
        finally:
            try:
                for holder in reversed(reservations):
                    holder.close()
            finally:
                observer.release()
                observer.write_resources(None)


def module_records(native, executable):
    found = []
    paths = list(native.glob('module-*.json'))
    assert len(paths) <= 256
    for path in paths:
        value = json.loads(path.read_text())
        if value['real'] == str(executable):
            identity = child_identity(value['child'], value['proxy'])
            if identity:
                found.append(dict(value, identity=identity))
    assert len(found) <= 1, ('duplicate owned native module', found)
    return found[0] if found else None


def helper_children(desktop, helper):
    """Inspect only direct children of the observed exact owned Desktop."""
    if process_identity(desktop['child']) != {k: desktop['identity'][k]
            for k in ('child', 'parent', 'starttime')}:
        return []
    import base64
    wanted = base64.b64encode(os.fsencode(helper)).decode()
    found, pids = [], set()
    # A Go process may create its child from a non-main OS thread.
    for task in Path('/proc/%d/task' % desktop['child']).iterdir():
        try:
            pids.update(map(int, (task / 'children').read_text().split()))
        except FileNotFoundError:
            continue
    assert len(pids) <= 128
    for pid in pids:
        identity = child_identity(pid, desktop['child'])
        if identity and identity['argv_bytes'] == [wanted]:
            found.append(identity)
    assert len(found) <= 1, ('multiple actual native helper children', found)
    return found


def wait_retired(identity, core=None):
    def gone():
        current = process_identity(identity['child'])
        # A retained zombie has not been reaped and cannot pass this check.
        return dict(old=identity, now=current, exact_process_reaped_or_pid_reused=True) \
            if current is None or current['starttime'] != identity['starttime'] else None
    return wait_for_predicate('exact direct helper actual reap', gone,
        time.monotonic() + 6, process=core)


def toolkit_environment(args, env, evidence):
    result = dict(env)
    if args.toolkit_env:
        raw = args.toolkit_env.read_bytes()
        assert len(raw) <= 65536
        receipt = json.loads(raw)
        overrides = receipt.get('environment', receipt.get('env', receipt))
        assert isinstance(overrides, dict) and len(overrides) <= 64
        assert all(isinstance(k, str) and isinstance(v, str) and '\0' not in k + v
            for k, v in overrides.items())
        # Never redirect the producer away from this owned X server/root.
        assert not set(overrides).intersection(('DISPLAY', 'HOME', 'FVWM_USERDIR',
            'FVWMMFL_SOCKET_PATH', 'XAUTHORITY'))
        result.update(overrides)
        evidence['toolkit_env'] = dict(path=str(args.toolkit_env),
            sha256=hashlib.sha256(raw).hexdigest(), overrides=overrides)
    result['GDK_BACKEND'] = 'x11'
    result['GSETTINGS_BACKEND'] = 'memory'
    for key in ('XCURSOR_THEME', 'XCURSOR_SIZE', 'XENVIRONMENT'):
        result.pop(key, None)
    # GTK's runtime MIME database is not a production Desktop dependency.
    result['XDG_DATA_DIRS'] = result.get('XDG_DATA_DIRS', '') + ':/usr/local/share:/usr/share'
    return result


def xcursor_probe():
    assert sys.platform == 'linux', 'controller Xcursor probe requires Linux'
    """Fresh real libXcursor consumer, before any display resource cache exists."""
    client = connect_client(os.environ['DISPLAY'])
    try:
        library = C.CDLL('libXcursor.so.1')
        library.XcursorGetTheme.argtypes = [C.c_void_p]
        library.XcursorGetTheme.restype = C.c_char_p
        library.XcursorGetDefaultSize.argtypes = [C.c_void_p]
        library.XcursorGetDefaultSize.restype = C.c_int
        theme = library.XcursorGetTheme(client.display)
        paths = sorted({line.rsplit(None, 1)[-1]
            for line in Path('/proc/self/maps').read_text().splitlines()
            if '/' in line and 'libXcursor.so' in line})
        return dict(theme=theme.decode() if theme else None,
            size=library.XcursorGetDefaultSize(client.display), pid=os.getpid(),
            libraries={p: digest(p) for p in paths})
    finally:
        client.close()


def fresh_xcursor(env, out, evidence, name, theme, size):
    fixture = Path(__file__).resolve()
    argv = [sys.executable, str(fixture), '--xcursor-probe']
    clean_env = dict(env)
    for key in ('XCURSOR_THEME', 'XCURSOR_SIZE', 'XENVIRONMENT'):
        clean_env.pop(key, None)
    result = subprocess.run(argv, env=clean_env, capture_output=True, timeout=6)
    (out / (name + '.stdout')).write_bytes(result.stdout)
    (out / (name + '.stderr')).write_bytes(result.stderr)
    receipt = dict(argv=argv, returncode=result.returncode,
        stdout=name + '.stdout', stderr=name + '.stderr', actual_wait=True)
    evidence.setdefault('xcursor_probes', []).append(receipt)
    assert result.returncode == 0, receipt
    assert len(result.stdout) <= 16384
    value = json.loads(result.stdout)
    receipt['result'] = value
    assert value['theme'] == theme and value['size'] == size, value
    return value


def gtk_state(peer):
    peer.send(b'{"verb":"state"}\n')
    value = json.loads(peer.line())
    assert value['pid'] == peer.process.pid and value['gtk'][0] == 3
    return value


def wait_gtk(peer, values):
    expected = {GTK_KEYS[k]: bool(v) if k == 'Gtk/EnableAnimations' else v
        for k, v in values.items()}
    def observed():
        state = gtk_state(peer)
        return state if state['mapped'] and state['xid'] and all(
            state['values'].get(k) == v for k, v in expected.items()) else None
    return wait_for_predicate('actual GTK3 consumer values', observed,
        time.monotonic() + 8)


def controller_cases(args, profile, runtime, env, out, evidence, observer, native):
    assert sys.platform == 'linux', 'controller/toolkit observations require Linux'
    source, build = Path(__file__).resolve().parents[2], args.build
    modules, user = runtime.root / 'modules', runtime.root / 'user'
    for path in (modules, user, native):
        path.mkdir(mode=0o700, exist_ok=True)
    fixture = Path(__file__).resolve()
    for directory in (build / 'modules').iterdir():
        executable = directory / directory.name
        if executable.is_file() and directory.name not in ('FvwmForm', 'FvwmMFL'):
            (modules / directory.name).symlink_to(executable)
    for name, executable in [('FvwmForm', args.form), ('FvwmMFL', args.mfl),
            ('FvwmDesktop', args.desktop)]:
        wrapper = modules / name
        wrapper.write_text('#!/bin/sh\nexec ' + shlex.join([sys.executable,
            str(fixture), '--module-proxy', str(executable), str(native)]) +
            ' "$@" 2>>' + shlex.quote(str(runtime.root / (name + '.log'))) + '\n')
        wrapper.chmod(0o700)
    assets = source / 'default-config'
    palette, appearance = ('dune', 'light') if profile == 'dune-light' else ('slate', 'dark')
    local = user / 'local.config'
    local.write_text('*FvwmDesktop: palette ' + palette +
        '\n*FvwmDesktop: appearance ' + appearance +
        '\n*FvwmDesktop: transparency Off\n')
    state_path = user / 'desktop-state.json'
    # Valid previous schema, loaded read-only with off/inherit defaults.
    old = dict(schema_version=8, palette=palette, appearance=appearance,
        day='07:00', night='19:00', transparency=False, motion=False,
        keep_wallpaper=False, launcher_opacity=94, wallpaper_palette='follow',
        wallpaper_appearance='follow', contrast='normal', session_bus='off',
        session_notifications='off', notification_dnd=False, session_power='portable',
        tray_enabled=False, tray_gap=4, tray_padding=4, tray_orientation='horizontal',
        tray_modern=False, tray_icon_size=24,
        wallpaper=dict(palette=palette, appearance=appearance))
    state_path.write_text(json.dumps(old) + '\n')
    state_path.chmod(0o600)
    old_bytes = state_path.read_bytes()
    body = (assets / 'config').read_text().replace(
        '$[FVWM_DATADIR]/default-config', str(assets))
    if profile == 'classic-large':
        body = body.replace('Sans:size=10:antialias=True',
            'DejaVu Sans Mono:size=18:antialias=True')
        body += ('\n*FvwmFormDefault: Fore wheat\n*FvwmFormDefault: Back navy\n'
            '*FvwmFormDefault: ItemFore black\n*FvwmFormDefault: ItemBack ivory\n')
    config = runtime.root / 'config'
    config.write_text('ModulePath ' + str(modules) +
        '\nKillModule FvwmMFL\nSchedule 1000 Module FvwmMFL\nRead ' +
        str(source / 'fvwm/ConfigFvwmDefaults') + '\n' + body)
    for name, path in [('configuration.fvwm', config), ('local.config', local),
            ('old-state.json', state_path), ('default.config', assets / 'config'),
            ('desktop.fvwm', assets / 'desktop.fvwm'), ('palettes.json', assets / 'palettes.json')]:
        shutil.copy2(path, out / name)
        evidence.setdefault('configuration_sha256', {})[name] = digest(path)
    env.update(FVWM_USERDIR=str(user), FVWM_DESKTOP_ROOT=str(assets),
        FVWMMFL_SOCKET_PATH=str(runtime.root), FVWM_DESKTOP_WORKER=str(args.worker))
    if args.installed_helper_lookup:
        env.pop('FVWM_NATIVE_SETTINGS_HELPER', None)
    else:
        env['FVWM_NATIVE_SETTINGS_HELPER'] = str(args.helper)
    evidence['helper_lookup'] = dict(
        mode='installed-default' if args.installed_helper_lookup else 'absolute-override',
        desktop=str(args.desktop), expected_helper=str(args.helper),
        override_environment_present='FVWM_NATIVE_SETTINGS_HELPER' in env)
    core = runtime.start('core', [str(args.fvwm), '-d', env['DISPLAY'],
        '-f', str(config)], 'core.log', env)
    control = peer = None
    portal = None
    toolkits = []
    owned_helpers = {}
    try:
        if args.portal_helper:
            from settings_portal_live import start_cfg_probe
            portal = start_cfg_probe(args, runtime, env, out, evidence)
        endpoint = runtime.root / ('fvwm_mfl_' + env['DISPLAY'].replace(':', '_') + '.sock')
        def connected():
            module = module_records(native, args.mfl)
            if module is None:
                return None
            connections = []
            try:
                for _ in range(2):
                    connection = Connection(endpoint)
                    connections.append(connection)
                    connection.socket.settimeout(.5)
                    pid, uid, gid = struct.unpack('3i', connection.socket.getsockopt(
                        socket.SOL_SOCKET, socket.SO_PEERCRED, struct.calcsize('3i')))
                    assert pid == module['child']
                    assert 'connection_profile' in connection.record()
                connections[0].send(b'set echo\n')
                connections[0].echo('native-settings-ready')
                for connection in connections:
                    connection.socket.settimeout(8)
                return (*connections, dict(module, peer_pid=pid, peer_uid=uid, peer_gid=gid))
            except (OSError, AssertionError):
                for connection in connections:
                    connection.close()
                return None
        control, peer, mfl = wait_for_predicate('actual proxied MFL peer', connected,
            time.monotonic() + 15, process=core)
        evidence['mfl'] = mfl
        serial = 0

        def request(method='query', success=True, **fields):
            nonlocal serial
            serial += 1
            token = 'native-preferences-%d' % serial
            raw = frame(token, method=method, resource='settings', **fields)
            peer.send(raw)
            result = peer.record()['reply']
            assert result['id'] == token
            assert (result['status'] == 'ok') == success, result
            evidence.setdefault('requests', []).append(dict(request=json.loads(
                raw.decode().removeprefix('request ')), reply=result))
            assert len(evidence['requests']) <= 256
            return result['result'] if success else result['error']['code']

        def query():
            return request(scope='group:application')

        def ready_query():
            nonlocal serial
            serial += 1
            token = 'native-start-%d' % serial
            peer.send(frame(token, resource='settings', scope='group:application'))
            result = peer.record()['reply']
            assert result['id'] == token
            return result.get('result') if result['status'] == 'ok' else None
        snapshot = wait_for_predicate('actual settings provider', ready_query,
            time.monotonic() + 15, process=core)

        def observation(value):
            fields = value['settings']
            assert len(fields) == 5
            shared = value['application_defaults']
            defaults = value['setting_defaults']
            assert isinstance(shared, dict) and isinstance(defaults, dict)
            keys = ('readiness', 'resource_state', 'native_run',
                'native_sequence', 'xserial', 'failure_reason')
            for key in keys:
                assert key in shared, (key, shared)
            for item in fields.values():
                assert item['group'] == 'application'
                assert all(key not in item for key in keys), item
            # Public IPC inherits shared metadata; standalone native text
            # records remain self-contained. Explicit row metadata wins.
            state = {key: value[key] for key in ('owner', 'generation',
                'preview', 'persistence_error', 'available', 'capability')}
            state.update(defaults)
            state.update(shared)
            state.update(fields['native_settings_enabled'])
            return state

        def wait_readiness(wanted):
            def observed():
                result = query()
                return result if observation(result)['readiness'] in wanted else None
            return wait_for_predicate('actual native readiness ' + str(wanted),
                observed, time.monotonic() + 8, process=core)

        def change(values, current=None):
            current = current or query()
            return request('action', operation='set', provider=current['provider'],
                generation=current['generation'], values=values)

        def desktop_record():
            return wait_for_predicate('actual Desktop module child',
                lambda: module_records(native, args.desktop),
                time.monotonic() + 5, process=core)

        desktop = desktop_record()
        assert observation(snapshot)['readiness'] == 'off'
        assert snapshot['settings']['native_settings_enabled']['saved'] is False
        assert all(snapshot['settings'][key]['effective'] == value for key, value in
            [('application_font', ''), ('icon_theme', ''),
             ('cursor_theme', ''), ('cursor_size', 0)])
        assert helper_children(desktop, args.helper) == [] and observer.owner() == 0
        assert state_path.read_bytes() == old_bytes
        evidence['cases']['old_schema_off_inherit_no_write'] = snapshot

        def exact_helper():
            values = helper_children(desktop_record(), args.helper)
            if len(values) == 1:
                owned_helpers[values[0]['child']] = values[0]
                return values[0]
            return None

        baseline = b'Fixture.keep: controller-preserved\nXcursor.size: 17\n'
        observer.write_resources(baseline)
        toolkit_env = toolkit_environment(args, env, evidence)
        def toolkit(name):
            value = PipeChild([sys.executable, fixture.with_name('native_settings_gtk_probe.py')],
                toolkit_env, out, name, evidence)
            toolkits.append(value)
            return value
        existing = toolkit('gtk-existing')
        try:
            initial_gtk = wait_for_predicate('existing actual GTK3 mapped window',
                lambda: (lambda v: v if v['mapped'] and v['xid'] else None)(gtk_state(existing)),
                time.monotonic() + 8)
        except BaseException:
            evidence['toolkit_capability'] = (
                'unavailable or failed real GTK3 startup; see gtk-existing.stderr')
            raise
        evidence['gtk_existing_before'] = initial_gtk
        values = dict(native_settings_enabled=True, application_font='Sans 11',
            icon_theme='Adwaita', cursor_theme='Adwaita', cursor_size=24, motion=False)
        committed = change(values, snapshot)
        ready = wait_readiness({'ready'})
        helper = wait_for_predicate('one direct C helper child', exact_helper,
            time.monotonic() + 4, process=core)
        if args.installed_helper_lookup:
            assert 'FVWM_NATIVE_SETTINGS_HELPER' not in env
            evidence['cases']['installed_default_helper_lookup'] = dict(
                desktop=desktop, helper=helper, override_environment_present=False)
            evidence['limits']['installed_helper_lookup'] = (
                'passed installed default lookup; exact direct native child observed')
        published = wait_snapshot(observer, SETTINGS, core)
        if portal:
            portal.wait_value(1, public=True, source=dict(
                kind='actual-Desktop-CFG-motion-false', native_helper=helper,
                x_owner=published['owner'], x_serial=published['serial']))
        assert observation(ready)['xserial'] == published['serial']
        assert observer.lookup() == {'Fixture.keep': 'controller-preserved',
            'Xcursor.theme': 'Adwaita', 'Xcursor.size': '24'}
        first_cursor = fresh_xcursor(env, out, evidence, 'xcursor-before',
            'Adwaita', 24)
        gtk_before = wait_gtk(existing, SETTINGS)
        assert json.loads(state_path.read_text())['schema_version'] == 9
        evidence['cases']['actual_controller_admission_publication'] = dict(
            committed=committed, ready=ready, helper=helper, publication=published,
            xrm=observer.lookup(), xcursor=first_cursor, gtk=gtk_before)

        # Public MFL strings share the native 0..128-byte data boundary.
        # These names deliberately need not exist on disk: this checks typed
        # admission and exact publication, not toolkit installation/fallback.
        boundary_values = dict(application_font='F' * 128, icon_theme='i' * 128,
            cursor_theme='c' * 128)
        boundary_settings = {**SETTINGS, 'Gtk/FontName': 'F' * 128,
            'Net/IconThemeName': 'i' * 128, 'Gtk/CursorThemeName': 'c' * 128}

        def observed_boundary(wanted, publication):
            def observed():
                current = query()
                state = observation(current)
                fields = current['settings']
                matched = all(fields[key]['saved'] == value and
                    fields[key]['effective'] == value for key, value in wanted.items())
                return current if matched and state['readiness'] == 'ready' and (
                    state['xserial'] == publication['serial']) else None
            result = wait_for_predicate('public boundary saved/effective/native acknowledgment',
                observed, time.monotonic() + 6, process=core)
            persisted = json.loads(state_path.read_text())
            assert all(persisted[key] == value for key, value in wanted.items())
            return result

        # Per-string admission does not expand the existing 384-byte MFL
        # settings payload. The combined maximum strings must still refuse
        # atomically; submit each valid maximum through its current generation.
        initial_saved, initial_resources = state_path.read_bytes(), observer.resources()
        initial_publication, initial_query = observer.snapshot(), query()
        combined_code = request('action', success=False, operation='set',
            provider=initial_query['provider'], generation=initial_query['generation'],
            values=boundary_values)
        assert combined_code == 'request_too_large', combined_code
        assert state_path.read_bytes() == initial_saved
        assert observer.snapshot() == initial_publication
        assert observer.resources() == initial_resources
        refused_query = query()
        assert refused_query['generation'] == initial_query['generation']
        assert all(refused_query['settings'][key][field] == item[field]
            for key, item in initial_query['settings'].items()
            for field in ('saved', 'effective'))
        single_key_actions = []
        for key, value in boundary_values.items():
            committed_boundary = change({key: value})
            assert committed_boundary['settings'][key]['saved'] == value
            assert committed_boundary['settings'][key]['effective'] == value
            assert json.loads(state_path.read_text())[key] == value
            single_key_actions.append(dict(key=key, result=committed_boundary))
        boundary_publication = wait_snapshot(observer, boundary_settings, core)
        boundary_query = observed_boundary(boundary_values, boundary_publication)
        boundary_state = observation(boundary_query)
        assert boundary_state['resource_state'] == 'applied'
        marker = ('%s %08x 0\n' % (boundary_state['native_run'],
            boundary_publication['owner'])).encode('ascii')
        expected_resources = (baseline + b'! FVWM_NATIVE_SETTINGS FNS1 BEGIN ' + marker +
            b'Xcursor.theme: ' + b'c' * 128 + b'\nXcursor.size: 24\n' +
            b'! FVWM_NATIVE_SETTINGS FNS1 END ' + marker)
        assert observer.resources() == expected_resources
        assert observer.lookup() == {'Fixture.keep': 'controller-preserved',
            'Xcursor.theme': 'c' * 128, 'Xcursor.size': '24'}

        empty_values = dict(application_font='', icon_theme='', cursor_theme='', cursor_size=0)
        change(empty_values)
        empty_publication = wait_snapshot(observer, {'Gtk/EnableAnimations': 0}, core)
        empty_query = observed_boundary(empty_values, empty_publication)
        assert observation(empty_query)['resource_state'] == 'inherited'
        assert observer.resources() == baseline
        assert observer.lookup() == {'Fixture.keep': 'controller-preserved', 'Xcursor.size': '17'}

        restore_values = dict(application_font='Sans 11', icon_theme='Adwaita',
            cursor_theme='Adwaita', cursor_size=24)
        change(restore_values)
        restored_publication = wait_snapshot(observer, SETTINGS, core)
        restored_query = observed_boundary(restore_values, restored_publication)
        assert observation(restored_query)['resource_state'] == 'applied'
        gtk_before = wait_gtk(existing, SETTINGS)
        evidence['cases']['public_128_byte_and_empty_preference_boundaries'] = dict(
            combined_maximum_refused=dict(code=combined_code,
                generation_unchanged=True, saved_and_effective_unchanged=True,
                publication_and_resources_unchanged=True, payload_limit_bytes=384),
            maximum=dict(values=boundary_values, query=boundary_query,
                single_key_actions=single_key_actions,
                publication=boundary_publication,
                exact_resource_sha256=hashlib.sha256(expected_resources).hexdigest()),
            empty=dict(values=empty_values, query=empty_query, publication=empty_publication,
                exact_resources_restored=True),
            restored=dict(query=restored_query, publication=restored_publication, gtk=gtk_before),
            toolkit_scope='restored installed preferences observed; 128-byte names are data only')

        saved, raw_resources = state_path.read_bytes(), observer.resources()
        before = observer.snapshot()
        invalid = [dict(cursor_size=-1), dict(cursor_size=129),
            dict(cursor_size='24'), dict(icon_theme='../not-a-theme'),
            dict(application_font='bad\nfont'), dict(application_font='x' * 129),
            dict(cursor_theme='.')]
        refusals = []
        for invalid_values in invalid:
            current = query()
            code = request('action', success=False, operation='set',
                provider=current['provider'], generation=current['generation'],
                values=invalid_values)
            expected_code = ('invalid_request' if invalid_values.get('application_font') ==
                'x' * 129 else 'invalid_value')
            assert code == expected_code, (invalid_values, code)
            assert state_path.read_bytes() == saved
            assert observer.snapshot() == before and observer.resources() == raw_resources
            refusals.append(dict(values=invalid_values, code=code,
                layer='MFL transport' if expected_code == 'invalid_request' else 'Go value policy'))
        stale = request('action', success=False, operation='set',
            provider=ready['provider'], generation=snapshot['generation'],
            values={'application_font': 'Sans 19'})
        assert stale == 'stale_generation'
        assert state_path.read_bytes() == saved and observer.snapshot() == before
        current = query()
        change({'application_font': 'Sans 11'}, current)
        unchanged = wait_readiness({'ready'})
        assert observer.snapshot() == before and observer.resources() == raw_resources
        evidence['cases']['invalid_save_stale_target_and_noop_unchanged'] = dict(
            refusals=refusals, stale=stale, noop=unchanged,
            direct_go_129_byte_validation='not run here; separate lead-owned Go unit gate')

        changed = {**SETTINGS, 'Gtk/FontName': 'Sans 22',
            'Net/IconThemeName': 'hicolor', 'Gtk/CursorThemeName': 'default',
            'Gtk/CursorThemeSize': 32, 'Gtk/EnableAnimations': 1}
        change(dict(application_font='Sans 22', icon_theme='hicolor',
            cursor_theme='default', cursor_size=32, motion=True))
        publication = wait_snapshot(observer, changed, core)
        if portal:
            portal.wait_value(0, public=True, source=dict(
                kind='actual-Desktop-CFG-motion-true', native_helper=helper,
                x_owner=publication['owner'], x_serial=publication['serial']))
        gtk_after = wait_gtk(existing, changed)
        old_notifications = len(gtk_before['notifications'])
        notified = {row['property'] for row in gtk_after['notifications'][old_notifications:]}
        assert notified == set(GTK_KEYS.values()), notified
        assert gtk_after['font_sample_natural'][1] > gtk_before['font_sample_natural'][1]
        fresh = toolkit('gtk-fresh')
        fresh_state = wait_gtk(fresh, changed)
        second_cursor = fresh_xcursor(env, out, evidence, 'xcursor-after',
            'default', 32)
        for path in {p for s in (gtk_before, gtk_after, fresh_state)
                for p in s['loaded_libraries']}:
            evidence.setdefault('toolkit_library_sha256', {})[path] = digest(path)
        evidence['cases']['gtk_existing_notifications_fresh_and_enlarged_sample'] = dict(
            before=gtk_before, after=gtk_after, fresh=fresh_state, publication=publication,
            cursor_resources=observer.lookup(), xcursor=second_cursor,
            toolkit_scope='GTK3 settings and fresh libXcursor resource consumption only')

        view = NativeView(observer.client, observer.errors, core)
        control.send(b'SendToModule FvwmDesktop edit-settings group:application\n')
        window = view.wait('Application preferences')
        # GrabFocusOff leaves selection to the user. Select the actual mapped
        # editor through the shared command engine before observing its focus;
        # do not change the product policy or resize/repaint for capture.
        focus_commands = ['WindowId %d Raise' % window,
            'WindowId %d Focus NoWarp' % window]
        focus_witness = dict(target=window, commands=focus_commands, status='submitted')
        evidence['native_editor_focus'] = focus_witness
        control.send(('\n'.join(focus_commands) + '\n').encode())
        control.echo('native-application-editor-selected')

        def editor_focused():
            actual = view.focus()
            focus_witness.update(actual_focus=actual, observed_at=time.monotonic())
            if actual == window:
                focus_witness['status'] = 'focus-observed'
                return True
            return False

        wait_for_predicate('actual application editor focused', editor_focused,
            time.monotonic() + 6, process=core)
        published_rows = wait_for_predicate('actual native application Form rows', lambda:
            [r for r in rows(native) if r['direction'] == 'out' and
                'Application preferences' in r['text']], time.monotonic() + 6, process=core)
        assert any('Title' in r['text'] for r in published_rows)
        before_cancel = state_path.read_bytes()
        evidence['native_pixels'] = view.pixels(window)
        capture = subprocess.run([sys.executable, str(fixture), '--capture',
            str(args.capture), str(out)], env=env, capture_output=True,
            text=True, timeout=40)
        assert capture.returncode == 0, capture.stderr
        image = Path(json.loads(capture.stdout))
        image.replace(out / 'application-preferences.png')
        evidence['screenshots']['application-preferences.png'] = dict(
            sha256=digest(out / 'application-preferences.png'),
            window=window, geometry=list(observer.client.geometry(window)),
            visual_inspection='pending separate reviewer')
        view.key('Escape')
        wait_for_predicate('editor cancelled', lambda: not view.named('Application preferences'),
            time.monotonic() + 6, process=core)
        assert state_path.read_bytes() == before_cancel
        evidence['cases']['native_application_editor_cancel'] = dict(
            actual_rows=published_rows, saved_bytes_unchanged=True)

        # Disable must remove publication and reap the exact direct native child.
        if portal:
            change({'motion': False})
            portal_publication = wait_snapshot(observer,
                {**changed, 'Gtk/EnableAnimations': 0}, core)
            portal.wait_value(1, public=True, source=dict(
                kind='actual-Desktop-CFG-before-owner-retirement',
                native_helper=helper, x_owner=portal_publication['owner'],
                x_serial=portal_publication['serial']))
        change({'native_settings_enabled': False})
        off = wait_readiness({'off'})
        retired = wait_retired(helper, core)
        assert helper_children(desktop_record(), args.helper) == []
        assert observer.owner() == 0 and observer.resources() == baseline
        if portal:
            portal.wait_value(0, public=True, source=dict(
                kind='actual-native-owner-retired',
                retirement=retired, x_owner=0,
                meaning='no preference; not an application animation claim'))
            change({'motion': True})
        evidence['cases']['direct_child_disable_eof_cleanup_reap'] = dict(off=off,
            retirement=retired, actual_wait_owner='Go supervisor; kernel disappearance observed')

        # A competing real owner is never stolen; explicit retry after its exit
        # uses a fresh run only after the previous direct child was reaped.
        contender = observer.claim()
        change({'native_settings_enabled': True})
        conflict = wait_readiness({'conflict'})
        assert observer.owner() == contender and observer.resources() == baseline
        failed_run = observation(conflict)['native_run']
        wait_for_predicate('conflicting direct helper retired',
            lambda: not helper_children(desktop_record(), args.helper),
            time.monotonic() + 5, process=core)
        quiet_samples = []
        for index in range(3):
            # Cross several real controller ticks before allowing explicit retry.
            select.select([], [], [], .35)
            control.echo('no-helper-restart-%d' % index)
            assert not helper_children(desktop_record(), args.helper)
            quiet = observation(query())
            assert quiet['native_run'] == failed_run
            quiet_samples.append(dict(time=time.monotonic(), state=quiet))
        observer.release()
        control.send(b'SendToModule FvwmDesktop retry-native-settings\n')
        retried = wait_readiness({'ready'})
        assert observation(retried)['native_run'] != failed_run
        helper2 = wait_for_predicate('retried actual direct helper', exact_helper,
            time.monotonic() + 4, process=core)
        wait_snapshot(observer, changed, core)
        evidence['cases']['controller_owner_conflict_no_loop_explicit_retry'] = dict(
            conflict=conflict, retry=retried, helper=helper2,
            no_automatic_restart_samples=quiet_samples)

        # Actual Desktop restart with saved intent; same core/MFL, new owner/run.
        old_desktop = desktop_record()
        old_run = observation(retried)['native_run']
        control.send(b'KillModule FvwmDesktop\n')
        waited_path = native / ('stopped-%d.json' % old_desktop['proxy'])
        waited = wait_for_predicate('actual Desktop native Wait', lambda:
            json.loads(waited_path.read_text()) if waited_path.exists() else None,
            time.monotonic() + 6, process=core)
        assert waited['child'] == old_desktop['child'] and waited['returncode'] is not None
        retired2 = wait_retired(helper2, core)
        assert observer.owner() == 0 and observer.resources() == baseline
        control.send(b'ModuleSynchronous Timeout 15 FvwmDesktop\n')
        wait_for_predicate('replacement Desktop module', lambda:
            (lambda value: value if value and value['child'] != old_desktop['child'] else None)(
                module_records(native, args.desktop)), time.monotonic() + 8, process=core)
        restarted = wait_readiness({'ready'})
        assert observation(restarted)['native_run'] != old_run
        helper3 = wait_for_predicate('replacement direct C helper', exact_helper,
            time.monotonic() + 4, process=core)
        assert restarted['provider'] != retried['provider']
        wait_snapshot(observer, changed, core)
        assert wait_gtk(existing, changed)['values'] == gtk_after['values']
        change({'native_settings_enabled': False})
        wait_readiness({'off'})
        retired3 = wait_retired(helper3, core)
        assert observer.owner() == 0 and observer.resources() == baseline
        evidence['cases']['actual_desktop_restart_saved_intent_new_run'] = dict(
            desktop_wait=waited, old_helper_retirement=retired2,
            restarted=restarted, final_helper_retirement=retired3)
        evidence['toolkit_capability'] = (
            'passed actual existing/fresh GTK3 property consumption and notifications')
    finally:
        for toolkit in reversed(toolkits):
            toolkit.close()
        for connection in (peer, control):
            if connection:
                connection.close()
        core.close(time.monotonic() + 6)
        # Only the exact direct children discovered under our Desktop may be
        # touched here. Successful normal cases already observed their reap.
        for identity in owned_helpers.values():
            current = process_identity(identity['child'])
            if current and current['starttime'] == identity['starttime']:
                evidence.setdefault('cleanup_remaining_helpers', []).append(current)
                if child_identity(identity['child'], identity['parent']) == identity:
                    os.kill(identity['child'], signal.SIGTERM)
                wait_retired(identity)
        evidence['owned_helpers'] = list(owned_helpers.values())
        if portal:
            portal.close()


def run(args, profile):
    assert sys.platform == 'linux' or (
        sys.platform.startswith(('freebsd', 'openbsd')) and args.groups == ['wire'])
    source = Path(__file__).resolve().parents[2]
    out = args.artifacts_dir / profile
    out.mkdir(parents=True, exist_ok=False)
    evidence = dict(status='running', profile=profile,
        execution_profile=args.execution_profile, groups=args.groups,
        platform=sys.platform, observed_os=dict(zip(
            ('sysname', 'nodename', 'release', 'version', 'machine'), os.uname())),
        cases={}, screenshots={}, cleanup_failures=[],
        toolkit_capability=('pending actual runtime' if 'controller' in args.groups
            else 'not run; controller group omitted'),
        captures=('pending actual native UI' if 'controller' in args.groups
            else 'not run; controller group omitted'),
        group_results={group: 'pending' if group in args.groups else 'skipped by --groups'
            for group in ('wire', 'controller')},
        limits=dict(native_cpu_acceptance=False, physical_hardware=False,
            non_gtk_qt_browser='unavailable; outside GTK3 consumer scope',
            cached_xcursor_client_effects='unavailable; fresh Xrm/libXcursor lookups only',
            serial_rollover='not run here; lead-owned pure-policy gate',
            oversized_controller_queue_and_late_retired_run='not run; lead-owned Go fault gate',
            sanitizers_and_race='not run; separate lead gate',
            six_target_release='not run; separate qualification',
            installed_helper_lookup=('pending actual installed default lookup'
                if args.installed_helper_lookup else
                'not run; explicit absolute actual helper override')))
    inputs = ['tests/live_x/native_settings_live.py',
        'tests/live_x/native_settings_gtk_probe.py',
        'tests/live_x/live_x_harness.py', 'tests/live_x/browser_handoff_live.py',
        'tests/live_x/colorset_live.py',
        'tests/live_x/native_module_proxy.py', 'tests/live_x/native_bindings_live.py',
        'tests/live_x/inspector_live.py', 'tests/live_x/session_x_errors.py',
        'tests/live_x/native_capture.py', 'tests/live_x/browser_continuity_live.py',
        'tests/live_x/mfl_request_live.py', 'tests/live_x/mfl_stream_live.py',
        'bin/native_settings.c', 'bin/native_settings_policy.c',
        'bin/native_settings_policy.h', 'bin/meson.build',
        'bin/FvwmPrompt/meson.build', 'bin/FvwmPrompt/cmd/fvwmdesktop/main.go',
        'modules/FvwmMFL/request.c']
    if args.portal_helper:
        inputs.extend(['tests/live_x/settings_portal_live.py',
            'bin/settings_portal.c', 'bin/settings_portal_policy.c',
            'bin/settings_portal_policy.h'])
    for directory in ('bin/FvwmPrompt/internal/desktopcontroller',
            'bin/FvwmPrompt/internal/desktopsettings'):
        inputs.extend(str(p.relative_to(source)) for p in sorted((source / directory).glob('*.go')))
    evidence['source_sha256'] = {p: digest(source / p) for p in inputs}
    binary_names = (('fvwm', 'mfl', 'form', 'desktop', 'worker', 'helper', 'capture')
        if 'controller' in args.groups else ('helper',))
    evidence['binary_inputs'] = {name: dict(path=str(getattr(args, name)),
        sha256=digest(getattr(args, name))) for name in binary_names}
    if 'controller' not in args.groups:
        evidence['controller_profiles_skipped'] = dict(profiles=args.profiles,
            reason='controller group omitted; wire executes only first selected profile')
    evidence['external_tools'] = {name: dict(path=str(path), sha256=digest(path))
        for name, path in [('python', Path(sys.executable).resolve()),
            ('Xvfb', Path(shutil.which('Xvfb')).resolve())]}
    runtime = HarnessRuntime(prefix='fvwm-native-settings-')
    observer = None
    native = runtime.root / 'native-records'
    try:
        _, display = start_display_server(runtime, 'Xvfb',
            ['Xvfb', '-screen', '0', '1440x1200x24', '-noreset'],
            'xserver.log', time.monotonic() + 10)
        env = runtime.environment({'DISPLAY': display,
            'HOME': str(runtime.root), 'XDG_DATA_HOME': str(runtime.root / 'data'),
            'XDG_DATA_DIRS': str(runtime.root / 'absent')})
        observer = XObserver(display, evidence)
        # Protocol cases run once per invocation, before any real Desktop.
        if 'wire' in args.groups:
            if profile == args.profiles[0]:
                wire_cases(args, env, out, evidence, observer)
                evidence['group_results']['wire'] = 'passed'
            else:
                evidence['wire_cases'] = 'not repeated; recorded under ' + args.profiles[0]
                evidence['group_results']['wire'] = ('not repeated; recorded under ' +
                    args.profiles[0])
        else:
            evidence['wire_cases'] = 'not run; wire group omitted'
        if 'controller' in args.groups:
            controller_cases(args, profile, runtime, env, out, evidence, observer, native)
            evidence['group_results']['controller'] = 'passed'
            evidence['captures'] = 'captured; separate visual review required'
        assert not evidence.get('observer_unexpected_x_errors')
        evidence['status'] = 'passed'
    except BaseException as failure:
        evidence.update(status='failed', failure=dict(type=type(failure).__name__,
            message=str(failure)[:2048]))
        raise
    finally:
        def clean(label, operation):
            try:
                operation()
            except BaseException as failure:
                evidence['status'] = 'failed'
                evidence['cleanup_failures'].append(dict(step=label,
                    error=repr(failure)[:2048]))
        # Keep the observer and error policy alive through all child teardown.
        for process in reversed(runtime.processes):
            if process.name != 'Xvfb':
                clean('stop ' + process.name,
                    lambda p=process: p.close(time.monotonic() + 5))
        if native.exists():
            clean('actual native module Wait receipts', lambda:
                evidence.update(module_cleanup=cleanup_receipts(native)))
        if observer:
            clean('X observer', observer.close)
        for process in reversed(runtime.processes):
            clean('stop ' + process.name,
                lambda p=process: p.close(time.monotonic() + 5))
        evidence['owned_processes'] = [dict(name=p.name, argv=p.argv,
            pid=p.process.pid, returncode=p.poll()) for p in runtime.processes]
        for path in runtime.root.iterdir():
            if path.is_dir() and path.name in ('native-records', 'user'):
                clean('copy ' + path.name,
                    lambda p=path: shutil.copytree(p, out / p.name))
            elif path.is_file() and path.suffix == '.log':
                clean('copy ' + path.name,
                    lambda p=path: shutil.copy2(p, out / p.name))
        if evidence.get('observer_unexpected_x_errors'):
            evidence['status'] = 'failed'
        clean('runtime', runtime.close)
        atomic_json(out / 'result.json', evidence)
    assert evidence['status'] == 'passed', evidence['cleanup_failures']
    print(json.dumps(dict(status='passed', profile=profile, artifacts=str(out))))


def main():
    if sys.platform != 'linux' and len(sys.argv) > 1 and sys.argv[1] in (
            '--xcursor-probe', '--module-proxy', '--capture'):
        raise SystemExit('controller helper modes require Linux')
    if len(sys.argv) == 2 and sys.argv[1] == '--xcursor-probe':
        print(json.dumps(xcursor_probe()))
        return 0
    if len(sys.argv) > 1 and sys.argv[1] == '--module-proxy':
        return module_proxy(sys.argv[2], sys.argv[3], sys.argv[4:])
    if len(sys.argv) == 4 and sys.argv[1] == '--capture':
        print(json.dumps(NativeCapture(sys.argv[2]).screenshot(
            sys.argv[3]).removeprefix('Screenshot saved: ')))
        return 0
    parser = argparse.ArgumentParser(description=__doc__, allow_abbrev=False)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--artifacts-dir', type=Path, required=True)
    parser.add_argument('--execution-profile', required=True,
        choices=('native-linux', 'emulated-linux', 'native-cpu', 'emulated-cpu'))
    parser.add_argument('--profiles', nargs='+', choices=PROFILES, default=list(PROFILES))
    parser.add_argument('--groups', nargs='+', choices=('wire', 'controller'),
        default=['wire', 'controller'],
        help='wire runs once; controller includes real GTK and native captures per profile')
    parser.add_argument('--installed-helper-lookup', action='store_true',
        help='unset helper override; requires absolute installed --desktop and --helper')
    parser.add_argument('--toolkit-env', type=Path,
        help='existing private GTK runtime environment JSON receipt')
    from settings_portal_live import portal_arguments, validate_portal_args
    portal_arguments(parser, prefix='portal-')
    defaults = dict(fvwm='fvwm3', mfl='modules/FvwmMFL/FvwmMFL',
        form='modules/FvwmForm/FvwmForm', desktop='bin/FvwmPrompt/FvwmDesktop',
        worker='bin/FvwmPrompt/fvwm-desktop-worker', helper='bin/fvwm-native-settings',
        capture='bin/fvwm-desktop-capture')
    for name in defaults:
        parser.add_argument('--' + name, type=Path)
    args = parser.parse_args()
    if args.portal_helper:
        if 'controller' not in args.groups:
            parser.error('--portal-helper requires the real controller group')
        portal_options = argparse.Namespace(**{
            **vars(args), 'helper': args.portal_helper})
        validate_portal_args(parser, portal_options, frontend=True)
        args.portal_helper = portal_options.helper
        for name in ('frontend', 'portal_env', 'portal_descriptor',
                'activation_service'):
            setattr(args, name, getattr(portal_options, name))
    elif any((args.frontend, args.portal_env, args.portal_descriptor,
            args.activation_service)):
        parser.error('portal options require explicit --portal-helper')
    if args.installed_helper_lookup:
        if 'controller' not in args.groups:
            parser.error('--installed-helper-lookup requires the controller group')
        if any(getattr(args, name) is None or not getattr(args, name).is_absolute()
                for name in ('desktop', 'helper')):
            parser.error('--installed-helper-lookup requires absolute --desktop and --helper')
    if sys.platform.startswith(('freebsd', 'openbsd')):
        if args.groups != ['wire']:
            parser.error('BSD supports only --groups wire; controller requires Linux')
        if args.execution_profile not in ('native-cpu', 'emulated-cpu'):
            parser.error('BSD requires --execution-profile native-cpu or emulated-cpu')
    elif sys.platform != 'linux':
        parser.error('requires an owning Linux, FreeBSD or OpenBSD executor')
    if not shutil.which('Xvfb'):
        parser.error('unavailable required actual Xvfb server')
    args.build, args.artifacts_dir = args.build.resolve(), args.artifacts_dir.resolve()
    for name, relative in defaults.items():
        path = (getattr(args, name) or args.build / relative).resolve()
        required = name == 'helper' or 'controller' in args.groups
        if required and (not path.is_file() or not os.access(path, os.X_OK)):
            parser.error('unavailable executable for ' + name + ': ' + str(path))
        setattr(args, name, path)
    if args.toolkit_env:
        args.toolkit_env = args.toolkit_env.resolve()
        if 'controller' in args.groups and not args.toolkit_env.is_file():
            parser.error('unavailable existing GTK environment receipt')
    if len(set(args.groups)) != len(args.groups):
        parser.error('group names must be unique')
    if len(set(args.profiles)) != len(args.profiles):
        parser.error('profile names must be unique')
    args.artifacts_dir.mkdir(parents=True, exist_ok=True)
    profiles = args.profiles if 'controller' in args.groups else args.profiles[:1]
    for profile in profiles:
        run(args, profile)
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
