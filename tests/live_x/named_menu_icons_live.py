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
import re
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
                ['Xvfb', '-screen', '0', '1280x1200x24' if args.lucide_example else '900x600x24', '-noreset'],
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

            def ready_directory_images(text, label, color, extent):
                # Reuse the exact-path persistent probe from the Lucide
                # fixture. A self-destroying menu cannot retain its pending
                # SVG subscription across the popdown commit boundary.
                paths = sorted(set(re.findall(r'%([^%\n]+)%', text)))
                assert len(paths) <= 4
                for picture in paths:
                    command('DestroyMenu recreate IconReadiness\n'
                        'AddToMenu IconReadiness ' + json.dumps('%' + picture + '%Ready') + ' Nop')
                    for attempt in range(40):
                        connection.send(b'Popup IconReadiness\n')
                        windows = wait_for_predicate('exact directory image probe maps', mapped,
                            time.monotonic() + 5, process=core)
                        assert len(windows) == 1
                        time.sleep(.05)
                        pixels = color_count(windows[0], color)
                        dismiss()
                        command('Nop')
                        if pixels >= extent * extent:
                            evidence.setdefault('directory_image_readiness', []).append(
                                dict(name=label, path=picture, matching_pixels=pixels,
                                     attempts=attempt + 1))
                            break
                    else:
                        raise AssertionError((label, picture, 'persistent image never ready', pixels))

            def menu(kind, label, color, scale='2', capture=True, disabled=False):
                if kind == 'application':
                    argv = [str(args.disabled_desktop if disabled else args.desktop), '--title', 'IconApps', '--include-items',
                        'none', '--enable-mini-icons', '--theme', 'fixture', '--size', '24',
                        '--icon-scale', scale, '--app-icon', 'fixture', '--mini-icon-dir', str(cache)]
                    name = 'IconApps'
                else:
                    argv = [str(args.disabled_directory if disabled else args.directory), '--dir', str(listing), '--title', '',
                        '--command-title', 'Nop', '--command-file', 'Nop', '--icon-theme',
                        'fixture', '--icon-size', '24', '--icon-scale', scale,
                        '--mini-icon-dir', str(cache)]
                    name = str(listing)
                result = subprocess.run(argv, env=env, capture_output=True, timeout=12)
                assert result.returncode == 0, result.stderr.decode()
                text = result.stdout.decode()
                (args.artifacts_dir / (label + '.fvwm')).write_text(text)
                publication = 'PipeRead ' + json.dumps(' '.join(shlex.quote(v) for v in argv))
                command(publication)
                pointer.motion(350, 180)
                pixels = None
                extent = 48 if scale == '2' else 30
                # The mixed directory contains one broken folder image and
                # one healthy file image; the ordinary scene expects both.
                minimum = extent * extent * (2 if kind == 'directory' and
                                             not args.invalid_assets else 1)
                if kind == 'directory' and args.invalid_assets and color is not None:
                    ready_directory_images(text, label, color, extent)
                for attempt in range(40):
                    recreated = kind == 'directory' and attempt > 0
                    if recreated:
                        # Directory menus destroy themselves on popdown. The
                        # image-readiness retry needs a fresh published menu.
                        command(publication)
                        if args.invalid_assets and color is not None:
                            ready_directory_images(text, label, color, extent)
                    trace = {'name': label, 'index': attempt,
                             'recreated': recreated, 'mapped': False}
                    evidence.setdefault('popup_attempts', []).append(trace)
                    connection.send(('Popup ' + json.dumps(name) + '\n').encode())
                    windows = wait_for_predicate('named icon menu maps', mapped,
                        time.monotonic() + 5, process=core)
                    trace['mapped'] = True
                    assert len(windows) == 1, windows
                    # An open modal menu retains its prepared pictures. Closing
                    # it exposes the existing async-image commit boundary;
                    # validate the next popup rather than demand an idle repaint.
                    time.sleep(.05)
                    pixels = color_count(windows[0], color) if color is not None else None
                    trace['matching_pixels'] = pixels
                    if color is None or pixels >= minimum:
                        if kind == 'directory' and args.invalid_assets and attempt == 0:
                            # Always witness dismissal/recreation, even when
                            # the first popup's images were already ready.
                            dismiss()
                            command('Nop')
                            continue
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
                if kind == 'directory' and args.invalid_assets:
                    command('DestroyMenu IconReadiness')
                return text

            if args.lucide_example:
                from lucide_menu_cases import run as run_lucide
                run_lucide(args, source=source, data=data, env=env, palettes=palettes,
                    core=core, client=client, pointer=pointer, connection=connection,
                    command=command, mapped=mapped, dismiss=dismiss, evidence=evidence)
            elif args.invalid_assets:
                # A broken preferred PNG must not discard a valid sibling or
                # usable action. The native renderer still owns SVG decoding.
                (apps / 'healthy.desktop').write_text('[Desktop Entry]\n'
                    'Type=Application\nName=Healthy sibling\nExec=/bin/true\n'
                    'Icon=healthy\n')
                for directory in ('one', 'two'):
                    (theme / directory / 'healthy.svg').write_text(artwork('#00ff00'))
                    (theme / directory / 'fixture.png').write_bytes(b'not a PNG')
                    (theme / directory / 'folder.png').write_bytes(b'not a PNG')
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
                    text = menu('application', name + '-invalid-application', 0x00ff00)
                    assert 'Named icon application' in text and 'Healthy sibling' in text
                    menu('directory', name + '-invalid-directory', 0x00ff00)
                # Replacement creates a fresh immutable identity without
                # invalidating exports retained by the earlier menu.
                menu('application', 'mixed-quarter-scale', 0x00ff00, scale='1.25')
                before = menu('application', 'mixed-before-refresh', 0x00ff00, capture=False)
                retained = {path: path.read_bytes() for path in cache.glob('.fvwm-xdg-*')
                            if path.is_file()}
                assert retained
                (theme / 'two/healthy.svg').write_text(artwork('#0000ff'))
                after = menu('application', 'mixed-after-refresh', 0x0000ff)
                assert before != after
                for path, contents in retained.items():
                    assert path.read_bytes() == contents, 'referenced export changed'
                if args.disabled_desktop:
                    for kind in ('application', 'directory'):
                        text = menu(kind, 'formats-disabled-' + kind, None, disabled=True)
                        assert '.fvwm-xdg-' not in text
                    evidence['formats_disabled'] = True
                # A malformed selected index yields text, without skipping
                # its precedence in favour of another theme's metadata.
                (theme / 'index.theme').write_text('[Icon Theme]\nDirectories=../bad\n')
                text = menu('application', 'invalid-theme-text', None)
                assert 'Named icon application' in text and 'Healthy sibling' in text
                assert '.fvwm-xdg-' not in text
                text = menu('directory', 'invalid-theme-directory-text', None)
                assert '.fvwm-xdg-' not in text
                evidence['invalid_assets_preserve_actions_and_siblings'] = True
                evidence['replacement_retains_exports'] = True
            else:
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
    parser.add_argument('--lucide-example', type=Path,
        help='Run the optional installed menu example cases instead of synthetic themes')
    parser.add_argument('--invalid-assets', action='store_true',
        help='Run only the mixed malformed-image and text-fallback checkpoint')
    parser.add_argument('--disabled-desktop', type=Path)
    parser.add_argument('--disabled-directory', type=Path)
    args = parser.parse_args()
    if bool(args.disabled_desktop) != bool(args.disabled_directory):
        parser.error('both disabled-format producers are required together')
    if args.invalid_assets and args.lucide_example:
        parser.error('invalid-asset and Lucide scenes are separate checkpoints')
    run(args)
