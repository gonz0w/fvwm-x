#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or any later version.
"""One combined checkpoint for independently composed native components."""
import argparse
import json
from pathlib import Path
import subprocess
import time

from binding_origin_live import run
from inspector_live import PROFILES
from live_x_harness import wait_for_predicate
from mfl_request_live import frame, reply
from mfl_stream_live import Connection
from settings_snapshot import expanded_settings


def component_scene(*, runtime, out, user, probe, query, snapshot,
                    before_settings, view, core, evidence, required, records, env):
    endpoint = runtime.root / ('fvwm_mfl_' + env['DISPLAY'].replace(':', '_') + '.sock')
    owner = Connection(endpoint)
    owner.record()
    commands = Connection(endpoint)
    commands.record()
    serial = 0
    titles = {'defaults': 'Desktop component defaults',
              'launcher': 'Launcher settings', 'forms': 'Form settings'}
    editor = None

    def wait(label, predicate):
        return wait_for_predicate(label, predicate, time.monotonic() + 10,
                                  process=core)

    def settings(scope='group:desktop'):
        return expanded_settings(query('settings', scope=scope))

    def change(values=None, reset=None):
        nonlocal serial
        current = settings('all')
        serial += 1
        token = 'components-' + str(serial)
        fields = dict(operation='reset' if reset else 'set',
                      provider=current['provider'], generation=current['generation'])
        if reset:
            fields['scope'] = reset
        else:
            fields['values'] = values
        owner.send(frame(token, method='action', resource='settings', **fields))
        result = expanded_settings(reply(owner, token))
        probe.barrier()
        state = json.loads((user / 'desktop-state.json').read_text())
        assert state['schema_version'] == 12
        evidence.setdefault('settings_changes', []).append(dict(fields=fields, result=result))
        return result

    def geometry(window, controls=False):
        attrs = view.attributes(window)
        if not attrs or attrs.map_state != 2:
            return None
        result = dict(width=attrs.width, height=attrs.height)
        assert 0 < attrs.width <= 1440 and 0 < attrs.height <= 1160, result
        if controls:
            items = []
            for child in view.children(window):
                item = view.attributes(child)
                if item and item.map_state == 2:
                    assert item.x >= 0 and item.y >= 0
                    assert item.x + item.width + 2 * item.border_width <= attrs.width
                    assert item.y + item.height + 2 * item.border_width <= attrs.height
                    items.append(dict(x=item.x, y=item.y, width=item.width,
                                      height=item.height, border=item.border_width))
            if not items:
                return None
            result['controls'] = sorted(items, key=lambda x: (x['y'], x['x'], x['width']))
        return result

    def settled(window, controls=False):
        previous, repeats = None, 0
        def stable():
            nonlocal previous, repeats
            current = geometry(window, controls)
            repeats = repeats + 1 if current and current == previous else 0
            previous = current
            return current if repeats >= 2 else None
        return wait('settled native component geometry', stable)

    def close(title):
        window = view.named(title)
        if window:
            probe.command('WindowId ' + str(window) + ' Close')
            wait('closed ' + title, lambda: view.named(title) is None)

    def open_editor(page):
        nonlocal editor
        probe.command('SendToModule FvwmDesktop edit-settings component:' + page)
        editor = view.wait(titles[page])
        return settled(editor, True)

    def capture(name):
        result = subprocess.run([str(required['capture']), str(out)], env=env,
                                text=True, capture_output=True, timeout=35, check=False)
        assert result.returncode == 0 and result.stdout.startswith('Screenshot saved: ')
        filename = 'component-' + name + '-' + evidence['profile'] + '.png'
        Path(result.stdout.strip().removeprefix('Screenshot saved: ')).replace(out / filename)
        evidence['screenshots'].append(filename)

    def menu(capture_name=None):
        def mapped():
            return [window for window in view.children(view.client.root)
                    if (attrs := view.attributes(window)) and attrs.map_state == 2
                    and attrs.save_under and attrs.width > 20 and attrs.height > 20]
        before = set(mapped())
        view.pointer.motion(1050, 100)
        # Menu is synchronous in the core. Do not wait for an observer command
        # acknowledgement that can only be delivered after this menu closes.
        commands.send(b'Menu ComponentCompositionProbe\n')
        try:
            wait('native component menu grab', view.pointer.grabbed)
            def target():
                found = [window for window in mapped() if window not in before]
                return found[0] if len(found) == 1 else None
            window = wait('actual component menu surface', target)
            result = settled(window)
            if capture_name:
                capture(capture_name)
            return result
        finally:
            view.key('Escape')
            wait('component menu released', lambda: not view.pointer.grabbed())

    try:
        capabilities = query('capabilities')
        assert capabilities['desktop_settings_result_max_bytes'] == 16384
        assert capabilities['files_result_max_bytes'] == 8192
        evidence['broker_limits'] = capabilities
        probe.command('DestroyMenu ComponentCompositionProbe',
            'AddToMenu ComponentCompositionProbe "Component title" Title',
            'AddToMenu ComponentCompositionProbe "Alpha action" Nop',
            'AddToMenu ComponentCompositionProbe "Bravo action" Nop',
            'AddToMenu ComponentCompositionProbe "Close" Nop',
            'ChangeMenuStyle SlateLauncherStyle ComponentCompositionProbe')
        original = settings()
        assert len(original['settings']) == 12
        base_font = 'xft:DejaVu Sans Mono:size=18' if evidence['profile'] == 'classic-large' else 'xft:Sans:size=10'
        change(dict(desktop_font=base_font, desktop_density='compact'))
        baseline_menu = menu()
        probe.command('SendToModule FvwmDesktop schedule')
        schedule = view.wait('Day / night schedule')
        original_form = settled(schedule, True)

        # Compare like font families in enlarged Classic: changing from a
        # monospace baseline to a proportional family need not grow its width.
        item_font, title_font = ('xft:DejaVu Sans Mono:size=24',
                                 'xft:DejaVu Sans Mono:size=28') \
            if evidence['profile'] == 'classic-large' else \
            ('xft:Sans:size=20', 'xft:DejaVu Sans Mono:size=24')
        change(dict(desktop_style='relief', launcher_font=item_font,
                    launcher_title_font=title_font, launcher_density='spacious'))
        assert view.named('Day / night schedule') == schedule
        assert settled(schedule, True) == original_form
        enlarged_menu = menu('relief-launcher')
        evidence['launcher_geometry'] = dict(baseline=baseline_menu,
            changed=enlarged_menu, item_font=item_font, title_font=title_font)
        assert enlarged_menu['height'] > baseline_menu['height']
        assert enlarged_menu['width'] > baseline_menu['width']
        close('Day / night schedule')
        probe.command('SendToModule FvwmDesktop schedule')
        schedule = view.wait('Day / night schedule')
        assert settled(schedule, True) == original_form
        evidence['cases']['launcher_axes_preserve_form_geometry'] = True

        body, input_, button = (24, 28, 26) if evidence['profile'] == 'classic-large' else (14, 18, 16)
        change(dict(form_font='xft:Sans:size=' + str(body),
                    form_input_font='xft:Sans:size=' + str(input_),
                    form_button_font='xft:Sans:size=' + str(button),
                    form_density='spacious'))
        assert view.named('Day / night schedule') == schedule
        assert settled(schedule, True) == original_form
        close('Day / night schedule')
        probe.command('SendToModule FvwmDesktop schedule')
        schedule = view.wait('Day / night schedule')
        new_form = settled(schedule, True)
        assert new_form['height'] > original_form['height']
        assert menu() == enlarged_menu
        close('Day / night schedule')
        evidence['cases']['form_roles_apply_only_at_next_start'] = True
        evidence['geometry'] = dict(baseline_menu=baseline_menu,
            enlarged_menu=enlarged_menu, original_form=original_form, new_form=new_form)

        current = settings()
        for key in ('desktop_style', 'launcher_font', 'launcher_title_font',
                    'launcher_density', 'form_font', 'form_input_font',
                    'form_button_font', 'form_density'):
            assert current['settings'][key]['component_version'] == 2
        saved_forms = settings('component:forms')['settings']
        change(reset='component:launcher')
        assert menu() == baseline_menu
        assert settings('component:forms')['settings'].keys() == saved_forms.keys()
        for key, row in saved_forms.items():
            assert settings('component:forms')['settings'][key]['saved'] == row['saved']
        change(reset='component:forms')
        probe.command('SendToModule FvwmDesktop schedule')
        schedule = view.wait('Day / night schedule')
        assert settled(schedule, True) == original_form
        close('Day / night schedule')
        evidence['cases']['scoped_reset_preserves_other_components'] = True

        # Inspect all compact pages with genuine keyboard navigation. Unsaved
        # editor presentation and Cancel must leave the persisted axes intact.
        before = (user / 'desktop-state.json').read_bytes()
        probe.command('SendToModule FvwmDesktop edit-settings group:desktop')
        editor = view.wait(titles['defaults'])
        evidence['default_editor'] = settled(editor, True)
        probe.command('WindowId ' + str(editor) + ' Focus')
        wait('component editor focus', lambda: view.focus() == editor)
        view.key('F9')
        editor = view.wait(titles['launcher'])
        settled(editor, True)
        capture('launcher-settings')
        probe.command('WindowId ' + str(editor) + ' Focus')
        wait('launcher editor focus', lambda: view.focus() == editor)
        view.key('F10')
        editor = view.wait(titles['forms'])
        settled(editor, True)
        capture('form-settings')
        probe.command('WindowId ' + str(editor) + ' Focus')
        wait('form editor focus', lambda: view.focus() == editor)
        view.key('Escape')
        wait('component Escape closes editor', lambda: view.named(titles['forms']) is None)
        editor = None
        assert (user / 'desktop-state.json').read_bytes() == before
        evidence['cases']['compact_pages_keyboard_and_cancel'] = True
        evidence['settings_final'] = settings()
        if evidence['profile'] == 'slate-dark':
            # Exercise the actual producer/broker round trip with distinct
            # maximum-length saved and module values, after visual captures.
            # Font matching may fall back; this is a transport oracle, not an
            # assertion that a synthetic 128-byte family exists on this host.
            font_keys = ('desktop_font', 'launcher_font', 'launcher_title_font',
                         'form_font', 'form_input_font', 'form_button_font')
            prefix = 'xft:Sans:size=10:family='
            saved, overrides = {}, {}
            for index, key in enumerate(font_keys):
                saved[key] = prefix + chr(65 + index) * (128 - len(prefix))
                overrides[key] = prefix + chr(75 + index) * (128 - len(prefix))
            for index, key in enumerate(('application_font', 'icon_theme', 'cursor_theme',
                                         'desktop_icon_theme')):
                saved[key] = chr(85 + index) * 128
                overrides[key] = chr(97 + index) * 128
            for key, value in saved.items():
                change({key: value})
            config = user / 'component-wire-overrides.config'
            config.write_text(''.join('*FvwmDesktop: ' + key + ' ' + value + '\n'
                                      for key, value in overrides.items()))
            probe.command('Read "' + str(config) + '"')
            probe.command('KillModule FvwmDesktop')
            probe.command('Module FvwmDesktop')
            def restarted():
                try:
                    current = settings('all')
                except AssertionError as failure:
                    response = failure.args[0] if failure.args else None
                    if isinstance(response, dict) and response.get('status') == 'error' \
                            and response.get('error', {}).get('code') in \
                            ('provider_unavailable', 'provider_not_ready'):
                        return None
                    raise
                if all(current['settings'].get(key, {}).get('effective') == value
                       for key, value in overrides.items()):
                    return current
            current = wait('restarted component provider with module overrides', restarted)
            raw = query('settings', scope='all')
            current = expanded_settings(raw)
            assert len(current['settings']) == 38
            for key, value in saved.items():
                assert current['settings'][key]['saved'] == value
                assert current['settings'][key]['effective'] == overrides[key]
                assert current['settings'][key]['source'] == 'module-configuration'
            wire_bytes = len(json.dumps({'result': raw}, ensure_ascii=False,
                                       separators=(',', ':')).encode())
            assert wire_bytes <= 16384
            evidence['maximum_font_wire'] = dict(bytes=wire_bytes,
                exceeds_previous_8192=wire_bytes > 8192, reply=raw)
            evidence['cases']['maximum_font_saved_override_wire_roundtrip'] = True
    finally:
        for title in (*titles.values(), 'Day / night schedule'):
            close(title)
        if view.pointer.grabbed():
            view.key('Escape')
            wait('final component grab released', lambda: not view.pointer.grabbed())
        probe.command('DestroyMenu ComponentCompositionProbe')
        owner.close()
        commands.close()


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__, allow_abbrev=False)
    parser.add_argument('--build', required=True, type=Path)
    parser.add_argument('--artifacts-dir', required=True, type=Path)
    parser.add_argument('--svg-worker', type=Path)
    parser.add_argument('--backer', type=Path)
    parser.add_argument('--profiles', nargs='+', choices=PROFILES, default=list(PROFILES))
    args = parser.parse_args()
    for profile in args.profiles:
        run(argparse.Namespace(**{**vars(args), 'profile': profile,
            'artifacts_dir': args.artifacts_dir / profile}), scenario=component_scene)
