#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
"""Independently observe advertised atoms and basic EWMH publication contracts."""
import argparse
import ctypes as C
import hashlib
import json
from pathlib import Path
import re
import shutil
import time

from geometry_transition_live import Client
from live_x_harness import HarnessRuntime, start_display_server, wait_for_predicate
from mfl_stream_live import Connection


def run(options):
    options.artifacts_dir.mkdir(parents=True, exist_ok=True)
    evidence = {}
    with HarnessRuntime(prefix='fvwm-ewmh-inventory-') as runtime:
        connection = None
        clients = []
        try:
            display = options.display
            if display is None:
                _, display = start_display_server(runtime, 'Xvfb',
                    ['Xvfb', '-screen', '0', '800x600x24', '-noreset'],
                    'xserver.log', time.monotonic()+10)
            config = runtime.root / 'config'
            config.write_text(f'ModulePath {options.mfl.parent}\n'
                'Style * TileCascadePlacement\nAddToFunc StartFunction I Module FvwmMFL\n')
            core = runtime.start('core', [str(options.fvwm), '-d', display,
                '-f', str(config)], 'core.log', runtime.environment({
                    'FVWMMFL_SOCKET_PATH': str(runtime.root)}))
            endpoint = runtime.root / f"fvwm_mfl_{display.replace(':', '_')}.sock"
            wait_for_predicate('EWMH endpoint', endpoint.exists,
                time.monotonic()+10, process=core)
            connection = Connection(endpoint)
            connection.record()
            connection.send(b'set echo\n')
            connection.echo('inventory-ready')
            client = Client(display)
            clients.append(client)
            x, d = client.x, client.display
            x.XInternAtom.argtypes = [C.c_void_p, C.c_char_p, C.c_int]
            x.XInternAtom.restype = C.c_ulong
            x.XGetAtomName.argtypes = [C.c_void_p, C.c_ulong]
            x.XGetAtomName.restype = C.c_void_p
            x.XGetWindowProperty.argtypes = [C.c_void_p, C.c_ulong, C.c_ulong,
                C.c_long, C.c_long, C.c_int, C.c_ulong, C.POINTER(C.c_ulong),
                C.POINTER(C.c_int), C.POINTER(C.c_ulong), C.POINTER(C.c_ulong),
                C.POINTER(C.c_void_p)]
            x.XGetWindowProperty.restype = C.c_int
            atom = lambda name: x.XInternAtom(d, name.encode(), 0)

            def property_value(window, name, kind, fmt):
                actual, count, after = C.c_ulong(), C.c_ulong(), C.c_ulong()
                format_ = C.c_int()
                data = C.c_void_p()
                status = x.XGetWindowProperty(d, window, atom(name), 0, 1024,
                    0, 0, C.byref(actual), C.byref(format_), C.byref(count),
                    C.byref(after), C.byref(data))
                try:
                    assert status == 0 and actual.value == atom(kind), name
                    assert format_.value == fmt and after.value == 0, name
                    assert count.value <= (1024 if fmt == 32 else 4096), name
                    if fmt == 32:
                        # Xlib expands protocol CARD32 to native unsigned long.
                        values = C.cast(data, C.POINTER(C.c_ulong))
                        return [values[i] for i in range(count.value)]
                    return C.string_at(data, count.value) if count.value else b''
                finally:
                    if data:
                        x.XFree(data)

            def atom_name(value):
                text = x.XGetAtomName(d, value)
                assert text
                try:
                    return C.string_at(text).decode('ascii')
                finally:
                    x.XFree(text)

            supported = property_value(client.root, '_NET_SUPPORTED', 'ATOM', 32)
            names = [atom_name(value) for value in supported]
            assert len(names) == len(set(names)), names
            source = options.ewmh_source.read_bytes()
            tables = re.findall(rb'ewmh_atom ewmh_atom_\w+\[\]\s*=\s*\{(.*?)\n\};',
                source, re.S)
            assert len(tables) == 9, 'review inventory parser after table changes'
            declared = [name.decode('ascii') for table in tables
                for name in re.findall(rb'ENTRY\(\s*"([^"]+)"', table)]
            assert sorted(names) == sorted(declared), (names, declared)
            candidates = ['_NET_WM_STRUT_PARTIAL', '_NET_WM_FULLSCREEN_MONITORS',
                '_NET_WM_USER_TIME', '_NET_WM_USER_TIME_WINDOW',
                '_NET_WM_PING', '_NET_WM_SYNC_REQUEST', '_NET_REQUEST_FRAME_EXTENTS']
            evidence['advertisement'] = {'names': names,
                'source_sha256': hashlib.sha256(source).hexdigest(),
                'candidate_presence': {name: name in names for name in candidates}}
            check = property_value(client.root, '_NET_SUPPORTING_WM_CHECK', 'WINDOW', 32)
            assert len(check) == 1 and check[0] != client.root
            assert property_value(check[0], '_NET_SUPPORTING_WM_CHECK', 'WINDOW', 32) == check
            name = property_value(check[0], '_NET_WM_NAME', 'UTF8_STRING', 8)
            assert name and b'\0' not in name
            evidence['supporting_wm_check'] = {'window': check[0], 'name': name.decode('utf-8')}
            desktop_count = property_value(client.root, '_NET_NUMBER_OF_DESKTOPS', 'CARDINAL', 32)
            current = property_value(client.root, '_NET_CURRENT_DESKTOP', 'CARDINAL', 32)
            assert len(desktop_count) == len(current) == 1
            assert 0 <= current[0] < desktop_count[0]
            work = property_value(client.root, '_NET_WORKAREA', 'CARDINAL', 32)
            assert len(work) % 4 == 0, work
            evidence['desktop_publication'] = {'count': desktop_count[0],
                'current': current[0], 'workarea': work,
                'workarea_desktop_count_matches': len(work) == desktop_count[0]*4}
            # This is an audit of current publication, not a blanket conformance
            # gate. Preserve required-clause findings for the EWMH-01 owner.
            evidence['required_clause_findings'] = ([] if len(work) == desktop_count[0]*4
                else ['EWMH 1.5 section 3.9: workarea count differs from desktops'])
            for index in range(3):
                if index:
                    clients.append(Client(display))
                clients[index].create()
                wait_for_predicate('mapped independent client',
                    lambda: clients[index].frame() is not None,
                    time.monotonic()+5, process=core)
            windows = [item.window for item in clients]
            wait_for_predicate('mapping order publication', lambda:
                property_value(client.root, '_NET_CLIENT_LIST', 'WINDOW', 32) == windows,
                time.monotonic()+5, process=core)
            connection.send(f'WindowId {windows[0]} Raise\n'.encode())
            connection.echo('inventory-raise')
            expected = windows[1:]+windows[:1]
            wait_for_predicate('stacking publication', lambda:
                property_value(client.root, '_NET_CLIENT_LIST_STACKING', 'WINDOW', 32) == expected,
                time.monotonic()+5, process=core)
            extents = []
            for item in clients:
                value = property_value(item.window, '_NET_FRAME_EXTENTS', 'CARDINAL', 32)
                assert len(value) == 4
                _, _, width, height = item.geometry(item.window)
                _, _, frame_width, frame_height = item.geometry(item.frame())
                assert frame_width == width+value[0]+value[1], value
                assert frame_height == height+value[2]+value[3], value
                extents.append(value)
            evidence['clients_and_extents'] = {'mapping_order': windows,
                'stacking_after_raise': expected, 'actual_pixel_extents': extents}
            # Client destruction must remove stale IDs from both publications.
            client.x.XDestroyWindow(d, windows[0])
            client.window = None
            client.x.XSync(d, 0)
            wait_for_predicate('destroyed client removed', lambda:
                property_value(client.root, '_NET_CLIENT_LIST', 'WINDOW', 32) == windows[1:] and
                property_value(client.root, '_NET_CLIENT_LIST_STACKING', 'WINDOW', 32) == windows[1:],
                time.monotonic()+5, process=core)
            connection.echo('inventory-recovered')
            evidence['client_death_and_continued_commands'] = 'passed'
        finally:
            if connection:
                connection.close()
            for client in clients:
                client.close()
            for path in runtime.root.iterdir():
                if path.is_file():
                    shutil.copy2(path, options.artifacts_dir / path.name)
            (options.artifacts_dir / 'results.json').write_text(json.dumps(evidence, indent=2)+'\n')
    print('Independent EWMH advertisement, identity, clients and extents passed')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('fvwm', 'mfl', 'ewmh-source', 'artifacts-dir'):
        parser.add_argument('--'+name, type=Path, required=True)
    parser.add_argument('--display')
    options = parser.parse_args()
    for name in ('fvwm', 'mfl', 'ewmh_source', 'artifacts_dir'):
        setattr(options, name, getattr(options, name).resolve())
    run(options)
