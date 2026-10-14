#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
"""Exercise common release waits, deferred image publication and shutdown."""
import argparse
import ctypes as C
import json
import os
from pathlib import Path
import shutil
import signal
import sys
import time

from geometry_transition_live import Client, Pointer
from image_colorsets_live import records
from live_x_harness import HarnessRuntime, start_display_server, wait_for_predicate
from mfl_stream_live import Connection


def run(args):
    args.artifacts_dir.mkdir(parents=True, exist_ok=True)
    evidence = {"active_viewport": getattr(args, "active_viewport", False),
                "active_menu": getattr(args, "active_menu", False)}
    with HarnessRuntime(prefix='fvwm-button-release-') as runtime:
        client = other = connection = pointer = core = None
        pressed = False
        active_menu = False
        try:
            display = args.display
            if display is None:
                _, display = start_display_server(runtime, 'Xvfb',
                    ['Xvfb', '-screen', '0', '800x600x24', '-noreset'],
                    'xserver.log', time.monotonic() + 10)
            marker = runtime.root / 'worker-started'
            gate = runtime.root / 'worker-gate'
            worker = runtime.root / 'gate-worker'
            worker.write_text(f'#!{sys.executable}\n'
                'import os,pathlib,sys,time\n'
                f'pathlib.Path({str(marker)!r}).write_text(str(os.getpid()))\n'
                f'while not pathlib.Path({str(gate)!r}).exists(): time.sleep(.01)\n'
                f'os.execv({str(args.worker)!r}, [{str(args.worker)!r}]+sys.argv[1:])\n')
            worker.chmod(0o700)
            config = runtime.root / 'config'
            config.write_text(f'ModulePath {args.mfl.parent}:{args.probe.parent}\n'
                'Style * ClickToFocus, ClickToFocusPassesClickOff, TileCascadePlacement, '
                'FPAllowFocusClickFunction, FPAllowRaiseClickFunction\n'
                'OpaqueMoveSize unlimited\nTitleStyle Height 24\n'
                'AddToMenu ReleaseMenu "Release" Title\n+ "Item" Nop\n'
                'AddToFunc StartFunction I Module FvwmMFL\n')
            log = runtime.root / 'debug.log'
            core = runtime.start('core', [str(args.fvwm), '-d', display,
                '-f', str(config), '-v', '-o', str(log)], 'core.log',
                runtime.environment({'FVWMMFL_SOCKET_PATH': str(runtime.root),
                    'FVWM_IMAGE_SVG_WORKER': str(worker)}))
            endpoint = runtime.root / f"fvwm_mfl_{display.replace(':', '_')}.sock"
            wait_for_predicate('release endpoint', endpoint.exists,
                time.monotonic() + 10, process=core)
            connection = Connection(endpoint)
            connection.record()
            connection.send(b'set echo\n')
            connection.echo('release-ready')
            client = Client(display)
            client.create()
            pointer = Pointer(client)
            wait_for_predicate('release client management', client.frame,
                time.monotonic() + 5, process=core)
            other = Client(display)
            other.create()
            wait_for_predicate('other client management', other.frame,
                time.monotonic() + 5, process=core)
            client.x.XKeysymToKeycode.argtypes = [C.c_void_p, C.c_ulong]
            client.x.XKeysymToKeycode.restype = C.c_uint
            pointer.x.XTestFakeKeyEvent.argtypes = [C.c_void_p, C.c_uint,
                C.c_int, C.c_ulong]
            client.x.XClearArea.argtypes = [C.c_void_p, C.c_ulong,
                C.c_int, C.c_int, C.c_uint, C.c_uint, C.c_int]

            def observed(tag):
                return 'CMD_Echo: ' + tag in log.read_text()

            def worker_started():
                return marker.exists() and marker.read_text().strip().isdigit()

            def await_echo(tag):
                wait_for_predicate(tag, lambda: observed(tag),
                    time.monotonic() + 5, process=core)

            sequence = 0

            def command(text):
                nonlocal sequence
                sequence += 1
                connection.send((text + '\n').encode())
                connection.echo(f'release-setup-{sequence}')

            def escape():
                code = client.x.XKeysymToKeycode(client.display, 0xff1b)
                assert code
                for down in (True, False):
                    assert pointer.x.XTestFakeKeyEvent(client.display, code, down, 0)
                client.x.XSync(client.display, 0)
                time.sleep(.05)

            def release():
                nonlocal pressed
                if active_menu:
                    # Preserve sticky Popup release behavior; Escape ends the
                    # active menu before the enclosing physical release wait.
                    escape()
                pointer.button(False)
                pressed = False
                wait_for_predicate('release grab restored',
                    lambda: not pointer.grabbed(), time.monotonic() + 5,
                    process=core)

            for label, context, action, cancel in (
                    ('root', 'R', 'Nop', False),
                    ('title', 'T', 'Nop', False),
                    ('focus', 'W', 'Nop', False),
                    ('menu', 'R', 'Popup ReleaseMenu', True),
                    ('paging', 'R', 'Scroll', True)):
                command('Mouse 1 R N Nop\nMouse 1 T N Nop\nMouse 1 W N Nop\n'
                    'DestroyFunc ReleaseCase\n'
                    f'AddToFunc ReleaseCase I Echo release-{label}-entered\n'
                    f'+ I {action}\n+ I Echo release-{label}-continuation\n'
                    f'Mouse 1 {context} N ReleaseCase\n'
                    f'WindowId {client.window} Move 100p 100p\n'
                    f'WindowId {other.window} Move 500p 300p\n'
                    f'WindowId {other.window} Focus')
                if context == 'R':
                    pointer.motion(20, 40)
                elif context == 'T':
                    x, y, width, _ = client.geometry(client.frame())
                    pointer.motion(x + width // 2, y + 12)
                else:
                    pointer.motion(250, 210)
                pointer.button(True)
                pressed = True
                await_echo(f'release-{label}-entered')
                wait_for_predicate(label + ' grab', pointer.grabbed,
                    time.monotonic() + 5, process=core)
                if cancel:
                    escape()
                    assert not observed(f'release-{label}-continuation')
                connection.send(f'Schedule 30 Echo release-{label}-timer\n'
                    f'Echo release-{label}-module\n'.encode())
                client.x.XClearArea(client.display, client.frame(), 0, 0, 4, 4, True)
                client.x.XSync(client.display, 0)
                await_echo(f'release-{label}-timer')
                await_echo(f'release-{label}-module')
                assert pointer.grabbed() and pointer.position()[2] & 256
                if cancel:
                    assert not observed(f'release-{label}-continuation')
                release()
                await_echo(f'release-{label}-continuation')
                evidence[label + '-safe-service-and-release'] = 'passed'

            # A worker already started at the outer boundary completes while a
            # root release wait holds the command stack. Transport reaps it;
            # immutable owners must not publish until the physical release.
            report = runtime.root / 'colorsets.txt'
            image = runtime.root / 'gate.svg'
            image.write_text('blue')
            command(f'Colorset 1 bg black\n'
                f'Module {args.probe.name} colorsets {report}\n'
                f'Colorset 1 Pixmap {image}')
            wait_for_predicate('release image worker started', worker_started,
                time.monotonic() + 5, process=core)
            worker_pid = int(marker.read_text())
            active_menu = getattr(args, 'active_menu', False)
            action = 'Popup ReleaseMenu' if active_menu else (
                'Scroll' if getattr(args, 'active_viewport', False) else 'Nop')
            command('Mouse 1 R N ' + action)
            pointer.motion(20, 40)
            pointer.button(True)
            pressed = True
            wait_for_predicate('image release grab', pointer.grabbed,
                time.monotonic() + 5, process=core)
            connection.send(b'Echo release-image-entered\n')
            await_echo('release-image-entered')
            gate.touch()

            def reaped():
                try:
                    os.kill(worker_pid, 0)
                    return False
                except ProcessLookupError:
                    return True

            wait_for_predicate('worker reaped during release wait', reaped,
                time.monotonic() + 3, process=core)
            assert not any(row[0] == 1 and row[8] != 0 for row in records(report))
            assert pointer.grabbed()
            release()
            wait_for_predicate('image publication after release',
                lambda: any(row[0] == 1 and row[8] != 0 for row in records(report)),
                time.monotonic() + 5, process=core)
            evidence['image-transport-with-publication-after-unwind'] = 'passed'

            published = [row for row in records(report) if row[0] == 1][-1]
            stalled = runtime.root / 'stall.svg'
            stalled.write_text('red')
            marker.unlink()
            command(f'Colorset 1 Pixmap {stalled}')
            wait_for_predicate('stalled release worker started', worker_started,
                time.monotonic() + 5, process=core)
            worker_pid = int(marker.read_text())
            pointer.motion(20, 40)
            pointer.button(True)
            pressed = True
            wait_for_predicate('deadline release grab', pointer.grabbed,
                time.monotonic() + 5, process=core)
            connection.send(b'Schedule 30 Echo release-deadline-timer\n'
                b'Echo release-deadline-module\n')
            await_echo('release-deadline-timer')
            await_echo('release-deadline-module')
            wait_for_predicate('stalled worker deadline and reap', reaped,
                time.monotonic() + args.timeout + 2, process=core)
            assert [row for row in records(report) if row[0] == 1][-1] == published
            assert pointer.grabbed()
            release()
            # Decoder fallback/final refusal needs the outer boundary. Only
            # transport deadlines and worker reap run inside a release wait.
            wait_for_predicate('final image refusal after release',
                lambda: 'SVG refused:' in log.read_text(),
                time.monotonic() + 5, process=core)
            connection.echo('release-after-deadline')
            assert [row for row in records(report) if row[0] == 1][-1] == published
            evidence['image-deadline-reap-and-deferred-fallback'] = 'passed'

            pointer.motion(20, 40)
            pointer.button(True)
            pressed = True
            wait_for_predicate('termination release grab', pointer.grabbed,
                time.monotonic() + 5, process=core)
            connection.send(b'Echo release-termination-entered\n')
            await_echo('release-termination-entered')
            core.process.send_signal(signal.SIGTERM)
            assert core.wait_until_exit(time.monotonic() + 2) == 0
            assert not pointer.grabbed() and pointer.position()[2] & 256
            release()
            evidence['termination-with-physical-button-held'] = 'passed'
            text = log.read_text()
            for failure in ('X error', 'AddressSanitizer', 'runtime error:',
                    'Unbound deferred image', 'readiness wait failed'):
                assert failure not in text, text
        except BaseException as error:
            evidence['error'] = str(error)
            raise
        finally:
            if pressed and pointer:
                pointer.button(False)
            if connection:
                connection.close()
            if other:
                other.close()
            if client:
                client.close()
            for path in runtime.root.iterdir():
                if path.suffix in ('.txt', '.log'):
                    shutil.copy2(path, args.artifacts_dir / path.name)
            (args.artifacts_dir / 'evidence.json').write_text(
                json.dumps(evidence, indent=2) + '\n')
    print('Common release waits retain ordering, image boundaries and shutdown')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(allow_abbrev=False)
    for name in ('fvwm', 'mfl', 'probe', 'worker', 'artifacts-dir'):
        parser.add_argument('--' + name, type=Path, required=True)
    parser.add_argument('--display')
    parser.add_argument('--active-viewport', action='store_true')
    parser.add_argument('--active-menu', action='store_true')
    parser.add_argument('--timeout', type=int, default=5)
    args = parser.parse_args()
    for key, value in vars(args).items():
        if isinstance(value, Path):
            setattr(args, key, value.resolve())
    run(args)
