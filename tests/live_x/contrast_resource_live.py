#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or any later version.
"""Observe resource-preview ownership through real core, module and X state.

Requires a PNG-enabled built core/MFL, image_owner_probe, Xvfb and libXtst.
No worker or core reply is synthesized. Run the old mfl_preview_live.py as a
separate scalar-opacity regression. --execution-profile labels actual execution;
it does not establish native CPU or sanitizer qualification.

--complete-bundle-only selects the version3 group without rerunning v1/v2.
--complete-bundle-tail-only selects only named launcher conflict, nonmodal actor
loss and external-root conflict/preservation through actual core shutdown. Earlier
v3 groups are explicitly skipped; their prior results must be retained separately.
--owned-root-shutdown-only selects normal owned publication, Quit and removal
without leases or external takeover; full v3 separately preserves external roots.
Supply the actual --backer and --worker; ordinary CoreOwnedPixmap publication
must finish before a lease starts. --plain-root selects a solid ordinary root
for a separately supplied minimal build. --allocation-failure still requires
the separately linked fault core. No successful worker/owner reply is injected.
--topology-change accepts JSON {"change": [argv...], "restore": [argv...]};
only literal {display} substitution is performed, with no shell. It requires
an actual changed server/RandR snapshot and exact restored topology. Omission
records an unrun topology case. Source/binary hashes and actual module Wait
receipts are retained; the native image manager owns its private worker Wait.
"""
import argparse
import copy
import ctypes as C
import hashlib
import json
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import sys
import time

# Native wrappers must relay immediately, before fixture imports initialize X.
if (__name__ == '__main__' and len(sys.argv) > 1 and
        sys.argv[1] == '--module-proxy'):
    from native_module_proxy import module_proxy
    raise SystemExit(module_proxy(sys.argv[2], sys.argv[3], sys.argv[4:]))

from geometry_transition_live import Client, Pointer, ConfigureEvent
from image_colorsets_live import records, XColor
from colorset_live import WindowAttributes
from raise_unmanaged_live import SetAttributes
from image_owners_live import png
from live_x_harness import HarnessRuntime, start_display_server, wait_for_predicate
from mfl_request_live import frame, reply
from mfl_stream_live import Connection
from session_x_errors import SessionXErrors
from browser_continuity_live import cleanup_receipts
from browser_handoff_live import child_identity
from settings_preview_live import (BufferedTintReference, PreviewWitness,
    binary_hash, hashes, png_rgb)


class PropertyEvent(C.Structure):
    _fields_ = [('type', C.c_int), ('serial', C.c_ulong),
        ('send_event', C.c_int), ('display', C.c_void_p),
        ('window', C.c_ulong), ('atom', C.c_ulong),
        ('time', C.c_ulong), ('state', C.c_int)]


class UnmapEvent(C.Structure):
    _fields_ = [('type', C.c_int), ('serial', C.c_ulong),
        ('send_event', C.c_int), ('display', C.c_void_p),
        ('event', C.c_ulong), ('window', C.c_ulong),
        ('from_configure', C.c_int)]


class ObserverEvent(C.Union):
    _fields_ = [('property', PropertyEvent), ('unmap', UnmapEvent),
        ('configure', ConfigureEvent), ('pad', C.c_long * 24)]


ROLE_IDS = (0, 1, 2, 3, 4, 5, 6, 7, 8, 10, 11, 12, 13, 14)
PLAN = {'plan_version': 1, 'roles': [dict(id=identifier,
    foreground='#ffffff', background='#000000', highlight='#ffffff',
    shadow='#000000') for identifier in ROLE_IDS]}
OPAQUE_PLAN = {'plan_version': 2, 'roles': [dict(id=identifier,
    foreground='#102030', background='#e0d0c0', highlight='#abcdef',
    shadow='#010203') for identifier in ROLE_IDS]}


def run(args):
    args.artifacts_dir.mkdir(parents=True, exist_ok=False)
    evidence = {'status': 'running', 'execution_profile': args.execution_profile,
        'platform': dict(system=os.uname().sysname,
            release=os.uname().release, machine=os.uname().machine),
        'cases': {}, 'limits': [
            'No physical GPU, external compositor or native-CPU claim.',
            'Build-tree fixture reads source-owned ConfigFvwmDefaults; an absent '
            'compiled-prefix startup Read remains a setup warning, not installed acceptance.',
            'Synthetic ownerless masks need pure-policy coverage.'],
        'core_artifact': 'fault-test' if args.allocation_failure else 'normal',
        'group_results': {
            'v1': 'skipped by --complete-bundle-only' if args.complete_bundle_only
                else 'pending',
            'v2': 'pending' if args.opaque_colors and not args.complete_bundle_only
                else 'skipped',
            'v3': 'pending' if args.complete_bundle or args.complete_bundle_only
                else 'skipped'},
        'core_sha256': hashlib.sha256(args.fvwm.read_bytes()).hexdigest()}
    source = Path(__file__).resolve().parents[2]
    inputs = ['tests/live_x/contrast_resource_live.py',
        'tests/live_x/settings_preview_live.py', 'tests/image_owner_probe.c',
        'tests/live_x/native_module_proxy.py', 'tests/live_x/session_x_errors.py',
        'tests/live_x/live_x_harness.py', 'tests/live_x/mfl_request_live.py',
        'tests/live_x/mfl_stream_live.py', 'tests/live_x/image_owners_live.py',
        'tests/live_x/image_colorsets_live.py',
        'tests/live_x/geometry_transition_live.py',
        'tests/live_x/colorset_live.py',
        'tests/live_x/raise_unmanaged_live.py',
        'tests/live_x/browser_continuity_live.py',
        'tests/live_x/browser_handoff_live.py',
        'fvwm/appearance_preview.c', 'fvwm/preview_policy.c',
        'fvwm/colorset.c', 'fvwm/image_async.c', 'fvwm/image_async.h',
        'fvwm/image_async_wait.c', 'fvwm/module_interface.c',
        'fvwm/query_json.c', 'libs/PictureImageLoader.c',
        'libs/PictureUtils.c', 'libs/picture_argb.c', 'libs/FRender.c',
        'libs/FRenderInit.c',
        'modules/FvwmMFL/request.c', 'modules/FvwmMFL/FvwmMFL.c',
        'modules/FvwmBacker/FvwmBacker.c']
    inputs.extend(str(path.relative_to(source))
        for path in sorted((source / 'fvwm').glob('root_background.*')))
    for pattern in ('picture_async*.[ch]',
            'image_background*.[ch]', 'image_effect*.[ch]',
            'image_png*.[ch]', 'image_process*.[ch]'):
        inputs.extend(str(path.relative_to(source))
            for path in sorted((source / 'libs').glob(pattern)))
    inputs.append('bin/fvwm-image-svg-worker.c')
    evidence['source_sha256'] = hashes(source, inputs)
    evidence['binary_inputs'] = {name: binary_hash(getattr(args, name))
        for name in ('fvwm', 'mfl', 'probe')}
    if args.backer:
        evidence['binary_inputs']['backer'] = binary_hash(args.backer)
    if args.worker:
        evidence['binary_inputs']['worker'] = binary_hash(args.worker)
    config_header = args.config_header or next((path for path in (
        args.fvwm.parent / 'config.h', args.fvwm.parent.parent / 'config.h')
        if path.is_file()), None)
    if args.complete_bundle or args.complete_bundle_only:
        assert config_header, 'v3 requires actual build --config-header'
    evidence['generated_config'] = (binary_hash(config_header)
        if config_header else 'unavailable; use lead build receipt')
    if config_header:
        evidence['binary_inputs']['config_header'] = binary_hash(config_header)
    with HarnessRuntime(prefix='fvwm-contrast-resource-') as runtime:
        connections, client, errors, core = [], None, None, None
        native = None
        held = False
        try:
            display = args.display
            if display is None:
                _, display = start_display_server(runtime, 'Xvfb',
                    ['Xvfb', '-screen', '0', '800x600x24', '-noreset'],
                    'xserver.log', time.monotonic() + 10)
            modules = runtime.root / 'modules'
            modules.mkdir()
            (modules / 'ContrastObserver').symlink_to(args.probe)
            native = runtime.root / 'native-records'
            native.mkdir()
            if args.complete_bundle or args.complete_bundle_only:
                assert args.backer, '--backer required for actual owned-root setup'
                for name, executable in (('FvwmBacker', args.backer),
                        ('FvwmMFL', args.mfl), ('ContrastObserver', args.probe),
                        ('ContrastSnapshot', args.probe),
                        (args.probe.name, args.probe)):
                    wrapper = modules / name
                    wrapper.unlink(missing_ok=True)
                    wrapper.write_text('#!/bin/sh\nexec ' + shlex.join([
                        sys.executable, str(Path(__file__).resolve()),
                        '--module-proxy', str(executable), str(native)]) +
                        ' "$@"\n')
                    wrapper.chmod(0o700)
            image, shape = runtime.root / 'source.png', runtime.root / 'shape.xbm'
            image.write_bytes(png())
            evidence['fixture_inputs'] = dict(source_png=binary_hash(image),
                candidate_png_sha256=hashlib.sha256(
                    png_rgb(0, 0, 255)).hexdigest())
            shape.write_text('#define shape_width 2\n#define shape_height 2\n'
                'static unsigned char shape_bits[] = {\n 0x01, 0x02};\n')
            config = runtime.root / 'config'
            defaults = Path(__file__).resolve().parents[2] / 'fvwm/ConfigFvwmDefaults'
            assert defaults.is_file(), defaults
            config.write_text(f'ModulePath {modules}:{args.mfl.parent}:{args.probe.parent}\n'
                f'Read {defaults}\n'
                'DefaultFont fixed\nEdgeScroll 0 0\nEdgeThickness 0\n'
                'Style * TileCascadePlacement\n'
                'MenuStyle * Translucent 31\n'
                'MenuStyle SlateLauncherStyle Translucent 42\n'
                'AddToMenu ContrastMenu "Held preview" Title\n+ "Close" Nop\n'
                'ChangeMenuStyle SlateLauncherStyle ContrastMenu\n'
                'Mouse 1 R N Popup ContrastMenu\n'
                'AddToFunc ContrastWait I Echo contrast-wait-entered\n'
                '+ I Wait ContrastWaitWitness\n'
                '+ I Echo contrast-wait-finished\n'
                '*FvwmBacker: CoreOwnedPixmap\n'
                '*FvwmBacker: Command (Desk *, Page * *) Colorset 20\n'
                'AddToFunc StartFunction I Module FvwmMFL\n' +
                ''.join(f'Colorset {identifier} fg #123456, bg #345678, '
                    'hi #56789a, sh #789abc, fgsh #aa55aa, IconAlpha 63, '
                    'IconTint #332211 17\n' for identifier in ROLE_IDS))
            (args.artifacts_dir / 'configuration.fvwm').write_bytes(config.read_bytes())
            allocation_marker = runtime.root / 'allocation-failure.txt'
            environment = runtime.environment({
                'FVWMMFL_SOCKET_PATH': str(runtime.root),
                'FVWM_USERDIR': str(runtime.root),
                'FVWM_TEST_CONTRAST_ALLOC_FAIL': str(allocation_marker)})
            if args.worker:
                environment['FVWM_IMAGE_SVG_WORKER'] = str(args.worker)
                environment['FVWM_IMAGE_EFFECT_WORKER'] = str(args.worker)
            core = runtime.start('core', [str(args.fvwm), '-d', display,
                '-f', str(config), '-v', '-o', str(runtime.root / 'debug.log')],
                'core.log', environment)
            evidence['core_identity'] = child_identity(core.process.pid, os.getpid())
            endpoint = runtime.root / f"fvwm_mfl_{display.replace(':', '_')}.sock"
            wait_for_predicate('contrast endpoint', endpoint.exists,
                time.monotonic() + 12, process=core)

            def connect():
                connection = Connection(endpoint)
                connection.socket.settimeout(10)
                connection.record()
                connections.append(connection)
                return connection

            writer, owner, other = connect(), connect(), connect()
            writer.send(b'set echo\n')
            serial = 0

            def command(text):
                nonlocal serial
                serial += 1
                writer.send((text + '\n').encode())
                writer.echo('contrast-command-' + str(serial))

            def rpc(c=owner, resource='preview', error=None, **fields):
                nonlocal serial
                serial += 1
                token = 'contrast-' + str(serial)
                c.send(frame(token, resource=resource, **fields))
                result = reply(c, token, 'error' if error else 'ok')
                if error:
                    assert result == error, (result, error)
                return result

            client = Client(display)
            errors = SessionXErrors(client, evidence).install()
            pointer = Pointer(client)
            x, d = client.x, client.display
            x.XGetImage.argtypes = [C.c_void_p, C.c_ulong, C.c_int, C.c_int,
                C.c_uint, C.c_uint, C.c_ulong, C.c_int]
            x.XGetImage.restype = C.c_void_p
            x.XGetPixel.argtypes = [C.c_void_p, C.c_int, C.c_int]
            x.XGetPixel.restype = C.c_ulong
            x.XDestroyImage.argtypes = [C.c_void_p]
            x.XDefaultColormap.argtypes = [C.c_void_p, C.c_int]
            x.XDefaultColormap.restype = C.c_ulong
            x.XQueryColor.argtypes = [C.c_void_p, C.c_ulong,
                C.POINTER(XColor)]
            x.XQueryColor.restype = C.c_int
            x.XKeysymToKeycode.argtypes = [C.c_void_p, C.c_ulong]
            x.XKeysymToKeycode.restype = C.c_ubyte
            pointer.x.XTestFakeKeyEvent.argtypes = [C.c_void_p, C.c_uint,
                C.c_int, C.c_ulong]
            pointer.x.XTestFakeKeyEvent.restype = C.c_int
            x.XCreatePixmap.argtypes = [C.c_void_p, C.c_ulong, C.c_uint,
                C.c_uint, C.c_uint]
            x.XCreatePixmap.restype = C.c_ulong
            x.XCreateGC.argtypes = [C.c_void_p, C.c_ulong, C.c_ulong, C.c_void_p]
            x.XCreateGC.restype = C.c_void_p
            x.XSetForeground.argtypes = [C.c_void_p, C.c_void_p, C.c_ulong]
            x.XFillRectangle.argtypes = [C.c_void_p, C.c_ulong, C.c_void_p,
                C.c_int, C.c_int, C.c_uint, C.c_uint]
            x.XFreeGC.argtypes = [C.c_void_p, C.c_void_p]
            x.XInternAtom.argtypes = [C.c_void_p, C.c_char_p, C.c_int]
            x.XInternAtom.restype = C.c_ulong
            x.XChangeProperty.argtypes = [C.c_void_p, C.c_ulong, C.c_ulong,
                C.c_ulong, C.c_int, C.c_int, C.c_void_p, C.c_int]
            x.XSetWindowBackgroundPixmap.argtypes = [C.c_void_p, C.c_ulong, C.c_ulong]
            x.XClearWindow.argtypes = [C.c_void_p, C.c_ulong]
            x.XGetWindowProperty.argtypes = [C.c_void_p, C.c_ulong, C.c_ulong,
                C.c_long, C.c_long, C.c_int, C.c_ulong, C.POINTER(C.c_ulong),
                C.POINTER(C.c_int), C.POINTER(C.c_ulong), C.POINTER(C.c_ulong),
                C.POINTER(C.POINTER(C.c_ubyte))]

            def publish_root(color=0x00cc33):
                # The fixture remains the actual X resource owner until after
                # core shutdown. Only a real PropertyNotify reports this change.
                pixmap = x.XCreatePixmap(d, client.root, 2, 2, 24)
                assert pixmap
                gc = x.XCreateGC(d, pixmap, 0, None)
                assert gc
                try:
                    x.XSetForeground(d, gc, color)
                    x.XFillRectangle(d, pixmap, gc, 0, 0, 2, 2)
                finally:
                    x.XFreeGC(d, gc)
                value = C.c_ulong(pixmap)
                pixmap_type = x.XInternAtom(d, b'PIXMAP', 0)
                for name in (b'_XROOTPMAP_ID', b'ESETROOT_PMAP_ID'):
                    atom = x.XInternAtom(d, name, 0)
                    x.XChangeProperty(d, client.root, atom, pixmap_type, 32,
                        0, C.byref(value), 1)
                x.XSetWindowBackgroundPixmap(d, client.root, pixmap)
                x.XClearWindow(d, client.root)
                x.XSync(d, 0)
                assert pixels(pixmap) == [color] * 4
                return pixmap

            def root_property():
                actual, count, after = C.c_ulong(), C.c_ulong(), C.c_ulong()
                format_ = C.c_int()
                data = C.POINTER(C.c_ubyte)()
                atom = x.XInternAtom(d, b'_XROOTPMAP_ID', 0)
                try:
                    assert x.XGetWindowProperty(d, client.root, atom, 0, 1,
                        0, 0, C.byref(actual), C.byref(format_), C.byref(count),
                        C.byref(after), C.byref(data)) == 0
                    assert format_.value == 32 and count.value == 1 and data
                    return C.cast(data, C.POINTER(C.c_ulong))[0]
                finally:
                    if data:
                        x.XFree(data)

            def key(symbol):
                code = x.XKeysymToKeycode(d, symbol)
                assert code
                for down in (1, 0):
                    assert pointer.x.XTestFakeKeyEvent(d, code, down, 0)
                x.XSync(d, 0)

            def pixels(drawable, width=2, height=2):
                assert drawable > 1
                assert 0 < width <= 800 and 0 < height <= 600
                image_ = x.XGetImage(d, drawable, 0, 0, width, height,
                    C.c_ulong(-1).value, 2)
                x.XSync(d, 0)
                assert image_ and not errors.unexpected, errors.unexpected
                try:
                    return [x.XGetPixel(image_, px, py)
                        for py in range(height) for px in range(width)]
                finally:
                    x.XDestroyImage(image_)

            def backing_witnesses(rows):
                # Identical source PNGs can yield distinct rendered pixmaps.
                # Establish each role's actual resource identities and pixels;
                # ownership/restoration does not promise cross-role aliasing.
                witnesses = {}
                for identifier in (0, 1):
                    row = rows[identifier]
                    assert row[8] > 1 and row[9] > 1, row
                    assert row[11:13] == (2, 2) and row[14:16] == (2, 2), row
                    image_pixels, mask_pixels = pixels(row[8]), pixels(row[9])
                    assert image_pixels == [0xff0000] * 4, (identifier, image_pixels)
                    assert set(mask_pixels) == {0, 1}, (identifier, mask_pixels)
                    witnesses[identifier] = {'pixmap_id': row[8], 'shape_id': row[9],
                        'pixmap_pixels': image_pixels, 'shape_pixels': mask_pixels}
                return witnesses

            def gradient_witnesses(rows):
                witnesses = {}
                for identifier in ROLE_IDS:
                    row = rows[identifier]
                    assert row[8] > 1 and row[13] == 1, row
                    root_, gx, gy = C.c_ulong(), C.c_int(), C.c_int()
                    width, height, border, depth = (C.c_uint() for _ in range(4))
                    assert x.XGetGeometry(d, row[8], C.byref(root_),
                        C.byref(gx), C.byref(gy), C.byref(width),
                        C.byref(height), C.byref(border), C.byref(depth))
                    x.XSync(d, 0)
                    assert not errors.unexpected, errors.unexpected
                    assert width.value == 2 and 0 < height.value <= 600
                    assert depth.value == 24 and border.value == 0
                    assert row[11:13] == (width.value, height.value), row
                    first_row = pixels(row[8], width.value, 1)
                    assert first_row == [0xff0000, 0x0000ff], (identifier, first_row)
                    witnesses[identifier] = {'pixmap_id': row[8],
                        'width': width.value, 'height': height.value,
                        'depth': depth.value, 'first_row_pixels': first_row}
                return witnesses

            fresh_report = runtime.root / 'fresh-snapshots.txt'
            fresh_offset = 0
            fresh_enabled = False

            def snapshot(identifiers=ROLE_IDS):
                nonlocal serial, fresh_offset
                serial += 1
                if fresh_enabled:
                    started = time.monotonic()
                    begin = ('BEGIN %d\n' % serial).encode()
                    end = ('END %d\n' % serial).encode()
                    writer.send(('SendToModule ContrastSnapshot snapshot %d\n'
                        % serial).encode())
                    def fresh():
                        with fresh_report.open('rb') as report:
                            report.seek(fresh_offset)
                            data = report.read(2 * 1024 * 1024 + 1)
                        assert len(data) <= 2 * 1024 * 1024
                        if not data.endswith(end):
                            return None
                        lines = data.splitlines(keepends=True)
                        # The helper resets BEGIN at actual Colorset0. Queued
                        # broadcasts before the core's contiguous dump cannot
                        # satisfy the final ascending-ID + native END frame.
                        starts = [index for index, line in enumerate(lines)
                            if line == begin]
                        assert starts, lines
                        selected = lines[starts[-1] + 1:-1]
                        assert 0 < len(selected) <= 4096
                        rows = {}
                        previous = -1
                        for line in selected:
                            words = line.decode('ascii').split()
                            assert len(words) == 22 and words[0] == 'Colorset'
                            values = tuple(int(value, 16)
                                for value in words[1:])
                            identifier = values[0]
                            assert identifier > previous, values
                            assert previous != -1 or identifier == 0, values
                            previous = identifier
                            rows[identifier] = values
                        assert set(identifiers) <= rows.keys(), rows
                        return rows, len(data), hashlib.sha256(
                            b''.join(lines[starts[-1]:])).hexdigest()
                    rows, consumed, checksum = wait_for_predicate(
                        'fresh native colorset dump END', fresh,
                        time.monotonic() + 5, process=core)
                    evidence.setdefault('fresh_snapshots', []).append(dict(
                        serial=serial, offset=fresh_offset, bytes=consumed,
                        rows=len(rows), frame_sha256=checksum,
                        elapsed_ms=(time.monotonic() - started) * 1000))
                    fresh_offset += consumed
                    return {identifier: rows[identifier]
                        for identifier in identifiers}
                path = runtime.root / f'snapshot-{serial}.txt'
                command(f'Module {args.probe.name} colorsets {path}')
                def complete():
                    rows = {row[0]: row for row in records(path)}
                    return rows if set(identifiers) <= rows.keys() else None
                result = wait_for_predicate('complete native colorset snapshot', complete,
                    time.monotonic() + 5, process=core)
                command('KillModule ' + args.probe.name)
                return {identifier: result[identifier] for identifier in identifiers}

            def styles():
                page = rpc(other, 'menu-styles', limit=8)
                result = {item['name']: item['opacity_percent']
                    for item in page['menu_styles'] if item['name'] in
                    ('*', 'SlateLauncherStyle')}
                assert set(result) == {'*', 'SlateLauncherStyle'}, page
                return result

            updates = runtime.root / 'updates.txt'
            command(f'Module ContrastObserver colorsets {updates}')
            wait_for_predicate('initial observer role set',
                lambda: set(ROLE_IDS) <= {row[0] for row in records(updates)},
                time.monotonic() + 5, process=core)

            if args.complete_bundle or args.complete_bundle_only:
                command('Module ContrastSnapshot snapshot-colorsets ' +
                    str(fresh_report))
                wait_for_predicate('fresh snapshot helper core END readiness',
                    lambda: fresh_report.exists() and
                        fresh_report.read_bytes() == b'READY\n',
                    time.monotonic() + 5, process=core)
                fresh_offset = len(b'READY\n')
                evidence['fresh_snapshot_helper'] = dict(
                    mode='snapshot-colorsets', name='ContrastSnapshot',
                    startup='actual native M_END_CONFIG_INFO',
                    report=str(fresh_report))

            def current():
                return {row[0]: row for row in records(updates) if row[0] in ROLE_IDS}

            def publication_counts():
                counts = {identifier: 0 for identifier in ROLE_IDS}
                for row in records(updates):
                    if row[0] in counts:
                        counts[row[0]] += 1
                return counts

            def published_command(text, identifiers, predicate=lambda rows: True):
                before = publication_counts()
                command(text)
                def complete():
                    counts, rows = publication_counts(), current()
                    if not all(counts[identifier] > before[identifier]
                            for identifier in identifiers):
                        return None
                    return rows if predicate(rows) else None
                # A command echo acknowledges submission. Every affected role
                # needs its own fresh native publication and final witnesses;
                # fourteen updates to just one role cannot satisfy this wait.
                return wait_for_predicate('fresh affected-role publication', complete,
                    time.monotonic() + 10, process=core)

            def await_rows(expected):
                # A native subscriber must receive the recovery before any
                # owner query. A late query cannot manufacture expiry evidence.
                return wait_for_predicate('native complete resource publication',
                    lambda: current() == expected, time.monotonic() + 5, process=core)

            def reset():
                image.write_bytes(png())
                shape.write_text('#define shape_width 2\n#define shape_height 2\n'
                    'static unsigned char shape_bits[] = {\n 0x01, 0x02};\n')
                def normal_ready(rows):
                    return (all(rows[identifier][1:5] ==
                        (0x123456, 0x345678, 0x56789a, 0x789abc)
                        for identifier in ROLE_IDS) and
                        rows[0][8] > 1 and rows[0][9] > 1 and
                        rows[1][8] > 1 and rows[1][9] > 1)
                published_command('MenuStyle * Translucent 31\n'
                    'MenuStyle SlateLauncherStyle Translucent 42\n' +
                    'ChangeMenuStyle SlateLauncherStyle ContrastMenu\n' +
                    ''.join(f'Colorset {identifier} Plain, NoShape, NoTint, '
                        'fg #123456, bg #345678, hi #56789a, sh #789abc, '
                        'fgsh #aa55aa, fgTint #000000 0, bgTint #000000 0, '
                        'fg_alpha 100\n' for identifier in ROLE_IDS) +
                    f'Colorset 0 TiledPixmap {image}, Shape {shape}\n'
                    f'Colorset 1 TiledPixmap {image}, AspectShape {shape}',
                    ROLE_IDS, normal_ready)
                baseline = snapshot()
                backing_witnesses(baseline)
                return baseline

            def prepare(c=owner, timeout=15000, plan_version=1):
                appearance = rpc(c, 'appearance')
                capability = appearance['resource_preview']
                assert capability == dict(version=1, scope='high-contrast',
                    role_count=14, style_count=2, metadata_max=65536,
                    preparation_timeout_ms=5000, lease_limit=1, slot_available=True), capability
                if plan_version == 2:
                    assert appearance['opaque_colors_preview'] == dict(version=1,
                        plan_version=2, scope='opaque-colors', role_count=14,
                        style_count=2, metadata_max=65536,
                        preparation_timeout_ms=5000, lease_limit=1, slot_available=True)
                result = rpc(c, method='action', operation='prepare',
                    generation=appearance['generation'],
                    values=OPAQUE_PLAN if plan_version == 2 else PLAN, timeout_ms=timeout)
                assert result['scope'] == ('opaque-colors' if plan_version == 2 else 'high-contrast')
                assert result['plan_version'] == plan_version
                return result

            def state(wanted, c=owner):
                def ready():
                    value = rpc(c)
                    return value if value['state'] == wanted else None
                return wait_for_predicate('resource preview ' + wanted, ready,
                    time.monotonic() + 7, process=core)

            def action(item, operation, c=owner, error=None):
                return rpc(c, method='action', operation=operation,
                    generation=item['generation'], target=item['lease'], error=error)

            def active(timeout=15000, c=owner):
                original = snapshot()
                prepare(c, timeout)
                prepared = state('prepared', c)
                assert snapshot() == original and styles() == {'*': 31, 'SlateLauncherStyle': 42}
                result = action(prepared, 'apply', c)
                assert result['state'] in ('applying', 'active'), result
                result = state('active', c)
                candidate = snapshot()
                for identifier in ROLE_IDS:
                    row, old = candidate[identifier], original[identifier]
                    assert row[1:5] == (0xffffff, 0, 0xffffff, 0), row
                    assert row[8] == 0 and row[9] == old[9], row
                    assert row[14:17] == old[14:17] and row[10] == 100, (row, old)
                    assert row[5] == old[5] and row[7] == old[7], (row, old)
                    assert row[18:] == old[18:], (row, old)
                assert styles() == {'*': 100, 'SlateLauncherStyle': 100}
                # Recovery must follow an observed candidate publication on
                # this same subscriber, not its still-cached original rows.
                await_rows(candidate)
                return result, candidate

            def terminal(reason, recovery, c=owner):
                result = state('terminal', c)
                assert result['terminal_reason'] == reason, result
                assert result['recovery'] == recovery and result['remaining_ms'] == 0, result
                return result

            def complete_bundle_cases():
                """Version3 uses the same live core, real actor and observer."""
                nonlocal held, fresh_enabled
                fresh_enabled = True
                capabilities = rpc(owner, 'capabilities')
                assert capabilities['resource_transaction_version'] == 3
                assert capabilities['complete_bundle_preview'] is True
                evidence['v3_capabilities'] = capabilities
                ids = ROLE_IDS + (20,)
                witness = PreviewWitness(client)
                candidate_image = runtime.root / 'candidate.png'
                backer_running = False
                cases = {}
                evidence['complete_bundle'] = cases
                x.XSelectInput(d, client.root, (1 << 19) | (1 << 22))
                x.XCheckTypedWindowEvent.argtypes = [C.c_void_p, C.c_ulong,
                    C.c_int, C.POINTER(ObserverEvent)]
                x.XGetWindowAttributes.argtypes = [C.c_void_p, C.c_ulong,
                    C.POINTER(WindowAttributes)]
                x.XChangeWindowAttributes.argtypes = [C.c_void_p, C.c_ulong,
                    C.c_ulong, C.POINTER(SetAttributes)]
                x.XMoveResizeWindow.argtypes = [C.c_void_p, C.c_ulong,
                    C.c_int, C.c_int, C.c_uint, C.c_uint]

                def root_events(kind):
                    rows, event = [], ObserverEvent()
                    while x.XCheckTypedWindowEvent(d, client.root, kind,
                            C.byref(event)):
                        if kind == 28:
                            item = event.property
                            rows.append(dict(type=kind, serial=item.serial,
                                synthetic=bool(item.send_event),
                                window=item.window, atom=item.atom,
                                state=item.state))
                        elif kind == 18:
                            item = event.unmap
                            rows.append(dict(type=kind, serial=item.serial,
                                synthetic=bool(item.send_event),
                                event=item.event, window=item.window))
                        else:
                            item = event.configure
                            rows.append(dict(type=kind, serial=item.serial,
                                synthetic=bool(item.send_event),
                                event=item.event, window=item.window,
                                x=item.x, y=item.y, width=item.width,
                                height=item.height,
                                override_redirect=bool(item.override_redirect)))
                        assert len(rows) <= 4096
                    return rows

                def menu_windows():
                    root_, parent = C.c_ulong(), C.c_ulong()
                    children = C.POINTER(C.c_ulong)()
                    count = C.c_uint()
                    assert x.XQueryTree(d, client.root, C.byref(root_),
                        C.byref(parent), C.byref(children), C.byref(count))
                    try:
                        assert count.value <= 2048
                        found = []
                        for target in children[:count.value]:
                            attrs = WindowAttributes()
                            ok = errors.read(target, 3, lambda:
                                x.XGetWindowAttributes(d, target,
                                    C.byref(attrs)))
                            if ok and attrs.map_state == 2 and attrs.save_under:
                                found.append(target)
                        return found
                    finally:
                        if children:
                            x.XFree(children)

                def escape_release_wait():
                    menus = menu_windows()
                    assert menus
                    root_events(18)
                    key(0xff1b)
                    observed = []
                    def popped_down():
                        observed.extend(root_events(18))
                        return any(row['window'] in menus and
                            row['event'] == client.root and not row['synthetic']
                            for row in observed)
                    wait_for_predicate('actual menu popdown before release',
                        popped_down, time.monotonic() + 5, process=core)
                    position = pointer.position()
                    assert position[2] & (1 << 8), position
                    wait_for_predicate('actual physical release-wait grab',
                        pointer.grabbed, time.monotonic() + 5, process=core)
                    return dict(unmaps=observed, menus=menus,
                        pointer_position_and_mask=position, grabbed=True)

                def unrelated_flood():
                    atom = x.XInternAtom(d, b'_FVWM_TEST_PREVIEW_QUEUE', 0)
                    integer = x.XInternAtom(d, b'INTEGER', 0)
                    root_events(28)
                    for index in range(128):
                        value = C.c_ulong(index)
                        x.XChangeProperty(d, client.root, atom, integer, 32,
                            0, C.byref(value), 1)
                    x.XSync(d, 0)
                    events = [row for row in root_events(28)
                        if row['atom'] == atom]
                    assert len(events) == 128, events
                    assert all(not row['synthetic'] and row['window'] ==
                        client.root and row['state'] == 0 for row in events)
                    assert len({row['serial'] for row in events}) == 128
                    return dict(atom=atom, delivered=events, count=128)

                def child_configure():
                    child = x.XCreateSimpleWindow(d, client.root,
                        20, 20, 20, 20, 0, 0, 0)
                    assert child
                    try:
                        attrs = SetAttributes(override_redirect=1)
                        x.XChangeWindowAttributes(d, child, 1 << 9,
                            C.byref(attrs))
                        x.XMapWindow(d, child)
                        x.XSync(d, 0)
                        root_events(22)
                        x.XMoveResizeWindow(d, child, 35, 45, 27, 31)
                        x.XSync(d, 0)
                        events = [row for row in root_events(22)
                            if row['window'] == child]
                        assert len(events) == 1, events
                        row = events[0]
                        assert not row['synthetic'] and row['event'] == (
                            client.root) and row['window'] != client.root
                        assert row['override_redirect']
                        assert (row['x'], row['y'], row['width'],
                            row['height']) == (35, 45, 27, 31)
                        return dict(child=child, delivered=events)
                    finally:
                        x.XDestroyWindow(d, child)
                        x.XSync(d, 0)

                def bundle(live=False):
                    # Modal barriers block new Module and Echo commands. This
                    # native subscriber was running before modal entry; read
                    # only its complete actual published fifteen-role set.
                    if live:
                        rows = {row[0]: row for row in records(updates)
                            if row[0] in ids}
                        assert set(rows) == set(ids), rows
                    else:
                        rows = snapshot(ids)
                    result = dict(roles=rows, root=witness.root_state(),
                        scalars=styles())
                    result['buffered10'] = (witness.drawable(rows[10][8])
                        if rows[10][8] > 1 else None)
                    return result

                def reset_owned():
                    nonlocal backer_running
                    previous_root = witness.property('_XROOTPMAP_ID')
                    if args.plain_root:
                        published_command('MenuStyle * Translucent 31\n'
                            'MenuStyle SlateLauncherStyle Translucent 42\n' +
                            ''.join(f'Colorset {identifier} Plain, NoShape, '
                                'NoTint, fg #123456, bg #345678, '
                                'hi #56789a, sh #789abc\n'
                                for identifier in ROLE_IDS), ROLE_IDS)
                    else:
                        reset()
                    candidate_image.write_bytes(png_rgb(0, 0, 255))
                    command('Colorset 20 Plain, bg #113355' + (
                        f'\nColorset 20 AspectPixmap {image}'
                        if not args.plain_root else ''))
                    if not backer_running:
                        command('Module FvwmBacker')
                        backer_running = True
                    def owned():
                        appearance = rpc(owner, 'appearance')
                        capability = appearance['complete_bundle_preview']
                        if not capability['root_owned']:
                            return None
                        result = witness.root_state()
                        expected = 0x113355 if args.plain_root else 0xff0000
                        if (not args.plain_root and result['properties'][
                                '_XROOTPMAP_ID'] == previous_root):
                            return None
                        return result if result['image']['corner_pixels'] == (
                            [expected] * 4) else None
                    wait_for_predicate('actual core-owned normal root',
                        owned, time.monotonic() + 10, process=core)
                    published_command('Colorset 10 RootTransparent buffer, '
                        'Tint #112233 35', (10,),
                        lambda rows: rows[10][8] > 1)
                    result = bundle()
                    assert result['buffered10'] is not None
                    return result

                def plan(mode=None, transparency=True, preserve=False):
                    mode = mode or ('solid' if args.plain_root else 'png')
                    return dict(plan_version=3,
                        roles=copy.deepcopy(OPAQUE_PLAN['roles']),
                        root_transparency=transparency, root_mode=mode,
                        preserve_menu=preserve, menu_opacity=73,
                        launcher_opacity=84, root=dict(rgb='#336699',
                            path=str(candidate_image) if mode == 'png' else ''))

                def prepare_bundle(value=None, c=owner, timeout=15000):
                    selected = value if value is not None else plan()
                    if selected['root']['path']:
                        evidence.setdefault('prepared_inputs', []).append(
                            binary_hash(selected['root']['path']))
                    appearance = rpc(c, 'appearance')
                    capability = appearance['complete_bundle_preview']
                    expected = dict(version=1, scope='complete-bundle',
                        plan_version=3, role_count=14, style_count=2,
                        metadata_max=65536, preparation_timeout_ms=5000,
                        lease_limit=1, slot_available=True,
                        root_background=True, buffered_transparency=True,
                        path_max=256, token_max=701, root_owned=True)
                    assert capability == expected, capability
                    return rpc(c, method='action', operation='prepare',
                        generation=appearance['generation'], values=selected,
                        timeout_ms=timeout)

                def activate(original, value=None, c=owner, timeout=15000):
                    prepare_bundle(value, c, timeout)
                    prepared = state('prepared', c)
                    preparation = dict(prepared_reply=prepared)
                    evidence.setdefault('v3_apply_setup', []).append(
                        preparation)
                    started = time.monotonic()
                    assert bundle() == original
                    preparation['bundle_snapshot_elapsed_ms'] = (
                        time.monotonic() - started) * 1000
                    before_apply = rpc(c)
                    preparation['before_apply_reply'] = before_apply
                    assert before_apply['state'] == 'prepared', before_apply
                    assert before_apply['remaining_ms'] > 0, before_apply
                    action(prepared, 'apply', c)
                    item = state('active', c)
                    assert item['scope'] == 'complete-bundle'
                    assert item['plan_version'] == 3
                    candidate = bundle()
                    for identifier in ROLE_IDS:
                        assert candidate['roles'][identifier][1:5] == (
                            0x102030, 0xe0d0c0, 0xabcdef, 0x010203)
                    mode = (value or plan())['root_mode']
                    if mode == 'png':
                        assert candidate['root']['image']['corner_pixels'] == (
                            [0x0000ff] * 4)
                    if (value or plan())['root_transparency']:
                        assert candidate['buffered10'] is not None
                        source_color = candidate['root']['image'][
                            'corner_pixels'][0]
                        def queried_rgb(pixel):
                            color = XColor(pixel=pixel)
                            assert x.XQueryColor(d,
                                x.XDefaultColormap(d, 0), C.byref(color))
                            return (color.red, color.green, color.blue)

                        source_rgb = queried_rgb(source_color)
                        tint_rgb = queried_rgb(candidate['roles'][10][2])
                        reference = BufferedTintReference(witness,
                            config_header)
                        expected, receipt = reference.expected(source_color,
                            source_rgb, tint_rgb)
                        evidence.setdefault('buffered10_blends', []).append(
                            receipt)
                        assert candidate['buffered10']['corner_pixels'] == (
                            [expected] * 4), candidate['buffered10']
                    else:
                        assert candidate['buffered10'] is None
                    if value is not None and value['preserve_menu']:
                        assert candidate['scalars'] == original['scalars']
                    else:
                        assert candidate['scalars'] == {
                            '*': 73, 'SlateLauncherStyle': 84}
                    wait_for_predicate('v3 candidate subscriber publication',
                        lambda: {row[0]: row for row in records(updates)
                            if row[0] in ids} == candidate['roles'],
                        time.monotonic() + 7, process=core)
                    return item, candidate

                def restored(original):
                    # Observe native subscriber recovery before asking the
                    # owner for terminal state; queries cannot manufacture it.
                    try:
                        wait_for_predicate('v3 original native role publication',
                            lambda: {row[0]: row for row in records(updates)
                                if row[0] in ids} == original['roles'],
                            time.monotonic() + 7, process=core)
                    except BaseException:
                        # Diagnose only after the independent publication wait
                        # has failed. Queries cannot turn this failure into a
                        # pass or manufacture its expected recovery evidence.
                        diagnostic = dict(context=evidence.get(
                            'v3_case_context'), expected_roles=original['roles'])
                        evidence['v3_recovery_failure'] = diagnostic
                        queries = (
                            ('live_roles', lambda: {row[0]: row for row in
                                records(updates) if row[0] in ids}),
                            ('root', lambda: witness.root_state(owned=False)),
                            ('pointer_grabbed', pointer.grabbed),
                            ('preview', lambda: rpc(owner if owner in
                                connections else other)),
                            ('appearance', lambda: rpc(other, 'appearance')))
                        for label, query in queries:
                            try:
                                diagnostic[label] = query()
                            except BaseException as failure:
                                diagnostic[label] = dict(
                                    diagnostic_error=repr(failure))
                        raise
                    assert bundle() == original

                def external_conflict(external):
                    evidence['v3_case_context'] = dict(
                        group='external-command-conflict', command=external)
                    original = reset_owned()
                    item, candidate = activate(original)
                    if external.startswith('Colorset'):
                        published_command(external, (14,),
                            lambda rows: rows[14][2] == 0x135724)
                    else:
                        command(external)
                    after = bundle()
                    result = terminal('conflicted', 'refused-conflict')
                    assert bundle() == after
                    action(item, 'revert', error='preview_conflict')
                    cases['external-' + external] = dict(
                        preserved=after, terminal=result)

                def controller_actor_loss():
                    evidence['v3_case_context'] = dict(
                        group='nonmodal-controller-actor-loss')
                    original = reset_owned()
                    image.unlink(missing_ok=True)
                    actor = connect()
                    activate(original, c=actor)
                    actor.close()
                    connections.remove(actor)
                    restored(original)
                    assert rpc(other)['state'] == 'inactive'
                    cases['actual-controller-actor-loss'] = 'passed'
                    evidence['owned_root_before_shutdown'] = witness.root_state()

                def external_root_conflict():
                    evidence['v3_case_context'] = dict(
                        group='external-root-conflict-preservation')
                    # External authority is terminal for this core incarnation.
                    # No later reset/selector may reclaim it or adopt its XID.
                    original = reset_owned()
                    item, candidate = activate(original)
                    external = publish_root(0x00cc33)
                    terminal('conflicted', 'refused-conflict')
                    assert witness.property('_XROOTPMAP_ID') == external
                    assert witness.drawable(external)['corner_pixels'] == (
                        [0x00cc33] * 4)
                    external_rows = snapshot(ids)
                    assert all(external_rows[identifier] == (
                        candidate['roles'][identifier])
                        for identifier in ids if identifier != 10)
                    action(item, 'revert', error='preview_conflict')
                    external_state = witness.root_state(owned=False)
                    cases['external-root-conflict-preserves-external'] = dict(
                        root=external_state, roles=external_rows,
                        buffered10=(witness.drawable(external_rows[10][8])
                            if external_rows[10][8] > 1 else None))
                    evidence['external_root_before_shutdown'] = external_state

                if args.complete_bundle_tail_only:
                    evidence['complete_bundle_scope'] = 'remaining-tail-only'
                    evidence['v3_skipped_prior_groups'] = [
                        'reverted-deleted-source', 'expired-deleted-source',
                        'confirmed-deleted-source', 'root-mode-retain',
                        'root-mode-solid', 'root-mode-png',
                        'root-transparency-False', 'root-transparency-True',
                        'actual-worker-decode-failure',
                        'queued-apply-notification-flood',
                        'nested-menu-revert', 'nested-menu-confirm',
                        'nested-menu-expiry', 'nested-menu-owner-loss',
                        'actual-nested-Wait-active-expiry',
                        'actual-nested-Wait-preparing-expiry',
                        'actual-Backer-loss-retains-core-root',
                        'external-Colorset 14 bg #135724',
                        'external-MenuStyle * Translucent 64']
                    evidence['group_results']['v3'] = (
                        'skipped full group by --complete-bundle-tail-only')
                    evidence['v3_skipped_optional_groups'] = [
                        'actual-color-allocation-failure',
                        'actual-topology-change', 'plain-normal-root']
                    evidence['group_results']['v3-tail'] = 'pending'
                    external_conflict(
                        'MenuStyle SlateLauncherStyle Translucent 57')
                    controller_actor_loss()
                    external_root_conflict()
                    return

                if args.owned_root_shutdown_only:
                    original = reset_owned()
                    evidence['owned_root_before_shutdown'] = original['root']
                    evidence['group_results']['v3'] = (
                        'skipped by --owned-root-shutdown-only')
                    evidence['group_results']['owned-root-shutdown'] = 'pending'
                    # Quit has no echo. Actual process Wait and matching root
                    # property removal below establish completed shutdown.
                    writer.send(b'Quit\n')
                    core.process.wait(timeout=5)
                    evidence['owned_root_quit_returncode'] = (
                        core.process.returncode)
                    assert core.process.returncode == 0
                    return

                for ending in ('reverted', 'expired', 'confirmed'):
                    evidence['v3_case_context'] = dict(
                        group='deleted-source', ending=ending)
                    original = reset_owned()
                    # Full 1920px root/reference captures under TCG can exceed
                    # one second before setup finishes; preserve lease margin.
                    item, candidate = activate(original,
                        timeout=5000 if ending == 'expired' else 15000)
                    # Both old and candidate files disappear before recovery.
                    # Pixel/XID equality must come from retained resources.
                    image.unlink(missing_ok=True)
                    candidate_image.unlink()
                    if ending == 'confirmed':
                        action(item, 'confirm')
                        result = terminal('confirmed', 'kept')
                        assert bundle() == candidate
                    else:
                        if ending == 'reverted':
                            action(item, 'revert')
                        restored(original)
                        result = terminal(ending, 'restored')
                    cases[ending + '-deleted-source'] = dict(
                        original=original, candidate=candidate, result=result)

                for mode in (('retain', 'solid') if args.plain_root else (
                        'retain', 'solid', 'png')):
                    original = reset_owned()
                    item, candidate = activate(original,
                        plan(mode, preserve=True))
                    if mode == 'retain':
                        assert candidate['root'] == original['root']
                    elif mode == 'solid':
                        assert candidate['root']['image']['corner_pixels'] == (
                            [0x336699] * 4)
                    action(item, 'revert')
                    restored(original)
                    terminal('reverted', 'restored')
                    cases['root-mode-' + mode] = dict(
                        original=original, candidate=candidate)
                for transparency in (False, True):
                    original = reset_owned()
                    item, candidate = activate(original,
                        plan(transparency=transparency))
                    action(item, 'revert')
                    restored(original)
                    terminal('reverted', 'restored')
                    cases['root-transparency-' + str(transparency)] = dict(
                        original=original, candidate=candidate)

                original = reset_owned()
                candidate_image.write_bytes(b'invalid PNG; worker refuses')
                prepare_bundle(plan('png'))
                result = terminal('refused', 'not-applied')
                assert result['preparation_error'] == 'preview_image_failed'
                assert bundle() == original
                cases['actual-worker-decode-failure'] = result
                if args.allocation_failure:
                    original = reset_owned()
                    fault_line = 'contrast allocation failure at 1'
                    prior_faults = (runtime.root / 'core.log').read_text().count(
                        fault_line)
                    allocation_marker.write_text('1\n')
                    try:
                        prepare_bundle()
                        result = terminal('refused', 'not-applied')
                        assert result['preparation_error'] == (
                            'color_allocation_failed'), result
                        actual_faults = (runtime.root / 'core.log').read_text().count(
                            fault_line)
                        assert actual_faults > prior_faults, (
                            'actual allocation wrapper did not fire')
                        assert bundle() == original
                        cases['actual-color-allocation-failure'] = dict(
                            result=result, fault_witness=fault_line,
                            prior_occurrences=prior_faults,
                            observed_occurrences=actual_faults)
                    finally:
                        allocation_marker.unlink(missing_ok=True)
                else:
                    cases['actual-color-allocation-failure'] = dict(
                        status='skipped',
                        reason='requires --allocation-failure')

                evidence['v3_case_context'] = dict(
                    group='queued-apply', ending='revert')
                original = reset_owned()
                prepare_bundle()
                prepared = state('prepared')
                wait_for_predicate('queued apply original subscriber set',
                    lambda: {row[0]: row for row in records(updates)
                        if row[0] in ids} == original['roles'],
                    time.monotonic() + 7, process=core)
                pointer.motion(500, 400)
                pointer.button(True)
                held = True
                wait_for_predicate('queued apply actual native menu',
                    pointer.grabbed, time.monotonic() + 5, process=core)
                applying = action(prepared, 'apply')
                assert applying['state'] == 'applying', applying
                assert bundle(live=True) == original
                release_wait = escape_release_wait()
                notifications = unrelated_flood()
                assert bundle(live=True) == original
                pointer.button(False)
                held = False
                def candidate_published():
                    rows = {row[0]: row for row in records(updates)
                        if row[0] in ids}
                    if set(rows) != set(ids) or not all(rows[identifier][
                            1:5] == (0x102030, 0xe0d0c0, 0xabcdef, 0x010203)
                            for identifier in ROLE_IDS):
                        return None
                    root = witness.root_state()
                    color = 0x336699 if args.plain_root else 0x0000ff
                    if (root == original['root'] or root['image'][
                            'corner_pixels'] != [color] * 4):
                        return None
                    return rows
                # No owner query can manufacture this required publication.
                wait_for_predicate('queued apply actual candidate publication',
                    candidate_published, time.monotonic() + 7, process=core)
                item = state('active')
                candidate = bundle(live=True)
                assert candidate['scalars'] == {
                    '*': 73, 'SlateLauncherStyle': 84}
                action(item, 'revert')
                restored(original)
                result = terminal('reverted', 'restored')
                cases['queued-apply-notification-flood'] = dict(
                    status='passed', release_wait=release_wait,
                    queued_notifications=notifications, original=original,
                    candidate=candidate, terminal=result)

                for ending in ('revert', 'confirm', 'expiry', 'owner-loss'):
                    evidence['v3_case_context'] = dict(
                        group='nested-menu', ending=ending)
                    menu_case = {}
                    cases['nested-menu-' + ending] = menu_case
                    original = reset_owned()
                    actor = connect() if ending == 'owner-loss' else owner
                    item, candidate = activate(original, c=actor,
                        timeout=5000 if ending == 'expiry' else 15000)
                    pointer.motion(500, 400)
                    pointer.button(True)
                    held = True
                    wait_for_predicate('v3 actual held native menu',
                        pointer.grabbed, time.monotonic() + 5, process=core)
                    if ending == 'owner-loss':
                        actor.close()
                        connections.remove(actor)
                    elif ending in ('revert', 'confirm'):
                        action(item, ending)
                    else:
                        entered = rpc(owner)
                        menu_case['actual_menu_entry'] = entered
                        assert entered['state'] == 'active', entered
                        assert entered['remaining_ms'] > 0, entered
                        time.sleep(entered['remaining_ms'] / 1000 + .15)
                    assert bundle(live=True) == candidate
                    if ending != 'owner-loss':
                        pending = rpc(owner)
                        assert pending['state'] == 'recovery-pending'
                        assert pending['recovery'] == 'pending'
                    release_wait = escape_release_wait()
                    notifications = (unrelated_flood() if ending in (
                        'revert', 'confirm') else child_configure() if ending ==
                        'owner-loss' else None)
                    assert bundle(live=True) == candidate
                    pointer.button(False)
                    held = False
                    if ending == 'confirm':
                        terminal('confirmed', 'kept')
                        assert bundle(live=True) == candidate
                    else:
                        restored(original)
                    if ending not in ('owner-loss', 'confirm'):
                        terminal('expired' if ending == 'expiry' else (
                            'reverted'), 'restored')
                    menu_case.update(status='passed',
                        release_wait=release_wait,
                        queued_notifications=notifications)

                def wait_entries():
                    return sum('CMD_Echo:' in line and
                        line.endswith(' contrast-wait-entered') for line in
                        (runtime.root / 'debug.log').read_text().splitlines())
                for phase in ('active', 'preparing'):
                    evidence['v3_case_context'] = dict(
                        group='nested-Wait', phase=phase, ending='expiry')
                    wait_case = {}
                    cases['actual-nested-Wait-' + phase + '-expiry'] = wait_case
                    original = reset_owned()
                    if phase == 'active':
                        item, candidate = activate(original, timeout=5000)
                    entries = wait_entries()
                    writer.send(b'Schedule 0 ContrastWait\n')
                    wait_for_predicate('v3 actual core Wait entered',
                        lambda: wait_entries() > entries,
                        time.monotonic() + 5, process=core)
                    if phase == 'active':
                        entered = rpc(owner)
                        wait_case['actual_wait_entry'] = entered
                        assert entered['state'] == 'active', entered
                        assert entered['remaining_ms'] > 0, entered
                        delay = entered['remaining_ms'] / 1000 + .15
                    else:
                        item = prepare_bundle()
                        wait_case['preparing_reply'] = item
                        assert item['state'] == 'preparing'
                        delay = 5.15
                    time.sleep(delay)
                    pending = rpc(owner)
                    wait_case['pending'] = pending
                    assert pending['state'] == 'recovery-pending'
                    assert pending['recovery'] == 'pending'
                    assert bundle(live=True) == (original if phase == 'preparing'
                        else candidate)
                    wait_child = Client(display)
                    try:
                        wait_child.window = wait_child.x.XCreateSimpleWindow(
                            wait_child.display, wait_child.root,
                            200, 150, 80, 50, 0, 0, 0)
                        wait_child.x.XStoreName(wait_child.display,
                            wait_child.window, b'ContrastWaitWitness')
                        wait_child.x.XMapWindow(wait_child.display,
                            wait_child.window)
                        wait_child.x.XSync(wait_child.display, 0)
                        wait_for_predicate('v3 matching Wait client managed',
                            wait_child.frame, time.monotonic() + 5,
                            process=core)
                        restored(original)
                        result = terminal('expired', 'not-applied' if phase ==
                            'preparing' else 'restored')
                    finally:
                        wait_child.close()
                    wait_case['terminal'] = result

                original = reset_owned()
                item, candidate = activate(original)
                command('KillModule FvwmBacker')
                backer_running = False
                backer_records = [json.loads(path.read_text()) for path in
                    native.glob('module-*.json') if json.loads(
                        path.read_text())['real'] == str(args.backer)]
                backer_record = max(backer_records,
                    key=lambda row: row['started'])
                stopped_path = native / (
                    'stopped-%d.json' % backer_record['proxy'])
                stopped = wait_for_predicate('actual Backer Wait receipt',
                    lambda: json.loads(stopped_path.read_text())
                        if stopped_path.exists() else None,
                    time.monotonic() + 6, process=core)
                assert stopped['child'] == backer_record['child']
                assert stopped['returncode'] is not None
                assert bundle() == candidate
                action(item, 'revert')
                restored(original)
                terminal('reverted', 'restored')
                cases['actual-Backer-loss-retains-core-root'] = stopped

                for external in ('Colorset 14 bg #135724',
                        'MenuStyle * Translucent 64',
                        'MenuStyle SlateLauncherStyle Translucent 57'):
                    external_conflict(external)

                controller_actor_loss()
                cases['plain-normal-root'] = dict(
                    status='passed' if args.plain_root else 'skipped',
                    reason='solid setup; build feature profile is separate')
                cases['actual-topology-change'] = dict(status='skipped',
                    reason='requires explicit --topology-change receipt')
                if args.topology_change:
                    operations = json.loads(args.topology_change.read_text())
                    assert set(operations) == {'change', 'restore'}
                    assert all(isinstance(operations[key], list) and
                        0 < len(operations[key]) <= 16 and all(
                            isinstance(part, str) and len(part) <= 1024
                            for part in operations[key])
                        for key in operations)
                    original = reset_owned()
                    def topology():
                        observed = {}
                        for option in ('--current', '--listactivemonitors'):
                            result = subprocess.run(['xrandr', '--display',
                                display, option], capture_output=True,
                                text=True, timeout=5)
                            assert result.returncode == 0, result.stderr
                            observed[option] = result.stdout
                        geometry = witness.drawable(client.root)
                        observed['root_geometry'] = {key: geometry[key]
                            for key in ('width', 'height', 'depth')}
                        return observed
                    before_topology = topology()
                    prepare_bundle()
                    prepared = state('prepared')
                    receipts = []
                    try:
                        argv = [part.replace('{display}', display)
                            for part in operations['change']]
                        result = subprocess.run(argv,
                            env=runtime.environment({'DISPLAY': display}),
                            capture_output=True, text=True, timeout=10)
                        receipts.append(dict(argv=argv,
                            returncode=result.returncode,
                            stdout=result.stdout, stderr=result.stderr))
                        assert result.returncode == 0, receipts
                        after_topology = topology()
                        assert after_topology != before_topology, (
                            'command did not change actual server topology')
                        result_state = state('terminal')
                        assert result_state['terminal_reason'] == 'conflicted'
                        assert result_state['recovery'] == 'refused-conflict'
                        action(prepared, 'apply', error='preview_conflict')
                    finally:
                        restore_argv = [part.replace('{display}', display)
                            for part in operations['restore']]
                        result = subprocess.run(restore_argv,
                            env=runtime.environment({'DISPLAY': display}),
                            capture_output=True, text=True, timeout=10)
                        receipts.append(dict(argv=restore_argv,
                            returncode=result.returncode,
                            stdout=result.stdout, stderr=result.stderr))
                        assert result.returncode == 0, receipts
                        restored_topology = topology()
                        assert restored_topology == before_topology
                    cases['actual-topology-change'] = dict(
                        status='passed', commands=receipts,
                        terminal=result_state,
                        before=before_topology, after=after_topology,
                        restored=restored_topology)
                external_root_conflict()
                evidence['complete_bundle'] = cases
                evidence['group_results']['v3'] = 'passed'

            if args.complete_bundle_only:
                complete_bundle_cases()
                if not args.owned_root_shutdown_only:
                    assert core.poll() is None
                assert not errors.unexpected
                assert evidence['source_sha256'] == hashes(source, inputs)
                assert all(binary_hash(item['path']) == item for item in
                    evidence['binary_inputs'].values())
                evidence['status'] = 'passed'
                return

            original = reset()
            stale = rpc(owner, 'appearance')['generation']
            command('Colorset 14 bg #345678')
            rpc(owner, method='action', operation='prepare', generation=stale,
                values=PLAN, error='stale_generation')
            for change in ('missing', 'duplicate', 'unknown', 'color', 'extra'):
                invalid = copy.deepcopy(PLAN)
                if change == 'missing':
                    invalid['roles'].pop()
                elif change == 'duplicate':
                    invalid['roles'][1]['id'] = 0
                elif change == 'unknown':
                    invalid['roles'][-1]['id'] = 9
                elif change == 'color':
                    invalid['roles'][0]['background'] = '#000000;Quit'
                else:
                    invalid['roles'][0]['command'] = 'Quit'
                rpc(owner, method='action', operation='prepare',
                    generation=rpc(owner, 'appearance')['generation'],
                    values=invalid, error='invalid_request')
            assert snapshot() == original and styles() == {'*': 31, 'SlateLauncherStyle': 42}
            evidence['cases']['typed-plan-and-stale-refusal-no-publication'] = 'passed'

            original = reset()
            prepare()
            prepared = state('prepared')
            action(prepared, 'revert')
            terminal('reverted', 'not-applied')
            assert snapshot() == original and styles() == {'*': 31, 'SlateLauncherStyle': 42}
            prepare(timeout=3000)
            prepared = state('prepared')
            time.sleep(1.1)
            action(prepared, 'apply')
            published = state('active')
            assert published['remaining_ms'] >= 2500, published
            action(published, 'revert')
            await_rows(original)
            terminal('reverted', 'restored')
            evidence['cases']['cancel-before-apply-and-active-clock-starts-at-publication'] = 'passed'

            original = reset()
            prepare()
            prepared = state('prepared')
            published_command('Colorset 14 bg #135724', (14,),
                lambda rows: rows[14][2] == 0x135724)
            external = snapshot()
            terminal('conflicted', 'refused-conflict')
            assert snapshot() == external and styles() == {'*': 31, 'SlateLauncherStyle': 42}
            action(prepared, 'apply', error='preview_conflict')
            evidence['cases']['prepared-generation-conflict-no-apply'] = 'passed'

            for ending in ('reverted', 'expired'):
                original = reset()
                witnesses = backing_witnesses(original)
                item, candidate = active(1000 if ending == 'expired' else 15000)
                # Replace both source contents, then delete them. Exact XIDs,
                # full metadata and actual server pixels must survive recovery.
                image.write_bytes(b'changed source; never reload this')
                shape.write_text('changed shape; never reload this')
                image.unlink()
                shape.unlink()
                assert backing_witnesses(original) == witnesses
                if ending == 'reverted':
                    action(item, 'revert')
                await_rows(original)
                result = terminal(ending, 'restored')
                assert snapshot() == original
                assert backing_witnesses(original) == witnesses
                assert styles() == {'*': 31, 'SlateLauncherStyle': 42}
                evidence['cases'][ending + '-actual-backing-after-source-deletion'] = {
                    'result': result, 'original': original, 'candidate': candidate,
                    'server_pixels': witnesses}

            original = reset()
            prepare()
            prepared = state('prepared')
            assert snapshot() == original
            time.sleep(5.15)
            assert current() == original
            terminal('expired', 'not-applied')
            action(prepared, 'apply', error='preview_finished')
            evidence['cases']['prepared-deadline-never-publishes'] = 'passed'

            for policy in ('Transparent', 'fgTint red 50', 'bgTint red 50', 'fg_alpha 70'):
                reset()
                published_command('Colorset 14 ' + policy, (14,))
                before, previous_styles = snapshot(), styles()
                prepare()
                result = terminal('refused', 'not-applied')
                assert result['preparation_error'] == 'preview_resources_unavailable', result
                assert snapshot() == before and styles() == previous_styles
                evidence['cases']['refusal-' + policy] = result
            reset()
            command('DestroyMenuStyle SlateLauncherStyle')
            before = snapshot()
            prepare()
            result = terminal('refused', 'not-applied')
            assert result['preparation_error'] == 'preview_menu_unavailable', result
            assert snapshot() == before
            evidence['cases']['missing-menu-style-no-partial-publication'] = result

            if args.allocation_failure:
                failures = {}
                for allocation in (1, 17, 56):
                    reset()
                    before, previous_styles = snapshot(), styles()
                    allocation_marker.write_text(str(allocation) + '\n')
                    try:
                        prepare()
                        result = terminal('refused', 'not-applied')
                        assert result['preparation_error'] == 'color_allocation_failed', result
                        assert snapshot() == before and styles() == previous_styles
                        # Distinguish an actually armed wrapper from syntax
                        # rejection or an unrelated resource refusal.
                        assert ('contrast allocation failure at ' + str(allocation)) in (
                            runtime.root / 'core.log').read_text()
                    finally:
                        allocation_marker.unlink()
                    failures[allocation] = result
                evidence['cases']['real-color-allocation-failure-no-publication'] = failures
            else:
                evidence['cases']['real-color-allocation-failure-no-publication'] = {
                    'status': 'skipped', 'reason': 'Requires separately linked --allocation-failure core.'}

            if args.metadata_limit:
                reset()
                # The superseded filename control failed before preview in
                # retained resource-x05: PeekToken's static 1023-byte buffer
                # copies at most 1022 filename bytes. A 3.7 KiB source path
                # cannot establish a valid loaded resource through that route.
                evidence['cases']['metadata-filename-setup'] = {
                    'status': 'unavailable', 'current_execution': 'not-attempted',
                    'retained_failure': 'resource-x05: truncated filename could not load picture',
                    'token_storage_bytes': 1023, 'filename_max_copied_bytes': 1022,
                    'source': ['libs/Parse.h:MAX_TOKEN_LENGTH',
                        'libs/Parse.c:CopyToken/DoPeekToken',
                        'fvwm/colorset.c:Pixmap PeekToken']}
                # Gradient args retain internal whitespace in an owned dynamic
                # string. Fourteen valid two-color gradients plus the fixed
                # 16 KiB reservation exceed the cap without long filenames,
                # extra colors or bypassing ordinary resource preparation.
                gradient_args = '2 red' + ' ' * 4096 + 'blue'
                metadata_minimum = (len(gradient_args.encode()) + 1) * len(ROLE_IDS) + 16384
                assert metadata_minimum > 65536
                bulk = runtime.root / 'metadata-gradients.fvwm'
                bulk.write_text(''.join(f'Colorset {identifier} NoDither, HGradient {gradient_args}\n'
                    for identifier in ROLE_IDS))
                shutil.copy2(bulk, args.artifacts_dir / bulk.name)
                published_command('Read ' + str(bulk), ROLE_IDS,
                    lambda rows: all(row[8] > 1 and row[13] == 1
                        for row in rows.values()))
                before, previous_styles = snapshot(), styles()
                witnesses = gradient_witnesses(before)
                for identifier in ROLE_IDS:
                    observed = rpc(other, 'colorsets', target=str(identifier))
                    assert observed['colorsets'][0]['id'] == identifier, observed
                    assert observed['colorsets'][0]['update_state'] == 'idle', observed
                prepare()
                result = terminal('refused', 'not-applied')
                assert result['preparation_error'] == 'preview_metadata_limit', result
                assert snapshot() == before and styles() == previous_styles
                assert gradient_witnesses(before) == witnesses
                evidence['cases']['metadata-overflow-no-publication'] = {
                    'result': result, 'copied_metadata_minimum_bytes': metadata_minimum,
                    'gradient_args_bytes_per_role': len(gradient_args.encode()) + 1,
                    'actual_gradient_witnesses': witnesses,
                    'configuration_sha256': hashlib.sha256(bulk.read_bytes()).hexdigest()}
            else:
                evidence['cases']['metadata-overflow-no-publication'] = {
                    'status': 'skipped', 'reason': 'Opt in with --metadata-limit for fourteen real gradients.'}

            for external in ('Colorset 14 bg #135724',
                    'MenuStyle SlateLauncherStyle Translucent 57',
                    'MenuStyle * Translucent 64'):
                reset()
                item, candidate = active()
                if external.startswith('Colorset '):
                    published_command(external, (14,),
                        lambda rows: rows[14][2] == 0x135724)
                else:
                    command(external)
                external_rows, external_styles = snapshot(), styles()
                result = terminal('conflicted', 'refused-conflict')
                assert snapshot() == external_rows and styles() == external_styles
                action(item, 'revert', error='preview_conflict')
                evidence['cases']['preserve-external-' + external] = result

            reset()
            publish_root(0x223344)
            published_command('Colorset 10 RootTransparent buffer, Tint #112233 35',
                (10,), lambda rows: rows[10][8] > 1)
            buffered = snapshot()
            assert buffered[10][8] > 1
            item, candidate = active()
            external_root = publish_root()
            result = terminal('conflicted', 'refused-conflict')
            assert snapshot() == candidate and styles() == {'*': 100, 'SlateLauncherStyle': 100}
            assert root_property() == external_root
            assert pixels(external_root) == [0x00cc33] * 4
            action(item, 'revert', error='preview_conflict')
            evidence['cases']['external-root-property-conflict-preserves-external-owner'] = result
            reset()
            published_command('Colorset 14 RootTransparent', (14,),
                lambda rows: rows[14][8] == external_root)
            before = snapshot()
            assert before[14][8] == external_root, before[14]
            prepare()
            result = terminal('refused', 'not-applied')
            assert result['preparation_error'] == 'preview_resources_unavailable', result
            assert snapshot() == before and root_property() == external_root
            evidence['cases']['raw-root-pixmap-not-adopted'] = result

            for operation in ('expiry', 'revert', 'confirm', 'confirm-expiry'):
                original = reset()
                item, candidate = active(1000 if operation in ('expiry', 'confirm-expiry') else 15000)
                pointer.motion(500, 400)
                pointer.button(True)
                held = True
                wait_for_predicate('real held-button menu', pointer.grabbed,
                    time.monotonic() + 5, process=core)
                if operation == 'expiry':
                    time.sleep(1.15)
                    pending = rpc(owner)
                else:
                    pending = action(item, 'confirm' if operation == 'confirm-expiry' else operation)
                    if operation == 'confirm-expiry':
                        time.sleep(1.15)
                        pending = rpc(owner)
                assert pending['state'] == 'recovery-pending', pending
                assert pending['recovery'] == 'pending', pending
                assert current() == candidate and styles() == {'*': 100, 'SlateLauncherStyle': 100}
                assert pixels(original[0][8]) == [0xff0000] * 4
                key(0xff1b)
                # Escape may leave the native physical-release wait active.
                # Keep the real button held while checking no early restore.
                assert current() == candidate
                pointer.button(False)
                held = False
                wait_for_predicate('menu physical release', lambda: not pointer.grabbed(),
                    time.monotonic() + 5, process=core)
                if operation == 'confirm':
                    result = terminal('confirmed', 'kept')
                    assert snapshot() == candidate and styles() == {'*': 100, 'SlateLauncherStyle': 100}
                else:
                    await_rows(original)
                    result = terminal('expired' if operation in ('expiry', 'confirm-expiry') else 'reverted', 'restored')
                evidence['cases']['nested-menu-' + operation] = {'pending': pending, 'terminal': result}

            for phase in ('preparing', 'active'):
                original = reset()
                if phase == 'active':
                    item, candidate = active(1000)
                def wait_entries():
                    return sum('CMD_Echo:' in line and
                        line.endswith(' contrast-wait-entered') for line in
                        (runtime.root / 'debug.log').read_text().splitlines())
                before = wait_entries()
                writer.send(b'Schedule 0 ContrastWait\n')
                wait_for_predicate('actual nested core Wait entered',
                    lambda: wait_entries() > before,
                    time.monotonic() + 5, process=core)
                if phase == 'preparing':
                    item = prepare()
                    assert item['state'] == 'preparing', item
                time.sleep(5.15 if phase == 'preparing' else 1.15)
                expected = original if phase == 'preparing' else candidate
                assert current() == expected
                pending = rpc(owner)
                assert pending['state'] == 'recovery-pending' and pending['recovery'] == 'pending', pending
                # Own and map a real matching child after naming it. Mapping
                # the generic Client first would race Wait's MapNotify match.
                child = Client(display)
                try:
                    child.window = child.x.XCreateSimpleWindow(child.display,
                        child.root, 200, 150, 80, 50, 0, 0, 0)
                    assert child.window
                    child.x.XStoreName(child.display, child.window, b'ContrastWaitWitness')
                    child.x.XMapWindow(child.display, child.window)
                    child.x.XSync(child.display, 0)
                    wait_for_predicate('actual Wait matching child managed', child.frame,
                        time.monotonic() + 5, process=core)
                    await_rows(original)
                    result = terminal('expired', 'not-applied' if phase == 'preparing' else 'restored')
                finally:
                    child.close()
                evidence['cases']['native-Wait-' + phase + '-expiry'] = {
                    'pending': pending, 'terminal': result, 'matching_child_owned': True}

            if args.opaque_colors:
                opaque_cases = {}
                unequal = {'*': 31, 'SlateLauncherStyle': 42}

                def opaque_active(c=owner, timeout=15000):
                    original = snapshot()
                    assert styles() == unequal
                    prepare(c, timeout, plan_version=2)
                    prepared = state('prepared', c)
                    assert snapshot() == original and styles() == unequal
                    action(prepared, 'apply', c)
                    item = state('active', c)
                    assert item['scope'] == 'opaque-colors' and item['plan_version'] == 2
                    candidate = snapshot()
                    for identifier in ROLE_IDS:
                        row, old = candidate[identifier], original[identifier]
                        assert row[1:5] == (0x102030, 0xe0d0c0, 0xabcdef, 0x010203), row
                        assert row[8] == 0 and row[9] == old[9], (row, old)
                        assert row[14:17] == old[14:17] and row[10] == 100
                        assert row[5] == old[5] and row[7] == old[7]
                        assert row[18:] == old[18:]
                    assert styles() == unequal
                    await_rows(candidate)
                    return item, candidate

                original = reset()
                stale = rpc(owner, 'appearance')['generation']
                published_command('Colorset 14 bg #135724', (14,),
                    lambda rows: rows[14][2] == 0x135724)
                rpc(owner, method='action', operation='prepare', generation=stale,
                    values=OPAQUE_PLAN, error='stale_generation')
                original = snapshot()
                for malformed in ('version-type', 'unknown-version', 'role-count', 'role-order', 'color'):
                    invalid = copy.deepcopy(OPAQUE_PLAN)
                    if malformed == 'version-type':
                        invalid['plan_version'] = '2'
                    elif malformed == 'unknown-version':
                        invalid['plan_version'] = 3
                    elif malformed == 'role-count':
                        invalid['roles'].pop()
                    elif malformed == 'role-order':
                        invalid['roles'][0], invalid['roles'][1] = invalid['roles'][1], invalid['roles'][0]
                    else:
                        invalid['roles'][0]['foreground'] = '#102030;Quit'
                    rpc(owner, method='action', operation='prepare',
                        generation=rpc(owner, 'appearance')['generation'],
                        values=invalid, error='invalid_request')
                assert snapshot() == original and styles() == unequal
                opaque_cases['typed-stale-refusal-without-publication'] = 'passed'

                for ending in ('reverted', 'expired', 'confirmed'):
                    original = reset()
                    backing = backing_witnesses(original)
                    item, candidate = opaque_active(timeout=3000 if ending == 'expired' else 15000)
                    for capability in ('resource_preview', 'opaque_colors_preview'):
                        assert not rpc(other, 'appearance')[capability]['slot_available']
                    for plan in (PLAN, OPAQUE_PLAN):
                        rpc(other, method='action', operation='prepare',
                            generation=rpc(other, 'appearance')['generation'],
                            values=plan, error='preview_busy')
                    rpc(other, method='action', operation='start', target='SlateLauncherStyle',
                        generation=rpc(other, 'appearance')['generation'],
                        values={'opacity': 70}, error='preview_busy')
                    action(item, 'confirm', other, error='preview_unavailable')
                    if ending == 'confirmed':
                        action(item, 'confirm')
                        result = terminal('confirmed', 'kept')
                        assert snapshot() == candidate
                    else:
                        image.unlink()
                        shape.unlink()
                        if ending == 'reverted':
                            action(item, 'revert')
                        await_rows(original)
                        result = terminal(ending, 'restored')
                        assert snapshot() == original and backing_witnesses(original) == backing
                    assert styles() == unequal
                    assert result['scope'] == 'opaque-colors' and result['plan_version'] == 2
                    opaque_cases[ending] = {'result': result,
                        'unchanged_unequal_scalars': styles(), 'original': original,
                        'candidate': candidate}

                original = reset()
                departed = connect()
                opaque_active(c=departed)
                departed.close()
                connections.remove(departed)
                await_rows(original)
                assert snapshot() == original and styles() == unequal
                assert rpc(other)['state'] == 'inactive'
                opaque_cases['owner-loss-actual-resource-restoration'] = 'passed'
                # The inverse slot direction must also reject version2, while
                # version1 retains its established force-100 scalar behavior.
                original = reset()
                version_one, _ = active()
                rpc(other, method='action', operation='prepare',
                    generation=rpc(other, 'appearance')['generation'],
                    values=OPAQUE_PLAN, error='preview_busy')
                action(version_one, 'revert')
                await_rows(original)
                terminal('reverted', 'restored')
                assert styles() == unequal
                opaque_cases['version-one-exclusion-and-force-100-unchanged'] = 'passed'
                evidence['opaque_colors'] = opaque_cases
                evidence['group_results']['v2'] = 'passed'

            original = reset()
            abandoned = connect()
            item, _ = active(c=abandoned)
            assert rpc(other)['state'] == 'inactive'
            action(item, 'confirm', other, error='preview_unavailable')
            appearance = rpc(other, 'appearance')
            assert not appearance['resource_preview']['slot_available']
            rpc(other, method='action', operation='start', target='SlateLauncherStyle',
                generation=appearance['generation'], values={'opacity': 70}, error='preview_busy')
            abandoned.close()
            connections.remove(abandoned)
            await_rows(original)
            assert rpc(other)['state'] == 'inactive'
            evidence['cases']['actor-isolation-scalar-exclusion-disconnect'] = 'passed'

            original = reset()
            departed = connect()
            _, candidate = active(c=departed)
            pointer.motion(500, 400)
            pointer.button(True)
            held = True
            wait_for_predicate('owner-loss real held menu', pointer.grabbed,
                time.monotonic() + 5, process=core)
            departed.close()
            connections.remove(departed)
            time.sleep(.15)
            assert current() == candidate
            assert not rpc(other, 'appearance')['resource_preview']['slot_available']
            key(0xff1b)
            pointer.button(False)
            held = False
            await_rows(original)
            assert rpc(other)['state'] == 'inactive'
            assert rpc(other, 'appearance')['resource_preview']['slot_available']
            evidence['cases']['owner-loss-detached-during-menu-outer-recovery'] = 'passed'

            original = reset()
            active()
            writer.send(b'Schedule 500 Module FvwmMFL\nKillModule FvwmMFL\n')
            for connection in connections:
                connection.close()
            connections.clear()
            # Observe recovery through the independent native subscriber before
            # connecting to the replacement producer or querying any lease.
            await_rows(original)
            time.sleep(.7)
            wait_for_predicate('replacement MFL endpoint', endpoint.exists,
                time.monotonic() + 5, process=core)
            writer, owner, other = connect(), connect(), connect()
            writer.send(b'set echo\n')
            assert rpc(owner)['state'] == 'inactive' and styles() == {'*': 31, 'SlateLauncherStyle': 42}
            evidence['cases']['native-producer-death-fresh-actor-no-inheritance'] = 'passed'
            evidence['group_results']['v1'] = 'passed'
            if args.complete_bundle:
                complete_bundle_cases()
            log = (runtime.root / 'debug.log').read_text()
            assert 'X error' not in log and 'AddressSanitizer' not in log and 'runtime error:' not in log
            assert core.poll() is None and not errors.unexpected
            evidence['configuration_sha256'] = hashlib.sha256(config.read_bytes()).hexdigest()
            assert evidence['source_sha256'] == hashes(source, inputs)
            assert all(binary_hash(item['path']) == item for item in
                evidence['binary_inputs'].values()), 'binary changed during run'
            evidence['status'] = 'passed'
        except BaseException as error:
            evidence.update(status='failed', error=repr(error),
                wm_exit_status=core.poll() if core else None)
            raise
        finally:
            def clean(label, operation):
                try:
                    return operation()
                except BaseException as failure:
                    evidence['status'] = 'failed'
                    evidence.setdefault('cleanup_failures', []).append(dict(
                        step=label, error=repr(failure)))
            if held:
                clean('physical button release', lambda: pointer.button(False))
            for connection in connections:
                clean('connection', connection.close)
            if core:
                clean('core', lambda: core.close(time.monotonic() + 5))
                evidence['core_wait'] = dict(returncode=core.process.returncode)
                if (client and evidence.get('external_root_before_shutdown') and
                        core.poll() is not None):
                    def external_preserved():
                        if args.complete_bundle_tail_only:
                            assert core.process.returncode == 0, (
                                evidence['core_wait'])
                        actual = PreviewWitness(client).root_state(owned=False)
                        assert actual == evidence['external_root_before_shutdown']
                        return dict(root=actual,
                            external_publication_and_pixels_preserved=True)
                    evidence['external_root_shutdown'] = clean(
                        'external root shutdown preservation', external_preserved)
                    if (args.complete_bundle_tail_only and
                            evidence['external_root_shutdown'] is not None):
                        evidence['group_results']['v3-tail'] = 'passed'
                elif (client and evidence.get('owned_root_before_shutdown') and
                        core.poll() is not None):
                    def root_retired():
                        remaining = PreviewWitness(client).property(
                            '_XROOTPMAP_ID')
                        assert remaining is None, remaining
                        return dict(root_property=None,
                            matching_core_publication_removed=True)
                    evidence['owned_root_shutdown'] = clean(
                        'owned root shutdown publication', root_retired)
                    if evidence['owned_root_shutdown'] is not None:
                        evidence['group_results'][
                            'owned-root-shutdown'] = 'passed'
            if native and native.exists():
                evidence['module_waits'] = clean('actual module Wait receipts',
                    lambda: cleanup_receipts(native))
                clean('module records', lambda: shutil.copytree(native,
                    args.artifacts_dir / 'native-records'))
            if errors:
                clean('X observer', errors.close_display)
            elif client:
                clean('X client', client.close)
            for path in runtime.root.iterdir():
                if path.suffix in ('.txt', '.log'):
                    clean('copy ' + path.name, lambda p=path:
                        shutil.copy2(p, args.artifacts_dir / p.name))
            (args.artifacts_dir / 'evidence.json').write_text(json.dumps(evidence, indent=2) + '\n')
            if evidence['status'] != 'passed' and sys.exc_info()[0] is None:
                raise AssertionError(evidence.get('cleanup_failures', evidence))
    print(json.dumps(evidence))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__, allow_abbrev=False)
    for name in ('fvwm', 'mfl', 'probe', 'artifacts-dir'):
        parser.add_argument('--' + name, type=Path, required=True)
    parser.add_argument('--display')
    parser.add_argument('--config-header', type=Path,
        help='Actual matching build config.h, required for installed v3 cores')
    parser.add_argument('--backer', type=Path)
    parser.add_argument('--worker', type=Path,
        help='Actual fvwm-image-svg-worker; v3 never substitutes a fake worker')
    parser.add_argument('--complete-bundle', action='store_true')
    parser.add_argument('--owned-root-shutdown-only', action='store_true',
        help='Only actual normal root publication, Quit, Wait and removal')
    parser.add_argument('--complete-bundle-only', action='store_true',
        help='Run v3 complete bundles; keep existing v1/v2 groups separate')
    parser.add_argument('--complete-bundle-tail-only', action='store_true',
        help='Only named launcher conflict, actor loss and external root '
             'through shutdown')
    parser.add_argument('--plain-root', action='store_true',
        help='Use a solid ordinary root for a minimal feature-profile run')
    parser.add_argument('--topology-change', type=Path,
        help='JSON change/restore argv for an actual supported RandR mutation')
    parser.add_argument('--execution-profile', required=True)
    parser.add_argument('--metadata-limit', action='store_true')
    parser.add_argument('--opaque-colors', action='store_true',
        help='Also exercise version2 opaque colors and unchanged unequal scalars')
    parser.add_argument('--allocation-failure', action='store_true',
        help='--fvwm is a separate test core linked with contrast_alloc_fail.c')
    options = parser.parse_args()
    if options.complete_bundle_tail_only:
        if (options.complete_bundle or options.owned_root_shutdown_only or
                options.opaque_colors or options.metadata_limit or
                options.allocation_failure or options.topology_change):
            parser.error('--complete-bundle-tail-only conflicts with full, '
                'owned-root, v1/v2, allocation and topology selections')
        options.complete_bundle_only = True
    if options.owned_root_shutdown_only:
        options.complete_bundle_only = True
    for name in ('fvwm', 'mfl', 'probe', 'artifacts_dir'):
        setattr(options, name, getattr(options, name).resolve())
    if options.config_header:
        options.config_header = options.config_header.resolve()
        if not options.config_header.is_file():
            parser.error('unavailable actual --config-header')
    if options.backer:
        options.backer = options.backer.resolve()
    if options.worker:
        options.worker = options.worker.resolve()
    if ((options.complete_bundle or options.complete_bundle_only) and
            not options.backer):
        parser.error('v3 complete-bundle tests require the actual --backer')
    if ((options.complete_bundle or options.complete_bundle_only) and
            not options.worker):
        parser.error('v3 complete-bundle tests require the actual --worker')
    for name in ('fvwm', 'mfl', 'probe', 'backer', 'worker'):
        path = getattr(options, name)
        if path and (not path.is_file() or not os.access(path, os.X_OK)):
            parser.error('unavailable actual executable --' + name)
    if options.topology_change and not options.topology_change.is_file():
        parser.error('unavailable topology change/restore JSON')
    run(options)
