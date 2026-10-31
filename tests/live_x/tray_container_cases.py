#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the GNU General Public License, version 2 or any later version.
"""Additional bounded native-container cases on the caller's owned display.

Imported after the basic modern/container fixture has restarted its Tray owner.
These checks require enabled modern support, XFixes and Linux pidfd identity.
Source authoring is not executed acceptance or real-toolkit qualification.
"""
import ctypes as C
import json
import os
import select
import signal
import subprocess
import time


class ItemPeer:
    """One independent native SNI producer; no main-fixture import cycle."""
    def __init__(self, session):
        self.records = []
        self.pending = b''
        self.log = (session.out / 'container-item.stderr').open('wb')
        self.process = subprocess.Popen([str(session.options.sni_client),
            '--private-bus', session.options.session_address],
            stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=self.log,
            env=session.env, start_new_session=True)
        self.identity = self.request('hello')

    def request(self, verb, **fields):
        request = dict(verb=verb, **fields)
        wire = (json.dumps(request) + '\n').encode()
        assert len(wire) <= 16384 and len(self.records) < 512
        self.process.stdin.write(wire)
        self.process.stdin.flush()
        deadline = time.monotonic() + 4
        while b'\n' not in self.pending:
            assert self.process.poll() is None, self.process.returncode
            ready = select.select([self.process.stdout], [], [],
                max(0, deadline - time.monotonic()))[0]
            assert ready, 'container SNI producer response deadline'
            data = os.read(self.process.stdout.fileno(), 65536)
            assert data, 'container SNI producer disconnected'
            self.pending += data
            assert len(self.pending) <= 262144
        line, self.pending = self.pending.split(b'\n', 1)
        response = json.loads(line)
        self.records.append(dict(request=request, response=response))
        return response

    def close(self):
        forced = False
        try:
            if self.process.poll() is None:
                self.process.stdin.close()
                try:
                    self.process.wait(timeout=3)
                except subprocess.TimeoutExpired:
                    forced = True
                    self.process.kill()
                    self.process.wait(timeout=2)
        finally:
            self.process.stdout.close()
            self.log.close()
        return dict(exit=self.process.returncode, forced_cleanup=forced)


def run(session, evidence):
    from legacy_property_live import Event
    from tray_bar_live import wait
    from tray_live import digest, released, wait_state

    alias = 'TrayContainerExtra'
    peer = None
    plugs = []
    stopped = []
    receipt = dict(profiles=[], source_only=False)
    evidence['cases']['additional_native_container'] = receipt
    assert not session.options.expect_no_container
    assert not session.options.expect_modern_unavailable
    assert session.service()['container_capable']
    initial_owner = session.state['owner']
    initial_host = session.state['host']
    overlay = session.out / 'container-extra.fvwm'

    for name in ('XUnmapWindow', 'XMapWindow'):
        function = getattr(session.x, name)
        function.argtypes = [C.c_void_p, C.c_ulong]
        function.restype = C.c_int
    session.x.XSetInputFocus.argtypes = [C.c_void_p, C.c_ulong,
        C.c_int, C.c_ulong]
    session.x.XSetInputFocus.restype = C.c_int
    session.x.XClearArea.argtypes = [C.c_void_p, C.c_ulong, C.c_int,
        C.c_int, C.c_uint, C.c_uint, C.c_int]
    session.x.XClearArea.restype = C.c_int
    session.x.XSendEvent.argtypes = [C.c_void_p, C.c_ulong, C.c_int,
        C.c_long, C.POINTER(Event)]
    session.x.XSendEvent.restype = C.c_int

    def stop(handle):
        assert handle, 'authenticated Linux pidfd process required'
        handle.send(signal.SIGSTOP)
        stopped.append(handle)
        return wait('owned process actually stopped', lambda:
            (lambda value: value if value['state'] in ('T', 't') else None)(
                handle.sample()), session)

    def resume(handle):
        handle.send(signal.SIGCONT)
        stopped.remove(handle)

    def root_host():
        return session.client.parent(initial_host) == session.client.root

    def retire():
        session.command('KillModule FvwmButtons ' + alias)
        wait('extra container normally retired', lambda:
            session.named(alias) is None, session)
        wait('normal release roots the same host', root_host, session)
        wait('normal release hides the same host', session.hidden, session)
        state = session.fence()
        assert state['owner'] == initial_owner and state['host'] == initial_host
        assert not session.service()['attached']

    def start(width=480, height=72, font='xft:Sans:size=11', single=False):
        if session.named(alias) is not None:
            retire()
        rows = ('*%s: (1x1, Id native, Title "Tray", Tray)\n' % alias
            if single else
            '*%s: (1x1, Id before, Title "Before", '
            'Action Echo CONTAINER_BEFORE)\n'
            '*%s: (6x1, Id native, Title "Tray", Tray)\n'
            '*%s: (1x1, Id after, Title "After", '
            'Action Echo CONTAINER_AFTER)\n' % (alias, alias, alias))
        overlay.write_text('DestroyModuleConfig %s:*\n'
            '*%s: Geometry %dx%d+80+160\n'
            '*%s: Rows 1\n*%s: Columns %d\n'
            '*%s: Frame 0\n*%s: Padding 0 0\n'
            '*%s: Colorset 56\n*%s: Font "%s"\n'
            '*%s: KeyboardNavigation 1\n' %
            (alias, alias, width, height, alias, alias,
                1 if single else 8, alias, alias, alias, alias, font, alias)
            + rows + 'Style %s !Title, !Borders, !Handles, '
            'UsePPosition, ClickToFocus\nModule FvwmButtons %s\n' %
            (alias, alias))
        session.command('Read "%s"' % overlay)
        bar = wait('extra native container managed window', lambda:
            session.named(alias), session)
        state = wait('extra native container attachment', lambda:
            (lambda value: value if value['attached'] else None)(
                session.service()), session)
        assert session.client.parent(state['slot']) == bar
        assert session.client.parent(initial_host) == state['slot']
        session.tray('Show')
        session.fence(lambda value: not value['pending'])
        wait('extra contained host is viewable', lambda:
            session.rect(initial_host), session)
        return bar, state

    def foreign(width, name):
        identifier = session.peer_client.create(width=width, height=20,
            name=name)
        plugs.append(identifier)
        session.peer_client.request('dock', id=identifier)
        wait_state(session, session.peer_client, identifier, lambda value:
            value['parent'] != session.client.root,
            'extra foreign icon admitted')
        return identifier

    def click(window, button=1):
        # Control stacking independently of focus; exercise real XTest input.
        session.command('WindowId %d Raise' % bar)
        result = session.click(window, button)
        receipt.setdefault('actual_pointer_inputs', []).append(dict(
            raised_container=bar, target=window, **result))
        return result

    def focus_pointer(identifier):
        session.command('WindowId %d Focus' % bar)
        wait('managed scope before fresh external-focus witness', lambda:
            session.buttons_focused(alias), session)
        session.focus_app()
        click(identifier)
        # The foreign fixture owns pointer input. Like an XEmbed toolkit,
        # it must explicitly request focus; the host does not grab its clicks.
        session.peer_client.request('xembed', id=identifier, opcode=3)
        wait('pointer delegates after managed Buttons focus', lambda:
            session.focus() == session.state['proxy'], session)

    def no_popups():
        return (session.named('Tray application menu') is None and
            session.named('Tray tooltip') is None)

    try:
        # Isolate exact foreign paging from unrelated modern producer rows.
        session.change({'tray_enabled': True, 'tray_modern': False,
            'tray_padding': 4, 'tray_gap': 4,
            'tray_orientation': 'horizontal'})
        wait('modern members retired for foreign layout', lambda:
            session.service()['modern_count'] == 0, session)
        session.command('Colorset 56 fg #e5edf5, bg #253140')
        bar, _ = start()
        survivor = foreign(24, 'Container theme witness')
        original_socket = session.peer_client.state(survivor)['parent']
        profiles = [
            ('slate', 'dark', 'xft:Sans:size=11', '#e5edf5', '#253140'),
            ('dune', 'light', 'xft:Serif:size=12', '#322a20', '#eee4cb'),
            ('slate', 'dark', 'xft:DejaVu Sans Mono:size=18',
                '#ffffe0', '#252550')]
        for index, (palette, appearance, font, foreground, background) in (
                enumerate(profiles)):
            session.change({'palette': palette, 'appearance': appearance})
            theme = session.out / ('container-theme-%d.fvwm' % index)
            theme.write_text('Colorset 56 fg %s, bg %s\n'
                '*SlateTray Colorset 56\n*SlateTray Font "%s"\n'
                'SendToModule SlateTray Reload\n' %
                (foreground, background, font))
            session.command('Read "%s"' % theme)
            session.fence(lambda value: not value['pending'])
            bar, state = start(width=800, font=font)
            actual = wait_state(session, session.peer_client, survivor,
                lambda value: value['map'] == 2,
                'same foreign icon in material container theme')
            assert actual['parent'] == original_socket
            assert (actual['width'], actual['height']) == (24, 20)
            session.capture('container-theme-%d.png' % index)
            receipt['profiles'].append(dict(palette=palette,
                appearance=appearance, font=font, overlay=theme.name,
                sha256=digest(theme), native=session.fence(),
                service=state, actual=actual, host=session.rect(initial_host)))
        assert receipt['profiles'][2]['native']['font_height'] > (
            receipt['profiles'][0]['native']['font_height'])
        session.peer_client.request('destroy', id=survivor)
        plugs.remove(survivor)
        session.fence(lambda value: value['count'] == 0)

        # Two wide icons fit separately, but cannot fit beside side controls.
        bar, state = start(100, 36, font='fixed', single=True)
        first = foreign(80, 'Container wide first')
        second = foreign(80, 'Container wide second')
        actual_slot = session.rect(state['slot'])
        assert (actual_slot['width'], actual_slot['height']) == (100, 36)
        initial = wait('one reachable wide foreign member', lambda:
            (lambda values: values if len(values) == 1 else None)(
                session.visible([first, second])), session)
        session.command('WindowId %d Focus' % bar)
        wait('managed compact container focus', lambda:
            session.buttons_focused(alias), session)
        session.focus_app()
        session.command('WindowId %d Focus' % bar)
        wait('managed compact focus after fresh external witness', lambda:
            session.buttons_focused(alias), session)
        session.press('Return')
        wait('compact proxy entry', lambda:
            session.focus() == session.state['proxy'], session)
        session.press('Page_Down')
        following = wait('keyboard exposes the other wide icon', lambda:
            (lambda values: values if len(values) == 1 and values != initial
                else None)(session.visible([first, second])), session)
        assert set(initial + following) == {first, second}
        session.press('Page_Up')
        wait('keyboard returns to first compact page', lambda:
            session.visible([first, second]) == initial, session)
        session.capture('container-wide-keyboard-pages.png')
        receipt['wide_keyboard_pages'] = dict(slot=actual_slot,
            initial=initial, following=following, native=session.fence())

        # Expiry and restoration must also work with StatusNotifier Off.
        focus_trace = receipt['compact_focus_trace'] = []
        focus_trace.append(dict(actual=session.focus(), keyboard=session.keyboard(alias)))
        session.press('Escape')
        wait('compact Escape returns to Buttons', lambda:
            session.buttons_focused(alias), session)
        focus_trace.append(dict(actual=session.focus(), keyboard=session.keyboard(alias)))
        session.press('Escape')
        focus_trace.append(dict(actual=session.focus(), keyboard=session.keyboard(alias)))
        wait('leaving Buttons restores external application', lambda:
            session.focus() == session.app, session)

        # A client focus request after real pointer input creates the target.
        # A controlled ACK is independently queued on the server: Buttons focus
        # alone cannot prove when its event handler has sent its own ACK.
        buttons = session.process_handle(session.options.buttons, alias)
        tray = session.process_handle(session.options.tray, 'SlateTray')
        tuple_before = session.service()
        stop(buttons)
        started = time.monotonic()
        click(initial[0])
        session.peer_client.request('xembed', id=initial[0], opcode=3)
        pending = wait('actual outstanding native client-focus request', lambda:
            (lambda value: value if value['focus_pending'] and
                value['focus_id'] == initial[0] else None)(session.service()), session)
        request_fence = session.fence()
        stop(tray)
        resume(buttons)
        wait('Buttons actually has managed focus before ACK', lambda:
            session.buttons_focused(alias), session)
        acknowledgment = Event()
        acknowledgment.message.type = 33
        acknowledgment.message.display = session.d
        acknowledgment.message.window = initial_owner
        acknowledgment.message.message_type = session.x.XInternAtom(
            session.d, b'_FVWM_TRAY_FOCUS_ACK_V1', 0)
        acknowledgment.message.format = 32
        live_tuple = [tuple_before['slot'], tuple_before['offer'],
            initial_host, initial_owner, 0]
        for index, value in enumerate(live_tuple):
            acknowledgment.message.data[index] = value
        assert session.x.XSendEvent(session.d, initial_owner, 0, 0,
            C.byref(acknowledgment))
        session.x.XSync(session.d, 0)
        session.focus_app()
        elapsed = time.monotonic() - started
        assert elapsed < 1.8, ('delayed ACK must precede focus expiry', elapsed)
        resume(tray)
        session.fence()
        session.barrier()
        assert session.focus() == session.app, 'delayed ACK stole app focus'
        after = session.service()
        assert (after['slot'], after['offer']) == (
            tuple_before['slot'], tuple_before['offer'])
        receipt['delayed_ack_external_focus'] = dict(
            before=tuple_before, pending_request=pending, request_fence=request_fence,
            acknowledgment_source='controlled actual-X protocol input',
            queued_live_tuple=live_tuple,
            elapsed_before_resume=elapsed, after=after,
            actual_focus=session.focus())

        # A current tuple alone cannot authorize reclaiming an external app.
        # Observe owner service twice after delivery as well as actual focus.
        keyboard_before = session.keyboard(alias)
        stale_return = Event()
        stale_return.message.type = 33
        stale_return.message.display = session.d
        stale_return.message.window = tuple_before['offer']
        stale_return.message.message_type = session.x.XInternAtom(
            session.d, b'_FVWM_TRAY_RETURN_V1', 0)
        stale_return.message.format = 32
        return_tuple = [tuple_before['slot'], tuple_before['offer'],
            initial_host, initial_owner, 1]
        for index, value in enumerate(return_tuple):
            stale_return.message.data[index] = value
        assert session.x.XSendEvent(session.d, tuple_before['offer'], 0, 0,
            C.byref(stale_return))
        session.x.XSync(session.d, 0)
        keyboard_first = session.keyboard(alias)
        keyboard_after = session.keyboard(alias)
        session.barrier()
        assert session.focus() == session.app, 'stale RETURN stole app focus'
        assert keyboard_after['selected'] == keyboard_before['selected']
        receipt['stale_return_external_focus'] = dict(
            queued_live_tuple=return_tuple, before=keyboard_before,
            first_owner_observation=keyboard_first, after=keyboard_after,
            actual_focus=session.focus())

        sibling = session.peer_client.create(width=12, height=12,
            name='Unrelated Buttons child focus witness')
        plugs.append(sibling)
        session.peer_client.request('reparent', id=sibling, parent=bar)
        session.peer_client.request('map', id=sibling)
        wait_state(session, session.peer_client, sibling, lambda value:
            value['parent'] == bar and value['map'] == 2,
            'unrelated owned child viewable beside the Tray slot')
        session.command('WindowId %d Focus' % bar)
        session.x.XSetInputFocus(session.d, sibling, 1, 0)
        session.x.XSync(session.d, 0)
        assert session.focus() == sibling
        sibling_before = session.keyboard(alias)
        assert session.x.XSendEvent(session.d, tuple_before['offer'], 0, 0,
            C.byref(stale_return))
        session.x.XSync(session.d, 0)
        session.keyboard(alias)
        sibling_after = session.keyboard(alias)
        session.barrier()
        assert session.focus() == sibling, 'RETURN stole sibling child focus'
        assert sibling_after['selected'] == sibling_before['selected']
        receipt['stale_return_unrelated_child'] = dict(window=sibling,
            before=sibling_before, after=sibling_after,
            actual_focus=session.focus())
        session.peer_client.request('destroy', id=sibling)
        plugs.remove(sibling)
        session.focus_app()

        # Queue more than the native event-turn budget before normal release.
        stopped_tray = stop(tray)
        for index in range(64):
            session.x.XClearArea(session.d, initial_host, index, 0, 1, 1, 1)
        session.x.XSync(session.d, 0)
        session.command('KillModule FvwmButtons ' + alias)
        wait('normal Buttons release while Tray is stopped', root_host, session)
        wait('normal Buttons exit while Tray is stopped', lambda:
            session.named(alias) is None, session)
        resume(tray)
        session.fence()
        wait('queued Expose detach remains hidden and alive',
            session.hidden, session)
        assert session.service()['attached'] == 0
        assert session.peer_client.request('owner')['owner'] == initial_owner
        bar, state = start()
        session.fence()
        assert session.state['host'] == initial_host
        receipt['queued_expose_normal_release_and_rebuild'] = dict(
            stopped_identity=stopped_tray, real_expose_requests=64,
            rebuilt=state, same_host=initial_host, same_owner=initial_owner)

        # Keep a logical foreign focus before entering a second target.
        visible_first = wait_state(session, session.peer_client, first,
            lambda value: value['map'] == 2, 'foreign member after rebuild')
        focus_pointer(first)
        pointer_trace = receipt['pointer_focus_trace'] = []
        pointer_trace.append(dict(actual=session.focus(), keyboard=session.keyboard(alias)))
        session.press('Escape')
        wait('pointer Escape stays in managed Buttons scope', lambda:
            session.buttons_focused(alias), session)
        pointer_trace.append(dict(actual=session.focus(), keyboard=session.keyboard(alias)))
        session.press('Escape')
        pointer_trace.append(dict(actual=session.focus(), keyboard=session.keyboard(alias)))
        try:
            wait('pointer scope leave restores application', lambda:
                session.focus() == session.app, session)
        except BaseException:
            # Diagnostic only, after the original activation has failed. It
            # cannot turn that failure into a passing observation.
            receipt['pointer_exit_failure'] = dict(actual=session.focus(),
                keyboard=session.keyboard(alias),
                foreign_input=session.peer_client.request('events'))
            session.command('WindowId %d Focus NoWarp' % session.app)
            receipt['pointer_exit_failure']['after_explicit_no_warp'] = session.focus()
            raise
        foreign_events = session.peer_client.request('events')['events']
        raw_escape = [event for event in foreign_events if event.get('kind') == 'key'
            and event.get('type') == 2 and event.get('keysym') == 65307
            and not event.get('synthetic')]
        assert not raw_escape, 'Buttons navigation escaped into foreign client'
        receipt['pointer_focus_scope'] = dict(foreign=visible_first,
            final_focus=session.focus(), foreign_raw_escape_events=raw_escape)

        bar, _ = start(800, 96)
        session.change({'tray_modern': True})
        wait('enabled modern adapter connected for contained menu', lambda:
            session.service()['connected'], session)
        session.fence(lambda value: not value['pending'])
        session.tray('Show')
        session.fence()
        peer = ItemPeer(session)
        assert peer.request('add', path='/ContainerItem',
            title='Contained application actions')['ok']
        cell = wait('independent contained modern cell', lambda:
            (lambda value: value if value and value['actual'] else None)(
                session.cell('Contained application actions')), session)
        session.focus_app()
        click(cell['window'], 3)
        menu = wait('contained menu ready after pointer focus handshake',
            session.menu_view, session)
        assert session.focus() == session.state['proxy']
        session.capture('container-application-menu.png')
        session.focus_app()
        wait('external focus dismisses contained root popups',
            no_popups, session)
        session.fence()
        assert session.focus() == session.app
        receipt['menu_external_deactivation'] = dict(
            actual_focus=session.focus(), popups_retired=True,
            menu=menu, cell=cell)

        click(cell['window'], 3)
        wait('contained menu ready before ancestor hide',
            session.menu_view, session)
        frame = session.client.parent(bar)
        assert frame not in (bar, session.client.root)
        desktop = session.property(bar, '_NET_WM_DESKTOP', 'CARDINAL')
        assert len(desktop) == 1 and desktop[0] < 0x7fffffe0, desktop
        attached_before_hide = session.service()
        foreign_before_hide = {identifier: session.peer_client.state(identifier)
            for identifier in plugs}
        session.x.XUnmapWindow(session.d, frame)
        session.x.XSync(session.d, 0)
        wait('actual container ancestor unmapped', lambda:
            session.attrs(frame).map_state == 0, session)
        hidden_host = session.attrs(initial_host)
        assert hidden_host and hidden_host.map_state == 1
        session.focus_app()
        wait('ancestor hide retires root popups', no_popups, session)
        session.fence()
        assert session.focus() == session.app
        receipt['ancestor_hide_deactivation'] = dict(frame=frame,
            host_map_state=hidden_host.map_state,
            popups_retired=True, actual_focus=session.focus())
        # The hidden ancestor is the core's inner wrapper. Another X client
        # cannot remap it directly: SubstructureRedirect turns that request
        # into an already-mapped-client notification. Let the owner restore
        # both wrappers through managed desk transitions, preserving clients.
        other_desktop = desktop[0] + 17
        session.command('WindowId %d MoveToDesk 0 %d' % (bar, other_desktop))
        wait('managed window moved to temporary desk', lambda:
            session.property(bar, '_NET_WM_DESKTOP', 'CARDINAL') ==
                [other_desktop], session)
        session.command('WindowId %d MoveToDesk 0 %d' % (bar, desktop[0]))
        wait('managed window returned to original desk', lambda:
            session.property(bar, '_NET_WM_DESKTOP', 'CARDINAL') == desktop,
            session)
        wait('contained host viewable after owner restores ancestor', lambda:
            session.rect(initial_host), session)
        restored = session.service()
        for field in ('attached', 'slot', 'offer'):
            assert restored[field] == attached_before_hide[field]
        assert session.state['owner'] == initial_owner
        assert session.state['host'] == initial_host
        for identifier, before in foreign_before_hide.items():
            assert session.peer_client.state(identifier)['parent'] == before['parent']
        receipt['ancestor_hide_deactivation']['managed_restore'] = dict(
            original_desktop=desktop, temporary_desktop=other_desktop,
            attached_before=attached_before_hide, restored=restored,
            same_foreign_parents=True)
        (session.out / 'container-item-wire.json').write_text(
            json.dumps(peer.records, indent=2) + '\n')
        cleanup = peer.close()
        receipt['peer_cleanup'] = cleanup
        peer = None
        assert cleanup == dict(exit=0, forced_cleanup=False), cleanup
        wait('independent modern member retired', lambda:
            session.cell('Contained application actions') is None, session)
        retire()
        for identifier in list(plugs):
            session.peer_client.request('info', id=identifier, flags=0)
            wait_state(session, session.peer_client, identifier,
                lambda value: value['map'] == 0,
                'foreign icon unmapped before explicit release')
            session.peer_client.request('reparent', id=identifier,
                parent=session.client.root)
            released(session, session.peer_client, identifier)
            session.peer_client.request('destroy', id=identifier)
            plugs.remove(identifier)
        session.fence()
        assert root_host() and session.hidden()
        assert session.peer_client.request('owner')['owner'] == initial_owner
        receipt['final'] = dict(native=session.fence(),
            service=session.service(), foreign_owned_windows_retired=True)
    finally:
        # Resume only authenticated handles paused by this finite fixture.
        for handle in list(stopped):
            if handle.alive():
                handle.send(signal.SIGCONT)
            stopped.remove(handle)
        if peer:
            (session.out / 'container-item-wire.json').write_text(
                json.dumps(peer.records, indent=2) + '\n')
            cleanup = peer.close()
            receipt['peer_cleanup'] = cleanup
            assert cleanup == dict(exit=0, forced_cleanup=False), cleanup
        session.command('KillModule FvwmButtons ' + alias)
        for identifier in plugs:
            if session.peer_client.state(identifier)['exists']:
                session.peer_client.request('destroy', id=identifier)
