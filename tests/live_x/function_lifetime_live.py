#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or (at your option) any later
# version.
"""Exercise cancellation and retained contexts in command-owned waits."""
import argparse
import ctypes as C
import json
from pathlib import Path
import shutil
import signal
import time

from geometry_transition_live import Client, Pointer
from live_x_harness import HarnessRuntime, start_display_server, wait_for_predicate
from mfl_stream_live import Connection


def run_case(args, label):
    artifact = args.artifacts_dir / label
    artifact.mkdir(parents=True, exist_ok=True)
    evidence = {'case': label, 'status': 'running'}
    with HarnessRuntime(prefix='fvwm-function-lifetime-') as runtime:
        connection = client = pointer = core = None
        held = False
        try:
            display = args.display
            if display is None:
                _, display = start_display_server(runtime, 'Xvfb',
                    ['Xvfb', '-screen', '0', '800x600x24', '-noreset'],
                    'xserver.log', time.monotonic() + 10)
            log = runtime.root / 'debug.log'
            config = runtime.root / 'config'
            config.write_text(f'ModulePath {args.mfl.parent}\n'
                'Style * ClickToFocus, TileCascadePlacement, '
                'FPAllowFocusClickFunction, FPAllowRaiseClickFunction\n'
                'ClickTime 150\nTitleStyle Height 24\n'
                'AddToFunc StartFunction I Module FvwmMFL\n'
                'AddToFunc LifetimeClick I Echo lifetime-entered\n'
                '+ C Echo lifetime-click\n+ D Echo lifetime-double\n'
                'Mouse 1 W N LifetimeClick\n'
                'AddToFunc ExitFunction I Echo lifetime-exit\n')
            core = runtime.start('core', [str(args.fvwm), '-d', display,
                '-f', str(config), '-v', '-o', str(log)], 'core.log',
                runtime.environment({'FVWMMFL_SOCKET_PATH': str(runtime.root)}))
            endpoint = runtime.root / f"fvwm_mfl_{display.replace(':', '_')}.sock"
            wait_for_predicate('function endpoint', endpoint.exists,
                time.monotonic() + 10, process=core)
            connection = Connection(endpoint)
            connection.record()
            connection.send(b'set echo\n')
            connection.echo('lifetime-ready')
            client = Client(display)
            client.create()
            pointer = Pointer(client)
            wait_for_predicate('managed function target', client.frame,
                time.monotonic() + 5, process=core)
            connection.send(f'WindowId {client.window} Move 100p 100p\n'.encode())
            connection.echo('lifetime-positioned')

            def observed(text):
                return 'CMD_Echo: ' + text in log.read_text()

            def wait_echo(text):
                wait_for_predicate(text, lambda: observed(text),
                    time.monotonic() + 5, process=core)

            def escape():
                client.x.XKeysymToKeycode.argtypes = [C.c_void_p, C.c_ulong]
                client.x.XKeysymToKeycode.restype = C.c_uint
                pointer.x.XTestFakeKeyEvent.argtypes = [C.c_void_p, C.c_uint,
                    C.c_int, C.c_ulong]
                key = client.x.XKeysymToKeycode(client.display, 0xff1b)
                for down in (True, False):
                    assert pointer.x.XTestFakeKeyEvent(client.display, key, down, 0)
                client.x.XSync(client.display, 0)

            def replacement_window():
                replacement = Client(display)
                replacement.window = replacement.x.XCreateSimpleWindow(
                    replacement.display, replacement.root, 400, 300,
                    180, 100, 0, 0, 0)
                replacement.x.XStoreName(replacement.display,
                    replacement.window, b'Lifetime replacement')
                replacement.x.XMapWindow(replacement.display, replacement.window)
                replacement.x.XSync(replacement.display, 0)
                return replacement

            if label.startswith('selection'):
                # No window context: Raise must wait for a target selection.
                connection.send(b'Raise\n')
                wait_for_predicate('target selection grab', pointer.grabbed,
                    time.monotonic() + 5, process=core)
                if label == 'selection-escape':
                    escape()
                    connection.echo('selection-returned')
                    assert not pointer.grabbed()
                else:
                    started = time.monotonic()
                    core.process.send_signal(signal.SIGTERM)
                    assert core.wait_until_exit(time.monotonic() + 2) == 0
                    evidence['shutdown_seconds'] = time.monotonic() - started
            elif label.startswith('classification'):
                if label == 'classification-late-destroy':
                    connection.send(b'AddToFunc LifetimeClick J Echo lifetime-late\n'
                        b'+ J Wait "Lifetime replacement"\n')
                    connection.echo('late-defined')
                pointer.motion(250, 210)
                pointer.button(True)
                held = True
                wait_echo('lifetime-entered')
                wait_for_predicate('classification grab', pointer.grabbed,
                    time.monotonic() + 5, process=core)
                if label == 'classification-sigterm':
                    core.process.send_signal(signal.SIGTERM)
                    assert core.wait_until_exit(time.monotonic() + 2) == 0
                    assert not observed('lifetime-click')
                    assert not observed('lifetime-double')
                else:
                    if label == 'classification-destroy':
                        client.destroy()
                        # Lifecycle must stay queued until classification
                        # unwinds. Release the physical button separately.
                        time.sleep(.1)
                    pointer.button(False)
                    held = False
                    if label == 'classification-late-destroy':
                        wait_echo('lifetime-late')
                        client.destroy()
                        replacement = replacement_window()
                        try:
                            connection.echo('late-resumed')
                        finally:
                            replacement.close()
                    if label == 'classification-click':
                        wait_echo('lifetime-click')
                    connection.echo('classification-returned')
                    if label in ('classification-destroy',
                            'classification-late-destroy'):
                        assert not observed('lifetime-click')
                        assert not observed('lifetime-double')
            else:
                connection.send(b'AddToFunc LifetimeWait I Echo lifetime-wait\n'
                    b'+ I Wait "Lifetime replacement"\n'
                    b'+ I Echo lifetime-resumed\n')
                connection.echo('wait-defined')
                connection.send((f'Schedule 100 DestroyFunc LifetimeWait\n'
                    f'WindowId {client.window} LifetimeWait\n').encode())
                wait_echo('lifetime-wait')
                # The active function cannot be destroyed while Wait retains
                # its items. Its target may be destroyed by nested dispatch.
                client.destroy()
                wait_for_predicate('active function retained',
                    lambda: 'Function is in use' in log.read_text(),
                    time.monotonic() + 5, process=core)
                replacement = replacement_window()
                try:
                    wait_echo('lifetime-resumed')
                    connection.echo('wait-resumed')
                    connection.send(b'DestroyFunc LifetimeWait\n')
                    connection.echo('function-retired')
                    assert 'Function is in use' in log.read_text()
                finally:
                    replacement.close()
            if core.poll() is None:
                connection.send(b'Quit\n')
                assert core.wait_until_exit(time.monotonic() + 5) == 0
            assert observed('lifetime-exit'), 'ExitFunction must still execute'
            evidence['status'] = 'passed'
        finally:
            if held and pointer:
                pointer.button(False)
            if connection:
                connection.close()
            if client:
                client.close()
            for process in reversed(runtime.processes):
                process.close(time.monotonic() + 5)
            evidence['children_after_cleanup'] = {p.name: p.poll()
                for p in runtime.processes}
            for path in runtime.root.rglob('*'):
                if path.is_file():
                    target = artifact / path.relative_to(runtime.root)
                    target.parent.mkdir(parents=True, exist_ok=True)
                    shutil.copy2(path, target)
            (artifact / 'evidence.json').write_text(json.dumps(evidence, indent=2) + '\n')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ['fvwm', 'mfl', 'artifacts-dir']:
        parser.add_argument('--' + name, type=Path, required=True)
    parser.add_argument('--display')
    parser.add_argument('--case', choices=['selection', 'classification', 'wait',
        'selection-sigterm', 'classification-sigterm', 'classification-destroy'],
        required=True)
    args = parser.parse_args()
    args.fvwm, args.mfl = args.fvwm.resolve(), args.mfl.resolve()
    cases = {'selection': ['selection-escape', 'selection-sigterm'],
        'classification': ['classification-click', 'classification-destroy',
            'classification-sigterm', 'classification-late-destroy'],
        'wait': ['wait']}
    for case in cases.get(args.case, [args.case]):
        run_case(args, case)
    print('Command wait lifetimes passed: ' + args.case)
