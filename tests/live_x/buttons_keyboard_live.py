#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or any later version.
"""Exercise native Buttons focus, actual input, state and legacy activation."""
import argparse
import ctypes as C
import json
from pathlib import Path
import re
import shutil
import time

from geometry_transition_live import Client, Pointer, XEvent
from live_x_harness import HarnessRuntime, start_display_server, wait_for_predicate
from mfl_stream_live import Connection


def run(args):
    out = args.artifacts_dir
    out.mkdir(parents=True, exist_ok=True)
    evidence = {"status": "running"}
    with HarnessRuntime(prefix='fvwm-buttons-keyboard-') as runtime:
        client = connection = None
        try:
            display = args.display
            if display is None:
                _, display = start_display_server(runtime, 'Xvfb',
                    ['Xvfb', '-screen', '0', '800x600x24', '-noreset'],
                    'xserver.log', time.monotonic() + 10)
            config = runtime.root / 'config'
            config.write_text(f'ModulePath {args.mfl.parent}:{args.module.parent}\n'
                'Style * NoTitle, NoHandles, BorderWidth 0, ClickToFocus, TileCascadePlacement\n'
                'Colorset 0 fg white, bg #203040\n'
                'Colorset 1 fg white, bg #008000\n'
                'Colorset 2 fg white, bg #0000ff\n'
                'Colorset 3 fg white, bg #ff0000\n'
                'Colorset 4 fg white, bg #606060\n'
                'AddToFunc StartFunction I Module FvwmMFL\n')
            log = runtime.root / 'core-debug.log'
            core = runtime.start('core', [str(args.fvwm.resolve()), '-d', display,
                '-f', str(config), '-v', '-o', str(log)], 'core.log',
                runtime.environment({'FVWMMFL_SOCKET_PATH': str(runtime.root)}))
            endpoint = runtime.root / f"fvwm_mfl_{display.replace(':', '_')}.sock"
            wait_for_predicate('Buttons endpoint', endpoint.exists,
                time.monotonic() + 10, process=core)
            connection = Connection(endpoint)
            connection.record()
            connection.send(b'set echo\n')
            connection.echo('buttons-ready')
            client = Client(display)
            client.create()
            x, d = client.x, client.display
            pointer = Pointer(client)
            signatures = {
                'XDefaultScreen': ([C.c_void_p], C.c_int),
                'XDefaultColormap': ([C.c_void_p, C.c_int], C.c_ulong),
                'XQueryColor': ([C.c_void_p, C.c_ulong, C.c_void_p], C.c_int),
                'XGrabServer': ([C.c_void_p], C.c_int),
                'XUngrabServer': ([C.c_void_p], C.c_int),
                'XInternAtom': ([C.c_void_p, C.c_char_p, C.c_int], C.c_ulong),
                'XGetWindowProperty': ([C.c_void_p, C.c_ulong, C.c_ulong,
                    C.c_long, C.c_long, C.c_int, C.c_ulong,
                    C.POINTER(C.c_ulong), C.POINTER(C.c_int),
                    C.POINTER(C.c_ulong), C.POINTER(C.c_ulong),
                    C.POINTER(C.c_void_p)], C.c_int),
                'XFetchName': ([C.c_void_p, C.c_ulong,
                    C.POINTER(C.c_void_p)], C.c_int),
                'XSetInputFocus': ([C.c_void_p, C.c_ulong, C.c_int,
                    C.c_ulong], C.c_int),
                'XGetInputFocus': ([C.c_void_p, C.POINTER(C.c_ulong),
                    C.POINTER(C.c_int)], C.c_int),
                'XKeysymToKeycode': ([C.c_void_p, C.c_ulong], C.c_uint),
                'XGetImage': ([C.c_void_p, C.c_ulong, C.c_int, C.c_int,
                    C.c_uint, C.c_uint, C.c_ulong, C.c_int], C.c_void_p),
                'XGetPixel': ([C.c_void_p, C.c_int, C.c_int], C.c_ulong),
                'XDestroyImage': ([C.c_void_p], C.c_int),
                'XNextEvent': ([C.c_void_p, C.POINTER(XEvent)], C.c_int),
                'XPending': ([C.c_void_p], C.c_int),
                'XTranslateCoordinates': ([C.c_void_p, C.c_ulong, C.c_ulong,
                    C.c_int, C.c_int, C.POINTER(C.c_int), C.POINTER(C.c_int),
                    C.POINTER(C.c_ulong)], C.c_int),
            }
            for name, (arguments, result) in signatures.items():
                getattr(x, name).argtypes = arguments
                getattr(x, name).restype = result
            pointer.x.XTestFakeKeyEvent.argtypes = [C.c_void_p, C.c_uint,
                C.c_int, C.c_ulong]
            pointer.x.XTestFakeKeyEvent.restype = C.c_int
            class XError(C.Structure):
                _fields_ = [('type', C.c_int), ('display', C.c_void_p),
                    ('resource', C.c_ulong), ('serial', C.c_ulong),
                    ('code', C.c_ubyte), ('request', C.c_ubyte),
                    ('minor', C.c_ubyte)]

            errors = []
            handler_type = C.CFUNCTYPE(C.c_int, C.c_void_p, C.POINTER(XError))

            @handler_type
            def on_error(_, error):
                errors.append((error.contents.code, error.contents.request))
                return 0

            x.XSetErrorHandler.argtypes = [handler_type]
            x.XSetErrorHandler.restype = C.c_void_p
            previous_handler = x.XSetErrorHandler(on_error)
            held_keys = set()
            sequence = 0
            module_name = 'KeyboardButtons'

            def command(text):
                connection.send((text + '\n').encode())
                connection.echo('buttons-command')

            def window_named(name):
                x.XGrabServer(d)
                try:
                    return find_named_client(name)
                finally:
                    x.XUngrabServer(d)
                    x.XSync(d, 0)
                    # A client may have died before the WM publishes removal.
                    assert all(error == (3, 20) for error in errors), errors
                    errors.clear()

            def find_named_client(name):
                atom = x.XInternAtom(d, b'_NET_CLIENT_LIST', 0)
                actual, fmt = C.c_ulong(), C.c_int()
                count, after = C.c_ulong(), C.c_ulong()
                data = C.c_void_p()
                assert x.XGetWindowProperty(d, client.root, atom, 0, 256,
                    0, 33, C.byref(actual), C.byref(fmt), C.byref(count),
                    C.byref(after), C.byref(data)) == 0
                try:
                    if not actual.value:
                        return None
                    assert actual.value == 33 and fmt.value == 32 and not after.value
                    windows = C.cast(data, C.POINTER(C.c_ulong))
                    for index in range(count.value):
                        name_ptr = C.c_void_p()
                        if x.XFetchName(d, windows[index], C.byref(name_ptr)) and name_ptr:
                            try:
                                if C.string_at(name_ptr).decode() == name:
                                    return windows[index]
                            finally:
                                x.XFree(name_ptr)
                finally:
                    if data:
                        x.XFree(data)
                return None

            def barrier():
                nonlocal sequence
                sequence += 1
                tag = f'BUTTON-BARRIER-{sequence}'
                connection.send(f'SendToModule {module_name} '
                    f'ExpandButtonVars alpha Echo {tag}\n'.encode())
                wait_for_predicate(tag, lambda: f'CMD_Echo: {tag}' in log.read_text(),
                    time.monotonic() + 5, process=core)
                x.XSync(d, 0)

            def actions():
                barrier()
                return re.findall(r'CMD_Echo: ACT-(\w+)', log.read_text())

            def focus(window):
                x.XSetInputFocus(d, window, 1, 0)
                x.XSync(d, 0)
                barrier()
                actual, revert = C.c_ulong(), C.c_int()
                x.XGetInputFocus(d, C.byref(actual), C.byref(revert))
                assert actual.value == window

            def key(symbol, down):
                code = x.XKeysymToKeycode(d, symbol)
                assert code and pointer.x.XTestFakeKeyEvent(d, code, down, 0)
                if down:
                    held_keys.add(code)
                else:
                    held_keys.discard(code)
                x.XSync(d, 0)

            def tap(symbol, shift=False):
                if shift:
                    key(0xffe1, True)
                key(symbol, True)
                key(symbol, False)
                if shift:
                    key(0xffe1, False)
                barrier()

            def pixels(window, left, top, width, height):
                image = x.XGetImage(d, window, left, top, width, height,
                    C.c_ulong(-1).value, 2)
                assert image
                try:
                    return tuple(x.XGetPixel(image, a, b)
                        for b in range(height) for a in range(width))
                finally:
                    x.XDestroyImage(image)

            class XColor(C.Structure):
                _fields_ = [('pixel', C.c_ulong), ('red', C.c_ushort),
                    ('green', C.c_ushort), ('blue', C.c_ushort),
                    ('flags', C.c_byte), ('pad', C.c_byte)]

            palette = {}

            def rgb(pixel):
                if pixel not in palette:
                    value = XColor(pixel=pixel)
                    assert x.XQueryColor(d, x.XDefaultColormap(d,
                        x.XDefaultScreen(d)), C.byref(value))
                    palette[pixel] = ((value.red >> 8) << 16 |
                        (value.green >> 8) << 8 | (value.blue >> 8))
                return palette[pixel]

            def color(left, top):
                return rgb(pixels(shown, left + 10, top + 10, 1, 1)[0])

            def near(actual, expected):
                # Actual BSD default displays can use RGB565. Compare RGB
                # values within quantisation, rather than assuming pixel ABI.
                return all(abs(((actual >> shift) & 255) -
                    ((expected >> shift) & 255)) <= 8 for shift in (0, 8, 16))

            def await_color(left, top, expected):
                wait_for_predicate('button palette',
                    lambda: near(color(left, top), expected),
                    time.monotonic() + 5, process=core)

            def activate(expected):
                old = actions()
                tap(0xff0d)
                assert actions() == old + [expected], actions()

            def move_pointer(window, px, py):
                rx, ry, child = C.c_int(), C.c_int(), C.c_ulong()
                assert x.XTranslateCoordinates(d, window, client.root, px, py,
                    C.byref(rx), C.byref(ry), C.byref(child))
                pointer.motion(rx.value, ry.value)
                barrier()

            lines = ['Rows 2', 'Columns 3', 'Geometry 600x120+0+0',
                'Frame 0', 'Padding 0 0', 'Font fixed', 'Colorset 0',
                'FocusColorset 1', 'DisabledColorset 4',
                'ActiveColorset 2', 'PressColorset 3', 'KeyboardNavigation 1',
                '(1x1+0+0, Id alpha, Title Alpha, Action "Echo ACT-alpha")',
                '(1x1+1+0, Title Label, Action (Mouse 2) "Echo ACT-secondary")',
                '(1x1+2+0, Id disabled, Disabled, Title Unavailable, '
                'Action "Echo ACT-disabled")',
                '(2x1+0+1, Container(Rows 1, Columns 2, Frame 0, Padding 0))',
                '(Id beta, Title Beta, Action (Mouse 1) "Echo ACT-beta")',
                '(Id gamma, Title Gamma, Action "Echo ACT-gamma")', '(End)',
                '(1x1+2+1, Id epsilon, Title Epsilon, ActionOnPress, '
                'Action "Echo ACT-epsilon")']
            command('\n'.join(f'*{module_name}: {line}' for line in lines))
            command(f'Module {args.module.name} {module_name}')
            shown = wait_for_predicate('Buttons window',
                lambda: window_named(module_name), time.monotonic() + 10, process=core)
            focus(shown)
            await_color(0, 0, 0x008000)
            assert rgb(pixels(shown, 1, 1, 1, 1)[0]) == 0xffffff
            activate('alpha')
            for expected in ('beta', 'gamma', 'epsilon', 'alpha'):
                tap(0xff09)
                activate(expected)
            tap(0xff09, shift=True)
            activate('epsilon')
            tap(0xff09)
            tap(0xff54)  # Down across the nested container.
            activate('beta')
            tap(0xff53)
            activate('gamma')
            tap(0xff53)
            activate('epsilon')
            tap(0xff53)  # Arrow at the edge stays put.
            activate('epsilon')
            tap(0xff51)
            tap(0xff52)
            activate('alpha')
            evidence['tab-wrap-reverse-arrows-nested-grid-and-label-skip'] = 'passed'

            # Hover is independent of keyboard focus and activation identity.
            move_pointer(shown, 250, 90)
            await_color(200, 60, 0x0000ff)
            await_color(0, 0, 0x008000)
            activate('alpha')
            before = actions()
            key(0x20, True)
            await_color(0, 0, 0xff0000)
            time.sleep(.85)  # Include actual server autorepeat.
            key(0x20, False)
            assert actions() == before + ['alpha']
            # ActionOnPress also fires once while held, before release.
            tap(0xff09, shift=True)
            before = actions()
            key(0x20, True)
            barrier()
            assert actions() == before + ['epsilon']
            time.sleep(.85)
            key(0x20, False)
            assert actions() == before + ['epsilon']
            tap(0xff09)
            evidence['focus-hover-pressed-palettes-and-single-autorepeat-action'] = 'passed'

            # Disabling a held/focused control cancels activation and chooses
            # the first remaining eligible control. PressButton shares refusal.
            before = actions()
            key(0x20, True)
            command(f'SendToModule {module_name} ChangeButton alpha Enabled 0')
            barrier()
            key(0x20, False)
            command(f'SendToModule {module_name} PressButton alpha')
            command(f'SendToModule {module_name} PressButton disabled')
            move_pointer(shown, 450, 30)
            pointer.button(True)
            pointer.button(False)
            assert actions() == before
            await_color(0, 0, 0x606060)
            activate('beta')
            command(f'SendToModule {module_name} ChangeButton alpha Enabled 1, '
                'Selected 1, ActiveColorset 2, PressColorset 3, '
                'FocusColorset 1, DisabledColorset 4')
            barrier()
            await_color(0, 0, 0xff0000)
            command(f'SendToModule {module_name} ChangeButton alpha Selected 0')
            barrier()
            await_color(0, 0, 0x203040)
            before = actions()
            command(f'SendToModule {module_name} PressButton alpha')
            assert actions() == before + ['alpha']
            for button in ('alpha', 'beta', 'gamma', 'epsilon'):
                command(f'SendToModule {module_name} ChangeButton {button} Enabled 0')
            before = actions()
            tap(0xff09)
            tap(0x20)
            assert actions() == before
            command(f'SendToModule {module_name} ChangeButton gamma Enabled 1')
            barrier()
            activate('gamma')
            command(f'SendToModule {module_name} ChangeButton gamma '
                'FocusColorset 3')
            barrier()
            await_color(200, 60, 0xff0000)
            command(f'SendToModule {module_name} ChangeButton gamma '
                'FocusColorset 1')
            barrier()
            await_color(200, 60, 0x008000)
            command(f'SendToModule {module_name} ChangeButton alpha Enabled 2')
            before = actions()
            command(f'SendToModule {module_name} PressButton alpha')
            assert actions() == before
            evidence['disabled-all-entry-points-cancel-reenable-and-selected-state'] = 'passed'

            def group(text):
                command(f'SendToModule {module_name} ChangeButtons {text}')
                barrier()

            # Restore a known state, preserving the currently focused gamma.
            group('(alpha Enabled 1, Selected 0) (beta Enabled 1) '
                '(gamma Enabled 1) (epsilon Enabled 1)')
            activate('gamma')
            original = pixels(shown, 0, 0, 600, 120)
            bad = [
                '(missing Title Bad) (alpha Selected 1)',
                '(alpha Selected 1) (missing Title Bad) (beta Enabled 0)',
                '(alpha Selected 1) (beta Enabled 0) (missing Title Bad)',
                '(alpha Selected 1) (beta Enabled 2)',
                '(alpha Selected 1) (beta Unknown 1)',
                '(alpha Selected 1) (beta Colorset 2147483647)',
                '(alpha Selected 1) (beta Colorset 1junk)',
                '(alpha Selected 1) (0 Enabled 0)',
                '(alpha Selected 1) (+0+0 Enabled 0)',
                '(alpha Selected 1, Selected 0)',
                '(alpha Selected 1) (beta Title "unfinished)',
                '(alpha Selected 1) (beta Title Bad) junk',
                '(alpha Selected 1) (beta Icon /no-such-buttons-icon.xpm)',
                '(alpha Selected 1) (beta Enabled 0,)',
                '(alpha Selected 1) (99999999999999999999 Enabled 0)',
                ''.join(f'({n} Enabled 1)' for n in range(33)),
            ]
            for request in bad:
                group(request)
                assert pixels(shown, 0, 0, 600, 120) == original, request
                activate('gamma')
            evidence['atomic-invalid-first-middle-last-and-duplicates-rollback'] = 'passed'

            # Preparation acquires actual picture-cache references. A later
            # missing icon must unwind them without replacing any old resource.
            icon = runtime.root / 'transaction.xpm'
            icon.write_text('/* XPM */\nstatic char *icon[] = {\n'
                '"8 8 1 1", "x c #ffff00",\n' +
                ',\n'.join('"xxxxxxxx"' for _ in range(8)) + '};\n')
            for _ in range(3):
                group(f'(alpha Icon {icon}, ActiveIcon {icon}, '
                    f'PressIcon {icon}) (beta Icon /no-such-final-icon.xpm)')
                assert pixels(shown, 0, 0, 600, 120) == original
            for n in range(3):
                group(f'(alpha Title "Ready, ({n})", ActiveTitle Hover{n}, '
                    f'PressTitle Press{n}, Icon {icon}, ActiveIcon {icon}, '
                    f'PressIcon {icon}, Colorset 0, ActiveColorset 2, '
                    'PressColorset 3, FocusColorset 1, DisabledColorset 4, '
                    'Selected 1) (beta Selected 1)')
                await_color(0, 0, 0xff0000)
                await_color(0, 60, 0xff0000)
                activate('gamma')
            evidence['atomic-icons-titles-all-palettes-owned-prepare-and-commit'] = 'passed'

            # All final states are visible to focus repair. A held gamma key
            # cannot activate when the same group disables gamma and beta;
            # alpha is selected from the final eligible set, without restart.
            before = actions()
            key(0x20, True)
            group('(alpha Selected 0) (beta Enabled 0) (gamma Enabled 0)')
            key(0x20, False)
            assert actions() == before
            await_color(0, 0, 0x008000)
            await_color(0, 60, 0x606060)
            await_color(200, 60, 0x606060)
            activate('alpha')
            group('(alpha Enabled 0) (gamma Enabled 1, FocusColorset 1)')
            activate('gamma')
            # A refused group during a held key leaves that key/focus valid.
            before = actions()
            key(0x20, True)
            group('(gamma Enabled 0) (missing Enabled 1)')
            key(0x20, False)
            assert actions() == before + ['gamma']
            group('(gamma Selected 0, Title Recovered) (epsilon Selected 0)')
            assert window_named(module_name) == shown
            activate('gamma')
            evidence['atomic-held-key-cancel-final-focus-recovery-stable-window-action'] = 'passed'

            # Legacy ChangeButton still commits earlier valid pairs when a
            # later pair is refused, even beside the additive group command.
            command(f'SendToModule {module_name} ChangeButton gamma '
                'Selected 1, Enabled 2')
            barrier()
            await_color(200, 60, 0xff0000)
            group('(gamma Selected 0)')
            await_color(200, 60, 0x008000)
            evidence['legacy-incremental-changebutton-preserved'] = 'passed'

            # A live title update must preserve the other owned strings and
            # focused action. Exercise all three repeatedly under sanitizers.
            for n in range(3):
                command(f'SendToModule {module_name} ChangeButton gamma '
                    f'Title Normal{n}, ActiveTitle Hover{n}, PressTitle Press{n}')
            barrier()
            activate('gamma')
            before = actions()
            key(0x20, True)
            focus(client.window)
            key(0x20, False)
            assert actions() == before
            assert rgb(pixels(shown, 201, 61, 1, 1)[0]) != 0xffffff
            evidence['live-titles-stable-action-and-focus-loss-cancellation'] = 'passed'

            # This client selects real key events. Every rapid press must arrive
            # here, with no Buttons action or focus request during typing.
            x.XSelectInput(d, client.window, 3)
            x.XSync(d, 0)
            while x.XPending(d):
                event = XEvent()
                x.XNextEvent(d, C.byref(event))
            for _ in range(200):
                key(0x61, True)
                key(0x61, False)
            counts = {2: 0, 3: 0}
            while x.XPending(d):
                event = XEvent()
                x.XNextEvent(d, C.byref(event))
                kind = C.cast(C.byref(event), C.POINTER(C.c_int))[0]
                if kind in counts:
                    counts[kind] += 1
            assert counts == {2: 200, 3: 200}, counts
            assert actions() == before
            actual, revert = C.c_ulong(), C.c_int()
            x.XGetInputFocus(d, C.byref(actual), C.byref(revert))
            assert actual.value == client.window
            evidence['unrelated-client-200-rapid-keys-no-grab-or-focus-theft'] = 'passed'

            command(f'KillModule {args.module.name} {module_name}')
            wait_for_predicate('navigation module closed',
                lambda: window_named(module_name) is None,
                time.monotonic() + 5, process=core)
            module_name = 'LegacyButtons'
            command('\n'.join(f'*{module_name}: {line}' for line in
                ['Rows 1', 'Columns 2', 'Geometry 400x60+0+0', 'Font fixed',
                 '(Id alpha, Title First, Action "Echo ACT-first")',
                 '(Id second, Title Second, Action "Echo ACT-second")']))
            command(f'Module {args.module.name} {module_name}')
            shown = wait_for_predicate('legacy Buttons window',
                lambda: window_named(module_name), time.monotonic() + 10, process=core)
            focus(shown)
            move_pointer(shown, 300, 30)
            activate('second')
            before = actions()
            tap(0xff09)
            tap(0x20)
            assert actions() == before
            evidence['legacy-return-under-pointer-and-opt-in-only-traversal'] = 'passed'
            command(f'KillModule {args.module.name} {module_name}')
            connection.send(b'Quit\n')
            assert core.wait_until_exit(time.monotonic() + 5) == 0
            text = ''.join(p.read_text() for p in runtime.root.glob('*.log'))
            assert not any(marker in text for marker in
                ['ERROR: AddressSanitizer', 'ERROR: LeakSanitizer',
                 'runtime error:', 'Assertion']), text
            assert not errors, errors
            evidence['status'] = 'passed'
        finally:
            if connection:
                connection.close()
            if client and 'held_keys' in locals():
                for code in held_keys:
                    pointer.x.XTestFakeKeyEvent(d, code, False, 0)
                pointer.button(False)
                x.XSync(d, 0)
            # End the WM before Xlib client teardown, including failures
            # while a fixture accidentally leaves interactive placement active.
            if 'core' in locals():
                core.close(time.monotonic() + 5)
            if client:
                client.close()
            for path in runtime.root.glob('*.log'):
                shutil.copy2(path, out / path.name)
            if client and 'previous_handler' in locals():
                x.XSetErrorHandler.argtypes = [C.c_void_p]
                x.XSetErrorHandler(previous_handler)
            if config.exists():
                shutil.copy2(config, out / 'config')
            (out / 'evidence.json').write_text(json.dumps(evidence, indent=2) + '\n')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(allow_abbrev=False)
    parser.add_argument('--module', type=Path, required=True)
    parser.add_argument('--fvwm', type=Path, required=True)
    parser.add_argument('--mfl', type=Path, required=True)
    parser.add_argument('--artifacts-dir', type=Path, required=True)
    parser.add_argument('--display')
    run(parser.parse_args())
