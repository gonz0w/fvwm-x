#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or any later version.
"""Directed core queries checked against independent X/configuration observers."""
import argparse
import ctypes as C
import json
import os
from pathlib import Path
import shutil
import signal
import subprocess
import time

from geometry_transition_live import Client
from live_x_harness import (HarnessRuntime, parse_xrandr_monitors,
    parse_xrandr_output_ids, start_display_server, wait_for_predicate)
from mfl_stream_live import Connection
from mfl_request_live import frame, reply


def run(args):
    args.artifacts_dir.mkdir(parents=True, exist_ok=True)
    evidence = {'status': 'running'}
    with HarnessRuntime(prefix='fvwm-core-query-') as runtime:
        core, observer, stopped = None, None, False
        connections = []
        try:
            display = args.display
            if display is None:
                _, display = start_display_server(runtime, 'Xvfb',
                    ['Xvfb', '-screen', '0', '800x600x24', '-noreset'],
                    'xserver.log', time.monotonic() + 10)
            marker = runtime.root / 'must-not-execute'
            config = runtime.root / 'config'
            config.write_text(f'ModulePath {args.mfl.parent}\n'
                'DesktopSize 3x2\nEwmhNumberOfDesktops 4\n'
                'EdgeScroll 0 0\nEdgeThickness 0\nStyle * TileCascadePlacement\n'
                f'Key F1 R N Exec touch {marker}\n'
                'PointerKey F2 R N Echo pointer-key-query\n'
                'Mouse 3 R N Echo mouse-query\n'
                'AddToFunc StartFunction I Module FvwmMFL\n')
            core = runtime.start('core', [str(args.fvwm), '-d', display,
                '-f', str(config), '-v', '-o', str(runtime.root / 'debug.log')],
                'core.log', runtime.environment({'FVWMMFL_SOCKET_PATH': str(runtime.root),
                    'FVWM_USERDIR': str(runtime.root)}))
            endpoint = runtime.root / f"fvwm_mfl_{display.replace(':', '_')}.sock"
            wait_for_predicate('core-query endpoint', endpoint.exists,
                               time.monotonic() + 12, process=core)
            def connect():
                c = Connection(endpoint)
                c.socket.settimeout(10)
                c.record()
                connections.append(c)
                return c
            writer, rpc = connect(), connect()
            writer.send(b'set echo\n')
            writer.echo('core-query-started')
            rpc.send(frame('ro', 'set_mode', mode='read-only'))
            assert reply(rpc, 'ro')['read_only']
            serial = 0
            def query(resource, expected=None, **fields):
                nonlocal serial
                serial += 1
                token = 'core-' + str(serial)
                rpc.send(frame(token, resource=resource, **fields))
                return reply(rpc, token, 'error' if expected else 'ok')
            topology = query('topology')
            assert topology['state'] == 'observed'
            assert topology['scope'] == 'committed-topology'
            assert topology['units'] == 'actual-pixels'
            assert len(topology['topology']['epoch']) == 16
            listing = subprocess.check_output(['xrandr', '--display', display,
                '--listmonitors'], text=True, timeout=5)
            verbose = subprocess.check_output(['xrandr', '--display', display,
                '--verbose'], text=True, timeout=5)
            facts = parse_xrandr_monitors(listing)
            outputs = parse_xrandr_output_ids(verbose)
            monitors = topology['topology']['monitors']
            if facts:
                assert not topology['root_fallback']
                assert {m['name'] for m in monitors} == set(facts)
                for monitor in monitors:
                    fact = facts[monitor['name']]
                    assert monitor['geometry'] == {k: fact[k]
                        for k in ['x', 'y', 'width', 'height']}
                    assert monitor['physical_mm'] == dict(width=fact['mm_width'],
                                                         height=fact['mm_height'])
                    assert monitor['outputs'] == [outputs[n] for n in fact['output_names']]
            else:
                assert topology['root_fallback'] and len(monitors) == 1
                assert monitors[0]['name'] == 'FVWM-X-Root'
                assert monitors[0]['outputs'] == []
            evidence['topology'] = dict(observed=topology, xrandr_monitors=listing,
                                       xrandr_outputs=verbose)

            observer = Client(display)
            x, d = observer.x, observer.display
            x.XInternAtom.argtypes = [C.c_void_p, C.c_char_p, C.c_int]
            x.XInternAtom.restype = C.c_ulong
            x.XGetWindowProperty.argtypes = [C.c_void_p, C.c_ulong, C.c_ulong,
                C.c_long, C.c_long, C.c_int, C.c_ulong, C.POINTER(C.c_ulong),
                C.POINTER(C.c_int), C.POINTER(C.c_ulong), C.POINTER(C.c_ulong),
                C.POINTER(C.c_void_p)]
            def prop(name):
                actual, count, after = C.c_ulong(), C.c_ulong(), C.c_ulong()
                format_, data = C.c_int(), C.c_void_p()
                atom = x.XInternAtom(d, name.encode(), 0)
                assert x.XGetWindowProperty(d, observer.root, atom, 0, 256,
                    0, 6, C.byref(actual), C.byref(format_), C.byref(count),
                    C.byref(after), C.byref(data)) == 0
                try:
                    assert actual.value == 6 and format_.value == 32 and after.value == 0
                    values = C.cast(data, C.POINTER(C.c_ulong))
                    return [values[i] for i in range(count.value)]
                finally:
                    if data:
                        x.XFree(data)
            initial = query('workspaces')
            assert initial['work_area_freshness'] == 'cached'
            observer.create()
            wait_for_predicate('observer managed',
                lambda: observer.parent(observer.window) != observer.root,
                time.monotonic() + 5, process=core)
            writer.send(b'GotoDesk 0 2\nGotoPage 1 1\n')
            writer.echo('workspace-change-processed')
            wait_for_predicate('independent desk publication',
                lambda: prop('_NET_CURRENT_DESKTOP') == [2], time.monotonic() + 5,
                process=core)
            current = query('desktop')
            workspaces = query('workspaces')
            assert workspaces['desktop_mode'] == 'global'
            assert len(workspaces['monitors']) == len(monitors)
            for monitor in workspaces['monitors']:
                assert monitor['desk'] == 2
                assert monitor['viewport'] == current['viewport']
                assert monitor['pages'] == dict(width=3, height=2)
                assert monitor['scale_quarters'] >= 4
                assert monitor['work_area'] == current['work_area']
                assert set(monitor['dynamic_work_area']) == {'x', 'y', 'width', 'height'}
            area = workspaces['monitors'][0]['work_area']
            assert prop('_NET_WORKAREA')[8:12] == [area[k]
                for k in ['x', 'y', 'width', 'height']], (prop('_NET_WORKAREA'), area)
            assert query('topology') == topology  # queries never publish topology
            evidence['workspaces'] = workspaces

            first = query('bindings', limit=1)
            generation = first['generation']
            assert len(generation) == 48
            bindings = first['bindings'][:]
            page, pages = first, 1
            while page['next_offset'] is not None:
                assert pages < 256
                page = query('bindings', offset=page['next_offset'], limit=8,
                             generation=generation)
                assert page['generation'] == generation
                bindings.extend(page['bindings'])
                pages += 1
            assert any(b['type'] == 'Key' and b['key'] == 'F1' and
                       b['action'] == f'Exec touch {marker}' for b in bindings)
            assert any(b['type'] == 'PointerKey' and b['key'] == 'F2' for b in bindings)
            assert any(b['type'] == 'MousePress' and b['code'] == 3 for b in bindings)
            assert all(isinstance(b['context_mask'], int) and
                       isinstance(b['modifier_mask'], int) for b in bindings)
            assert not marker.exists()
            assert query('bindings', limit=1)['generation'] == generation
            writer.send(b'Key F3 R N Echo new-binding\n')
            writer.echo('binding-change-processed')
            assert query('bindings', expected=True, offset=first['next_offset'],
                         generation=generation) == 'stale_generation'
            updated = query('bindings')
            assert updated['generation'] != generation
            writer.send(b'Key F3 R N -\n')
            writer.echo('binding-unbound')
            assert query('bindings')['generation'] != updated['generation']
            large = runtime.root / 'large-binding'
            large.write_text('Key F9 R N Nop ' + 'x' * 1100 + '\n')
            writer.send(('Read ' + str(large) + '\n').encode())
            writer.echo('large-binding-loaded')
            assert query('bindings', expected=True, limit=1) == 'query_too_large'
            writer.send(b'Key F9 R N -\n')
            writer.echo('large-binding-removed')
            assert query('bindings')['bindings']
            evidence['bindings'] = dict(entries=len(bindings), pages=pages,
                configuration_observed=True, no_action_execution=True,
                generation_stable_on_queries=True, stale_continuation_refused=True,
                unbind_invalidates=True, oversized_action_refused=True)

            os.kill(core.process.pid, signal.SIGSTOP)
            stopped = True
            rpc.send(frame('cancelled-core', resource='topology') +
                     frame('cancel-core', 'cancel', target='cancelled-core'))
            assert reply(rpc, 'cancelled-core', 'error') == 'wait_cancelled'
            assert reply(rpc, 'cancel-core')['state'] == 'wait-cancelled'
            rpc.send(frame('cancelled-core', resource='workspaces'))
            os.kill(core.process.pid, signal.SIGCONT)
            stopped = False
            assert reply(rpc, 'cancelled-core')['scope'] == 'core-monitors'
            assert query('topology') == topology
            evidence['readonly_cancel_late_result_isolation'] = True
            assert core.poll() is None and not marker.exists()
            evidence['status'] = 'passed'
        finally:
            if stopped:
                os.kill(core.process.pid, signal.SIGCONT)
            for c in connections:
                c.close()
            if observer:
                observer.close()
            if core:
                core.close(time.monotonic() + 5)
            for log in runtime.root.glob('*.log'):
                shutil.copy2(log, args.artifacts_dir / log.name)
            (args.artifacts_dir / 'evidence.json').write_text(json.dumps(evidence, indent=2) + '\n')
    print(json.dumps(evidence))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--fvwm', type=Path, required=True)
    parser.add_argument('--mfl', type=Path, required=True)
    parser.add_argument('--display')
    parser.add_argument('--artifacts-dir', type=Path, required=True)
    run(parser.parse_args())
