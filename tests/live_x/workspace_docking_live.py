#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or any later version.
"""One native checkpoint for persisted workspace docking profiles."""
import argparse
import base64
import ctypes as C
import hashlib
import json
import os
from pathlib import Path
import re
import signal
import stat
import tempfile
import time

from action_palette_live import PROFILES, Probe, run
from live_x_harness import run_required_xrandr, wait_for_predicate
from native_bindings_live import rows
from workspace_placement_mixed_live import (LEFT, RIGHT, client_size, contains,
    frame_rect, output_setup, server_state, start_display)
from workspace_tools_live import ClassHint


PROFILE = 'travel'
PREVIEW = 'Apply docking profile'
PANEL = 'SlateBar'
TARGET_DESK = 1
SELECTORS = [
    {'instance': 'first', 'class': 'DockFixture', 'offset_x': 130, 'offset_y': 0},
    {'instance': 'second', 'class': 'DockFixture', 'offset_x': 640, 'offset_y': 270},
]
CASE_NAMES = {
    'desktop_saves_private_data_only_profile',
    'preview_is_read_only',
    'newer_workspace_refuses_plan',
    'newer_panel_assignment_refuses_plan',
    'newer_desktop_settings_refuse_plan',
    'newer_window_move_is_preserved',
    'apply_restores_workspace_panel_and_client_placement',
    'topology_change_refuses_plan_and_missing_connector_falls_back',
    'restart_reloads_profile_and_resolves_current_allocations',
}
FORM_TOKEN = re.compile(r'^\*SlateDockingForm: CloseCommand SendToModule FvwmDesktop '
    r'dock-control ([0-9a-f]{32}) close$')


def encoded(text):
    return 'x' + base64.urlsafe_b64encode(str(text).encode()).decode().rstrip('=')


def scene(**s):
    runtime, evidence, probe, core, client, view, native = (s[key] for key in (
        'runtime', 'evidence', 'probe', 'core', 'client', 'view', 'native'))
    display = evidence['display']
    cases = evidence['cases'] = {}
    observed = evidence['observations'] = {}
    deadline = time.monotonic() + 145

    def wait(label, predicate, seconds=8):
        return wait_for_predicate(label, predicate,
            min(deadline, time.monotonic() + seconds), process=core)

    def workspace():
        return {row['name']: row for row in probe.query('workspaces', limit=8)['result']['monitors']}

    def topology(label, scales, after=0):
        def committed():
            top = probe.query('topology', limit=8)['result']['topology']
            monitors = {row['name']: row for row in top['monitors']}
            # Keep cached workspace rows only for committed active monitors.
            work = {name: row for name, row in workspace().items() if name in monitors}
            result = dict(epoch=int(top['epoch'], 16), monitors=monitors, workspaces=work)
            observed[label] = result
            if (result['epoch'] > after and set(monitors) == set(work) == set(scales) and
                    monitors[LEFT]['primary'] and
                    all(work[name]['scale_quarters'] == scale for name, scale in scales.items())):
                return result
            return None
        return wait('committed ' + label, committed)

    def panel_at(output):
        def located():
            work = workspace()
            area = work[output]['dynamic_work_area']
            other = RIGHT if output == LEFT else LEFT
            if not area or area['y'] <= 0:
                return None
            if other in work:
                other_area = work[other]['dynamic_work_area']
                if not other_area or other_area['y'] != 0:
                    return None
            bar = view.named(PANEL)
            attrs = view.attributes(bar) if bar is not None else None
            if attrs is None or attrs.width != area['width'] or attrs.height != area['y']:
                return None
            return dict(area=area, size=(attrs.width, attrs.height), desk=work[output]['desk'])
        return wait('actual Slate panel and strut on ' + output, located)

    def set_desk(output, desk):
        probe.command('GotoDesk screen %s 0 %d' % (output, desk))
        return wait('active desk %d on %s' % (desk, output),
            lambda: workspace()[output]['desk'] == desk)

    def retire(window):
        client.x.XDestroyWindow(client.display, window)
        client.x.XSync(client.display, 0)
        s['windows'].remove(window)

    def make_client(title, selector, x, y):
        window = client.x.XCreateSimpleWindow(client.display, client.root,
            x, y, 310, 156, 0, 0, 0x354756)
        assert window
        s['windows'].append(window)
        client.x.XStoreName(client.display, window, title.encode())
        client.x.XSetClassHint.argtypes = [C.c_void_p, C.c_ulong, C.POINTER(ClassHint)]
        hint = ClassHint(selector['instance'].encode(), selector['class'].encode())
        assert client.x.XSetClassHint(client.display, window, C.byref(hint))
        client.x.XMapWindow(client.display, window)
        client.x.XSync(client.display, 0)
        wait('managed ' + title, lambda: frame_rect(client, window))
        return window

    def move(window, x, y):
        probe.command('WindowId %d Move %dp %dp ewmhiwa' % (window, x, y))
        return wait('exact manual fixture move', lambda: (frame if
            (frame := frame_rect(client, window)) and (frame['x'], frame['y']) == (x, y)
            else None))

    def measure(windows):
        return [dict(frame=frame_rect(client, w), client=client_size(client, w))
                for w in windows]

    def fresh_rows(after, direction='out'):
        return [row for row in rows(native)
                if row['direction'] == direction and row['time'] >= after]

    def form_token(after):
        def token():
            return next((m.group(1) for row in reversed(fresh_rows(after))
                if (m := FORM_TOKEN.fullmatch(row['text']))), None)
        return wait('new desktop docking Form callback', token)

    def desktop(command):
        started = time.monotonic()
        assert probe.command('SendToModule FvwmDesktop ' + command)['done']
        return started

    def callback(token, command):
        return desktop('dock-control ' + token + ' ' + command)

    def edit():
        token = form_token(desktop('dock ' + PROFILE))
        view.wait('Docking profile')
        return token

    def dock_reply(after, action):
        def delivered():
            commands = [row['text'].split() for row in fresh_rows(after)
                if row['text'].startswith('Send_Dock ')]
            sent = next((cmd for cmd in reversed(commands) if cmd[3] == action), None)
            if sent is None:
                return None
            prefix = 'MFL_CORE_1 ' + ' '.join(sent[1:3]) + ' '
            parts = []
            for row in fresh_rows(after, 'in'):
                if not row['text'].startswith(prefix):
                    continue
                fields = row['text'][len(prefix):].split()
                if fields[0] == 'part':
                    assert len(fields) == 3 and int(fields[1]) == len(parts), row
                    parts.append(bytes.fromhex(fields[2]))
                    assert sum(map(len, parts)) <= 2048, parts
                else:
                    assert fields[0] == 'end' and int(fields[1]) == len(parts), row
                    return dict(command=sent, payload=json.loads(b''.join(parts)))
            return None
        return wait('directed docking ' + action + ' reply', delivered)

    def preview(label, output=RIGHT, fallback=False):
        started = callback(edit(), 'preview')
        reply = dock_reply(started, 'prepare')
        observed[label] = dict(preview=reply)
        assert reply['payload'] == {'result': dict(
            matched=2, skipped=0, fallback=fallback, monitor=output)}, reply
        token = form_token(started)
        view.wait(PREVIEW)
        return token

    def desktop_notice(started, text):
        line = '*SlateNoticeForm: TextHex x' + text.encode().hex()
        wait('desktop displays docking outcome', lambda: any(
            row['text'] == line for row in fresh_rows(started)))

    def apply(label, token, expected):
        started = callback(token, 'apply')
        reply = dock_reply(started, 'apply')
        observed[label]['apply'] = reply
        assert reply['command'][1] == observed[label]['preview']['command'][1], reply
        assert reply['command'][4] == observed[label]['preview']['command'][2], reply
        assert reply['payload'] == expected, reply
        if 'error' in expected:
            notice = 'Docking placement changed; preview again'
        else:
            value = expected['result']
            monitor = observed[label]['preview']['payload']['result']['monitor']
            notice = ('Docking applied to %d windows. %d skipped. Panel assigned to %s.' %
                (value['applied'], value['skipped'], monitor))
        desktop_notice(started, notice)
        observed[label]['notice'] = notice

    def placement(label, windows, output, expected_sizes, indexes):
        def placed():
            current = measure(windows)
            observed[label]['last_geometry'] = current
            panel = workspace()[output]
            area = panel['dynamic_work_area']
            if panel['desk'] != TARGET_DESK or not area:
                return None
            for i, rect in zip(indexes, current):
                frame = rect['frame']
                if not frame or not contains(area, frame):
                    return None
                if (frame['x'], frame['y']) != (area['x'] + SELECTORS[i]['offset_x'],
                        area['y'] + SELECTORS[i]['offset_y']):
                    return None
            return current
        final = wait('restored geometry on ' + output, placed)
        assert [row['client'] for row in final] == expected_sizes, final
        return final

    def result(applied, skipped=0, fallback=False):
        return {'result': dict(applied=applied, skipped=skipped, fallback=fallback,
            panel_applied=True)}

    stale = {'error': {'code': 'stale-plan'}}

    # Use managed, exact WM_CLASS fixtures in place of the shared setup windows.
    for window in s['windows'][:]:
        retire(window)
    probe.command('DesktopConfiguration per-monitor')
    start = topology('initial_mixed_topology', {LEFT: 4, RIGHT: 8})
    assert start['monitors'][RIGHT]['geometry']['x'] == 1920, start
    set_desk(LEFT, 0)
    set_desk(RIGHT, 0)
    panel_at(LEFT)
    view.pointer.motion(500, 400)
    windows = [make_client('Docking ' + row['instance'], row, 260 + 470*i, 330)
               for i, row in enumerate(SELECTORS)]
    for i, window in enumerate(windows):
        move(window, 260 + 470*i, 330)
    original = measure(windows)

    path = s['user'] / 'docking-profiles' / (PROFILE + '.json')
    assert not path.exists()
    token = edit()
    for selector in SELECTORS:
        callback(token, 'add ' + ' '.join(encoded(selector[key])
            for key in ('instance', 'class', 'offset_x', 'offset_y')))
    started = callback(token, 'save ' + ' '.join((encoded(RIGHT), encoded(TARGET_DESK), 'on')))
    assert form_token(started) != token
    wait('profile published by native desktop module', path.is_file)
    saved = path.read_bytes()
    expected = dict(schema_version=1, name=PROFILE, connector=RIGHT, desk=TARGET_DESK,
        panel=True, windows=SELECTORS)
    assert json.loads(saved) == expected, saved
    assert stat.S_IMODE(path.stat().st_mode) == 0o600
    assert stat.S_IMODE(path.parent.stat().st_mode) == 0o700
    observed['published_profile'] = dict(data=expected, sha256=hashlib.sha256(saved).hexdigest())
    cases['desktop_saves_private_data_only_profile'] = True

    label = 'stale_workspace'
    token = preview(label)
    assert measure(windows) == original
    assert workspace()[RIGHT]['desk'] == 0
    observed[label]['panel_before'] = panel_at(LEFT)
    assert path.read_bytes() == saved
    cases['preview_is_read_only'] = True
    set_desk(RIGHT, 2)
    apply(label, token, stale)
    assert workspace()[RIGHT]['desk'] == 2
    assert measure(windows) == original
    panel_at(LEFT)
    set_desk(RIGHT, 0)
    cases['newer_workspace_refuses_plan'] = True

    label = 'stale_panel'
    token = preview(label)
    # Pin the already visible panel in place, changing its assignment revision.
    probe.command('SlatePanelAssign ' + LEFT)
    panel_at(LEFT)
    assert workspace()[RIGHT]['desk'] == 0
    apply(label, token, stale)
    assert measure(windows) == original
    assert workspace()[RIGHT]['desk'] == 0
    panel_at(LEFT)
    cases['newer_panel_assignment_refuses_plan'] = True

    label = 'stale_settings'
    token = preview(label)
    # An independent writer creates a private settings revision after preview.
    # Restore the original test baseline once the controller refuses Apply.
    settings = s['user'] / 'desktop-state.json'
    assert not settings.exists()
    with os.fdopen(os.open(settings, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600), 'w') as stream:
        # The settings reader accepts an unversioned legacy preference object.
        json.dump({'appearance': 'light'}, stream)
    try:
        started = callback(token, 'apply')
        notice = 'Docking profile or desktop settings changed; preview again'
        desktop_notice(started, notice)
        assert not any(row['text'].startswith('Send_Dock ') for row in fresh_rows(started))
        assert measure(windows) == original
        assert workspace()[RIGHT]['desk'] == 0
        panel_at(LEFT)
        observed[label]['notice'] = notice
    finally:
        settings.unlink()
    cases['newer_desktop_settings_refuse_plan'] = True

    label = 'connected_apply'
    token = preview(label)
    manual = move(windows[0], 430, 470)
    observed[label]['manual'] = manual
    apply(label, token, result(1, 1))
    positioned = placement(label, windows[1:], RIGHT, [original[1]['client']], [1])
    assert frame_rect(client, windows[0]) == manual
    assert client_size(client, windows[0]) == original[0]['client']
    assert workspace()[LEFT]['desk'] == 0
    panel = panel_at(RIGHT)
    assert panel['desk'] == TARGET_DESK
    observed[label].update(windows=positioned, panel=panel)
    cases['newer_window_move_is_preserved'] = True
    cases['apply_restores_workspace_panel_and_client_placement'] = True

    label = 'stale_topology'
    token = preview(label)
    run_required_xrandr(runtime, display, ['--output', LEFT, '--primary', '--output', RIGHT,
        '--off', '--fb', '1920x1080'], 'unplug-profile-connector', evidence['operations'])
    evidence['unplugged_randr'] = server_state(runtime, display, 'unplugged-randr', LEFT, None)
    unplugged = topology('unplugged_topology', {LEFT: 4}, start['epoch'])
    panel_at(LEFT)
    before = measure(windows)
    apply(label, token, stale)
    assert measure(windows) == before

    label = 'primary_fallback'
    # Both allocations are still live after the automatic monitor migration.
    # Establish their new baseline before taking an explicit fallback preview.
    set_desk(LEFT, TARGET_DESK)
    for i, window in enumerate(windows):
        probe.command('WindowId %d MoveToDesk 0 %d' % (window, TARGET_DESK))
        move(window, 270 + 450*i, 430)
    before = measure(windows)
    token = preview(label, LEFT, fallback=True)
    apply(label, token, result(2, fallback=True))
    observed[label]['windows'] = placement(label, windows, LEFT,
        [row['client'] for row in before], [0, 1])
    panel_at(LEFT)
    assert path.read_bytes() == saved
    cases['topology_change_refuses_plan_and_missing_connector_falls_back'] = True

    run_required_xrandr(runtime, display, ['--output', LEFT, '--primary'] +
        output_setup(169, 135), 'replug-profile-connector', evidence['operations'])
    evidence['replugged_randr'] = server_state(runtime, display, 'replugged-randr', LEFT, 169)
    topology('replugged_topology', {LEFT: 4, RIGHT: 8}, unplugged['epoch'])
    old = probe.inventory()
    probe.release()

    # Replace an allocation with the same exact selector but a different title.
    # Restart through FVWM's same-PID exec and start a fresh directed observer.
    retired = windows[0]
    retire(retired)
    view.pointer.motion(500, 400)
    windows[0] = make_client('Docking replacement', SELECTORS[0], 270, 450)
    assert windows[0] != retired
    move(windows[0], 270, 450)
    fresh = runtime.root / 'probe-dock-restart'
    fresh.mkdir(mode=0o700)
    config = s['config']
    config.write_text(config.read_text().replace(str(s['directory']), str(fresh)) +
        '\nDesktopConfiguration per-monitor\n')
    pid = core.process.pid
    restart_at = time.monotonic()
    probe.command('Schedule 1000 Restart')
    wait('fresh observer after WM restart', (fresh / 'ready.json').exists, 15)
    probe = Probe(fresh, core)
    s['probes'].append(probe)
    new = probe.inventory()
    probe.release()
    assert core.process.pid == pid and new['generation'][:32] != old['generation'][:32]
    assert path.read_bytes() == saved
    wait('desktop module catalog published after restart', lambda: any(
        row['text'].startswith('AddToMenu SlateLauncherMenu ') and
        'Palette argv fixture' in row['text'] for row in fresh_rows(restart_at)), 12)

    # The retained file is the only profile input after restart.
    probe.command('DesktopConfiguration per-monitor')
    topology('restart_topology', {LEFT: 4, RIGHT: 8})
    set_desk(LEFT, TARGET_DESK)
    set_desk(RIGHT, 0)
    panel_at(LEFT)
    assert view.wait('Docking replacement') == windows[0]
    before = measure(windows)
    assert all(row['frame']['x'] < 1920 for row in before), before
    label = 'restart_apply'
    token = preview(label)
    assert measure(windows) == before
    apply(label, token, result(2))
    observed[label].update(pid=pid, old_generation=old['generation'],
        new_generation=new['generation'], retired=retired, replacement=windows[0],
        windows=placement(label, windows, RIGHT, [row['client'] for row in before], [0, 1]),
        panel=panel_at(RIGHT))
    assert path.read_bytes() == saved
    cases['restart_reloads_profile_and_resolves_current_allocations'] = True

    # Capture the completed profile editor after the restore cases pass.
    # Its client and visible child controls must fit the selected screen.
    edit()
    editor = view.wait('Docking profile')
    attributes = view.attributes(editor)
    assert attributes is not None
    controls = []
    for child in view.children(editor):
        control = view.attributes(child)
        if control is None or control.map_state != 2:
            continue
        bounds = dict(x=control.x, y=control.y, width=control.width,
            height=control.height)
        assert bounds['x'] >= 0 and bounds['y'] >= 0 and (
            bounds['x'] + bounds['width'] <= attributes.width and
            bounds['y'] + bounds['height'] <= attributes.height), bounds
        controls.append(bounds)
    frame = frame_rect(client, editor)
    assert frame and frame['x'] >= 0 and frame['y'] >= 0
    assert frame['x'] + frame['width'] <= 3200 and frame['y'] + frame['height'] <= 1080, frame
    observed['finished_editor'] = dict(frame=frame,
        client=dict(width=attributes.width, height=attributes.height), controls=controls)
    s['capture']('workspace-docking.png')


def main():
    parser = argparse.ArgumentParser(description=__doc__, allow_abbrev=False)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--artifacts-dir', type=Path, required=True)
    parser.add_argument('--profiles', nargs='+', choices=PROFILES, default=['slate-dark'])
    parser.add_argument('--xorg-config', type=Path,
        default=Path(__file__).with_name('xorg-dummy.conf'))
    args = parser.parse_args()
    if len(set(args.profiles)) != len(args.profiles):
        parser.error('each profile may be selected once')
    config = args.xorg_config.resolve(strict=True)
    args.artifacts_dir.mkdir(parents=True, exist_ok=True)
    root = Path(tempfile.mkdtemp(prefix='workspace-docking-', dir=args.artifacts_dir.resolve()))

    def display_initializer(runtime, evidence):
        source = Path(__file__).resolve().parents[2]
        for name in ('tests/live_x/workspace_docking_live.py', 'fvwm/workspace_dock.c',
                'fvwm/workspace_placement.c', 'default-config/desktop.fvwm',
                'bin/FvwmPrompt/internal/desktopcontroller/docking.go',
                'bin/FvwmPrompt/internal/desktopcontroller/docking_state.go'):
            evidence['source_sha256'][name] = hashlib.sha256((source / name).read_bytes()).hexdigest()
        return start_display(runtime, evidence, config)

    def expired(_signal, _frame):
        raise TimeoutError('workspace docking checkpoint deadline')

    previous = signal.signal(signal.SIGALRM, expired)
    try:
        for profile in args.profiles:
            signal.alarm(195)
            run(args, profile, root, scenario=scene, display_initializer=display_initializer)
            signal.alarm(0)
            path = root / profile / 'evidence.json'
            evidence = json.loads(path.read_text())
            assert evidence['status'] == 'passed' and not evidence['cleanup_failures'], path
            assert not evidence['observer_unexpected_x_errors'], path
            assert evidence['cases'] == dict.fromkeys(CASE_NAMES, True), path
            assert evidence['screenshots'] == ['workspace-docking.png'], path
            assert all(code is not None for code in evidence['children_after_cleanup'].values()), path
            print(path, flush=True)
    finally:
        signal.alarm(0)
        signal.signal(signal.SIGALRM, previous)
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
