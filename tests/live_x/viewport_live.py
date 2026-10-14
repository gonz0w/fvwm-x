#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
"""Check viewport direction, keyboard barriers and deferred X ownership."""
import argparse
import ctypes as C
import json
from pathlib import Path
import shutil
import time

from geometry_transition_live import Client, Pointer
from live_x_harness import (HarnessRuntime, StatusCollector, query_xrandr,
    start_display_server, start_xorg_dummy, topology_epoch, wait_for_predicate)
from mfl_stream_live import Connection


def run(args):
    args.artifacts_dir.mkdir(parents=True, exist_ok=True)
    evidence = {}
    with HarnessRuntime(prefix='fvwm-viewport-') as runtime:
        client = other = pointer = connection = collector = None
        pressed = False
        try:
            display = args.display
            if display is None:
                if args.xorg_config:
                    _, display = start_xorg_dummy(runtime, args.xorg_config,
                        time.monotonic() + 10)
                else:
                    _, display = start_display_server(runtime, 'Xvfb',
                        ['Xvfb', '-screen', '0', '800x600x24', '-noreset'],
                        'xserver.log', time.monotonic() + 10)
            log = runtime.root / 'debug.log'
            config = runtime.root / 'config'
            config.write_text(f'ModulePath {args.mfl.parent}\n'
                'Style * TileCascadePlacement\nDesktopSize 3x3\n'
                'EwmhNumberOfDesktops 1\n'
                'EdgeScroll 0 0\nEdgeThickness 0\n'
                'AddToFunc StartFunction I Module FvwmMFL\n')
            core = runtime.start('core', [str(args.fvwm), '-d', display,
                '-f', str(config), '-v', '-o', str(log)], 'core.log',
                runtime.environment({'FVWMMFL_SOCKET_PATH': str(runtime.root),
                    'FVWM_USERDIR': str(runtime.root)}))
            endpoint = runtime.root / f"fvwm_mfl_{display.replace(':', '_')}.sock"
            wait_for_predicate('viewport endpoint', endpoint.exists,
                time.monotonic() + 10, process=core)
            connection = Connection(endpoint)
            connection.record()
            connection.send(b'set echo\n')
            connection.echo('viewport-ready')
            client = Client(display)
            pointer = Pointer(client)
            x, d = client.x, client.display
            x.XInternAtom.argtypes = [C.c_void_p, C.c_char_p, C.c_int]
            x.XInternAtom.restype = C.c_ulong
            x.XGetWindowProperty.argtypes = [C.c_void_p, C.c_ulong, C.c_ulong,
                C.c_long, C.c_long, C.c_int, C.c_ulong, C.POINTER(C.c_ulong),
                C.POINTER(C.c_int), C.POINTER(C.c_ulong), C.POINTER(C.c_ulong),
                C.POINTER(C.c_void_p)]
            x.XKeysymToKeycode.argtypes = [C.c_void_p, C.c_ulong]
            x.XKeysymToKeycode.restype = C.c_uint
            pointer.x.XTestFakeKeyEvent.argtypes = [C.c_void_p, C.c_uint,
                C.c_int, C.c_ulong]

            def viewport():
                atom = x.XInternAtom(d, b'_NET_DESKTOP_VIEWPORT', 0)
                actual, count, remaining = C.c_ulong(), C.c_ulong(), C.c_ulong()
                fmt, data = C.c_int(), C.c_void_p()
                assert x.XGetWindowProperty(d, client.root, atom, 0, 128, 0,
                    6, C.byref(actual), C.byref(fmt), C.byref(count),
                    C.byref(remaining), C.byref(data)) == 0
                try:
                    assert actual.value == 6 and fmt.value == 32 and count.value >= 2, (actual.value, fmt.value, count.value)
                    values = C.cast(data, C.POINTER(C.c_ulong))
                    return values[0], values[1]
                finally:
                    if data:
                        x.XFree(data)

            def observed(tag):
                return 'CMD_Echo: ' + tag in log.read_text()

            def await_echo(tag):
                wait_for_predicate(tag, lambda: observed(tag),
                    time.monotonic() + 5, process=core)

            def key(symbol):
                code = x.XKeysymToKeycode(d, symbol)
                assert code
                for down in (True, False):
                    assert pointer.x.XTestFakeKeyEvent(d, code, down, 0)
                x.XSync(d, 0)

            def begin(tag, reverse=False):
                nonlocal pressed
                connection.send(('GotoPage 1 1\nDestroyFunc ViewportCase\n'
                    f'AddToFunc ViewportCase I Echo {tag}-entered\n'
                    f'+ I Scroll {"reverse" if reverse else ""}\n'
                    f'+ I Echo {tag}-continuation\n'
                    'Mouse 1 R N ViewportCase\n').encode())
                connection.echo(tag + '-configured')
                origin = viewport()
                pointer.motion(400, 300)
                pointer.button(True)
                pressed = True
                await_echo(tag + '-entered')
                wait_for_predicate('active Scroll grab', pointer.grabbed,
                    time.monotonic() + 5, process=core)
                # The root binding has a passive grab before Scroll records
                # its pointer origin. A serviced command confirms active entry.
                connection.echo(tag + '-active-service')
                return origin

            def release(tag):
                nonlocal pressed
                pointer.button(False)
                pressed = False
                await_echo(tag + '-continuation')
                wait_for_predicate('viewport grab released',
                    lambda: not pointer.grabbed(), time.monotonic() + 5,
                    process=core)

            for reverse in (False, True):
                tag = 'viewport-reverse' if reverse else 'viewport-forward'
                origin = begin(tag, reverse)
                pointer.motion(430, 320)
                sign = -1 if reverse else 1
                expected = (origin[0] + sign * 30, origin[1] + sign * 20)
                wait_for_predicate('viewport pointer direction',
                    lambda: viewport() == expected, time.monotonic() + 5,
                    process=core)
                # Keyboard motion uses the same policy as mouse motion.
                key(0xff53)  # Right
                wait_for_predicate('viewport keyboard direction',
                    lambda: (viewport()[0] - expected[0]) * sign > 0,
                    time.monotonic() + 5, process=core)
                key(0xff0d if reverse else 0xff1b)  # Return / Escape
                connection.send(f'Echo {tag}-cancelled\n'.encode())
                await_echo(tag + '-cancelled')
                assert pointer.grabbed() and pointer.position()[2] & 256
                assert not observed(tag + '-continuation')
                release(tag)
                evidence[tag + '-pointer-keyboard-and-release'] = 'passed'

            if args.xorg_config:
                connection.send(b'Status On\n')
                collector = StatusCollector.open(runtime.root / 'fvwm3.pipe',
                    time.monotonic() + 5, process=core)
                connection.echo('viewport-status-ready')
                before = collector.wait_for('initial topology', lambda r: True,
                    time.monotonic() + 5, process=core)
                epoch = topology_epoch(before)
                begin('viewport-topology')
                code, text = query_xrandr(runtime, display,
                    ['--output', 'DUMMY0', '--mode', '1280x1024',
                     '--fb', '1280x1024'], time.monotonic() + 5,
                    'viewport-layout-change.log')
                assert code == 0, text
                connection.send(b'Schedule 20 Echo viewport-topology-timer\n'
                    b'Echo viewport-topology-service\n')
                await_echo('viewport-topology-timer')
                await_echo('viewport-topology-service')
                pointer.motion(410, 305)
                time.sleep(.2)
                collector.read_available()
                assert all(topology_epoch(r) == epoch for r in collector.records)
                key(0xff1b)
                connection.send(b'Echo viewport-topology-final-wait\n')
                await_echo('viewport-topology-final-wait')
                collector.read_available()
                assert all(topology_epoch(r) == epoch for r in collector.records)
                release('viewport-topology')
                after = collector.wait_for('topology after viewport unwind',
                    lambda r: topology_epoch(r) > epoch,
                    time.monotonic() + 5, process=core)
                evidence['topology-retained-through-active-and-final-waits'] = {
                    'before_epoch': epoch, 'after_epoch': topology_epoch(after)}

            begin('viewport-map')
            other = Client(display)
            other.create()
            connection.send(b'Echo viewport-map-service\n')
            await_echo('viewport-map-service')
            time.sleep(.1)
            assert other.parent(other.window) == other.root
            key(0xff1b)
            connection.send(b'Echo viewport-map-final-wait\n')
            await_echo('viewport-map-final-wait')
            assert other.parent(other.window) == other.root
            release('viewport-map')
            wait_for_predicate('queued client managed after unwind', other.frame,
                time.monotonic() + 5, process=core)
            evidence['map-request-retained-through-active-and-final-waits'] = 'passed'
            for failure in ('X error', 'AddressSanitizer', 'runtime error:',
                    'readiness wait failed'):
                assert failure not in log.read_text(), log.read_text()
        except BaseException as error:
            evidence['error'] = str(error)
            raise
        finally:
            if pressed and pointer:
                pointer.button(False)
            if collector:
                collector.close()
            if connection:
                connection.close()
            if other:
                other.close()
            if client:
                client.close()
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
    parser.add_argument('--xorg-config', type=Path)
    run(parser.parse_args())
