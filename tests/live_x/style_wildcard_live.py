#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
"""Check wildcard style application/removal on real managed X clients."""
import argparse
import ctypes as C
import hashlib
import json
from pathlib import Path
import shutil
import sys
import time

from geometry_transition_live import Client
from live_x_harness import HarnessRuntime, start_display_server, wait_for_predicate
from mfl_stream_live import Connection


def client_checks(args):
    out = args.artifacts_dir.resolve()
    out.mkdir(parents=True, exist_ok=True)
    endpoint = args.endpoint
    evidence = {'core_sha256': hashlib.sha256(args.fvwm.read_bytes()).hexdigest(),
                'mfl_sha256': hashlib.sha256(args.mfl.read_bytes()).hexdigest(),
                'driver_sha256': hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
                'cases': [], 'status': 'running'}
    connection = client = None
    try:
        connection = Connection(endpoint)
        connection.record()
        connection.send(b'set echo\n')
        connection.echo('style-ready')
        client = Client(args.display)
        client.create()
        wait_for_predicate('managed style client', client.frame,
            time.monotonic() + 10, process=None)
        x, d = client.x, client.display
        x.XInternAtom.argtypes = [C.c_void_p, C.c_char_p, C.c_int]
        x.XInternAtom.restype = C.c_ulong
        x.XGetWindowProperty.argtypes = [C.c_void_p, C.c_ulong, C.c_ulong,
            C.c_long, C.c_long, C.c_int, C.c_ulong, C.POINTER(C.c_ulong),
            C.POINTER(C.c_int), C.POINTER(C.c_ulong), C.POINTER(C.c_ulong),
            C.POINTER(C.c_void_p)]
        x.XGetWindowProperty.restype = C.c_int
        atom = x.XInternAtom(d, b'_NET_FRAME_EXTENTS', 0)

        def extents():
            actual, count, after = C.c_ulong(), C.c_ulong(), C.c_ulong()
            fmt, data = C.c_int(), C.c_void_p()
            status = x.XGetWindowProperty(d, client.window, atom, 0, 4,
                0, 6, C.byref(actual), C.byref(fmt), C.byref(count),
                C.byref(after), C.byref(data))
            try:
                assert status == 0
                if actual.value == 0:
                    return None
                assert actual.value == 6
                assert fmt.value == 32 and count.value == 4 and after.value == 0
                return list(C.cast(data, C.POINTER(C.c_ulong))[:4])
            finally:
                if data:
                    x.XFree(data)

        def check(width):
            def ready():
                if extents() != [width]*4:
                    return False
                frame = client.frame()
                if frame is None:
                    return False
                _, _, cw, ch = client.geometry(client.window)
                _, _, fw, fh = client.geometry(frame)
                return (fw, fh) == (cw + 2*width, ch + 2*width)
            wait_for_predicate('style extents and frame geometry', ready,
                time.monotonic() + 10, process=None)

        check(3)
        noise = ''.join(f'Style {selector} BorderWidth 19\n'
                        for field in ('compatibility', 'Resource', 'Class',
                                      'Name', 'Icon')
                        for i in range(40)
                        for selector in [f'index-no-client-{i}' if
                                         field == 'compatibility' else
                                         f'({field} index-no-client-{i})'])
        noise += ''.join(f'Style (WindowID 0x{0x7f000000+i:x}) BorderWidth 19\n'
                         for i in range(40))
        connection.send((noise + 'UpdateStyles\n').encode())
        connection.echo('index-noise-ready')
        check(3)
        evidence['index_noise_rules'] = 240
        cases = [('quoted-literal', '"FVWM transition probe"', 15),
                 ('minimum', 'FVWM' + '?'*40 + '*', 3),
                 ('suffix', 'FVWM?*wrong', 3),
                 ('single-star', 'FVWM*probe', 7),
                 ('one-star-questions', 'FVWM?*probe', 9),
                 ('multi-star-cursors', 'FVWM**probe', 11),
                 ('non-overlap', '"FVWM transition *transition probe"', 3),
                 ('fixed-width-hit', 'FVWM?transition?probe', 17),
                 ('fixed-width-miss', 'FVWM?transition?pro?', 3),
                 ('empty-tail', '"FVWM transition probe**"', 3),
                 ('deep-stars', 'FVWM' + '*'*512 + '?probe', 19),
                 ('escaped-fixed-hit', r'"FVWM \\transition pro?e"', 17),
                 ('escaped-fixed-miss', r'"FVWM \\transition pro?"', 3),
                 ('escaped-star-hit', r'"FVWM \\transition*pro?e"', 7),
                 ('escaped-star-miss', r'"FVWM \\transition*pr\\?be"', 3),
                 ('escaped-cursor-hit', r'"FVWM \\t**r*probe"', 11),
                 ('escaped-empty-tail', r'"FVWM \\transition probe**"', 3)]
        for label, selector, width in cases:
            connection.send((f'Style (Name {selector}) BorderWidth {width if width != 3 else 13}, '
                f'HandleWidth {width if width != 3 else 13}\nUpdateStyles\n').encode())
            connection.echo(label + '-applied')
            check(width)
            connection.send((f'DestroyStyle (Name {selector})\n'
                'UpdateStyles\n').encode())
            connection.echo(label + '-removed')
            check(3)
            evidence['cases'].append({'case': label, 'selector': selector,
                'applied_width': width, 'removed_width': 3})
        evidence['lookup_candidates'] = []
        for label, selector in (
                ('compatibility', '"FVWM transition probe"'),
                ('explicit', '(Name "FVWM transition probe", Resource *)'),
                ('window-id', f'(WindowID 0x{client.window:x})')):
            connection.send((f'Style {selector} BorderWidth 13, HandleWidth 13\n'
                             'UpdateStyles\n').encode())
            connection.echo('index-' + label + '-applied')
            check(13)
            connection.send((f'DestroyStyle {selector}\nUpdateStyles\n').encode())
            connection.echo('index-' + label + '-removed')
            check(3)
            evidence['lookup_candidates'].append({'case': label,
                'applied_width': 13, 'removed_width': 3})
        evidence['simplification'] = []
        pairs = [
            ('literal', '"FVWM transition probe"', '"other client"'),
            ('prefix', 'FVWM*probe', 'Other*probe'),
            ('suffix', '*transition*probe', '*other'),
            ('length', 'FVWM?transition?probe', 'FVWM' + '?'*18 + '*'),
            ('position', 'FVWM?t???????????????', 'FVWM?x???????????????'),
            ('literal-member', '"FVWM transition probe"', 'FVWM*other*probe'),
            ('escaped-length', r'"FVWM \\transition pro?e"',
             r'"FVWM \\transition pro??e*"'),
            ('escaped-position', r'"FVWM \\transition pro?e"',
             r'"FVWM \\transition prx?e"'),
            ('escaped-member', '"FVWM transition probe"',
             r'"FVWM *\\other*probe"'),
            ('anchor-head', r'"FVWM \\transition pr?be*"',
             r'"FVWM \\transition px?be*"'),
            ('anchor-tail', r'"*transition pr?b\\e"',
             r'"*transition px?b\\e"'),
            ('zero-slack', 'FVWM?transition?probe',
             '*FVWM?transition?pxobe*')]
        for label, selected, other in pairs:
            # Both fields conflict with the middle rule. Disjointness permits
            # the equal selectors to merge without changing this client.
            connection.send((
                f'Style (Name {selected}) BorderWidth 5, HandleWidth 5\n'
                f'Style (Name {other}) BorderWidth 9, HandleWidth 9, '
                'TitleFormat other %n\n'
                f'Style (Name {selected}) TitleFormat merged %n\n'
                'UpdateStyles\n').encode())
            connection.echo('simplify-' + label + '-applied')
            check(5)
            connection.send((f'DestroyStyle (Name {selected})\n'
                f'DestroyStyle (Name {other})\nUpdateStyles\n').encode())
            connection.echo('simplify-' + label + '-removed')
            check(3)
            evidence['simplification'].append({
                'case': label, 'selected': selected, 'other': other,
                'applied_width': 5, 'removed_width': 3})
        evidence['shared_conditions'] = []
        conditions = [
            ('suffix-hit', '*probe', 13),
            ('suffix-miss', '*prope', 3),
            ('plain-hit', 'FVWM**probe', 13),
            ('plain-miss', 'FVWM*a*a*a*absent', 3),
            ('escaped-hit', r'FVWM*\\probe', 13),
            ('escaped-miss', r'FVWM*\\?probe', 3),
            ('legacy-empty-tail', 'FVWM*probe**', 3),
            ('deep-plain', 'FVWM' + '*'*512 + '?probe', 13),
            ('deep-escaped', r'FVWM\\ t' + '*'*512 + '?probe', 13),
        ]
        for label, pattern, width in conditions:
            connection.send((f'All ("{pattern}") WindowStyle '
                'BorderWidth 13, HandleWidth 13\nUpdateStyles\n').encode())
            connection.echo('condition-' + label + '-applied')
            check(width)
            connection.send((f'WindowId {client.window} DestroyWindowStyle\n'
                'UpdateStyles\n').encode())
            connection.echo('condition-' + label + '-removed')
            check(3)
            evidence['shared_conditions'].append({'case': label,
                'applied_width': width, 'removed_width': 3})
        evidence['status'] = 'passed'
    finally:
        # Persist partial observations before potentially blocking X cleanup.
        (out / 'results.json').write_text(json.dumps(evidence, indent=2)+'\n')
        if connection:
            connection.close()
        if client:
            client.close()
    print(f'Real-client styles: {len(cases)} wildcard and '
          f'{len(pairs)} simplification cases passed')


def run(args):
    out = args.artifacts_dir.resolve()
    out.mkdir(parents=True, exist_ok=True)
    with HarnessRuntime(prefix='fvwm-style-wildcard-') as runtime:
        receipt = {'status': 'running'}
        try:
            display = args.display
            if display is None:
                _, display = start_display_server(runtime, 'Xvfb',
                    ['Xvfb', '-screen', '0', '800x600x24', '-noreset'],
                    'server.log', time.monotonic() + 15)
            config = runtime.root / 'config'
            # Automatic placement is necessary for an unattended client.
            config.write_text(f'ModulePath {args.mfl.parent}\n'
                'Style * NoTitle, BorderWidth 3, HandleWidth 3, TileCascadePlacement\n'
                'AddToFunc StartFunction I Module FvwmMFL\n')
            core = runtime.start('core', [str(args.fvwm), '-d', display,
                '-f', str(config)], 'core.log', runtime.environment({
                    'FVWMMFL_SOCKET_PATH': str(runtime.root)}))
            endpoint = runtime.root / f"fvwm_mfl_{display.replace(':', '_')}.sock"
            wait_for_predicate('style endpoint', endpoint.exists,
                time.monotonic() + 15, process=core)
            # Xlib calls can block while another client owns a server grab.
            # Keep them in a supervised worker so the controller always owns
            # bounded teardown of the worker, WM/module group and server.
            worker = runtime.start('client-checks', [
                sys.executable, str(Path(__file__).resolve()),
                '--client-worker', '--endpoint', str(endpoint),
                '--display', display, '--fvwm', str(args.fvwm),
                '--mfl', str(args.mfl), '--artifacts-dir', str(out)],
                'client-checks.log', runtime.environment())
            status = worker.wait_until_exit(time.monotonic() + 90)
            receipt.update(worker_status=status, core_status=core.poll())
            assert status == 0 and core.poll() is None
            assert json.loads((out / 'results.json').read_text())['status'] == 'passed'
            receipt['status'] = 'passed'
        finally:
            for process in reversed(runtime.processes):
                process.close(time.monotonic() + 5)
            receipt['children_after_cleanup'] = {
                process.name: process.poll() for process in runtime.processes}
            for path in runtime.root.iterdir():
                if path.is_file():
                    shutil.copy2(path, out / path.name)
            (out / 'supervisor.json').write_text(json.dumps(receipt, indent=2)+'\n')
    print('Supervised style wildcard client checks and teardown passed')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('fvwm', 'mfl', 'artifacts-dir'):
        parser.add_argument('--' + name, type=Path, required=True)
    parser.add_argument('--display')
    parser.add_argument('--client-worker', action='store_true')
    parser.add_argument('--endpoint', type=Path)
    args = parser.parse_args()
    args.fvwm, args.mfl = args.fvwm.resolve(), args.mfl.resolve()
    if args.client_worker:
        assert args.endpoint and args.display
        client_checks(args)
    else:
        run(args)
