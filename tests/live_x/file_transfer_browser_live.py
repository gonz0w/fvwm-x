#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or any later version.
"""Opt-in actual browser/Form/file-worker clipboard and XDND UI acceptance.

Uses real core/MFL/Form/browser/helper binaries, XTest input, independent X11
selection peers and actual typed file jobs. No Form metadata, callback, native
generation, helper reply or job result is invented. The external drag target
runs the real typed job worker and Wait before sending XdndFinished success.
Actual incoming browser operations require their native confirmation button.

The transparent helper stdin relay owns its child's Wait. Its one explicit idle
EOF fault closes that child's stdin. URI-bearing helper output is never logged.
Build/helper overrides do not qualify installed helper discovery. With
--installed-discovery --installed-prefix PREFIX, explicit installed core/MFL/Form/
browser/helper paths and a PNG-capable fixture --capture are required. Helper
overrides are removed; --helper is an expected installed binary identity only,
never a process override. A bare
fixed-font configuration reuses nine actual UI groups, while the stdin-EOF fault
is explicitly skipped because installed mode has no helper stdin proxy. Clipboard
owner/payload continuity crosses actual Form navigation. Direct helper observation
uses only bounded portable ps pid/ppid/comm. FileManager1 service is not qualified.
Browser Close/Wait and subsequent PID absence never establish an independently
observed helper exit code. Installed helper stderr diagnostics are unavailable
because the production client discards them. Bare captures do not
qualify any of the three normal themes. Screenshots
need separate visual review; native sanitizer and platform gates remain separate.
--startup-only bounds startup to 40 seconds and stops after ready/aware/theme/
capture with zero file-job groups. Its intentional process-group teardown is
separate from the full fixture's explicit helper stdin EOF acceptance.
Captures focus/raise once, observe two read-only native list status turns, then
capture immediately. This fixture event-servicing rule is not a universal paint
fence. --capture-copy-only runs startup and actual Copy case one per theme, with
three images per profile and no full-group or helper-EOF qualification.
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
import subprocess
import sys
import time
from urllib.parse import quote_from_bytes, unquote_to_bytes, urlsplit

from browser_continuity_live import ClassHint, cleanup_receipts
from file_transfer_live import XPeer
from geometry_transition_live import SizeHints
from inspector_live import NativeView, atomic_json
from live_x_harness import HarnessRuntime, start_display_server, wait_for_predicate
from mfl_request_live import frame
from mfl_stream_live import Connection
from native_bindings_live import rows
from native_capture import NativeCapture
from native_module_proxy import module_proxy, terminate
from session_x_errors import SessionXErrors

PROFILES = ('slate-dark', 'dune-light', 'classic-large')
MAX_BYTES = 8 * 1024 * 1024
FAST_DRAG_NAME = 'z-fast-drag-item'
COPY_NAME = os.fsdecode(b'b-copy percent% name-\xff')


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def b64(value):
    return base64.b64encode(os.fsencode(value)).decode('ascii')


def uri(path):
    return 'file://' + quote_from_bytes(os.fsencode(path), safe='/') + '\r\n'


def process_records(parent=None, pid=None):
    """Bound portable metadata; decode comm only for the requested parent."""
    assert (parent is None) != (pid is None)
    child = subprocess.Popen(['ps', '-axo', 'pid,ppid,comm'],
        stdout=subprocess.PIPE, stderr=subprocess.DEVNULL)
    data = bytearray()
    deadline = time.monotonic() + 2
    try:
        os.set_blocking(child.stdout.fileno(), False)
        while True:
            remaining = deadline - time.monotonic()
            assert remaining > 0, 'bounded ps metadata deadline expired'
            ready, _, _ = select.select([child.stdout], [], [], remaining)
            assert ready, 'bounded ps metadata deadline expired'
            block = os.read(child.stdout.fileno(), min(65536,
                MAX_BYTES + 1 - len(data)))
            if not block:
                break
            data.extend(block)
            assert len(data) <= MAX_BYTES, 'ps metadata exceeds byte bound'
        assert child.wait(timeout=max(0.001, deadline - time.monotonic())) == 0
    finally:
        if child.poll() is None:
            child.kill()
        child.wait(timeout=2)
        child.stdout.close()
    records = []
    for line in bytes(data).splitlines()[1:]:
        columns = line.split(None, 2)
        if len(columns) < 2 or not all(value.isdigit() for value in columns[:2]):
            continue
        actual_pid, actual_parent = map(int, columns[:2])
        if (parent is not None and actual_parent != parent) or \
                (pid is not None and actual_pid != pid):
            continue
        comm = None
        if parent is not None:
            assert len(columns) == 3, 'missing owned child process name'
            comm = Path(columns[2].decode('utf-8', errors='replace')).name
        records.append(dict(pid=actual_pid, ppid=actual_parent, comm=comm))
    return records


def helper_proxy(real, directory, arguments):
    """Relay actual browser commands unchanged, retaining an exact child Wait.

    The child writes stdout directly to its browser. We never read or fabricate
    its events. Only an owned helper-eof marker intentionally closes child stdin.
    """
    root = Path(directory)
    token = str(os.getpid())
    errors = (root / ('helper-' + token + '.stderr')).open('wb')
    child = subprocess.Popen([real, *arguments], stdin=subprocess.PIPE,
        stdout=sys.stdout.buffer, stderr=errors)
    atomic_json(root / ('started-' + token + '.json'), dict(proxy=os.getpid(),
        child=child.pid, binary=real, started=time.monotonic(),
        argv_sha256=hashlib.sha256('\0'.join(arguments).encode()).hexdigest()))
    pending, consumed, eof, fault = bytearray(), 0, False, False
    saved = signal.getsignal(signal.SIGTERM)

    def stopped(signum, stack):
        raise KeyboardInterrupt('owned helper relay stopped')

    signal.signal(signal.SIGTERM, stopped)
    try:
        os.set_blocking(0, False)
        os.set_blocking(child.stdin.fileno(), False)
        while child.poll() is None:
            if (root / 'helper-eof').exists() and not fault:
                assert not pending, 'idle EOF fault must not truncate a command'
                fault, eof = True, True
            if eof and not pending and not child.stdin.closed:
                child.stdin.close()
            readable = [] if eof else [0]
            writable = [child.stdin] if pending and not child.stdin.closed else []
            ready, written, _ = select.select(readable, writable, [], 0.01)
            if ready:
                data = os.read(0, 65536)
                if data:
                    consumed += len(data)
                    assert consumed <= MAX_BYTES
                    pending.extend(data)
                    assert len(pending) <= 1024 * 1024
                else:
                    eof = True
            if written:
                try:
                    count = os.write(child.stdin.fileno(), pending)
                    del pending[:count]
                except (BlockingIOError, InterruptedError):
                    pass
        return child.wait(timeout=1)
    finally:
        signal.signal(signal.SIGTERM, signal.SIG_IGN)
        try:
            if not child.stdin.closed:
                child.stdin.close()
            terminate(child)
            errors.close()
            atomic_json(root / ('waited-' + token + '.json'), dict(
                proxy=os.getpid(), child=child.pid, returncode=child.returncode,
                parent_eof=eof, explicit_idle_eof=fault, bytes_forwarded=consumed,
                waited=time.monotonic()))
        finally:
            signal.signal(signal.SIGTERM, saved)


class BrowserUI:
    def __init__(self, args, profile, runtime, display, out, evidence, native,
            helpers, environment, endpoint, browser):
        self.args, self.profile, self.runtime = args, profile, runtime
        self.display, self.out, self.evidence = display, out, evidence
        self.native, self.helpers, self.env = native, helpers, environment
        self.browser, self.serial, self.provider = browser, 0, None
        self.x = self.errors = self.view = None
        self.rpc_peer = self.control = None
        self.forms = {}  # Never populate: this fixture has no Form socket peer.
        self.total = 0
        self.drop_expected = []
        self.tracefile = (out / 'independent-peer.ndjson').open('w')
        self.x = XPeer(display, self)
        self.errors = SessionXErrors(self.x, evidence).install()
        self.view = NativeView(self.x, self.errors, browser)
        self.pointer = self.view.pointer
        p, w, i = C.c_void_p, C.c_ulong, C.c_int
        for name, arguments, result in [
            ('XSetInputFocus', [p, w, i, w], i),
            ('XRaiseWindow', [p, w], i),
            ('XUnmapWindow', [p, w], i),
            ('XMoveResizeWindow', [p, w, i, i, C.c_uint, C.c_uint], i),
            ('XGetClassHint', [p, w, C.POINTER(ClassHint)], i),
            ('XSetWMNormalHints', [p, w, C.POINTER(SizeHints)], None),
        ]:
            getattr(self.x.x, name).argtypes = arguments
            getattr(self.x.x, name).restype = result
        self.x.x.XUnmapWindow(self.x.display, self.x.target)
        self.x.x.XUnmapWindow(self.x.display, self.x.source)
        self.x.x.XSync(self.x.display, 0)
        self.rpc_peer, self.control = Connection(endpoint), Connection(endpoint)
        for connection in (self.rpc_peer, self.control):
            connection.socket.settimeout(5)
            assert 'connection_profile' in connection.record()
        self.control.send(b'set echo\n')
        self.control.echo('file-transfer-browser-transport-ready')
        self.provider = self.wait(lambda: self.providers(), timeout=10)[0]['provider']
        assert len(self.providers()) == 1

    def trace(self, kind, **fields):
        if kind == 'x-receive' and self.drop_expected and \
                fields.get('window') == self.x.source and \
                fields.get('atom') == self.x.atom('XdndFinished') and \
                fields['words'][1] & 1:
            # Observe at actual X event receipt, before the ordinary case wait
            # can hide an early success behind a later completed job.
            assert all(path.read_bytes() == expected
                for path, expected in self.drop_expected)
            jobs = self.state()['jobs']
            assert not jobs['active'] and jobs['state'] == 'completed'
            self.evidence['xdnd_success_observed_after_wait'] = dict(
                jobs=jobs, event_words=fields['words'],
                observed=time.monotonic(),
                destination_sha256={str(path): digest(path)
                    for path, _ in self.drop_expected})
        line = json.dumps(dict(kind=kind, monotonic=time.monotonic(), **fields))
        self.total += len(line) + 1
        assert self.total <= MAX_BYTES
        self.tracefile.write(line + '\n')
        self.tracefile.flush()

    def tick(self):
        self.x.pump()

    def wait(self, predicate, timeout=10):
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            self.tick()
            value = predicate()
            if value:
                return value
            assert self.browser.poll() is None, 'actual browser exited'
            select.select([], [], [], 0.01)
        raise AssertionError('bounded actual UI wait expired')

    def quiet(self, seconds=0.2):
        deadline = time.monotonic() + seconds
        while time.monotonic() < deadline:
            self.tick()
            assert self.browser.poll() is None
            select.select([], [], [], 0.01)

    def rpc(self, allow_not_ready=False, **fields):
        self.serial += 1
        token = 'ft-ui-%d' % self.serial
        self.trace('rpc-send', serial=self.serial)
        try:
            self.rpc_peer.send(frame(token, resource='files', **fields))
            value = self.rpc_peer.record()['reply']
            self.trace('rpc-reply', serial=self.serial)
            assert value['id'] == token, value
            if allow_not_ready and value['status'] == 'error' and \
                    value.get('error', {}).get('code') == 'not_ready':
                return None
            assert value['status'] == 'ok', value
            return value['result']
        except BaseException:
            self.trace('rpc-failure', serial=self.serial)
            raise

    def providers(self):
        return self.rpc()['providers']

    def state(self):
        # The public MFL state scope rejects explicit pagination fields;
        # its private provider request receives the native default limit.
        return self.rpc(allow_not_ready=True, provider=self.provider,
            scope='state')

    def ready(self, path=None):
        def observed():
            state = self.state()
            if not state or state['busy'] or not state['availability']['ready'] or \
                    state['bindings']['projection']['state'] != 'current':
                return None
            if path is not None and base64.b64decode(state['path_bytes']) != \
                    os.fsencode(path):
                return None
            return state
        return self.wait(observed)

    def items(self, selection=False):
        values, generation, offset = [], '', 0
        for _ in range(8):
            result = self.rpc(provider=self.provider,
                scope='selection' if selection else 'items', limit=8,
                offset=offset, **({'generation': generation} if generation else {}))
            generation = result['generation']
            values.extend(result['items'])
            if result['next_offset'] is None:
                return values
            offset = result['next_offset']
        raise AssertionError('fixture item-page bound')

    def window(self, wait=True):
        window = self.view.wait('File browser') if wait else \
            self.view.named('File browser')
        assert window, 'actual browser Form disappeared'
        hint = ClassHint()
        assert self.x.x.XGetClassHint(self.x.display, window, C.byref(hint))
        try:
            alias = C.string_at(hint.name).decode('ascii')
        finally:
            if hint.name:
                self.x.x.XFree(hint.name)
            if hint.class_:
                self.x.x.XFree(hint.class_)
        assert alias.startswith('FvwmFiles-' + self.provider + '-')
        assert alias.endswith('-ready')
        return window, alias

    def controls(self):
        window, alias = self.window()
        definitions = []
        for record in rows(self.native):
            text = record['text']
            prefix = '*' + alias + ': '
            if record['direction'] != 'out' or not text.startswith(prefix):
                continue
            command = text[len(prefix):]
            if command.startswith('Input '):
                definitions.append(('input', command.split()[1]))
            elif command.startswith('List '):
                definitions.append(('list', command.split()[1]))
            elif command.startswith('Button continue '):
                match = re.fullmatch(r'Button continue "([^"]*)"(?: .*)?', command)
                assert match, command
                definitions.append(('button', match[1]))
        children = self.view.children(window)
        assert len(children) == len(definitions), (children, definitions)
        # Form creates these native children in declaration order. No child
        # raise/restack is issued. QueryTree returns that bottom-to-top order.
        controls = dict(zip(definitions, children))
        assert len(controls) == len(children)
        list_window = controls[('list', 'picks')]
        box = self.view.attributes(list_window)
        assert box and box.map_state == 2 and box.height > 20
        for child in children:
            actual = self.view.attributes(child)
            assert actual and actual.map_state == 2
        return window, alias, controls

    def label(self, name='transfer'):
        _, alias = self.window()
        text = None
        for record in rows(self.native):
            if record['direction'] != 'out':
                continue
            command = record['text']
            initial = re.fullmatch(re.escape('*' + alias + ': Label ' + name) +
                r' \d+ "(.*)"', command)
            changed = re.fullmatch(re.escape('SendToModule ' + alias +
                ' SetLabel ' + name) + r' "(.*)"', command)
            if initial or changed:
                text = (initial or changed)[1]
        return text

    def wait_label(self, prefix):
        return self.wait(lambda: (value if value and value.startswith(prefix)
            else None) if (value := self.label()) is not None else None)

    def focus(self):
        window, alias = self.window()
        # Raise through the WM command engine, including over retained Jobs
        # dialogs. Raising a reparented client alone cannot raise its WM frame.
        self.control.send(('WindowId %d Raise\n' % window).encode())
        self.control.echo('ft-ui-focus-' + os.urandom(4).hex())
        self.x.x.XSetInputFocus(self.x.display, window, 1, 0)
        self.x.x.XSync(self.x.display, 0)
        assert self.view.focus() == window
        return window, alias

    def key(self, symbol, control=False, alt=False):
        self.focus()
        alt_code = self.x.x.XKeysymToKeycode(self.x.display,
            self.x.x.XStringToKeysym(b'Alt_L'))
        if alt:
            assert self.pointer.x.XTestFakeKeyEvent(self.x.display, alt_code, 1, 0)
        try:
            self.view.key(symbol, control=control)
        finally:
            if alt:
                self.pointer.x.XTestFakeKeyEvent(self.x.display, alt_code, 0, 0)
            self.x.x.XSync(self.x.display, 0)
        self.trace('actual-key', symbol=symbol, control=control, alt=alt)

    def click(self, window, px=None, py=None):
        actual = self.view.attributes(window)
        assert actual and actual.map_state == 2
        point = self.x.root_point(window,
            actual.width // 2 if px is None else px,
            actual.height // 2 if py is None else py)
        self.pointer.motion(*point)
        self.pointer.button(True)
        self.pointer.button(False)
        self.trace('actual-click', window=window, point=point,
            observed_all_event_masks=int(actual.all_event_masks))

    def prepare_target(self):
        x = self.x
        root = self.view.attributes(x.root)
        assert root is not None
        desired = (max(0, root.width - 320), min(180, max(0, root.height - 200)))
        # StaticGravity keeps this fixture-owned client's configure request
        # in actual client root pixels. Wait for WM management before moving.
        hints = SizeHints(flags=1 << 9, win_gravity=10)
        x.x.XSetWMNormalHints(x.display, x.target, C.byref(hints))
        x.put(x.target, x.atom('XdndAware'), x.atom('ATOM'), 32, [5])
        x.x.XMapWindow(x.display, x.target)
        x.x.XSync(x.display, 0)

        def managed_target():
            state = x.get(x.target, x.atom('WM_STATE'))
            box = self.view.attributes(x.target)
            return state if state[:2] == (x.atom('WM_STATE'), 32) and \
                len(state[2]) == 2 and state[2][0] == 1 and box is not None \
                and box.map_state == 2 else None
        managed = self.wait(managed_target)
        x.x.XMoveResizeWindow(x.display, x.target, *desired, 220, 180)
        x.x.XSync(x.display, 0)

        def positioned_target():
            box = self.view.attributes(x.target)
            if box is None or box.map_state != 2:
                return None
            origin = x.root_point(x.target, 0, 0)
            size = (box.width, box.height)
            return dict(origin=origin, size=size) if \
                origin == desired and size == (220, 180) else None
        positioned = self.wait(positioned_target)
        self.control.send(('WindowId %d Raise\n' % x.target).encode())
        self.control.echo('ft-ui-independent-target-raised')
        fresh = positioned_target()
        assert fresh is not None, 'managed target geometry changed after Raise'
        target_point = x.root_point(x.target, 60, 60)
        self.evidence.setdefault('managed_target_readiness', []).append(dict(
            target=x.target,
            root_size=[root.width, root.height], desired_origin=desired,
            wm_state=managed, gravity=10, positioned=positioned,
            after_raise=fresh, intended_point=target_point))
        return target_point

    def target_observation(self, stage, intended):
        """Retain actual target geometry and bounded root hit paths."""
        target = self.x.target
        record = dict(stage=stage, observed=time.monotonic(), target=target,
            intended_point=intended)
        try:
            box = self.view.attributes(target)
            record['geometry'] = None if box is None else dict(
                map_state=box.map_state, origin=self.x.root_point(target, 0, 0),
                size=[box.width, box.height])
            px, py, state = self.pointer.position()
            record['pointer'] = dict(point=[px, py], state=state,
                buttons=state & 0x1f00)
            paths = {}
            for name, point in (('actual', (px, py)), ('intended', intended)):
                if point is None:
                    continue
                current, path = self.x.root, []
                for _ in range(16):
                    assert current not in path, 'target hit path cycle'
                    path.append(current)
                    local_x, local_y, child = C.c_int(), C.c_int(), C.c_ulong()
                    assert self.x.x.XTranslateCoordinates(self.x.display,
                        self.x.root, current, point[0], point[1],
                        C.byref(local_x), C.byref(local_y), C.byref(child))
                    if not child.value:
                        break
                    current = child.value
                else:
                    raise AssertionError('target hit path exceeds depth bound')
                paths[name] = path
            record['paths'] = paths
        except Exception as error:
            record['observation_failure'] = repr(error)
        self.evidence.setdefault('target_readiness_observations', []).append(record)
        return record

    def button(self, caption):
        self.focus()
        _, _, controls = self.controls()
        self.click(controls[('button', caption)])

    def focus_list(self):
        identity = self.focus()
        window, alias, controls = self.controls()
        assert (window, alias) == identity
        target = controls[('list', 'picks')]
        point = self.x.root_point(target, 20, 12)
        hit = dict(frame=window, alias=alias, list_window=target,
            intended_point=point, observed=time.monotonic(), path=[])
        self.evidence.setdefault('list_focus_hits', []).append(hit)
        current = self.x.root
        for _ in range(16):
            assert current not in hit['path'], 'list focus hit path cycle'
            hit['path'].append(current)
            local_x, local_y, child = C.c_int(), C.c_int(), C.c_ulong()
            assert self.x.x.XTranslateCoordinates(self.x.display,
                self.x.root, current, point[0], point[1],
                C.byref(local_x), C.byref(local_y), C.byref(child))
            if not child.value:
                break
            current = child.value
        else:
            raise AssertionError('list focus hit path exceeds depth bound')
        assert target in hit['path'], 'intended focus click does not hit list'
        # One ordinary click establishes logical list focus; selection is then
        # changed exclusively by actual native keys and observed MFL snapshots.
        self.click(target, 20, 12)
        self.clear_selection()

    def clear_selection(self):
        window, alias = self.window(wait=False)
        identity = (window, alias)
        epoch = re.fullmatch('FvwmFiles-' + self.provider +
            r'-([1-9][0-9]*)-ready', alias)[1]
        transcript_position = len(rows(self.native))
        after = time.monotonic()
        self.key('d', alt=True)

        def packet(field):
            assert field.startswith('x') and len(field) <= 693
            value = base64.b64decode(field[1:] + '=' * (-len(field[1:]) % 4),
                altchars=b'-_', validate=True)
            assert 6 <= len(value) <= 518
            generation = int.from_bytes(value[:4], 'big')
            assert generation > 0
            return generation, value

        def completed():
            assert self.window(wait=False) == identity, \
                'Form replaced while clearing native selection'
            records = [record for record in rows(self.native)
                if record['direction'] == 'out' and record['time'] >= after]
            prefix = 'Echo FVWM_FILES_3 %s %s clear-selection - ' % (
                self.provider, epoch)
            callbacks = [record for record in records
                if record['text'].startswith(prefix)]
            assert len(callbacks) <= 1, 'duplicate actual clear-selection callback'
            if not callbacks:
                return None
            callback = callbacks[0]
            fields = callback['text'].split()
            assert len(fields) == 8 and re.fullmatch('[0-9a-f]{16}', fields[7])
            assert int(fields[7], 16) > 0
            generation, original = packet(fields[6])
            begin = next((record for record in records
                if record['time'] > callback['time'] and record['text'] ==
                'SendToModule %s ListBegin picks %d %d' %
                (alias, generation, generation + 1)), None)
            if begin is None:
                return None
            commit = next((record for record in records
                if record['time'] > begin['time'] and record['text'] ==
                'SendToModule %s ListCommit picks %d' %
                (alias, generation + 1)), None)
            if commit is None:
                return None
            committed = None
            render_serial = None
            for record in records:
                fields = record['text'].split()
                if record['time'] <= commit['time'] or len(fields) != 7 or \
                        fields[:2] != ['Echo', 'FVWM_FORM_LIST']:
                    continue
                tag = re.fullmatch(self.provider + r'_([0-9a-f]+)_committed',
                    fields[2])
                if tag is None:
                    continue
                assert fields[3:6] == [str(generation + 1), '0', '0'], \
                    'native clear-selection commit refused'
                observed, value = packet(fields[6])
                assert observed == generation + 1 and not any(value[6:])
                committed, render_serial = record, int(tag[1], 16)
                break
            if committed is None:
                return None
            rendered = next((record for record in records
                if record['time'] > committed['time'] and record['text'] ==
                'Echo FVWM_FILES_RENDER %s %s %d %d 0' %
                (self.provider, epoch, render_serial, generation + 1)), None)
            if rendered is None:
                return None
            state = self.state()
            if state is None or state['busy'] or not state['availability']['ready'] or \
                    state['bindings']['projection']['state'] != 'current' or \
                    state['selected'] != 0:
                return None
            assert self.window(wait=False) == identity
            return dict(frame=window, alias=alias,
                transcript_position=transcript_position,
                after=after, invocation_ticket=callbacks[0]['text'].split()[7],
                callback_packet=original.hex(), callback_generation=generation,
                callback=callback, begin=begin, commit=commit, committed=committed,
                render=rendered, public_generation=state['generation'], selected=0)

        observed = self.wait(completed)
        self.evidence.setdefault('clear_selection_barriers', []).append(observed)

    def select(self, name):
        self.ready()
        values = self.items()
        wanted = next(item for item in values if
            base64.b64decode(item['name_bytes']) == os.fsencode(name))
        self.focus_list()
        self.key('Home')
        for _ in range(values.index(wanted)):
            self.key('Down')
        self.key('space')
        self.wait(lambda: self.state()['focus'] == wanted['id'] and
            self.state()['selected'] == 1)
        selected = self.items(selection=True)
        assert [base64.b64decode(item['name_bytes']) for item in selected] == \
            [os.fsencode(name)]
        return wanted

    def row_point(self, name):
        self.ready()
        identity = self.window(wait=False)
        _, _, controls = self.controls()
        window = controls[('list', 'picks')]
        box = self.view.attributes(window)

        def native_report(after, values):
            records = [record for record in rows(self.native)
                if record['direction'] == 'out' and record['time'] >= after]
            requests = {}
            for record in records:
                fields = record['text'].split()
                if len(fields) != 6 or fields[:2] != \
                        ['SendToModule', identity[1]] or fields[3] != 'picks':
                    continue
                if fields[2] == 'ListStatus' and fields[5] == 'selection':
                    requests[fields[4]] = record
                elif fields[2] == 'ListWatchStatus':
                    requests[fields[5]] = record
            reports = []
            for record in records:
                fields = record['text'].split()
                if len(fields) != 7 or fields[:2] != ['Echo', 'FVWM_FORM_LIST']:
                    continue
                request = requests.get(fields[2])
                if request is None or record['time'] <= request['time']:
                    continue
                # The earlier select() clear can report an open transaction.
                if fields[4] != '0':
                    continue
                assert fields[5] == '0', 'native row report refused'
                packet = fields[6]
                assert packet.startswith('x') and len(packet) <= 693
                value = base64.b64decode(packet[1:] + '=' * (-len(packet[1:]) % 4),
                    altchars=b'-_', validate=True)
                assert len(value) == 6 + (len(values) + 7) // 8
                generation = int.from_bytes(value[:4], 'big')
                focus = int.from_bytes(value[4:6], 'big')
                assert generation > 0 and fields[3] == str(generation)
                assert focus < len(values)
                if len(values) % 8:
                    assert value[-1] >> (len(values) % 8) == 0
                selected = [item['id'] for index, item in enumerate(values)
                    if value[6 + index // 8] & (1 << (index % 8))]
                reports.append(dict(tag=fields[2], generation=generation,
                    focus=values[focus]['id'], selected=selected,
                    request=request['text'], requested=request['time'],
                    received=record['time'], packet_sha256=
                        hashlib.sha256(value).hexdigest()))
            return reports[-1] if reports else None

        # Reveal the wanted row before every candidate; an opposite-end focus
        # reset can scroll it out of a small real viewport.
        for py in range(12, box.height - 4, max(4, box.height // 64)):
            before_time = time.monotonic()
            wanted = self.select(name)
            assert self.window(wait=False) == identity
            values = self.items()
            before = self.ready()
            before_ids = {item['id'] for item in self.items(selection=True)}
            assert before_ids == {wanted['id']} and before['focus'] == wanted['id']
            prior = native_report(before_time, values)
            assert prior is not None and set(prior['selected']) == before_ids
            assert prior['focus'] == wanted['id']
            clicked_at = time.monotonic()
            self.click(window, min(60, box.width // 3), py)

            def click_observed():
                assert self.window(wait=False) == identity
                state = self.state()
                if state is None or state['busy'] or \
                        not state['availability']['ready'] or \
                        state['bindings']['projection']['state'] != 'current':
                    return None
                selected = {item['id'] for item in self.items(selection=True)}
                if len(before_ids ^ selected) != 1:
                    return None
                native = native_report(clicked_at, values)
                if native is None:
                    return None
                assert native['generation'] == prior['generation'], \
                    'native generation changed during candidate row click'
                if set(native['selected']) != selected or native['focus'] != \
                        state['focus'] or state['selected'] != len(selected):
                    return None
                return dict(state=state, selected=selected, native=native)
            clicked = self.wait(click_observed)
            point = self.x.root_point(window, min(60, box.width // 3), py)
            receipt = dict(frame=identity[0], alias=identity[1], list_window=window,
                selected_id=wanted['id'], point=point,
                before_ids=sorted(before_ids), after_ids=sorted(clicked['selected']),
                native_before=prior, native_after=clicked['native'],
                public_generation_before=before['generation'],
                public_generation_after=clicked['state']['generation'])
            self.evidence.setdefault('row_discovery_barriers', []).append(receipt)
            if clicked['state']['focus'] == wanted['id'] and not clicked['selected']:
                self.clear_selection()
                restored_at = time.monotonic()
                self.key('space')

                def selected_wanted():
                    assert self.window(wait=False) == identity
                    state = self.state()
                    if state['focus'] != wanted['id'] or state['selected'] != 1 or \
                            state['busy'] or not state['availability']['ready'] or \
                            state['bindings']['projection']['state'] != 'current':
                        return None
                    selected = {item['id'] for item in self.items(selection=True)}
                    if selected != {wanted['id']}:
                        return None
                    native = native_report(restored_at, values)
                    return native if native is not None and \
                        set(native['selected']) == selected and \
                        native['focus'] == wanted['id'] else None
                receipt['native_restored'] = self.wait(selected_wanted)
                return point
            assert clicked['selected'] > before_ids, \
                'candidate row neither removed wanted nor added one other ID'
        raise AssertionError('actual row point was not found')

    def enter(self, directory):
        self.select(Path(directory).name)
        previous = self.window()
        self.key('Return')
        state = self.ready(directory)
        self.wait(lambda: self.window()[1] != previous[1])
        self.record_form_transition(previous)
        return state

    def up(self, directory):
        previous = self.window()
        self.key('u', alt=True)
        state = self.ready(directory)
        self.wait(lambda: self.window()[1] != previous[1])
        self.record_form_transition(previous)
        return state

    def record_form_transition(self, previous):
        current = self.window()
        # XIDs can be recycled after the old Form's X connection exits.
        # The browser's epoch-bearing alias identifies the new lifetime.
        assert current[1] != previous[1]
        self.evidence.setdefault('native_form_transitions', []).append(dict(
            old_frame=previous[0], old_alias=previous[1],
            new_frame=current[0], new_alias=current[1],
            numeric_xid_reused=current[0] == previous[0]))

    def retired_form(self, alias):
        def observed():
            records = [json.loads(path.read_text())
                for path in self.native.glob('module-*.json')]
            matches = [record for record in records
                if record['real'] == str(self.args.form) and
                alias in record['arguments']]
            assert len(matches) == 1, (alias, matches)
            record = matches[0]
            path = self.native / ('stopped-%d.json' % record['proxy'])
            if not path.exists():
                return None
            waited = json.loads(path.read_text())
            assert waited['child'] == record['child']
            assert waited['returncode'] is not None
            return dict(alias=alias, module=record, waited=waited)
        return self.wait(observed)

    def installed_helper(self):
        expected = self.args.helper.name
        # Linux task comm may be truncated to 15 bytes. Parentage plus idle
        # browser state identifies the direct helper; comm is not image proof.
        matches = [record for record in
            process_records(parent=self.browser.process.pid)
            if record['comm'] in (expected, expected[:15])]
        assert len(matches) <= 1, 'multiple direct installed transfer helpers'
        return dict(child=matches[0]['pid'], **matches[0]) if matches else None

    def aware(self):
        window, _ = self.window()
        return self.x.get(window, self.x.atom('XdndAware')) == (
            self.x.atom('ATOM'), 32, [5])

    def confirm(self, outputs, contents, move_sources=(), keyboard=False):
        self.wait_label('Confirm ')
        self.quiet()
        assert all(not path.exists() for path in outputs), \
            'mutation before confirmation'
        assert all(path.exists() for path in move_sources), \
            'source removed before confirmation'
        previous = self.state()['jobs'].get('id')
        if keyboard:
            self.key('y', alt=True)
        else:
            self.button('Confirm Copy/Move (Alt-Y)')
        self.wait_label('File transfer completed:')

        def reaped():
            jobs = self.state()['jobs']
            return jobs if jobs.get('id') != previous and \
                jobs.get('state') == 'completed' and not jobs['active'] else None
        jobs = self.wait(reaped, timeout=15)
        assert jobs['items'] >= 1
        for path, expected in zip(outputs, contents):
            assert path.read_bytes() == expected, path
        assert all(not path.exists() for path in move_sources)
        self.evidence.setdefault('actual_reaped_browser_jobs', []).append(jobs)
        self.ready()
        return jobs

    def capture_turns(self, alias, name):
        # These two real read-only turns let Form service queued X events after
        # raising. They do not prove that arbitrary clients finished painting.
        prefix = 'SendToModule %s ListStatus picks ' % alias

        def reply(record, tag):
            fields = record['text'].split()
            assert len(fields) == 7 and fields[:3] == \
                ['Echo', 'FVWM_FORM_LIST', tag]
            assert fields[4:6] == ['0', '0']
            packet = fields[6]
            assert packet.startswith('x') and len(packet) <= 693
            value = base64.b64decode(packet[1:] + '=' * (-len(packet[1:]) % 4),
                altchars=b'-_', validate=True)
            assert 6 <= len(value) <= 518
            generation = int(fields[3])
            assert generation > 0 and int.from_bytes(value[:4], 'big') == generation
            return dict(tag=tag, generation=generation, pending=0, refused=0,
                time=record['time'], selection_packet_sha256=
                    hashlib.sha256(value).hexdigest())

        records = rows(self.native)
        requests = [record for record in records if record['direction'] == 'out'
            and record['text'].startswith(prefix)
            and len(record['text'].split()) == 6
            and record['text'].split()[5] == 'selection']
        assert requests, 'no actual native status before capture'
        baseline = None
        for request in reversed(requests):
            tag = request['text'].split()[4]
            answers = [record for record in records if record['direction'] == 'out'
                and record['time'] > request['time'] and
                record['text'].startswith('Echo FVWM_FORM_LIST ' + tag + ' ')]
            assert len(answers) <= 1
            if answers:
                baseline = reply(answers[0], tag)
                break
        assert baseline is not None, 'no completed native status before capture'
        receipt = dict(alias=alias, baseline=baseline, turns=[])
        self.evidence.setdefault('capture_event_turns', {})[name] = receipt
        for _ in range(2):
            tag = 'capture_' + os.urandom(8).hex()
            after = time.monotonic()
            command = prefix + tag + ' selection'
            self.control.send((command + '\n').encode())

            def observed():
                records = [record for record in rows(self.native)
                    if record['direction'] == 'out' and record['time'] >= after]
                sent = [record for record in records if record['text'] == command]
                answers = [record for record in records if
                    record['text'].startswith('Echo FVWM_FORM_LIST ' + tag + ' ')]
                assert len(sent) <= 1 and len(answers) <= 1
                if not sent or not answers:
                    return None
                assert answers[0]['time'] > sent[0]['time']
                actual = reply(answers[0], tag)
                assert actual['generation'] == baseline['generation']
                assert actual['selection_packet_sha256'] == \
                    baseline['selection_packet_sha256']
                return dict(sent=sent[0]['time'], reply=actual)
            receipt['turns'].append(self.wait(observed))

    def capture(self, name):
        identity = self.focus()
        self.capture_turns(identity[1], name)
        previous = os.environ.get('DISPLAY')
        os.environ['DISPLAY'] = self.display
        try:
            path = Path(NativeCapture(self.args.capture).screenshot(self.out)
                .removeprefix('Screenshot saved: '))
            destination = self.out / name
            path.replace(destination)
            if self.args.installed_discovery:
                with destination.open('rb') as image:
                    assert image.read(8) == b'\x89PNG\r\n\x1a\n', \
                        'installed fixture capture did not produce PNG'
            window, alias = self.window()
            assert (window, alias) == identity, 'Form replaced during capture'
            actual = self.view.attributes(window)
            self.evidence['screenshots'][name] = dict(sha256=digest(destination),
                frame=window, alias=alias,
                actual_size=[actual.width, actual.height],
                visual_review='required; capture is not review')
        finally:
            if previous is None:
                os.environ.pop('DISPLAY', None)
            else:
                os.environ['DISPLAY'] = previous

    def external_job(self, path, destination):
        """Independent target uses the actual typed worker, with real Wait."""
        inspected = subprocess.run([str(self.args.browser), '--inspect',
            '--dir', str(path.parent)], env=self.env, capture_output=True,
            timeout=15)
        assert inspected.returncode == 0 and len(inspected.stdout) <= MAX_BYTES
        snapshot = json.loads(inspected.stdout)['snapshot']
        item = next(item for item in snapshot['items'] if
            base64.b64decode(item['name_bytes']) == os.fsencode(path.name))
        snapshot['items'] = [item]
        request = dict(schema_version=1, id='independent-xdnd-target-1',
            generation=1, operation='copy', snapshot=snapshot,
            ids=[item['id']], destination_bytes=b64(destination))
        error_path = self.out / 'independent-job.stderr'
        with error_path.open('wb') as errors:
            child = subprocess.Popen([str(self.args.browser), '--job-worker'],
                stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=errors,
                env=self.env, start_new_session=True)
            output, events = bytearray(), []
            try:
                request_bytes = (json.dumps(request) + '\n').encode()
                assert len(request_bytes) <= 8192
                child.stdin.write(request_bytes)
                child.stdin.flush()  # Keep control stdin open until actual Wait.
                os.set_blocking(child.stdout.fileno(), False)
                deadline = time.monotonic() + 15
                while child.poll() is None:
                    assert time.monotonic() < deadline, 'typed target job deadline'
                    if select.select([child.stdout], [], [], 0.01)[0]:
                        output.extend(os.read(child.stdout.fileno(), 65536))
                        assert len(output) <= MAX_BYTES
                    self.tick()
                status = child.wait(timeout=1)
                while True:
                    data = os.read(child.stdout.fileno(), 65536)
                    if not data:
                        break
                    output.extend(data)
                    assert len(output) <= MAX_BYTES
                events = [json.loads(line) for line in output.splitlines()]
                assert status == 0 and events
                assert all(event['id'] == request['id'] and
                    event['generation'] == 1 for event in events)
                assert events[-1]['kind'] == 'finished' and \
                    events[-1]['state'] == 'completed', events
                outcomes = [event['outcome'] for event in events
                    if event.get('outcome')]
                assert outcomes and all(value['content_complete'] and
                    value['metadata_complete'] for value in outcomes)
                assert (destination / path.name).read_bytes() == path.read_bytes()
                self.evidence['independent_target_real_job'] = dict(
                    pid=child.pid, wait=status, events=events,
                    destination_sha256=digest(destination / path.name),
                    role='external X target; not browser confirmation evidence')
                self.evidence.setdefault('independent_target_real_jobs', []).append(
                    self.evidence['independent_target_real_job'])
            finally:
                terminate(child)
                child.stdin.close()
                child.stdout.close()
                self.evidence['independent_target_worker_wait'] = child.returncode

    def case(self, name, operation):
        record = dict(status='running', started=time.monotonic())
        self.evidence['cases'][name] = record
        try:
            operation()
        except BaseException as error:
            record.update(status='failed', failure=repr(error))
            raise
        else:
            record.update(status='passed', seconds=time.monotonic() - record['started'])

    def close(self):
        try:
            for connection in (self.rpc_peer, self.control):
                if connection is not None:
                    connection.close()
        finally:
            self.tracefile.close()
            if self.errors is not None:
                self.errors.close_display()
            elif self.x is not None:
                self.x.close()
        if self.errors is not None:
            assert not self.errors.unexpected, self.errors.unexpected


def exercise(ui, source, destination, outside, second, drag_destination,
        fast_drag_destination):
    x = ui.x
    copy_name, cut_name, drag_name = COPY_NAME, 'c-cut-name', 'a-drag-item'
    copy_bytes = (source / copy_name).read_bytes()
    cut_bytes = (source / cut_name).read_bytes()

    def copy_navigation():
        ui.wait(ui.aware)
        ui.select(copy_name)
        ui.key('c', control=True)
        ui.wait_label('File references copied;')
        value = x.convert('CLIPBOARD', 'text/uri-list')
        paths = [unquote_to_bytes(urlsplit(line).path) for line in
            value[2].decode('ascii').splitlines() if line and not line.startswith('#')]
        assert paths == [os.fsencode(source / copy_name)]
        owner = x.x.XGetSelectionOwner(x.display, x.atom('CLIPBOARD'))
        assert owner != 0
        if not ui.args.installed_discovery:
            helpers = list(ui.helpers.glob('started-*.json'))
            assert len(helpers) == 1
            helper = json.loads(helpers[0].read_text())
        ui.enter(destination)
        assert ui.wait(ui.aware)
        if ui.args.installed_discovery:
            current_owner = x.x.XGetSelectionOwner(x.display, x.atom('CLIPBOARD'))
            assert current_owner == owner
            current = x.convert('CLIPBOARD', 'text/uri-list')
            assert current == value, 'clipboard payload changed on navigation'
            ui.evidence['installed_navigation_continuity'] = dict(
                owner_before=owner, owner_after=current_owner,
                uri_payload_sha256=hashlib.sha256(current[2]).hexdigest(),
                basis='actual X selection owner and payload; no helper proxy')
        else:
            assert json.loads(helpers[0].read_text()) == helper
        ui.focus_list()
        ui.key('v', control=True)
        ui.wait_label('Confirm Copy:')
        ui.capture('clipboard-copy-confirm.png')
        ui.confirm([destination / copy_name], [copy_bytes], keyboard=True)
        assert (source / copy_name).read_bytes() == copy_bytes
        ui.capture('clipboard-copy-completed.png')
    ui.case('actual-list-copy-navigation-paste-confirm-bytes', copy_navigation)
    if ui.args.capture_copy_only:
        return

    def cut_navigation():
        ui.up(source)
        ui.select(cut_name)
        ui.key('x', control=True)
        ui.wait_label('File references cut;')
        ui.enter(destination)
        ui.focus_list()
        ui.key('v', control=True)
        ui.wait_label('Confirm Move:')
        ui.confirm([destination / cut_name], [cut_bytes], [source / cut_name])
        ui.wait(lambda: x.x.XGetSelectionOwner(x.display, x.atom('CLIPBOARD')) == 0)
    ui.case('actual-list-cut-navigation-move-after-confirmation', cut_navigation)

    def mixed_parents():
        payload = (uri(outside / 'external-one') +
            uri(second / 'external-two')).encode()
        x.offer('CLIPBOARD', payload)
        ui.focus_list()
        ui.key('v', control=True)
        label = ui.wait_label('Confirm Copy:')
        assert '2 entries in 2 folders' in label, label
        ui.confirm([destination / 'external-one', destination / 'external-two'],
            [(outside / 'external-one').read_bytes(),
                (second / 'external-two').read_bytes()])
    ui.case('external-uri-mixed-parents-real-grouped-workers', mixed_parents)

    def invalid_references():
        before = {path.name: digest(path) for path in destination.iterdir()
            if path.is_file()}
        prior = ui.state()['jobs'].get('id')
        for text in ('file://remote.invalid/tmp/not-local\r\n',
                'file:///tmp/%GG\r\n'):
            x.offer('CLIPBOARD', text.encode())
            ui.focus_list()
            ui.key('v', control=True)
            ui.wait_label('File clipboard read ended without a usable proposal;')
            ui.quiet()
            assert ui.state()['jobs'].get('id') == prior
            assert {path.name: digest(path) for path in destination.iterdir()
                if path.is_file()} == before
    ui.case('external-malformed-remote-refusal-no-mutation', invalid_references)

    def cancelled_confirmation():
        payload = uri(outside / 'cancelled-file').encode()
        x.offer('CLIPBOARD', payload)
        ui.focus_list()
        ui.key('v', control=True)
        ui.wait_label('Confirm Copy:')
        prior = ui.state()['jobs'].get('id')
        assert ('button', 'Cancel transfer (Alt-Z)') in ui.controls()[2]
        ui.key('z', alt=True)
        ui.wait_label('File transfer stopped:')
        ui.quiet()
        assert not (destination / 'cancelled-file').exists()
        assert ui.state()['jobs'].get('id') == prior
    ui.case('actual-confirmation-cancel-no-file-job', cancelled_confirmation)

    def drop_in():
        point = ui.row_point('inner-folder')
        frame_window, _ = ui.window()
        assert ui.aware()
        x.offer('XdndSelection', uri(outside / 'drop-file').encode())
        action = x.atom('XdndActionCopy')
        x.message(frame_window, 'XdndEnter', [x.source, 5 << 24,
            x.atom('text/uri-list'), 0, 0])
        x.message(frame_window, 'XdndPosition', [x.source, 0,
            ((point[0] & 0xffff) << 16) | (point[1] & 0xffff), x.timestamp(), action])
        status = x.wait_message(x.source, 'XdndStatus')
        assert status[0] == frame_window and status[1] & 1 and status[4] == action
        ui.drop_expected = [(destination / 'drop-file',
            (outside / 'drop-file').read_bytes())]
        x.message(frame_window, 'XdndDrop', [x.source, 0, x.timestamp(), 0, 0])
        ui.wait_label('Confirm Copy:')
        ui.quiet()
        assert x.take(lambda event: event.type == 33 and
            event.message.message_type == x.atom('XdndFinished')) is None
        assert not (destination / 'drop-file').exists()
        ui.capture('xdnd-drop-confirm.png')
        ui.confirm([destination / 'drop-file'], [(outside / 'drop-file').read_bytes()])
        finished = x.wait_message(x.source, 'XdndFinished')
        assert finished[:3] == [frame_window, 1, action]
        assert not (destination / 'inner-folder' / 'drop-file').exists()
        assert not ui.state()['jobs']['active']
        assert 'xdnd_success_observed_after_wait' in ui.evidence
        ui.drop_expected = []
    ui.case('independent-xdnd-drop-actual-form-confirm-displayed-directory', drop_in)

    def drag_out():
        ui.up(source)
        point = ui.row_point(drag_name)
        ui.pointer.motion(*point)
        ui.pointer.button(True)
        ui.pointer.motion(point[0] + 12, point[1] + 12)
        ui.wait(ui.pointer.grabbed)
        ui.key('Escape')
        ui.wait(lambda: not ui.pointer.grabbed())
        ui.pointer.button(False)
        assert ui.ready(source) and (source / drag_name).exists()
        ui.evidence['actual_escape_cancelled_drag'] = True
        target_point = None
        try:
            point = ui.row_point(drag_name)
            frame_window, _ = ui.window()
            target_point = ui.prepare_target()
            ui.pointer.motion(*point)
            ui.pointer.button(True)
            ui.pointer.motion(point[0] + 12, point[1] + 12)
            ui.wait(ui.pointer.grabbed)
            before = ui.target_observation('before-final-motion', target_point)
            assert 'observation_failure' not in before
            assert x.target in before['paths']['intended'], \
                'intended root point does not hit independent target'
            assert before['pointer']['state'] & 256
            ui.pointer.motion(*target_point)
            after = ui.target_observation('after-final-motion', target_point)
            assert 'observation_failure' not in after
            assert after['pointer']['point'] == list(target_point)
            assert after['pointer']['state'] & 256
            assert x.target in after['paths']['actual'], \
                'actual drag pointer does not hit independent target'
            enter = x.wait_message(x.target, 'XdndEnter')
            assert enter[0] == frame_window and enter[1] >> 24 == 5
            position = x.wait_message(x.target, 'XdndPosition')
            action = x.atom('XdndActionCopy')
            assert position[4] == action
            x.message(frame_window, 'XdndStatus', [x.target, 3, 0, 0, action])
            converted = x.convert('XdndSelection', 'text/uri-list')
            uris = converted[2].decode('ascii').splitlines()
            assert len(uris) == 1
            parsed = urlsplit(uris[0])
            assert parsed.hostname == socket.gethostname()
            assert unquote_to_bytes(parsed.path) == os.fsencode(source / drag_name)
            assert ui.pointer.grabbed(), 'Form lost its actual pointer grab'
            ui.pointer.button(False)
            # Release can generate a final Position before the actual Drop.
            deadline = time.monotonic() + 5
            while True:
                event = x.wait_event(lambda event: event.type == 33 and
                    event.message.window == x.target and
                    event.message.message_type in (x.atom('XdndPosition'),
                        x.atom('XdndDrop')),
                    timeout=max(0.01, deadline - time.monotonic()))
                if event.message.message_type == x.atom('XdndDrop'):
                    break
                x.message(frame_window, 'XdndStatus', [x.target, 3, 0, 0, action])
            assert not (drag_destination / drag_name).exists()
            ui.external_job(source / drag_name, drag_destination)
            # This real independent target decides success only after the
            # actual copy's outcome, destination bytes and child Wait above.
            x.message(frame_window, 'XdndFinished', [x.target, 1, action, 0, 0])
            ui.quiet()
            assert (source / drag_name).exists(), 'Finished must not delete source'
            ui.ready(source)
        except BaseException:
            ui.target_observation('drag-failure', target_point)
            raise
        finally:
            ui.pointer.button(False)
            x.x.XUnmapWindow(x.display, x.target)
            x.delete(x.target, x.atom('XdndAware'))
            x.x.XSync(x.display, 0)
    ui.case('real-form-drag-out-independent-target-real-job-wait', drag_out)

    def stopped_form_fast_drag():
        point = ui.row_point(drag_name)
        values = ui.items()
        fast = next(item for item in values if
            base64.b64decode(item['name_bytes']) == os.fsencode(FAST_DRAG_NAME))
        dragged = next(item for item in values if
            base64.b64decode(item['name_bytes']) == os.fsencode(drag_name))
        ui.key('End')
        ui.wait(lambda: ui.state()['focus'] == fast['id'])
        ui.key('space')
        expected_ids = {fast['id'], dragged['id']}
        ui.wait(lambda: ui.state()['selected'] == 2 and
            {item['id'] for item in ui.items(selection=True)} == expected_ids)
        ui.ready(source)
        ui.focus()
        frame_window, alias = ui.window()
        owners = [json.loads(path.read_text()) for path in
            ui.native.glob('module-*.json')]
        owners = [owner for owner in owners if owner['real'] == str(ui.args.form)
            and alias in owner['arguments']]
        assert len(owners) == 1
        owner = owners[0]
        assert not (ui.native / ('stopped-%d.json' % owner['proxy'])).exists()
        child = owner['child']

        def kernel_state():
            observed = subprocess.run(['ps', '-o', 'ppid=', '-o', 'stat=',
                '-p', str(child)], capture_output=True, timeout=0.5)
            assert observed.returncode == 0 and len(observed.stdout) <= 1024
            fields = observed.stdout.split()
            assert len(fields) == 2 and int(fields[0]) == owner['proxy']
            return fields[1].decode('ascii')
        live_state = kernel_state()
        assert 'T' not in live_state and 'Z' not in live_state
        target_point = None
        paths = [source / drag_name, source / FAST_DRAG_NAME]
        contents = [path.read_bytes() for path in paths]
        try:
            target_point = ui.prepare_target()
            ui.pointer.motion(*point)
            before = ui.target_observation('fast-before-press', target_point)
            assert 'observation_failure' not in before
            assert x.target in before['paths']['intended']
            assert before['pointer']['buttons'] == 0
            assert ui.window(wait=False) == (frame_window, alias)
            live_state = kernel_state()
            assert 'T' not in live_state and 'Z' not in live_state
            try:
                os.kill(child, signal.SIGSTOP)

                def stopped_state():
                    state = kernel_state()
                    return state if 'T' in state else None
                stopped = ui.wait(stopped_state, timeout=1)
                ui.pointer.button(True)
                ui.pointer.motion(*target_point)
                ui.pointer.button(False)
                x.x.XSync(x.display, 0)
                after = ui.target_observation('fast-input-synced', target_point)
                assert 'observation_failure' not in after
                assert after['pointer']['point'] == list(target_point)
                assert after['pointer']['buttons'] == 0
                assert x.target in after['paths']['actual']
                ui.evidence['stopped_form_fast_drag'] = dict(alias=alias,
                    frame=frame_window, child=child, proxy=owner['proxy'],
                    parent_verified_before_stop=True, prior_state=live_state,
                    stopped_state=stopped, real_xtest_press_motion_release_synced=True,
                    selected_count=2)
            finally:
                try:
                    os.kill(child, signal.SIGCONT)
                except ProcessLookupError:
                    pass
            enter = x.wait_message(x.target, 'XdndEnter')
            assert enter[0] == frame_window and enter[1] >> 24 == 5
            position = x.wait_message(x.target, 'XdndPosition')
            action = x.atom('XdndActionCopy')
            assert position[4] == action
            x.message(frame_window, 'XdndStatus', [x.target, 3, 0, 0, action])
            converted = x.convert('XdndSelection', 'text/uri-list')
            parsed = [urlsplit(value) for value in
                converted[2].decode('ascii').splitlines()]
            assert len(parsed) == 2 and all(value.scheme == 'file' and
                value.hostname == socket.gethostname() for value in parsed)
            assert {unquote_to_bytes(value.path) for value in parsed} == \
                {os.fsencode(path) for path in paths}
            deadline = time.monotonic() + 5
            while True:
                event = x.wait_event(lambda event: event.type == 33 and
                    event.message.window == x.target and
                    event.message.message_type in (x.atom('XdndPosition'),
                        x.atom('XdndDrop')),
                    timeout=max(0.01, deadline - time.monotonic()))
                if event.message.message_type == x.atom('XdndDrop'):
                    break
                x.message(frame_window, 'XdndStatus', [x.target, 3, 0, 0, action])
            outputs = [fast_drag_destination / path.name for path in paths]
            assert all(not output.exists() for output in outputs)
            for path in paths:
                ui.external_job(path, fast_drag_destination)
            assert [output.read_bytes() for output in outputs] == contents
            x.message(frame_window, 'XdndFinished', [x.target, 1, action, 0, 0])
            ui.quiet()
            assert [path.read_bytes() for path in paths] == contents
            assert {item['id'] for item in ui.items(selection=True)} == expected_ids
            ui.wait(lambda: not ui.pointer.grabbed())
            ui.evidence['stopped_form_fast_drag']['selected_group_preserved'] = True
            ui.evidence['stopped_form_fast_drag']['sources_preserved'] = True
            ui.ready(source)
        except BaseException:
            ui.target_observation('fast-drag-failure', target_point)
            raise
        finally:
            ui.pointer.button(False)
            x.x.XUnmapWindow(x.display, x.target)
            x.delete(x.target, x.atom('XdndAware'))
            x.x.XSync(x.display, 0)
    ui.case('stopped-form-fast-drag-selected-group-real-job-waits',
        stopped_form_fast_drag)

    def retire_pending_frame():
        ui.enter(destination)
        x.offer('CLIPBOARD', uri(outside / 'stale-file').encode())
        ui.focus_list()
        ui.key('v', control=True)
        ui.wait_label('Confirm Copy:')
        old_window, old_alias = ui.window()
        prior = ui.state()['jobs'].get('id')
        ui.up(source)
        new_window, new_alias = ui.window()
        assert old_alias != new_alias
        old_wait = ui.retired_form(old_alias)
        ui.quiet()
        assert not (destination / 'stale-file').exists()
        assert ui.state()['jobs'].get('id') == prior
        ui.evidence['actual_form_retirement'] = dict(old_frame=old_window,
            new_frame=new_window, old_alias=old_alias, new_alias=new_alias,
            numeric_xid_reused=old_window == new_window, old_form_wait=old_wait,
            late_callback_injection='not performed; no fabricated callback')
    ui.case('pending-transfer-navigation-retires-actual-form', retire_pending_frame)

    def helper_eof():
        ui.wait(ui.aware)
        (ui.helpers / 'helper-eof').touch()
        waited = ui.wait(lambda: list(ui.helpers.glob('waited-*.json')))
        assert len(waited) == 1
        receipt = json.loads(waited[0].read_text())
        assert receipt['explicit_idle_eof'] and receipt['returncode'] == 0
        ui.wait_label('File transfer unavailable')
        ui.wait(lambda: not ui.aware())
        ui.enter(destination)
        assert ui.ready(destination)['file_transfer'] is True
        assert not ui.aware()
        ui.capture('helper-eof-browsing-survives.png')
        ui.evidence['helper_eof_real_wait'] = receipt
    if ui.args.installed_discovery:
        ui.evidence['cases']['actual-helper-stdin-eof-browsing-survives'] = dict(
            status='skipped', reason='installed mode has no helper stdin proxy; '
                'normal fixture separately qualifies the actual EOF fault')
    else:
        ui.case('actual-helper-stdin-eof-browsing-survives', helper_eof)


def run_profile(args, profile):
    source = Path(__file__).resolve().parents[2]
    out = args.artifacts_dir / profile
    out.mkdir()
    names = ('fvwm', 'mfl', 'form', 'browser', 'helper', 'capture')
    if not args.installed_discovery:
        names += ('desktop', 'worker')
    binaries = {name: getattr(args, name) for name in names}
    inputs = [Path(__file__).resolve(), source / 'bin/file_transfer.c',
        source / 'bin/file_transfer_policy.c', source / 'libs/file_transfer_protocol.c',
        source / 'dev-docs/decisions/FILE-TRANSFER-2026-10-31.md']
    for relative in ('bin/FvwmPrompt/cmd/fvwmfilebrowser',
            'bin/FvwmPrompt/internal/filetransfer', 'bin/FvwmPrompt/internal/filejobs',
            'bin/FvwmPrompt/internal/filemodel'):
        inputs.extend(sorted((source / relative).glob('*.go')))
    inputs.extend(source / 'modules/FvwmForm' / name for name in (
        'file_transfer_form.c', 'file_transfer_form.h', 'ReadXServer.c',
        'list_widget.c', 'FvwmForm.c'))
    inputs.extend(source / name for name in ('fvwm/ConfigFvwmDefaults',
        'bin/FvwmPrompt/meson.build'))
    inputs.extend(Path(__file__).with_name(name) for name in (
        'file_transfer_live.py', 'native_module_proxy.py', 'native_bindings_live.py',
        'browser_continuity_live.py', 'live_x_harness.py', 'session_x_errors.py',
        'inspector_live.py', 'native_capture.py', 'geometry_transition_live.py'))
    evidence = dict(status='running', profile=profile,
        startup_only=args.startup_only, installed_discovery=args.installed_discovery,
        capture_copy_only=args.capture_copy_only,
        coverage='Copy capture follow-up; one of ten UI groups per profile'
            if args.capture_copy_only else
            'startup-only' if args.startup_only else
            ('installed bare actual UI; EOF fault skipped' if args.installed_discovery
                else 'actual UI groups'),
        execution_profile=args.execution_profile, platform=sys.platform,
        fixture_argv=sys.argv, cases={}, screenshots={}, cleanup_failures=[],
        binaries={name: dict(path=str(path), sha256=digest(path))
            for name, path in binaries.items()},
        config_header=dict(path=str(args.config_header),
            sha256=digest(args.config_header)),
        source_sha256={str(path.relative_to(source)): digest(path) for path in inputs},
        limits=dict(installed_helper_discovery=
                'not qualified; explicit build helper override',
            visual_review='required separately for captures',
            forged_stale_callback_injection=
                'not performed; actual Form retirement tested',
            native_sanitizers_and_six_targets='separate qualification',
            external_target='real typed worker; not browser confirmation evidence'))
    if args.capture_copy_only:
        evidence['limits']['capture_copy_only'] = (
            'one actual Copy UI group per profile; remaining nine not run; '
            'helper EOF not qualified; visual review required')
    if args.installed_discovery:
        evidence.pop('fixture_argv')  # Keep receipt to explicit binary identity metadata.
        evidence['installed_expected_helper'] = dict(path=str(args.helper),
            sha256=digest(args.helper),
            linked_absolute_path_literal=True, override_used=False,
            identity_basis='expected installed path/hash plus compiled absolute '
                'lookup contract and actual X awareness/selection/XDND')
        evidence['limits'].update(installed_helper_discovery=
                'actual installed browser; compiled private helper; no override',
            helper_eof='skipped: no helper stdin relay',
            helper_exit_code='not independently observed; browser Wait and PID '
                'retirement are recorded separately',
            helper_diagnostics='unavailable; native helper stderr discarded by '
                'production client',
            theme_qualification='none; bare fixed font/colors only; normal three '
                'themes remain separate',
            filemanager1_service='not qualified by this UI fixture')
    runtime = HarnessRuntime(prefix='fvwm-ft-ui-')
    ui = browser = core = None
    helper_identity = None
    native, helpers = runtime.root / 'native-records', runtime.root / 'helper-records'
    failure = None
    previous_alarm = None
    try:
        if args.startup_only:
            def startup_timeout(signum, stack):
                raise TimeoutError('startup-only 40-second deadline expired')
            previous_alarm = signal.getsignal(signal.SIGALRM)
            signal.signal(signal.SIGALRM, startup_timeout)
            signal.setitimer(signal.ITIMER_REAL, 40)
        if args.display:
            display = args.display
            evidence['display_ownership'] = 'borrowed explicitly supplied display'
        else:
            _, display = start_display_server(runtime, 'Xvfb',
                ['Xvfb', '-screen', '0', '1920x1800x24', '-noreset'],
                'xserver.log', time.monotonic() + 10)
            evidence['display_ownership'] = 'owned private Xvfb'
        evidence['display'] = display
        modules, user, folder, outside, second, drag_destination = [
            runtime.root / name for name in
            ('modules', 'user', 'source', 'external-one', 'external-two',
                'drag-destination')]
        destination = folder / 'destination'
        fast_drag_destination = runtime.root / 'fast-drag-destination'
        for directory in (modules, user, folder, outside, second, drag_destination,
                destination, native, helpers, destination / 'inner-folder',
                fast_drag_destination):
            directory.mkdir(mode=0o700)
        for name, data in [('a-drag-item', b'actual native drag source\n'),
                (COPY_NAME, b'copy bytes\0with binary\xff\n'),
                ('c-cut-name', b'move bytes remain until confirmation\n'),
                (FAST_DRAG_NAME, b'actual stopped Form drag member\n')]:
            (folder / name).write_bytes(data)
        (destination / 'sentinel').write_text('focusable destination\n')
        for name in ('external-one', 'cancelled-file', 'drop-file', 'stale-file'):
            (outside / name).write_text(name + '\n')
        (second / 'external-two').write_bytes(b'second parent bytes\xff\n')
        fixture = Path(__file__).resolve()
        for name, executable in (('FvwmForm', args.form), ('FvwmMFL', args.mfl)):
            path = modules / name
            path.write_text('#!/bin/sh\nexec ' + shlex.join([sys.executable,
                str(fixture.with_name('native_module_proxy.py')),
                str(executable), str(native)]) +
                ' "$@" 2>>' + shlex.quote(str(runtime.root / (name + '.log'))) + '\n')
            path.chmod(0o700)
        config = runtime.root / 'config'
        local = None
        if args.installed_discovery:
            # Only the affected core, MFL, Form, browser and helper are product
            # dependencies. Capture/XTest/independent peers remain fixture tools.
            body = ('DefaultFont fixed\nColorset 0 fg black, bg gray90\n'
                'Colorset 1 fg black, bg gray80\n'
                '*FvwmFormDefault: Font fixed\n'
                '*FvwmFormDefault: ButtonFont fixed\n'
                '*FvwmFormDefault: InputFont fixed\n'
                '*FvwmFormDefault: Fore black\n'
                '*FvwmFormDefault: Back gray90\n'
                '*FvwmFormDefault: ItemFore black\n'
                '*FvwmFormDefault: ItemBack white\n')
            configuration_inputs = [config]
        else:
            for directory in (args.build / 'modules').iterdir():
                program = directory / directory.name
                if program.is_file() and directory.name not in ('FvwmForm', 'FvwmMFL'):
                    (modules / directory.name).symlink_to(program)
            (modules / 'FvwmDesktop').symlink_to(args.desktop)
            helper = runtime.root / 'transfer-helper'
            helper.write_text('#!/bin/sh\nexec ' + shlex.join([sys.executable,
                str(fixture), '--helper-proxy', str(args.helper), str(helpers)]) +
                ' "$@"\n')
            helper.chmod(0o700)
            assets = source / 'default-config'
            palette, appearance = ('dune', 'light') if profile == 'dune-light' else \
                ('slate', 'dark')
            local = user / 'local.config'
            local.write_text('*FvwmDesktop: palette ' + palette +
                '\n*FvwmDesktop: appearance ' + appearance +
                '\n*FvwmDesktop: motion Off\n*FvwmDesktop: transparency Off\n')
            body = (assets / 'config').read_text().replace(
                '$[FVWM_DATADIR]/default-config', str(assets))
            if profile == 'classic-large':
                body = body.replace('Sans:size=10:antialias=True',
                    'DejaVu Sans Mono:size=18:antialias=True')
                body += ('\n*FvwmFormDefault: Fore wheat\n*FvwmFormDefault: Back navy\n'
                    '*FvwmFormDefault: ItemFore black\n*FvwmFormDefault: ItemBack ivory\n')
            configuration_inputs = [config, local]
        config.write_text('ModulePath ' + str(modules) +
            '\nKillModule FvwmMFL\nSchedule 1000 Module FvwmMFL\nRead ' +
            str(source / 'fvwm/ConfigFvwmDefaults') + '\n' + body +
            '\nStyle FvwmFiles-* PositionPlacement 20 60\n')
        evidence['configuration'] = {str(path.relative_to(runtime.root)): digest(path)
            for path in configuration_inputs}
        # Retain the exact startup files before browser-created Form metadata
        # exists; private transfer nonce/helper arguments are produced separately.
        shutil.copyfile(config, out / 'session.fvwm')
        artifacts = dict(session=dict(path='session.fvwm', source='config',
            sha256=digest(out / 'session.fvwm')))
        assert artifacts['session']['sha256'] == evidence['configuration']['config']
        if local is not None:
            shutil.copyfile(local, out / 'theme.local.config')
            artifacts['theme'] = dict(path='theme.local.config',
                source='user/local.config', sha256=digest(out / 'theme.local.config'))
            assert artifacts['theme']['sha256'] == \
                evidence['configuration']['user/local.config']
        else:
            artifacts['theme'] = dict(status='absent',
                reason='installed bare configuration creates no local theme file')
        evidence['configuration_artifacts'] = artifacts
        overrides = dict(DISPLAY=display, HOME=str(runtime.root),
            FVWM_USERDIR=str(user), FVWMMFL_SOCKET_PATH=str(runtime.root),
            XDG_DATA_HOME=str(runtime.root / 'data'))
        if not args.installed_discovery:
            overrides.update(FVWM_DESKTOP_ROOT=str(assets),
                FVWM_DESKTOP_WORKER=str(args.worker),
                FVWM_FILE_TRANSFER_HELPER=str(helper))
        env = runtime.environment(overrides)
        if args.installed_discovery:
            for name in ('FVWM_FILE_TRANSFER_HELPER', 'FVWM_SESSION_BUS_HELPER',
                    'FVWM_FILEMANAGER_BUS_HELPER', 'FVWM_DESKTOP_ROOT',
                    'FVWM_DESKTOP_WORKER'):
                env.pop(name, None)
            assert 'FVWM_FILE_TRANSFER_HELPER' not in env
            assert 'FVWM_SESSION_BUS_HELPER' not in env
            assert 'FVWM_FILEMANAGER_BUS_HELPER' not in env
            evidence['helper_environment_overrides_absent'] = True
        core = runtime.start('core', [str(args.fvwm), '-d', display,
            '-f', str(config)], 'core.log', env)
        endpoint = runtime.root / ('fvwm_mfl_' + display.replace(':', '_') + '.sock')
        def proxied_mfl():
            actual = [json.loads(path.read_text())
                for path in native.glob('module-*.json')]
            return next((record for record in actual if record['real'] == str(args.mfl)
                and not (native /
                    ('stopped-%d.json' % record['proxy'])).exists()), None)
        evidence['actual_proxied_mfl'] = wait_for_predicate('actual MFL module proxy',
            proxied_mfl, time.monotonic() + 12, process=core)
        wait_for_predicate('owned MFL endpoint', endpoint.exists,
            time.monotonic() + 12, process=core)
        browser = runtime.start('browser', [str(args.browser), '--file-transfer',
            '--socket', str(endpoint), '--dir', str(folder), '--view', 'list',
            '--watch', 'off', '--opener', '/bin/true', '--terminal', '/bin/true'],
            'browser.log', env)
        ui = BrowserUI.__new__(BrowserUI)
        ui.__init__(args, profile, runtime, display, out, evidence, native,
            helpers, env, endpoint, browser)
        ui.ready(folder)
        assert ui.state()['file_transfer'] is True
        ui.wait(ui.aware)
        if args.installed_discovery:
            evidence['theme_settings'] = dict(status='not_applicable',
                reason='bare fixed-font/colors configuration; no FvwmDesktop')
            helper_identity = ui.wait(ui.installed_helper)
            evidence['installed_direct_helper'] = helper_identity
        else:
            def theme():
                ui.control.send(b'SendToModule FvwmDesktop query-settings transferui all\n')
                settings = {}
                for _ in range(128):
                    text = ui.control.record().get('echo', {}).get('message', '')
                    if text == 'desktop-settings-transferui done':
                        expected = (('palette', palette), ('appearance', appearance),
                            ('transparency', False))
                        return settings if all(
                            settings.get(name, {}).get('effective') == value
                            for name, value in expected) else None
                    if text.startswith('desktop-settings-transferui '):
                        _, key, value = text.split(' ', 2)
                        settings[key] = json.loads(value)
                raise AssertionError('theme settings response bound')
            evidence['theme_settings'] = ui.wait(theme, timeout=15)
            started_helpers = ui.wait(lambda: list(helpers.glob('started-*.json')))
            assert len(started_helpers) == 1
            helper_identity = json.loads(started_helpers[0].read_text())
        evidence['helper_idle_cost'] = dict(child=helper_identity['child'],
            observed=time.monotonic(), comparison='none; single idle observation')
        if sys.platform.startswith('linux') and not args.installed_discovery:
            values = (Path('/proc') / str(helper_identity['child']) /
                'status').read_text()
            fields = dict(line.split(':', 1) for line in values.splitlines()
                if ':' in line)
            expected_parent = browser.process.pid if args.installed_discovery \
                else helper_identity['proxy']
            assert int(fields['PPid']) == expected_parent
            evidence['helper_idle_cost'].update(
                VmRSS=fields.get('VmRSS', '').strip(),
                VmSize=fields.get('VmSize', '').strip(), source='Linux procfs status')
        else:
            evidence['helper_idle_cost']['memory'] = ('not captured; portable PID metadata only'
                if args.installed_discovery else 'not captured on BSD')
        evidence['font'] = 'fixed' if args.installed_discovery else \
            ('DejaVu Sans Mono 18' if profile == 'classic-large' else 'Sans 10')
        ui.capture('transfer-ready.png')
        if not args.startup_only:
            exercise(ui, folder, destination, outside, second, drag_destination,
                fast_drag_destination)
        assert all('file://' not in record['text'] for record in rows(native)
            if record['direction'] == 'out' and 'FvwmFiles-' in record['text'])
        evidence['status'] = 'passed'
    except BaseException as error:
        evidence.update(status='failed', failure=repr(error))
        failure = error
    finally:
        if previous_alarm is not None:
            signal.setitimer(signal.ITIMER_REAL, 0)
            signal.signal(signal.SIGALRM, previous_alarm)
        def clean(name, operation):
            nonlocal failure
            try:
                return operation()
            except BaseException as error:
                evidence['cleanup_failures'].append(dict(component=name,
                    failure=repr(error)))
                evidence['status'] = 'failed'
                if failure is None:
                    failure = error
        if browser is not None:
            if args.capture_copy_only:
                evidence['capture_copy_teardown'] = dict(
                    browser_running_before_close=browser.poll() is None,
                    action='owned process-group TERM if running; bounded Wait',
                    helper_normal_eof_qualified=False,
                    scope='intentional capture follow-up teardown')
            if args.startup_only:
                evidence['startup_teardown'] = dict(
                    browser_running_before_close=browser.poll() is None,
                    action='owned process-group TERM if running; bounded Wait',
                    helper_normal_eof_qualified=False,
                    scope='intentional fixture teardown; readiness evidence only')
            if args.installed_discovery and not args.startup_only and ui is not None:
                def close_installed_browser():
                    assert browser.poll() is None, \
                        'browser exited before actual main Form Escape Close'
                    ui.focus()
                    assert browser.poll() is None
                    ui.key('Escape')
                    status = browser.wait_until_exit(time.monotonic() + 5)
                    assert status == 0
                    evidence['actual_installed_browser_close_wait'] = dict(
                        returncode=status, action='actual main Form Escape Close')
                clean('actual installed browser Close and Wait', close_installed_browser)
            clean('actual browser Wait', lambda: browser.close(time.monotonic() + 5))
            if args.installed_discovery and helper_identity is not None:
                def retired_installed_helper():
                    pid = helper_identity['pid']
                    wait_for_predicate('direct installed helper PID retirement',
                        lambda: not process_records(pid=pid), time.monotonic() + 5)
                    evidence['installed_direct_helper_retirement'] = dict(
                        pid=pid, absent_after_browser_wait=True,
                        helper_exit_code='not independently observed',
                        helper_diagnostics='unavailable; native helper stderr '
                            'discarded by production client')
                clean('direct installed helper retirement', retired_installed_helper)
        if ui is not None:
            clean('independent X/MFL observers', ui.close)
        if core is not None:
            clean('actual core Wait', lambda: core.close(time.monotonic() + 5))
        if native.exists():
            receipts = clean('native module child Wait receipts',
                lambda: cleanup_receipts(native))
            if receipts is not None:
                evidence['native_child_waits'] = receipts
        if helpers.exists() and not args.installed_discovery:
            def helper_waits():
                evidence['helper_child_waits'] = [json.loads(path.read_text())
                    for path in helpers.glob('waited-*.json')]
                if args.startup_only and 'startup_teardown' in evidence:
                    evidence['startup_teardown']['actual_helper_exit_statuses'] = [
                        receipt['returncode']
                        for receipt in evidence['helper_child_waits']]
                started = list(helpers.glob('started-*.json'))
                assert len(evidence['helper_child_waits']) == len(started)
                assert all(b'unexpected X error' not in path.read_bytes()
                    for path in helpers.glob('*.stderr'))
            clean('actual helper child Wait receipts', helper_waits)
        for process in reversed(runtime.processes):
            clean(process.name + ' Wait', lambda process=process:
                process.close(time.monotonic() + 3))
        for directory in (native, helpers):
            if directory.exists():
                clean('retain ' + directory.name, lambda directory=directory:
                    shutil.copytree(directory, out / directory.name))
        for path in runtime.root.glob('*.log'):
            clean('retain ' + path.name, lambda path=path:
                shutil.copyfile(path, out / path.name))
        evidence['passed_ui_groups'] = sum(record['status'] == 'passed'
            for record in evidence['cases'].values())
        evidence['skipped_ui_groups'] = {name: record['reason']
            for name, record in evidence['cases'].items()
            if record['status'] == 'skipped'}
        evidence['process_waits'] = [dict(name=process.name, pid=process.process.pid,
            returncode=process.process.returncode) for process in runtime.processes]
        evidence['actual_job_groups'] = len(
            evidence.get('actual_reaped_browser_jobs', [])) + len(
                evidence.get('independent_target_real_jobs', []))
        clean('runtime cleanup', runtime.close)
        atomic_json(out / 'results.json', evidence)
    if failure is not None:
        raise failure
    return evidence


def main():
    if len(sys.argv) > 1 and sys.argv[1] == '--module-proxy':
        return module_proxy(sys.argv[2], sys.argv[3], sys.argv[4:])
    if len(sys.argv) > 1 and sys.argv[1] == '--helper-proxy':
        return helper_proxy(sys.argv[2], sys.argv[3], sys.argv[4:])
    parser = argparse.ArgumentParser(description=__doc__, allow_abbrev=False)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--config-header', type=Path)
    parser.add_argument('--artifacts-dir', type=Path, required=True)
    parser.add_argument('--execution-profile', required=True)
    parser.add_argument('--display', help='borrow caller-owned authenticated display')
    parser.add_argument('--installed-discovery', action='store_true',
        help='bare installed browser/private-helper discovery; nine UI groups; '
            'helper stdin-EOF fault explicitly skipped; no theme qualification')
    parser.add_argument('--installed-prefix', type=Path,
        help='actual configured private install prefix for installed discovery')
    parser.add_argument('--startup-only', action='store_true',
        help='40-second ready/aware/theme/capture check; zero file-job groups')
    parser.add_argument('--capture-copy-only', action='store_true',
        help='themed startup+actual Copy group one of ten; three PNGs per profile; '
            'no helper EOF qualification')
    parser.add_argument('--profiles', nargs='+', choices=PROFILES,
        default=None)
    defaults = dict(fvwm='fvwm3', mfl='modules/FvwmMFL/FvwmMFL',
        form='modules/FvwmForm/FvwmForm', browser='bin/FvwmPrompt/fvwm-file-browser',
        helper='bin/fvwm-file-transfer', desktop='bin/FvwmPrompt/FvwmDesktop',
        worker='bin/FvwmPrompt/fvwm-desktop-worker', capture='bin/fvwm-desktop-capture')
    for name in defaults:
        parser.add_argument('--' + name, type=Path)
    args = parser.parse_args()
    if not sys.platform.startswith(('linux', 'freebsd', 'openbsd')):
        parser.error('actual UI fixture supports Linux/FreeBSD/OpenBSD only')
    if args.capture_copy_only and (args.startup_only or args.installed_discovery):
        parser.error('--capture-copy-only requires normal themed mode, without '
            '--startup-only/--installed-discovery')
    installed_names = ('fvwm', 'mfl', 'form', 'browser', 'helper')
    if args.installed_discovery:
        if args.profiles is not None or args.installed_prefix is None or \
                args.capture is None or \
                any(getattr(args, name) is None for name in installed_names):
            parser.error('--installed-discovery requires --installed-prefix and '
                'explicit --fvwm/--mfl/--form/--browser/--helper and PNG-capable '
                'fixture --capture; no --profiles')
        args.installed_prefix = args.installed_prefix.resolve(strict=True)
        args.profiles = ['installed-bare']
    else:
        if args.installed_prefix is not None:
            parser.error('--installed-prefix requires --installed-discovery')
        args.profiles = args.profiles or list(PROFILES)
    args.build = args.build.resolve(strict=True)
    args.config_header = (args.config_header or args.build /
        'config.h').resolve(strict=True)
    for name, relative in defaults.items():
        if args.installed_discovery and name in ('desktop', 'worker'):
            continue
        setattr(args, name, (getattr(args, name) or args.build /
            relative).resolve(strict=True))
    if args.installed_discovery:
        for name in installed_names:
            path = getattr(args, name)
            assert path.is_relative_to(args.installed_prefix) and \
                path.is_file() and os.access(path, os.X_OK), \
                'expected actual executable inside private install prefix'
        assert args.helper.name == 'fvwm-file-transfer'
        assert args.helper.parent == args.mfl.parent == args.form.parent, \
            'helper and native modules must share installed private directory'
        assert os.fsencode(args.helper) in args.browser.read_bytes(), \
            'installed browser lacks expected absolute private-helper path literal'

    args.artifacts_dir = args.artifacts_dir.resolve()
    args.artifacts_dir.mkdir(parents=True, exist_ok=False)
    results = [run_profile(args, profile) for profile in args.profiles]
    atomic_json(args.artifacts_dir / 'results.json', dict(status='passed',
        startup_only=args.startup_only, installed_discovery=args.installed_discovery,
        capture_copy_only=args.capture_copy_only,
        coverage='Copy capture follow-up; one of ten UI groups per profile'
            if args.capture_copy_only else
            'startup-only' if args.startup_only else
            ('installed bare actual UI; EOF fault skipped' if args.installed_discovery
                else 'actual UI groups'),
        actual_job_groups=sum(value['actual_job_groups'] for value in results),
        profiles=args.profiles, mandatory_profiles=list(PROFILES),
        all_profiles_exercised=not args.startup_only and not args.capture_copy_only and
            set(args.profiles) == set(PROFILES),
        execution_profile=args.execution_profile,
        groups={value['profile']: value['passed_ui_groups'] for value in results},
        skipped_groups={value['profile']: value['skipped_ui_groups']
            for value in results}, visual_review='required separately',
        installed_discovery_scope=('startup-only actual compiled private helper; '
            'zero UI job groups; no theme qualification' if args.startup_only else
            'actual compiled private helper, nine UI groups; EOF skipped; '
            'no theme qualification') if args.installed_discovery else
            'not qualified; helper override'))
    print(json.dumps(dict(status='passed', profiles=args.profiles,
        startup_only=args.startup_only, capture_copy_only=args.capture_copy_only,
        actual_job_groups=sum(value['actual_job_groups'] for value in results),
        artifacts=str(args.artifacts_dir))))
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
