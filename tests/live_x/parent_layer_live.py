#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
"""Check public transient hints, Layer ancestry and retired real windows."""
import argparse
import ctypes as C
import json
from pathlib import Path
import shutil
import time

from geometry_transition_live import Client
from live_x_harness import HarnessRuntime, start_display_server, wait_for_predicate
from mfl_stream_live import Connection


def run(args):
    args.artifacts_dir.mkdir(parents=True, exist_ok=True)
    evidence, clients = {}, []
    connection = None
    with HarnessRuntime(prefix='fvwm-parent-layer-') as runtime:
        try:
            display = args.display
            if display is None:
                _, display = start_display_server(runtime, 'Xvfb',
                    ['Xvfb', '-screen', '0', '1000x600x24', '-noreset'],
                    'xserver.log', time.monotonic() + 10)
            config, log = runtime.root / 'config', runtime.root / 'debug.log'
            config.write_text(f'ModulePath {args.mfl.parent}\nDefaultFont fixed\n'
                'Style * DecorateTransient, ClickToFocus, Layer 4, '
                'StackTransientParent, DontRaiseTransient, DontLowerTransient, '
                'TileCascadePlacement\n'
                'AddToFunc StartFunction I Module FvwmMFL\n')
            core = runtime.start('core', [str(args.fvwm), '-d', display,
                '-f', str(config), '-v', '-o', str(log)], 'core.log',
                runtime.environment({'FVWMMFL_SOCKET_PATH': str(runtime.root)}))
            endpoint = runtime.root / f"fvwm_mfl_{display.replace(':', '_')}.sock"
            wait_for_predicate('parent endpoint', endpoint.exists,
                time.monotonic() + 10, process=core)
            connection = Connection(endpoint)
            connection.record()
            connection.send(b'set echo\n')
            connection.echo('parent-ready')

            def create(parent=None):
                target = Client(display)
                clients.append(target)
                x, d = target.x, target.display
                x.XSetTransientForHint.argtypes = [C.c_void_p, C.c_ulong,
                    C.c_ulong]
                x.XGetTransientForHint.argtypes = [C.c_void_p, C.c_ulong,
                    C.POINTER(C.c_ulong)]
                x.XGetInputFocus.argtypes = [C.c_void_p,
                    C.POINTER(C.c_ulong), C.POINTER(C.c_int)]
                target.window = x.XCreateSimpleWindow(d, target.root,
                    30, 30, 120, 60, 0, 0, 0)
                x.XStoreName(d, target.window, b'Parent layer probe')
                if parent is not None:
                    x.XSetTransientForHint(d, target.window, parent)
                x.XMapWindow(d, target.window)
                x.XSync(d, 0)
                wait_for_predicate('parent frame', target.frame,
                    time.monotonic() + 5, process=core)
                connection.echo(f'parent-mapped-{len(clients)}')
                return target

            def observe(tag, expected):
                for index, layer in expected.items():
                    connection.send(f'WindowId {clients[index].window} Echo '
                        f'{tag}-{index}-$[w.layer]\n'.encode())
                connection.echo(tag + '-observed')
                text = log.read_text()
                for index, layer in expected.items():
                    assert f'CMD_Echo: {tag}-{index}-{layer}' in text, text

            def layer(target, value, tag, expected):
                connection.send(f'WindowId {target.window} Layer 0 '
                    f'{value}\n'.encode())
                observe(tag, expected)

            first = create()
            for _ in range(15):
                create(clients[-1].window)
            unrelated = create()
            last = clients[15]
            connection.send(f'WindowId {last.window} Focus\n'.encode())
            connection.echo('parent-focus')
            def focused():
                focus, revert = C.c_ulong(), C.c_int()
                last.x.XGetInputFocus(last.display, C.byref(focus),
                    C.byref(revert))
                return focus.value == last.window
            wait_for_predicate('parent focus confirmed', focused,
                time.monotonic() + 5, process=core)
            expected = dict.fromkeys(range(17), 4)
            expected[0] = 6
            layer(last, 6, 'deep-ancestor', expected)
            assert focused()
            evidence['16-window-ancestor-and-focus'] = 'passed'

            # Closing a public cycle is rejected by existing hint validation.
            # The client property itself remains intact; only the WM's selected
            # parent changes. This is distinct from synthetic internal cycles.
            first.x.XSetTransientForHint(first.display, first.window, last.window)
            first.x.XSync(first.display, 0)
            expected[0] = 2
            layer(last, 2, 'rejected-public-cycle', expected)
            value = C.c_ulong()
            assert first.x.XGetTransientForHint(first.display, first.window,
                C.byref(value)) and value.value == last.window
            assert 'Cyclic transient parent chain' not in log.read_text()
            evidence['public-cycle-hint-and-existing-rejection'] = 'passed'

            first.destroy()
            connection.echo('parent-destroyed')
            expected.pop(0)
            expected[1] = 6
            layer(last, 6, 'retired-ancestor', expected)
            assert focused()
            evidence['retired-ancestor-and-following-command'] = 'passed'

            for tag, parent in [('root', last.root), ('missing', 0x60000000)]:
                target = create(parent)
                expected[len(clients) - 1] = 2
                layer(target, 2, tag + '-parent', expected)
            target = create()
            target.x.XSetTransientForHint(target.display, target.window,
                target.window)
            target.x.XSync(target.display, 0)
            expected[len(clients) - 1] = 6
            layer(target, 6, 'self-parent', expected)
            evidence['root-missing-self-and-unrelated-identity'] = 'passed'
            connection.echo('parent-final-command')
            for failure in ('X error', 'AddressSanitizer', 'runtime error:'):
                assert failure not in log.read_text(), log.read_text()
        except BaseException as error:
            evidence['error'] = repr(error)
            raise
        finally:
            if connection:
                connection.close()
            for target in reversed(clients):
                target.close()
            for path in runtime.root.glob('*.log'):
                shutil.copy2(path, args.artifacts_dir / path.name)
            (args.artifacts_dir / 'evidence.json').write_text(
                json.dumps(evidence, indent=2) + '\n')
    print(json.dumps(evidence, indent=2))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(allow_abbrev=False)
    for name in ('fvwm', 'mfl', 'artifacts-dir'):
        parser.add_argument('--' + name, type=Path, required=True)
    parser.add_argument('--display')
    run(parser.parse_args())
