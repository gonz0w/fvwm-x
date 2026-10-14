#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
"""Verify submenu delays under early wakes and menu action/lifecycle ordering."""
import argparse
import ctypes as C
import json
from pathlib import Path
import shutil
import signal
import time

from colorset_live import WindowAttributes
from geometry_transition_live import Client, Pointer
from live_x_harness import HarnessRuntime, start_display_server, wait_for_predicate
from mfl_stream_live import Connection


def run(args):
    args.artifacts_dir.mkdir(parents=True, exist_ok=True)
    evidence = {}
    with HarnessRuntime(prefix='fvwm-menu-service-') as runtime:
        client = pointer = connection = other = None
        pressed = False
        try:
            display = args.display
            if display is None:
                _, display = start_display_server(runtime, 'Xvfb',
                    ['Xvfb', '-screen', '0', '800x600x24', '-noreset'],
                    'xserver.log', time.monotonic() + 10)
            log, config = runtime.root / 'debug.log', runtime.root / 'config'
            config.write_text(f'ModulePath {args.mfl.parent}\nDefaultFont fixed\n'
                'Style * TileCascadePlacement\n'
                'MenuStyle * Font fixed, PopupDelayed, PopupDelay 600, '
                'PopdownDelayed, PopdownDelay 600, PopupActiveArea 75\n'
                'AddToMenu TimingChild "Select" Echo timing-selected\n'
                'AddToMenu TimingMain "Submenu" Popup TimingChild\n'
                '+ "Other" Nop\n'
                '+ "DynamicPopupAction" Echo timing-dynamic-popup\n'
                '+ "DynamicPopdownAction" Echo timing-dynamic-popdown\n'
                'AddToFunc TimingOpen I Echo timing-entered\n'
                '+ I Popup TimingMain\n+ I Echo timing-continuation\n'
                'Mouse 1 R N TimingOpen\n'
                'AddToFunc StartFunction I Module FvwmMFL\n')
            core = runtime.start('core', [str(args.fvwm), '-d', display,
                '-f', str(config), '-v', '-o', str(log)], 'core.log',
                runtime.environment({'FVWMMFL_SOCKET_PATH': str(runtime.root)}))
            endpoint = runtime.root / f"fvwm_mfl_{display.replace(':', '_')}.sock"
            wait_for_predicate('menu service endpoint', endpoint.exists,
                time.monotonic() + 10, process=core)
            connection = Connection(endpoint)
            connection.record()
            connection.send(b'set echo\n')
            connection.echo('timing-ready')
            client = Client(display)
            pointer = Pointer(client)
            x, d = client.x, client.display
            x.XGetWindowAttributes.argtypes = [C.c_void_p, C.c_ulong,
                C.POINTER(WindowAttributes)]
            x.XKeysymToKeycode.argtypes = [C.c_void_p, C.c_ulong]
            x.XKeysymToKeycode.restype = C.c_uint
            pointer.x.XTestFakeKeyEvent.argtypes = [C.c_void_p, C.c_uint,
                C.c_int, C.c_ulong]

            def observed(tag):
                return 'CMD_Echo: ' + tag in log.read_text()

            def await_echo(tag):
                wait_for_predicate(tag, lambda: observed(tag),
                    time.monotonic() + 5, process=core)

            def mapped_menus():
                root, parent = C.c_ulong(), C.c_ulong()
                children, count = C.POINTER(C.c_ulong)(), C.c_uint()
                assert x.XQueryTree(d, client.root, C.byref(root), C.byref(parent),
                    C.byref(children), C.byref(count))
                result = {}
                try:
                    for window in children[:count.value]:
                        attrs = WindowAttributes()
                        assert x.XGetWindowAttributes(d, window, C.byref(attrs))
                        if attrs.map_state == 2 and attrs.save_under and attrs.width > 20:
                            result[window] = client.geometry(window)
                finally:
                    if children:
                        x.XFree(children)
                return result

            def key(symbol):
                code = x.XKeysymToKeycode(d, symbol)
                assert code
                for down in (True, False):
                    assert pointer.x.XTestFakeKeyEvent(d, code, down, 0)
                x.XSync(d, 0)

            def begin():
                nonlocal pressed
                pointer.motion(200, 200)
                pointer.button(True)
                pressed = True
                wait_for_predicate('one menu mapped',
                    lambda: mapped_menus() if len(mapped_menus()) == 1 else None,
                    time.monotonic() + 5, process=core)
                await_echo('timing-entered')
                await_echo('timing-dynamic-popup')
                menus = mapped_menus()
                return next(iter(menus)), next(iter(menus.values()))

            def wake_flood(duration, predicate):
                start = time.monotonic()
                sequence = 0
                while time.monotonic() - start < duration:
                    sequence += 1
                    tag = f'timing-wake-{sequence}-{start}'
                    connection.send(f'Echo {tag}\n'.encode())
                    await_echo(tag)
                    assert predicate(), mapped_menus()
                    time.sleep(.002)
                return sequence

            parent, geometry = begin()
            px, py, width, height = geometry
            pointer.motion(px + 5, py + height // 4)
            began = time.monotonic()
            wakes = wake_flood(.2, lambda: len(mapped_menus()) == 1)
            menus = wait_for_predicate('delayed submenu opens',
                lambda: mapped_menus() if len(mapped_menus()) == 2 else None,
                time.monotonic() + 3, process=core)
            opened_after = time.monotonic() - began
            assert opened_after >= .55
            child = next(window for window in menus if window != parent)
            pointer.motion(px + 5, py + 3 * height // 4)
            began = time.monotonic()
            wakes += wake_flood(.2, lambda: child in mapped_menus())
            wait_for_predicate('delayed submenu closes',
                lambda: child not in mapped_menus(), time.monotonic() + 3,
                process=core)
            closed_after = time.monotonic() - began
            assert closed_after >= .55
            assert pointer.grabbed() and not observed('timing-continuation')
            evidence['submenu-monotonic-delays-under-early-wakes'] = {
                'popup_seconds': opened_after, 'popdown_seconds': closed_after,
                'independent_wakes': wakes, 'configured_ms': 600}

            # MapRequest stays queued through the active menu and final wait.
            other = Client(display)
            other.create()
            connection.send(b'Echo timing-map-service\n')
            await_echo('timing-map-service')
            assert other.parent(other.window) == other.root
            key(0xff1b)
            connection.send(b'Echo timing-final-wait\n')
            await_echo('timing-final-wait')
            assert other.parent(other.window) == other.root
            assert not observed('timing-continuation')
            pointer.button(False)
            pressed = False
            await_echo('timing-continuation')
            await_echo('timing-dynamic-popdown')
            wait_for_predicate('menu queued client managed', other.frame,
                time.monotonic() + 5, process=core)
            evidence['dynamic-actions-and-deferred-map-release'] = 'passed'

            # Keyboard navigation into a submenu and Return retain selection.
            connection.send(f'WindowId {other.window} Move 500p 300p\n'.encode())
            connection.echo('timing-selection-setup')
            parent, geometry = begin()
            px, py, width, height = geometry
            pointer.motion(px + 5, py + height // 4)
            key(0xff53)  # Right opens/enters the submenu.
            wait_for_predicate('keyboard submenu', lambda: len(mapped_menus()) == 2,
                time.monotonic() + 5, process=core)
            key(0xff0d)
            # A selected submenu can retain its parent until the nested
            # physical-release wait returns. Observe selection ordering rather
            # than requiring all menu windows to vanish before that release.
            connection.echo('timing-selection-held-service')
            assert not observed('timing-selected')
            assert pointer.position()[2] & 256
            pointer.button(False)
            pressed = False
            await_echo('timing-selected')
            wait_for_predicate('selected menus unmapped', lambda: not mapped_menus(),
                time.monotonic() + 5, process=core)
            wait_for_predicate('selection releases pointer',
                lambda: not pointer.grabbed(), time.monotonic() + 5,
                process=core)
            connection.echo('timing-healthy-after-selection')
            evidence['keyboard-submenu-return-selection'] = 'passed'
            begin()
            menus = mapped_menus()
            px, py, width, height = next(iter(menus.values()))
            pointer.motion(px + 5, py + height // 4)
            connection.send(b'Echo timing-submenu-wait-entered\n')
            await_echo('timing-submenu-wait-entered')
            assert len(mapped_menus()) == 1
            core.process.send_signal(signal.SIGTERM)
            assert core.wait_until_exit(time.monotonic() + 2) == 0
            assert not pointer.grabbed() and pointer.position()[2] & 256
            assert not mapped_menus()
            pointer.button(False)
            pressed = False
            evidence['termination-during-submenu-deadline-wait'] = 'passed'
            for failure in ('X error', 'AddressSanitizer', 'runtime error:',
                    'readiness wait failed'):
                assert failure not in log.read_text(), log.read_text()
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
