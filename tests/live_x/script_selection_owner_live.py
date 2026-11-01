#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or any later version.
"""Independent Script owner acceptance, for the shared Script checkpoint.

Authoring this fixture does not establish a runtime result. Run only under the
checkpoint's execution authority. No requestor INCR/paste servicing is asserted.
The Script starts through real module pipes, as in script_message_live; XTest
drives its mapped TextField without requiring a WM or a second packet reader.
Existing 40/41 and two-second grace controls are composed unchanged at the end.
"""
import argparse
import ctypes as C
import json
import os
from pathlib import Path
import selectors
import shutil
import struct
import time
from types import SimpleNamespace

from colorset_live import WindowAttributes
from geometry_transition_live import Client, Pointer
from live_x_harness import (HarnessRuntime, start_display_server,
    wait_for_predicate)
from script_message_live import run as message_controls
from script_transport_live import packet, WORD
from selection_live import SelectionRequest, SelectionNotify


class Clear(C.Structure):
    _fields_ = SelectionRequest._fields_[:4] + [
        (name, C.c_ulong) for name in ('window', 'selection', 'time')]


class Property(C.Structure):
    _fields_ = SelectionRequest._fields_[:4] + [
        ('window', C.c_ulong), ('atom', C.c_ulong), ('time', C.c_ulong),
        ('state', C.c_int)]


class Error(C.Structure):
    _fields_ = [('type', C.c_int), ('display', C.c_void_p),
        ('resourceid', C.c_ulong), ('serial', C.c_ulong),
        ('error_code', C.c_ubyte), ('request_code', C.c_ubyte),
        ('minor_code', C.c_ubyte)]


class Event(C.Union):
    _fields_ = [('request', SelectionRequest), ('selection', SelectionNotify),
        ('clear', Clear), ('property', Property), ('pad', C.c_long * 24)]


class Peer:
    """Xlib peer with explicit native-long versus packed-32 conversion."""

    def __init__(self, display):
        self.client = Client(display)
        self.x, self.d = self.client.x, self.client.display
        p, w, i = C.c_void_p, C.c_ulong, C.c_int
        signatures = {
            'XFlush': ([p], i),
            'XSetErrorHandler': ([p], p),
            'XGetAtomName': ([p, w], p),
            'XInternAtom': ([p, C.c_char_p, i], w),
            'XGetSelectionOwner': ([p, w], w),
            'XSetSelectionOwner': ([p, w, w, w], i),
            'XConvertSelection': ([p, w, w, w, w, w], i),
            'XChangeProperty': ([p, w, w, w, i, i, p, i], i),
            'XDeleteProperty': ([p, w, w], i),
            'XGetWindowProperty': ([p, w, w, C.c_long, C.c_long, i, w,
                C.POINTER(w), C.POINTER(i), C.POINTER(w), C.POINTER(w),
                C.POINTER(p)], i),
            'XCheckTypedWindowEvent': ([p, w, i, C.POINTER(Event)], i),
            'XSendEvent': ([p, w, i, C.c_long, C.POINTER(Event)], i),
            'XFetchName': ([p, w, C.POINTER(p)], i),
            'XTranslateCoordinates': ([p, w, w, i, i, C.POINTER(i),
                C.POINTER(i), C.POINTER(w)], i),
            'XGetWindowAttributes': ([p, w, C.POINTER(WindowAttributes)], i),
            'XGetImage': ([p, w, i, i, C.c_uint, C.c_uint, w, i], p),
            'XGetPixel': ([p, i, i], w),
            'XDestroyImage': ([p], i),
        }
        for name, (arguments, result) in signatures.items():
            getattr(self.x, name).argtypes = arguments
            getattr(self.x, name).restype = result
        self.window = self.x.XCreateSimpleWindow(self.d, self.client.root,
            0, 0, 1, 1, 0, 0, 0)
        self.x.XSelectInput(self.d, self.window, 1 << 22)
        self.atoms = {name: self.atom(name) for name in (
            'PRIMARY', 'STRING', 'TARGETS', 'TIMESTAMP', 'MULTIPLE',
            'UTF8_STRING', 'ATOM', 'ATOM_PAIR', 'INTEGER')}
        self.prop = self.atom('owner-result')
        self.control = self.atom('owner-multiple')

    def atom(self, name):
        raw = name.encode() if isinstance(name, str) else name
        return self.x.XInternAtom(self.d, raw, False)

    def nonexistent_atom(self):
        # Confirm the atom on this isolated peer connection. Only this one
        # synchronous query runs under the handler; restore it on every exit.
        invalid, errors = 0xffffffff, []
        callback_type = C.CFUNCTYPE(C.c_int, C.c_void_p, C.POINTER(Error))

        def record(display, error):
            value = error.contents
            errors.append((display, value.resourceid, value.error_code,
                value.request_code, value.minor_code))
            return 0

        callback = callback_type(record)
        self.x.XSync(self.d, False)
        previous = self.x.XSetErrorHandler(C.cast(callback, C.c_void_p))
        try:
            name = self.x.XGetAtomName(self.d, invalid)
            if name:
                self.x.XFree(name)
            assert not name, 'chosen atom unexpectedly exists'
        finally:
            self.x.XSetErrorHandler(previous)
        assert errors == [(self.d, invalid, 5, 17, 0)], errors
        return invalid

    def owner(self, selection):
        return self.x.XGetSelectionOwner(self.d, selection)

    def wait(self, label, predicate, seconds=3, process=None):
        return wait_for_predicate(label, predicate, time.monotonic() + seconds,
            process=process)

    def put(self, prop, kind, form, values, window=None):
        if form == 32:
            # XChangeProperty consumes one native unsigned long per 32-bit
            # element, even on LP64. Never pass a c_uint32 array to Xlib.
            raw = (C.c_ulong * len(values))(*values)
        else:
            raw = C.create_string_buffer(values)
        self.x.XChangeProperty(self.d, window or self.window, prop, kind,
            form, 0, raw, len(values))
        self.x.XSync(self.d, False)

    def read(self, prop, window=None):
        kind, count, after = C.c_ulong(), C.c_ulong(), C.c_ulong()
        form, data = C.c_int(), C.c_void_p()
        assert self.x.XGetWindowProperty(self.d, window or self.window, prop,
            0, 32769, False, 0, C.byref(kind), C.byref(form), C.byref(count),
            C.byref(after), C.byref(data)) == 0
        try:
            assert after.value == 0, ('incomplete property', after.value)
            if form.value == 32:
                native = C.cast(data, C.POINTER(C.c_ulong))
                values = [native[index] for index in range(count.value)]
                assert all(value <= 0xffffffff for value in values), values
                # Independently construct the wire-sized sequence. Reading
                # count*4 contiguous bytes from Xlib data aliases LP64 padding.
                raw = b''.join(struct.pack('=I', value) for value in values)
                assert len(raw) == 4 * count.value
            else:
                assert form.value in (0, 8, 16), form.value
                raw = C.string_at(data, count.value * form.value // 8)
            return kind.value, form.value, raw
        finally:
            if data:
                self.x.XFree(data)

    def words(self, prop, kind):
        actual, form, raw = self.read(prop)
        assert (actual, form) == (kind, 32), (actual, form)
        assert len(raw) % 4 == 0
        return list(struct.unpack('=' + 'I' * (len(raw) // 4), raw))

    def request(self, selection, target, prop=None, stamp=0, owner=None,
            deliver_to=None):
        destination = self.prop if prop is None else prop
        if owner is None:
            self.x.XConvertSelection(self.d, selection, target, destination,
                self.window, stamp)
        else:
            event = Event()
            event.request = SelectionRequest(type=30, display=self.d,
                owner=owner, requestor=self.window, selection=selection,
                target=target, property=destination, time=stamp)
            assert self.x.XSendEvent(self.d, deliver_to or owner, False, 0,
                C.byref(event))
        self.x.XFlush(self.d)
        event = Event()
        self.wait('correlated SelectionNotify', lambda:
            self.x.XCheckTypedWindowEvent(self.d, self.window, 31,
                C.byref(event)))
        reply = event.selection
        assert (reply.requestor, reply.selection, reply.target, reply.time) == (
            self.window, selection, target, stamp)
        assert reply.property in (0, destination or target), reply.property
        return reply.property

    def multiple(self, selection, pairs, kind=None, form=32):
        flat = [item for pair in pairs for item in pair]
        self.put(self.control, kind or self.atoms['ATOM_PAIR'], form, flat)
        return self.request(selection, self.atoms['MULTIPLE'], self.control)

    def metadata(self, selection):
        a = self.atoms
        assert self.request(selection, a['TARGETS']) == self.prop
        targets = self.words(self.prop, a['ATOM'])
        assert len(targets) == len(set(targets)) and len(targets) <= 256
        assert self.request(selection, a['TIMESTAMP']) == self.prop
        stamp = self.words(self.prop, a['INTEGER'])
        assert len(stamp) == 1 and stamp[0] != 0, stamp
        assert ((self.timestamp() - stamp[0]) & 0xffffffff) < 0x80000000
        return set(targets), stamp[0]

    def text(self, selection, target, expected, prop=None):
        destination = self.request(selection, target, prop)
        assert destination != 0
        assert self.read(destination) == (target, 8, expected)

    def timestamp(self):
        tick = self.atom('owner-clock')
        self.put(tick, self.atoms['STRING'], 8, b'tick')
        event = Event()

        def clock_event():
            for _ in range(256):
                if not self.x.XCheckTypedWindowEvent(self.d, self.window, 28,
                        C.byref(event)):
                    return False
                if event.property.atom == tick and event.property.state == 0:
                    return True
            return False
        self.wait('server timestamp', clock_event)
        assert event.property.atom == tick and event.property.time
        return event.property.time

    def children(self, window):
        root, parent = C.c_ulong(), C.c_ulong()
        items, count = C.POINTER(C.c_ulong)(), C.c_uint()
        assert self.x.XQueryTree(self.d, window, C.byref(root),
            C.byref(parent), C.byref(items), C.byref(count))
        try:
            assert count.value <= 4096
            return [items[index] for index in range(count.value)]
        finally:
            if items:
                self.x.XFree(items)

    def pixel(self, window, x, y):
        image = self.x.XGetImage(self.d, window, x, y, 1, 1,
            C.c_ulong(-1).value, 2)
        assert image
        try:
            return self.x.XGetPixel(image, 0, 0)
        finally:
            self.x.XDestroyImage(image)

    def named(self, title):
        # No WM/reparenting or competing module teardown in this fixture.
        for window in self.children(self.client.root):
            value = C.c_void_p()
            if self.x.XFetchName(self.d, window, C.byref(value)) and value:
                try:
                    if C.string_at(value) == title.encode():
                        return window
                finally:
                    self.x.XFree(value)
        return None

    def close(self):
        self.x.XDestroyWindow(self.d, self.window)
        self.client.close()


class Script:
    """One real Script process; only the fixture owns its native pipe ends."""

    def __init__(self, runtime, module, display, peer):
        self.runtime, self.peer = runtime, peer
        self.report = runtime.root / 'owner.report'
        source = runtime.root / 'owner.script'
        self.parent_name = b'\xa1owner-peer'
        self.parent = peer.atom(self.parent_name)
        self.selection = peer.atom(self.parent_name + b'00')
        peer.x.XSetSelectionOwner(peer.d, self.parent, peer.window, 0)
        peer.x.XSync(peer.d, False)
        payload = 'x' * 32767  # Exactly 32768 bytes including the wire NUL.
        cases = {
            10: ' ChangeTitle 1 {widget-updated}\n',
            11: ' ChangeTitle 1 {owner-two}\n',
            20: ''.join(f' SendToScript {dest} {{{text}}}\n' for dest, text in
                [(0, 'a1'), (1, 'b1'), (0, 'a2'), (1, 'b2'), (0, 'a3')]),
            21: (' Set $large={' + payload + '}\n'
                ' For $i=1 To 5 Do\n Begin\n SendToScript 0 $large\n End\n'
                ' SendToScript 1 $large {x}\n SendToScript 1 {tail}\n'),
            22: ' SendToScript 0 {retained}\n',
            24: (' SendToScript 0 {invalid-atom-first}\n'
                ' SendToScript 0 {invalid-atom-second}\n'),
            23: ' Quit\n',
        }
        source.write_text('WindowTitle {OwnerProbe}\nWindowSize 300 70\n'
            'Font {fixed}\nInit\nBegin\n Set $file={' + str(self.report) +
            '}\n WriteToFile $file {ready}\nEnd\n'
            'QuitFunc\nBegin\n WriteToFile $file {quitting}\nEnd\n'
            'Widget 1\nProperty\n Type TextField\n Position 10 10\n'
            ' Size 260 26\n Title {owner-one}\nMain\nCase message of\n' +
            ''.join(str(number) + ' :\nBegin\n' + body +
                ' WriteToFile $file {case-' + str(number) + '}\nEnd\n'
                for number, body in cases.items()) + 'End\n')
        incoming, self.producer = os.pipe()
        self.consumer, outgoing = os.pipe()
        self.descriptors = [self.producer, self.consumer]
        try:
            self.process = runtime.start('owner', [str(module), str(outgoing),
                str(incoming), '/dev/null', '0', '0', str(source),
                os.fsdecode(self.parent_name + b'00')], 'owner.log',
                pass_fds=(incoming, outgoing),
                environment=runtime.environment({'DISPLAY': display}))
        finally:
            os.close(incoming)
            os.close(outgoing)
        os.set_blocking(self.consumer, False)
        self.buffer, self.commands = bytearray(), []
        self.receive(lambda: any('Send_ConfigInfo' in c for c in self.commands))
        os.write(self.producer, packet(1 << 19))
        self.receive(lambda: 'NOP FINISHED STARTUP' in self.commands)
        self.peer.wait('Script initialization', lambda: self.report.exists()
            and 'ready' in self.report.read_text() and peer.named('OwnerProbe'),
            seconds=10, process=self.process)
        self.window = peer.named('OwnerProbe')
        self.peer.wait('TextField mapped', lambda:
            bool(peer.children(self.window)), process=self.process)
        self.field = peer.children(self.window)[0]
        self.peer.wait('private ownership', lambda: peer.owner(self.selection)
            not in (0, peer.client.root), process=self.process)

    def receive(self, predicate):
        deadline = time.monotonic() + 10
        with selectors.DefaultSelector() as ready:
            ready.register(self.consumer, selectors.EVENT_READ)
            while not predicate():
                remaining = deadline - time.monotonic()
                assert remaining > 0 and ready.select(remaining)
                data = os.read(self.consumer, 65536)
                assert data, self.commands
                self.buffer.extend(data)
                assert len(self.buffer) <= 131072
                while len(self.buffer) >= 2 * WORD:
                    _, length = struct.unpack('@LL', self.buffer[:2 * WORD])
                    assert length <= 1000
                    size = 3 * WORD + length
                    if len(self.buffer) < size:
                        break
                    self.commands.append(bytes(self.buffer[2 * WORD:
                        2 * WORD + length]).decode())
                    del self.buffer[:size]

    def command(self, number):
        os.write(self.producer, packet(1 << 22, (0, 0, 0),
            f'SendString 1 {number}'))
        marker = 'quitting' if number == 23 else f'case-{number}'
        self.peer.wait(marker, lambda: marker in self.report.read_text(),
            process=self.process)

    def select(self):
        pointer = Pointer(self.peer.client)
        x, y, child = C.c_int(), C.c_int(), C.c_ulong()
        assert self.peer.x.XTranslateCoordinates(self.peer.d, self.field,
            self.peer.client.root, 0, 0, C.byref(x), C.byref(y), C.byref(child))
        pointer.motion(x.value + 6, y.value + 12)
        pointer.button(True)
        try:
            self.peer.wait('native TextField grab', pointer.grabbed,
                process=self.process)
            # XTest/XSync confirms server delivery, not handler execution.
            # Keep the start point stable for the module's pointer query.
            time.sleep(.1)
            pointer.motion(x.value + 110, y.value + 12)
            # Let the module observe MotionNotify before server-side release.
            # A bounded pause is input sequencing, not ownership evidence.
            time.sleep(.05)
        finally:
            pointer.button(False)

    def close(self):
        for descriptor in self.descriptors:
            os.close(descriptor)
        self.descriptors.clear()


def primary_cases(peer, script, evidence):
    a, primary = peer.atoms, peer.atoms['PRIMARY']
    script.select()
    peer.wait('real TextField PRIMARY', lambda: peer.owner(primary) not in
        (0, peer.window), process=script.process)
    owner1 = peer.owner(primary)
    attrs = WindowAttributes()
    assert peer.x.XGetWindowAttributes(peer.d, owner1, C.byref(attrs))
    assert attrs.window_class == 2, 'snapshot owner must be InputOnly'
    targets, stamp1 = peer.metadata(primary)
    required = {a[name] for name in
        ('TARGETS', 'TIMESTAMP', 'MULTIPLE', 'STRING')}
    assert required <= targets <= required | {a['UTF8_STRING']}, targets
    peer.text(primary, a['STRING'], b'owner-one')
    peer.text(primary, a['STRING'], b'owner-one', prop=0)
    if a['UTF8_STRING'] in targets:
        peer.text(primary, a['UTF8_STRING'], b'owner-one')
    out, meta = peer.atom('primary-out'), peer.atom('primary-meta')
    bad = peer.atom('unsupported-conversion')
    pairs = [(a['STRING'], out), (bad, meta), (a['TIMESTAMP'], meta),
        (a['MULTIPLE'], peer.atom('nested')), (a['STRING'], 0)]
    assert peer.multiple(primary, pairs) == peer.control
    assert peer.words(peer.control, a['ATOM_PAIR']) == [
        a['STRING'], out, bad, 0, a['TIMESTAMP'], meta,
        a['MULTIPLE'], 0, a['STRING'], 0]
    assert peer.read(out) == (a['STRING'], 8, b'owner-one')
    assert peer.words(meta, a['INTEGER']) == [stamp1]
    assert peer.request(primary, a['MULTIPLE'], prop=0) == 0
    # A stale request and a foreign selection must be refused by the actual
    # current owner; XSendEvent bypasses the server's owner routing only here.
    stale = (stamp1 - 1) & 0xffffffff
    assert stale != 0
    assert peer.request(primary, a['STRING'], stamp=stale, owner=owner1) == 0
    assert peer.request(peer.atom('foreign-role'), a['STRING'],
        owner=owner1) == 0
    script.command(10)
    for index in range(8):
        peer.put(peer.atom(f'CUT_BUFFER{index}'), a['STRING'], 8,
            f'cut-buffer-{index}'.encode(), window=peer.client.root)
    peer.text(primary, a['STRING'], b'owner-one')
    assert peer.metadata(primary) == (targets, stamp1)
    evidence['primary-snapshot-metadata-multiple-cutbuffers'] = 'passed'

    script.command(11)
    script.select()
    peer.wait('confirmed fresh snapshot owner', lambda:
        peer.owner(primary) not in (0, owner1), process=script.process)
    owner2 = peer.owner(primary)
    targets2, stamp2 = peer.metadata(primary)
    assert ((stamp2 - stamp1) & 0xffffffff) < 0x80000000
    # Deliver an old-window clear to a live Script window on the same X
    # connection. Sending to the retired owner XID would test only BadWindow.
    event = Event()
    event.clear = Clear(type=29, display=peer.d, window=owner1,
        selection=primary, time=stamp1)
    assert peer.x.XSendEvent(peer.d, script.window, False, 0, C.byref(event))
    peer.x.XSync(peer.d, False)
    peer.text(primary, a['STRING'], b'owner-two')
    selected_pixel = peer.pixel(script.field, 30, 8)
    assert peer.owner(primary) == owner2
    assert peer.metadata(primary) == (targets2, stamp2)
    # Even a matching synthetic clear cannot retire a still-owned selection.
    event.clear = Clear(type=29, display=peer.d, window=owner2,
        selection=primary, time=stamp2)
    assert peer.x.XSendEvent(peer.d, script.window, False, 0, C.byref(event))
    peer.x.XSync(peer.d, False)
    peer.text(primary, a['STRING'], b'owner-two')
    assert peer.pixel(script.field, 30, 8) == selected_pixel
    # Deliver the old identity to a live window on the same connection after
    # genuine loss; do not send to an owner XID which may already be destroyed.
    peer.x.XSetSelectionOwner(peer.d, primary, peer.window, 0)
    peer.x.XSync(peer.d, False)
    assert peer.owner(primary) == peer.window
    assert peer.request(primary, a['STRING'], owner=owner2,
        deliver_to=script.window) == 0
    unselected_pixel = peer.pixel(script.field, 30, 8)
    assert unselected_pixel != selected_pixel, 'loss did not unselect TextField'
    evidence['primary-loss-visible-unselection'] = {
        'status': 'passed', 'selected_pixel': selected_pixel,
        'unselected_pixel': unselected_pixel}
    evidence['primary-confirmed-replacement-stale-clear-loss'] = 'passed'


def private_cases(peer, script, evidence):
    a, selection = peer.atoms, script.selection
    metadata = {a[name] for name in ('TARGETS', 'TIMESTAMP', 'MULTIPLE')}
    targets, stamp = peer.metadata(selection)
    assert targets == metadata, targets
    attrs = WindowAttributes()
    assert peer.x.XGetWindowAttributes(peer.d, peer.owner(selection),
        C.byref(attrs))
    assert attrs.window_class == 2
    script.command(20)
    queued_metadata = (metadata | {script.parent, selection}, stamp)
    assert peer.metadata(selection) == queued_metadata
    peer.text(selection, script.parent, b'a1\0')
    assert peer.metadata(selection) == queued_metadata
    first, second, observed = [peer.atom(name) for name in
        ('private-first', 'private-second', 'private-observed')]
    bad = peer.atom('never-enqueued')
    pairs = [(script.parent, first), (bad, second), (a['TARGETS'], observed),
        (script.parent, first), (selection, second)]
    assert peer.multiple(selection, pairs) == peer.control
    expected = [script.parent, first, bad, 0, a['TARGETS'], observed,
        script.parent, first, selection, second]
    assert peer.words(peer.control, a['ATOM_PAIR']) == expected
    assert peer.read(first) == (script.parent, 8, b'a3\0')
    assert peer.read(second) == (selection, 8, b'b1\0')
    assert set(peer.words(observed, a['ATOM'])) == queued_metadata[0]
    assert peer.metadata(selection) == (metadata | {selection}, stamp)
    peer.text(selection, selection, b'b2\0', prop=0)
    assert peer.metadata(selection) == (metadata, stamp)
    evidence['private-observational-metadata-ordered-multiple'] = 'passed'

    script.command(22)
    # Full control validation must precede any successful pair's dequeue.
    for kind, values in ((a['ATOM'], [script.parent, first]),
            (a['ATOM_PAIR'], [script.parent, first, selection]),
            (a['ATOM_PAIR'], [script.parent, first] * 129)):
        peer.put(peer.control, kind, 32, values)
        assert peer.request(selection, a['MULTIPLE'], peer.control) == 0
        assert peer.metadata(selection) == (metadata | {script.parent}, stamp)
    peer.put(peer.control, a['ATOM_PAIR'], 8, b'malformed')
    assert peer.request(selection, a['MULTIPLE'], peer.control) == 0
    assert peer.request(selection, a['MULTIPLE'], prop=0) == 0
    owner = peer.owner(selection)
    stale = (stamp - 1) & 0xffffffff
    assert stale != 0
    assert peer.request(selection, script.parent, stamp=stale, owner=owner) == 0
    assert peer.request(peer.atom('foreign-private-role'), script.parent,
        owner=owner) == 0
    pairs = [(script.parent, peer.control), (script.parent, 0),
        (a['MULTIPLE'], first), (bad, first)]
    assert peer.multiple(selection, pairs) == peer.control
    assert peer.words(peer.control, a['ATOM_PAIR']) == [
        item for target, _ in pairs for item in (target, 0)]
    peer.text(selection, script.parent, b'retained\0')
    assert peer.metadata(selection) == (metadata, stamp)
    # 128 pairs / 1024 packed control bytes are legal. A 129-pair list above
    # refuses as a whole; this at-limit list must succeed as one MULTIPLE.
    assert peer.multiple(selection, [(bad, first)] * 128) == peer.control
    assert peer.words(peer.control, a['ATOM_PAIR']) == [bad, 0] * 128
    evidence['private-malformed-control-stale-foreign-pair-bound'] = 'passed'

    script.command(24)
    invalid = peer.nonexistent_atom()
    owner = peer.owner(selection)
    assert peer.request(selection, script.parent, prop=invalid,
        owner=owner) == 0
    assert peer.request(selection, a['MULTIPLE'], prop=invalid,
        owner=owner) == 0
    assert peer.multiple(selection, [(script.parent, invalid),
        (script.parent, first)]) == peer.control
    assert peer.words(peer.control, a['ATOM_PAIR']) == [
        script.parent, 0, script.parent, first]
    assert peer.read(first) == (script.parent, 8, b'invalid-atom-first\0')
    peer.text(selection, script.parent, b'invalid-atom-second\0')
    assert peer.metadata(selection) == (metadata, stamp)
    evidence['private-nonexistent-property-atom-preserves-fifo'] = 'passed'

    script.command(21)
    outputs = [peer.atom(f'budget-{index}') for index in range(6)]
    for prop in outputs:
        peer.x.XDeleteProperty(peer.d, peer.window, prop)
    # The 48-byte control rewrite counts towards 128 KiB. Three NUL-inclusive
    # 32768-byte replies fit; two fail, while the small tail still fits.
    pairs = [(script.parent, prop) for prop in outputs[:5]] + [
        (selection, outputs[5])]
    assert peer.multiple(selection, pairs) == peer.control
    assert peer.words(peer.control, a['ATOM_PAIR']) == [
        item for index, (target, prop) in enumerate(pairs)
        for item in (target, prop if index in (0, 1, 2, 5) else 0)]
    expected = b'x' * 32767 + b'\0'
    for prop in outputs[:3]:
        assert peer.read(prop) == (script.parent, 8, expected)
    for prop in outputs[3:5]:
        assert peer.read(prop) == (0, 0, b'')
    assert peer.read(outputs[5]) == (selection, 8, b'tail\0')
    assert peer.metadata(selection) == (metadata | {script.parent}, stamp)
    peer.text(selection, script.parent, expected)
    peer.text(selection, script.parent, expected)
    assert peer.metadata(selection) == (metadata, stamp)
    evidence['private-wire-cap-and-aggregate-budget-retention'] = 'passed'

    script.command(22)
    # Confirm loss before Quit; teardown must not assign either selection to
    # root/None over a newer peer. PRIMARY has already been taken by this peer.
    previous_owner = peer.owner(selection)
    peer.x.XSetSelectionOwner(peer.d, selection, peer.window, 0)
    peer.x.XSync(peer.d, False)
    assert peer.owner(selection) == peer.window
    assert peer.request(selection, script.parent, owner=previous_owner,
        deliver_to=script.window) == 0
    script.command(23)
    assert script.process.wait_until_exit(time.monotonic() + 3) == 0
    assert peer.owner(selection) == peer.window
    assert peer.owner(a['PRIMARY']) == peer.window
    evidence['newer-primary-and-private-owner-survive-shutdown'] = 'passed'


def run(args):
    args.artifacts_dir.mkdir(parents=True, exist_ok=True)
    evidence = {'status': 'started', 'runtime': 'not completed'}
    with HarnessRuntime(prefix='fvwm-script-owner-') as runtime:
        peer = script = None
        try:
            display = args.display
            if display is None:
                _, display = start_display_server(runtime, 'Xvfb',
                    ['Xvfb', '-screen', '0', '800x600x24', '-noreset'],
                    'xserver.log', time.monotonic() + 10)
            peer = Peer(display)
            script = Script(runtime, args.module, display, peer)
            primary_cases(peer, script, evidence)
            private_cases(peer, script, evidence)
            text = script.process.log_path.read_text()
            assert not any(marker in text for marker in
                ('AddressSanitizer', 'runtime error:', 'Assertion')), text
            script.close()
            script = None
            peer.close()
            peer = None
            # Compose rather than duplicate prior count/grace/cancel controls.
            message_controls(SimpleNamespace(module=args.module,
                artifacts_dir=args.artifacts_dir / 'message-controls',
                display=display, case='all'))
            evidence['existing-40-41-and-grace-controls'] = 'passed'
            evidence.update(status='passed', runtime='completed')
        except BaseException as error:
            evidence.update(status='failed', error=repr(error))
            raise
        finally:
            if script:
                script.close()
            if peer:
                peer.close()
            for pattern in ('*.log', '*.script', '*.report'):
                for path in runtime.root.glob(pattern):
                    shutil.copy2(path, args.artifacts_dir / path.name)
            (args.artifacts_dir / 'evidence.json').write_text(
                json.dumps(evidence, indent=2) + '\n')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(allow_abbrev=False)
    parser.add_argument('--module', type=Path, required=True)
    parser.add_argument('--artifacts-dir', type=Path, required=True)
    parser.add_argument('--display')
    options = parser.parse_args()
    options.module = options.module.resolve()
    options.artifacts_dir = options.artifacts_dir.resolve()
    run(options)
