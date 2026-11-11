#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, version 2 or later.
"""Observe workspace placement on an owned two-output Xorg dummy server."""

import argparse
import ctypes as C
import hashlib
import json
from pathlib import Path
import shlex
import shutil
import signal
import sys
import tempfile
import time

from action_palette_live import run
from geometry_transition_live import SizeHints
from live_x_harness import (HarnessError, run_required_xrandr, start_xorg_dummy,
    wait_for_authoritative_state, wait_for_predicate, wait_for_randr)
from workspace_tools_live import fixture_records, replies, stop_clients


LEFT, RIGHT = 'DUMMY0', 'DUMMY1'
OVERVIEW = 'Window and workspace overview'
SCRATCH = 'Mixed scratchpad client'
STRUT = 56


def output_setup(width_mm, height_mm):
    return ['--output', RIGHT, '--mode', '1280x1024', '--pos', '1920x0',
        '--set', 'WIDTH_MM', str(width_mm), '--set', 'HEIGHT_MM', str(height_mm),
        '--fb', '3200x1080', '--fbmm', '846x286']


def server_state(runtime, display, label, primary, right_mm):
    def wanted(state):
        monitors = state['monitors']
        names = {LEFT} if right_mm is None else {LEFT, RIGHT}
        return (set(monitors) == names and monitors[primary]['primary'] and
            (right_mm is None or monitors[RIGHT]['mm_width'] == right_mm))
    return wait_for_authoritative_state(runtime, display, label, wanted,
        time.monotonic() + 8, label)


def start_display(runtime, evidence, config):
    evidence['scope'] = 'Owned Xorg dummy outputs, actual RandR changes and X geometry'
    evidence['operations'] = []
    evidence['source_sha256']['tests/live_x/workspace_placement_mixed_live.py'] = (
        hashlib.sha256(Path(__file__).read_bytes()).hexdigest())
    evidence['xorg_config'] = dict(path=str(config),
        sha256=hashlib.sha256(config.read_bytes()).hexdigest())
    _, display = start_xorg_dummy(runtime, config, time.monotonic() + 10)
    wait_for_randr(runtime, display, time.monotonic() + 10)
    run_required_xrandr(runtime, display, ['--addmode', RIGHT, '1280x1024'],
        'add-secondary-mode', evidence['operations'])
    run_required_xrandr(runtime, display,
        ['--output', LEFT, '--mode', '1920x1080', '--pos', '0x0', '--primary',
         '--set', 'WIDTH_MM', '508', '--set', 'HEIGHT_MM', '286'] +
        output_setup(169, 135), 'mixed-scale-baseline', evidence['operations'])
    evidence['initial_randr'] = server_state(runtime, display, 'initial-randr', LEFT, 169)
    return display


def frame_rect(client, window):
    current = window
    for _ in range(8):
        parent = client.parent(current)
        if parent == client.root:
            if current == window:
                return None
            return dict(zip(('x', 'y', 'width', 'height'), client.geometry(current)))
        current = parent
    raise AssertionError('fixture frame ancestry exceeds bound')


def contains(area, rect):
    return (area['x'] <= rect['x'] and area['y'] <= rect['y'] and
        rect['x'] + rect['width'] <= area['x'] + area['width'] and
        rect['y'] + rect['height'] <= area['y'] + area['height'])


def client_size(client, window):
    return client.geometry(window)[2:]


def scene(**s):
    runtime, evidence, probe, core, client, view = (s[key] for key in (
        'runtime', 'evidence', 'probe', 'core', 'client', 'view'))
    display = evidence['display']
    cases = evidence['cases'] = {}
    observations = evidence['observations'] = {}
    launched = runtime.root / 'mixed-scratchpad.jsonl'
    deadline = time.monotonic() + 130

    def wait(label, predicate, seconds=8):
        return wait_for_predicate(label, predicate,
            min(deadline, time.monotonic() + seconds), process=core)

    def mutate(label, arguments):
        run_required_xrandr(runtime, display, arguments, label, evidence['operations'])

    def commit(label, primary, quarters, after=0):
        def observed():
            t = probe.query('topology', limit=8).get('result', {}).get('topology')
            w = probe.query('workspaces', limit=8).get('result')
            if t is None or w is None:
                return None
            monitors = {m['name']: m for m in t['monitors']}
            # The workspace query includes cached rows for disabled monitors.
            work = {m['name']: m for m in w['monitors'] if m['name'] in monitors}
            epoch = int(t['epoch'], 16)
            sample = dict(epoch=epoch, monitors=monitors, workspaces=work)
            observations[label] = sample
            if (epoch <= after or set(monitors) != set(quarters) or
                    set(work) != set(quarters) or not monitors[primary]['primary'] or
                    any(work[name]['scale_quarters'] != scale
                        for name, scale in quarters.items())):
                return None
            return sample
        return wait('committed ' + label, observed)

    def measure(windows):
        return [dict(frame=frame_rect(client, w), client=client_size(client, w))
                for w in windows]

    def make_window(title, x, y, hints=None):
        window = client.x.XCreateSimpleWindow(client.display, client.root,
            x, y, 390, 180, 0, 0, 0x324452)
        assert window
        s['windows'].append(window)
        client.x.XStoreName(client.display, window, title.encode())
        if hints is not None:
            client.x.XSetWMNormalHints(client.display, window, C.byref(hints))
        client.x.XMapWindow(client.display, window)
        client.x.XSync(client.display, 0)
        wait('managed ' + title, lambda: frame_rect(client, window))
        return window

    def retire(window):
        client.x.XDestroyWindow(client.display, window)
        client.x.XSync(client.display, 0)
        s['windows'].remove(window)

    def move(window, x, y):
        probe.command('WindowId %d Move %dp %dp ewmhiwa' % (window, x, y))
        return wait('exact explicit fixture move', lambda: (r if
            (r := frame_rect(client, window)) and (r['x'], r['y']) == (x, y) else None))

    def install_strut():
        probe.command('Style WorkspaceFixtureStrut Sticky, NoTitle, BorderWidth 0, HandleWidth 0, EWMHUseStrutHints')
        x, d = client.x, client.display
        x.XInternAtom.argtypes = [C.c_void_p, C.c_char_p, C.c_int]
        x.XInternAtom.restype = C.c_ulong
        x.XChangeProperty.argtypes = [C.c_void_p, C.c_ulong, C.c_ulong,
            C.c_ulong, C.c_int, C.c_int, C.c_void_p, C.c_int]
        x.XChangeProperty.restype = C.c_int
        window = x.XCreateSimpleWindow(d, client.root, 0, 0, 1920, STRUT,
            0, 0, 0x445566)
        assert window
        s['windows'].append(window)
        x.XStoreName(d, window, b'WorkspaceFixtureStrut')

        def property_(name, kind, values):
            data = (C.c_ulong * len(values))(*values)
            assert x.XChangeProperty(d, window, x.XInternAtom(d, name, 0),
                kind, 32, 0, C.cast(data, C.c_void_p), len(values))

        property_(b'_NET_WM_WINDOW_TYPE', 4,
            [x.XInternAtom(d, b'_NET_WM_WINDOW_TYPE_DOCK', 0)])
        property_(b'_NET_WM_STRUT', 6, [0, 0, STRUT, 0])
        property_(b'_NET_WM_STRUT_PARTIAL', 6,
            [0, 0, STRUT, 0, 0, 0, 0, 0, 0, 1919, 0, 0])
        x.XMapWindow(d, window)
        x.XSync(d, 0)

    def scratch_reply(after, **wanted):
        return wait('actual mixed scratchpad reply', lambda: next((r for r in
            reversed(replies(s['native'])) if r['name'] == 'mixed' and r['time'] >= after
            and all(r.get(key) == value for key, value in wanted.items())), None))

    def overview_refresh(previous=None):
        probe.command('SendToModule FvwmDesktop overview')
        def ready():
            result = probe.diagnostic()
            if (not result['open'] or not result['native_ready'] or
                    result['states'][:2] != ['observed', 'observed']):
                return None
            binding = result['binding']
            if (binding['in_flight'] or binding['pending'] or
                    (previous is not None and binding['published'] <= previous)):
                return None
            assert result['view']['mode'] == 'overview' and result['counts'][2:] == [0, 0]
            return result
        return wait('overview publishes current topology', ready, 12)

    def verify_roles(topology, overview):
        inventory = probe.inventory()
        probe.release()
        desktops = [r for r in inventory['rows'] if r['kind'] == 'desktop']
        expected = {str(int(m['name_atom'])): {str(int(o)) for o in m['outputs']}
                    for m in topology['monitors'].values()}
        assert {r['monitor_role'] for r in desktops} == set(expected), (desktops, expected)
        assert all(r['monitor_output'] in expected[r['monitor_role']]
                   for r in desktops), (desktops, expected)
        assert overview['counts'][1] == len(desktops), (overview, desktops)
        return desktops

    def positioned(windows, area, changed_from=None, samples=None):
        def observed():
            result = measure(windows)
            if samples is not None:
                samples['last'] = result
            if result == changed_from:
                return None
            return result if all(contains(area, row['frame']) for row in result) else None
        return wait('windows within destination work area', observed)

    def restore(name, applied, skipped, fallback=False):
        log_offset = core.log_path.stat().st_size
        probe.command('WorkspaceDock Restore ' + name)
        suffix = ', primary fallback' if fallback else ''
        expected = ('WorkspaceDock: restored %s (%d applied, %d skipped%s)' %
            (name, applied, skipped, suffix))

        def reported():
            with core.log_path.open('rb') as stream:
                stream.seek(log_offset)
                text = stream.read(65536).decode(errors='replace')
            observations['last_dock_report'] = text
            return next((line for line in text.splitlines() if expected in line), None)
        return wait('reported docking outcome for ' + name, reported)

    try:
        # Remove the shared harness's four sample windows so layout membership
        # is independently known. Keep the existing desktop and observer.
        for window in s['windows'][:]:
            retire(window)
        probe.command('DesktopConfiguration per-monitor')
        view.pointer.motion(500, 400)
        probe.command('GotoDesk 0 0', 'GotoPage 0 0')
        install_strut()
        initial = commit('initial-topology', LEFT, {LEFT: 4, RIGHT: 8})
        assert initial['monitors'][RIGHT]['geometry']['x'] == 1920
        def reserved():
            reply = probe.query('workspaces', limit=8)['result']
            row = next(m for m in reply['monitors'] if m['name'] == LEFT)
            area = row['dynamic_work_area']
            return area if row['desk'] == 0 and area and area['y'] >= STRUT else None
        area = wait('primary desk work area includes actual strut', reserved)
        observations['reserved_primary_area'] = area

        hint = SizeHints(flags=(1 << 4) | (1 << 6) | (1 << 8),
            min_width=83, min_height=61, width_inc=11, height_inc=7,
            base_width=83, base_height=61)
        layout = [make_window('Mixed layout hints', 130, 160, hint),
                  make_window('Mixed layout peer', 680, 180),
                  make_window('Mixed layout third', 1180, 200)]
        for i, window in enumerate(layout):
            move(window, 130 + 500*i, 160 + 40*i)
        before = measure(layout)
        samples = observations['columns_attempt'] = dict(before=before)
        view.pointer.motion(500, 400)
        samples['command'] = probe.command('WorkspaceLayout columns')
        try:
            columns = positioned(layout, area, changed_from=before, samples=samples)
        except HarnessError:
            try:
                s['capture']('workspace-placement-columns-failure.png')
            except Exception as error:
                samples['capture_error'] = str(error)
            raise
        ordered = sorted((r['frame'] for r in columns), key=lambda r: r['x'])
        for i, rect in enumerate(ordered):
            left, right = (area['width']*i//3, area['width']*(i+1)//3)
            assert contains(dict(x=area['x']+left, y=area['y'],
                width=right-left, height=area['height']), rect), columns
        width, height = columns[0]['client']
        assert (width - 83) % 11 == 0 and (height - 61) % 7 == 0, columns[0]
        assert columns[0]['client'] != before[0]['client']
        cases['columns_respect_work_area_and_size_hints'] = True

        # Restore the exact tiled coordinates after an intervening user move.
        # Coordinate equality alone must not restore this window's old shape.
        owned = columns[0]['frame']
        moved = move(layout[0], owned['x']+23, owned['y'])
        assert moved != owned
        move(layout[0], owned['x'], owned['y'])
        assert measure(layout)[0] == columns[0]
        probe.command('WorkspaceLayout floating')
        floated = [columns[0], *before[1:]]
        wait('floating restores only still-owned windows', lambda: measure(layout) == floated)
        cases['layout_restore_preserves_newer_away_and_back_move'] = True

        probe.command('WorkspaceLayout master-stack')
        master = positioned(layout, area, changed_from=floated)
        split_x = area['x'] + (area['width']+1)//2
        split_y = area['y'] + (area['height']+1)//2
        right = sorted((r['frame'] for r in master if r['frame']['x'] >= split_x),
            key=lambda r: r['y'])
        assert len(right) == 2 and right[0]['y'] < split_y <= right[1]['y'], master
        probe.command('WorkspaceLayout floating')
        wait('floating restores the master-stack baseline', lambda: measure(layout) == floated)
        observations['layout'] = dict(original=before, columns=columns,
            manual_away=moved, floating=floated, master_stack=master)
        cases['master_stack_and_owned_floating_restore'] = True
        for window in layout:
            retire(window)

        dock = [make_window('Mixed dock manual', 140, 170),
                make_window('Mixed dock untouched', 700, 210),
                make_window('Mixed dock retired', 1230, 250)]
        for i, window in enumerate(dock):
            move(window, 140 + 520*i, 170 + 40*i)
        dock_before = measure(dock)
        right_area = initial['workspaces'][RIGHT]['dynamic_work_area']
        assert right_area is not None and right_area['x'] >= 1920
        probe.command('WorkspaceDock Capture connected DUMMY1')
        connected_report = restore('connected', 3, 0)
        connected = positioned(dock, right_area)
        assert [r['client'] for r in connected] == [r['client'] for r in dock_before]
        observations['connected_dock'] = dict(source=dock_before, destination=connected,
            report=connected_report)
        cases['connected_docking_preserves_client_pixels'] = True
        for window, original in zip(dock, dock_before):
            move(window, original['frame']['x'], original['frame']['y'])
        view.pointer.motion(500, 400)
        probe.command('WorkspaceDock Capture travel DUMMY1')

        # Launch the scratchpad after capture so its window is outside the dock
        # profile. Observe the same allocation on both sides of the topology change.
        scratch_helper = Path(__file__).with_name('workspace_tools_live.py').resolve()
        argv = [sys.executable, str(scratch_helper), '--client', '--record', str(launched),
            '--class', 'MixedScratch', '--instance', 'mixed', '--title', SCRATCH]
        local = s['user'] / 'local.config'
        local.write_text(local.read_text() + '\n*FvwmDesktop: Scratchpad ' +
            shlex.join(['mixed', 'MixedScratch', 'mixed', *argv]) + '\n')
        installed = time.monotonic()
        probe.command('Read ' + shlex.quote(str(local)), 'KillModule FvwmDesktop',
            'Module FvwmDesktop')
        scratch_reply(installed, state='unmatched')
        view.pointer.motion(2200, 350)
        launch_at = time.monotonic()
        probe.command('SendToModule FvwmDesktop scratchpad mixed')
        grant = scratch_reply(launch_at, action='launch')
        scratch_reply(launch_at, state='claimed')
        scratch = view.wait(SCRATCH)
        scratch_size = client_size(client, scratch)
        scratch_first = positioned([scratch], right_area)[0]
        hide_at = time.monotonic()
        probe.command('SendToModule FvwmDesktop scratchpad mixed')
        scratch_reply(hide_at, action='dismissed')
        wait('secondary scratchpad is hidden', lambda: not view.named(SCRATCH))

        native_initial = overview_refresh()
        observations['overview_initial'] = dict(native=native_initial,
            desktops=verify_roles(initial, native_initial))
        stale = probe.inventory()
        source_row = next(r for r in stale['rows'] if r['label'] == 'Mixed dock manual')
        role = str(int(initial['monitors'][RIGHT]['name_atom']))
        destination = next(r for r in stale['rows'] if r['kind'] == 'desktop' and
            r['monitor_role'] == role)
        mutate('secondary-primary-and-144dpi', ['--output', RIGHT, '--primary',
            '--set', 'WIDTH_MM', '226', '--set', 'HEIGHT_MM', '181'])
        evidence['scaled_randr'] = server_state(runtime, display, 'scaled-randr', RIGHT, 226)
        changed = commit('scaled-secondary', RIGHT, {LEFT: 4, RIGHT: 6}, initial['epoch'])
        refused = probe.exchange('Send_Palette', stale['generation'], str(source_row['index']),
            'move', str(destination['index']), '0', '0')
        assert refused.get('error', {}).get('code') in (
            'stale_topology', 'stale_placement', 'stale_generation'), refused
        probe.release()
        native_changed = overview_refresh(native_initial['binding']['published'])
        observations['stale_overview_action'] = dict(refusal=refused,
            desktops=verify_roles(changed, native_changed), native_before=native_initial,
            native_after=native_changed)
        cases['scale_and_primary_change_refuses_stale_overview_move'] = True

        mutate('disable-secondary', ['--output', LEFT, '--primary', '--output', RIGHT,
            '--off', '--fb', '1920x1080'])
        evidence['disabled_randr'] = server_state(runtime, display, 'disabled-randr', LEFT, None)
        disabled = commit('disabled-secondary', LEFT, {LEFT: 4}, changed['epoch'])
        native_disabled = overview_refresh(native_changed['binding']['published'])
        observations['overview_single_output'] = dict(native=native_disabled,
            desktops=verify_roles(disabled, native_disabled))
        cases['overview_refresh_tracks_remaining_committed_role'] = True

        left_area = disabled['workspaces'][LEFT]['dynamic_work_area']
        assert left_area is not None and left_area['y'] >= STRUT
        before_fallback = measure(dock)
        reported = restore('travel', 3, 0, fallback=True)
        fallback = positioned(dock, left_area)
        observations['docking_fallback'] = dict(message=reported, before=before_fallback,
            windows=fallback, area=left_area)
        assert [r['client'] for r in fallback] == [r['client'] for r in before_fallback]
        cases['missing_connector_reports_reachable_fallback'] = True

        view.pointer.motion(500, 400)
        summon_at = time.monotonic()
        probe.command('SendToModule FvwmDesktop scratchpad mixed')
        summoned = scratch_reply(summon_at, action='summoned')
        assert view.wait(SCRATCH) == scratch
        scratch_after = positioned([scratch], left_area)[0]
        assert scratch_after['client'] == scratch_size
        rect = scratch_after['frame']
        assert abs(2*rect['x']+rect['width']-2*left_area['x']-left_area['width']) <= 2
        assert abs(2*rect['y']+rect['height']-2*left_area['y']-left_area['height']) <= 2
        assert len(fixture_records(launched, 'launch')) == 1
        observations['scratchpad_remaining_output'] = dict(grant=grant, summon=summoned,
            secondary=scratch_first, primary=scratch_after)
        cases['scratchpad_summon_after_unplug_preserves_client_pixels'] = True

        manual = move(dock[0], 260, 340)
        retired = probe.inventory()
        retired_row = next(r for r in retired['rows'] if r['label'] == 'Mixed dock retired')
        retire(dock[2])
        replacement = make_window('Mixed dock retired', 1160, 390)
        replacement_rect = move(replacement, 1160, 390)
        assert replacement != dock[2]
        old_action = probe.exchange('Send_Palette', retired['generation'], str(retired_row['index']))
        assert old_action == {'error': {'code': 'stale_target'}}, old_action
        probe.release()

        mutate('reenable-secondary', ['--output', LEFT, '--primary'] + output_setup(169, 135))
        evidence['reenabled_randr'] = server_state(runtime, display, 'reenabled-randr', LEFT, 169)
        enabled = commit('reenabled-secondary', LEFT, {LEFT: 4, RIGHT: 8}, disabled['epoch'])
        before_replug_restore = measure(dock[:2])
        replug_report = restore('travel', 1, 2)
        untouched = positioned([dock[1]], enabled['workspaces'][RIGHT]['dynamic_work_area'])[0]
        assert untouched['client'] == before_replug_restore[1]['client']
        assert frame_rect(client, dock[0]) == manual
        assert client_size(client, dock[0]) == before_replug_restore[0]['client']
        assert frame_rect(client, replacement) == replacement_rect
        observations['replug_restore'] = dict(untouched=untouched, manual=manual,
            replacement=replacement_rect, retired_action=old_action, report=replug_report)
        cases['replug_restore_preserves_newer_move_and_skips_retired_allocation'] = True

        native_enabled = overview_refresh(native_disabled['binding']['published'])
        observations['overview_replug'] = dict(native=native_enabled,
            desktops=verify_roles(enabled, native_enabled))
        view.wait(OVERVIEW)
        s['capture']('workspace-placement-mixed.png')
        assert evidence['screenshots'] == ['workspace-placement-mixed.png']
    finally:
        try:
            observations['retired_fixture_pids'] = stop_clients([launched])
        finally:
            if launched.exists():
                shutil.copy2(launched, s['out'] / launched.name)


def main():
    parser = argparse.ArgumentParser(description=__doc__, allow_abbrev=False)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--artifacts-dir', type=Path, required=True)
    parser.add_argument('--xorg-config', type=Path,
        default=Path(__file__).with_name('xorg-dummy.conf'))
    args = parser.parse_args()
    config = args.xorg_config.resolve(strict=True)
    args.artifacts_dir.mkdir(parents=True, exist_ok=True)
    root = Path(tempfile.mkdtemp(prefix='workspace-placement-mixed-',
        dir=args.artifacts_dir.resolve()))

    def expired(_signal, _frame):
        raise TimeoutError('mixed workspace placement deadline')

    previous = signal.signal(signal.SIGALRM, expired)
    signal.alarm(180)
    try:
        run(args, 'slate-dark', root, scenario=scene,
            display_initializer=lambda runtime, evidence: start_display(runtime, evidence, config))
        path = root / 'slate-dark' / 'evidence.json'
        evidence = json.loads(path.read_text())
        assert evidence['status'] == 'passed' and not evidence['cleanup_failures'], path
        assert not evidence['observer_unexpected_x_errors'], path
        assert len(evidence['cases']) == 9 and all(evidence['cases'].values()), path
        assert all(code is not None for code in evidence['children_after_cleanup'].values()), path
        print(path)
    finally:
        signal.alarm(0)
        signal.signal(signal.SIGALRM, previous)
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
