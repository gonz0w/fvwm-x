#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or any later version.
"""Ordinary popup font lifetime across DefaultFont replacement, without preview.

Requires built core/MFL, Xvfb, X11/Xtst and base X fonts fixed/6x13/9x15.
The lead's retained inspect_asan01.json reported a stale FlocaleFont in
bind_popup_font after CMD_DefaultFont unloaded the startup default. This fixture
has no preview requests or sanitizer suppression. A recipe is not a fresh repro.
"""
import argparse
from collections import Counter
import ctypes as C
import hashlib
import json
from pathlib import Path
import shutil
import struct
import subprocess
import time
import zlib

from colorset_live import WindowAttributes
from geometry_transition_live import Client, Pointer
from image_colorsets_live import XColor
from live_x_harness import (HarnessRuntime, parse_xrandr_monitors,
    start_display_server, wait_for_predicate)
from mfl_request_live import frame, reply
from mfl_stream_live import Connection
from session_x_errors import SessionXErrors


def png_rgb(width, height, rgb):
    """Encode observed server pixels, not a generated menu image."""
    def chunk(kind, value):
        return (struct.pack('!I', len(value)) + kind + value +
            struct.pack('!I', zlib.crc32(kind + value) & 0xffffffff))
    stride = width * 3
    rows = b''.join(b'\0' + rgb[offset:offset + stride]
        for offset in range(0, len(rgb), stride))
    return (b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR',
        struct.pack('!IIBBBBB', width, height, 8, 2, 0, 0, 0)) +
        chunk(b'IDAT', zlib.compress(rows)) + chunk(b'IEND', b''))


def run(args):
    args.artifacts_dir.mkdir(parents=True, exist_ok=False)
    evidence = {'status': 'running', 'execution_profile': args.execution_profile,
        'scale_requested': args.scale, 'observations': [],
        'core_sha256': hashlib.sha256(args.fvwm.read_bytes()).hexdigest(),
        'limits': ['Build-tree ordinary-menu control, not installed acceptance.',
            'No physical display or native-CPU qualification is inferred.']}
    with HarnessRuntime(prefix='fvwm-menu-font-lifetime-') as runtime:
        core = connection = client = errors = None
        held = False
        try:
            display = args.display
            if display is None:
                _, display = start_display_server(runtime, 'Xvfb',
                    ['Xvfb', '-screen', '0', '800x600x24', '-noreset'],
                    'xserver.log', time.monotonic() + 10)
            listing = subprocess.check_output(['xrandr', '--display', display,
                '--listmonitors'], text=True, timeout=5)
            names = list(parse_xrandr_monitors(listing)) or ['FVWM-X-Root']
            assert all(name and not any(c in name for c in '\"\n\r') for name in names)
            scale_config = ''.join(f'MonitorScale \"{name}\" {args.scale}\n'
                for name in names)
            defaults = Path(__file__).resolve().parents[2] / 'fvwm/ConfigFvwmDefaults'
            assert defaults.is_file(), defaults
            config = runtime.root / 'config'
            menu_definition = ('AddToMenu FontLifetimeMenu "Ordinary font lifetime" Title\n'
                '+ "Text must remain readable" Nop\n+ "Close with Escape" Nop\n')
            before_default_definition = menu_definition.replace(
                'FontLifetimeMenu', 'FontBeforeDefaultMenu')
            named_definition = menu_definition.replace(
                'FontLifetimeMenu', 'NamedFontLifetimeMenu')
            config.write_text(f'ModulePath {args.mfl.parent}\nRead {defaults}\n'
                # NewMenuRoot captures popup-font slots at menu creation;
                # this root must precede the first default replacement, not
                # merely precede the popup which resolves its scaled font.
                + before_default_definition + 'DefaultFont fixed\n' +
                f'MonitorScaleFallback {args.scale}\n' + scale_config +
                'EdgeScroll 0 0\nEdgeThickness 0\n'
                'Colorset 0 fg white, bg black, hi white, sh black\n'
                # Inherit the default. An explicit MenuStyle Font would avoid
                # the startup default's borrowed-pointer lifetime under test.
                'MenuStyle * Colorset 0, ActiveColorset 0\n' +
                menu_definition + 'Mouse 1 R N Popup FontLifetimeMenu\n'
                'AddToFunc StartFunction I Module FvwmMFL\n')
            evidence['configuration_sha256'] = hashlib.sha256(config.read_bytes()).hexdigest()
            evidence['defaults_sha256'] = hashlib.sha256(defaults.read_bytes()).hexdigest()
            shutil.copy2(config, args.artifacts_dir / 'configuration.fvwm')
            core = runtime.start('core', [str(args.fvwm), '-d', display,
                '-f', str(config), '-v', '-o', str(runtime.root / 'debug.log')],
                'core.log', runtime.environment({'FVWM_USERDIR': str(runtime.root),
                    'FVWMMFL_SOCKET_PATH': str(runtime.root)}))
            endpoint = runtime.root / f"fvwm_mfl_{display.replace(':', '_')}.sock"
            wait_for_predicate('ordinary font-control endpoint', endpoint.exists,
                time.monotonic() + 12, process=core)
            connection = Connection(endpoint)
            connection.record()
            connection.send(b'set echo\n')
            connection.send(frame('font-scale', resource='workspaces'))
            workspace = reply(connection, 'font-scale')
            scales = [monitor['scale_quarters'] for monitor in workspace['monitors']]
            assert scales and all(value == int(args.scale) * 4 for value in scales), workspace
            evidence['observed_scale_quarters'] = scales
            evidence['xrandr_monitors'] = listing
            client = Client(display)
            errors = SessionXErrors(client, evidence).install()
            pointer = Pointer(client)
            x, d = client.x, client.display
            x.XGetWindowAttributes.argtypes = [C.c_void_p, C.c_ulong,
                C.POINTER(WindowAttributes)]
            x.XKeysymToKeycode.argtypes = [C.c_void_p, C.c_ulong]
            x.XKeysymToKeycode.restype = C.c_uint
            pointer.x.XTestFakeKeyEvent.argtypes = [C.c_void_p, C.c_uint,
                C.c_int, C.c_ulong]
            pointer.x.XTestFakeKeyEvent.restype = C.c_int
            x.XGetImage.argtypes = [C.c_void_p, C.c_ulong, C.c_int, C.c_int,
                C.c_uint, C.c_uint, C.c_ulong, C.c_int]
            x.XGetImage.restype = C.c_void_p
            x.XGetPixel.argtypes = [C.c_void_p, C.c_int, C.c_int]
            x.XGetPixel.restype = C.c_ulong
            x.XDestroyImage.argtypes = [C.c_void_p]
            x.XDefaultColormap.argtypes = [C.c_void_p, C.c_int]
            x.XDefaultColormap.restype = C.c_ulong
            x.XQueryColor.argtypes = [C.c_void_p, C.c_ulong, C.POINTER(XColor)]
            x.XListFonts.argtypes = [C.c_void_p, C.c_char_p, C.c_int,
                C.POINTER(C.c_int)]
            x.XListFonts.restype = C.POINTER(C.c_char_p)
            x.XFreeFontNames.argtypes = [C.POINTER(C.c_char_p)]
            for name in ('fixed', '6x13', '9x15'):
                count = C.c_int()
                names = x.XListFonts(d, name.encode(), 1, C.byref(count))
                try:
                    assert names and count.value == 1, ('Required base X font unavailable', name)
                finally:
                    if names:
                        x.XFreeFontNames(names)

            serial = 0
            def command(text):
                nonlocal serial
                serial += 1
                evidence.setdefault('commands', []).append(text)
                connection.send((text + '\n').encode())
                connection.echo('font-lifetime-' + str(serial))

            def mapped_menus():
                root_, parent, count = C.c_ulong(), C.c_ulong(), C.c_uint()
                children = C.POINTER(C.c_ulong)()
                assert x.XQueryTree(d, client.root, C.byref(root_),
                    C.byref(parent), C.byref(children), C.byref(count))
                assert count.value <= 256, count.value
                result = {}
                try:
                    for window in children[:count.value]:
                        attrs = WindowAttributes()
                        present = errors.read(window, 3,
                            lambda: x.XGetWindowAttributes(d, window, C.byref(attrs)))
                        if (present and attrs.map_state == 2 and attrs.save_under
                                and attrs.width > 20 and attrs.height > 20):
                            assert attrs.width <= 800 and attrs.height <= 600
                            result[window] = (attrs.width, attrs.height)
                finally:
                    if children:
                        x.XFree(children)
                return result

            def capture(window, width, height, label):
                previous, stable = None, 0
                def painted():
                    nonlocal previous, stable
                    image = x.XGetImage(d, window, 0, 0, width, height,
                        C.c_ulong(-1).value, 2)
                    assert image
                    try:
                        pixels = [x.XGetPixel(image, px, py)
                            for py in range(height) for px in range(width)]
                    finally:
                        x.XDestroyImage(image)
                    palette = {}
                    assert len(set(pixels)) <= 256
                    for pixel in set(pixels):
                        color = XColor(pixel=pixel)
                        assert x.XQueryColor(d, x.XDefaultColormap(d, 0), C.byref(color))
                        palette[pixel] = bytes((color.red >> 8, color.green >> 8, color.blue >> 8))
                    rows = [Counter(palette[pixels[py * width + px]]
                        for px in range(6, width - 6)) for py in range(6, height - 6)]
                    interior = sum(rows, Counter())
                    text_rows = sum(0 < row[b'\xff\xff\xff'] < (width - 12) // 2
                        for row in rows)
                    if (interior[b'\0\0\0'] <= (width - 12) * (height - 12) // 2
                            or interior[b'\xff\xff\xff'] < 8 or text_rows < 8):
                        stable = 0
                        return None
                    rgb = b''.join(palette[pixel] for pixel in pixels)
                    stable = stable + 1 if rgb == previous else 0
                    previous = rgb
                    return (rgb, interior, text_rows) if stable >= 2 else None
                rgb, interior, text_rows = wait_for_predicate('stable painted menu text',
                    painted, time.monotonic() + 5, process=core)
                data = png_rgb(width, height, rgb)
                (args.artifacts_dir / (label + '.png')).write_bytes(data)
                return {'window': window, 'width': width, 'height': height,
                    'capture': label + '.png', 'sha256': hashlib.sha256(data).hexdigest(),
                    'black_interior_samples': interior[b'\0\0\0'],
                    'white_interior_samples': interior[b'\xff\xff\xff'],
                    'rendered_text_rows': text_rows}

            def popup(label, font, menu='FontLifetimeMenu'):
                nonlocal held
                assert not mapped_menus() and not pointer.grabbed()
                command('Mouse 1 R N Popup ' + menu)
                pointer.motion(300, 250)
                pointer.button(True)
                held = True
                def shown():
                    menus = mapped_menus()
                    return menus if len(menus) == 1 and pointer.grabbed() else None
                menus = wait_for_predicate('ordinary popup after ' + label, shown,
                    time.monotonic() + 5, process=core)
                window, size = next(iter(menus.items()))
                try:
                    observed = capture(window, *size, label)
                    observed.update(case=label, requested_default_font=font,
                        menu=menu)
                    evidence['observations'].append(observed)
                    assert core.poll() is None and not errors.unexpected
                finally:
                    code = x.XKeysymToKeycode(d, 0xff1b)
                    assert code
                    for down in (1, 0):
                        assert pointer.x.XTestFakeKeyEvent(d, code, down, 0)
                    x.XSync(d, 0)
                    pointer.button(False)
                    held = False
                wait_for_predicate('ordinary popup fully dismissed',
                    lambda: not mapped_menus() and not pointer.grabbed(),
                    time.monotonic() + 5, process=core)
                command('Nop')

            # The first popup follows Read + DefaultFont fixed with no prior
            # menu bind or explicit menu-font override to mask the stale borrow.
            # This original control passed on the lead's old ASan core; that
            # green boundary is retained, not relabeled as a reproduction.
            popup('startup-before-first-popup', 'fixed')
            popup('created-before-startup-default-first-popup', 'fixed',
                'FontBeforeDefaultMenu')
            # Use a previously unbound default font so another popup's owned
            # fixed-font cache reference cannot keep this borrow alive. Both
            # the named style and the final root exist before the replacement.
            # Destroying the style resets its existing menu binding to '*';
            # recreation and ChangeMenuStyle must reconnect the new identity.
            command('DefaultFont 6x13\n'
                # Refresh the inherited source before root creation without
                # changing Font/TitleFont policy or awaiting an outer flush.
                'MenuStyle * Colorset 0, ActiveColorset 0\n'
                'MenuStyle SlateLauncherStyle Colorset 0, ActiveColorset 0, Translucent 100\n' +
                named_definition +
                'ChangeMenuStyle SlateLauncherStyle NamedFontLifetimeMenu\n'
                'DestroyMenuStyle SlateLauncherStyle\n'
                'MenuStyle SlateLauncherStyle Colorset 0, ActiveColorset 0, Translucent 100\n'
                'DestroyMenu NamedFontLifetimeMenu\n' + named_definition +
                'ChangeMenuStyle SlateLauncherStyle NamedFontLifetimeMenu\n'
                'DefaultFont 9x15')
            popup('named-style-recreated-before-first-popup', '9x15',
                'NamedFontLifetimeMenu')
            sizes = {}
            for index, font in enumerate(('6x13', '9x15', 'fixed') * 2):
                command('DefaultFont ' + font)
                popup('replacement-' + str(index), font)
                observed = evidence['observations'][-1]
                size = (observed['width'], observed['height'])
                if font in sizes:
                    assert size == sizes[font], (font, size, sizes[font])
                sizes[font] = size
            assert sizes['9x15'][1] > sizes['6x13'][1], sizes
            command('DestroyMenu FontLifetimeMenu\nDefaultFont 9x15\n' + menu_definition)
            popup('destroy-recreate-after-replacement', '9x15')
            command('DefaultFont fixed')
            connection.send(b'Quit\n')
            assert core.wait_until_exit(time.monotonic() + 5) == 0
            log = (runtime.root / 'core.log').read_text() + (runtime.root / 'debug.log').read_text()
            assert 'AddressSanitizer' not in log and 'runtime error:' not in log, log
            assert 'X error' not in log and not errors.unexpected, log
            evidence['status'] = 'passed'
        except BaseException as error:
            evidence.update(status='failed', error=repr(error),
                core_exit_status=core.poll() if core else None)
            raise
        finally:
            if held:
                pointer.button(False)
            if connection:
                connection.close()
            if core:
                core.close(time.monotonic() + 5)
            if errors:
                errors.close_display()
            elif client:
                client.close()
            for path in runtime.root.glob('*.log'):
                shutil.copy2(path, args.artifacts_dir / path.name)
            (args.artifacts_dir / 'evidence.json').write_text(json.dumps(evidence, indent=2) + '\n')
    print(json.dumps(evidence))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__, allow_abbrev=False)
    for name in ('fvwm', 'mfl', 'artifacts-dir'):
        parser.add_argument('--' + name, type=Path, required=True)
    parser.add_argument('--display')
    parser.add_argument('--scale', choices=('1', '2'), default='1')
    parser.add_argument('--execution-profile', required=True)
    options = parser.parse_args()
    for name in ('fvwm', 'mfl', 'artifacts_dir'):
        setattr(options, name, getattr(options, name).resolve())
    run(options)
