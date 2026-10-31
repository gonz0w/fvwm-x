#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the GNU General Public License, version 2 or any later version.
"""Actual-X tooltip lifetime cases using a caller-owned independent BusPeer.

The caller creates and retains the peer in its cleanup list before run().
Only the queued-Expose case needs an authenticated Linux pidfd Tray handle.
Source authoring alone does not establish executed or full-platform coverage.
"""
import ctypes as C
import json
from pathlib import Path
import signal


def run(session, evidence, peer):
    from tray_bar_live import wait
    from tray_live import digest

    receipt = dict(status='running', cases={}, peer_unique=peer.unique,
        source_sha256=digest(Path(__file__)))
    evidence['cases']['tooltip_lifetime'] = receipt
    path = '/TooltipLifetime'
    title = 'Tray tooltip lifetime original'
    refreshed_title = 'Tray tooltip lifetime refreshed'
    stopped = None
    peer_closed = False

    def current(name):
        value = session.cell(name)
        return value if value and value['actual'] else None

    def hidden(window):
        attributes = session.attrs(window)
        return attributes is not None and attributes.map_state == 0

    def hover(value):
        # Cross a real Leave/Enter boundary so a dismissed hover arms again.
        session.pointer.motion(20, 300)
        session.x.XSync(session.d, 0)
        rect = wait('tooltip target actual rectangle', lambda:
            session.rect(value['window']), session)
        session.pointer.motion(rect['x'] + rect['width'] // 2,
            rect['y'] + rect['height'] // 2)
        session.x.XSync(session.d, 0)
        window = wait('actual tooltip mapped after fresh hover', lambda:
            session.named('Tray tooltip'), session)
        assert session.focus() == focus, 'hover changed application focus'
        return window

    def hidden_fences(window):
        # Four fresh module replies pass the bounded turns containing the 64
        # queued Exposes, without accepting an older Status from the pipe.
        fences = []
        for _ in range(4):
            fences.append(session.fence())
            assert hidden(window), 'dismissed tooltip remapped after fresh fence'
            assert session.named('Tray tooltip') is None
            assert session.focus() == focus, 'tooltip retirement changed focus'
        return fences

    try:
        assert session.service()['connected'], 'modern bus must be enabled'
        assert peer.request('add', path=path, title=title)['ok']
        session.tray('Show')
        session.fence(lambda value: not value['pending'])
        original = wait('independent tooltip producer cell', lambda:
            current(title), session)
        session.focus_app()
        focus = session.focus()
        tooltip = hover(original)

        # A property refresh changes the immutable item generation while the
        # pointer remains over the same cell: no Leave event can hide this view.
        assert peer.request('update', path=path, title=refreshed_title)['ok']
        refreshed = wait('tooltip producer property generation refreshed', lambda:
            (lambda value: value if value and
                value['generation'] != original['generation'] else None)(
                    current(refreshed_title)), session)
        assert refreshed['id'] == original['id']
        wait('old-generation tooltip actually unmapped', lambda:
            hidden(tooltip), session)
        receipt['cases']['property_generation_retirement'] = dict(
            before=original, after=refreshed, tooltip_window=tooltip,
            fences=hidden_fences(tooltip), focus=focus)

        # Passive membership must also retire a currently painted tooltip.
        assert hover(refreshed) == tooltip
        assert peer.request('update', path=path, title=refreshed_title,
            passive=True)['ok']
        passive = wait('tooltip producer becomes passive', lambda:
            (lambda value: value if value and not value['actual'] else None)(
                session.cell(refreshed_title)), session)
        wait('passive tooltip actually unmapped', lambda: hidden(tooltip), session)
        receipt['cases']['passive_retirement'] = dict(cell=passive,
            tooltip_window=tooltip, fences=hidden_fences(tooltip))
        assert peer.request('update', path=path, title=refreshed_title)['ok']
        active = wait('tooltip producer active again', lambda:
            current(refreshed_title), session)
        assert hover(active) == tooltip

        handle = session.process_handle(session.options.tray, 'SlateTray')
        if handle is None:
            reason = ('Queued real Expose ordering requires an authenticated '
                'Linux proc/executable/birth/pidfd Tray handle; refresh, passive '
                'and owner-loss cases still run on this platform.')
            receipt['cases']['queued_expose_after_click'] = dict(
                status='unavailable', reason=reason)
            evidence.setdefault('limits', []).append(reason)
        else:
            session.x.XClearArea.argtypes = [C.c_void_p, C.c_ulong,
                C.c_int, C.c_int, C.c_uint, C.c_uint, C.c_int]
            session.x.XClearArea.restype = C.c_int
            # Never signal a property-supplied PID directly. The retained handle
            # authenticates this exact core child, executable and birth first.
            handle.send(signal.SIGSTOP)
            stopped = handle
            paused = wait('authenticated Tray actually stopped', lambda:
                (lambda value: value if value['state'] in ('T', 't') else None)(
                    handle.sample()), session)
            assert session.named('Tray tooltip') == tooltip
            click = session.click(active['window'], 1)
            for _ in range(64):
                session.x.XClearArea(session.d, tooltip, 0, 0, 0, 0, 1)
            session.x.XSync(session.d, 0)
            assert session.named('Tray tooltip') == tooltip
            handle.send(signal.SIGCONT)
            stopped = None
            activated = wait('actual click before queued tooltip Exposes', lambda:
                peer.observed('Activate', path), session)
            assert (activated['x'], activated['y']) == (click['x'], click['y'])
            wait('click dismisses actual tooltip', lambda: hidden(tooltip), session)
            receipt['cases']['queued_expose_after_click'] = dict(status='passed',
                tooltip_window=tooltip, cell=active, paused_identity=paused,
                actual_click=click, activation=activated,
                clear_requests=64, exposures=True, input='actual XTest left click',
                fences=hidden_fences(tooltip))

        # Rehover the same retained cell before dropping its actual bus owner.
        assert hover(active) == tooltip
        cleanup = peer.close()
        peer_closed = True
        receipt['peer_cleanup'] = cleanup
        assert cleanup == dict(exit=0, forced_cleanup=False), cleanup
        wait('bus owner loss retires exact tooltip cell', lambda:
            all(value['id'] != active['id'] for value in session.cells()), session)
        wait('owner-loss tooltip actually unmapped', lambda: hidden(tooltip), session)
        receipt['cases']['owner_loss_retirement'] = dict(before=active,
            tooltip_window=tooltip, fences=hidden_fences(tooltip),
            remaining_cells=session.cells(), focus=focus)
        receipt['status'] = ('passed_with_unavailable_case' if handle is None
            else 'passed')
    except BaseException:
        receipt['status'] = 'failed'
        raise
    finally:
        if stopped is not None and stopped.alive():
            stopped.send(signal.SIGCONT)
        if not peer_closed:
            cleanup = peer.close()
            receipt['peer_cleanup'] = cleanup
        wire = session.out / 'tooltip-lifetime-peer-wire.json'
        wire.write_text(json.dumps(peer.records, indent=2) + '\n')
        receipt['peer_wire'] = dict(path=str(wire), sha256=digest(wire))
        assert receipt['peer_cleanup'] == dict(exit=0, forced_cleanup=False), (
            receipt['peer_cleanup'])
