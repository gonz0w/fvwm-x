#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or any later version.
"""Opt-in native file-transfer protocol checks with independent real X peers.

The socket peer stands in for Form and sends metadata, not human gestures.
Controller admit/complete commands simulate job admission/completion; this is
never browser, actual Form input, filesystem mutation or installed UI evidence.
Selection bytes and XDND replies come from real independent X11 clients and the
actual helper. No helper event/reply is fabricated. Linux/FreeBSD/OpenBSD only.
"""
import argparse
import base64
import ctypes as C
import hashlib
import json
import os
from pathlib import Path
import select
import shutil
import socket
import struct
import subprocess
import sys
import time

from geometry_transition_live import Client
from live_x_harness import HarnessRuntime, ManagedProcess, start_display_server
from selection_live import SelectionNotify, SelectionRequest
from session_x_errors import SessionXErrors

MAX_RECORD = 512 * 1024
MAX_TRACE = 8 * 1024 * 1024
WIRE_BYTES = 608
TOKEN = bytes.fromhex('7d3f4a19c60288e61599cb4a6725e301')
ATTACH, GENERATION, BEGIN, MOTION, RELEASE, CANCEL = range(1, 7)
CLIENT_MESSAGE, FEEDBACK, DETACH, ACTION = range(7, 11)


class Property(C.Structure):
    _fields_ = SelectionRequest._fields_[:4] + [
        ('window', C.c_ulong), ('atom', C.c_ulong), ('time', C.c_ulong),
        ('state', C.c_int)]


class MessageData(C.Union):
    _fields_ = [('b', C.c_char * 20), ('s', C.c_short * 10),
                ('l', C.c_long * 5)]


class Message(C.Structure):
    _fields_ = SelectionRequest._fields_[:4] + [
        ('window', C.c_ulong), ('message_type', C.c_ulong),
        ('format', C.c_int), ('data', MessageData)]


class Event(C.Union):
    _fields_ = [('type', C.c_int), ('request', SelectionRequest),
                ('selection', SelectionNotify), ('property', Property),
                ('message', Message), ('pad', C.c_long * 24)]


def digest(data):
    return hashlib.sha256(data).hexdigest()


def save(path, value):
    temporary = path.with_suffix('.tmp')
    temporary.write_text(json.dumps(value, indent=2) + '\n')
    temporary.replace(path)


class XPeer(Client):
    """Independent source, target and requestor; all bytes use X properties."""
    def __init__(self, display, run):
        super().__init__(display)
        self.run = run
        p, w, i = C.c_void_p, C.c_ulong, C.c_int
        signatures = {
            'XInternAtom': ([p, C.c_char_p, i], w),
            'XPending': ([p], i),
            'XFlush': ([p], i),
            'XNextEvent': ([p, C.POINTER(Event)], i),
            'XSendEvent': ([p, w, i, C.c_long, C.POINTER(Event)], i),
            'XChangeProperty': ([p, w, w, w, i, i, p, i], i),
            'XDeleteProperty': ([p, w, w], i),
            'XGetWindowProperty': ([p, w, w, C.c_long, C.c_long, i, w,
                C.POINTER(w), C.POINTER(i), C.POINTER(w), C.POINTER(w),
                C.POINTER(p)], i),
            'XSetSelectionOwner': ([p, w, w, w], i),
            'XGetSelectionOwner': ([p, w], w),
            'XConvertSelection': ([p, w, w, w, w, w], i),
            'XTranslateCoordinates': ([p, w, w, i, i, C.POINTER(i),
                C.POINTER(i), C.POINTER(w)], i),
        }
        for name, (arguments, result) in signatures.items():
            getattr(self.x, name).argtypes = arguments
            getattr(self.x, name).restype = result
        self.atoms, self.events, self.sources, self.incr = {}, [], {}, {}
        self.windows = []
        self.clock = self.create_window(0, 0, 1, 1, mapped=False)
        self.requestor = self.create_window(0, 0, 1, 1, mapped=False)
        self.source = self.create_window(750, 100, 100, 100)
        self.target = self.create_window(450, 100, 220, 180)
        self.clock_serial = 0

    def atom(self, name):
        if name not in self.atoms:
            self.atoms[name] = self.x.XInternAtom(
                self.display, name.encode('ascii'), 0)
        return self.atoms[name]

    def create_window(self, x, y, width, height, parent=None, mapped=True):
        window = self.x.XCreateSimpleWindow(self.display,
            self.root if parent is None else parent, x, y, width, height,
            0, 0, 0)
        assert window, 'XCreateSimpleWindow failed'
        self.windows.append(window)
        self.x.XSelectInput(self.display, window, (1 << 22) | (1 << 17))
        if mapped:
            self.x.XMapWindow(self.display, window)
        self.x.XSync(self.display, 0)
        return window

    def put(self, window, prop, kind, fmt, value):
        if fmt == 32:
            data = (C.c_ulong * len(value))(*value)
            length = len(value)
        else:
            assert fmt == 8 and isinstance(value, bytes)
            data, length = C.create_string_buffer(value), len(value)
        assert length <= MAX_RECORD
        self.x.XChangeProperty(self.display, window, prop, kind, fmt, 0,
            C.cast(data, C.c_void_p), length)
        self.x.XFlush(self.display)

    def get(self, window, prop, delete=False):
        kind, count, remaining = C.c_ulong(), C.c_ulong(), C.c_ulong()
        fmt, pointer = C.c_int(), C.c_void_p()
        status = self.x.XGetWindowProperty(self.display, window, prop, 0,
            MAX_RECORD // 4, int(delete), 0, C.byref(kind), C.byref(fmt),
            C.byref(count), C.byref(remaining), C.byref(pointer))
        try:
            assert status == 0 and remaining.value == 0
            assert count.value * max(1, fmt.value // 8) <= MAX_RECORD
            if fmt.value == 32:
                values = C.cast(pointer, C.POINTER(C.c_ulong))
                data = [values[n] & 0xffffffff for n in range(count.value)]
            else:
                assert fmt.value in (0, 8), fmt.value
                data = C.string_at(pointer, count.value) if pointer else b''
            return kind.value, fmt.value, data
        finally:
            if pointer:
                self.x.XFree(pointer)

    def delete(self, window, prop):
        self.x.XDeleteProperty(self.display, window, prop)
        self.x.XFlush(self.display)

    def timestamp(self):
        self.clock_serial += 1
        prop = self.atom('_FVWM_FIXTURE_CLOCK')
        self.put(self.clock, prop, self.atom('INTEGER'), 8,
            str(self.clock_serial).encode())
        event = self.wait_event(lambda e: e.type == 28 and
            e.property.window == self.clock and e.property.atom == prop and
            e.property.state == 0)
        assert event.property.time > 0
        return event.property.time & 0xffffffff

    def root_point(self, window, local_x, local_y):
        x, y, child = C.c_int(), C.c_int(), C.c_ulong()
        assert self.x.XTranslateCoordinates(self.display, window, self.root,
            local_x, local_y, C.byref(x), C.byref(y), C.byref(child))
        assert -32768 <= x.value <= 32767 and -32768 <= y.value <= 32767
        return x.value, y.value

    def message(self, window, name, words):
        assert len(words) == 5
        event = Event()
        event.message = Message(type=33, display=self.display,
            window=window, message_type=self.atom(name), format=32)
        for n, value in enumerate(words):
            event.message.data.l[n] = value & 0xffffffff
        assert self.x.XSendEvent(self.display, window, 0, 0, C.byref(event))
        self.x.XFlush(self.display)
        self.run.trace('x-send', window=window, atom=name, words=words)

    def take(self, predicate):
        for n, event in enumerate(self.events):
            if predicate(event):
                return self.events.pop(n)
        return None

    def wait_event(self, predicate, timeout=5):
        return self.run.wait(lambda: self.take(predicate), timeout)

    def wait_message(self, window, name, timeout=5):
        atom = self.atom(name)
        event = self.wait_event(lambda e: e.type == 33 and
            e.message.window == window and e.message.message_type == atom,
            timeout)
        assert event.message.format == 32
        return [word & 0xffffffff for word in event.message.data.l]

    def offer(self, selection, payload, mode='direct', gnome=False):
        selection = self.atom(selection)
        self.sources[selection] = dict(payload=payload, mode=mode, gnome=gnome)
        stamp = self.timestamp()
        self.x.XSetSelectionOwner(self.display, selection, self.source, stamp)
        self.x.XSync(self.display, 0)
        assert self.x.XGetSelectionOwner(self.display, selection) == self.source
        return stamp

    def serve(self, request):
        source = self.sources.get(request.selection)
        prop = request.property or request.target
        accepted = source is not None and request.owner == self.source
        mode = source['mode'] if source else 'refuse'
        if accepted and request.target == self.atom('TARGETS'):
            kinds = [self.atom('text/uri-list')]
            if source['gnome']:
                kinds.append(self.atom('x-special/gnome-copied-files'))
            self.put(request.requestor, prop, self.atom('ATOM'), 32, kinds)
        elif accepted and request.target in (
                self.atom('text/uri-list'),
                self.atom('x-special/gnome-copied-files')):
            data = source['payload']
            if mode == 'owner-loss':
                self.x.XSetSelectionOwner(self.display, request.selection,
                    0, self.timestamp())
            if mode in ('incr', 'oversize'):
                amount = len(data) if mode == 'incr' else 256 * 1024 + 6
                self.put(request.requestor, prop, self.atom('INCR'), 32,
                    [amount])
                if mode == 'incr':
                    self.x.XSelectInput(self.display, request.requestor,
                        1 << 22)
                    self.incr[(request.requestor, prop)] = dict(
                        target=request.target, payload=data, offset=0)
            elif mode == 'format32':
                self.put(request.requestor, prop, request.target, 32, [1])
            else:
                kind = self.atom('STRING') if mode == 'wrong-type' else \
                    request.target
                self.put(request.requestor, prop, kind, 8, data)
        else:
            accepted = False
        event = Event()
        event.selection = SelectionNotify(type=31, display=self.display,
            requestor=request.requestor, selection=request.selection,
            target=request.target, property=prop if accepted else 0,
            time=request.time)
        assert self.x.XSendEvent(self.display, request.requestor, 0, 0,
            C.byref(event))
        self.x.XFlush(self.display)
        self.run.trace('x-selection-serve', selection=request.selection,
            target=request.target, requestor=request.requestor,
            property=prop if accepted else 0, mode=mode)

    def pump(self):
        for _ in range(128):
            if not self.x.XPending(self.display):
                break
            event = Event()
            self.x.XNextEvent(self.display, C.byref(event))
            if event.type == 30:
                self.serve(event.request)
                continue
            if event.type == 28:
                key = (event.property.window, event.property.atom)
                outgoing = self.incr.get(key)
                if outgoing is not None and event.property.state == 1:
                    offset = outgoing['offset']
                    data = outgoing['payload'][offset:offset + 16384]
                    self.put(*key, outgoing['target'], 8, data)
                    outgoing['offset'] += len(data)
                    if not data:
                        del self.incr[key]
                    continue
            if event.type == 33:
                words = [v & 0xffffffff for v in event.message.data.l]
                self.run.trace('x-receive', window=event.message.window,
                    atom=event.message.message_type, words=words)
                peer = self.run.forms.get(event.message.window)
                if peer is not None and not peer.closed:
                    assert event.message.format == 32
                    peer.send(CLIENT_MESSAGE,
                        detail=event.message.message_type, data=words,
                        fmt=event.message.format)
                    continue
            self.events.append(event)
            assert len(self.events) <= 4096, 'independent X event bound'

    def convert_start(self, selection, target, prop=None):
        prop = self.atom(prop or '_FVWM_FIXTURE_REPLY')
        stamp = self.timestamp()
        self.delete(self.requestor, prop)
        self.x.XConvertSelection(self.display, self.atom(selection),
            self.atom(target), prop, self.requestor, stamp)
        self.x.XFlush(self.display)
        event = self.wait_event(lambda e: e.type == 31 and
            e.selection.requestor == self.requestor and
            e.selection.target == self.atom(target) and
            e.selection.time == stamp)
        self.run.trace('x-conversion-notify', selection=selection,
            target=target, property=event.selection.property, time=stamp)
        return event.selection.property

    def incr_read(self, prop, initial):
        assert initial[0:2] == (self.atom('INCR'), 32)
        assert len(initial[2]) == 1 and 16384 < initial[2][0] <= 256 * 1024
        # Discard only stale pre-handshake PropertyNotify events. Subsequent
        # chunks are generated after the matching deletion, by the real owner.
        self.events = [e for e in self.events if not (e.type == 28 and
            e.property.window == self.requestor and e.property.atom == prop)]
        output, chunks = bytearray(), []
        self.delete(self.requestor, prop)
        for _ in range(40):
            self.wait_event(lambda e: e.type == 28 and
                e.property.window == self.requestor and
                e.property.atom == prop and e.property.state == 0)
            kind, fmt, data = self.get(self.requestor, prop, delete=True)
            assert kind == self.atom('text/uri-list') and fmt == 8
            assert len(data) <= 16384
            chunks.append(len(data))
            output.extend(data)
            assert len(output) <= 256 * 1024
            if not data:
                self.run.trace('x-incr-received', chunks=chunks,
                    bytes=len(output), sha256=digest(output))
                return bytes(output)
        raise AssertionError('INCR chunk count exceeded')

    def convert(self, selection, target):
        prop = self.convert_start(selection, target)
        assert prop, 'selection unexpectedly refused: ' + target
        value = self.get(self.requestor, prop)
        if value[0] == self.atom('INCR'):
            return self.atom(target), 8, self.incr_read(prop, value)
        self.delete(self.requestor, prop)
        return value


class FormPeer:
    """Explicit stand-in: only real forwarded ClientMessages originate in X."""
    def __init__(self, run, epoch, generation=1):
        self.run, self.epoch, self.generation = run, epoch, generation
        self.frame = run.x.create_window(100, 100, 260, 200)
        self.list = run.x.create_window(10, 10, 220, 140, parent=self.frame)
        self.socket = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        self.socket.settimeout(3)
        self.socket.connect(str(run.endpoint))
        self.socket.setblocking(False)
        self.pending, self.feedback, self.closed = bytearray(), [], False
        run.peers.append(self)

    def wire(self, kind, timestamp=0, x=0, y=0, state=0, detail=0,
            data=None, fmt=0, rows=0, bits=b'', nonce=TOKEN, epoch=None,
            generation=None):
        words = [0x46565431, kind, self.epoch >> 32,
            self.epoch & 0xffffffff, self.generation, self.frame, self.list,
            timestamp, x & 0xffffffff, y & 0xffffffff, state, detail,
            *(data or [0] * 5), fmt, len(bits), rows]
        if epoch is not None:
            words[2:4] = [epoch >> 32, epoch & 0xffffffff]
        if generation is not None:
            words[4] = generation
        assert len(words) == 20 and len(nonce) == 16 and len(bits) <= 512
        return struct.pack('!20I', *words) + nonce + bits.ljust(512, b'\0')

    def raw_send(self, data):
        assert len(data) <= WIRE_BYTES
        deadline, sent = time.monotonic() + 3, 0
        while sent < len(data):
            assert time.monotonic() < deadline, 'Form socket send deadline'
            try:
                count = self.socket.send(data[sent:])
                assert count > 0
                sent += count
            except BlockingIOError:
                select.select([], [self.socket], [], 0.01)
        self.run.trace('form-send', bytes=len(data), sha256=digest(data))

    def send(self, kind, **fields):
        self.raw_send(self.wire(kind, **fields))

    def pump(self):
        if self.closed:
            return
        try:
            data = self.socket.recv(8 * WIRE_BYTES - len(self.pending))
        except BlockingIOError:
            return
        if not data:
            self.closed = True
            return
        self.pending.extend(data)
        assert len(self.pending) <= 8 * WIRE_BYTES
        while len(self.pending) >= WIRE_BYTES:
            record = bytes(self.pending[:WIRE_BYTES])
            del self.pending[:WIRE_BYTES]
            words = struct.unpack('!20I', record[:80])
            assert words[:7] == (0x46565431, FEEDBACK,
                self.epoch >> 32, self.epoch & 0xffffffff,
                self.generation, self.frame, self.list), words
            assert record[80:96] == TOKEN and record[96:] == bytes(512)
            assert all(words[n] == 0 for n in
                [7, 8, 9, 10, 12, 13, 14, 15, 16, 17, 18, 19])
            assert 0 <= words[11] <= 5
            self.feedback.append(words[11])
            self.run.trace('form-feedback', epoch=str(self.epoch),
                generation=self.generation, detail=words[11])

    def attach(self, partial=False):
        wire = self.wire(ATTACH)
        if partial:
            self.raw_send(wire[:97])
            self.run.quiet(0.12)
            assert not any(record.get('event') == 'attached' and
                record.get('epoch') == str(self.epoch)
                for record in self.run.records), 'partial attach published'
            self.raw_send(wire[97:])
        else:
            self.raw_send(wire)
        event = self.run.event('attached', epoch=str(self.epoch))
        assert event['generation'] == self.generation
        assert (event['frame'], event['list']) == (self.frame, self.list)
        self.run.wait(lambda: 5 in self.feedback)
        self.run.forms[self.frame] = self
        # This is the stand-in's advertisement, never evidence that actual
        # Form publishes or removes this property correctly.
        self.run.x.put(self.frame, self.run.x.atom('XdndAware'),
            self.run.x.atom('ATOM'), 32, [5])
        assert self.run.x.get(self.list,
            self.run.x.atom('XdndAware'))[0] == 0

    def close(self):
        self.run.forms.pop(self.frame, None)
        self.socket.close()
        self.closed = True


class Run:
    def __init__(self, args, runtime, display, evidence):
        self.args, self.runtime, self.evidence = args, runtime, evidence
        self.records, self.peers, self.forms = [], [], {}
        self.child = self.x = self.errors = None
        self.input = self.output = self.raw = self.parent = self.xtrace = None
        self.pending, self.serial, self.total = bytearray(), 0, 0
        self.endpoint = runtime.root / 'ft' / 'form.sock'
        self.endpoint.parent.mkdir(mode=0o700)
        self.x = XPeer(display, self)
        self.errors = SessionXErrors(self.x, evidence).install()
        self.raw = (args.artifacts_dir / 'helper.ndjson').open('wb')
        self.parent = (args.artifacts_dir / 'controller.ndjson').open('wb')
        self.xtrace = (args.artifacts_dir / 'peer-trace.ndjson').open('w')
        log = runtime.root / 'helper.stderr'
        errors = log.open('wb')
        argv = [str(args.helper), '--form-socket', str(self.endpoint),
            '--token', TOKEN.hex()]
        try:
            process = subprocess.Popen(argv, stdin=subprocess.PIPE,
                stdout=subprocess.PIPE, stderr=errors,
                env=runtime.environment({'DISPLAY': display}),
                start_new_session=True)
        except BaseException:
            errors.close()
            raise
        self.child = ManagedProcess('file-transfer', argv, log, process, errors)
        runtime.processes.append(self.child)
        self.input, self.output = process.stdin, process.stdout
        os.set_blocking(self.input.fileno(), False)
        os.set_blocking(self.output.fileno(), False)
        evidence['helper_process'] = dict(pid=process.pid, argv=argv)
        ready = self.event('ready')
        assert set(ready) == {'v', 'event', 'hostname'}
        assert isinstance(ready['hostname'], str) and ready['hostname']
        self.hostname = ready['hostname']

    def trace(self, kind, **fields):
        value = dict(kind=kind, monotonic=time.monotonic(), **fields)
        line = json.dumps(value) + '\n'
        self.total += len(line)
        assert self.total <= MAX_TRACE, 'fixture trace bound'
        self.xtrace.write(line)
        self.xtrace.flush()

    def tick(self):
        self.x.pump()
        for peer in self.peers:
            peer.pump()
        for _ in range(8):
            try:
                data = os.read(self.output.fileno(), 65536)
            except BlockingIOError:
                break
            assert data, 'helper stdout EOF before expected Wait'
            self.raw.write(data)
            self.raw.flush()
            self.total += len(data)
            assert self.total <= MAX_TRACE, 'helper output aggregate bound'
            self.pending.extend(data)
            while b'\n' in self.pending:
                line, _, remainder = self.pending.partition(b'\n')
                self.pending = bytearray(remainder)
                assert 0 < len(line) < MAX_RECORD
                record = json.loads(line)
                assert isinstance(record, dict) and record.get('v') == 1
                for name in ('id', 'epoch', 'transfer', 'lease'):
                    if name in record:
                        value = record[name]
                        assert isinstance(value, str) and value.isdecimal()
                        assert str(int(value)) == value and \
                            0 < int(value) <= 2**64 - 1
                self.records.append(record)
                assert len(self.records) <= 1024
            assert len(self.pending) < MAX_RECORD

    def wait(self, predicate, timeout=5):
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            value = predicate()
            if value:
                return value
            self.tick()
            assert self.child.poll() is None, 'helper exited unexpectedly'
            select.select([self.output], [], [], 0.005)
        raise AssertionError('bounded protocol wait expired')

    def quiet(self, seconds=0.12):
        deadline = time.monotonic() + seconds
        while time.monotonic() < deadline:
            self.tick()
            assert self.child.poll() is None
            select.select([self.output], [], [], 0.005)

    def take(self, predicate):
        for n, record in enumerate(self.records):
            if predicate(record):
                return self.records.pop(n)
        return None

    def event(self, name, **fields):
        return self.wait(lambda: self.take(lambda r:
            r.get('event') == name and all(r.get(k) == v
                for k, v in fields.items())))

    def command(self, name, ok=True, **fields):
        self.serial += 1
        value = dict(v=1, id=str(self.serial), command=name, **fields)
        data = (json.dumps(value, separators=(',', ':')) + '\n').encode()
        assert len(data) < MAX_RECORD
        self.total += len(data)
        assert self.total <= MAX_TRACE, 'controller aggregate bound'
        self.parent.write(data)
        self.parent.flush()
        deadline, sent = time.monotonic() + 5, 0
        while sent < len(data):
            assert time.monotonic() < deadline
            try:
                count = os.write(self.input.fileno(), data[sent:])
                assert count > 0
                sent += count
            except BlockingIOError:
                self.tick()
                select.select([], [self.input], [], 0.005)
        record = self.wait(lambda: self.take(lambda r:
            r.get('id') == value['id'] and 'status' in r))
        assert record['status'] == ('ok' if ok else 'error'), record
        return record

    def read_offer_batch(self, timestamp, uris):
        # One atomic pipe write presents both complete commands to the helper's
        # bounded input_process before its next X dispatch. The positive server
        # time starts READ immediately instead of deferring it for a clock.
        values = []
        for fields in (dict(command='read', time=timestamp),
                dict(command='offer', time=timestamp, action='move', uris=uris)):
            self.serial += 1
            values.append(dict(v=1, id=str(self.serial), **fields))
        data = b''.join((json.dumps(value, separators=(',', ':')) + '\n').encode()
            for value in values)
        assert len(data) <= os.fpathconf(self.input.fileno(), 'PC_PIPE_BUF')
        self.total += len(data)
        assert self.total <= MAX_TRACE
        self.parent.write(data)
        self.parent.flush()
        deadline = time.monotonic() + 5
        while True:
            assert time.monotonic() < deadline
            try:
                assert os.write(self.input.fileno(), data) == len(data)
                break
            except BlockingIOError:
                self.tick()
                select.select([], [self.input], [], 0.005)
        replies = []
        for value in values:
            reply = self.wait(lambda: self.take(lambda r:
                r.get('id') == value['id'] and 'status' in r))
            assert reply['status'] == 'ok', reply
            replies.append(reply)
        return replies

    def context(self, epoch, generation=1):
        self.command('expect-form', epoch=str(epoch))
        form = FormPeer(self, epoch, generation)
        form.attach(partial=True)
        self.command('context', epoch=str(epoch), generation=generation,
            enabled=True)
        return form

    def complete(self, proposal, success=True):
        # Explicit simulation: no file job or filesystem mutation occurs.
        self.command('admit', transfer=proposal['transfer'])
        self.command('complete', transfer=proposal['transfer'],
            success=success, action=proposal['action'])
        record = self.event('result', transfer=proposal['transfer'])
        assert record['status'] == ('success' if success else 'failed')

    def case(self, name, operation):
        started = time.monotonic()
        record = dict(status='running', started=started)
        self.evidence['cases'][name] = record
        try:
            operation()
        except BaseException as error:
            record.update(status='failed', error=repr(error))
            raise
        else:
            record.update(status='passed', seconds=time.monotonic() - started)

    def close(self):
        for peer in self.peers:
            peer.close()
        if self.input is not None:
            self.input.close()
        try:
            if self.child is not None:
                status = self.child.wait_until_exit(time.monotonic() + 3)
                self.evidence['helper_process']['wait'] = status
                assert status == 0, 'helper parent EOF must exit cleanly'
                assert b'unexpected X error' not in \
                    self.child.log_path.read_bytes()
        finally:
            for stream in (self.output, self.raw, self.parent, self.xtrace):
                if stream is not None:
                    stream.close()
            if self.errors is not None:
                self.errors.close_display()
            elif self.x is not None:
                self.x.close()
        if self.errors is not None:
            assert not self.errors.unexpected, self.errors.unexpected


def exercise(run):
    x = run.x
    current = [None]
    short = 'file:///tmp/fvwm-transfer-fixture/a%20b\r\n'
    large = ''.join('file:///tmp/fvwm-transfer-fixture/' + str(n) + '-' +
        'a' * 300 + '\r\n' for n in range(100))

    def attach_failures():
        run.command('expect-form', epoch='1')
        for fields in (dict(nonce=bytes(16)), dict(epoch=2)):
            peer = FormPeer(run, 1)
            peer.send(ATTACH, **fields)
            run.event('unavailable', code='invalid-form')
            run.wait(lambda: peer.closed)
            peer.close()
        peer = FormPeer(run, 1)
        peer.attach(partial=True)
        current[0] = peer
        run.command('context', epoch='1', generation=1, enabled=True)
    run.case('strict-partial-nonce-stale-attach', attach_failures)

    def action_snapshot():
        current[0].send(ACTION, timestamp=x.timestamp(), detail=1,
            rows=9, bits=b'\x81\x01')
        record = run.event('action', kind='copy')
        assert record['epoch'] == '1' and record['generation'] == 1
        assert record['rows'] == 9
        assert base64.b64decode(record['selection'], validate=True) == b'\x81\x01'
    run.case('native-action-selection-snapshot', action_snapshot)

    def clipboard_formats():
        for action, prefix in (('copy', b'copy\n'), ('move', b'cut\n')):
            reply = run.command('offer', action=action, uris=short,
                time=x.timestamp())
            assert reply['lease'].isdecimal()
            assert x.convert('CLIPBOARD', 'text/uri-list') == (
                x.atom('text/uri-list'), 8, short.encode())
            assert x.convert('CLIPBOARD', 'x-special/gnome-copied-files') == (
                x.atom('x-special/gnome-copied-files'), 8,
                prefix + short.replace('\r', '').encode())
            targets = x.convert('CLIPBOARD', 'TARGETS')
            assert targets[0:2] == (x.atom('ATOM'), 32)
            assert set(targets[2]) == {x.atom(name) for name in (
                'TARGETS', 'TIMESTAMP', 'MULTIPLE', 'text/uri-list',
                'x-special/gnome-copied-files')}
            timestamp = x.convert('CLIPBOARD', 'TIMESTAMP')
            assert timestamp[0:2] == (x.atom('INTEGER'), 32)
            assert len(timestamp[2]) == 1 and timestamp[2][0] > 0
            assert x.convert_start('CLIPBOARD', 'DELETE') == 0
    run.case('clipboard-copy-cut-targets-timestamp-direct', clipboard_formats)

    def multiple():
        prop = x.atom('_FVWM_FIXTURE_MULTIPLE')
        first, second = x.atom('_FVWM_PAIR_1'), x.atom('_FVWM_PAIR_2')
        pairs = [x.atom('text/uri-list'), first, x.atom('DELETE'), second]
        x.put(x.requestor, prop, x.atom('ATOM_PAIR'), 32, pairs)
        stamp = x.timestamp()
        x.x.XConvertSelection(x.display, x.atom('CLIPBOARD'),
            x.atom('MULTIPLE'), prop, x.requestor, stamp)
        x.x.XFlush(x.display)
        event = x.wait_event(lambda e: e.type == 31 and
            e.selection.requestor == x.requestor and e.selection.time == stamp)
        assert event.selection.property == prop
        assert x.get(x.requestor, prop) == (
            x.atom('ATOM_PAIR'), 32, pairs[:3] + [0])
        assert x.get(x.requestor, first) == (
            x.atom('text/uri-list'), 8, short.encode())
        for name in (prop, first, second):
            x.delete(x.requestor, name)
    run.case('multiple-supported-and-failed-pair', multiple)

    def immutable_incr():
        old = run.command('offer', action='copy', uris=large)
        prop = x.convert_start('CLIPBOARD', 'text/uri-list')
        initial = x.get(x.requestor, prop)
        assert initial[0] == x.atom('INCR')
        new = run.command('offer', action='move', uris=short)
        assert old['lease'] != new['lease']
        assert x.incr_read(prop, initial) == large.encode()
        run.command('clear', lease=old['lease'])
        assert x.convert('CLIPBOARD', 'text/uri-list')[2] == short.encode()
    run.case('real-incr-send-immutable-replaced-lease', immutable_incr)

    def navigation():
        old = current[0]
        current[0] = run.context(2, generation=7)
        run.wait(lambda: old.closed)
        old.close()
        assert x.convert('CLIPBOARD', 'text/uri-list')[2] == short.encode()
        run.command('read', time=x.timestamp())
        proposal = run.event('proposal', kind='clipboard', epoch='2')
        assert proposal['generation'] == 7 and proposal['action'] == 'move'
        assert proposal['uris'] == short.replace('\r', '')
        assert 'lease' in proposal
        run.complete(proposal)
    run.case('clipboard-survives-epoch-navigation-replacement', navigation)

    def incoming_direct_incr():
        for mode, payload, gnome in (
                ('direct', ('cut\n' + short.replace('\r', '')).encode(), True),
                ('incr', large.encode(), False)):
            x.offer('CLIPBOARD', payload, mode=mode, gnome=gnome)
            run.command('read', time=x.timestamp())
            proposal = run.event('proposal', kind='clipboard')
            assert proposal['uris'] == (short.replace('\r', '') if gnome else large)
            assert proposal['action'] == ('move' if gnome else 'copy')
            assert 'lease' not in proposal
            run.complete(proposal)
    run.case('real-independent-direct-and-incr-receive', incoming_direct_incr)

    def refusals():
        for mode in ('wrong-type', 'format32', 'oversize', 'owner-loss'):
            x.offer('CLIPBOARD', short.encode(), mode=mode)
            run.command('read', time=x.timestamp())
            result = run.event('result')
            assert result['status'] == 'failed', (mode, result)
            assert not any(r.get('event') == 'proposal' for r in run.records)
    run.case('wrong-type-format-oversize-owner-loss-refusal', refusals)

    def source_dnd(version, move):
        peer = current[0]
        peer.feedback.clear()
        x.put(x.target, x.atom('XdndAware'), x.atom('ATOM'), 32, [version])
        target_x, target_y = x.root_point(x.target, 50, 50)
        stamp = x.timestamp()
        peer.send(BEGIN, timestamp=stamp, x=target_x, y=target_y,
            state=1 if move else 0, rows=2, bits=b'\x03')
        begin = run.event('drag-begin')
        run.command('drag-data', transfer=begin['transfer'],
            action='move' if move else 'copy', uris=short)
        enter = x.wait_message(x.target, 'XdndEnter')
        assert enter[0] == peer.frame and enter[1] >> 24 == version
        position = x.wait_message(x.target, 'XdndPosition')
        action = x.atom('XdndActionMove' if move else 'XdndActionCopy')
        assert position[0] == peer.frame and position[4] == action
        x.message(peer.frame, 'XdndStatus', [x.target, 3, 0, 0, action])
        run.wait(lambda: (3 if move else 2) in peer.feedback)
        selection = x.convert('XdndSelection', 'text/uri-list')
        assert selection[2] == ('file://' + run.hostname +
            '/tmp/fvwm-transfer-fixture/a%20b\r\n').encode()
        assert x.convert('XdndSelection', 'text/x-xdnd-username')[1] == 8
        peer.send(RELEASE, timestamp=x.timestamp(), x=target_x, y=target_y,
            state=1 if move else 0)
        # RELEASE requests another real position/status round trip.
        x.wait_message(x.target, 'XdndPosition')
        x.message(peer.frame, 'XdndStatus', [x.target, 3, 0, 0, action])
        drop = x.wait_message(x.target, 'XdndDrop')
        assert drop[0] == peer.frame and drop[2] > 0
        x.message(peer.frame, 'XdndFinished', [x.target,
            1 if version == 5 else 0, action if version == 5 else 0, 0, 0])
        result = run.event('result', transfer=begin['transfer'])
        assert result['status'] == 'success'
        x.delete(x.target, x.atom('XdndAware'))
    run.case('xdnd-source-v3-copy-real-conversion', lambda: source_dnd(3, False))
    run.case('xdnd-source-v5-move-real-conversion', lambda: source_dnd(5, True))

    def target_dnd(version, move):
        peer = current[0]
        target_x, target_y = x.root_point(peer.list, 40, 40)
        x.offer('XdndSelection', short.encode())
        action = x.atom('XdndActionMove' if move else 'XdndActionCopy')
        x.message(peer.frame, 'XdndEnter', [x.source, version << 24,
            x.atom('text/uri-list'), 0, 0])
        x.message(peer.frame, 'XdndPosition', [x.source, 0,
            ((target_x & 0xffff) << 16) | (target_y & 0xffff),
            x.timestamp(), action])
        status = x.wait_message(x.source, 'XdndStatus')
        assert status[0] == peer.frame and status[1] & 1 and status[4] == action
        x.message(peer.frame, 'XdndDrop', [x.source, 0, x.timestamp(), 0, 0])
        proposal = run.event('proposal', kind='dnd')
        assert proposal['uris'] == short
        assert proposal['action'] == ('move' if move else 'copy')
        run.command('complete', ok=False, transfer=proposal['transfer'],
            success=True, action=proposal['action'])
        run.quiet()
        assert x.take(lambda e: e.type == 33 and
            e.message.message_type == x.atom('XdndFinished')) is None
        run.command('admit', transfer=proposal['transfer'])
        run.quiet()
        assert x.take(lambda e: e.type == 33 and
            e.message.message_type == x.atom('XdndFinished')) is None
        run.command('complete', transfer=proposal['transfer'],
            success=True, action=proposal['action'])
        finished = x.wait_message(x.source, 'XdndFinished')
        assert finished[0] == peer.frame and finished[1] & 1
        assert finished[2] == action
        assert run.event('result', transfer=proposal['transfer'])['status'] == 'success'
    run.case('xdnd-target-v3-copy-admit-complete', lambda: target_dnd(3, False))
    run.case('xdnd-target-v5-move-admit-complete', lambda: target_dnd(5, True))

    def target_refusal_cancel():
        peer = current[0]
        outside_x, outside_y = x.root_point(peer.frame, 250, 190)
        action = x.atom('XdndActionCopy')
        x.message(peer.frame, 'XdndEnter', [x.source, 5 << 24,
            x.atom('text/uri-list'), 0, 0])
        x.message(peer.frame, 'XdndPosition', [x.source, 0,
            ((outside_x & 0xffff) << 16) | (outside_y & 0xffff),
            x.timestamp(), action])
        status = x.wait_message(x.source, 'XdndStatus')
        assert status[0] == peer.frame and not status[1] & 1
        x.message(peer.frame, 'XdndLeave', [x.source, 0, 0, 0, 0])
        run.quiet()
        assert x.take(lambda e: e.type == 33 and
            e.message.message_type == x.atom('XdndFinished')) is None
        peer.send(BEGIN, timestamp=x.timestamp(), x=150, y=150,
            rows=1, bits=b'\x01')
        begin = run.event('drag-begin')
        peer.send(CANCEL, timestamp=x.timestamp())
        assert run.event('result', transfer=begin['transfer'])['status'] == 'cancelled'
    run.case('xdnd-outside-list-refusal-leave-source-cancel', target_refusal_cancel)

    def stale_generation():
        peer = current[0]
        peer.send(BEGIN, timestamp=x.timestamp(), x=150, y=150,
            rows=1, bits=b'\x01')
        begin = run.event('drag-begin')
        # Drain old feedback before changing expected metadata generation.
        run.quiet()
        peer.generation += 1
        peer.feedback.clear()
        peer.send(GENERATION)
        assert run.event('result', transfer=begin['transfer'])['status'] == 'cancelled'
        attached = run.event('attached', epoch=str(peer.epoch),
            generation=peer.generation)
        assert (attached['frame'], attached['list']) == (peer.frame, peer.list)
        run.wait(lambda: 5 in peer.feedback)
        run.command('context', epoch=str(peer.epoch),
            generation=peer.generation, enabled=True)
        peer.send(MOTION, timestamp=x.timestamp(), generation=peer.generation - 1)
        run.event('unavailable', code='invalid-form')
        run.wait(lambda: peer.closed)
    run.case('generation-cancel-and-stale-record-detach', stale_generation)


def retired_peer_regression(run, first_epoch):
    def variants():
        for offset, partial in enumerate((True, False)):
            epoch = first_epoch + 2 * offset
            variant = 'partial-attach' if partial else 'late-connect-attach'
            run.trace('retired-peer-variant', variant=variant, phase='begin',
                epoch=str(epoch))
            run.command('expect-form', epoch=str(epoch))
            old = None
            if partial:
                old = FormPeer(run, epoch)
                old.raw_send(old.wire(ATTACH)[:97])
            run.command('expect-form', epoch=str(epoch + 1))
            current = FormPeer(run, epoch + 1, generation=41 + offset)
            current.attach()
            run.command('context', epoch=str(current.epoch),
                generation=current.generation, enabled=True)
            if partial:
                old.raw_send(old.wire(ATTACH)[97:])
            else:
                # This peer did not exist at expect-form: a peer sweep alone
                # cannot retire a delayed old-epoch connection safely.
                old = FormPeer(run, epoch)
                old.send(ATTACH)
            run.wait(lambda: old.closed)
            timestamp = run.x.timestamp()
            current.send(ACTION, timestamp=timestamp, detail=3)
            action = run.event('action', kind='paste',
                epoch=str(current.epoch), generation=current.generation)
            assert action['time'] == timestamp
            assert action['rows'] == 0 and action['selection'] == ''
            assert not current.closed, 'retired peer closed current attachment'
            # EOF and the subsequent current action are positive barriers.
            # Every earlier helper output has now crossed its ordered pipe.
            assert not any(record.get('event') == 'unavailable'
                for record in run.records), 'retired peer caused global failure'
            assert not any(record.get('event') == 'attached'
                for record in run.records), 'retired peer changed attachment'
            run.trace('retired-peer-variant', variant=variant, phase='passed',
                epoch=str(current.epoch), generation=current.generation)
            old.close()
            current.close()
    run.case('retired-peer-isolation-partial-and-late-attach', variants)


def invalid_selection_regression(run, epoch):
    def variants():
        run.command('expect-form', epoch=str(epoch))
        current = FormPeer(run, epoch, generation=51)
        current.attach()
        run.command('context', epoch=str(epoch),
            generation=current.generation, enabled=True)
        remote = 'remote.invalid' if run.hostname != 'remote.invalid' else \
            'other.invalid'
        valid = 'file:///tmp/fvwm-transfer-fixture/recovered\r\n'
        invalid = (
            ('remote-authority', 'file://' + remote + '/tmp/refused\r\n'),
            ('bad-percent', 'file:///tmp/fvwm-transfer-fixture/a%zz\r\n'))
        for variant, payload in invalid:
            run.trace('invalid-selection-variant', variant=variant,
                phase='begin', epoch=str(current.epoch))
            run.x.offer('CLIPBOARD', payload.encode())
            rejected = run.command('read', time=run.x.timestamp())
            result = run.event('result', transfer=rejected['transfer'])
            assert result['status'] == 'failed', (variant, result)
            assert not any(record.get('event') == 'proposal' and
                (record.get('transfer') == rejected['transfer'] or
                record.get('id') == rejected['id'])
                for record in run.records), 'invalid selection was proposed'
            # A real subsequent foreign conversion is a positive recovery
            # barrier on the same attached context, with no helper restart.
            run.x.offer('CLIPBOARD', valid.encode())
            recovered = run.command('read', time=run.x.timestamp())
            assert recovered['transfer'] != rejected['transfer']
            proposal = run.event('proposal', kind='clipboard',
                id=recovered['id'], transfer=recovered['transfer'],
                epoch=str(current.epoch), generation=current.generation)
            assert proposal['action'] == 'copy' and proposal['uris'] == valid
            assert 'lease' not in proposal
            run.complete(proposal)
            assert not current.closed, 'invalid selection closed attachment'
            assert run.child.poll() is None, 'invalid selection stopped helper'
            assert not any(record.get('event') == 'unavailable'
                for record in run.records), 'invalid selection caused global failure'
            run.trace('invalid-selection-variant', variant=variant,
                phase='passed', epoch=str(current.epoch),
                generation=current.generation)
        current.close()
    run.case('invalid-selection-correlated-failure-and-recovery', variants)


def peer_output_progress_regression(run, epoch):
    def admitted_job():
        run.command('expect-form', epoch=str(epoch))
        current = FormPeer(run, epoch, generation=71)
        current.attach()
        run.command('context', epoch=str(epoch),
            generation=current.generation, enabled=True)
        payload = 'file:///tmp/fvwm-transfer-fixture/quiet-job\r\n'
        run.x.offer('CLIPBOARD', payload.encode())
        read = run.command('read', time=run.x.timestamp())
        proposal = run.event('proposal', kind='clipboard',
            id=read['id'], transfer=read['transfer'],
            epoch=str(epoch), generation=current.generation)
        assert proposal['action'] == 'copy' and proposal['uris'] == payload
        assert 'lease' not in proposal
        run.command('admit', transfer=proposal['transfer'])
        # Drain earlier feedback, then simulate controller work without Form
        # metadata or keepalives. Admission already allows a 300-second job.
        run.quiet()
        assert not current.closed and not current.pending
        current.feedback.clear()
        quiet_started = time.monotonic()
        run.trace('peer-output-progress', phase='admitted-quiet-start',
            epoch=str(epoch), generation=current.generation,
            transfer=proposal['transfer'])
        run.quiet(6)
        quiet_seconds = time.monotonic() - quiet_started
        assert quiet_seconds > 5
        assert not current.closed, 'empty attached peer expired during job'
        assert not current.feedback and not current.pending
        assert not any(record.get('event') == 'result' and
            record.get('transfer') == proposal['transfer']
            for record in run.records), 'admitted transfer ended early'
        # Only the controller completes: no socket input refreshes peer time.
        # This acknowledges simulated work, never an actual filesystem job.
        run.command('complete', transfer=proposal['transfer'],
            success=True, action=proposal['action'])
        result = run.event('result', transfer=proposal['transfer'])
        assert result['status'] == 'success', result
        run.wait(lambda: current.closed or 5 in current.feedback)
        run.trace('peer-output-progress', phase='completed-feedback',
            quiet_seconds=quiet_seconds, result=result['status'],
            peer_closed=current.closed, feedback=list(current.feedback))
        assert not current.closed, 'fresh completion feedback closed idle peer'
        assert current.feedback == [5], 'fresh completion IDLE feedback missing'
        timestamp = run.x.timestamp()
        current.send(ACTION, timestamp=timestamp, detail=3)
        action = run.event('action', kind='paste', time=timestamp,
            epoch=str(epoch), generation=current.generation)
        assert action['rows'] == 0 and action['selection'] == ''
        assert not current.closed, 'completion retired current attachment'
        assert not any(record.get('event') in ('unavailable', 'attached')
            for record in run.records), 'completion changed current attachment'
        current.close()
    run.case('admitted-job-idle-peer-fresh-output-and-native-action',
        admitted_job)


def review_superseded_self_read(run, epoch):
    run.command('expect-form', epoch=str(epoch))
    current = FormPeer(run, epoch, generation=61)
    current.attach()
    run.command('context', epoch=str(epoch), generation=61, enabled=True)
    old_uris = ''.join('file:///tmp/fvwm-transfer-fixture/old-' + str(n) +
        '-' + 'a' * 300 + '\r\n' for n in range(100))
    new_uris = 'file:///tmp/fvwm-transfer-fixture/new-cut\r\n'
    old = run.command('offer', action='move', uris=old_uris)
    read, new = run.read_offer_batch(run.x.timestamp(), new_uris)
    assert old['lease'] != new['lease']
    terminal = run.wait(lambda: run.take(lambda r:
        r.get('event') in ('result', 'proposal') and
        r.get('transfer') == read['transfer']))
    cancelled = terminal.get('event') == 'result' and \
        terminal.get('status') == 'cancelled'
    if terminal.get('event') == 'proposal':
        # Exercise the real protocol's stale completion/conditional clear path;
        # these controller acknowledgments do not represent filesystem jobs.
        run.complete(terminal)
        run.command('clear', lease=terminal['lease'])
    else:
        run.command('complete', ok=False, transfer=read['transfer'],
            success=True, action='move')
        run.command('clear', lease=old['lease'])
    owner = run.x.x.XGetSelectionOwner(run.x.display, run.x.atom('CLIPBOARD'))
    preserved = bool(owner) and run.x.convert('CLIPBOARD',
        'x-special/gnome-copied-files')[2] == \
        ('cut\n' + new_uris.replace('\r', '')).encode()
    # Separate baseline: a read admitted before replacement can finish, and
    # clearing its captured old lease must still preserve the new Cut lease.
    earlier = run.command('offer', action='move', uris=old_uris)
    admitted = run.command('read', time=run.x.timestamp())
    proposal = run.event('proposal', transfer=admitted['transfer'],
        id=admitted['id'], epoch=str(epoch), generation=61)
    assert proposal['lease'] == earlier['lease']
    assert proposal['uris'] == old_uris.replace('\r', '')
    run.command('admit', transfer=proposal['transfer'])
    later = run.command('offer', action='move', uris=new_uris)
    run.command('complete', transfer=proposal['transfer'],
        success=True, action='move')
    assert run.event('result', transfer=proposal['transfer'])['status'] == 'success'
    run.command('clear', lease=proposal['lease'])
    admitted_preserved = run.x.convert('CLIPBOARD',
        'x-special/gnome-copied-files')[2] == \
        ('cut\n' + new_uris.replace('\r', '')).encode()
    assert earlier['lease'] != later['lease']
    run.evidence.setdefault('review_observations', {})['self_read'] = dict(
        superseded_read_cancelled=cancelled, new_cut_preserved=preserved,
        admitted_old_completion_preserved_new_cut=admitted_preserved,
        self_incr_phase='not established; atomic pair precedes X dispatch',
        filesystem_completion='simulated controller only')
    assert not current.closed
    current.close()
    assert cancelled and preserved and admitted_preserved, \
        'superseded self-read must cancel and preserve the new Cut lease'


def review_late_drag_intent(run, epoch):
    run.command('expect-form', epoch=str(epoch))
    current = FormPeer(run, epoch, generation=62)
    current.attach()
    run.command('context', epoch=str(epoch), generation=62, enabled=True)
    x, observed = run.x, []
    for shift in (True, False):
        x.put(x.target, x.atom('XdndAware'), x.atom('ATOM'), 32, [5])
        target_x, target_y = x.root_point(x.target, 50, 50)
        modifiers = 1 if shift else 0
        current.send(BEGIN, timestamp=x.timestamp(), x=target_x, y=target_y,
            state=modifiers, rows=1, bits=b'\x01')
        begin = run.event('drag-begin', epoch=str(epoch), generation=62)
        current.send(MOTION, timestamp=x.timestamp(), x=target_x, y=target_y,
            state=modifiers | 256)
        current.send(RELEASE, timestamp=x.timestamp(), x=target_x, y=target_y,
            state=modifiers | 256)
        # This same-stream action can be observed only after both preceding
        # gesture records were consumed. It performs no clipboard operation.
        barrier_time = x.timestamp()
        current.send(ACTION, timestamp=barrier_time, detail=3)
        run.event('action', kind='paste', time=barrier_time,
            epoch=str(epoch), generation=62)
        run.command('drag-data', transfer=begin['transfer'], action='copy',
            uris='file:///tmp/fvwm-transfer-fixture/drag-intent\r\n')
        enter = x.wait_message(x.target, 'XdndEnter')
        position = x.wait_message(x.target, 'XdndPosition')
        assert enter[0] == current.frame and position[0] == current.frame
        action = position[4]
        observed.append(action == x.atom(
            'XdndActionMove' if shift else 'XdndActionCopy'))
        # Complete the actual protocol even when the advertised action is wrong,
        # so both variants and the other review regression remain assessable.
        x.message(current.frame, 'XdndStatus', [x.target, 3, 0, 0, action])
        assert x.wait_message(x.target, 'XdndDrop')[0] == current.frame
        assert x.convert('XdndSelection', 'text/uri-list')[1] == 8
        x.message(current.frame, 'XdndFinished', [x.target, 1, action, 0, 0])
        assert run.event('result', transfer=begin['transfer'])['status'] == 'success'
        x.delete(x.target, x.atom('XdndAware'))
    run.evidence.setdefault('review_observations', {})['late_drag'] = dict(
        shift_requested_move=observed[0], no_shift_requested_copy=observed[1])
    current.close()
    assert all(observed), 'late drag-data must preserve native Shift Move intent'


def review_regressions(run, epoch):
    failures = []
    for offset, (name, operation) in enumerate((
            ('superseded-self-read-cancel-preserves-new-cut',
                review_superseded_self_read),
            ('late-drag-data-preserves-native-shift-move',
                review_late_drag_intent))):
        try:
            run.case(name, lambda: operation(run, epoch + offset))
        except AssertionError as error:
            failures.append(dict(case=name, error=repr(error)))
            if run.child.poll() is not None:
                raise
    run.evidence['review_regression_failures'] = failures
    assert not failures, failures


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--helper', type=Path, required=True)
    parser.add_argument('--artifacts-dir', type=Path, required=True)
    parser.add_argument('--execution-profile', required=True)
    parser.add_argument('--display')
    selection = parser.add_mutually_exclusive_group()
    selection.add_argument('--retired-peer-only', action='store_true',
        help='run only the deterministic retired-peer isolation regression')
    selection.add_argument('--invalid-selection-only', action='store_true',
        help='run only malformed clipboard selection refusal and recovery')
    selection.add_argument('--review-regressions-only', action='store_true',
        help='run both bounded native lease and Shift intent regressions')
    selection.add_argument('--peer-output-progress-only', action='store_true',
        help='run only admitted-job idle-peer feedback and action regression')
    parser.add_argument('--config-header', type=Path,
        help='matching generated config.h, required if not beside helper build')
    args = parser.parse_args()
    if not sys.platform.startswith(('linux', 'freebsd', 'openbsd')):
        parser.error('native fixture execution supports Linux/FreeBSD/OpenBSD only')
    args.helper = args.helper.resolve(strict=True)
    args.artifacts_dir = args.artifacts_dir.resolve()
    args.artifacts_dir.mkdir(parents=True, exist_ok=False)
    source = Path(__file__).resolve().parents[2]
    paths = [Path(__file__).resolve(), source / 'bin/file_transfer.c',
        source / 'bin/file_transfer_policy.c', source / 'bin/file_transfer_policy.h',
        source / 'libs/file_transfer_protocol.c',
        source / 'libs/file_transfer_protocol.h',
        source / 'dev-docs/decisions/FILE-TRANSFER-2026-10-31.md']
    paths += [Path(__file__).with_name(name) for name in (
        'geometry_transition_live.py', 'selection_live.py',
        'live_x_harness.py', 'session_x_errors.py')]
    evidence = dict(status='running', execution_profile=args.execution_profile,
        platform=sys.platform, fixture_argv=sys.argv, cases={},
        cleanup_failures=[],
        helper_sha256=digest(args.helper.read_bytes()),
        source_sha256={str(p.relative_to(source)): digest(p.read_bytes())
            for p in paths}, limits={
                'actual_form_gestures': 'not exercised; stand-in socket peer',
                'browser_and_file_jobs': 'not exercised; simulated controller',
                'filesystem_mutation': 'none',
                'installed_discovery_and_theme_captures': 'not exercised',
                'native_sanitizer_and_six_target_qualification': 'separate gates'})
    runtime, run, failure = HarnessRuntime(prefix='fvwm-ft-'), None, None
    try:
        config = args.config_header
        if config is None:
            config = next((p / 'config.h' for p in list(args.helper.parents)[:5]
                if (p / 'config.h').is_file()), None)
        assert config is not None, 'supply matching --config-header'
        evidence['config_header'] = dict(path=str(config.resolve()),
            sha256=digest(config.read_bytes()))
        if args.display:
            display = args.display
            evidence['display_ownership'] = 'borrowed explicitly supplied display'
        else:
            _, display = start_display_server(runtime, 'Xvfb',
                ['Xvfb', '-screen', '0', '1000x700x24', '-noreset'],
                'xserver.log', time.monotonic() + 10)
            evidence['display_ownership'] = 'private harness Xvfb'
        evidence['display'] = display
        # Retain the partially initialized owner so startup failures also close
        # their pipes, wait the child and restore the process-global X handler.
        run = Run.__new__(Run)
        run.__init__(args, runtime, display, evidence)
        if args.retired_peer_only:
            retired_peer_regression(run, 1)
        elif args.invalid_selection_only:
            invalid_selection_regression(run, 1)
        elif args.review_regressions_only:
            review_regressions(run, 1)
        elif args.peer_output_progress_only:
            peer_output_progress_regression(run, 1)
        else:
            exercise(run)
            retired_peer_regression(run, 3)
            invalid_selection_regression(run, 7)
            review_regressions(run, 8)
            peer_output_progress_regression(run, 10)
        evidence['status'] = 'passed'
    except BaseException as error:
        evidence.update(status='failed', failure=repr(error))
        failure = error
    finally:
        if run is not None:
            try:
                run.close()
            except BaseException as error:
                evidence['cleanup_failures'].append(repr(error))
                evidence['status'] = 'failed'
                if failure is None:
                    failure = error
        # Stop and Wait before copying final logs; runtime.close subsequently
        # removes only already retired owned resources and the private tree.
        for process in reversed(runtime.processes):
            try:
                process.close(time.monotonic() + 3)
            except BaseException as error:
                evidence['cleanup_failures'].append(repr(error))
                evidence['status'] = 'failed'
                if failure is None:
                    failure = error
        for process in runtime.processes:
            try:
                if process.log_path.exists():
                    shutil.copyfile(process.log_path,
                        args.artifacts_dir / process.log_path.name)
            except BaseException as error:
                evidence['cleanup_failures'].append(repr(error))
                evidence['status'] = 'failed'
                if failure is None:
                    failure = error
        try:
            runtime.close()
        except BaseException as error:
            evidence['cleanup_failures'].append(repr(error))
            evidence['status'] = 'failed'
            if failure is None:
                failure = error
        evidence['process_waits'] = [dict(name=p.name, pid=p.process.pid,
            returncode=p.process.returncode) for p in runtime.processes]
        save(args.artifacts_dir / 'results.json', evidence)
    if failure is not None:
        raise failure
    print(json.dumps(dict(status='passed', groups=len(evidence['cases']),
        artifacts=str(args.artifacts_dir))))


if __name__ == '__main__':
    main()
