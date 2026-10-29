#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or any later version.
"""Directed core queries checked against independent X/configuration observers."""
import argparse
import ctypes as C
import json
import os
from pathlib import Path
import shutil
import signal
import subprocess
import time

from geometry_transition_live import Client
from live_x_harness import (HarnessRuntime, parse_xrandr_monitors,
    parse_xrandr_output_ids, start_display_server, wait_for_predicate)
from mfl_stream_live import Connection
from mfl_request_live import frame, reply


def run(args):
    args.artifacts_dir.mkdir(parents=True, exist_ok=True)
    evidence = {'status': 'running'}
    with HarnessRuntime(prefix='fvwm-core-query-') as runtime:
        core, observer, stopped = None, None, False
        connections = []
        try:
            display = args.display
            if display is None:
                _, display = start_display_server(runtime, 'Xvfb',
                    ['Xvfb', '-screen', '0', '800x600x24', '-noreset'],
                    'xserver.log', time.monotonic() + 10)
            marker = runtime.root / 'must-not-execute'
            config = runtime.root / 'config'
            config.write_text(f'ModulePath {args.mfl.parent}\n'
                'DesktopSize 3x2\nEwmhNumberOfDesktops 4\n'
                'EdgeScroll 0 0\nEdgeThickness 0\nStyle * TileCascadePlacement\n'
                f'Key F1 R N Exec touch {marker}\n'
                'PointerKey F2 R N Echo pointer-key-query\n'
                'Mouse 3 R N Echo mouse-query\n'
                'AddToFunc StartFunction I Module FvwmMFL\n')
            core = runtime.start('core', [str(args.fvwm), '-d', display,
                '-f', str(config), '-v', '-o', str(runtime.root / 'debug.log')],
                'core.log', runtime.environment({'FVWMMFL_SOCKET_PATH': str(runtime.root),
                    'FVWM_USERDIR': str(runtime.root)}))
            endpoint = runtime.root / f"fvwm_mfl_{display.replace(':', '_')}.sock"
            wait_for_predicate('core-query endpoint', endpoint.exists,
                               time.monotonic() + 12, process=core)
            def connect():
                c = Connection(endpoint)
                c.socket.settimeout(10)
                c.record()
                connections.append(c)
                return c
            writer, rpc = connect(), connect()
            writer.send(b'set echo\n')
            writer.echo('core-query-started')
            rpc.send(frame('ro', 'set_mode', mode='read-only'))
            assert reply(rpc, 'ro')['read_only']
            serial = 0
            def query(resource, expected=None, **fields):
                nonlocal serial
                serial += 1
                token = 'core-' + str(serial)
                rpc.send(frame(token, resource=resource, **fields))
                return reply(rpc, token, 'error' if expected else 'ok')
            topology = query('topology')
            assert topology['state'] == 'observed'
            assert topology['scope'] == 'committed-topology'
            assert topology['units'] == 'actual-pixels'
            assert len(topology['topology']['epoch']) == 16
            listing = subprocess.check_output(['xrandr', '--display', display,
                '--listmonitors'], text=True, timeout=5)
            verbose = subprocess.check_output(['xrandr', '--display', display,
                '--verbose'], text=True, timeout=5)
            facts = parse_xrandr_monitors(listing)
            outputs = parse_xrandr_output_ids(verbose)
            monitors = topology['topology']['monitors']
            if facts:
                assert not topology['root_fallback']
                assert {m['name'] for m in monitors} == set(facts)
                for monitor in monitors:
                    fact = facts[monitor['name']]
                    assert monitor['geometry'] == {k: fact[k]
                        for k in ['x', 'y', 'width', 'height']}
                    assert monitor['physical_mm'] == dict(width=fact['mm_width'],
                                                         height=fact['mm_height'])
                    assert monitor['outputs'] == [outputs[n] for n in fact['output_names']]
            else:
                assert topology['root_fallback'] and len(monitors) == 1
                assert monitors[0]['name'] == 'FVWM-X-Root'
                assert monitors[0]['outputs'] == []
            evidence['topology'] = dict(observed=topology, xrandr_monitors=listing,
                                       xrandr_outputs=verbose)

            observer = Client(display)
            x, d = observer.x, observer.display
            x.XInternAtom.argtypes = [C.c_void_p, C.c_char_p, C.c_int]
            x.XInternAtom.restype = C.c_ulong
            x.XGetWindowProperty.argtypes = [C.c_void_p, C.c_ulong, C.c_ulong,
                C.c_long, C.c_long, C.c_int, C.c_ulong, C.POINTER(C.c_ulong),
                C.POINTER(C.c_int), C.POINTER(C.c_ulong), C.POINTER(C.c_ulong),
                C.POINTER(C.c_void_p)]
            def prop(name, type_=6):
                actual, count, after = C.c_ulong(), C.c_ulong(), C.c_ulong()
                format_, data = C.c_int(), C.c_void_p()
                atom = x.XInternAtom(d, name.encode(), 0)
                assert x.XGetWindowProperty(d, observer.root, atom, 0, 256,
                    0, type_, C.byref(actual), C.byref(format_), C.byref(count),
                    C.byref(after), C.byref(data)) == 0
                try:
                    assert actual.value == type_ and format_.value == 32 and after.value == 0
                    values = C.cast(data, C.POINTER(C.c_ulong))
                    return [values[i] for i in range(count.value)]
                finally:
                    if data:
                        x.XFree(data)
            initial = query('workspaces')
            assert initial['work_area_freshness'] == 'cached'
            observer.create()
            wait_for_predicate('observer managed',
                lambda: observer.parent(observer.window) != observer.root,
                time.monotonic() + 5, process=core)
            writer.send(b'GotoDesk 0 2\nGotoPage 1 1\n')
            writer.echo('workspace-change-processed')
            wait_for_predicate('independent desk publication',
                lambda: prop('_NET_CURRENT_DESKTOP') == [2], time.monotonic() + 5,
                process=core)
            current = query('desktop')
            workspaces = query('workspaces')
            assert workspaces['desktop_mode'] == 'global'
            assert len(workspaces['monitors']) == len(monitors)
            for monitor in workspaces['monitors']:
                assert monitor['desk'] == 2
                assert monitor['viewport'] == current['viewport']
                assert monitor['pages'] == dict(width=3, height=2)
                assert monitor['scale_quarters'] >= 4
                assert monitor['work_area'] == current['work_area']
                assert set(monitor['dynamic_work_area']) == {'x', 'y', 'width', 'height'}
            area = workspaces['monitors'][0]['work_area']
            assert prop('_NET_WORKAREA')[8:12] == [area[k]
                for k in ['x', 'y', 'width', 'height']], (prop('_NET_WORKAREA'), area)
            assert query('topology') == topology  # queries never publish topology
            evidence['workspaces'] = workspaces

            first = query('bindings', limit=1)
            generation = first['generation']
            assert len(generation) == 48
            bindings = first['bindings'][:]
            page, pages = first, 1
            while page['next_offset'] is not None:
                assert pages < 256
                page = query('bindings', offset=page['next_offset'], limit=8,
                             generation=generation)
                assert page['generation'] == generation
                bindings.extend(page['bindings'])
                pages += 1
            assert any(b['type'] == 'Key' and b['key'] == 'F1' and
                       b['action'] == f'Exec touch {marker}' for b in bindings)
            assert any(b['type'] == 'PointerKey' and b['key'] == 'F2' for b in bindings)
            assert any(b['type'] == 'MousePress' and b['code'] == 3 for b in bindings)
            assert all(isinstance(b['context_mask'], int) and
                       isinstance(b['modifier_mask'], int) for b in bindings)
            assert not marker.exists()
            assert query('bindings', limit=1)['generation'] == generation
            writer.send(b'Key F3 R N Echo new-binding\n')
            writer.echo('binding-change-processed')
            assert query('bindings', expected=True, offset=first['next_offset'],
                         generation=generation) == 'stale_generation'
            updated = query('bindings')
            assert updated['generation'] != generation
            writer.send(b'Key F3 R N -\n')
            writer.echo('binding-unbound')
            assert query('bindings')['generation'] != updated['generation']
            large = runtime.root / 'large-binding'
            large.write_text('Key F9 R N Nop ' + 'x' * 1100 + '\n')
            writer.send(('Read ' + str(large) + '\n').encode())
            writer.echo('large-binding-loaded')
            assert query('bindings', expected=True, limit=1) == 'query_too_large'
            writer.send(b'Key F9 R N -\n')
            writer.echo('large-binding-removed')
            assert query('bindings')['bindings']
            evidence['bindings'] = dict(entries=len(bindings), pages=pages,
                configuration_observed=True, no_action_execution=True,
                generation_stable_on_queries=True, stale_continuation_refused=True,
                unbind_invalidates=True, oversized_action_refused=True)

            caps = query('capabilities')
            assert caps['binding_scopes'] == ['global', 'menu', 'menu-fallback',
                                              'placement', 'policy']
            def binding_list(scope):
                page = query('bindings', scope=scope, limit=2)
                first_page = page
                entries = page['bindings'][:]
                while page['next_offset'] is not None:
                    assert len(entries) < 4096
                    page = query('bindings', scope=scope,
                        offset=page['next_offset'], limit=16,
                        generation=first_page['generation'])
                    assert page['generation'] == first_page['generation']
                    entries.extend(page['bindings'])
                return first_page, entries
            fallback_page, fallback = binding_list('menu-fallback')
            assert fallback_page['scope'] == 'menu-fallback-bindings'
            assert fallback and all(b['context_mask'] & 16 for b in fallback)
            menu_page, menu_before = binding_list('menu')
            writer.send(b'Mouse 1 M N Nop literal-$$[desk.n]\n')
            writer.echo('menu-override-loaded')
            menu_page, menu = binding_list('menu')
            assert menu_page['scope'] == 'menu-bindings'
            assert any(b['type'] == 'MousePress' and b['code'] == 1 and
                       b['action'] == 'Nop literal-$[desk.n]' for b in menu), menu
            assert binding_list('menu-fallback')[1] == fallback
            assert query('bindings', expected=True, scope='menu-fallback',
                generation=fallback_page['generation']) == 'stale_generation'
            writer.send(b'Mouse 1 M N -\n')
            writer.echo('menu-override-removed')
            assert binding_list('menu')[1] == menu_before
            assert binding_list('menu-fallback')[1] == fallback

            placement = query('bindings', scope='placement')
            assert placement['scope'] == 'placement-bindings'
            assert placement['button_count'] == 15
            assert not placement['key_bindings'] and not placement['modifier_bindings']
            writer.send(b'Mouse 0 P N CancelPlacement\n'
                        b'Mouse 3 P N PlaceWindowDrag\n'
                        b'Mouse 2 P N PlaceWindowInteractive\n')
            writer.echo('placement-policy-loaded')
            placed = query('bindings', scope='placement')
            assert placed['drag_finish_mask'] == 4
            assert placed['interactive_finish_mask'] == 2
            assert query('bindings', expected=True, scope='placement',
                generation=placement['generation']) == 'stale_generation'
            assert query('bindings', scope='placement',
                         generation=placed['generation']) == placed
            for fields in [dict(scope='placement', offset=0),
                           dict(scope='policy', limit=1), dict(scope='unknown')]:
                assert query('bindings', expected=True, **fields) == 'invalid_request'

            policy = query('bindings', scope='policy')
            assert policy['scope'] == 'binding-policy'
            assert policy['contexts']['menu_item'] == 20
            assert policy['contexts']['placement'] == 32
            assert policy['modifiers']['control'] == 4
            writer.send(b'IgnoreModifiers L2\n')
            writer.echo('ignore-modifiers-loaded')
            changed_policy = query('bindings', scope='policy')
            assert changed_policy['ignored_modifier_mask'] == 18
            assert changed_policy['used_modifier_mask'] == 237
            assert changed_policy['generation'] != policy['generation']
            assert query('bindings', expected=True, scope='policy',
                generation=policy['generation']) == 'stale_generation'
            assert query('bindings', expected=True, limit=1,
                generation=placed['generation']) == 'stale_generation'
            # A no-op policy command keeps a stable observation generation.
            writer.send(b'IgnoreModifiers L2\n')
            writer.echo('ignore-modifiers-unchanged')
            assert query('bindings', scope='policy') == changed_policy
            writer.send(b'IgnoreModifiers default\n')
            writer.echo('ignore-modifiers-restored')
            assert query('bindings', scope='policy')['generation'] != changed_policy['generation']
            assert not marker.exists()
            evidence['binding_scopes'] = dict(menu_entries=len(menu),
                fallback_entries=len(fallback), override_does_not_remove_fallback=True,
                normalized_placement=placed, policy=changed_policy,
                generation_checked_across_domains=True, literal_query_no_execution=True)

            # Appearance policy is observable without executing stored data.
            appearance = query('appearance')
            assert appearance['scope'] == 'core-appearance-configuration'
            assert not appearance['conditional_application']
            assert appearance['preview_supported']
            assert appearance['resource_preview'] == dict(version=1,
                scope='high-contrast', role_count=14, style_count=2,
                metadata_max=65536, preparation_timeout_ms=5000,
                lease_limit=1, slot_available=True)
            assert appearance['opaque_colors_preview'] == dict(version=1,
                plan_version=2, scope='opaque-colors', role_count=14, style_count=2,
                metadata_max=65536, preparation_timeout_ms=5000,
                lease_limit=1, slot_available=True)
            assert query('appearance') == appearance
            def policy_pages(resource, property):
                generation = query('appearance')['generation']
                offset, values = 0, []
                while offset is not None:
                    page = query(resource, limit=1, offset=offset,
                        generation=generation)
                    assert page['generation'] == generation
                    assert page['units'] == 'configuration-pixels'
                    values.extend(page[property])
                    offset = page['next_offset']
                return values, generation
            writer.send(b'MenuStyle "Query menu $$[desk.n]" Animation, '
                b'Translucent 73, BorderWidth 3, PopupDelay 250, '
                b'PopdownDelay 400, MenuColorset 101, ActiveColorset 102, '
                b'GreyedColorset 103, TitleColorset 104\n'
                b'CopyMenuStyle "Query menu $$[desk.n]" QueryCopy\n'
                b'AddToDecor "Query decor $$[desk.n]" TitleStyle Height 23 LeftJustified\n'
                b'+ ButtonStyle 1 (Solid black)\n')
            writer.echo('appearance-definitions-loaded')
            menu_styles, page_generation = policy_pages('menu-styles', 'menu_styles')
            indexed = {s['name']: s for s in menu_styles}
            assert menu_styles[0]['default'] and menu_styles[0]['name'] == '*'
            menu = indexed['Query menu $[desk.n]']
            assert menu['animation'] and menu['opacity_percent'] == 73
            assert menu['border_width'] == 3 and menu['popup_delay_10ms'] == 25
            assert menu['popdown_delay_10ms'] == 40
            assert menu['colorsets'] == dict(menu=101, active=102, greyed=103, title=104)
            evidence['appearance_menu_initial'] = indexed
            assert indexed['QueryCopy']['opacity_percent'] == 73, indexed['QueryCopy']
            decors, _ = policy_pages('decorations', 'decorations')
            decor = next(s for s in decors if s['name'] == 'Query decor $[desk.n]')
            assert decors[0]['default'] and decor['title_height'] == 23
            assert decor['title_justification'] == 'left' and decor['button_slots'] == 10
            stable_colors = query('colorsets', scope='summary')
            commands = ['Style appearance-probe WindowShadeSteps 7',
                'FocusStyle appearance-probe ClickToFocus',
                f'WindowId {observer.window} WindowStyle WindowShadeSteps 8',
                f'WindowId {observer.window} DestroyWindowStyle',
                'DestroyStyle appearance-probe',
                'TitleStyle Centered', 'AddTitleStyle (Solid black)',
                'ButtonStyle 1 (Solid black)', 'AddButtonStyle 1 (Solid white)',
                'BorderStyle Active (Solid black)', 'ButtonState ActiveDown True',
                'DefaultFont fixed', 'MenuStyle QueryCopy !Animation, Translucent 31',
                'CopyMenuStyle "Query menu $$[desk.n]" QueryCopy',
                'DestroyMenuStyle QueryCopy',
                'DestroyDecor "Query decor $$[desk.n]"']
            changed_commands = []
            for command in commands:
                before = query('appearance')['generation']
                writer.send((command + '\n').encode())
                writer.echo('appearance-change-' + str(len(changed_commands)))
                after = query('appearance')['generation']
                assert before != after, command
                assert query('appearance', expected=True, generation=before) == 'stale_generation'
                assert query('menu-styles', expected=True, offset=1,
                    generation=before) == 'stale_generation'
                assert query('decorations', expected=True, offset=1,
                    generation=before) == 'stale_generation'
                if command.startswith('CopyMenuStyle'):
                    copies, _ = policy_pages('menu-styles', 'menu_styles')
                    copied = next(s for s in copies if s['name'] == 'QueryCopy')
                    assert copied['opacity_percent'] == 73 and copied['animation']
                changed_commands.append(command)
            assert query('colorsets', scope='summary') == stable_colors
            writer.send(b'AddToFunc AppearanceProbe I Style nested-probe WindowShadeSteps 4\n')
            writer.echo('appearance-function-stored')
            before = query('appearance')['generation']
            writer.send(b'AppearanceProbe\n')
            writer.echo('appearance-function-evaluated')
            assert query('appearance')['generation'] != before
            assert query('menu-styles', expected=True, offset=4096,
                generation=query('appearance')['generation']) == 'invalid_offset'
            remaining, _ = policy_pages('decorations', 'decorations')
            assert all(s['name'] != 'Query decor $[desk.n]' for s in remaining)
            assert not marker.exists()
            evidence['appearance'] = dict(menu_styles=menu_styles, decorations=decors,
                changed_commands=changed_commands, nested_function_tracked=True,
                pages_complete=True, stored_literals_not_executed=True,
                independent_colorset_generation=True)

            # Sparse colorsets are core-owned. Queries cannot create a record.
            caps = query('capabilities')
            assert 'colorsets' in caps['resources'] and caps['colorset_page_max'] == 16
            before_color_activity = query('appearance')['generation']
            initial_colors = query('colorsets', scope='summary')
            assert not initial_colors['preview_supported']
            assert query('colorsets', expected=True, target='2147483646') == 'unavailable'
            assert query('colorsets', scope='summary') == initial_colors
            ids = list(range(21, 38)) + [2147483647]
            writer.send((''.join(f'Colorset {n} fg #112233, bg #445566\n' for n in ids)).encode())
            writer.echo('colorsets-published')
            summary = query('colorsets', scope='summary')
            assert summary['generation'] != initial_colors['generation']
            assert summary['total'] == initial_colors['total'] + len(ids)
            generation = summary['generation']
            offset, inventory = 0, []
            while offset is not None:
                page = query('colorsets', offset=offset, limit=2, generation=generation)
                inventory.extend(page['colorsets'])
                offset = page['next_offset']
            assert len(inventory) == summary['total']
            assert set(ids) <= {item['id'] for item in inventory}
            exact = query('colorsets', target='2147483647')
            entry = exact['colorsets'][0]
            assert exact['targeted'] and exact['offset'] is None
            assert entry['id'] == 2147483647 and entry['update_state'] == 'idle'
            assert entry['opacity_percent'] == dict(foreground=100, image=100, icon=100)
            assert exact['pixel_encoding'] == 'colormap-hex'
            class XColor(C.Structure):
                _fields_ = [('pixel', C.c_ulong), ('red', C.c_ushort),
                    ('green', C.c_ushort), ('blue', C.c_ushort),
                    ('flags', C.c_char), ('pad', C.c_char)]
            x.XDefaultColormap.argtypes = [C.c_void_p, C.c_int]
            x.XDefaultColormap.restype = C.c_ulong
            x.XQueryColor.argtypes = [C.c_void_p, C.c_ulong, C.POINTER(XColor)]
            rgb = XColor(pixel=int(entry['pixels']['foreground'], 16))
            assert x.XQueryColor(d, x.XDefaultColormap(d, 0), C.byref(rgb))
            assert (rgb.red, rgb.green, rgb.blue) == (0x1111, 0x2222, 0x3333)
            assert query('colorsets', scope='summary') == summary
            assert query('colorsets', expected=True, offset=summary['total'] + 1,
                generation=generation) == 'invalid_offset'
            writer.send(b'Colorset 21 fg #abcdef, fgAlpha 70\n')
            writer.echo('colorset-change-processed')
            assert query('colorsets', expected=True, scope='summary',
                generation=generation) == 'stale_generation'
            assert query('colorsets', expected=True, offset=2,
                generation=generation) == 'stale_generation'
            assert query('colorsets', expected=True, target='21',
                generation=generation) == 'stale_generation'
            updated = query('colorsets', target='21')
            assert updated['colorsets'][0]['opacity_percent']['foreground'] == 70
            # A failed asynchronous replacement keeps published colors visible.
            writer.send(f'Colorset 21 Pixmap {runtime.root}/missing.xpm\n'.encode())
            writer.echo('colorset-image-requested')
            def refused():
                return query('colorsets', target='21')['colorsets'][0]['update_state'] == 'refused'
            wait_for_predicate('colorset replacement refused', refused,
                time.monotonic() + 10, process=core)
            failed = query('colorsets', target='21')
            assert failed['colorsets'][0]['pixels'] == updated['colorsets'][0]['pixels']
            assert failed['generation'] != updated['generation']
            writer.send(b'Colorset 21 Plain\n')
            writer.echo('colorset-recovered')
            recovered = query('colorsets', target='21')
            assert recovered['colorsets'][0]['update_state'] == 'idle'
            assert recovered['generation'] != failed['generation']
            assert query('appearance')['generation'] != before_color_activity
            evidence['colorsets'] = dict(summary=summary, exact=exact,
                failed=failed, recovered=recovered, x_color_rgb=[rgb.red, rgb.green, rgb.blue],
                pages_complete=True, missing_read_did_not_allocate=True,
                stale_pages_and_targets_refused=True)

            # Core appearance covers definitions, not every window lifetime.
            # Empty automatic retirement must not cancel a valid preview; an
            # actual WindowStyle removal must still invalidate it.
            explicit = query('appearance')['generation']
            writer.send(b'DestroyStyle missing-retirement-probe\n')
            writer.echo('explicit-empty-style-destruction')
            assert query('appearance')['generation'] != explicit
            ordinary = query('appearance')['generation']
            retired_window = observer.window
            observer.destroy()
            wait_for_predicate('ordinary window retired',
                lambda: retired_window not in prop('_NET_CLIENT_LIST', 33),
                time.monotonic() + 5, process=core)
            assert query('appearance')['generation'] == ordinary
            observer.create()
            wait_for_predicate('styled retirement observer managed',
                lambda: observer.parent(observer.window) != observer.root,
                time.monotonic() + 5, process=core)
            writer.send(f'WindowId {observer.window} WindowStyle BorderWidth 9\n'.encode())
            writer.echo('window-style-retirement-ready')
            styled = query('appearance')['generation']
            retired_window = observer.window
            observer.destroy()
            wait_for_predicate('styled window retired',
                lambda: retired_window not in prop('_NET_CLIENT_LIST', 33),
                time.monotonic() + 5, process=core)
            assert query('appearance')['generation'] != styled
            evidence['window_retirement_freshness'] = dict(
                empty_cleanup_preserved=True, definition_removal_invalidated=True)

            os.kill(core.process.pid, signal.SIGSTOP)
            stopped = True
            rpc.send(frame('cancelled-core', resource='topology') +
                     frame('cancel-core', 'cancel', target='cancelled-core'))
            assert reply(rpc, 'cancelled-core', 'error') == 'wait_cancelled'
            assert reply(rpc, 'cancel-core')['state'] == 'wait-cancelled'
            rpc.send(frame('cancelled-core', resource='workspaces'))
            os.kill(core.process.pid, signal.SIGCONT)
            stopped = False
            assert reply(rpc, 'cancelled-core')['scope'] == 'core-monitors'
            assert query('topology') == topology
            evidence['readonly_cancel_late_result_isolation'] = True
            assert core.poll() is None and not marker.exists()
            evidence['status'] = 'passed'
        finally:
            if stopped:
                os.kill(core.process.pid, signal.SIGCONT)
            for c in connections:
                c.close()
            if observer:
                observer.close()
            if core:
                core.close(time.monotonic() + 5)
            for log in runtime.root.glob('*.log'):
                shutil.copy2(log, args.artifacts_dir / log.name)
            (args.artifacts_dir / 'evidence.json').write_text(json.dumps(evidence, indent=2) + '\n')
    print(json.dumps(evidence))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--fvwm', type=Path, required=True)
    parser.add_argument('--mfl', type=Path, required=True)
    parser.add_argument('--display')
    parser.add_argument('--artifacts-dir', type=Path, required=True)
    run(parser.parse_args())
