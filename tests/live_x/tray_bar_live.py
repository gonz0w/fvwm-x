#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the GNU General Public License, version 2 or any later version.
"""Observe the shipped bar's optional root-child tray popup and stable pages.

Run once against one coherent build/stage on a disposable display. The old
standalone fixture remains a separate regression. Foreign client observations,
actual Buttons geometry/input and X ancestry are independent of tray Status and
requested settings. Source authoring alone is not an executed pass.
"""
import argparse
import ctypes as C
import json
import os
from pathlib import Path
import shutil
import signal
import time
import traceback

from live_x_harness import (HarnessRuntime, start_display_server,
    start_xorg_dummy, wait_for_randr, run_required_xrandr,
    wait_for_authoritative_state, wait_for_predicate)
from legacy_property_live import Event
from mfl_stream_live import Connection
from session_services_live import Session
from tray_live import Native, Peer, digest, released, wait_state


TRAY_KEYS = {'tray_enabled', 'tray_gap', 'tray_padding', 'tray_orientation',
    'tray_modern', 'tray_icon_size'}


class MapEvent(C.Structure):
    _fields_ = [('type', C.c_int), ('serial', C.c_ulong),
        ('synthetic', C.c_int), ('display', C.c_void_p),
        ('event', C.c_ulong), ('window', C.c_ulong),
        ('override_redirect', C.c_int)]


def wait(label, observed, session):
    return wait_for_predicate(label, observed, time.monotonic() + 8,
        process=session.process)


class Bar(Native):
    """Use the real session/controller while reusing bounded X observations."""
    def __init__(self, runtime, options, out):
        self.peers, self.handles = [], []
        self.tray_alias = 'SlateTray'
        # Session owns startup, optional services remain off. No adapter is
        # replaced and all module links point into this caller's candidate.
        Session.__init__(self, runtime, options, out, 'tray-bar',
            bus=getattr(options, 'session_bus', 'off'),
            address=getattr(options, 'session_address', None),
            extra_env=getattr(options, 'session_extra_env', None))
        self.evidence.update(cases={}, captures=[], limits=[
            'Disposable synthetic X clients; no toolkit or physical input claim.',
            'Root-child popup, not containment or StatusNotifier support.',
            'Linux pidfd identity required for abrupt bar-exit observation.',
            'Xlib calls are synchronous; event budgets do not preempt XIO.'])
        self.x.XGetWindowProperty.argtypes = [C.c_void_p, C.c_ulong,
            C.c_ulong, C.c_long, C.c_long, C.c_int, C.c_ulong,
            C.POINTER(C.c_ulong), C.POINTER(C.c_int), C.POINTER(C.c_ulong),
            C.POINTER(C.c_ulong), C.POINTER(C.POINTER(C.c_ubyte))]
        self.x.XGetWindowProperty.restype = C.c_int
        self.pointer.x.XTestFakeKeyEvent.argtypes = [C.c_void_p, C.c_uint,
            C.c_int, C.c_ulong]
        self.pointer.x.XTestFakeKeyEvent.restype = C.c_int
        self.core_handle = None
        if Path('/proc').is_dir() and hasattr(os, 'pidfd_open'):
            from session_recovery_live import OwnedCore
            self.core_handle = OwnedCore(self.process, options.fvwm)
        application = self.create('Tray popup managed focus witness')
        self.app = application['window']
        self.focus_app()
        self.peer_client = self.peer('bar-plugs')
        assert self.peer_client.request('owner')['owner'] == options.initial_owner
        self.x.XSelectInput(self.d, self.client.root, (1 << 19) | (1 << 17))
        self.x.XSync(self.d, 0)
        witness = self.peer_client.create(name='Root MapNotify observer witness')
        self.peer_client.request('map', id=witness)
        self.peer_client.request('unmap', id=witness)
        self.peer_client.request('destroy', id=witness)
        observed = self.root_maps()
        assert witness in observed, (witness, observed)
        self.evidence['root_map_observer_positive_witness'] = dict(
            window=witness, observed=observed)

    def root_maps(self):
        self.x.XSync(self.d, 0)
        event, windows = Event(), []
        for _ in range(512):
            if not self.x.XCheckTypedWindowEvent(self.d, self.client.root,
                    19, C.byref(event)):
                return windows
            mapped = C.cast(C.byref(event), C.POINTER(MapEvent)).contents
            assert mapped.type == 19 and mapped.event == self.client.root
            windows.append(mapped.window)
        raise AssertionError('root MapNotify observer event budget exhausted')

    def tray(self, command):
        self.command('SendToModule SlateTray ' + command)

    def fence(self, predicate=lambda state: True):
        # A fresh echo subscriber has no earlier Status queued. The newly
        # submitted Status follows this fixture's commands on the module pipe.
        endpoint = self.home / ('fvwm_mfl_' +
            self.display.replace(':', '_') + '.sock')
        old = self.control
        fresh = Connection(endpoint)
        try:
            assert 'connection_profile' in fresh.record()
            fresh.send(b'set echo\n')
            fresh.echo('tray-fresh-status-subscriber')
            self.control = fresh
            return self.wait_status(predicate)
        finally:
            self.control = old
            fresh.close()

    def observe_owner(self):
        owner = wait('actual reserved tray owner', lambda:
            self.peer_client.request('owner')['owner'], self)
        self.state = self.fence(lambda value: value['owner'] == owner and
            value['claimed'] and not value['pending'])
        assert self.property(owner, '_FVWM_TRAY_HOST', 'WINDOW') == [
            self.state['host']]
        assert self.client.parent(self.state['host']) == self.client.root
        return self.state

    def setting_snapshot(self):
        snapshot = self.request(resource='settings', scope='group:tray')
        assert set(snapshot['settings']) == TRAY_KEYS, snapshot
        for item in snapshot['settings'].values():
            assert item['readiness'] == 'unobserved', item
            assert item['application'] == 'requested-state-submitted', item
        return snapshot

    def button_rect(self):
        self.serial += 1
        token = 'tray-button-rect-' + str(self.serial)
        self.control.send(('SendToModule SlateBar ExpandButtonVars tray Echo ' +
            token + ' $left $top $width $height\n').encode())
        deadline = time.monotonic() + 8
        previous = self.control.socket.gettimeout()
        try:
            for _ in range(128):
                assert time.monotonic() < deadline, 'Buttons geometry deadline'
                self.control.socket.settimeout(max(.001,
                    deadline - time.monotonic()))
                record = self.control.record()
                message = record.get('echo', {}).get('message', '')
                if message.startswith(token + ' '):
                    fields = list(map(int, message.split()[1:]))
                    assert len(fields) == 4
                    result = dict(zip(('x', 'y', 'width', 'height'), fields))
                    assert result['width'] > 0 and result['height'] > 0
                    return result
            raise AssertionError('Buttons geometry reply record limit')
        finally:
            self.control.socket.settimeout(previous)

    def click_rect(self, rect):
        self.pointer.motion(rect['x'] + rect['width'] // 2,
            rect['y'] + rect['height'] // 2)
        self.pointer.button(True)
        self.pointer.button(False)

    def button(self, visible):
        first_rect = self.button_rect()
        # The first Buttons echo follows its ready callback. The correlated
        # settings reply follows that callback's refresh/Enabled commands; the
        # second Buttons echo processes those commands before returning geometry.
        settings = self.setting_snapshot()
        rect = self.button_rect()
        ready = self.fence(lambda value: not value['pending'])
        self.evidence.setdefault('button_readiness_fences', []).append(dict(
            initial_rect=first_rect, settings_generation=settings['generation'],
            ready_native=ready, actual_click_rect=rect))
        self.click_rect(rect)
        try:
            wait('actual tray button visibility', lambda:
                (self.attrs(self.state['host']).map_state == 2) == visible, self)
        except Exception as original:
            diagnostic = dict(status='diagnostic-only-original-failure-retained',
                original_error=repr(original), requested_visible=visible,
                clicked_rect=rect, theme=self.evidence.get('active_theme_case'))
            records = self.evidence.setdefault('button_failure_diagnostics', [])
            records.append(diagnostic)
            number = len(records)
            def record(name, operation):
                try:
                    value = operation()
                    diagnostic[name] = value
                    return value
                except Exception as error:
                    diagnostic[name + '_error'] = repr(error)
                    return None
            try:
                record('before', self.button_diagnostic_snapshot)
                record('before_capture', lambda: self.capture(
                    'button-failure-%d-before.png' % number))
                # PressButton retains the current Enabled/Disabled policy but
                # bypasses pointer hit testing. Observe it before changing that
                # policy so the two failure causes remain distinguishable.
                record('press_button_without_enable', lambda: self.command(
                    'SendToModule SlateBar PressButton tray'))
                record('host_mapped_after_press_button', lambda:
                    wait('diagnostic tray host maps after PressButton',
                        lambda: bool(self.rect(self.state['host'])), self))
                record('after_press_button', self.button_diagnostic_snapshot)
                # Diagnostic intervention only: one explicit runtime enable,
                # fresh actual geometry and one real click. It cannot turn the
                # failed product expectation into a fixture pass.
                record('explicit_enable', lambda: self.command(
                    'SendToModule SlateBar ChangeButton tray Enabled 1'))
                retry = record('fresh_rect', self.button_rect)
                if retry:
                    record('one_reclick', lambda: self.click_rect(retry))
                    record('host_mapped_after_enable_reclick', lambda:
                        wait('diagnostic tray host maps after enable/reclick',
                            lambda: bool(self.rect(self.state['host'])), self))
                record('after', self.button_diagnostic_snapshot)
                record('after_capture', lambda: self.capture(
                    'button-failure-%d-after.png' % number))
            except Exception as error:
                diagnostic['unexpected_diagnostic_error'] = repr(error)
            raise
        return rect

    def button_diagnostic_snapshot(self):
        status = self.fence()
        host = self.attrs(status['host'])
        bars = self.named_windows('SlateBar')
        root, child = C.c_ulong(), C.c_ulong()
        x, y, wx, wy = (C.c_int() for _ in range(4))
        mask = C.c_uint()
        pointer = bool(self.x.XQueryPointer(self.d, self.client.root,
            C.byref(root), C.byref(child), C.byref(x), C.byref(y),
            C.byref(wx), C.byref(wy), C.byref(mask)))
        return dict(native=status, actual_host=({key: getattr(host, key)
            for key in ('x', 'y', 'width', 'height', 'map_state',
                'override_redirect')} if host else None),
            popup_rect=self.rect(status['host']), named_bars=bars,
            bar_rects={str(window): self.rect(window) for window in bars},
            pointer=dict(same_screen=pointer, root=root.value, child=child.value,
                x=x.value, y=y.value, mask=mask.value))

    def hidden(self):
        return self.attrs(self.state['host']).map_state == 0

    def actual_clients(self, identifiers):
        values = [self.peer_client.state(identifier) for identifier in identifiers]
        assert all(value['exists'] for value in values), values
        return values

    def visible(self, identifiers):
        return tuple(identifier for identifier, value in zip(identifiers,
            self.actual_clients(identifiers)) if value['map'] == 2)

    def page(self, identifiers, command):
        before = self.visible(identifiers)
        self.tray(command)
        self.fence()
        after = wait('actual changed page', lambda:
            (lambda value: value if value and value != before else None)(
                self.visible(identifiers)), self)
        assert self.client.parent(self.state['host']) == self.client.root
        return after

    def process_handle(self, executable, alias):
        if not self.core_handle:
            return None
        from session_bus_lifetime_live import OwnedProcess, children, argv
        parent = self.core_handle.sample()['pid']
        if alias == 'SlateTray':
            values = self.property(self.state['owner'], '_NET_WM_PID', 'CARDINAL')
            assert len(values) == 1 and values[0] > 0, values
            # The owner property locates the process; direct PPID, executable
            # dev/inode, birth and pidfd independently authenticate it.
            handle = OwnedProcess(values[0], parent, executable)
            assert alias in handle.arguments, handle.arguments
            self.handles.append(handle)
            return handle
        def capture():
            for pid in children(parent):
                try:
                    arguments = argv(pid)
                    if alias in arguments and Path('/proc/%d/exe' % pid).stat(
                            ).st_ino == executable.stat().st_ino:
                        return OwnedProcess(pid, parent, executable)
                except (FileNotFoundError, ProcessLookupError):
                    pass
            return None
        handle = wait('exact owned ' + alias + ' process', capture, self)
        self.handles.append(handle)
        return handle

    def candidate_started(self, label, exclude=()):
        if not self.core_handle:
            self.evidence.setdefault('unavailable', []).append(
                label + ': exact candidate lifetime needs Linux proc/pidfd.')
            return None
        from session_bus_lifetime_live import OwnedProcess, children, argv
        parent = self.core_handle.sample()['pid']
        expected = self.options.tray.stat()
        def capture():
            for pid in children(parent):
                if pid in exclude:
                    continue
                try:
                    arguments = argv(pid)
                    actual = Path('/proc/%d/exe' % pid).stat()
                    if ('SlateTray' not in arguments or
                            (actual.st_dev, actual.st_ino) !=
                            (expected.st_dev, expected.st_ino)):
                        continue
                    return OwnedProcess(pid, parent, self.options.tray)
                except (FileNotFoundError, ProcessLookupError):
                    continue  # A different known core child may retire.
            return None
        # The native two-second ownership grace lets this independent observer
        # acquire the exact candidate before waiting for settings readiness.
        handle = wait('actual new candidate ' + label, capture, self)
        self.handles.append(handle)
        return handle

    @staticmethod
    def candidate_exited(handle):
        if handle is None:
            return dict(status='unavailable', terminal_lifetime_observed=False)
        return dict(status='observed', terminal_lifetime_observed=True,
            executable=handle.executable, arguments=handle.arguments,
            **handle.exited(seconds=5))

    def close(self):
        for name, peer in self.peers:
            (self.out / (name + '-wire.json')).write_text(
                json.dumps(peer.records, indent=2) + '\n')
            outcome = peer.close()
            self.evidence.setdefault('peer_cleanup', {})[name] = outcome
            assert outcome == {'exit': 0, 'forced_cleanup': False}, outcome
        for handle in self.handles:
            handle.close()
        # Core is stopped by the session wrapper/runtime, not this borrowed
        # observer handle. Close only its descriptor, preserving owned teardown.
        if getattr(self, 'core_handle', None) and self.core_handle.fd is not None:
            os.close(self.core_handle.fd)
            self.core_handle.fd = None
        Session.close(self)


def anchored(session, anchor, expected_quarters=None):
    session.focus_app()
    session.tray('ToggleAt {x} {y} {width} {height}'.format(**anchor))
    state = session.fence(lambda value: not value['pending'] and
        (expected_quarters is None or value['quarters'] == expected_quarters))
    rect = wait('visible anchored popup', lambda:
        session.rect(state['host']), session)
    assert session.client.parent(state['host']) == session.client.root
    assert session.attrs(state['host']).override_redirect
    assert session.focus() == session.app
    return dict(anchor=anchor, actual=rect, native=state)


def popup_cases(session, evidence):
    peer = session.peer_client
    initial = session.setting_snapshot()
    assert initial['settings']['tray_enabled']['saved'] is False
    # Startup off must leave a later independent owner intact, including an
    # ordinary theme/bar refresh; absence of a ready owner is not fabricated.
    competitor = peer.request('claim')['owner']
    session.command('SlatePanel')
    session.command('SendToModule FvwmDesktop set tray_enabled on')
    candidate = session.candidate_started('enable while independently owned')
    session.barrier()
    refused_attempt = session.candidate_exited(candidate)
    competing = session.setting_snapshot()
    assert competing['settings']['tray_enabled']['requested'] is True
    assert peer.request('owner')['owner'] == competitor
    session.change({'tray_enabled': False})
    assert peer.request('owner')['owner'] == competitor
    peer.request('release')
    session.change({'tray_enabled': True})
    state = session.observe_owner()
    assert session.hidden()
    owner, host = state['owner'], state['host']
    initial_maps = session.root_maps()
    assert host not in initial_maps, (host, initial_maps)
    evidence['cases']['saved_enablement_and_competing_owner'] = dict(
        initial=initial, requested_while_competing=competing,
        independent_owner=competitor, completed_candidate=refused_attempt,
        observed_ready=state, initial_root_map_events=initial_maps,
        hidden_after_initial_config=True, mapped_before_config=False)

    identifiers = [peer.create(width=24, height=20, name='Bar mail'),
        peer.create(width=30, height=26, name='Bar network')]
    for identifier in identifiers:
        peer.request('dock', id=identifier)
    session.fence(lambda value: value['count'] == 2 and value['mapped'] == 2)
    values = wait('embedded hidden popup clients', lambda:
        (lambda values: values if all(value['parent'] != peer.identity['root']
            and value['map'] != 2 for value in values) else None)(
                session.actual_clients(identifiers)), session)
    sockets = [value['parent'] for value in values]
    for identifier, socket in zip(identifiers, sockets):
        peer.message(identifier, 0, data1=socket, data2=0)
        assert session.client.parent(socket) == host
    original_process = session.process_handle(session.options.tray, 'SlateTray')
    original_pid = session.property(owner, '_NET_WM_PID', 'CARDINAL')[0]
    session.command('SendToModule FvwmDesktop set tray_enabled on')
    repeated_candidate = session.candidate_started('repeated enable',
        exclude=(original_pid,))
    session.barrier()
    repeated_exit = session.candidate_exited(repeated_candidate)
    repeated_state = session.fence(lambda value: not value['pending'])
    assert peer.request('owner')['owner'] == owner
    assert [value['parent'] for value in session.actual_clients(identifiers)] == sockets
    if original_process:
        original_process.sample()
    previous_provider = session.provider
    session.command('KillModule FvwmDesktop\nModule FvwmDesktop')
    restart_candidate = session.candidate_started('controller restart',
        exclude=(original_pid,))
    def controller_ready():
        snapshot = session.ready()
        return snapshot if snapshot and snapshot['provider'] != previous_provider else None
    restarted_settings = wait('new actual settings owner after controller restart',
        controller_ready, session)
    session.provider = restarted_settings['provider']
    restart_exit = session.candidate_exited(restart_candidate)
    restarted_state = session.fence(lambda value: not value['pending'])
    assert peer.request('owner')['owner'] == owner
    assert [value['parent'] for value in session.actual_clients(identifiers)] == sockets
    if original_process:
        original_process.sample()
    evidence['cases']['repeated_enable_and_controller_restart_retain_owner'] = dict(
        repeated_native=repeated_state, restarted_native=restarted_state,
        repeated_candidate=repeated_exit, restart_candidate=restart_exit,
        original_owner=owner, actual_clients=session.actual_clients(identifiers),
        original_process=original_process.initial if original_process else None,
        old_settings_provider=previous_provider,
        new_settings_provider=session.provider)
    button = session.button(True)
    rect = session.rect(host)
    assert rect['y'] >= button['y'] + button['height'], (button, rect)
    assert session.focus() == session.app
    session.tray('Focus')
    peer.message(identifiers[0], 4)
    wait('actual tray proxy focus', lambda:
        session.focus() == state['proxy'], session)
    session.press('Escape')
    wait('anchored Escape hides and restores managed focus', lambda:
        session.hidden() and session.focus() == session.app, session)
    assert session.actual_clients(identifiers)[0]['parent'] == sockets[0]
    reopened_button = session.button(True)
    session.capture('slate-popup.png')
    evidence['cases']['actual_buttons_activation_escape_reopen'] = dict(
        button=button, reopened_button=reopened_button, host=host,
        actual_client_states=session.actual_clients(identifiers),
        socket_ids=sockets, root_parent=session.client.parent(host))

    before = dict(rect=session.rect(host), clients=session.actual_clients(identifiers))
    invalid = ('ToggleAt', 'ToggleAt 1 2 3', 'ToggleAt 1 2 3 4 5',
        'ToggleAt 1,2 3 4', 'ToggleAt 0 0 0 5', 'ToggleAt 0 0 1 -1',
        'ToggleAt 32767 0 1 1', 'ToggleAt -32769 0 1 1',
        'ToggleAt 99999999999999999999999999 0 1 1',
        'PageNext extra', 'PagePrev extra')
    for command in invalid:
        session.tray(command)
        session.fence(lambda value: value['count'] == 2)
        assert dict(rect=session.rect(host),
            clients=session.actual_clients(identifiers)) == before, command
    evidence['cases']['invalid_native_controls_atomic'] = dict(
        commands=invalid, actual_before=before, witness='fresh Status after command')

    session.tray('Hide')
    wait('explicit Hide', session.hidden, session)
    root = session.attrs(session.client.root)
    edge = dict(x=root.width - 5, y=root.height - 5, width=4, height=4)
    placed = anchored(session, edge)
    popup = placed['actual']
    assert popup['x'] + popup['width'] <= root.width
    assert popup['y'] + popup['height'] <= edge['y']
    session.tray('Hide')
    wait('hide before negative-origin anchor', session.hidden, session)
    negative = anchored(session, dict(x=-20, y=-10, width=32, height=20))
    assert negative['actual']['x'] >= 0 and negative['actual']['y'] >= 0
    session.tray('Show')
    session.fence()
    standalone = session.rect(host)
    assert standalone is not None
    evidence['cases']['edge_flip_negative_anchor_and_standalone_show'] = dict(
        edge=placed, negative=negative, standalone=standalone,
        clients=session.actual_clients(identifiers))

    bar = wait('actual original SlateBar', lambda: session.named('SlateBar'), session)
    session.x.XSelectInput(session.d, bar, 1 << 17)
    session.x.XSync(session.d, 0)
    session.command('SlatePanel')
    wait('controlled rebuild hides popup', session.hidden, session)
    retired = Event()
    wait('actual prior bar DestroyNotify', lambda:
        session.x.XCheckTypedWindowEvent(session.d, bar, 17,
            C.byref(retired)), session)
    # The X server may recycle an XID. DestroyNotify is the observed lifetime
    # boundary, rather than requiring two window numbers to differ.
    new_bar = wait('rebuilt actual SlateBar', lambda:
        session.named('SlateBar'), session)
    assert peer.request('owner')['owner'] == owner
    assert [value['parent'] for value in session.actual_clients(identifiers)] == sockets
    # Shipped bars intentionally reject movement. This one owned window adopts
    # the supported movable style solely to exercise action-time anchoring.
    session.command('WindowId %d WindowStyle VariablePosition\nUpdateStyles' % new_bar)
    session.command('WindowId %d Move 120p 150p' % new_bar)
    moved = wait('actual moved bar geometry', lambda:
        (lambda value: value if value and value['y'] >= 150 else None)(
            session.rect(new_bar)), session)
    fresh_button = session.button(True)
    fresh_popup = session.rect(host)
    assert fresh_button['y'] >= moved['y']
    assert fresh_popup['y'] >= fresh_button['y'] + fresh_button['height']
    session.command('WindowId %d WindowStyle FixedPosition\nUpdateStyles' % new_bar)
    evidence['cases']['bar_rebuild_move_reopen_preserves_clients'] = dict(
        old_bar=bar, new_bar=new_bar, actual_bar=moved,
        old_destroy_notify=True, bar_xid_reused=bar == new_bar,
        temporary_owned_window_style='VariablePosition; UpdateStyles',
        restored_owned_window_style='FixedPosition; UpdateStyles',
        fresh_button=fresh_button, popup=fresh_popup,
        actual_clients=session.actual_clients(identifiers))
    return identifiers, sockets


def pages(session, evidence, identifiers):
    peer = session.peer_client
    for identifier in identifiers:
        peer.request('destroy', id=identifier)
    session.fence(lambda value: value['count'] == 0)
    session.change({'tray_gap': 4, 'tray_padding': 4,
        'tray_orientation': 'horizontal'})
    session.fence(lambda value: not value['pending'])
    session.tray('Show')
    root = session.attrs(session.client.root)
    # 32 wide icons force several pages even on the mixed-DPI 3200px display.
    icons = [peer.create(width=256, height=24, name='Paged plug %02d' % i)
        for i in range(32)]
    for identifier in icons:
        peer.request('dock', id=identifier)
    session.fence(lambda value: value['count'] == 32 and value['mapped'] == 32)
    first = wait('first page actual visible subset', lambda:
        (lambda value: value if 0 < len(value) < len(icons) else None)(
            session.visible(icons)), session)
    page_views, seen = [first], set(first)
    for _ in range(32):
        after = session.page(icons, 'PageNext')
        if after == first:
            break
        assert after not in page_views, page_views
        page_views.append(after)
        seen.update(after)
    else:
        raise AssertionError('page cycle exceeds membership bound')
    assert seen == set(icons), (seen, icons)
    for expected in reversed(page_views[1:]):
        assert session.page(icons, 'PagePrev') == expected
    assert session.page(icons, 'PagePrev') == first
    assert all(value['width'] == 256 and value['height'] == 24
        for value in session.actual_clients(icons))
    session.tray('Focus')
    focused = first[0]
    peer.message(focused, 4)
    wait('proxy owns page focus', lambda:
        session.focus() == session.state['proxy'], session)
    session.press('Next')  # actual PageDown keysym
    advanced = wait('PageDown changes actual page', lambda:
        (lambda value: value if value and value != first else None)(
            session.visible(icons)), session)
    assert peer.state(focused)['map'] != 2
    # A page transition must explicitly retire focus on the hidden foreign plug.
    peer.message(focused, 5)
    assert session.focus() in (session.app, session.state['proxy'])
    session.tray('Focus')
    wait('explicit entry before PageUp', lambda:
        session.focus() == session.state['proxy'], session)
    session.press('Prior')  # PageUp restores the first page
    wait('PageUp restores actual first page', lambda:
        session.visible(icons) == first, session)
    # Real header navigation uses 24-logical-unit hit targets, not commands.
    rect, state = session.rect(session.state['host']), session.fence()
    target = max(1, 24 * state['quarters'] // 4)
    padding = 4 * state['quarters'] // 4
    session.click_rect(dict(x=rect['x'] + rect['width'] - padding - target,
        y=rect['y'] + padding, width=target, height=target))
    header_next = wait('actual next header control', lambda:
        (lambda value: value if value != first else None)(session.visible(icons)), session)
    rect = session.rect(session.state['host'])
    session.click_rect(dict(x=rect['x'] + padding, y=rect['y'] + padding,
        width=target, height=target))
    wait('actual previous header control', lambda:
        session.visible(icons) == first, session)
    # Removal from the final page repairs a now-empty tail and retains all others.
    session.tray('PagePrev')
    session.fence()
    tail = wait('actual last page', lambda:
        (lambda value: value if value != first else None)(session.visible(icons)), session)
    for identifier in tail:
        peer.request('destroy', id=identifier)
    survivors = [identifier for identifier in icons if identifier not in tail]
    session.fence(lambda value: value['count'] == len(survivors))
    repaired = wait('nonempty page after tail removal', lambda:
        session.visible(survivors), session)
    assert all(value['parent'] != peer.identity['root']
        for value in session.actual_clients(survivors))
    evidence['cases']['all_pages_reverse_keys_header_and_removal'] = dict(
        first=first, pages=page_views, covered_ids=sorted(seen),
        page_down=advanced, header_next=header_next,
        removed_tail=tail, repaired_visible=repaired,
        logical_target=24, actual_target=target)
    for identifier in survivors:
        peer.request('destroy', id=identifier)
    session.fence(lambda value: value['count'] == 0)

    # A work area shorter than one 256px icon must still expose a later small
    # icon. Pure arithmetic/negative workarea cases belong to native model tests.
    topology = session.query('topology')['topology']
    monitor = next(item for item in topology['monitors'] if item['primary'])
    session.command('EwmhBaseStruts screen %s 0 0 %d 0' % (
        monitor['name'], root.height - 100))
    wait('actual reduced authoritative work area', lambda:
        any(item['name'] == monitor['name'] and item['work_area']['height'] <= 100
            for item in session.query('workspaces')['monitors']), session)
    omitted = peer.create(width=24, height=256, name='Does not fit this workarea')
    fitting = peer.create(width=24, height=20, name='Fits after omitted prefix')
    for identifier in (omitted, fitting):
        peer.request('dock', id=identifier)
    session.tray('Show')
    session.fence(lambda value: value['count'] == 2)
    fitting_state = wait_state(session, peer, fitting,
        lambda value: value['map'] == 2, 'later fitting icon reachable')
    assert peer.state(omitted)['map'] != 2
    evidence['cases']['oversized_prefix_does_not_strand_later_icon'] = dict(
        omitted=peer.state(omitted), fitting=fitting_state)
    peer.request('destroy', id=omitted)
    session.fence(lambda value: value['count'] == 1)
    session.command('SlatePanel')
    wait('bar rebuild after workarea restore', session.hidden, session)
    return fitting


def themes_and_preferences(session, evidence, survivor):
    peer = session.peer_client
    socket = peer.state(survivor)['parent']
    receipts = []
    profiles = [('slate', 'dark', 'xft:Sans:size=11'),
        ('dune', 'light', 'xft:Serif:size=12'),
        ('slate', 'dark', 'xft:DejaVu Sans Mono:size=18')]
    for index, (palette, appearance, font) in enumerate(profiles):
        session.change({'palette': palette, 'appearance': appearance,
            'tray_gap': 6 + index, 'tray_padding': 5 + index,
            'tray_orientation': 'vertical' if index == 1 else 'horizontal'})
        overlay = session.out / ('theme-%d.fvwm' % index)
        overlay.write_text('InfoStoreAdd slate_font "%s"\nSlatePanel\n'
            '*SlateTray Font "%s"\nSendToModule SlateTray Reload\n' % (font, font))
        session.command('Read "%s"' % overlay)
        wait('theme reload hidden popup', session.hidden, session)
        # The fresh subscriber's Status follows this Read's explicit Reload on
        # the native module pipe; pending=0 observes that processed snapshot.
        state = session.fence(lambda value: not value['pending'] and
            value['font_height'] > 13)
        assert session.property(state['owner'],
            '_NET_SYSTEM_TRAY_ORIENTATION', 'CARDINAL') == [1 if index == 1 else 0]
        session.evidence['active_theme_case'] = dict(index=index,
            palette=palette, appearance=appearance, font=font,
            overlay=overlay.name, sha256=digest(overlay),
            loaded_font_height=state['font_height'])
        session.button(True)
        client = wait_state(session, peer, survivor, lambda value:
            value['map'] == 2, 'same client visible in themed popup')
        assert client['parent'] == socket and client['width'] == 24
        assert client['height'] == 20
        session.command('SendToModule FvwmDesktop edit-settings group:tray')
        editor = wait('compact actual Tray settings editor', lambda:
            session.named('Tray settings'), session)
        session.capture('theme-%d-bar-popup-settings.png' % index)
        session.command('WindowId %d Delete' % editor)
        wait('tray editor closed', lambda:
            session.named('Tray settings') is None, session)
        receipts.append(dict(palette=palette, appearance=appearance,
            overlay=overlay.name, sha256=digest(overlay), font=font,
            loaded_font_height=state['font_height'], actual_client=client,
            actual_host=session.rect(session.state['host']),
            settings=session.setting_snapshot()))
    assert receipts[2]['loaded_font_height'] > max(
        entry['loaded_font_height'] for entry in receipts[:2])
    evidence['cases']['three_theme_font_and_compact_preferences'] = receipts
    saved = session.user / 'desktop-state.json'
    before = json.loads(saved.read_text())
    session.command('SendToModule FvwmDesktop reload-settings')
    snapshot = session.setting_snapshot()
    assert snapshot['settings']['tray_enabled']['saved'] is True
    assert json.loads(saved.read_text()) == before
    assert peer.state(survivor)['parent'] == socket
    session.change({'tray_enabled': False})
    wait('explicit disable releases reserved selection', lambda:
        peer.request('owner')['owner'] == 0, session)
    detached = released(session, peer, survivor)
    session.command('SendToModule FvwmDesktop reload-settings')
    disabled = session.setting_snapshot()
    assert disabled['settings']['tray_enabled']['saved'] is False
    assert peer.request('owner')['owner'] == 0
    session.change({'tray_enabled': True})
    restarted = session.observe_owner()
    assert restarted['count'] == 0
    peer.request('dock', id=survivor)
    session.fence(lambda value: value['count'] == 1)
    session.button(True)
    wait_state(session, peer, survivor, lambda value:
        value['map'] == 2, 'explicit redock after reenable')
    evidence['cases']['saved_preferences_reload_disable_and_reenable'] = dict(
        before=before, reload=snapshot, disabled=disabled,
        actual_detached=detached, observed_restarted=restarted,
        saved_file_sha256=digest(saved), autoreplay=False)
    old_host = session.state['host']
    session.x.XSelectInput(session.d, old_host, 1 << 17)
    session.x.XSync(session.d, 0)
    old_process = session.process_handle(session.options.tray, 'SlateTray')
    # No wait between these ordered requests: exercise native ownership grace
    # while the previous optional module is still completing its own release.
    session.command('SendToModule FvwmDesktop set tray_enabled off\n'
        'SendToModule FvwmDesktop set tray_enabled on')
    session.barrier()
    destroyed = Event()
    wait('rapid off/on old host actual DestroyNotify', lambda:
        session.x.XCheckTypedWindowEvent(session.d, old_host, 17,
            C.byref(destroyed)), session)
    old_exit = old_process.exited(seconds=5) if old_process else None
    rapid = session.observe_owner()
    assert rapid['count'] == 0
    after_release = peer.state(survivor)
    assert after_release['exists'] and after_release['parent'] == peer.identity['root']
    peer.request('dock', id=survivor)
    session.fence(lambda value: value['count'] == 1)
    session.button(True)
    after_redock = wait_state(session, peer, survivor, lambda value:
        value['map'] == 2, 'rapid off/on explicit client redock')
    evidence['cases']['rapid_off_on_eventual_owner_after_release'] = dict(
        old_host=old_host, old_destroy_notify=True, old_process_exit=old_exit,
        observed_replacement=rapid, surviving_root_client=after_release,
        actual_explicit_redock=after_redock, autoreplay=False)


def secondary_anchor(session, evidence):
    if not session.options.xorg_config:
        evidence.setdefault('unavailable', []).append(
            'Secondary 2x monitor anchor requires --xorg-config.')
        return
    session.tray('Hide')
    wait('hide before secondary-monitor anchor', session.hidden, session)
    ready_before_toggle = session.fence(lambda value: not value['pending'])
    result = anchored(session, dict(x=2000, y=40, width=48, height=32), 8)
    result['ready_before_toggle'] = ready_before_toggle
    rect = result['actual']
    assert 1920 <= rect['x'] and rect['x'] + rect['width'] <= 3200
    assert rect['y'] == 72
    assert session.client.parent(result['native']['host']) == session.client.root
    session.capture('secondary-2x-anchor.png')
    evidence['cases']['actual_secondary_anchor_scale'] = result


def abrupt_bar_exit(session, evidence, survivor):
    peer = session.peer_client
    diagnostic = dict(status='before-kill-readiness')
    evidence.setdefault('lifetime_diagnostics', {})['abrupt_bar_exit'] = diagnostic
    bar = wait('live bar before abrupt exit', lambda:
        session.named('SlateBar'), session)
    owner, host = session.state['owner'], session.state['host']
    def observed():
        host_attrs = session.attrs(host)
        return dict(owner=peer.request('owner')['owner'],
            foreign_client=peer.state(survivor), focus=session.focus(),
            observer=session.button_diagnostic_snapshot(),
            actual_host_parent=session.client.parent(host) if host_attrs else None)
    diagnostic['ready_before_kill'] = session.fence(
        lambda value: not value['pending'])
    diagnostic['before_kill'] = observed()
    # Establish the promised visible-popup precondition after queued Reloads
    # finish. Never attribute a popup already hidden before SIGKILL to bar exit.
    diagnostic['mapped_before_kill'] = wait('actual popup mapped before bar kill',
        lambda: session.rect(host), session)
    before = peer.state(survivor)
    handle = session.process_handle(session.options.buttons, 'SlateBar')
    if not handle:
        evidence.setdefault('unavailable', []).append(
            'Abrupt bar exit requires Linux proc/pidfd exact process identity.')
        diagnostic['status'] = 'unavailable-exact-bar-process-observer'
        return
    handle.send(signal.SIGKILL)
    exited = handle.exited(seconds=5)
    wait('actual dead bar window', lambda: session.attrs(bar) is None, session)
    diagnostic['exact_process_exit'] = exited
    diagnostic['ready_after_kill'] = session.fence(
        lambda value: not value['pending'])
    diagnostic['after_kill'] = observed()
    diagnostic['status'] = 'observed-before-and-after-exact-bar-exit'
    after = peer.state(survivor)
    assert after['exists'] and after['parent'] == before['parent']
    assert session.client.parent(after['parent']) == host
    assert session.client.parent(host) == session.client.root
    assert peer.request('owner')['owner'] == owner
    assert session.rect(host) is not None
    evidence['cases']['abrupt_bar_exit_preserves_popup_and_plug'] = dict(
        exact_process=exited, bar=bar, before=before, after=after,
        host_root_parent=session.client.parent(host), owner=owner)


def run(options):
    runtime = HarnessRuntime(prefix='fvwm-tray-bar-')
    out = options.artifacts_dir.resolve()
    out.mkdir(parents=True, exist_ok=True)
    session = initial_peer = None
    evidence = dict(status='running', cases={}, captures=[], operations=[])
    try:
        if options.xorg_config:
            assert not options.display, '--display and --xorg-config are exclusive'
            _, options.display = start_xorg_dummy(runtime,
                options.xorg_config.resolve(), time.monotonic() + 10)
            wait_for_randr(runtime, options.display, time.monotonic() + 10)
            run_required_xrandr(runtime, options.display,
                ['--addmode', 'DUMMY1', '1280x1024'], 'bar-second-mode',
                evidence['operations'])
            run_required_xrandr(runtime, options.display,
                ['--output', 'DUMMY0', '--mode', '1920x1080', '--pos', '0x0',
                '--primary', '--set', 'WIDTH_MM', '508', '--set', 'HEIGHT_MM',
                '286', '--output', 'DUMMY1', '--mode', '1280x1024', '--pos',
                '1920x0', '--set', 'WIDTH_MM', '169', '--set', 'HEIGHT_MM',
                '135', '--fb', '3200x1080', '--fbmm', '846x286'],
                'bar-mixed-dpi-baseline', evidence['operations'])
            evidence['baseline_randr'] = wait_for_authoritative_state(runtime,
                options.display, 'tray-bar-baseline', lambda state:
                    set(state['monitors']) == {'DUMMY0', 'DUMMY1'} and
                    state['monitors']['DUMMY0']['primary'],
                time.monotonic() + 8, 'tray-bar-baseline')
        elif not options.display:
            _, options.display = start_display_server(runtime, 'Xvfb',
                ['Xvfb', '-screen', '0', '1440x1000x24', '-noreset'],
                'xserver.log', time.monotonic() + 10)
        initial_peer = Peer(options.client, runtime.environment({
            'DISPLAY': options.display}), out, 'startup-competing-owner')
        assert initial_peer.request('owner')['owner'] == 0, (
            'Disposable display already has a tray owner; preserve it.')
        options.initial_owner = initial_peer.request('claim')['owner']
        # Retain the partially initialized owner too, so a readiness failure
        # still reaches its installed X error-handler/display cleanup.
        session = Bar.__new__(Bar)
        Bar.__init__(session, runtime, options, out)
        # Keep Session's live evidence object, also bound to the X error handler.
        session.evidence.update(evidence)
        evidence = session.evidence
        evidence['executor'] = dict(uname=list(os.uname()),
            execution_profile=options.execution_profile,
            cpu_execution='caller declaration, not inferred from uname')
        evidence['inputs'] = {name: dict(path=str(getattr(options, name)),
            sha256=digest(getattr(options, name))) for name in
            ('fvwm', 'session', 'desktop', 'form', 'buttons', 'tray',
                'mfl', 'client', 'capture')}
        evidence['cases']['startup_off_preserves_existing_owner'] = dict(
            independent_owner=options.initial_owner,
            observed_owner=initial_peer.request('owner')['owner'],
            settings=session.setting_snapshot())
        assert initial_peer.request('owner')['owner'] == options.initial_owner
        initial_peer.request('release')
        wait('controlled startup owner released', lambda:
            session.peer_client.request('owner')['owner'] == 0, session)
        identifiers, _ = popup_cases(session, evidence)
        survivor = pages(session, evidence, identifiers)
        themes_and_preferences(session, evidence, survivor)
        secondary_anchor(session, evidence)
        abrupt_bar_exit(session, evidence, survivor)
        session.change({'tray_enabled': False})
        wait('final graceful release', lambda:
            session.peer_client.request('owner')['owner'] == 0, session)
        evidence['final_foreign_client'] = released(session,
            session.peer_client, survivor)
        evidence['status'] = 'passed'
    except BaseException as error:
        evidence.update(status='failed', error=repr(error),
            traceback=traceback.format_exc())
        raise
    finally:
        if initial_peer:
            (out / 'startup-competing-owner-wire.json').write_text(
                json.dumps(initial_peer.records, indent=2) + '\n')
            try:
                initial_cleanup = initial_peer.close()
                evidence['initial_peer_cleanup'] = initial_cleanup
                assert initial_cleanup == {'exit': 0, 'forced_cleanup': False}
            except BaseException as error:
                evidence.update(status='failed', initial_peer_cleanup_error=repr(error))
        if session:
            try:
                session.close()
            except BaseException as error:
                evidence.update(status='failed', cleanup_error=repr(error))
        for process in runtime.processes:
            if process.log_path.exists():
                shutil.copyfile(process.log_path, out / process.log_path.name)
        try:
            runtime.close()
        except BaseException as error:
            evidence.update(status='failed', runtime_cleanup_error=repr(error))
        (out / 'result.json').write_text(json.dumps(evidence, indent=2) + '\n')
    assert evidence['status'] == 'passed', evidence


def main():
    parser = argparse.ArgumentParser(description=__doc__, allow_abbrev=False)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--artifacts-dir', type=Path, required=True)
    for name in ('fvwm', 'session', 'desktop', 'form', 'buttons', 'tray',
            'mfl', 'client', 'capture', 'data-root', 'modules-dir'):
        parser.add_argument('--' + name, type=Path)
    parser.add_argument('--display')
    parser.add_argument('--xorg-config', type=Path)
    parser.add_argument('--execution-profile', default='unspecified',
        choices=('unspecified', 'native-cpu', 'emulated-cpu'))
    options = parser.parse_args()
    options.build = options.build.resolve()
    source = Path(__file__).resolve().parents[2]
    defaults = dict(fvwm=options.build / 'fvwm3',
        session=options.build / 'bin/FvwmPrompt/fvwm-session',
        desktop=options.build / 'bin/FvwmPrompt/FvwmDesktop',
        form=options.build / 'modules/FvwmForm/FvwmForm',
        buttons=options.build / 'modules/FvwmButtons/FvwmButtons',
        tray=options.build / 'modules/FvwmTray/FvwmTray',
        mfl=options.build / 'modules/FvwmMFL/FvwmMFL',
        client=options.build / 'tests/tray-client',
        capture=options.build / 'bin/fvwm-desktop-capture',
        data_root=source / 'default-config')
    for name, default in defaults.items():
        value = (getattr(options, name) or default).resolve()
        assert value.exists(), value
        setattr(options, name, value)
    options.module_links = dict(FvwmTray=options.tray,
        FvwmButtons=options.buttons)
    if options.modules_dir:
        options.modules_dir = options.modules_dir.resolve()
    run(options)


if __name__ == '__main__':
    main()
