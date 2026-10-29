#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or any later version.
"""Observe resource-preview ownership through real core, module and X state.

Requires a PNG-enabled built core/MFL, image_owner_probe, Xvfb and libXtst.
No worker or core reply is synthesized. Run the old mfl_preview_live.py as a
separate scalar-opacity regression. --execution-profile labels actual execution;
it does not establish native CPU or sanitizer qualification.
"""
import argparse
import copy
import ctypes as C
import hashlib
import json
from pathlib import Path
import shutil
import time

from geometry_transition_live import Client, Pointer
from image_colorsets_live import records
from image_owners_live import png
from live_x_harness import HarnessRuntime, start_display_server, wait_for_predicate
from mfl_request_live import frame, reply
from mfl_stream_live import Connection
from session_x_errors import SessionXErrors


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
        'cases': {}, 'limits': [
            'No physical GPU, external compositor or native-CPU claim.',
            'Build-tree fixture reads source-owned ConfigFvwmDefaults; an absent '
            'compiled-prefix startup Read remains a setup warning, not installed acceptance.',
            'Synthetic ownerless masks need pure-policy coverage.'],
        'core_artifact': 'fault-test' if args.allocation_failure else 'normal',
        'core_sha256': hashlib.sha256(args.fvwm.read_bytes()).hexdigest()}
    with HarnessRuntime(prefix='fvwm-contrast-resource-') as runtime:
        connections, client, errors, core = [], None, None, None
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
            image, shape = runtime.root / 'source.png', runtime.root / 'shape.xbm'
            image.write_bytes(png())
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
                'AddToFunc StartFunction I Module FvwmMFL\n' +
                ''.join(f'Colorset {identifier} fg #123456, bg #345678, '
                    'hi #56789a, sh #789abc, fgsh #aa55aa, IconAlpha 63, '
                    'IconTint #332211 17\n' for identifier in ROLE_IDS))
            (args.artifacts_dir / 'configuration.fvwm').write_bytes(config.read_bytes())
            allocation_marker = runtime.root / 'allocation-failure.txt'
            core = runtime.start('core', [str(args.fvwm), '-d', display,
                '-f', str(config), '-v', '-o', str(runtime.root / 'debug.log')],
                'core.log', runtime.environment({'FVWMMFL_SOCKET_PATH': str(runtime.root),
                    'FVWM_USERDIR': str(runtime.root),
                    'FVWM_TEST_CONTRAST_ALLOC_FAIL': str(allocation_marker)}))
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

            def snapshot():
                nonlocal serial
                serial += 1
                path = runtime.root / f'snapshot-{serial}.txt'
                command(f'Module {args.probe.name} colorsets {path}')
                def complete():
                    rows = {row[0]: row for row in records(path)}
                    return rows if set(ROLE_IDS) <= rows.keys() else None
                result = wait_for_predicate('complete native colorset snapshot', complete,
                    time.monotonic() + 5, process=core)
                command('KillModule ' + args.probe.name)
                return {identifier: result[identifier] for identifier in ROLE_IDS}

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
            log = (runtime.root / 'debug.log').read_text()
            assert 'X error' not in log and 'AddressSanitizer' not in log and 'runtime error:' not in log
            assert core.poll() is None and not errors.unexpected
            evidence['configuration_sha256'] = hashlib.sha256(config.read_bytes()).hexdigest()
            evidence['status'] = 'passed'
        except BaseException as error:
            evidence.update(status='failed', error=repr(error),
                wm_exit_status=core.poll() if core else None)
            raise
        finally:
            if held:
                pointer.button(False)
            for connection in connections:
                connection.close()
            if core:
                core.close(time.monotonic() + 5)
            if errors:
                errors.close_display()
            elif client:
                client.close()
            for path in runtime.root.iterdir():
                if path.suffix in ('.txt', '.log'):
                    shutil.copy2(path, args.artifacts_dir / path.name)
            (args.artifacts_dir / 'evidence.json').write_text(json.dumps(evidence, indent=2) + '\n')
    print(json.dumps(evidence))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__, allow_abbrev=False)
    for name in ('fvwm', 'mfl', 'probe', 'artifacts-dir'):
        parser.add_argument('--' + name, type=Path, required=True)
    parser.add_argument('--display')
    parser.add_argument('--execution-profile', required=True)
    parser.add_argument('--metadata-limit', action='store_true')
    parser.add_argument('--opaque-colors', action='store_true',
        help='Also exercise version2 opaque colors and unchanged unequal scalars')
    parser.add_argument('--allocation-failure', action='store_true',
        help='--fvwm is a separate test core linked with contrast_alloc_fail.c')
    options = parser.parse_args()
    for name in ('fvwm', 'mfl', 'probe', 'artifacts_dir'):
        setattr(options, name, getattr(options, name).resolve())
    run(options)
