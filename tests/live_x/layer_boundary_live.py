#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
"""Check valid layer placement and overflow refusal with real clients."""
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
    clients, evidence, connection = [], {}, None
    with HarnessRuntime(prefix='fvwm-layer-boundary-') as runtime:
        try:
            display = args.display
            if display is None:
                _, display = start_display_server(runtime, 'Xvfb',
                    ['Xvfb', '-screen', '0', '800x600x24', '-noreset'],
                    'xserver.log', time.monotonic() + 10)
            config, log = runtime.root / 'config', runtime.root / 'debug.log'
            config.write_text(f'ModulePath {args.mfl.parent}\nDefaultFont fixed\n'
                'Style * Layer 4, TileCascadePlacement\n'
                'AddToFunc StartFunction I Module FvwmMFL\n')
            core = runtime.start('core', [str(args.fvwm), '-d', display,
                '-f', str(config), '-v', '-o', str(log)], 'core.log',
                runtime.environment({'FVWMMFL_SOCKET_PATH': str(runtime.root)}))
            endpoint = runtime.root / f"fvwm_mfl_{display.replace(':', '_')}.sock"

            def connect():
                nonlocal connection
                try:
                    connection = Connection(endpoint)
                    return True
                except (FileNotFoundError, ConnectionRefusedError):
                    return False

            wait_for_predicate('listening boundary endpoint', connect,
                time.monotonic() + 10, process=core)
            connection.record()
            connection.send(b'set echo\n')
            connection.echo('boundary-ready')
            for _ in range(2):
                client = Client(display)
                clients.append(client)
                client.create()
                wait_for_predicate('boundary frame', client.frame,
                    time.monotonic() + 5, process=core)
            connection.echo('boundary-mapped')

            def order():
                x, d = clients[0].x, clients[0].display
                root, parent, count = C.c_ulong(), C.c_ulong(), C.c_uint()
                children = C.POINTER(C.c_ulong)()
                assert x.XQueryTree(d, clients[0].root, C.byref(root),
                    C.byref(parent), C.byref(children), C.byref(count))
                frames = [c.frame() for c in clients]
                try:
                    return [frames.index(children[i])
                        for i in reversed(range(count.value))
                        if children[i] in frames]
                finally:
                    if children:
                        x.XFree(children)

            def command(index, value, expected, tag):
                connection.send(f'WindowId {clients[index].window} '
                    f'Layer {value}\n'.encode())
                for i, layer in enumerate(expected):
                    connection.send(f'WindowId {clients[i].window} Echo '
                        f'{tag}-{i}-$[w.layer]\n'.encode())
                connection.echo(tag + '-done')
                text = log.read_text()
                for i, layer in enumerate(expected):
                    assert f'CMD_Echo: {tag}-{i}-{layer}' in text, text

            command(0, '0 2147483647', [2147483647, 4], 'maximum')
            assert order() == [0, 1]
            command(1, '0 2147483647', [2147483647] * 2, 'equal-raise')
            assert order() == [1, 0]
            command(1, '-1', [2147483647, 2147483646], 'relative-lower')
            assert order() == [0, 1]
            command(1, '1', [2147483647] * 2, 'relative-maximum')
            assert order() == [1, 0]
            before = order()
            command(0, '1', [2147483647] * 2, 'overflow-refused')
            assert order() == before
            assert 'Layer offset exceeds INT_MAX; keeping layer 2147483647' in log.read_text()
            command(0, '2147483647', [2147483647] * 2, 'large-offset-refused')
            assert order() == before
            evidence['relative-overflow-preserves-layer-and-order'] = 'passed'
            command(0, 'default', [4, 2147483647], 'default')
            command(0, '-8', [0, 2147483647], 'negative-clamp')
            assert order() == [1, 0]
            evidence['maximum-relative-equal-layer-order-and-default'] = 'passed'
            connection.echo('boundary-following-command')
            for failure in ('X error', 'AddressSanitizer', 'runtime error:'):
                assert failure not in log.read_text(), log.read_text()
        except BaseException as error:
            evidence['error'] = repr(error)
            raise
        finally:
            if connection:
                connection.close()
            for client in clients:
                client.close()
            for path in runtime.root.glob('*.log'):
                shutil.copy2(path, args.artifacts_dir / path.name)
            (args.artifacts_dir / 'evidence.json').write_text(json.dumps(
                evidence, indent=2) + '\n')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(allow_abbrev=False)
    for name in ('fvwm', 'mfl', 'artifacts-dir'):
        parser.add_argument('--' + name, type=Path, required=True)
    parser.add_argument('--display')
    run(parser.parse_args())
