#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the GNU General Public License, version 2 or any later version.
"""Actual-X checks for explicit core focus after module-owned widget focus."""
import ctypes as C


def run(session, evidence):
    from tray_bar_live import wait
    from tray_live import wait_state

    session.tray('Show')
    session.fence(lambda value: not value['pending'])
    other = session.create('Tray core explicit focus target')['window']
    child = session.peer_client.create(width=16, height=16,
        name='Managed application child focus witness')
    session.x.XSetInputFocus.argtypes = [C.c_void_p, C.c_ulong,
        C.c_int, C.c_ulong]
    session.x.XSetInputFocus.restype = C.c_int
    try:
        session.peer_client.request('reparent', id=child, parent=session.app)
        session.peer_client.request('map', id=child)
        wait_state(session, session.peer_client, child, lambda value:
            value['parent'] == session.app and value['map'] == 2,
            'application child focus witness viewable')
        session.focus_app()
        session.x.XSetInputFocus(session.d, child, 1, 0)
        session.x.XSync(session.d, 0)
        assert session.focus() == child
        pointer_before = session.button_diagnostic_snapshot()['pointer']
        session.command('WindowId %d Focus NoWarp' % session.app)
        assert session.focus() == child, 'same-window Focus displaced app child'
        session.command('WindowId %d FlipFocus NoWarp' % other)
        wait('explicit different-window FlipFocus after widget focus', lambda:
            session.focus() == other, session)
        pointer_after = session.button_diagnostic_snapshot()['pointer']
        assert (pointer_before['x'], pointer_before['y']) == (
            pointer_after['x'], pointer_after['y']), 'NoWarp moved the pointer'
        # A root-popup proxy belongs to no managed frame. This controlled
        # CurrentTime assignment models a module's newer focus timestamp while
        # the core still remembers the other managed target.
        proxy = session.state['proxy']
        session.x.XSetInputFocus(session.d, proxy, 1, 0)
        session.x.XSync(session.d, 0)
        assert session.focus() == proxy
        session.command('WindowId %d Focus NoWarp' % session.app)
        wait('explicit different-window Focus after widget focus', lambda:
            session.focus() == session.app, session)
        session.command('Style "Tray core explicit focus target" '
            '!FPFocusByFunction')
        session.command('UpdateStyles')
        session.command('WindowId %d Focus NoWarp' % other)
        session.keyboard('SlateBar')
        assert session.focus() == session.app, 'Focus bypassed refusal policy'
        evidence['cases']['explicit_focus_clock_child_nowarp_and_refusal'] = dict(
            application=session.app, child=child, other=other, unmanaged_proxy=proxy,
            pointer_before=pointer_before, pointer_after=pointer_after,
            child_focus_preserved=True, different_target_restored=True,
            function_refusal_preserved=True)
    finally:
        session.peer_client.request('destroy', id=child)
        session.focus_app()
