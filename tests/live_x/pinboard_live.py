#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
"""Independent actual-X and data-only acceptance for the opt-in pinboard.

Only this fixture's children, private X server, XDG directories and synthetic
files are controlled. Cells locates targets, while X properties, propagated
SHAPE, root bindings, actual Form callbacks and committed state bytes establish
behavior. No native response or worker result is manufactured. --native-only
is the scoped server contract; --shape-unavailable uses an owned Xvfb server.
Execution belongs to the lead, including native/BSD and screenshot acceptance.
"""
import argparse
import base64
import ctypes as C
import ctypes.util
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
import sys
import tempfile
import time
import traceback
import zlib

from browser_continuity_live import cleanup_receipts
from desktop_live import connect_client
from inspector_live import NativeView, atomic_json
from live_x_harness import (HarnessRuntime, start_display_server,
    start_xorg_dummy, wait_for_predicate, wait_for_randr,
    wait_for_authoritative_state, run_required_xrandr)
from mfl_stream_live import Connection
from native_bindings_live import module_proxy, rows
from native_capture import NativeCapture
from session_x_errors import SessionXErrors

PROFILES = ('slate-dark', 'dune-light', 'classic-large')
SCALES = {'slate-dark': '1.25', 'dune-light': '1', 'classic-large': '2'}
MARKER = b'FVWM-X synthetic pinboard fixture\n'
MAX_RECORD = 1024 * 1024


def digest(path):
    value = hashlib.sha256()
    with open(path, 'rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            value.update(block)
    return value.hexdigest()


def published_picture_file(spec):
    """Decode only iconrender's private SVG :WIDTHxHEIGHT specification.

    iconrender/render.go appends the physical square size after its private
    content-addressed SVG filename. PNG/XPM publications are literal files.
    Other colons remain literal: this is not a general FVWM picture parser.
    """
    match = re.fullmatch(
        r'(.*/\.fvwm-xdg-[0-9a-f]{64}\.svg):([1-9][0-9]{0,4})x([1-9][0-9]{0,4})',
        spec)
    if match:
        width, height = int(match[2]), int(match[3])
        assert width == height and width <= 65535, spec
        return Path(match[1]), [width, height]
    return Path(spec), None


def solid_png(width, height, color):
    """Small bounded RGB/RGBA publication for actual native blit oracles."""
    assert 1 <= width <= 128 and 1 <= height <= 128
    assert len(color) in (3, 4)
    pixel = bytes(color)
    def chunk(kind, data):
        return (struct.pack('>I', len(data)) + kind + data +
            struct.pack('>I', zlib.crc32(kind + data)))
    raw = (b'\0' + pixel * width) * height
    return (b'\x89PNG\r\n\x1a\n' +
        chunk(b'IHDR', struct.pack('>IIBBBBB', width, height, 8,
            2 if len(color) == 3 else 6, 0, 0, 0)) +
        chunk(b'IDAT', zlib.compress(raw)) + chunk(b'IEND', b''))


class EchoStream:
    """Bound complete records as well as the socket's receive deadline."""

    def __init__(self, endpoint):
        self.peer = Connection(endpoint)
        self.peer.socket.settimeout(2)
        self.queue = []
        self.total = 0
        self.peer.record()
        self.peer.send(b'set echo\n')
        self.barrier()

    def send(self, command):
        wire = command.encode('ascii')
        assert 0 < len(wire) <= 1000 and not any(v in wire for v in (0, 10, 13))
        self.peer.send(wire + b'\n')

    def pump(self):
        for _ in range(16):
            if not select.select([self.peer.socket], [], [], 0)[0]:
                break
            data = self.peer.socket.recv(65536)
            assert data, 'MFL disconnected'
            self.peer.pending += data
            assert len(self.peer.pending) <= MAX_RECORD
        while b'\n' in self.peer.pending:
            line, self.peer.pending = self.peer.pending.split(b'\n', 1)
            self.total += 1
            assert self.total <= 65536
            record = json.loads(line)
            message = record.get('echo', {}).get('message')
            if message is not None:
                self.queue.append(message)
                assert len(self.queue) <= 8192
        return self.queue

    def take(self, prefix, timeout=5):
        def observed():
            for index, message in enumerate(self.pump()):
                if message.startswith(prefix):
                    return self.queue.pop(index)
            return None
        return wait_for_predicate('actual Echo ' + prefix, observed,
            time.monotonic() + timeout)

    def barrier(self):
        tag = 'pinboard-core-' + os.urandom(8).hex()
        self.send('Echo ' + tag)
        assert self.take(tag) == tag

    def close(self):
        self.peer.close()


class Rectangle(C.Structure):
    _fields_ = [('x', C.c_short), ('y', C.c_short),
        ('width', C.c_ushort), ('height', C.c_ushort)]


class ClassHint(C.Structure):
    _fields_ = [('name', C.c_void_p), ('class_', C.c_void_p)]


class PropertyEvent(C.Structure):
    _fields_ = [('type', C.c_int), ('serial', C.c_ulong),
        ('send_event', C.c_int), ('display', C.c_void_p),
        ('window', C.c_ulong), ('atom', C.c_ulong),
        ('time', C.c_ulong), ('state', C.c_int)]


class Event(C.Union):
    _fields_ = [('property', PropertyEvent), ('pad', C.c_long * 24)]


class XOracle:
    def __init__(self, client, view):
        self.client, self.view = client, view
        self.x, self.d = client.x, client.display
        ptr, word = C.c_void_p, C.c_ulong
        signatures = {
            'XInternAtom': ([ptr, C.c_char_p, C.c_int], word),
            'XGetSelectionOwner': ([ptr, word], word),
            'XSetSelectionOwner': ([ptr, word, word, word], C.c_int),
            'XDefaultScreen': ([ptr], C.c_int),
            'XGetWindowProperty': ([ptr, word, word, C.c_long, C.c_long,
                C.c_int, word, C.POINTER(word), C.POINTER(C.c_int),
                C.POINTER(word), C.POINTER(word), C.POINTER(ptr)], C.c_int),
            'XChangeProperty': ([ptr, word, word, word, C.c_int, C.c_int,
                ptr, C.c_int], C.c_int),
            'XCheckWindowEvent': ([ptr, word, C.c_long, C.POINTER(Event)], C.c_int),
            'XTranslateCoordinates': ([ptr, word, word, C.c_int, C.c_int,
                C.POINTER(C.c_int), C.POINTER(C.c_int), C.POINTER(word)], C.c_int),
            'XGetClassHint': ([ptr, word, C.POINTER(ClassHint)], C.c_int),
            'XStoreBuffer': ([ptr, C.c_char_p, C.c_int, C.c_int], C.c_int),
            'XGetImage': ([ptr, word, C.c_int, C.c_int, C.c_uint, C.c_uint,
                word, C.c_int], ptr),
            'XGetPixel': ([ptr, C.c_int, C.c_int], word),
            'XDestroyImage': ([ptr], C.c_int),
        }
        for name, (arguments, result) in signatures.items():
            function = getattr(self.x, name)
            function.argtypes, function.restype = arguments, result
        library = ctypes.util.find_library('Xext')
        assert library, 'libXext unavailable'
        self.shape = C.CDLL(library)
        self.shape.XShapeQueryExtension.argtypes = [ptr, C.POINTER(C.c_int),
            C.POINTER(C.c_int)]
        self.shape.XShapeQueryExtension.restype = C.c_int
        self.shape.XShapeGetRectangles.argtypes = [ptr, word, C.c_int,
            C.POINTER(C.c_int), C.POINTER(C.c_int)]
        self.shape.XShapeGetRectangles.restype = C.POINTER(Rectangle)
        self.selection = self.atom('_NET_DESKTOP_MANAGER_S%d' %
            self.x.XDefaultScreen(self.d))

    def atom(self, name):
        return self.x.XInternAtom(self.d, name.encode('ascii'), 0)

    def owner(self):
        return self.x.XGetSelectionOwner(self.d, self.selection)

    def property(self, window, name):
        actual, count, remaining = C.c_ulong(), C.c_ulong(), C.c_ulong()
        form, data = C.c_int(), C.c_void_p()
        try:
            status = self.x.XGetWindowProperty(self.d, window, self.atom(name),
                0, 4096, 0, 0, C.byref(actual), C.byref(form), C.byref(count),
                C.byref(remaining), C.byref(data))
            assert status == 0 and remaining.value == 0 and count.value <= 4096
            if not actual.value:
                return dict(type=0, format=0, values=[])
            assert form.value in (8, 16, 32)
            scalar = {8: C.c_ubyte, 16: C.c_ushort, 32: C.c_ulong}[form.value]
            assert count.value == 0 or data.value
            values = list(C.cast(data, C.POINTER(scalar))[:count.value])
            if form.value == 32:
                # Xlib stores format-32 wire values in unsigned long slots.
                # LP64 replies may sign-extend high-bit CARDINALs in that slot.
                values = [value & 0xffffffff for value in values]
            return dict(type=actual.value, format=form.value,
                values=values)
        finally:
            if data.value:
                self.x.XFree(data)

    def words(self, window, name, expected_type):
        value = self.property(window, name)
        assert value['type'] == expected_type and value['format'] == 32, value
        return value['values']

    def wallpaper(self):
        return {name: self.property(self.client.root, name) for name in
            ('_XROOTPMAP_ID', 'ESETROOT_PMAP_ID', '_XSETROOT_ID')}

    def rectangles(self, window):
        count, ordering = C.c_int(), C.c_int()
        rects = self.shape.XShapeGetRectangles(self.d, window, 0,
            C.byref(count), C.byref(ordering))
        assert rects and 0 < count.value <= 1024
        try:
            return [[r.x, r.y, r.width, r.height] for r in rects[:count.value]]
        finally:
            self.x.XFree(rects)

    def origin(self, window):
        x, y, child = C.c_int(), C.c_int(), C.c_ulong()
        assert self.x.XTranslateCoordinates(self.d, window, self.client.root,
            0, 0, C.byref(x), C.byref(y), C.byref(child))
        return [x.value, y.value]

    def top(self, window):
        for _ in range(8):
            parent = self.client.parent(window)
            if parent == self.client.root:
                return window
            window = parent
        raise AssertionError('unbounded parent chain')

    def alias(self, window):
        hint = ClassHint()
        assert self.x.XGetClassHint(self.d, window, C.byref(hint))
        try:
            return C.string_at(hint.name).decode('ascii')
        finally:
            if hint.name:
                self.x.XFree(hint.name)
            if hint.class_:
                self.x.XFree(hint.class_)

    def timestamp(self, window):
        atom = self.atom('_PINBOARD_FIXTURE_TIME')
        self.x.XSelectInput(self.d, window, 1 << 22)
        value = C.c_ubyte(1)
        self.x.XChangeProperty(self.d, window, atom, 19, 8, 0,
            C.byref(value), 1)
        self.x.XSync(self.d, 0)
        def observed():
            event = Event()
            if self.x.XCheckWindowEvent(self.d, window, 1 << 22, C.byref(event)):
                if event.property.type == 28 and event.property.atom == atom:
                    return event.property.time
            return None
        return wait_for_predicate('owned selection peer timestamp', observed,
            time.monotonic() + 2)

    def click(self, x, y, button=1):
        self.view.pointer.motion(x, y)
        for pressed in (1, 0):
            assert self.view.pointer.x.XTestFakeButtonEvent(self.d, button,
                pressed, 0)
        self.x.XSync(self.d, 0)

    def pixel_samples(self, window, points):
        attrs = self.view.attributes(window)
        assert attrs and attrs.map_state == 2
        image = self.x.XGetImage(self.d, window, 0, 0, attrs.width,
            attrs.height, C.c_ulong(-1).value, 2)
        assert image
        try:
            assert all(0 <= x < attrs.width and 0 <= y < attrs.height
                for x, y in points)
            return [self.x.XGetPixel(image, x, y) for x, y in points]
        finally:
            self.x.XDestroyImage(image)


class NativeProtocol:
    def __init__(self, echo, alias, token, evidence):
        assert re.fullmatch(r'[0-9a-f]{32}', token)
        self.echo, self.alias, self.token = echo, alias, token
        self.evidence = evidence

    def send(self, text):
        self.echo.send('SendToModule ' + self.alias + ' ' + text)

    def status(self, timeout=5):
        tag = 'observer-s-' + os.urandom(6).hex()
        self.send('Status ' + self.token + ' ' + tag)
        raw = self.echo.take('FVWM_PINBOARD_STATUS ' + self.token + ' ' + tag + ' ',
            timeout=timeout)
        fields = raw.split()
        assert len(fields) == 8, raw
        value = dict(raw=raw, current=int(fields[3]), pending=int(fields[4]),
            state=fields[5], window=int(fields[6]), epoch=fields[7])
        assert re.fullmatch(r'[0-9a-f]{16}', value['epoch']), raw
        self.evidence.setdefault('native_status', []).append(value)
        assert len(self.evidence['native_status']) <= 512
        return value

    def cells(self):
        # Status is a stream barrier, not a reservation on the next Cells
        # request. Topology may replace the publication between requests.
        # Restart the whole acquisition, never combine pages from two views.
        deadline = time.monotonic() + 5
        def remaining():
            value = deadline - time.monotonic()
            if value <= 0:
                raise TimeoutError('Cells acquisition exceeded five seconds')
            return value
        for attempt in range(3):
            state = self.status(timeout=remaining())
            publication = (state['current'], state['epoch'])
            entries, expected_total, offset = [], None, 0
            for _ in range(32):
                tag = 'observer-c-' + os.urandom(6).hex()
                self.send('Cells %s %s %d 8' % (self.token, tag, offset))
                end = self.echo.take('FVWM_PINBOARD_CELLS_END %s %s ' %
                    (self.token, tag), timeout=remaining())
                parts = end.split()
                assert len(parts) == 8, end
                view, epoch = int(parts[3]), parts[4]
                start, count, total = map(int, parts[5:])
                assert 0 <= count <= 8 and 0 <= total <= 256 and start == offset
                assert count == min(8, max(0, total - offset))
                assert re.fullmatch(r'[0-9a-f]{16}', epoch) and int(epoch, 16)
                page_publication, page = (view, epoch), []
                for index in range(count):
                    raw = self.echo.take('FVWM_PINBOARD_CELL %s %s ' %
                        (self.token, tag), timeout=remaining())
                    fields = raw.split()
                    assert len(fields) == 16, raw
                    assert (int(fields[3]), fields[4]) == page_publication, \
                        'one Cells page mixed row/END publications'
                    row = dict(index=int(fields[5]), id=fields[6], monitor=fields[7],
                        quarters=int(fields[8]), column=int(fields[9]), row=int(fields[10]),
                        x=int(fields[11]), y=int(fields[12]), width=int(fields[13]),
                        height=int(fields[14]), visible=int(fields[15]))
                    assert row['index'] == offset + index
                    assert re.fullmatch(r'[0-9a-f]{32}', row['id'])
                    page.append(row)
                if page_publication != publication:
                    self.evidence.setdefault('cells_restarts', []).append(dict(
                        attempt=attempt + 1, offset=offset,
                        old=publication, new=page_publication,
                        discarded_entries=len(entries) + len(page)))
                    break
                if expected_total is None:
                    expected_total = total
                assert total == expected_total and offset <= total
                entries.extend(page)
                offset += count
                if offset == total:
                    self.evidence.setdefault('cells', []).append(dict(view=view,
                        epoch=epoch, entries=entries))
                    return view, epoch, entries
            else:
                raise AssertionError('Cells page bound exceeded')
        raise AssertionError('Cells changed publication in three bounded acquisitions')


def run(args, profile, run_root):
    source, build = Path(__file__).resolve().parents[2], args.build.resolve()
    out = run_root / profile
    out.mkdir()
    scale = args.scale or SCALES[profile]
    svg_helper = None
    binaries = [build / 'fvwm3', build / 'modules/FvwmMFL/FvwmMFL',
        build / 'modules/FvwmPinboard/FvwmPinboard']
    if not args.native_only and not args.shape_unavailable:
        binaries += [build / 'modules/FvwmForm/FvwmForm',
            build / 'bin/FvwmPrompt/fvwm-pinboard',
            build / 'bin/FvwmPrompt/fvwm-file-browser',
            build / 'bin/FvwmPrompt/FvwmDesktop',
            build / 'bin/fvwm-desktop-capture']
        if args.named_icon_format == 'svg':
            svg_helper = build / 'bin/fvwm-image-svg-worker'
            if svg_helper.is_file():
                binaries.append(svg_helper)
    inputs = [Path(__file__).resolve(), source / 'dev-docs/decisions/PINBOARD-2026-10-27.md']
    for directory in ('modules/FvwmPinboard', 'bin/FvwmPrompt/internal/pinboard',
            'bin/FvwmPrompt/cmd/fvwmpinboard'):
        inputs += sorted(p for p in (source / directory).glob('*') if p.is_file())
    inputs += [source / 'tests/live_x' / name for name in ('live_x_harness.py',
        'desktop_live.py', 'inspector_live.py', 'native_bindings_live.py',
        'browser_continuity_live.py', 'mfl_stream_live.py', 'native_capture.py',
        'geometry_transition_live.py', 'session_x_errors.py', 'colorset_live.py')]
    inputs += [source / name for name in ('libs/module_workarea.c',
        'libs/module_workarea.h', 'libs/module_ui_scale.c',
        'modules/FvwmForm/FvwmForm.c', 'libs/PictureBase.c',
        'bin/FvwmPrompt/internal/iconrender/render.go')]
    evidence = dict(status='running', profile=profile, screenshots=[], checks=[],
        failures=[], cleanup_failures=[], source_sha256={str(p.relative_to(source)):
            digest(p) for p in inputs}, binary_sha256={str(p.relative_to(build)):
            digest(p) for p in binaries}, requested_scale=scale,
        scope='native pixel containment only' if args.crop_only else
            ('SHAPE unavailable' if args.shape_unavailable else
            ('native server contract' if args.native_only else 'data-only pinboard and native server')),
        limits=dict(max_pins=256, cells_pages=32, cells_page_size=8,
            cells_acquisition_attempts=3, cells_acquisition_seconds=5,
            fixture_targets=12, physical_display_qualified=False,
            native_execution_environment='caller must identify native versus emulated',
            automatic_module_scaling_qualified=bool(args.xorg_config),
            filesystem_payload_archived=False, timing_gate=False))
    with HarnessRuntime(prefix='fvwm-pinboard-') as runtime:
        client = errors = core = owner = browser = echo = None
        native = runtime.root / 'native-records'
        successor = None
        try:
            if svg_helper is not None:
                evidence['svg_helper'] = dict(path=str(svg_helper),
                    child_environment={'FVWM_IMAGE_SVG_WORKER': str(svg_helper)},
                    caller_environment_modified=False)
                if not svg_helper.is_file() or not os.access(svg_helper, os.X_OK):
                    evidence.update(status='unavailable', setup_limit=
                        'SVG named-icon profile needs executable build/bin/fvwm-image-svg-worker')
                    return 'unavailable'
                evidence['svg_helper'].update(bytes=svg_helper.stat().st_size,
                    sha256=digest(svg_helper))
            modules, user, folder = [runtime.root / name for name in ('modules', 'user', 'files')]
            for path in (modules, user, folder, native):
                path.mkdir(mode=0o700)
            (folder / '.pinboard-fixture').write_bytes(MARKER)
            originals = {'notes.txt': b'literal source bytes\0\xff\n',
                'new-reference.txt': b'new reference stays on disk\n'}
            for name, data in originals.items():
                (folder / name).write_bytes(data)
            raw_path = os.fsencode(folder) + b'/literal-\xff-$[];name'
            with open(raw_path, 'xb') as stream:
                stream.write(b'non-UTF8 source\n')
            os.symlink('notes.txt', folder / 'alias')
            private = {}
            for kind in ('data', 'config', 'cache', 'state'):
                private[kind] = runtime.root / ('private-' + kind)
                private[kind].mkdir(mode=0o700)
            state_dir = private['state'] / 'pinboard'
            state_dir.mkdir(mode=0o700)
            state_path = state_dir / 'state.json'
            evidence['fixture'] = dict(marker_sha256=digest(folder / '.pinboard-fixture'),
                source_files={name: digest(folder / name) for name in originals},
                literal_target_sha256=digest(raw_path), private_xdg=True, home_override=False)
            if args.display:
                display = args.display
            elif args.xorg_config:
                body = args.xorg_config.resolve().read_text()
                body = body.replace('    Modeline "1920x1080"',
                    '    Modeline "2560x2160" 225.00 2560 2720 2992 3424 2160 2163 2173 2190\n'
                    '    Modeline "1920x1080"')
                body = body.replace('Modes "1920x1080"', 'Modes "2560x2160" "1920x1080"')
                xorg = runtime.root / 'xorg.conf'
                xorg.write_text(body)
                _, display = start_xorg_dummy(runtime, xorg, time.monotonic() + 10)
                wait_for_randr(runtime, display, time.monotonic() + 10)
                mm = {'1': (677, 572), '1.25': (542, 458), '2': (339, 286)}[scale]
                run_required_xrandr(runtime, display, ['--output', 'DUMMY0',
                    '--mode', '2560x2160', '--pos', '0x0', '--primary', '--set',
                    'WIDTH_MM', str(mm[0]), '--set', 'HEIGHT_MM', str(mm[1])],
                    'pinboard-real-scale', evidence.setdefault('operations', []))
                evidence['randr'] = wait_for_authoritative_state(runtime, display,
                    'pinboard real monitor scale', lambda v:
                        v['monitors'].get('DUMMY0', {}).get('mm_width') == mm[0] and
                        v['monitors']['DUMMY0']['mm_height'] == mm[1],
                    time.monotonic() + 8, 'pinboard-scale')
            else:
                command = ['Xvfb', '-screen', '0', '2560x2160x24', '-noreset']
                if args.shape_unavailable:
                    command += ['-extension', 'SHAPE']
                _, display = start_display_server(runtime, 'Xvfb', command,
                    'xserver.log', time.monotonic() + 10)
            for directory in (build / 'modules').iterdir():
                executable = directory / directory.name
                if not executable.is_file():
                    continue
                link = modules / directory.name
                if directory.name in ('FvwmForm', 'FvwmPinboard', 'FvwmMFL'):
                    link.write_text('#!/bin/sh\nexec ' + shlex.join([sys.executable,
                        str(Path(__file__).resolve()), '--module-proxy',
                        str(executable), str(native)]) + ' "$@" 2>>' +
                        shlex.quote(str(runtime.root / (directory.name + '.log'))) + '\n')
                    link.chmod(0o700)
                else:
                    link.symlink_to(executable)
            assets = source / 'default-config'
            desktop = build / 'bin/FvwmPrompt/FvwmDesktop'
            palette, appearance = (('dune', 'light') if profile == 'dune-light'
                else ('slate', 'dark'))
            font_family = ('DejaVu Sans Mono:size=18:antialias=True'
                if profile == 'classic-large' else 'Sans:size=10:antialias=True')
            font = 'xft:' + font_family
            if not args.native_only and not args.shape_unavailable:
                (modules / 'FvwmDesktop').symlink_to(desktop)
                (user / 'local.config').write_text('*FvwmDesktop: palette ' + palette +
                    '\n*FvwmDesktop: appearance ' + appearance +
                    '\n*FvwmDesktop: motion Off\n*FvwmDesktop: transparency Off\n')
                body = (assets / 'config').read_text().replace('$[FVWM_DATADIR]/default-config', str(assets))
                if profile == 'classic-large':
                    body = body.replace('Sans:size=10:antialias=True', font_family)
                    body += ('\n*FvwmFormDefault: Fore wheat\n*FvwmFormDefault: Back navy\n'
                        '*FvwmFormDefault: ItemFore black\n*FvwmFormDefault: ItemBack ivory\n')
            else:
                body = 'DesktopSize 2x1\nStyle * ClickToFocus\n'
            config = runtime.root / 'config'
            config.write_text('ModulePath ' + str(modules) + '\nRead ' +
                str(source / 'fvwm/ConfigFvwmDefaults') + '\n' + body +
                '\nDestroyMenu PinboardFixtureRoot\nAddToMenu PinboardFixtureRoot "Root fixture" Title\n'
                '+ "Dismiss" Nop\nDestroyFunc PinboardFixtureRootAction\n'
                'AddToFunc PinboardFixtureRootAction I Echo pinboard-root-action\n'
                '+ I Menu PinboardFixtureRoot\nMouse 3 R A Function PinboardFixtureRootAction\n'
                'AddToFunc StartFunction I Module FvwmMFL\n')
            evidence['config_sha256'] = digest(config)
            evidence['font'] = font
            env = runtime.environment({'DISPLAY': display, 'FVWM_USERDIR': str(user),
                'FVWM_DESKTOP_ROOT': str(assets), 'FVWMMFL_SOCKET_PATH': str(runtime.root),
                'LANG': 'C.UTF-8', 'LC_ALL': 'C.UTF-8', 'XDG_DATA_HOME': str(private['data']),
                'XDG_CONFIG_HOME': str(private['config']), 'XDG_CACHE_HOME': str(private['cache']),
                'XDG_STATE_HOME': str(private['state']), 'XDG_DATA_DIRS': str(runtime.root / 'absent')})
            if svg_helper is not None:
                env['FVWM_IMAGE_SVG_WORKER'] = str(svg_helper)
            if args.display:
                # The caller owns this authenticated disposable server. Never
                # start/stop it or displace an existing window/desktop owner.
                client = connect_client(display)
                preflight = XOracle(client, None)
                screen = client.x.XDefaultScreen(client.display)
                wm_owner = client.x.XGetSelectionOwner(client.display,
                    preflight.atom('WM_S%d' % screen))
                bounds = client.geometry(client.root)
                evidence['caller_server'] = dict(display=display,
                    xauthority_inherited=bool(os.environ.get('XAUTHORITY')),
                    root_geometry=bounds, server_start_stop_owned=False,
                    prior_wm_selection=wm_owner,
                    prior_desktop_selection=preflight.owner())
                if wm_owner or preflight.owner() or bounds[2] < 640 or bounds[3] < 480:
                    evidence.update(status='unavailable', setup_limit=
                        'caller server needs no existing WM/desktop owner and at least 640x480')
                    return 'unavailable'
                client.close()
                client = None
            core = runtime.start('core', [str(build / 'fvwm3'), '-d', display,
                '-f', str(config)], 'core.log', env)
            endpoint = runtime.root / ('fvwm_mfl_' + display.replace(':', '_') + '.sock')
            wait_for_predicate('pinboard MFL endpoint', endpoint.exists,
                time.monotonic() + 12, process=core)
            echo = EchoStream(endpoint)
            if not args.native_only and not args.shape_unavailable:
                def theme_ready():
                    tag = 'pin-theme-' + os.urandom(6).hex()
                    echo.send('SendToModule FvwmDesktop query-settings ' + tag + ' all')
                    values = {}
                    for _ in range(64):
                        raw = echo.take('desktop-settings-' + tag + ' ')
                        if raw.endswith(' done'):
                            break
                        _, name, value = raw.split(' ', 2)
                        values[name] = json.loads(value)
                    else:
                        raise AssertionError('theme settings response exceeds bound')
                    if all(values.get(name, {}).get('effective') == value for name, value in
                            (('palette', palette), ('appearance', appearance), ('transparency', False))):
                        return values
                    return None
                evidence['theme_settings'] = wait_for_predicate('actual requested pinboard theme',
                    theme_ready, time.monotonic() + 12, process=core)
            client = connect_client(display)
            errors = SessionXErrors(client, evidence).install()
            view = NativeView(client, errors, core)
            oracle = XOracle(client, view)
            x, d = client.x, client.display

            def wm(command):
                echo.send(command)
                echo.barrier()

            def focus(window):
                wm('WindowId %d Focus' % window)
                wait_for_predicate('explicit owned window focus', lambda:
                    view.focus() == window, time.monotonic() + 3, process=core)

            def key(symbol, alt=False):
                modifier = x.XKeysymToKeycode(d, x.XStringToKeysym(b'Alt_L'))
                if alt:
                    assert view.pointer.x.XTestFakeKeyEvent(d, modifier, 1, 0)
                try:
                    view.key(symbol)
                finally:
                    if alt:
                        assert view.pointer.x.XTestFakeKeyEvent(d, modifier, 0, 0)
                x.XSync(d, 0)

            def start_native():
                token = os.urandom(16).hex()
                alias = 'FvwmPinboard-' + token
                wm('*' + alias + ': Font "' + font + '"')
                wm('Module FvwmPinboard ' + alias)
                protocol = NativeProtocol(echo, alias, token, evidence)
                protocol.send('Owner ' + token)
                return protocol

            def transaction(protocol, old=0, new=1):
                protocol.send('Begin %s %d %d 2' % (protocol.token, old, new))
                protocol.send('Layout %s %d 96 96 32' % (protocol.token, new))
                for index, column in enumerate((0, 2)):
                    identity = '%032x' % (index + 1)
                    protocol.send('Entry %s %d %s - %d 0 0' %
                        (protocol.token, new, identity, column))
                    protocol.send('Label %s %d %s %s' % (protocol.token, new,
                        identity, ('Fixture ' + str(index)).encode().hex()))
                protocol.send('Commit %s %d' % (protocol.token, new))
                return wait_for_predicate('native committed view', lambda:
                    (s if (s := protocol.status())['current'] == new and s['state'] == 'ready' else None),
                    time.monotonic() + 5, process=core)

            if args.shape_unavailable:
                event_base, error_base = C.c_int(), C.c_int()
                if oracle.shape.XShapeQueryExtension(d,
                        C.byref(event_base), C.byref(error_base)):
                    evidence.update(status='unavailable', shape_unavailable=dict(
                        reason='owned Xvfb still exposes SHAPE after -extension SHAPE',
                        candidate_refusal_exercised=False))
                    return 'unavailable'
                protocol = start_native()
                state = protocol.status()
                assert state['state'] == 'unavailable' and oracle.owner() == 0, state
                assert not view.named(protocol.alias)
                evidence.update(status='passed', shape_unavailable=state)
                return

            if args.crop_only:
                run_native_picture_containment(runtime, folder, start_native,
                    oracle, view, client, evidence, core)
                assert not errors.unexpected, errors.unexpected
                evidence['status'] = 'passed'
                return

            client.create()
            normal = view.wait('FVWM transition probe')
            root_geometry = client.geometry(client.root)
            normal_x = min(1100, root_geometry[2] - 200)
            normal_y = min(750, root_geometry[3] - 150)
            wm('WindowId %d ResizeMove 160p 90p %dp %dp' %
                (normal, normal_x, normal_y))
            focus(normal)
            normal_frame = oracle.top(normal)
            wallpaper = oracle.wallpaper()
            protocol = start_native()
            state = transaction(protocol)
            canvas = state['window']
            wait_for_predicate('native desktop selection', lambda: oracle.owner() == canvas,
                time.monotonic() + 3, process=core)
            echo.barrier()
            assert view.focus() == normal, 'pinboard stole startup focus'
            assert oracle.words(canvas, '_NET_WM_WINDOW_TYPE', 4) == [
                oracle.atom('_NET_WM_WINDOW_TYPE_DESKTOP')]
            assert oracle.words(client.root, '_NET_CLIENT_LIST', 33).count(canvas) == 1
            attrs = view.attributes(canvas)
            assert attrs and attrs.map_state == 2 and not attrs.override_redirect
            assert client.parent(canvas) != client.root
            canvas_frame = oracle.top(canvas)
            stack = view.children(client.root)
            assert stack.index(canvas_frame) < stack.index(normal_frame)
            assert oracle.wallpaper() == wallpaper
            _, _, cells = protocol.cells()
            visible = [cell for cell in cells if cell['visible']]
            assert len(visible) == 2
            expected_q = int(float(scale) * 4) if args.xorg_config else cells[0]['quarters']
            assert all(cell['quarters'] == expected_q and
                cell['width'] == 96 * expected_q // 4 and
                cell['height'] == 96 * expected_q // 4 for cell in visible), visible
            # Independently read both client and frame SHAPE. Test a gap and
            # every actual cell center in root coordinates; Cells alone cannot
            # establish click-through or core frame propagation.
            shapes = {str(w): oracle.rectangles(w) for w in (canvas, canvas_frame)}
            def covered(window, px, py):
                ox, oy = oracle.origin(window)
                return any(ox + a <= px < ox + a + width and
                    oy + b <= py < oy + b + height
                    for a, b, width, height in oracle.rectangles(window))
            centers = [(c['x'] + c['width'] // 2, c['y'] + c['height'] // 2) for c in visible]
            hole = (visible[1]['x'] - visible[1]['width'] // 2,
                visible[1]['y'] + visible[1]['height'] // 2)
            for window in (canvas, canvas_frame):
                assert all(covered(window, *point) for point in centers)
                assert not covered(window, *hole), 'frame filled sparse root hole'
            before = {w for w in view.children(client.root)
                if (a := view.attributes(w)) and a.map_state == 2}
            pressed = False
            try:
                view.pointer.motion(*hole)
                assert view.pointer.x.XTestFakeButtonEvent(d, 3, 1, 0)
                pressed = True
                x.XSync(d, 0)
                echo.take('pinboard-root-action')
                def popup():
                    result = {}
                    for window in view.children(client.root):
                        if window in before:
                            continue
                        a = view.attributes(window)
                        if a and a.map_state == 2 and a.save_under:
                            geometry = client.geometry(window)
                            if geometry[2] > 20 and geometry[3] > 10:
                                result[window] = geometry
                    return result if len(result) == 1 else None
                menu = wait_for_predicate('actual root menu through SHAPE hole', popup,
                    time.monotonic() + 3, process=core)
                assert view.pointer.grabbed(), 'root menu lacks real pointer grab'
            finally:
                if pressed:
                    assert view.pointer.x.XTestFakeButtonEvent(d, 3, 0, 0)
                key('Escape')
            wait_for_predicate('root menu pointer release', lambda: not view.pointer.grabbed(),
                time.monotonic() + 3, process=core)
            evidence['native_surface'] = dict(state=state, window=canvas,
                frame=canvas_frame, normal=normal, normal_frame=normal_frame,
                root_stack=stack, geometry=client.geometry(canvas),
                root_origin=oracle.origin(canvas), shapes=shapes, cells=cells,
                root_hole=hole, root_menu=menu, focus_preserved=True,
                wallpaper_properties_preserved=True)
            evidence['checks'].append('managed desktop, propagated shape, real root menu/grab, startup focus/stack')
            # Invalid staging must not mutate the previously committed view.
            protocol.send('Begin %s 1 2 1' % protocol.token)
            protocol.send('Entry %s 2 %s - 0 0 0' % (protocol.token, '0' * 31 + '3'))
            protocol.send('Entry %s 2 %s - 1 0 0' % (protocol.token, '0' * 31 + '3'))
            protocol.send('Commit %s 2' % protocol.token)
            rejected = protocol.status()
            assert rejected['current'] == 1
            assert protocol.cells()[2] == cells
            protocol.send('Abort %s 2' % protocol.token)
            assert protocol.status()['current'] == 1
            evidence['invalid_native_transaction'] = rejected
            # A second real native instance must not disturb the selection.
            duplicate = start_native()
            duplicate.send('Begin %s 0 1 0' % duplicate.token)
            duplicate.send('Commit %s 1' % duplicate.token)
            refused = wait_for_predicate('duplicate real desktop owner refusal', lambda:
                (s if (s := duplicate.status())['state'] == 'refused' else None),
                time.monotonic() + 3, process=core)
            assert refused['state'] == 'refused' and oracle.owner() == canvas, refused
            duplicate.send('Retire ' + duplicate.token)
            # Real input events are observed independently of metadata replies.
            focus(canvas)
            key('Home')
            key('Return')
            event = echo.take('FVWM_PINBOARD_EVENT ' + protocol.token + ' ')
            def event_verb(verb):
                for _ in range(16):
                    raw = echo.take('FVWM_PINBOARD_EVENT ' + protocol.token + ' ')
                    fields = raw.split()
                    assert len(fields) == 9, raw
                    if fields[4] == verb:
                        return raw
                raise AssertionError('missing native event ' + verb)
            if event.split()[4] != 'manage':
                event = event_verb('manage')
            snapshot = protocol.cells()
            key('Home')
            key('Tab')
            key('m')
            key('Right')
            key('Escape')
            echo.barrier()
            assert protocol.cells() == snapshot, 'cancel changed committed placement'
            evidence['native_keyboard'] = dict(open_event=event, cancelled_cells=snapshot)
            # A native Button3 implicit grab must be relinquished before the
            # owner asks the core to acquire a real menu grab. Keep the button
            # down across the handoff, including on caller-owned BSD displays.
            pin = next(c for c in snapshot[2] if c['visible'])
            committed_shapes = {str(w): oracle.rectangles(w)
                for w in (canvas, canvas_frame)}
            preview_point = (pin['x'] + pin['width'] * 3 // 2,
                pin['y'] + pin['height'] // 2)
            old_point = (pin['x'] + pin['width'] // 2,
                pin['y'] + pin['height'] // 2)
            focus(canvas)
            key('Home')
            key('Tab')
            key('m')
            key('Right')
            wait_for_predicate('actual native preview replaces the committed input shape',
                lambda: all(covered(w, *preview_point) and not covered(w, *old_point)
                    for w in (canvas, canvas_frame)), time.monotonic() + 3, process=core)
            preview_shapes = {str(w): oracle.rectangles(w)
                for w in (canvas, canvas_frame)}
            before = {w for w in view.children(client.root)
                if (a := view.attributes(w)) and a.map_state == 2}
            protocol.send('Renew ' + protocol.token)
            pressed = False
            try:
                view.pointer.motion(*preview_point)
                assert view.pointer.x.XTestFakeButtonEvent(d, 3, 1, 0)
                pressed = True
                x.XSync(d, 0)
                context_event = echo.take('FVWM_PINBOARD_EVENT %s %d %s context %s ' %
                    (protocol.token, snapshot[0], snapshot[1], pin['id']), timeout=3)
                # A core Echo barrier here would wait behind the active menu.
                echo.send('Menu PinboardFixtureRoot')
                context_popup = wait_for_predicate('native context hands grab to core menu',
                    popup, time.monotonic() + 3, process=core)
                assert view.pointer.grabbed(), 'core context menu lacks actual pointer grab'
            finally:
                if pressed:
                    assert view.pointer.x.XTestFakeButtonEvent(d, 3, 0, 0)
                key('Escape')
            wait_for_predicate('native context handoff releases menu pointer grab',
                lambda: not view.pointer.grabbed(), time.monotonic() + 3, process=core)
            echo.barrier()
            wait_for_predicate('context cancellation restores committed client and frame shape',
                lambda: all(oracle.rectangles(w) == committed_shapes[str(w)]
                    for w in (canvas, canvas_frame)), time.monotonic() + 3, process=core)
            # Dismissal must leave ordinary selection navigation, not a move
            # whose release was swallowed by the core menu's pointer grab.
            right_pin = next(c for c in snapshot[2]
                if c['visible'] and c['x'] > pin['x'])
            prefix = 'FVWM_PINBOARD_EVENT ' + protocol.token + ' '
            echo.queue[:] = [message for message in echo.pump()
                if not message.startswith(prefix)]
            focus(canvas)
            key('Right')
            selection_event = echo.take('FVWM_PINBOARD_EVENT %s %d %s focus %s ' %
                (protocol.token, snapshot[0], snapshot[1], right_pin['id']), timeout=3)
            assert all(oracle.rectangles(w) == committed_shapes[str(w)]
                for w in (canvas, canvas_frame))
            assert protocol.cells() == snapshot
            assert oracle.owner() == canvas
            evidence['native_context_handoff'] = dict(event=context_event,
                menu=context_popup, button3_held_until_menu=True,
                core_pointer_grab_observed=True, same_scene_preserved=True,
                context_id=pin['id'], actual_preview_point=preview_point,
                committed_shapes=committed_shapes, preview_shapes=preview_shapes,
                committed_shape_restored=True, post_dismiss_selection=selection_event,
                selection_owner=canvas, after_status=protocol.status())
            evidence['checks'].append('preview Button3 identifies its pin, restores shape and hands grab to core menu')
            # Actual selection loss must preserve the successor. Server time
            # avoids a CurrentTime ownership race with the original admission.
            successor = x.XCreateSimpleWindow(d, client.root, 0, 0, 1, 1, 0, 0, 0)
            assert successor
            stamp = oracle.timestamp(successor)
            x.XSetSelectionOwner(d, oracle.selection, successor, stamp)
            x.XSync(d, 0)
            assert oracle.owner() == successor
            wait_for_predicate('selection loss unmaps canvas', lambda:
                not (a := view.attributes(canvas)) or a.map_state != 2,
                time.monotonic() + 4, process=core)
            def exact_module_wait():
                matches = [json.loads(p.read_text()) for p in native.glob('module-*.json')
                    if protocol.alias in json.loads(p.read_text())['arguments']]
                assert len(matches) <= 1
                if not matches:
                    return None
                receipt = native / ('stopped-%d.json' % matches[0]['proxy'])
                if not receipt.exists():
                    return None
                waited = json.loads(receipt.read_text())
                assert waited['child'] == matches[0]['child']
                assert waited['returncode'] is not None
                return waited
            selection_wait = wait_for_predicate('exact native child Wait after selection loss',
                exact_module_wait, time.monotonic() + 4, process=core)
            assert oracle.owner() == successor
            x.XDestroyWindow(d, successor)
            successor = None
            x.XSync(d, 0)
            wait_for_predicate('owned successor destroyed', lambda: oracle.owner() == 0,
                time.monotonic() + 2, process=core)
            # A mapped module with no renewing owner must independently drop
            # its lease. No arbitrary sleep decides whether this happened.
            lease_result = 'not run; separately source-bound result must be reused by caller'
            if not args.skip_lease_expiry:
                leased = start_native()
                lease_state = transaction(leased)
                wait_for_predicate('expired owner lease releases desktop selection',
                    lambda: oracle.owner() == 0, time.monotonic() + 18, process=core)
                assert not (a := view.attributes(lease_state['window'])) or a.map_state != 2
                lease_result = 'selection released and native window unmapped'
                evidence['checks'].append('owner lease expiry releases and unmaps canvas')
            evidence['native_lifetime'] = dict(duplicate=refused,
                selection_loss='unmapped; successor preserved after exact child Wait',
                selection_loss_child_wait=selection_wait,
                expired_owner_lease=lease_result)
            evidence['checks'].append('duplicate refusal, keyboard cancel and selection loss')
            # A previously ordinary foreign client may later announce itself
            # as a desktop without changing _NET_CLIENT_LIST. Retire only this
            # canvas; never destroy or clear the other client's type.
            conflict = start_native()
            conflict_state = transaction(conflict)
            transients = []
            try:
                for index in range(8):
                    window = x.XCreateSimpleWindow(d, client.root,
                        400, 300, 40, 30, 0, 0, 0)
                    assert window
                    transients.append(window)
                    x.XStoreName(d, window,
                        ('Pinboard transient %d' % index).encode('ascii'))
                    x.XMapWindow(d, window)
                x.XSync(d, 0)
                wait_for_predicate('owned transient clients enter actual client list',
                    lambda: set(transients).issubset(oracle.words(client.root,
                        '_NET_CLIENT_LIST', 33)), time.monotonic() + 4, process=core)
                conflict.status()
            finally:
                for window in transients:
                    x.XDestroyWindow(d, window)
                x.XSync(d, 0)
            desktop_type = C.c_ulong(oracle.atom('_NET_WM_WINDOW_TYPE_DESKTOP'))
            x.XChangeProperty(d, normal, oracle.atom('_NET_WM_WINDOW_TYPE'),
                4, 32, 0, C.byref(desktop_type), 1)
            x.XSync(d, 0)
            wait_for_predicate('ordinary client changing type retires only pinboard',
                lambda: oracle.owner() == 0 and
                    (not (a := view.attributes(conflict_state['window'])) or a.map_state != 2),
                time.monotonic() + 4, process=core)
            assert view.attributes(normal).map_state == 2
            assert oracle.words(normal, '_NET_WM_WINDOW_TYPE', 4) == [desktop_type.value]
            existing_desktop = start_native()
            existing_desktop.send('Begin %s 0 1 0' % existing_desktop.token)
            existing_desktop.send('Commit %s 1' % existing_desktop.token)
            conflict_refused = wait_for_predicate('desktop client without selection refuses new canvas',
                lambda: (s if (s := existing_desktop.status())['state'] == 'refused' else None),
                time.monotonic() + 4, process=core)
            assert oracle.owner() == 0 and view.attributes(normal).map_state == 2
            existing_desktop.send('Retire ' + existing_desktop.token)
            normal_type = C.c_ulong(oracle.atom('_NET_WM_WINDOW_TYPE_NORMAL'))
            x.XChangeProperty(d, normal, oracle.atom('_NET_WM_WINDOW_TYPE'),
                4, 32, 0, C.byref(normal_type), 1)
            x.XSync(d, 0)
            evidence['later_desktop_client'] = dict(own_canvas=conflict_state['window'],
                foreign_window=normal, own_canvas_retired=True,
                existing_desktop_refusal=conflict_refused,
                transient_managed_clients_destroyed=transients,
                foreign_window_preserved=True, foreign_type_preserved_until_fixture_restore=True)
            evidence['checks'].append('ordinary client later becoming desktop retires only owned canvas')
            run_native_picture_containment(runtime, folder, start_native, oracle,
                view, client, evidence, core)
            if not args.native_only:
                run_owner(args, runtime, source, build, out, env, endpoint,
                    state_path, folder, raw_path, originals, native, echo,
                    client, view, oracle, wm, focus, key, evidence, font)
            assert not errors.unexpected, errors.unexpected
            evidence['status'] = 'passed'
        except BaseException as failure:
            evidence['status'] = 'failed'
            evidence['failures'].append(dict(type=type(failure).__name__,
                message=str(failure), traceback=traceback.format_exc()))
            raise
        finally:
            def clean(label, operation):
                try:
                    operation()
                except BaseException as failure:
                    evidence['cleanup_failures'].append(dict(step=label,
                        type=type(failure).__name__, message=str(failure)))
            if successor and client:
                clean('owned selection successor', lambda: client.x.XDestroyWindow(client.display, successor))
            if echo:
                clean('MFL observer', echo.close)
            if errors:
                clean('X observer', errors.close_display)
            elif client:
                clean('X observer', client.close)
            for process in reversed(runtime.processes):
                clean('owned process ' + process.name,
                    lambda p=process: p.close(time.monotonic() + 5))
            clean('actual module child Wait receipts', lambda:
                evidence.update(module_cleanup=cleanup_receipts(native)))
            evidence['children_after_cleanup'] = {p.name: p.poll() for p in runtime.processes}
            for path in runtime.root.iterdir():
                if path.is_file() and (path.suffix == '.log' or path.name in ('config', 'xorg.conf')):
                    clean('archive ' + path.name, lambda p=path: shutil.copy2(p, out / p.name))
                elif path.is_dir() and path.name == 'native-records':
                    clean('archive native receipts', lambda p=path: shutil.copytree(p, out / p.name))
            if evidence['cleanup_failures'] or evidence.get('observer_unexpected_x_errors'):
                evidence['status'] = 'failed'
            atomic_json(out / 'evidence.json', evidence)
    assert evidence['status'] == 'passed' and not evidence['cleanup_failures']


def run_native_picture_containment(runtime, folder, start_native, oracle,
        view, client, evidence, core):
    """A later oversized opaque/alpha picture cannot overwrite its neighbor."""
    protocol = start_native()
    identities = ['c' * 32, 'd' * 32]
    def scene(old, new, picture=None):
        token = protocol.token
        protocol.send('Renew ' + token)
        protocol.send('Begin %s %d %d 2' % (token, old, new))
        protocol.send('Layout %s %d 64 64 128' % (token, new))
        for index, identity in enumerate(identities):
            protocol.send('Entry %s %d %s - %d 0 0' %
                (token, new, identity, index + 1))
            protocol.send('Label %s %d %s %s' % (token, new, identity,
                ('Neighbor' if index == 0 else 'Image').encode().hex()))
        if picture:
            protocol.send('Picture %s %d %s %s' %
                (token, new, identities[1], os.fsencode(picture).hex()))
        protocol.send('Commit %s %d' % (token, new))
        state = wait_for_predicate('native containment scene commits', lambda:
            (s if (s := protocol.status())['current'] == new and
                s['state'] == 'ready' else None), time.monotonic() + 5, process=core)
        publication = protocol.cells()
        assert publication[:2] == (new, state['epoch'])
        cells = publication[2]
        assert len(cells) == 2 and all(c['visible'] for c in cells)
        first, second = cells
        assert first['column'] == 1 and second['column'] == 2
        assert first['y'] == second['y'] and first['height'] == second['height']
        assert first['x'] + first['width'] == second['x']
        return state, cells

    try:
        state, cells = scene(0, 1)
        canvas = state['window']
        origin = oracle.origin(canvas)
        point_groups = [[(cell['x'] - origin[0] + x,
            cell['y'] - origin[1] + y)
            for y in range(4, cell['height'] - 4)
            for x in range(4, cell['width'] - 4)] for cell in cells]
        points = point_groups[0] + point_groups[1]
        split = len(point_groups[0])
        def samples():
            pixels = oracle.pixel_samples(canvas, points)
            return pixels[:split], pixels[split:]
        baseline = samples()
        assert baseline[0] and baseline[1]
        cases = []
        generation = 1
        for name, color in (('opaque', (22, 120, 157)),
                ('alpha', (220, 48, 150, 128))):
            if generation != 1:
                scene(generation, generation + 1)
                generation += 1
                wait_for_predicate('actual no-image baseline restored',
                    lambda: samples() == baseline, time.monotonic() + 5, process=core)
            picture = folder / ('containment-' + name + '.png')
            picture.write_bytes(solid_png(128, 128, color))
            picture.chmod(0o600)
            state, after_cells = scene(generation, generation + 1, picture)
            generation += 1
            assert after_cells == cells
            def painted():
                neighbor, own = samples()
                if own == baseline[1]:
                    return None
                assert neighbor == baseline[0], name + ' picture overwrote neighboring cell'
                return dict(neighbor_samples=len(neighbor), own_samples=len(own),
                    changed_own_pixels=sum(a != b for a, b in zip(own, baseline[1])),
                    neighbor_sha256=hashlib.sha256(struct.pack(
                        '>' + 'I' * len(neighbor), *neighbor)).hexdigest(),
                    native_ack=protocol.status())
            observed = wait_for_predicate('oversized ' + name + ' picture paints only its cell',
                painted, time.monotonic() + 5, process=core)
            cases.append(dict(format=name, dimensions=[128, 128],
                color_type=2 if len(color) == 3 else 6,
                source_sha256=digest(picture), cells=cells, observed=observed))
        evidence['native_picture_containment'] = dict(layout=[64, 64, 128],
            earlier_neighbor_has_image=False, cases=cases)
        evidence['checks'].append('opaque and RGBA128 pictures preserve adjacent64-cell pixels')
    finally:
        protocol.send('Retire ' + protocol.token)
        wait_for_predicate('containment module releases only its canvas',
            lambda: oracle.owner() == 0, time.monotonic() + 4, process=core)


def run_owner(args, runtime, source, build, out, env, endpoint, state_path,
        folder, raw_path, originals, native, echo, client, view, oracle, wm,
        focus, key, evidence, font):
    """Real metadata Forms and independent committed-state assertions."""
    executable = build / 'bin/FvwmPrompt/fvwm-pinboard'
    real_browser = build / 'bin/FvwmPrompt/fvwm-file-browser'
    sequence = 0

    def cli(*arguments, success=True):
        nonlocal sequence
        sequence += 1
        assert sequence <= 32
        name = 'cli-%02d' % sequence
        process = runtime.start(name, [str(executable), '--state', str(state_path),
            *[os.fsdecode(v) if isinstance(v, bytes) else str(v) for v in arguments]],
            name + '.log', env)
        code = process.wait_until_exit(time.monotonic() + 8)
        text = (runtime.root / (name + '.log')).read_text(errors='replace')
        assert (code == 0) == success, dict(arguments=arguments, code=code, output=text)
        evidence.setdefault('cli', []).append(dict(arguments=[os.fsdecode(v)
            if isinstance(v, bytes) else str(v) for v in arguments],
            returncode=code, output=text))
        return text

    def stored():
        assert state_path.stat().st_size <= 2 * 1024 * 1024
        assert state_path.stat().st_mode & 0o777 == 0o600
        data = state_path.read_bytes()
        value = json.loads(data)
        assert value['schema_version'] == 1 and len(value['pins']) <= 256
        ids = [pin['id'] for pin in value['pins']]
        assert len(ids) == len(set(ids)) and all(re.fullmatch(r'[0-9a-f]{32}', i) for i in ids)
        for pin in value['pins']:
            assert base64.b64decode(pin['target_bytes'], validate=True).startswith(b'/')
            assert len(pin['label'].encode()) <= 256
        return value

    def committed(previous, predicate=lambda value: True):
        def observed():
            value = stored()
            if value['revision'] == previous['revision'] + 1 and predicate(value):
                evidence.setdefault('committed', []).append(dict(revision=value['revision'],
                    state_sha256=digest(state_path), state_inode=state_path.stat().st_ino,
                    pins=[dict(id=p['id'], label=p['label'], icon=p.get('icon', ''),
                        column=p['column'], row=p['row'], target_sha256=hashlib.sha256(
                            base64.b64decode(p['target_bytes'], validate=True)).hexdigest()) for p in value['pins']]))
                return value
            return None
        return wait_for_predicate('independent committed state bytes', observed,
            time.monotonic() + 8)

    # A deliberately isolated named theme makes image source identity explicit.
    theme = Path(env['XDG_DATA_HOME']) / 'icons/PinboardFixture'
    icons = theme / 'scalable/apps'
    icons.mkdir(parents=True, mode=0o700)
    (theme / 'index.theme').write_text('[Icon Theme]\nName=PinboardFixture\n'
        'Directories=scalable/apps\n[scalable/apps]\nSize=32\nType=Scalable\n'
        'MinSize=16\nMaxSize=256\nContext=Applications\n')
    icon = icons / ('pinboard-fixture.' + args.named_icon_format)
    if args.named_icon_format == 'svg':
        icon.write_text('<svg xmlns="http://www.w3.org/2000/svg" width="32" height="32" '
            'viewBox="0 0 32 32"><rect x="2" y="2" width="28" height="28" '
            'rx="4" fill="#16789d"/><path d="M8 16h16M16 8v16" '
            'stroke="#fff" stroke-width="4"/></svg>\n')
    else:
        def chunk(kind, data):
            return (struct.pack('>I', len(data)) + kind + data +
                struct.pack('>I', zlib.crc32(kind + data)))
        raw = b''.join(b'\0' + b''.join(b'\xff\xff\xff' if
            ((14 <= x < 18 and 8 <= y < 24) or
             (14 <= y < 18 and 8 <= x < 24)) else b'\x16\x78\x9d'
            for x in range(32)) for y in range(32))
        icon.write_bytes(b'\x89PNG\r\n\x1a\n' +
            chunk(b'IHDR', struct.pack('>IIBBBBB', 32, 32, 8, 2, 0, 0, 0)) +
            chunk(b'IDAT', zlib.compress(raw)) + chunk(b'IEND', b''))
    evidence['icon_inputs'] = {str(p.relative_to(theme)): digest(p)
        for p in (theme / 'index.theme', icon)}
    assert oracle.owner() == 0
    targets = [(folder / 'notes.txt', 'Notes', 0),
        (folder / 'missing.txt', 'Missing target', 1),
        (raw_path, 'Literal bytes', 2), (Path('/'), 'Root folder', 3),
        (folder / 'alias', 'Symlink', 4),
        (folder / 'notes.txt', 'Same target independent reference', 5)]
    for path, label, column in targets:
        cli('--pin', path, '--label', label, '--column', column)
        assert oracle.owner() == 0, '--pin unexpectedly claimed a desktop'
    initial = stored()
    assert len(initial['pins']) == 6 and initial['revision'] == 6
    for actual, (target, label, column) in zip(initial['pins'], targets):
        assert base64.b64decode(actual['target_bytes'], validate=True) == os.fsencode(target)
        assert (actual['label'], actual['column'], actual['row']) == (label, column, 0)
    assert initial['pins'][0]['id'] != initial['pins'][5]['id']
    evidence['checks'].append('data-only CLI, literal/base64/root/missing/symlink targets and independent stable IDs')

    def start_owner(name):
        process = runtime.start(name, [str(executable), '--state', str(state_path),
            '--socket', str(endpoint), '--opener', '/bin/true', '--browser', str(real_browser),
            '--font', font, '--icon-theme', 'PinboardFixture',
            '--icon-formats', args.named_icon_format], name + '.log', env)
        canvas = wait_for_predicate('explicit pinboard owner selection', oracle.owner,
            time.monotonic() + 10, process=process)
        alias = oracle.alias(canvas)
        assert re.fullmatch(r'FvwmPinboard-[0-9a-f]{32}', alias), alias
        protocol = NativeProtocol(echo, alias, alias.removeprefix('FvwmPinboard-'), evidence)
        wait_for_predicate('explicit owner first scene', lambda:
            (s if (s := protocol.status())['state'] == 'ready' and s['current'] else None),
            time.monotonic() + 10, process=process)
        return process, canvas, protocol

    owner, canvas, protocol = start_owner('owner')

    def projection(count=None):
        def observed():
            result = protocol.cells()
            if count is None or len(result[2]) == count:
                return result
            return None
        return wait_for_predicate('published native model matches committed count',
            observed, time.monotonic() + 8, process=owner)

    projection(6)
    before = state_path.read_bytes()
    duplicate = runtime.start('duplicate-owner', [str(executable), '--state', str(state_path),
        '--socket', str(endpoint)], 'duplicate-owner.log', env)
    duplicate_code = duplicate.wait_until_exit(time.monotonic() + 8)
    assert duplicate_code != 0 and oracle.owner() == canvas
    assert state_path.read_bytes() == before
    evidence['duplicate_owner'] = dict(returncode=duplicate_code,
        active_window_preserved=True, state_unchanged=True)

    def page(title, names=()):
        def observed():
            target = view.named(title)
            if not target:
                return None
            alias = oracle.alias(target)
            lines = [row['text'][len(alias)+1:].lstrip(': \t') for row in rows(native)
                if row['direction'] == 'out' and row['text'].startswith('*' + alias + ': ')]
            updates = [row['text'] for row in rows(native)
                if row['direction'] == 'out' and row['text'].startswith(
                    'SendToModule ' + alias + ' SetLabel status ')]
            schema = [line.split()[:2] for line in lines if line.split() and
                line.split()[0] in ('Input', 'InputHex', 'Button', 'List')]
            children = view.children(target)
            if not schema or len(schema) != len(children):
                return None
            inputs = {parts[1]: child for parts, child in zip(schema, children)
                if parts[0] in ('Input', 'InputHex')}
            if not set(names).issubset(inputs):
                return None
            lists = {parts[1]: child for parts, child in zip(schema, children)
                if parts[0] == 'List'}
            return dict(window=target, alias=alias, inputs=inputs, lists=lists,
                lines=lines, status_updates=updates)
        return wait_for_predicate('actual native Form ' + title, observed,
            time.monotonic() + 8, process=owner)

    def click_child(window, button=1):
        attrs = view.attributes(window)
        assert attrs and attrs.map_state == 2
        px, py = oracle.origin(window)
        oracle.click(px + min(6, attrs.width // 2), py + min(6, attrs.height // 2), button)

    def manager():
        def idle():
            if not view.named('Pinboard manager'):
                focus(canvas)
                key('Home')
                key('Return')
            current = page('Pinboard manager')
            observed = [row for row in rows(native) if row['direction'] == 'out' and
                (row['text'].startswith('*' + current['alias'] + ': Label work ') or
                 row['text'].startswith('SendToModule ' + current['alias'] + ' SetLabel work '))]
            if not observed or 'Worker idle; no pending work' not in observed[-1]['text']:
                return None
            epochs = set(re.findall(r'FVWM_PINBOARD_FORM ' + protocol.token +
                r' [0-9]+ [0-9]+ ([0-9a-f]{16}) ', '\n'.join(current['lines'])))
            assert len(epochs) == 1
            if epochs != {protocol.status()['epoch']}:
                # Topology deliberately retires the captured Form callbacks.
                # Close this exact draft before explicitly entering a new one.
                callback(current, 'dialog-close', 'Escape')
                return None
            evidence.setdefault('owner_idle_receipts', []).append(observed[-1])
            return current
        return wait_for_predicate('actual owner Form reports worker reaped and no pending work',
            idle, time.monotonic() + 12, process=owner)

    def select_first():
        current = manager()
        assert set(current['lists']) == {'picks'}, current['lists']
        click_child(current['lists']['picks'])
        focus(current['window'])
        key('Home')
        return current

    def callback(current, action, symbol, values=(), alt=False):
        started = time.monotonic()
        focus(current['window'])
        key(symbol, alt=alt)
        def observed():
            for row in rows(native):
                if row['direction'] != 'out' or row['time'] < started:
                    continue
                fields = row['text'].split()
                if len(fields) < 8 or fields[:2] != ['Echo', 'FVWM_PINBOARD_FORM']:
                    continue
                # Echo prefix shifts the frozen seven-field header by one.
                if fields[2] != protocol.token or fields[7] != action:
                    continue
                actual = []
                for field in fields[8:]:
                    assert field == '-' or field.startswith('x'), fields
                    data = '' if field == '-' else field[1:]
                    actual.append(base64.b64decode(data + '=' * (-len(data) % 4),
                        altchars=b'-_', validate=True))
                if values:
                    assert actual == list(values), dict(callback=row['text'],
                        expected_hex=[v.hex() for v in values])
                current['paste_pending'] = False
                evidence.setdefault('form_callbacks', []).append(dict(action=action,
                    raw=row['text'], decoded_sha256=[hashlib.sha256(v).hexdigest() for v in actual]))
                return row
            return None
        result = wait_for_predicate('actual Form callback readback ' + action,
            observed, time.monotonic() + 8, process=owner)
        if action == 'dialog-close':
            wait_for_predicate('actual callback retires its exact Form window',
                lambda: not (a := view.attributes(current['window'])) or a.map_state != 2,
                time.monotonic() + 5, process=owner)
        return result

    def paste(current, values):
        assert not current.get('paste_pending'), 'unobserved CUT_BUFFER0 paste'
        names = list(values)
        for name, value in values.items():
            assert name in current['inputs'] and b'\0' not in value and b'\t' not in value
            click_child(current['inputs'][name])
            focus(current['window'])
            view.key('u', control=True)
        # One immutable cut buffer for the entire draft. XSync alone does not
        # prove Form consumed it; callback() must read back bytes before reuse.
        data = b'\t'.join(values[name] for name in names)
        client.x.XStoreBuffer(client.display, data, len(data), 0)
        client.x.XSync(client.display, 0)
        click_child(current['inputs'][names[0]], button=2)
        focus(current['window'])
        current['paste_pending'] = True

    def capture(name):
        assert len(evidence['screenshots']) < 5
        previous = os.environ.get('DISPLAY')
        os.environ['DISPLAY'] = env['DISPLAY']
        try:
            image = Path(NativeCapture(build / 'bin/fvwm-desktop-capture').screenshot(out)
                .removeprefix('Screenshot saved: '))
            image.rename(out / name)
        finally:
            if previous is None:
                os.environ.pop('DISPLAY', None)
            else:
                os.environ['DISPLAY'] = previous
        evidence['screenshots'].append(name)
        evidence.setdefault('screenshot_manifest', []).append(dict(path=name,
            sha256=digest(out / name), pixels=list(struct.unpack('>II',
                (out / name).read_bytes()[16:24])), state_sha256=digest(state_path),
            native_window=canvas, generation=projection()[0]))

    capture('01-desktop-references.png')
    current = manager()
    capture('02-manager-missing-literal-root.png')
    callback(current, 'new', 'n', alt=True)
    current = page('New pin', ('path',))
    long_target = os.fsencode(folder) + b'/' + b'a' * 200 + b'/' + b'b' * 200
    assert 384 < len(long_target) < 4096
    refused_bytes = state_path.read_bytes()
    paste(current, {'path': long_target})
    callback(current, 'create', 'Return', (long_target,))
    wait_for_predicate('visible single-field input limit guidance', lambda:
        any('--pin PATH' in line and ('limit' in line.lower() or 'longer' in line.lower())
            for line in page('New pin', ('path',))['status_updates']),
        time.monotonic() + 8, process=owner)
    assert state_path.read_bytes() == refused_bytes
    callback(current, 'dialog-close', 'Escape')
    current = manager()
    callback(current, 'new', 'n', alt=True)
    current = page('New pin', ('path',))
    paste(current, {'path': os.fsencode(folder / 'new-reference.txt')})
    callback(current, 'create', 'Return', (os.fsencode(folder / 'new-reference.txt'),))
    value = committed(initial, lambda v: len(v['pins']) == 7)
    assert base64.b64decode(value['pins'][-1]['target_bytes']) == os.fsencode(folder / 'new-reference.txt')
    projection(7)
    current = select_first()
    callback(current, 'edit', 'F2')
    current = page('Edit pin', ('label', 'icon'))
    label = b'Relabeled $[]; literal'
    paste(current, {'label': label, 'icon': b''})
    callback(current, 'label', 'l', (label,), alt=True)
    value = committed(value, lambda v: v['pins'][0]['label'] == label.decode())
    assert value['pins'][0]['id'] == initial['pins'][0]['id']
    current = select_first()
    callback(current, 'edit', 'F2')
    current = page('Edit pin', ('label', 'icon'))
    paste(current, {'icon': b'pinboard-fixture'})
    callback(current, 'icon', 'i', (b'pinboard-fixture',), alt=True)
    value = committed(value, lambda v: v['pins'][0].get('icon') == 'pinboard-fixture')
    projection(7)
    current = manager()
    callback(current, 'dialog-close', 'Escape')
    wait_for_predicate('metadata dialog closes before independent canvas pixel read',
        lambda: not view.named('Pinboard manager'), time.monotonic() + 4, process=owner)
    # Observe the real picture admission, not merely the requested icon name.
    def picture():
        state = protocol.status()
        if state['state'] != 'ready' or state['pending']:
            return None
        for row in reversed(rows(native)):
            if row['direction'] != 'out':
                continue
            fields = row['text'].split()
            if len(fields) == 7 and fields[:2] == ['SendToModule', protocol.alias] and fields[2] == 'Picture':
                if (fields[3] == protocol.token and
                        int(fields[4]) == state['current'] and
                        fields[5] == initial['pins'][0]['id']):
                    publication = protocol.cells()
                    if publication[:2] != (state['current'], state['epoch']):
                        return None
                    spec = os.fsdecode(bytes.fromhex(fields[6]))
                    path, render_size = published_picture_file(spec)
                    # Check the underlying publication, not the SVG render
                    # specification. Missing *current* resources remain a hard
                    # failure unless an actual native barrier proves retirement.
                    if not path.is_file():
                        after = protocol.status()
                        if (after['current'], after['epoch']) != publication[:2]:
                            return None
                        raise AssertionError('missing current native picture: ' + spec)
                    assert not path.is_symlink()
                    info = path.stat()
                    file_hash = digest(path)
                    content_name = re.fullmatch(
                        r'\.fvwm-xdg-([0-9a-f]{64})\.(png|svg|xpm)', path.name)
                    assert content_name and content_name[1] == file_hash
                    assert info.st_mode & 0o777 == 0o600
                    after = protocol.status()
                    if (after['current'], after['epoch']) != publication[:2]:
                        return None
                    cell = next(c for c in publication[2]
                        if c['id'] == initial['pins'][0]['id'])
                    if render_size:
                        size = value['layout']['icon_size'] * cell['quarters'] // 4
                        assert render_size == [size, size]
                    return dict(command=row['text'], spec=spec, file=str(path),
                        render_size=render_size, file_sha256=file_hash,
                        file_identity=dict(device=info.st_dev, inode=info.st_ino,
                            size=info.st_size), view=state['current'],
                        epoch=state['epoch'], cell=cell, native_ack=after)
        return None
    def painted():
        current = picture()
        if current is None:
            return None
        cell = current['cell']
        origin = oracle.origin(canvas)
        points = [(cell['x'] - origin[0] + px, cell['y'] - origin[1] + py)
            for py in range(5, min(cell['height'], 80), 2)
            for px in range(4, cell['width'] - 4, 2)]
        pixels = oracle.pixel_samples(canvas, points)
        after = protocol.status()
        if (after['current'], after['epoch']) != (current['view'], current['epoch']):
            return None
        return dict(admission=current, samples=len(pixels),
            icon_color_count=pixels.count(0x16789d), native_ack_after_pixels=after) \
            if 0x16789d in pixels else None
    painted_icon = wait_for_predicate('independent named icon pixels on native canvas',
        painted, time.monotonic() + 8, process=owner)
    evidence['named_icon'] = dict(source_sha256=digest(icon), persisted_name='pinboard-fixture',
        admission=painted_icon['admission'], native_pixels=painted_icon,
        format=args.named_icon_format)
    current = select_first()
    callback(current, 'move-dialog', 'F4')
    current = page('Move pin', ('monitor', 'column', 'row'))
    paste(current, {'monitor': b'', 'column': b'2', 'row': b'2'})
    callback(current, 'move', 'Return', (b'', b'2', b'2'))
    value = committed(value, lambda v: (v['pins'][0]['column'], v['pins'][0]['row']) == (2, 2))
    projected = projection(7)
    moved = next(c for c in projected[2] if c['id'] == initial['pins'][0]['id'])
    assert moved['column'] == 2 and moved['row'] == 2 and moved['visible']
    current = manager()
    key('Escape')
    focus(canvas)
    key('Home')
    key('Tab')
    stable = state_path.read_bytes()
    key('m')
    key('Right')
    key('Escape')
    protocol.status()
    assert state_path.read_bytes() == stable
    focus(canvas)
    key('Home')
    key('Tab')
    key('m')
    key('Right')
    key('Return')
    value = committed(value, lambda v:
        (v['pins'][0]['column'], v['pins'][0]['row']) == (3, 2))
    current = manager()
    callback(current, 'dialog-close', 'Escape')
    moved = next(c for c in projection(7)[2] if c['id'] == initial['pins'][0]['id'])
    pressed = False
    try:
        oracle.view.pointer.motion(moved['x'] + moved['width'] // 2,
            moved['y'] + moved['height'] // 2)
        oracle.view.pointer.button(True)
        pressed = True
        oracle.view.pointer.motion(moved['x'] + moved['width'] * 3 // 2,
            moved['y'] + moved['height'] // 2)
        oracle.view.pointer.button(False)
        pressed = False
    finally:
        if pressed:
            oracle.view.pointer.button(False)
    value = committed(value, lambda v:
        (v['pins'][0]['column'], v['pins'][0]['row']) == (4, 2))
    current = manager()
    callback(current, 'dialog-close', 'Escape')
    moved = next(c for c in projection(7)[2] if c['id'] == initial['pins'][0]['id'])
    def context_menu():
        menus = [window for window in view.children(client.root)
            if (a := view.attributes(window)) and a.map_state == 2 and a.save_under]
        return menus if len(menus) == 1 else None
    pressed = False
    try:
        view.pointer.motion(moved['x'] + moved['width'] // 2,
            moved['y'] + moved['height'] // 2)
        assert view.pointer.x.XTestFakeButtonEvent(client.display, 3, 1, 0)
        pressed = True
        client.x.XSync(client.display, 0)
        context = wait_for_predicate('held Button3 maps real pin context menu', context_menu,
            time.monotonic() + 4, process=owner)
        assert view.pointer.grabbed()
    finally:
        if pressed:
            assert view.pointer.x.XTestFakeButtonEvent(client.display, 3, 0, 0)
        key('Escape')
    wait_for_predicate('pin context releases actual pointer grab',
        lambda: not view.pointer.grabbed(), time.monotonic() + 4, process=owner)
    evidence['move_input'] = dict(keyboard_cancel_preserved_state=True,
        keyboard_commit=[3, 2], pointer_commit=[4, 2], context_menu=context)
    current = manager()
    callback(current, 'dialog-close', 'Escape')
    moved_icon = wait_for_predicate('moved named icon is painted after owned work settles',
        painted, time.monotonic() + 8, process=owner)
    assert (moved_icon['admission']['cell']['column'],
        moved_icon['admission']['cell']['row']) == (4, 2)
    evidence['named_icon_after_move'] = moved_icon
    capture('03-relabeled-icon-move.png')
    current = select_first()
    callback(current, 'edit', 'F2')
    current = page('Edit pin', ('label', 'icon'))
    paste(current, {'icon': b'pinboard-fixture-unavailable'})
    callback(current, 'icon', 'i', (b'pinboard-fixture-unavailable',), alt=True)
    value = committed(value, lambda v:
        v['pins'][0].get('icon') == 'pinboard-fixture-unavailable')
    current = manager()
    callback(current, 'dialog-close', 'Escape')
    fallback_cell = next(c for c in projection(7)[2]
        if c['id'] == initial['pins'][0]['id'])
    points = [(fallback_cell['x'] + px, fallback_cell['y'] + py)
        for py in range(8, fallback_cell['height'] - 8, 3)
        for px in range(8, fallback_cell['width'] - 8, 3)]
    colors = oracle.pixel_samples(canvas, points)
    assert len(set(colors)) > 1, 'unavailable named icon left no rendered label'
    assert value['pins'][0]['label'] == label.decode()
    evidence['unavailable_icon'] = dict(reference_preserved=True,
        label_preserved=True, native_interior_colors=len(set(colors)))
    # A separate browser remains a normal client above the desktop canvas.
    browser = runtime.start('concurrent-browser', [str(real_browser), '--socket', str(endpoint),
        '--dir', str(folder), '--view', 'list', '--watch', 'off', '--opener', '/bin/true'],
        'concurrent-browser.log', env)
    browser_window = view.wait('File browser')
    assert browser.poll() is None and oracle.owner() == canvas
    stack = view.children(client.root)
    assert stack.index(oracle.top(canvas)) < stack.index(oracle.top(browser_window))
    current = select_first()
    callback(current, 'unpin', 'u', alt=True)
    value = committed(value, lambda v: len(v['pins']) == 6 and
        initial['pins'][0]['id'] not in [p['id'] for p in v['pins']])
    for name, data in originals.items():
        assert (folder / name).read_bytes() == data
    assert digest(raw_path) == evidence['fixture']['literal_target_sha256']
    assert os.readlink(folder / 'alias') == 'notes.txt'
    evidence['checks'].append('actual New/Edit/Move/Unpin Forms persist references and preserve every source')
    # External writers advance the disk revision only. The live view stays old
    # until explicit Reload; stale UI saves must report failure, preserving disk.
    count_before = len(projection()[2])
    current = select_first()
    callback(current, 'edit', 'F2')
    current = page('Edit pin', ('label', 'icon'))
    cli('--pin', folder / 'missing-external.txt', '--label', 'External edit')
    external = stored()
    assert external['revision'] == value['revision'] + 1
    assert len(projection()[2]) == count_before
    disk = state_path.read_bytes()
    paste(current, {'label': b'Stale edit must refuse'})
    callback(current, 'label', 'l', (b'Stale edit must refuse',), alt=True)
    # The failure is visible in the real Form, not inferred from a quiet file.
    wait_for_predicate('visible stale revision error', lambda:
        any('revision' in line.lower() or 'conflict' in line.lower() or
            'save outcome unavailable' in line.lower()
            for line in page('Edit pin', ('label',))['status_updates']), time.monotonic() + 8, process=owner)
    assert state_path.read_bytes() == disk
    key('Escape')
    current = manager()
    callback(current, 'reload', 'F6')
    projection(len(external['pins']))
    assert state_path.read_bytes() == disk
    # Corrupt state and a held real writer lock are owned failure fixtures.
    # Restore bytes atomically ourselves only after observing refusal.
    state_path.write_bytes(b'{"schema_version":99, broken fixture\n')
    corrupt = state_path.read_bytes()
    current = manager()
    callback(current, 'reload', 'F6')
    wait_for_predicate('visible corrupt state refusal', lambda:
        any('invalid' in line.lower() or 'corrupt' in line.lower() or 'json' in line.lower()
            for line in page('Pinboard manager')['status_updates']), time.monotonic() + 8, process=owner)
    assert state_path.read_bytes() == corrupt
    assert len(projection()[2]) == len(external['pins'])
    replacement = state_path.with_suffix('.fixture-restore')
    replacement.write_bytes(disk)
    replacement.chmod(0o600)
    replacement.replace(state_path)
    import fcntl
    with state_path.with_name(state_path.name + '.lock').open('a+b') as lock:
        lock_path = state_path.with_name(state_path.name + '.lock')
        lock_path.chmod(0o600)
        fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        cli('--pin', folder / 'must-not-save', success=False)
        assert state_path.read_bytes() == disk
        fcntl.flock(lock, fcntl.LOCK_UN)
    current = manager()
    callback(current, 'reload', 'F6')
    capture('04-recovery-concurrent-browser.png')
    evidence['checks'].append('revision conflict, corrupt reload retention and nonblocking real writer-lock refusal')
    cli('--pin', long_target, '--label', 'Long literal CLI reference')
    external = stored()
    assert base64.b64decode(external['pins'][-1]['target_bytes']) == long_target
    current = manager()
    callback(current, 'reload', 'F6')
    projection(len(external['pins']))
    disk = state_path.read_bytes()
    if args.xorg_config:
        # Reuse the already owned dummy server. Real output properties and
        # core MonitorUI/legacy reservations independently constrain Cells.
        operations = evidence.setdefault('operations', [])
        reservation_hook = ('AddToFunc SlatePanel I '
            'EwmhBaseStruts screen DUMMY1 11 13 160 19')
        wm(reservation_hook)
        evidence['secondary_reservation_setup'] = dict(hook=reservation_hook,
            reason='normal SlatePanel rebuild resets all struts before reserving the primary; '
                'append this fixture-owned secondary reservation after that reset',
            scope='owned session configuration; normal panel/topology rebuild retained',
            expected_actual_pixel_insets=[11, 13, 160, 19])
        def randr(arguments, label, predicate):
            run_required_xrandr(runtime, env['DISPLAY'], arguments, label, operations)
            return wait_for_authoritative_state(runtime, env['DISPLAY'], label,
                predicate, time.monotonic() + 8, label)

        run_required_xrandr(runtime, env['DISPLAY'], ['--addmode', 'DUMMY1', '2560x2160'],
            'pinboard-secondary-mode', operations)
        mixed_arguments = ['--output', 'DUMMY0', '--mode', '1280x1024', '--pos', '0x0',
            '--primary', '--set', 'WIDTH_MM', '339', '--set', 'HEIGHT_MM', '271',
            '--output', 'DUMMY1', '--mode', '2560x2160', '--pos', '1280x0',
            '--set', 'WIDTH_MM', '339', '--set', 'HEIGHT_MM', '286', '--fb', '3840x2160']
        def mixed_ready(v):
            return (v['monitors'].get('DUMMY0', {}).get('width') == 1280 and
                v['monitors'].get('DUMMY1', {}).get('x') == 1280 and
                v['monitors']['DUMMY1']['mm_width'] == 339 and
                v['monitors']['DUMMY1']['mm_height'] == 286)
        mixed = randr(mixed_arguments, 'pinboard-mixed-real-outputs', mixed_ready)
        def native_mixed_ready():
            publication = projection(len(external['pins']))
            metadata = {}
            for record in rows(native):
                if record['direction'] != 'in':
                    continue
                fields = record['text'].split()
                if (len(fields) == 13 and
                        fields[:3] == ['MonitorUI', '1', 'ROW'] and
                        fields[3] == publication[1]):
                    name = bytes.fromhex(fields[12]).decode('utf-8', 'strict')
                    metadata[name] = list(map(int, fields[8:12]))
            expected = {name: [fact[k] for k in ('x', 'y', 'width', 'height')]
                for name, fact in mixed['monitors'].items()}
            if set(metadata) == {'DUMMY0', 'DUMMY1'} and metadata == expected:
                return dict(view=publication[0], epoch=publication[1], bounds=metadata)
            return None
        evidence['secondary_reservation_setup']['native_before_manual_command'] = \
            wait_for_predicate('native publication has both actual mixed-monitor bounds',
                native_mixed_ready, time.monotonic() + 10, process=owner)
        cli('--pin', folder / 'secondary-missing', '--label', 'Secondary preferred',
            '--monitor', 'DUMMY1', '--column', '1', '--row', '1')
        external = stored()
        preferred = external['pins'][-1]
        assert base64.b64decode(preferred['monitor_bytes']) == b'DUMMY1'
        current = manager()
        callback(current, 'reload', 'F6')
        disk = state_path.read_bytes()
        wm('EwmhBaseStruts screen DUMMY1 11 13 160 19')

        def native_layout(facts, require_reserved=False):
            value = projection(len(external['pins']))
            incoming = [r['text'] for r in rows(native) if r['direction'] == 'in']
            metadata = {}
            for line in incoming:
                fields = line.split()
                if len(fields) == 13 and fields[:3] == ['MonitorUI', '1', 'ROW'] and fields[3] == value[1]:
                    name = bytes.fromhex(fields[12]).decode('utf-8', 'strict')
                    metadata[name] = dict(quarters=int(fields[6]), bounds=list(map(int, fields[8:12])), raw=line)
            if set(metadata) != set(facts['monitors']):
                return None
            reservations = {}
            for line in incoming:
                if line.startswith('Monitor '):
                    fields = shlex.split(line)
                    if len(fields) == 16 and fields[1] in metadata:
                        reservations[fields[1]] = list(map(int, fields[12:16]))
            if require_reserved and reservations.get('DUMMY1') != [11, 13, 160, 19]:
                return None
            for name, row in metadata.items():
                fact = facts['monitors'][name]
                assert row['bounds'] == [fact[k] for k in ('x', 'y', 'width', 'height')]
            for cell in value[2]:
                if not cell['visible']:
                    continue
                name = bytes.fromhex(cell['monitor']).decode('utf-8', 'strict')
                assert name in metadata and cell['quarters'] == metadata[name]['quarters']
                assert cell['width'] == external['layout']['cell_width'] * cell['quarters'] // 4
                assert cell['height'] == external['layout']['cell_height'] * cell['quarters'] // 4
                bx, by, width, height = metadata[name]['bounds']
                left, right, top, bottom = reservations.get(name, [0, 0, 0, 0])
                assert bx + left <= cell['x'] and by + top <= cell['y']
                assert cell['x'] + cell['width'] <= bx + width - right
                assert cell['y'] + cell['height'] <= by + height - bottom
            selected = next(c for c in value[2] if c['id'] == preferred['id'])
            return dict(view=value[0], epoch=value[1], metadata=metadata,
                legacy_reservations=reservations, preferred=selected, cells=value[2],
                reservation_freshness='locally associated legacy sample; not atomic')

        layout = wait_for_predicate('mixed-scale work-area-contained native projection',
            lambda: native_layout(mixed, True), time.monotonic() + 10, process=owner)
        assert {row['quarters'] for row in layout['metadata'].values()} == {4, 8}
        assert bytes.fromhex(layout['preferred']['monitor']) == b'DUMMY1'
        assert layout['preferred']['x'] >= 1280 + 11 and layout['preferred']['y'] >= 160
        before_epoch = layout['epoch']
        unplugged = randr(['--output', 'DUMMY1', '--off', '--fb', '1280x1024'],
            'pinboard-secondary-unplug', lambda v: set(v['monitors']) == {'DUMMY0'})
        fallback = wait_for_predicate('absent preferred monitor temporary fallback',
            lambda: native_layout(unplugged), time.monotonic() + 10, process=owner)
        assert fallback['epoch'] != before_epoch
        assert bytes.fromhex(fallback['preferred']['monitor']) == b'DUMMY0'
        assert state_path.read_bytes() == disk, 'monitor absence rewrote preferred placement'
        replugged = randr(mixed_arguments, 'pinboard-secondary-replug', mixed_ready)
        restored_layout = wait_for_predicate('preferred monitor returns without saved mutation',
            lambda: native_layout(replugged), time.monotonic() + 10, process=owner)
        assert bytes.fromhex(restored_layout['preferred']['monitor']) == b'DUMMY1'
        assert state_path.read_bytes() == disk
        # Workspace changes must retain the sticky managed surface and saved
        # reference positions while updating current core work-area samples.
        wm('GotoDesk 0 1')
        assert oracle.words(client.root, '_NET_CURRENT_DESKTOP', 6) == [1]
        assert view.attributes(canvas).map_state == 2 and oracle.owner() == canvas
        workspace_property = oracle.property(canvas, '_NET_WM_DESKTOP')
        evidence['workspace_property'] = dict(window=canvas,
            current_desktop=oracle.property(client.root, '_NET_CURRENT_DESKTOP'),
            desktop=workspace_property)
        assert workspace_property == dict(type=6, format=32, values=[0xffffffff])
        after_desk = projection(len(external['pins']))
        assert state_path.read_bytes() == disk
        wm('GotoDesk 0 0')
        # XRandR 1.5 named monitor coordinates may be negative even though the
        # drawable root begins at zero. Use a real server publication, retain
        # primary pins, and compare its metadata to the independent query.
        negative = randr(['--setmonitor', 'PinboardNegative',
            '640/169x480/127+-640+0', 'none'], 'pinboard-negative-monitor',
            lambda v: v['monitors'].get('PinboardNegative', {}).get('x') == -640)
        negative_layout = wait_for_predicate('negative-origin MonitorUI publication',
            lambda: native_layout(negative), time.monotonic() + 10, process=owner)
        assert negative_layout['metadata']['PinboardNegative']['bounds'][0] == -640
        assert state_path.read_bytes() == disk
        current = manager()
        cli('--pin', folder / 'negative-preferred-missing', '--label', 'Off-root preferred',
            '--monitor', 'PinboardNegative', '--column', '0', '--row', '0')
        external = stored()
        negative_pin = external['pins'][-1]
        assert base64.b64decode(negative_pin['monitor_bytes']) == b'PinboardNegative'
        assert (negative_pin['column'], negative_pin['row']) == (0, 0)
        callback(current, 'reload', 'F6')
        current = manager()
        callback(current, 'dialog-close', 'Escape')
        disk = state_path.read_bytes()
        negative_preferred_layout = wait_for_predicate('off-root preference projects to drawable root',
            lambda: native_layout(negative), time.monotonic() + 10, process=owner)
        negative_cell = next(c for c in negative_preferred_layout['cells']
            if c['id'] == negative_pin['id'])
        root_geometry = client.geometry(client.root)
        assert negative_cell['visible'] and bytes.fromhex(
            negative_cell['monitor']) != b'PinboardNegative'
        assert 0 <= negative_cell['x'] < negative_cell['x'] + negative_cell['width'] <= root_geometry[2]
        assert 0 <= negative_cell['y'] < negative_cell['y'] + negative_cell['height'] <= root_geometry[3]
        primary = negative_preferred_layout['metadata']['DUMMY0']
        left, _, top, _ = negative_preferred_layout['legacy_reservations']['DUMMY0']
        manage_point = (primary['bounds'][0] + left +
            external['layout']['cell_width'] * primary['quarters'] // 8,
            primary['bounds'][1] + top +
            external['layout']['cell_height'] * primary['quarters'] // 8)
        origin = oracle.origin(canvas)
        assert any(origin[0] + x <= manage_point[0] < origin[0] + x + width and
            origin[1] + y <= manage_point[1] < origin[1] + y + height
            for x, y, width, height in oracle.rectangles(canvas))
        oracle.click(*manage_point)
        reachable = page('Pinboard manager')
        assert any(negative_pin['id'][:8] in line for line in reachable['lines'])
        assert state_path.read_bytes() == disk
        positive = randr(['--delmonitor', 'PinboardNegative'], 'pinboard-negative-monitor-remove',
            lambda v: set(v['monitors']) == {'DUMMY0', 'DUMMY1'})
        removed_negative = wait_for_predicate('removed off-root preference remains a visible reference',
            lambda: native_layout(positive), time.monotonic() + 10, process=owner)
        assert next(c for c in removed_negative['cells']
            if c['id'] == negative_pin['id'])['visible']
        assert state_path.read_bytes() == disk
        evidence['negative_preference'] = dict(id=negative_pin['id'],
            saved_monitor=negative_pin['monitor_bytes'], saved_grid=[0, 0],
            actual_fallback=negative_cell, root_geometry=root_geometry,
            manager_point=manage_point, actual_manager_window=reachable['window'],
            saved_state_unchanged=True)
        evidence['topology_lifecycle'] = dict(mixed=layout, unplugged=fallback,
            replugged=restored_layout, workspace_publication=after_desk,
            negative_origin=negative_layout,
            negative_preferred=negative_preferred_layout,
            negative_removed=removed_negative, saved_state_unchanged=True,
            physical_hotplug_qualified=False)
        evidence['checks'].append('real RandR mixed scale, reservations, nonzero/negative origins, workspace and hotplug fallback')
    else:
        evidence.setdefault('skipped', []).append(dict(case='actual RandR topology/work-area/hotplug/mixed-scale',
            reason='requires --xorg-config; Xvfb result does not qualify that contract'))
    # Persistence across an actual owner lifecycle. Stop only our exact child;
    # the core-managed native module independently releases or loses its lease.
    owner.close(time.monotonic() + 5)
    wait_for_predicate('closed owner releases native selection', lambda: oracle.owner() == 0,
        time.monotonic() + 18)
    assert state_path.read_bytes() == disk
    owner, canvas, protocol = start_owner('restarted-owner')
    restored = projection(len(external['pins']))
    assert [p['id'] for p in stored()['pins']] == [p['id'] for p in external['pins']]
    assert state_path.read_bytes() == disk
    # KillModule is the core's ordinary owned-child teardown, addressed to this
    # exact random alias. The Go owner must detect loss and finish/reap work.
    enrolled = [json.loads(path.read_text()) for path in native.glob('module-*.json')
        if protocol.alias in json.loads(path.read_text())['arguments']]
    assert len(enrolled) == 1
    enrolled = enrolled[0]
    native_binary = build / 'modules/FvwmPinboard/FvwmPinboard'
    assert Path(enrolled['real']).resolve() == native_binary.resolve()
    assert digest(native_binary) == evidence['binary_sha256'][
        'modules/FvwmPinboard/FvwmPinboard']
    stopped = native / ('stopped-%d.json' % enrolled['proxy'])
    assert not stopped.exists(), 'native child already stopped before kill request'
    loss_deadline = time.monotonic() + 12
    evidence['native_child_loss_request'] = dict(module='FvwmPinboard',
        alias=protocol.alias, window=canvas, enrollment=enrolled,
        binary_sha256=digest(native_binary), deadline_monotonic=loss_deadline)
    wm('KillModule FvwmPinboard ' + protocol.alias)
    def native_child_lost():
        if not stopped.exists() or oracle.owner() != 0:
            return None
        waited = json.loads(stopped.read_text())
        assert waited['child'] == enrolled['child']
        assert waited['returncode'] is not None
        return dict(enrollment=enrolled, waited=waited,
            selection_owner=0, observed_monotonic=time.monotonic(),
            owner_running_when_native_loss_observed=owner.poll() is None)
    lost_native = wait_for_predicate('exact killed native child Wait and selection release',
        native_child_lost, loss_deadline)
    evidence['native_child_loss_request']['observed_loss'] = lost_native
    exit_code = owner.wait_until_exit(loss_deadline)
    assert oracle.owner() == 0 and state_path.read_bytes() == disk
    evidence['owner_lifetime'] = dict(persisted_state_sha256=digest(state_path),
        restored=restored, child_loss_owner_returncode=exit_code,
        native_child_loss=lost_native,
        state_preserved=True, browser_still_alive=browser.poll() is None)
    assert browser.poll() is None
    evidence['checks'].append('owner restart persistence and native-child loss without target mutation')


def main():
    if len(sys.argv) > 1 and sys.argv[1] == '--module-proxy':
        return module_proxy(sys.argv[2], sys.argv[3], sys.argv[4:])
    parser = argparse.ArgumentParser(description=__doc__, allow_abbrev=False)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--artifacts-dir', type=Path, required=True)
    parser.add_argument('--profiles', nargs='+', choices=PROFILES, default=list(PROFILES))
    parser.add_argument('--xorg-config', type=Path)
    parser.add_argument('--display', help='caller-owned authenticated disposable server; native-only')
    parser.add_argument('--scale', choices=('1', '1.25', '2'))
    parser.add_argument('--native-only', action='store_true')
    parser.add_argument('--crop-only', action='store_true',
        help='only native opaque/alpha containment; implies --native-only')
    parser.add_argument('--shape-unavailable', action='store_true')
    parser.add_argument('--skip-lease-expiry', action='store_true',
        help='reuse a separately source-bound lease-expiry result; records not run')
    parser.add_argument('--named-icon-format', choices=('png', 'svg'), default='png',
        help='PNG requires libpng; SVG is an explicit enabled-renderer precondition')
    args = parser.parse_args()
    if args.crop_only:
        args.native_only = True
        if args.shape_unavailable:
            parser.error('--crop-only needs SHAPE')
    if args.scale and not args.xorg_config:
        parser.error('--scale requires --xorg-config')
    if args.display and (not args.native_only or args.xorg_config or args.shape_unavailable):
        parser.error('--display requires --native-only without Xorg/SHAPE-disable setup')
    if args.shape_unavailable and args.xorg_config:
        parser.error('--shape-unavailable requires private Xvfb')
    args.artifacts_dir.mkdir(parents=True, exist_ok=True)
    root = Path(tempfile.mkdtemp(prefix='pinboard-', dir=args.artifacts_dir.resolve()))
    print(json.dumps({'artifacts_dir': str(root)}), flush=True)
    outcomes = []
    def expired(signum, stack):
        raise TimeoutError('pinboard profile exceeded its fixed 240-second bound')
    previous = signal.signal(signal.SIGALRM, expired)
    try:
        for profile in args.profiles:
            signal.alarm(240)
            try:
                outcomes.append(run(args, profile, root))
            finally:
                signal.alarm(0)
    finally:
        signal.signal(signal.SIGALRM, previous)
    return 77 if 'unavailable' in outcomes else 0


if __name__ == '__main__':
    raise SystemExit(main())
