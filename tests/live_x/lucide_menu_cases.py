# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License, version 2 or later.
"""Observe the installed opt-in example without enabling desktop actions."""
import ctypes as C
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import time

from live_x_harness import wait_for_predicate


def run(args, *, source, data, env, palettes, core, client, pointer, connection,
        command, mapped, dismiss, evidence):
    x, display = client.x, client.display
    destination = data / 'icons/FVWM-X-Lucide'
    shutil.copytree(source / 'default-config/icons/FVWM-X-Lucide', destination)
    command('Read ' + json.dumps(str(args.lucide_example.resolve())))
    # Ordinary example customization points its root at the private staged copy;
    # DESTDIR is deliberately absent from the installed runtime configuration.
    command('InfoStoreAdd fvwmx_lucide_root ' + json.dumps(str(destination / 'scalable/actions')))
    # This isolated build-tree fixture cannot rely on an installed defaults
    # file. Supply the exact Home menu binding from ConfigFvwmDefaults.
    command('Silent Key Home M A MenuMoveCursor 0 0')
    cases = evidence['lucide'] = dict(profiles=[],
        scope='three profiles at native scale 1; enlarged font included; no product actions',
        harness_icon_root=str(destination / 'scalable/actions'),
        harness_bindings=['Silent Key Home M A MenuMoveCursor 0 0'])

    def key(symbol):
        code = x.XKeysymToKeycode(display, symbol)
        assert code
        for down in (True, False):
            assert pointer.x.XTestFakeKeyEvent(display, code, down, 0)
        x.XSync(display, 0)

    def sample(window, roles):
        width, height = client.geometry(window)[2:]
        image = x.XGetImage(display, window, 0, 0, width, height,
            C.c_ulong(-1).value, 2)
        assert image
        try:
            # Only the prepared 24px mini-icon column, excluding border/text.
            pixels = {(px, y): x.XGetPixel(image, px, y)
                for y in range(2, height - 2)
                for px in range(2, min(26, width - 2))}
        finally:
            x.XDestroyImage(image)
        colors = set(value for pair in roles for value in pair)
        counts = {value: sum(p == value for p in pixels.values()) for value in colors}
        proofs = {}
        for role, (fg, bg) in enumerate(roles):
            f = [(fg >> shift) & 255 for shift in (16, 8, 0)]
            b = [(bg >> shift) & 255 for shift in (16, 8, 0)]
            channel = max(range(3), key=lambda i: abs(f[i] - b[i]))
            assert f[channel] != b[channel]
            for (px, y), value in pixels.items():
                rgb = [(value >> shift) & 255 for shift in (16, 8, 0)]
                alpha = (rgb[channel] - b[channel]) / (f[channel] - b[channel])
                if not .03 < alpha < .97 or any(abs(rgb[i] -
                        (b[i] + alpha * (f[i] - b[i]))) > 2 for i in range(3)):
                    continue
                nearby = {(nx, ny): pixels.get((nx, ny))
                    for ny in range(y - 2, y + 3)
                    for nx in range(px - 2, px + 3)}
                foreground = next((point for point, p in nearby.items() if p == fg), None)
                background = next((point for point, p in nearby.items() if p == bg), None)
                if foreground and background:
                    proofs[role] = dict(foreground=fg, background=bg, edge=value,
                        foreground_point=foreground, background_point=background,
                        edge_point=[px, y], blend_fraction=alpha,
                        measured_contrast=contrast(fg, bg))
                    break
        return counts, proofs

    def window_color_count(window, color):
        width, height = client.geometry(window)[2:]
        image = x.XGetImage(display, window, 0, 0, width, height,
            C.c_ulong(-1).value, 2)
        assert image
        try:
            return sum(x.XGetPixel(image, px, y) == color
                for y in range(height) for px in range(width))
        finally:
            x.XDestroyImage(image)

    def capture_to(name):
        result = subprocess.run([str(args.capture), str(args.artifacts_dir)],
            env=env, capture_output=True, text=True, timeout=35)
        assert result.returncode == 0, result.stderr
        saved = Path(result.stdout.strip().removeprefix('Screenshot saved: '))
        saved.replace(args.artifacts_dir / name)

    def contrast(fg, bg):
        def luminance(value):
            rgb = [(value >> shift & 255) / 255 for shift in (16, 8, 0)]
            linear = [v / 12.92 if v <= .04045 else ((v + .055) / 1.055) ** 2.4
                for v in rgb]
            return sum(a * b for a, b in zip(linear, (.2126, .7152, .0722)))
        light, dark = sorted((luminance(fg), luminance(bg)), reverse=True)
        return (light + .05) / (dark + .05)

    for profile in args.themes:
        if profile == 'classic-large':
            palette = dict(text='#000080', surface='#f5deb3', accent='#800000',
                on_accent='#ffffff', muted='#404040')
            font = 'xft:DejaVu Sans Mono:size=18'
        else:
            name, variant = profile.split('-')
            palette = palettes[name][variant]
            font = 'xft:Sans:size=11'
        roles = [(5, 'text', 'surface'), (6, 'on_accent', 'accent'),
                 (7, 'muted', 'surface'), (8, 'accent', 'surface')]
        setup = 'MenuStyle * Font "' + font + '", HilightBack, ActiveFore\n'
        for number, fg, bg in roles:
            setup += 'Colorset ' + str(number) + ' fg ' + palette[fg] + ', bg ' + palette[bg] + '\n'
        command(setup.rstrip())
        command('FVWMXLucideSetup')
        foregrounds = [int(palette[fg][1:], 16) for _, fg, _ in roles]
        backgrounds = [int(palette[bg][1:], 16) for _, _, bg in roles]
        pixel_roles = list(zip(foregrounds, backgrounds))
        # Each one-entry probe shares the catalog's retained picture by exact
        # path. Closing it supplies the existing async-image commit boundary.
        # A few ready role colors cannot prove that all 16 glyphs are ready.
        glyphs = json.loads((source / 'default-config/icons/FVWM-X-Lucide/provenance.json').read_text())['icons']
        ready_glyphs = []
        cases.setdefault('glyph_readiness', {})[profile] = ready_glyphs
        for glyph in glyphs:
            picture = str(destination / 'scalable/actions' / (glyph + '.svg'))
            command('DestroyMenu recreate FVWMXLucideReadiness\n'
                'AddToMenu FVWMXLucideReadiness ' + json.dumps('%' + picture + '%Glyph') +
                ' Nop\nChangeMenuStyle FVWMXLucideStyle FVWMXLucideReadiness')
            pointer.motion(350, 100)
            for ready_attempt in range(40):
                connection.send(b'Popup FVWMXLucideReadiness\n')
                probes = wait_for_predicate('individual Lucide glyph mapped', mapped,
                    time.monotonic() + 5, process=core)
                assert len(probes) == 1
                time.sleep(.05)
                glyph_counts, glyph_proofs = sample(probes[0], pixel_roles)
                ready_role = next((role for role in (0, 1)
                    if role in glyph_proofs and glyph_counts[foregrounds[role]] >= 16), None)
                dismiss()
                command('Nop')
                if ready_role is not None:
                    ready_glyphs.append(dict(name=glyph, role=ready_role,
                        pixel_witness=glyph_proofs[ready_role], attempts=ready_attempt + 1))
                    break
            else:
                raise AssertionError((profile, glyph, 'individual glyph never ready', glyph_counts))
        assert len(ready_glyphs) == 16
        command('DestroyMenu FVWMXLucideReadiness')
        pointer.motion(350, 100)
        for attempt in range(40):
            connection.send(b'Popup FVWMXLucideExamples\n')
            windows = wait_for_predicate('Lucide catalog mapped', mapped,
                time.monotonic() + 5, process=core)
            assert len(windows) == 1
            time.sleep(.05)
            counts, proofs = sample(windows[0], pixel_roles)
            if all(counts[foregrounds[i]] >= 16 and i in proofs for i in (0, 2, 3)):
                break
            dismiss()
            command('Nop')
        else:
            raise AssertionError((profile, 'glyphs not ready', counts))
        observation = dict(name=profile, status='normal witnessed; selection pending',
            geometry=client.geometry(windows[0]), normal_counts=counts,
            normal_pixel_witnesses=proofs.copy(), popup_attempts=attempt + 1)
        cases['profiles'].append(observation)
        key(0xff50)  # Explicit shipped Home binding: first selectable Nop.
        def selected():
            values, active = sample(windows[0], pixel_roles)
            observation['post_home_counts'] = values
            return (values, active) if values[foregrounds[1]] >= 16 and 1 in active else None
        try:
            selected_counts, selected_proofs = wait_for_predicate(
                'selected glyph uses active role', selected,
                time.monotonic() + 5, process=core)
        except BaseException as error:
            observation.update(status='selection failed; retained', error=repr(error),
                whole_window_active_pixels=window_color_count(windows[0], foregrounds[1]))
            failure_name = profile + '-lucide-selection-failed.png'
            try:
                capture_to(failure_name)
                observation['failure_screenshot'] = failure_name
            except BaseException as capture_error:
                observation['capture_error'] = repr(capture_error)
            raise
        proofs[1] = selected_proofs[1]
        assert all(p['measured_contrast'] >= 4.5 for p in proofs.values()), (profile, proofs)
        name = profile + '-lucide-menus.png'
        capture_to(name)
        observation.update(status='passed', selected_counts=selected_counts,
            whole_window_active_pixels=window_color_count(windows[0], foregrounds[1]),
            glyph_pixel_witnesses=proofs, screenshot=name)
        key(0xff0d)  # Return executes only the selected demonstration Nop.
        wait_for_predicate('Lucide keyboard action dismissed', lambda: not mapped(),
            time.monotonic() + 5, process=core)
        command('Nop')

    # An unrelated full-color menu keeps its original pixels after setup.
    image = data / 'icons/fixture/two/fixture.svg'
    command('DestroyMenu recreate LucideColorControl\nAddToMenu LucideColorControl '
        + json.dumps('%' + str(image) + '%Unrelated application artwork') + ' Nop')
    pointer.motion(350, 100)
    for attempt in range(40):
        connection.send(b'Popup LucideColorControl\n')
        windows = wait_for_predicate('unrelated image menu mapped', mapped,
            time.monotonic() + 5, process=core)
        assert len(windows) == 1
        time.sleep(.05)
        width, height = client.geometry(windows[0])[2:]
        image_data = x.XGetImage(display, windows[0], 0, 0, width, height,
            C.c_ulong(-1).value, 2)
        assert image_data
        try:
            green = sum(x.XGetPixel(image_data, px, y) == 0x00ff00
                for y in range(height) for px in range(width))
        finally:
            x.XDestroyImage(image_data)
        if green == 48 * 48:
            break
        dismiss()
        command('Nop')
    else:
        raise AssertionError(('unrelated image was tinted or unavailable', green))
    cases['untinted_control_green_pixels'] = green
    dismiss()
    files = list((source / 'default-config/icons/FVWM-X-Lucide').rglob('*'))
    files += [source / 'default-config/lucide-menus.fvwm.in', Path(__file__)]
    cases['source_hashes'] = {str(p.relative_to(source)):
        hashlib.sha256(p.read_bytes()).hexdigest() for p in files if p.is_file()}
    cases['configured_example_sha256'] = hashlib.sha256(args.lucide_example.read_bytes()).hexdigest()
    cases['status'] = 'passed'
