#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
"""Verify transient layer publication, focus, stack order and parent teardown."""
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
    with HarnessRuntime(prefix='fvwm-stack-layer-') as runtime:
        try:
            display = args.display
            if display is None:
                _, display = start_display_server(runtime, 'Xvfb',
                    ['Xvfb', '-screen', '0', '1000x600x24', '-noreset'],
                    'xserver.log', time.monotonic() + 10)
            config, log = runtime.root / 'config', runtime.root / 'debug.log'
            config.write_text(f'ModulePath {args.mfl.parent}\nDefaultFont fixed\n'
                'DefaultLayers 2 4 6\nColorset 0 fg white, bg blue\n'
                'Colorset 1 fg black, bg green\nTitleStyle Height 24\n'
                'Style * DecorateTransient, ClickToFocus, Colorset 0, '
                'HilightColorset 1, BorderWidth 3, HandleWidth 3, Layer 4, '
                'StackTransientParent, RaiseTransient, LowerTransient, '
                'TileCascadePlacement\nAddToFunc StartFunction I Module FvwmMFL\n')
            core = runtime.start('core', [str(args.fvwm), '-d', display,
                '-f', str(config), '-v', '-o', str(log)], 'core.log',
                runtime.environment({'FVWMMFL_SOCKET_PATH': str(runtime.root)}))
            endpoint = runtime.root / f"fvwm_mfl_{display.replace(':', '_')}.sock"
            wait_for_predicate('layer endpoint', endpoint.exists,
                time.monotonic() + 10, process=core)
            connection = Connection(endpoint)
            connection.record()
            connection.send(b'set echo\n')
            connection.echo('layer-ready')
            for index in range(4):
                client = Client(display)
                clients.append(client)
                x, d = client.x, client.display
                client.window = x.XCreateSimpleWindow(d, client.root,
                    50 + index * 220, 100, 160, 80, 0, 0, 0)
                x.XStoreName(d, client.window, f'Layer probe {index}'.encode())
                if index in (1, 2):
                    x.XSetTransientForHint.argtypes = [C.c_void_p, C.c_ulong,
                        C.c_ulong]
                    x.XSetTransientForHint(d, client.window,
                        clients[index - 1].window)
                x.XMapWindow(d, client.window)
                x.XSync(d, 0)
                wait_for_predicate('layer frame', client.frame,
                    time.monotonic() + 5, process=core)
                connection.send(f'WindowId {client.window} Move '
                    f'{20 + index * 220}p 100p\n'.encode())
            client = clients[0]
            x, d = client.x, client.display
            x.XInternAtom.argtypes = [C.c_void_p, C.c_char_p, C.c_int]
            x.XInternAtom.restype = C.c_ulong
            x.XGetWindowProperty.argtypes = [C.c_void_p, C.c_ulong, C.c_ulong,
                C.c_long, C.c_long, C.c_int, C.c_ulong, C.POINTER(C.c_ulong),
                C.POINTER(C.c_int), C.POINTER(C.c_ulong), C.POINTER(C.c_ulong),
                C.POINTER(C.c_void_p)]
            x.XGetInputFocus.argtypes = [C.c_void_p, C.POINTER(C.c_ulong),
                C.POINTER(C.c_int)]
            x.XGetImage.argtypes = [C.c_void_p, C.c_ulong, C.c_int, C.c_int,
                C.c_uint, C.c_uint, C.c_ulong, C.c_int]
            x.XGetImage.restype = C.c_void_p
            x.XGetPixel.argtypes = [C.c_void_p, C.c_int, C.c_int]
            x.XGetPixel.restype = C.c_ulong
            x.XDestroyImage.argtypes = [C.c_void_p]
            state = x.XInternAtom(d, b'_NET_WM_STATE', 0)
            atoms = {name: x.XInternAtom(d,
                ('_NET_WM_STATE_' + name.upper()).encode(), 0)
                for name in ('above', 'below', 'sticky')}

            def states(target):
                kind, fmt, count, after, data = (C.c_ulong(), C.c_int(),
                    C.c_ulong(), C.c_ulong(), C.c_void_p())
                assert x.XGetWindowProperty(d, target.window, state, 0, 64, 0,
                    0, C.byref(kind), C.byref(fmt), C.byref(count),
                    C.byref(after), C.byref(data)) == 0
                try:
                    values = [] if not data.value else list(
                        C.cast(data, C.POINTER(C.c_ulong))[:count.value])
                    assert not values or (fmt.value == 32 and after.value == 0)
                    return {name: atom in values for name, atom in atoms.items()}
                finally:
                    if data.value:
                        x.XFree(data)

            def title_pixel(target):
                gx, gy, _, _ = target.geometry(target.frame())
                image = x.XGetImage(d, client.root, gx + 20, gy + 8, 1, 1,
                    C.c_ulong(-1).value, 2)
                assert image
                try:
                    return x.XGetPixel(image, 0, 0)
                finally:
                    x.XDestroyImage(image)

            def stack_order(branch):
                root, parent, children, count = (C.c_ulong(), C.c_ulong(),
                    C.POINTER(C.c_ulong)(), C.c_uint())
                assert x.XQueryTree(d, client.root, C.byref(root),
                    C.byref(parent), C.byref(children), C.byref(count))
                try:
                    stack = list(children[:count.value])
                    return [index for _, index in sorted(
                        (stack.index(target.frame()), index)
                        for index, target in enumerate(branch))]
                finally:
                    if children:
                        x.XFree(children)

            connection.send(f'WindowId {clients[3].window} Layer 0 6\n'
                f'WindowId {clients[2].window} Stick on\n'.encode())
            connection.echo('layer-setup')
            unrelated = states(clients[3])
            assert unrelated == {'above': True, 'below': False, 'sticky': False}
            branch = clients[:3]
            for focused in (2, 0, 1):
                connection.send(f'WindowId {branch[focused].window} Focus\n'.encode())
                connection.echo(f'layer-focus-{focused}')
                def has_focus():
                    focus, revert = C.c_ulong(), C.c_int()
                    x.XGetInputFocus(d, C.byref(focus), C.byref(revert))
                    return focus.value == branch[focused].window
                wait_for_predicate('confirmed layer focus', has_focus,
                    time.monotonic() + 5, process=core)
                # Both parent and child invocation resolve the existing branch.
                for invoked in (0, 1):
                    for layer in (6, 2, 4):
                        tag = f'layer-{focused}-{invoked}-{layer}'
                        connection.send(f'WindowId {branch[invoked].window} '
                            f'Layer 0 {layer}\n'.encode())
                        for index, target in enumerate(branch):
                            connection.send(f'WindowId {target.window} Echo '
                                f'{tag}-{index}-$[w.layer]\n'.encode())
                        connection.echo(tag + '-observed')
                        assert all(f'CMD_Echo: {tag}-{i}-{layer}' in log.read_text()
                            for i in range(3))
                        for index, target in enumerate(branch):
                            assert states(target) == {'above': layer == 6,
                                'below': layer == 2, 'sticky': index == 2}
                        assert states(clients[3]) == unrelated
                        assert stack_order(branch) == [0, 1, 2]
                        expected = [65280 if i == focused else 255
                            for i in range(3)]
                        wait_for_predicate('preserved per-window title highlight',
                            lambda: [title_pixel(c) for c in branch] == expected,
                            time.monotonic() + 5, process=core)
            evidence['parent-child-state-stack-focus-and-unrelated-window'] = 'passed'
            # A lost transient parent must not leave stale identity behind.
            parent_window = branch[0].window
            branch[0].destroy()
            connection.echo('layer-parent-destroyed')
            connection.send(f'WindowId {branch[1].window} Layer 0 6\n'.encode())
            connection.echo('layer-survivors-observed')
            for index, target in enumerate(branch[1:], 1):
                assert states(target) == {'above': True, 'below': False,
                    'sticky': index == 2}
            assert states(clients[3]) == unrelated
            assert stack_order(branch[1:]) == [0, 1]
            evidence['destroyed-parent-survivor-publication'] = 'passed'
            for failure in ('X error', 'AddressSanitizer', 'runtime error:'):
                assert failure not in log.read_text(), log.read_text()
        except BaseException as error:
            evidence['error'] = str(error)
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
