#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
"""Two real menu producers share scaled icon lookup and content refresh."""
import argparse
import ctypes as C
import hashlib
import json
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import time

from colorset_live import WindowAttributes
from geometry_transition_live import Client, Pointer
from live_x_harness import HarnessRuntime, start_display_server, wait_for_predicate
from mfl_stream_live import Connection
from session_x_errors import SessionXErrors


def run(args):
    args.artifacts_dir.mkdir(parents=True, exist_ok=False)
    evidence = {'themes': {}, 'cases': [], 'product_actions_executed': False}
    source = Path(__file__).resolve().parents[2]
    palettes = json.loads((source / 'default-config/palettes.json').read_text())
    with HarnessRuntime(prefix='fvwm-named-icons-') as runtime:
        connection = client = errors = None
        try:
            _, display = start_display_server(runtime, 'Xvfb',
                ['Xvfb', '-screen', '0', '900x600x24', '-noreset'],
                'xserver.log', time.monotonic() + 10)
            data, config_home = runtime.root / 'data', runtime.root / 'xdg'
            apps, menus = data / 'applications', config_home / 'menus'
            listing, cache = runtime.root / 'listing', runtime.root / 'icons'
            for path in (apps, menus, listing / 'folder'):
                path.mkdir(parents=True)
            (listing / 'document.txt').write_text('fixture')
            (listing / 'folder/child.txt').write_text('child')
            app = apps / 'fixture.desktop'
            app.write_text('[Desktop Entry]\nType=Application\n'
                'Name=Named icon application\nExec=/bin/true\nIcon=fixture\n')
            (menus / 'applications.menu').write_text('<Menu><Name>Applications</Name>'
                '<DefaultAppDirs/><Include><All/></Include></Menu>')
            theme = data / 'icons/fixture'
            (theme / 'one').mkdir(parents=True)
            (theme / 'two').mkdir()
            (theme / 'index.theme').write_text('[Icon Theme]\nDirectories=one\n'
                'ScaledDirectories=two\n[one]\nSize=48\nScale=1\nType=Fixed\n'
                '[two]\nSize=24\nScale=2\nType=Fixed\n')
            def artwork(color):
                return ('<svg xmlns="http://www.w3.org/2000/svg" width="48" '
                    'height="48"><rect width="48" height="48" fill="' + color +
                    '"/></svg>')
            names = ('fixture', 'folder-open', 'folder', 'text-x-generic',
                     'application-x-executable')
            for name in names:
                (theme / ('one/' + name + '.svg')).write_text(artwork('#ff0000'))
                (theme / ('two/' + name + '.svg')).write_text(artwork('#00ff00'))
            config = runtime.root / 'config'
            config.write_text('DefaultFont fixed\nModulePath ' + str(args.mfl.parent) +
                '\nAddToFunc StartFunction I Module FvwmMFL\n')
            env = runtime.environment({'DISPLAY': display, 'HOME': str(runtime.root),
                'FVWM_USERDIR': str(runtime.root), 'FVWMMFL_SOCKET_PATH': str(runtime.root),
                'XDG_CONFIG_HOME': str(config_home), 'XDG_CONFIG_DIRS': str(runtime.root/'absent'),
                'XDG_DATA_HOME': str(data), 'XDG_DATA_DIRS': str(runtime.root/'absent'),
                'XDG_MENU_PREFIX': '', 'LC_ALL': 'C',
                'FVWM_IMAGE_SVG_WORKER': str(args.worker)})
            core = runtime.start('core', [str(args.fvwm), '-d', display,
                '-f', str(config)], 'core.log', env)
            socket = runtime.root / ('fvwm_mfl_' + display.replace(':', '_') + '.sock')
            wait_for_predicate('named icon MFL', socket.exists,
                time.monotonic() + 10, process=core)
            connection = Connection(socket)
            connection.record()
            connection.send(b'set echo\n')
            client, sequence = Client(display), 0
            errors = SessionXErrors(client, evidence).install()
            pointer = Pointer(client)
            x, d = client.x, client.display
            x.XGetWindowAttributes.argtypes = [C.c_void_p, C.c_ulong, C.POINTER(WindowAttributes)]
            x.XGetImage.argtypes = [C.c_void_p, C.c_ulong, C.c_int, C.c_int,
                C.c_uint, C.c_uint, C.c_ulong, C.c_int]
            x.XGetImage.restype = C.c_void_p
            x.XGetPixel.argtypes = [C.c_void_p, C.c_int, C.c_int]
            x.XGetPixel.restype = C.c_ulong
            x.XDestroyImage.argtypes = [C.c_void_p]
            x.XKeysymToKeycode.argtypes = [C.c_void_p, C.c_ulong]
            x.XKeysymToKeycode.restype = C.c_uint
            x.XGrabServer.argtypes = [C.c_void_p]
            x.XUngrabServer.argtypes = [C.c_void_p]
            pointer.x.XTestFakeKeyEvent.argtypes = [C.c_void_p, C.c_uint, C.c_int, C.c_ulong]

            def command(text):
                nonlocal sequence
                sequence += 1
                connection.send((text + '\n').encode())
                connection.echo('icon-command-' + str(sequence))

            def mapped():
                root, parent, count = C.c_ulong(), C.c_ulong(), C.c_uint()
                children = C.POINTER(C.c_ulong)()
                found = []
                x.XGrabServer(d)
                try:
                    assert x.XQueryTree(d, client.root, C.byref(root), C.byref(parent),
                        C.byref(children), C.byref(count))
                    for window in children[:count.value]:
                        attrs = WindowAttributes()
                        if x.XGetWindowAttributes(d, window, C.byref(attrs)) and (
                                attrs.map_state == 2 and attrs.save_under and attrs.width > 20):
                            found.append(window)
                finally:
                    if children:
                        x.XFree(children)
                    x.XUngrabServer(d)
                    x.XSync(d, 0)
                return found

            def color_count(window, color):
                width, height = client.geometry(window)[2:]
                image = x.XGetImage(d, window, 0, 0, width, height,
                    C.c_ulong(-1).value, 2)
                assert image
                try:
                    return sum(x.XGetPixel(image, px, py) == color
                        for py in range(height) for px in range(width))
                finally:
                    x.XDestroyImage(image)

            def dismiss():
                code = x.XKeysymToKeycode(d, 0xff1b)
                for _ in range(2):
                    for down in (True, False):
                        assert pointer.x.XTestFakeKeyEvent(d, code, down, 0)
                x.XSync(d, 0)
                wait_for_predicate('icon menu dismissed', lambda: not mapped(),
                    time.monotonic() + 5, process=core)

            def menu(kind, label, color, scale='2', capture=True):
                if kind == 'application':
                    argv = [str(args.desktop), '--title', 'IconApps', '--include-items',
                        'none', '--enable-mini-icons', '--theme', 'fixture', '--size', '24',
                        '--icon-scale', scale, '--app-icon', 'fixture', '--mini-icon-dir', str(cache)]
                    name = 'IconApps'
                else:
                    argv = [str(args.directory), '--dir', str(listing), '--title', '',
                        '--command-title', 'Nop', '--command-file', 'Nop', '--icon-theme',
                        'fixture', '--icon-size', '24', '--icon-scale', scale,
                        '--mini-icon-dir', str(cache)]
                    name = str(listing)
                result = subprocess.run(argv, env=env, capture_output=True, timeout=12)
                assert result.returncode == 0, result.stderr.decode()
                text = result.stdout.decode()
                (args.artifacts_dir / (label + '.fvwm')).write_text(text)
                command('PipeRead ' + json.dumps(' '.join(shlex.quote(v) for v in argv)))
                pointer.motion(350, 180)
                pixels = None
                extent = 48 if scale == '2' else 30
                minimum = extent * extent * (2 if kind == 'directory' else 1)
                for attempt in range(40):
                    connection.send(('Popup ' + json.dumps(name) + '\n').encode())
                    windows = wait_for_predicate('named icon menu maps', mapped,
                        time.monotonic() + 5, process=core)
                    assert len(windows) == 1, windows
                    # An open modal menu retains its prepared pictures. Closing
                    # it exposes the existing async-image commit boundary;
                    # validate the next popup rather than demand an idle repaint.
                    time.sleep(.05)
                    pixels = color_count(windows[0], color) if color is not None else None
                    if color is None or pixels >= minimum:
                        break
                    dismiss()
                    command('Nop')
                else:
                    raise AssertionError((label, 'image never ready', pixels, minimum))
                if color is not None:
                    assert pixels % (extent * extent) == 0, (label, pixels, extent)
                    if kind == 'application':
                        assert pixels == extent * extent, (label, pixels, extent)
                if capture:
                    q = subprocess.run([str(args.capture), str(args.artifacts_dir)],
                        env=env, capture_output=True, text=True, timeout=35)
                    assert q.returncode == 0, q.stderr
                    saved = Path(q.stdout.strip().removeprefix('Screenshot saved: '))
                    saved.replace(args.artifacts_dir / (label + '.png'))
                evidence['cases'].append({'name': label, 'argv': argv,
                    'geometry': client.geometry(windows[0]), 'expected_color': color,
                    'matching_pixels': pixels, 'popup_attempts': attempt + 1,
                    'output_sha256': hashlib.sha256(result.stdout).hexdigest()})
                dismiss()
                return text

            for name in args.themes:
                if name == 'classic-large':
                    fg, bg, hi, font = 'navy', 'wheat', 'ivory', 'xft:DejaVu Sans Mono:size=18'
                else:
                    palette, variant = name.split('-')
                    colors = palettes[palette][variant]
                    fg, bg, hi, font = colors['text'], colors['surface'], colors['accent'], 'xft:Sans:size=11'
                settings = ('Colorset 0 fg ' + fg + ', bg ' + bg + '\n'
                    'Colorset 1 fg ' + fg + ', bg ' + hi + '\n'
                    'MenuStyle * Font "' + font + '", MenuColorset 0, ActiveColorset 1\n')
                command(settings.rstrip())
                evidence['themes'][name] = settings
                (args.artifacts_dir / (name + '.config')).write_text(settings)
                menu('application', name + '-application', 0x00ff00)
                menu('directory', name + '-directory', 0x00ff00)
            menu('application', 'quarter-scale', 0xff0000, scale='1.25')
            before = menu('application', 'before-refresh', 0x00ff00, capture=False)
            (theme / 'two/fixture.svg').write_text(artwork('#0000ff'))
            after = menu('application', 'after-refresh', 0x0000ff)
            assert before != after, 'same-name overwrite reused presentation identity'
            app.write_text(app.read_text().replace('Icon=fixture', 'Icon=missing'))
            (theme / 'two/fixture.svg').unlink()
            (theme / 'one/fixture.svg').unlink()
            missing = menu('application', 'text-fallback', None)
            assert 'Named icon application' in missing and '%' not in missing
            evidence['refreshed_content_identity'] = True
            evidence['missing_icon_retains_label'] = True
            evidence['source_hashes'] = {str(path.relative_to(source)):
                hashlib.sha256(path.read_bytes()).hexdigest() for path in
                [source/'default-config/palettes.json', Path(__file__)]}
        finally:
            if connection:
                connection.close()
            if errors:
                errors.close_display()
            elif client:
                client.close()
            for log in runtime.root.glob('*.log'):
                shutil.copy2(log, args.artifacts_dir / log.name)
            (args.artifacts_dir / 'evidence.json').write_text(json.dumps(evidence, indent=2) + '\n')
    assert not evidence['observer_unexpected_x_errors'], evidence


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('fvwm', 'mfl', 'desktop', 'directory', 'worker', 'capture', 'artifacts-dir'):
        parser.add_argument('--' + name, type=Path, required=True)
    parser.add_argument('--themes', nargs='+',
        choices=('slate-dark', 'dune-light', 'classic-large'),
        default=['slate-dark', 'dune-light', 'classic-large'])
    run(parser.parse_args())
