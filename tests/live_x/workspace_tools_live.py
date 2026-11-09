#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or any later version.
"""One combined native scratchpad/overview checkpoint; no historical replay."""
import argparse
import ctypes as C
import hashlib
import json
import os
from pathlib import Path
import shlex
import shutil
import signal
import sys
import tempfile
import time

from action_palette_live import PROFILES, Probe, run
from geometry_transition_live import Client, SizeHints, XEvent
from live_x_harness import wait_for_predicate
from native_bindings_live import rows


class ClassHint(C.Structure):
    _fields_ = [('name', C.c_char_p), ('class_', C.c_char_p)]


class MessageData(C.Union):
    _fields_ = [('longs', C.c_long*5), ('bytes', C.c_char*20)]


class ClientMessage(C.Structure):
    _fields_ = [('type', C.c_int), ('serial', C.c_ulong), ('send_event', C.c_int),
                ('display', C.c_void_p), ('window', C.c_ulong),
                ('message_type', C.c_ulong), ('format', C.c_int), ('data', MessageData)]


class MessageEvent(C.Union):
    _fields_ = [('message', ClientMessage), ('padding', C.c_long*24)]


def client_main(args):
    """Independent application; launched through the real Desktop argv path."""
    def record(phase):
        path = Path(args.record)
        stamp = Path('/proc/self/stat').read_text().rsplit(')', 1)[1].split()[19]
        value = dict(phase=phase, pid=os.getpid(), start=stamp,
                     argv=sys.argv[1:], class_=args.class_, instance=args.instance)
        with path.open('a') as stream:
            stream.write(json.dumps(value)+'\n')
    record('launch')
    if args.fork:
        if os.fork():
            return
        record('fork-child')
    time.sleep(args.delay_ms / 1000)
    if args.terminal and Path('/usr/bin/xterm').is_file():
        record('terminal-process')
        os.execv('/usr/bin/xterm', ['xterm', '-class', args.class_, '-name', args.instance,
            '-title', args.title, '-geometry', '48x10', '-e', '/bin/sh', '-c',
            'printf "FVWM-X scratchpad terminal\\n"; exec /bin/sleep 120'])
    client = Client(os.environ['DISPLAY'])
    client.window = client.x.XCreateSimpleWindow(client.display, client.root,
        140, 120, 380, 180, 0, 0, 0x26343d)
    client.x.XSetClassHint.argtypes = [C.c_void_p, C.c_ulong, C.POINTER(ClassHint)]
    hint = ClassHint(args.instance.encode(), args.class_.encode())
    client.x.XSetClassHint(client.display, client.window, C.byref(hint))
    client.x.XStoreName(client.display, client.window, args.title.encode())
    if args.terminal:
        hint = SizeHints(flags=(1 << 4) | (1 << 6), min_width=80, min_height=40,
                         width_inc=8, height_inc=16)
        client.x.XSetWMNormalHints(client.display, client.window, C.byref(hint))
    client.x.XSelectInput(client.display, client.window, (1 << 17) | (1 << 15))
    client.x.XCreateGC.argtypes = [C.c_void_p, C.c_ulong, C.c_ulong, C.c_void_p]
    client.x.XCreateGC.restype = C.c_void_p
    client.x.XSetForeground.argtypes = [C.c_void_p, C.c_void_p, C.c_ulong]
    client.x.XDrawString.argtypes = [C.c_void_p, C.c_ulong, C.c_void_p,
                                    C.c_int, C.c_int, C.c_char_p, C.c_int]
    client.x.XFreeGC.argtypes = [C.c_void_p, C.c_void_p]
    gc = client.x.XCreateGC(client.display, client.window, 0, None)
    client.x.XSetForeground(client.display, gc, 0xe5edf0)
    client.x.XMapWindow(client.display, client.window)
    client.x.XSync(client.display, 0)
    record('window')
    signal.signal(signal.SIGTERM, lambda *_: sys.exit(0))
    try:
        deadline = time.monotonic() + 150
        while time.monotonic() < deadline:
            event = XEvent()
            if client.x.XCheckTypedWindowEvent(client.display, client.window, 17, C.byref(event)):
                client.window = None
                break
            if client.x.XCheckTypedWindowEvent(client.display, client.window, 12, C.byref(event)):
                text = ('FVWM-X fixture: ' + args.title).encode()
                client.x.XDrawString(client.display, client.window, gc, 12, 28, text, len(text))
                client.x.XSync(client.display, 0)
            time.sleep(.03)
    finally:
        client.x.XFreeGC(client.display, gc)
        client.close()


def replies(native):
    """Observe original directed bytes; do not synthesize product responses."""
    chunks, result = {}, []
    for row in rows(native):
        text = row['text']
        if row['direction'] != 'in' or not text.startswith('MFL_CORE_1 '):
            continue
        fields = text.split()
        key = tuple(fields[1:3])
        if len(fields) == 6 and fields[3] == 'part':
            values = chunks.setdefault(key, [])
            assert int(fields[4]) == len(values)
            values.append(bytes.fromhex(fields[5]))
            assert sum(map(len, values)) <= 32768
        elif len(fields) == 5 and fields[3] == 'end' and key in chunks:
            data = chunks.pop(key)
            assert len(data) == int(fields[4])
            value = json.loads(b''.join(data))
            item = value.get('result', {})
            if item.get('version') == 1 and 'name' in item and 'desired_visible' in item:
                result.append(dict(time=row['time'], cookie=key[0], serial=key[1], **item))
    return result


def fixture_records(path, phase=None):
    if not path.exists():
        return []
    result = [json.loads(line) for line in path.read_text().splitlines()]
    return [row for row in result if phase is None or row['phase'] == phase]


def stop_clients(paths):
    stopped = set()
    identities = []
    for path in paths:
        for row in fixture_records(path):
            if row['pid'] in stopped:
                continue
            proc = Path('/proc') / str(row['pid'])
            try:
                stamp = (proc/'stat').read_text().rsplit(')', 1)[1].split()[19]
                command = (proc/'cmdline').read_bytes().split(b'\0')
                matching = str(path).encode() in command or (row['phase'] == 'terminal-process'
                    and command[0] in (b'xterm', b'/usr/bin/xterm')
                    and row['class_'].encode() in command and row['instance'].encode() in command)
                if stamp == row['start'] and matching:
                    os.kill(row['pid'], signal.SIGTERM)
                    stopped.add(row['pid'])
                    identities.append(row)
            except (FileNotFoundError, ProcessLookupError):
                pass
    def gone():
        for row in identities:
            try:
                fields = (Path('/proc')/str(row['pid'])/'stat').read_text().rsplit(')', 1)[1].split()
                if fields[19] == row['start'] and fields[0] != 'Z':
                    return False
            except FileNotFoundError:
                pass
        return True
    wait_for_predicate('owned launched scratchpad clients retired', gone, time.monotonic()+3)
    return sorted(stopped)


# The scene below uses the established palette observer and actual native keys.
OVERVIEW = 'Window and workspace overview'
LITERAL_ARG = 'literal ; $[exec.touch] `unchanged` 100%'


def scene(**s):
    args, profile, runtime, out = (s[k] for k in ('args', 'profile', 'runtime', 'out'))
    client, view, probe, core = (s[k] for k in ('client', 'view', 'probe', 'core'))
    native, evidence = s['native'], s['evidence']
    cases, records = {}, []
    evidence.update(cases=cases, workspace_scene=dict(
        deadline_seconds=600, actual_xid_reuse_qualified=False,
        remaining_gates=['transient matching refusal on a live client', 'large collection reuses accepted palette cap/virtualization coverage',
            'mixed-monitor topology/scale changes', 'physical input/assistive technology',
            'CPU/RSS qualification requires interpretation of recorded observations']))
    source = Path(__file__).resolve()
    evidence['source_sha256']['tests/live_x/workspace_tools_live.py'] = hashlib.sha256(source.read_bytes()).hexdigest()
    deadline = time.monotonic() + 150

    def resources():
        result = {}
        paths = [('core', core.process.pid)]
        for path in native.glob('module-*.json'):
            record = json.loads(path.read_text())
            if Path(record['real']).name == 'FvwmDesktop':
                paths.append(('desktop', record['child']))
        for name, pid in paths:
            try:
                fields = (Path('/proc')/str(pid)/'stat').read_text().rsplit(')', 1)[1].split()
                result[str(pid)] = dict(role=name, start=fields[19], user_ticks=int(fields[11]),
                    system_ticks=int(fields[12]), rss_bytes=int(fields[21])*os.sysconf('SC_PAGE_SIZE'))
            except FileNotFoundError:
                pass
        return result

    def wait(label, predicate, seconds=8):
        return wait_for_predicate(label, predicate,
            min(deadline, time.monotonic()+seconds), process=core)

    def actual(name, after=0, **wanted):
        def found():
            return next((r for r in reversed(replies(native)) if r['name'] == name
                and r['time'] >= after and all(r.get(k) == v for k, v in wanted.items())), None)
        return wait('actual scratchpad reply '+name+' '+str(wanted), found)

    def inventory(label=None):
        result = probe.inventory()
        probe.release()
        if label is None:
            return result
        return next((r for r in result['rows'] if r['kind'] == 'window' and r['label'] == label), None)

    def row(label, **wanted):
        def found():
            item = inventory(label)
            return item if item and all(item.get(k) == v for k, v in wanted.items()) else None
        return wait('actual window metadata '+label+' '+str(wanted), found)

    def focus_is(window):
        focused = view.focus()
        for _ in range(8):
            if focused == window:
                return True
            if focused in (0, 1, client.root):
                return False
            focused = client.parent(focused)
        return False

    def focus_form(title):
        window = view.wait(title)
        probe.command('WindowId %d Focus' % window)
        wait('keyboard focus '+title, lambda: focus_is(window))
        return window

    def dismiss_notice():
        notice = view.named('FVWM-X')
        if notice:
            probe.command('WindowId %d Close' % notice)
            wait('notice retired', lambda: not view.named('FVWM-X'))

    def overview_open():
        probe.command('SendToModule FvwmDesktop overview')
        focus_form(OVERVIEW)
        state = s['settled']()
        assert state['view']['mode'] == 'overview', state
        assert state['counts'][2:] == [0, 0], state
        return state

    def filter_query(text, from_list=False):
        focus_form(OVERVIEW)
        if from_list:
            s['chord']('Tab', 'Shift_L')
        view.key('u', control=True)
        for char in text:
            assert char == ' ' or char.islower() or char.isdigit(), text
            view.key('space' if char == ' ' else char)
        view.key('F3')
        state = s['settled'](query=text)
        assert state['visible'] == 1, state
        return state

    def select_single():
        view.key('Tab')
        view.key('Home')
        view.key('space')

    def close_overview():
        focus_form(OVERVIEW)
        view.key('Escape')
        wait('overview Escape teardown', lambda: not view.named(OVERVIEW))
        assert probe.diagnostic() == {'open': False}

    def launch_count(path):
        return len(fixture_records(path, 'launch'))

    def scratchpad_key(name, symbol):
        started = time.monotonic()
        evidence['workspace_scene'].setdefault('scratchpad_keys', []).append(
            dict(symbol=symbol, name=name, time=started, focus=view.focus()))
        view.key(symbol)
        wait('actual '+symbol+' scratchpad binding callback', lambda: any(
            r['direction'] == 'in' and r['time'] >= started and
            r['text'] == 'scratchpad '+name for r in rows(native)))
        return started

    def definition(name, klass, instance, title, delay, terminal=False, fork=False):
        path = runtime.root / ('scratchpad-'+name+'.jsonl')
        records.append(path)
        argv = [sys.executable, str(source), '--client', '--record', str(path),
            '--class', klass, '--instance', instance, '--title', title,
            '--delay-ms', str(delay), '--literal', LITERAL_ARG]
        if terminal:
            argv.append('--terminal')
        if fork:
            argv.append('--fork')
        return '*FvwmDesktop: Scratchpad '+shlex.join([name, klass, instance, *argv]), path

    term_title, app_title = 'Workspace terminal', 'Workspace application'
    try:
        terminal_available = Path('/usr/bin/xterm').is_file()
        evidence['workspace_scene']['terminal_provider'] = ('xterm' if terminal_available else 'strict-size-hints fixture')
        evidence['workspace_scene']['real_terminal_qualified'] = False
        if not terminal_available:
            evidence['workspace_scene']['remaining_gates'].append('real terminal provider unavailable; terminal-style size-hints client only')
        term_def, term_record = definition('term', 'FVWMWorkspaceTerm', 'term', term_title, 600, terminal=True)
        app_def, app_record = definition('app', 'FVWMWorkspaceApp', 'application', app_title, 120)
        late_def, late_record = definition('late', 'FVWMWorkspaceLate', 'late', 'Workspace delayed application', 3000, fork=True)
        local = s['user']/'local.config'
        local.write_text(local.read_text()+'\n'+term_def+'\n'+app_def+'\n'+late_def+
            '\n*FvwmDesktop: Scratchpad missing FVWMWorkspaceMissing - /fvwm-workspace-fixture-no-such-program\n'
            'Key F10 A N SendToModule FvwmDesktop scratchpad term\n'
            'Key F11 A N SendToModule FvwmDesktop scratchpad app\n')
        # The shared scene hook runs after startup. Restart only the owned
        # helper to read its actual module configuration; no state is injected.
        previous = {r['cookie'] for r in replies(native)}
        probe.command('Read '+shlex.quote(str(local)), 'KillModule FvwmDesktop', 'Module FvwmDesktop')
        wait('new helper reads scratchpad definitions', lambda: next((r for r in replies(native)
            if r['name'] == 'term' and r['cookie'] not in previous), None), 12)
        prior = view.wait('Palette prior focus')
        probe.command('GotoDesk 0 0', 'GotoPage 0 0', 'WindowId %d Focus' % prior)
        wait('scratchpad prior focus', lambda: focus_is(prior))
        started = time.monotonic()
        probe.command(*(['SendToModule FvwmDesktop scratchpad term']*4))
        grant = actual('term', started, action='launch')
        hidden = actual('term', started, state='claimed', desired_visible=False)
        row(term_title, iconified=True)
        assert launch_count(term_record) == 1
        assert len([r for r in replies(native) if r['name'] == 'term' and r['action'] == 'launch']) == 1
        first = fixture_records(term_record, 'launch')[0]
        assert first['argv'][first['argv'].index('--literal')+1] == LITERAL_ARG, first
        cases['literal_terminal_single_launch_and_rapid_toggle_coalescing'] = True
        started = scratchpad_key('term', 'F10')
        shown = actual('term', started, action='summoned')
        terminal = view.wait(term_title)
        if terminal_available:
            terminal_process = fixture_records(term_record, 'terminal-process')
            assert len(terminal_process) == 1
            command = (Path('/proc')/str(terminal_process[0]['pid'])/'cmdline').read_bytes().split(b'\0')
            assert command[0] in (b'xterm', b'/usr/bin/xterm'), command
            evidence['workspace_scene']['real_terminal_qualified'] = True
        wait('terminal receives focus', lambda: focus_is(terminal))
        before = row(term_title, iconified=False)
        workspace = probe.query('workspaces', limit=8)['result']
        work = workspace['monitors'][0]['dynamic_work_area']
        assert work is not None
        assert abs(2*before['x']+before['width']-(2*work['x']+work['width'])) <= 2, (before, work)
        assert abs(2*before['y']+before['height']-(2*work['y']+work['height'])) <= 2, (before, work)
        started = scratchpad_key('term', 'F10')
        actual('term', started, action='dismissed')
        wait('scratchpad dismissal restores prior focus', lambda: focus_is(prior))
        scratchpad_key('term', 'F10')
        row(term_title, iconified=False)
        after = row(term_title)
        assert (before['width'], before['height']) == (after['width'], after['height'])
        cases['summon_center_preserves_size_and_dismiss_restores_focus'] = True
        started = scratchpad_key('app', 'F11')
        actual('app', started, action='launch')
        actual('app', started, state='claimed')
        application = view.wait(app_title)
        app_before = row(app_title)
        assert launch_count(app_record) == 1
        evidence['scratchpad_main'] = dict(grant=grant, hidden=hidden, shown=shown,
            before=before, after=after, work_area=work,
            terminal_pixels=view.pixels(terminal), application_pixels=view.pixels(application))
        s['capture']('workspace-scratchpads.png')
        cases['literal_application_launch'] = True

        overview_open()
        filter_query('workspace application')
        select_single()
        view.key('Return')
        wait('overview Enter focuses selected managed window', lambda: not view.named(OVERVIEW) and focus_is(application))
        cases['keyboard_search_and_window_activation'] = True

        # Esc must still return focus when no action was intentionally submitted.
        probe.command('WindowId %d Focus' % prior)
        overview_open()
        close_overview()
        wait('overview Cancel restores prior focus', lambda: focus_is(prior))
        cases['overview_escape_focus_return'] = True
        overview_open()
        selected = filter_query('workspace application')
        select_single()
        view.key('F6')
        marked = wait('actual F6 marks current window', lambda: (v if (v := probe.diagnostic())['open']
            and v['view'].get('marked') else None))
        assert marked['view']['marked']['id'], marked
        s['capture']('workspace-overview-marked.png')
        filtered = filter_query('palette desk five', from_list=True)
        select_single()
        moved_at = time.monotonic()
        view.key('F8')
        wait('actual F8 move closes overview', lambda: not view.named(OVERVIEW))
        moved = row(app_title, desk=5, page_x=0, page_y=0)
        assert (moved['width'], moved['height']) == (app_before['width'], app_before['height']), (app_before, moved)
        commands = [r['text'] for r in rows(native) if r['direction'] == 'out' and r['time'] >= moved_at
            and r['text'].startswith('Send_Palette ') and ' move ' in r['text']]
        assert commands and commands[-1].endswith(' 0 0'), commands
        evidence['overview_move'] = dict(selected=selected, marked=marked, destination=filtered,
            observed=moved, commands=commands)
        cases['keyboard_search_mark_move_to_workspace_page'] = True

        probe.command('GotoDesk 0 5', 'GotoPage 0 0')
        overview_open()
        filter_query('workspace application')
        select_single()
        original = row(app_title)
        pointer_at = time.monotonic()
        view.key('F7')
        wait('actual native pointer Move submitted', lambda: next((r for r in rows(native)
            if r['direction'] == 'out' and r['time'] >= pointer_at and
            r['text'].startswith('Send_Palette ') and r['text'].endswith(' drag')), None))
        wait('shared Move owns a real pointer grab', view.pointer.grabbed)
        x, y, _ = view.pointer.position()
        view.pointer.motion(min(1300, x+90), min(1050, y+70))
        view.pointer.button(True)
        view.pointer.button(False)
        wait('pointer Move releases grab', lambda: not view.pointer.grabbed())
        wait('pointer action retires overview', lambda: not view.named(OVERVIEW))
        changed = wait('actual frame moved by native pointer operation', lambda: (r if
            (r := inventory(app_title)) and (r['x'], r['y']) != (original['x'], original['y']) else None))
        assert (changed['width'], changed['height']) == (original['width'], original['height'])
        dismiss_notice()
        evidence['overview_pointer_move'] = dict(before=original, after=changed,
            completion='observed frame movement in this fixture; product reply only certifies submission')
        cases['native_pointer_move_with_observed_geometry'] = True
        probe.command('GotoDesk 0 0', 'GotoPage 0 0', 'SendToModule FvwmDesktop scratchpad app')
        row(app_title, desk=0)

        probe.command('SendToModule FvwmDesktop scratchpads')
        def manager_page(name, since):
            button = f'*SlateScratchpadsFormButton continue "Toggle {name}"'
            for path in native.glob('module-*.json'):
                module = json.loads(path.read_text())
                if (module['started'] < since or
                        module['arguments'][-1:] != ['SlateScratchpadsForm']):
                    continue
                log = path.with_suffix('.jsonl')
                if log.exists() and any(json.loads(line)['text'] == button
                                        for line in log.read_text().split('\n')[:-1]):
                    return view.named('Scratchpads')
            return None

        focus_form('Scratchpads')
        manager_at = time.monotonic()
        view.key('F2')
        wait('manager actual Next callback', lambda: any(r['direction'] == 'in' and r['time'] >= manager_at
            and r['text'].startswith('scratchpad-control ') and r['text'].endswith(' page 1') for r in rows(native)))
        # A replacement X connection can receive the same window ID.
        wait('replacement manager received app page', lambda: manager_page('app', manager_at))
        focus_form('Scratchpads')
        s['capture']('workspace-scratchpad-manager.png')
        previous_at = time.monotonic()
        view.key('F1')
        wait('manager actual Previous callback', lambda: any(r['direction'] == 'in' and r['time'] >= previous_at
            and r['text'].startswith('scratchpad-control ') and r['text'].endswith(' page 0') for r in rows(native)))
        wait('replacement manager received terminal page', lambda: manager_page('term', previous_at))
        focus_form('Scratchpads')
        refresh_at = time.monotonic()
        view.key('F5')
        wait('manager Refresh actual callback', lambda: any(r['direction'] == 'in' and r['time'] >= refresh_at
            and r['text'].startswith('scratchpad-control ') and r['text'].endswith(' refresh') for r in rows(native)))
        view.key('Escape')
        wait('scratchpad manager Escape teardown', lambda: not view.named('Scratchpads'))
        cases['paged_scratchpad_manager_keyboard_and_close'] = True

        if profile == 'slate-dark':
            evidence['workspace_scene']['resources_before'] = resources()
            probe.command('WindowId %d StickAcrossDesks on' % application)
            sticky = row(app_title, sticky_desks=True)
            started = time.monotonic()
            probe.command('SendToModule FvwmDesktop scratchpad app')
            refused_sticky = actual('app', started, state='unsupported', action='none')
            assert row(app_title, sticky_desks=True)['id'] == sticky['id']
            assert launch_count(app_record) == 1
            probe.command('WindowId %d StickAcrossDesks off' % application)
            row(app_title, sticky_desks=False)
            evidence['sticky_refusal'] = dict(reply=refused_sticky, observed=sticky)
            cases['sticky_refusal_preserves_flags_and_does_not_launch'] = True
            # Use a real application EWMH request, not a test-only core flag.
            client.x.XInternAtom.argtypes = [C.c_void_p, C.c_char_p, C.c_int]
            client.x.XInternAtom.restype = C.c_ulong
            client.x.XSendEvent.argtypes = [C.c_void_p, C.c_ulong, C.c_int, C.c_long, C.POINTER(MessageEvent)]
            def fullscreen(enabled):
                event = MessageEvent()
                event.message.type, event.message.display = 33, client.display
                event.message.window = application
                event.message.message_type = client.x.XInternAtom(client.display, b'_NET_WM_STATE', 0)
                event.message.format = 32
                atom = client.x.XInternAtom(client.display, b'_NET_WM_STATE_FULLSCREEN', 0)
                event.message.data.longs[:] = [int(enabled), atom, 0, 1, 0]
                assert client.x.XSendEvent(client.display, client.root, 0, (1<<19)|(1<<20), C.byref(event))
                client.x.XSync(client.display, 0)
            fullscreen(True)
            full = row(app_title, fullscreen=True)
            started = time.monotonic()
            probe.command('SendToModule FvwmDesktop scratchpad app')
            refused_full = actual('app', started, state='unsupported', action='none')
            unchanged = row(app_title, fullscreen=True)
            assert all(full[k] == unchanged[k] for k in ('x','y','width','height'))
            assert launch_count(app_record) == 1
            fullscreen(False)
            row(app_title, fullscreen=False)
            evidence['fullscreen_refusal'] = dict(reply=refused_full, before=full, after=unchanged)
            cases['fullscreen_refusal_preserves_flags_and_does_not_launch'] = True
            started = time.monotonic()
            probe.command('SendToModule FvwmDesktop scratchpad missing')
            actual('missing', started, action='launch')
            actual('missing', started, state='unmatched')
            assert any(r['direction'] == 'out' and r['time'] >= started and ' launch-failed ' in r['text']
                and r['text'].startswith('Send_Scratchpad ') for r in rows(native))
            dismiss_notice()
            cases['definite_missing_executable_acknowledged'] = True
            started = time.monotonic()
            probe.command('SendToModule FvwmDesktop scratchpad late')
            lease = actual('late', started, action='launch')
            # A core grant can be observed before Go starts the application.
            # Fence the actual forked child before testing helper loss.
            child = wait('actual delayed fork child started', lambda: next(iter(
                fixture_records(late_record, 'fork-child')), None))
            proc = Path('/proc')/str(child['pid'])
            assert (proc/'stat').read_text().rsplit(')', 1)[1].split()[19] == child['start']
            assert str(late_record).encode() in (proc/'cmdline').read_bytes().split(b'\0')
            assert not fixture_records(late_record, 'window'), 'delayed application already mapped before helper-loss checkpoint'
            probe.command('KillModule FvwmDesktop', 'Module FvwmDesktop')
            uncertain = actual('late', started, state='uncertain')
            assert uncertain['cookie'] != lease['cookie']
            claimed = actual('late', started, state='claimed')
            assert launch_count(late_record) == 1
            assert len([r for r in replies(native) if r['name'] == 'late' and r['action'] == 'launch']) == 1
            evidence['helper_restart_delayed_fork'] = dict(grant=lease, child_before_helper_loss=child,
                uncertain=uncertain, claimed=claimed)
            cases['helper_restart_and_forked_delayed_launch_no_duplicate'] = True

            # A stale target is refused against an actual destroyed allocation.
            stale = probe.inventory()
            target = next(r for r in stale['rows'] if r['label'] == 'Palette target focus')
            window = view.wait('Palette target focus')
            client.x.XDestroyWindow(client.display, window)
            client.x.XSync(client.display, 0)
            s['windows'].remove(window)
            refused = probe.exchange('Send_Palette', stale['generation'], str(target['index']), 'drag')
            assert 'error' in refused and refused['error']['code'] in ('stale_target', 'stale_generation'), refused
            probe.release()
            evidence['destroyed_overview_target'] = refused
            cases['destroyed_allocation_refuses_stale_action'] = True

            # Schedule ordinary Restart after this probe's command fence. The
            # real same-PID exec reads the modified test-owned config and starts
            # a fresh probe directory, so old queued probe files cannot replay.
            old_inventory = probe.inventory()
            probe.release()
            restart_probe = runtime.root/'probe-wm-restart'
            restart_probe.mkdir(mode=0o700)
            s['config'].write_text(s['config'].read_text().replace(str(s['directory']), str(restart_probe)))
            restart_at = time.monotonic()
            pid = core.process.pid
            probe.command('Schedule 1000 Restart')
            wait('real WM Restart starts fresh native probe', (restart_probe/'ready.json').exists, 15)
            probe = Probe(restart_probe, core)
            s['probes'].append(probe)
            restored = probe.inventory()
            probe.release()
            assert restored['generation'][:32] != old_inventory['generation'][:32]
            assert core.process.pid == pid and core.poll() is None
            missing = actual('missing', restart_at, state='uncertain', action='none')
            existing = actual('app', restart_at, state='claimed')
            assert view.wait(app_title) == application
            assert launch_count(app_record) == 1 and launch_count(term_record) == 1
            evidence['full_wm_restart'] = dict(pid=pid, before_generation=old_inventory['generation'],
                after_generation=restored['generation'], unmatched=missing, existing=existing)
            cases['full_wm_restart_uncertain_unmatched_and_claimable_existing'] = True
            evidence['workspace_scene']['resources_after'] = resources()

        evidence['workspace_scene']['launch_records'] = {p.name: fixture_records(p) for p in records}
        evidence['workspace_scene']['actual_scratchpad_replies'] = replies(native)
        commands = [r['text'] for r in rows(native) if r['direction'] == 'out'
            and r['text'].startswith(('Send_Scratchpad ', 'Send_Palette '))]
        assert commands and max(len(c.encode()) for c in commands) <= 1000
        evidence['workspace_scene']['command_bounds'] = dict(count=len(commands),
            maximum_bytes=max(len(c.encode()) for c in commands), limit=1000)
        assert len(evidence['screenshots']) == 3
    finally:
        # Independent launched clients are not owned by HarnessRuntime's WM
        # process group. Retire only identities and argv recorded by this scene.
        try:
            evidence['workspace_scene']['retired_client_pids'] = stop_clients(records)
        finally:
            for path in records:
                if path.exists():
                    shutil.copy2(path, out/path.name)


def main():
    parser = argparse.ArgumentParser(description=__doc__, allow_abbrev=False)
    parser.add_argument('--client', action='store_true')
    parser.add_argument('--record', type=Path)
    parser.add_argument('--class', dest='class_')
    parser.add_argument('--instance')
    parser.add_argument('--title')
    parser.add_argument('--delay-ms', type=int, default=0)
    parser.add_argument('--literal', default='')
    parser.add_argument('--terminal', action='store_true')
    parser.add_argument('--fork', action='store_true')
    parser.add_argument('--build', type=Path)
    parser.add_argument('--artifacts-dir', type=Path)
    parser.add_argument('--profiles', nargs='+', choices=PROFILES, default=list(PROFILES))
    args = parser.parse_args()
    if args.client:
        assert args.record and args.class_ and args.instance and args.title
        assert 0 <= args.delay_ms <= 10000
        client_main(args)
        return 0
    if args.build is None or args.artifacts_dir is None:
        parser.error('--build and --artifacts-dir are required for the native scene')
    assert len(set(args.profiles)) == len(args.profiles)
    args.artifacts_dir.mkdir(parents=True, exist_ok=True)
    root = Path(tempfile.mkdtemp(prefix='workspace-tools-', dir=args.artifacts_dir.resolve()))
    (root/'run.json').write_text(json.dumps(dict(profiles=args.profiles,
        build=str(args.build.resolve()), source=str(Path(__file__).resolve().parents[2]),
        hard_seconds=600, started_unix=time.time()), indent=2)+'\n')
    def deadline(_number, _frame):
        raise TimeoutError('workspace tools combined 600-second deadline')
    signal.signal(signal.SIGALRM, deadline)
    signal.alarm(600)
    try:
        for profile in args.profiles:
            run(args, profile, root, scenario=scene)
            result = json.loads((root/profile/'evidence.json').read_text())
            assert result['status'] == 'passed' and not result['cleanup_failures'], result
            assert result['cases'] and all(result['cases'].values()), result
            assert not result.get('observer_unexpected_x_errors'), result
            print('workspace tools '+profile+' passed: '+str(root/profile), flush=True)
    finally:
        signal.alarm(0)
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
