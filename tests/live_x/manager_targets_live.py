#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or (at your option) any later
# version.
"""Read manager conversions independently and reject foreign/stale requests."""
import argparse
import ctypes as C
import json
from pathlib import Path
import shutil
import time

from geometry_transition_live import Client
from live_x_harness import HarnessRuntime, start_display_server, wait_for_predicate
from mfl_stream_live import Connection
from selection_live import Event, SelectionRequest


def run(args):
    args.fvwm = args.fvwm.resolve()
    args.mfl = args.mfl.resolve()
    out = args.artifacts_dir.resolve()
    out.mkdir(parents=True, exist_ok=True)
    evidence = {'case': args.case, 'status': 'running'}
    with HarnessRuntime(prefix='fvwm-manager-targets-') as runtime:
        peer = connection = None
        try:
            display = args.display
            if display is None:
                _, display = start_display_server(runtime, 'Xvfb',
                    ['Xvfb', '-screen', '0', '800x600x24', '-noreset'],
                    'xserver.log', time.monotonic() + 10)
            peer = Client(display)
            x, d = peer.x, peer.display
            x.XInternAtom.argtypes = [C.c_void_p, C.c_char_p, C.c_int]
            x.XInternAtom.restype = C.c_ulong
            x.XDefaultScreen.argtypes = [C.c_void_p]
            x.XGetSelectionOwner.argtypes = [C.c_void_p, C.c_ulong]
            x.XGetSelectionOwner.restype = C.c_ulong
            x.XConvertSelection.argtypes = [C.c_void_p] + [C.c_ulong] * 5
            x.XChangeProperty.argtypes = [C.c_void_p] + [C.c_ulong] * 3 + [
                C.c_int, C.c_int, C.c_void_p, C.c_int]
            x.XDeleteProperty.argtypes = [C.c_void_p, C.c_ulong, C.c_ulong]
            x.XGetWindowProperty.argtypes = [C.c_void_p, C.c_ulong, C.c_ulong,
                C.c_long, C.c_long, C.c_int, C.c_ulong, C.POINTER(C.c_ulong),
                C.POINTER(C.c_int), C.POINTER(C.c_ulong),
                C.POINTER(C.c_ulong), C.POINTER(C.c_void_p)]
            x.XCheckTypedWindowEvent.argtypes = [C.c_void_p, C.c_ulong,
                C.c_int, C.POINTER(Event)]
            x.XSendEvent.argtypes = [C.c_void_p, C.c_ulong, C.c_int,
                C.c_long, C.POINTER(Event)]
            atom = lambda name: x.XInternAtom(d, name.encode(), 0)
            selection = atom(f'WM_S{x.XDefaultScreen(d)}')
            window = x.XCreateSimpleWindow(d, peer.root, 1, 1, 1, 1, 0, 0, 0)

            def start():
                core = runtime.start('core', [str(args.fvwm), '-d', display,
                    '-v', '-o', str(runtime.root / 'debug.log'),
                    '-c', f'ModulePath {args.mfl.parent}',
                    '-c', 'AddToFunc StartFunction I Module FvwmMFL'],
                    f'core-{len(runtime.processes)}.log', runtime.environment({
                        'FVWMMFL_SOCKET_PATH': str(runtime.root)}))
                endpoint = runtime.root / f"fvwm_mfl_{display.replace(':', '_')}.sock"
                wait_for_predicate('MFL endpoint', endpoint.exists,
                    time.monotonic() + 12, process=core)
                link = Connection(endpoint)
                link.record()
                link.send(b'set echo\n')
                link.echo('manager-targets-ready')
                return core, link

            core, connection = start()
            owner = x.XGetSelectionOwner(d, selection)
            assert owner

            def read(prop, kind=None):
                actual, count, remaining = C.c_ulong(), C.c_ulong(), C.c_ulong()
                fmt, data = C.c_int(), C.c_void_p()
                assert x.XGetWindowProperty(d, window, prop, 0, 512, 0, 0,
                    C.byref(actual), C.byref(fmt), C.byref(count),
                    C.byref(remaining), C.byref(data)) == 0
                try:
                    if kind is None:
                        assert actual.value == 0 and count.value == 0
                        return None
                    assert actual.value == atom(kind) and fmt.value == 32
                    assert remaining.value == 0
                    return list((C.c_ulong * count.value).from_address(data.value))
                finally:
                    if data:
                        x.XFree(data)

            def change(prop, values):
                data = (C.c_ulong * len(values))(*values)
                x.XChangeProperty(d, window, prop, atom('ATOM_PAIR'),
                    32, 0, data, len(values))

            def reply(target, prop, timestamp=0, forged=None):
                if forged is None:
                    x.XConvertSelection(d, selection, target, prop, window,
                        timestamp)
                else:
                    event = Event()
                    event.request = SelectionRequest(type=30, display=d,
                        owner=owner, requestor=window, selection=selection,
                        target=target, property=prop, time=timestamp)
                    for name, value in forged.items():
                        setattr(event.request, name, value)
                    assert x.XSendEvent(d, owner, 0, 0, C.byref(event))
                x.XSync(d, 0)
                event = Event()
                wait_for_predicate('correlated selection reply',
                    lambda: x.XCheckTypedWindowEvent(d, window, 31,
                        C.byref(event)), time.monotonic() + 5, process=core)
                result = event.selection
                assert result.requestor == window and result.target == target
                assert result.selection == (forged or {}).get('selection', selection)
                assert result.time == timestamp and result.send_event
                connection.echo('manager-targets-healthy')
                return result.property

            stamp = atom('FVWM_MANAGER_TIMESTAMP')
            assert reply(atom('TIMESTAMP'), stamp) == stamp
            acquired = read(stamp, 'INTEGER')
            assert len(acquired) == 1 and acquired[0] > 1
            acquired = acquired[0]
            evidence['acquired'] = acquired
            result = atom('FVWM_MANAGER_RESULT')
            if args.case == 'values':
                targets = [atom(n) for n in ['TARGETS', 'MULTIPLE', 'TIMESTAMP', 'VERSION']]
                assert reply(atom('TARGETS'), result, acquired) == result
                assert read(result, 'ATOM') == targets
                assert reply(atom('VERSION'), result) == result
                assert read(result, 'INTEGER') == [2, 0]
                assert reply(atom('VERSION'), 0) == atom('VERSION')
                assert read(atom('VERSION'), 'INTEGER') == [2, 0]
                assert reply(atom('UNSUPPORTED_TARGET'), result) == 0
                # Refusal leaves a previous result untouched.
                assert read(result, 'INTEGER') == [2, 0]
                assert reply(atom('MULTIPLE'), 0) == 0
                evidence['direct_typed_values_and_legacy_refusal'] = 'passed'
                # A fresh owner must report its own acquisition, not the old one.
                connection.send(b'Quit\n')
                assert core.wait_until_exit(time.monotonic() + 5) == 0
                connection.close()
                connection = None
                assert x.XGetSelectionOwner(d, selection) == 0
                time.sleep(.02)
                core, connection = start()
                owner = x.XGetSelectionOwner(d, selection)
                assert reply(atom('TIMESTAMP'), stamp) == stamp
                second = read(stamp, 'INTEGER')
                assert len(second) == 1 and 0 < ((second[0] - acquired) & 0xffffffff) < 0x80000000
                evidence['new_owner_acquisition'] = second[0]
            elif args.case == 'multiple':
                pairs = atom('FVWM_MANAGER_PAIRS')
                bad = atom('FVWM_MANAGER_BAD')
                values = [atom('TARGETS'), result, atom('UNSUPPORTED_TARGET'), bad,
                    atom('TIMESTAMP'), stamp, atom('VERSION'), result]
                change(pairs, values)
                assert reply(atom('MULTIPLE'), pairs) == pairs
                expected = list(values)
                expected[3] = 0
                assert read(pairs, 'ATOM_PAIR') == expected
                assert read(result, 'INTEGER') == [2, 0]
                assert read(stamp, 'INTEGER') == [acquired]
                read(bad)
                # Same destination, reverse order: TIMESTAMP wins this time.
                change(pairs, [atom('VERSION'), result, atom('TIMESTAMP'), result])
                assert reply(atom('MULTIPLE'), pairs) == pairs
                assert read(result, 'INTEGER') == [acquired]
                evidence['mixed_results_and_order_before_notify'] = 'passed'
                # Queue a request and destroy its requestor before dispatch.
                gone = x.XCreateSimpleWindow(d, peer.root, 1, 1, 1, 1, 0, 0, 0)
                event = Event()
                event.request = SelectionRequest(type=30, display=d, owner=owner,
                    requestor=gone, selection=selection, target=atom('VERSION'),
                    property=result, time=0)
                x.XGrabServer.argtypes = [C.c_void_p]
                x.XUngrabServer.argtypes = [C.c_void_p]
                x.XGrabServer(d)
                x.XSendEvent(d, owner, 0, 0, C.byref(event))
                x.XDestroyWindow(d, gone)
                x.XUngrabServer(d)
                x.XSync(d, 0)
                connection.echo('healthy-after-vanished-requestor')
                assert reply(atom('VERSION'), result) == result
                assert read(result, 'INTEGER') == [2, 0]
                evidence['vanished_requestor_recovery'] = 'passed'
            else:
                refused = []
                for label, timestamp, fields in [
                    ('foreign-selection', 0, {'selection': atom('PRIMARY')}),
                    ('foreign-owner', 0, {'owner': window}),
                    ('prior-acquisition', (acquired - 1) & 0xffffffff, {}),
                ]:
                    x.XDeleteProperty(d, window, result)
                    observed = reply(atom('VERSION'), result, timestamp, fields)
                    refused.append(observed == 0)
                    if observed == 0:
                        read(result)
                    else:
                        assert read(result, 'INTEGER') == [2, 0]
                    evidence[label] = 'passed' if observed == 0 else 'wrongly-accepted'
                    if observed == 0:
                        pairs = atom('FVWM_MANAGER_PAIRS')
                        values = [atom('VERSION'), result]
                        change(pairs, values)
                        assert reply(atom('MULTIPLE'), pairs, timestamp, fields) == 0
                        assert read(pairs, 'ATOM_PAIR') == values
                        read(result)
                assert reply(atom('VERSION'), result, acquired, {}) == result
                assert read(result, 'INTEGER') == [2, 0]
                evidence['valid_epoch_recovery'] = 'passed'
                assert all(refused), evidence
            evidence['status'] = 'passed'
        finally:
            if connection is not None:
                connection.close()
            if peer is not None:
                peer.close()
            for process in reversed(runtime.processes):
                process.close(time.monotonic() + 5)
            for log in runtime.root.glob('*.log'):
                shutil.copy2(log, out / log.name)
            evidence['children_after_cleanup'] = {p.name: p.poll()
                for p in runtime.processes}
            (out / 'evidence.json').write_text(json.dumps(evidence, indent=2) + '\n')
    print(json.dumps(evidence))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ['fvwm', 'mfl', 'artifacts-dir']:
        parser.add_argument('--' + name, required=True, type=Path)
    parser.add_argument('--display')
    parser.add_argument('--case', choices=['values', 'multiple', 'admission'], required=True)
    run(parser.parse_args())
