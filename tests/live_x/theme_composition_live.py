#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
"""Independent wallpaper choices through the existing desktop owner and IPC."""
import argparse
import ctypes as C
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import time

from desktop_live import connect_client
from live_x_harness import HarnessRuntime, start_display_server, wait_for_predicate
from mfl_request_live import frame, reply
from mfl_stream_live import Connection
from session_x_errors import SessionXErrors


def run(args, profile):
    out = args.artifacts_dir / profile
    out.mkdir(parents=True, exist_ok=False)
    evidence = {'profile': profile, 'settings': [], 'paint_observations': {}}
    source, build = Path(__file__).resolve().parents[2], args.build.resolve()
    with HarnessRuntime(prefix='fvwm-composition-') as runtime:
        control = rpc = client = errors = None
        try:
            _, display = start_display_server(runtime, 'Xvfb',
                ['Xvfb', '-screen', '0', '1440x1000x24', '-noreset'],
                'xserver.log', time.monotonic() + 10)
            user, modules = runtime.root / 'user', runtime.root / 'modules'
            user.mkdir(mode=0o700)
            modules.mkdir()
            for directory in (build / 'modules').iterdir():
                executable = directory / directory.name
                if executable.is_file():
                    (modules / directory.name).symlink_to(executable)
            (modules / 'FvwmDesktop').symlink_to(build / 'bin/FvwmPrompt/FvwmDesktop')
            root = source / 'default-config'
            body = (root / 'config').read_text().replace(
                '$[FVWM_DATADIR]/default-config', str(root))
            if profile == 'classic-large':
                body = body.replace('Sans:size=10:antialias=True',
                    'DejaVu Sans Mono:size=18:antialias=True')
            config = runtime.root / 'config'
            config.write_text('ModulePath ' + str(modules) + '\nRead ' +
                str(source / 'fvwm/ConfigFvwmDefaults') + '\n' + body +
                '\nAddToFunc StartFunction I Module FvwmMFL\n')
            (out / 'configuration.fvwm').write_bytes(config.read_bytes())
            env = runtime.environment({'DISPLAY': display, 'HOME': str(runtime.root),
                'FVWM_USERDIR': str(user), 'FVWM_DESKTOP_ROOT': str(root),
                'FVWMMFL_SOCKET_PATH': str(runtime.root),
                'XDG_DATA_HOME': str(runtime.root / 'data'),
                'XDG_DATA_DIRS': str(runtime.root / 'absent')})
            core = runtime.start('core', [str(build / 'fvwm3'), '-d', display,
                '-f', str(config)], 'core.log', env)
            endpoint = runtime.root / ('fvwm_mfl_' + display.replace(':', '_') + '.sock')
            wait_for_predicate('composition MFL', endpoint.exists,
                time.monotonic() + 10, process=core)
            control, rpc = Connection(endpoint), Connection(endpoint)
            for connection in (control, rpc):
                connection.socket.settimeout(15)
                connection.record()
            control.send(b'set echo\n')
            serial = 0

            def request(method='query', **fields):
                nonlocal serial
                serial += 1
                token = 'composition-' + str(serial)
                rpc.send(frame(token, method=method, resource='settings', **fields))
                return reply(rpc, token)

            deadline = time.monotonic() + 15
            while True:
                serial += 1
                token = 'ready-' + str(serial)
                rpc.send(frame(token, resource='settings', scope='all'))
                message = rpc.record()['reply']
                if message['status'] == 'ok':
                    snapshot = message['result']
                    if snapshot['settings']['palette']['application'] == 'submitted-to-fvwm':
                        break
                assert time.monotonic() < deadline, message
                time.sleep(.02)

            def barrier():
                nonlocal serial
                serial += 1
                token = 'composition-barrier-' + str(serial)
                control.send(('SendToModule FvwmDesktop barrier ' + token + '\n').encode())
                control.expect_echo(token)

            def change(values):
                nonlocal snapshot
                snapshot = request('action', operation='set', provider=snapshot['provider'],
                    generation=snapshot['generation'], values=values)
                barrier()
                evidence['settings'].append({'values': values, 'result': snapshot})
                return snapshot

            palette, mode = ('dune', 'light') if profile == 'dune-light' else ('slate', 'dark')
            change({'palette': palette, 'appearance': mode, 'transparency': False})
            client = connect_client(display)
            errors = SessionXErrors(client, evidence).install()
            x, d = client.x, client.display
            x.XGetImage.argtypes = [C.c_void_p, C.c_ulong, C.c_int, C.c_int,
                C.c_uint, C.c_uint, C.c_ulong, C.c_int]
            x.XGetImage.restype = C.c_void_p
            x.XGetPixel.argtypes = [C.c_void_p, C.c_int, C.c_int]
            x.XGetPixel.restype = C.c_ulong
            x.XDestroyImage.argtypes = [C.c_void_p]
            x.XFetchName.argtypes = [C.c_void_p, C.c_ulong, C.POINTER(C.c_void_p)]

            def pixel(px, py):
                image = x.XGetImage(d, client.root, px, py, 1, 1, C.c_ulong(-1).value, 2)
                assert image
                try:
                    return x.XGetPixel(image, 0, 0)
                finally:
                    x.XDestroyImage(image)

            def wallpaper():
                # Require a stable observed root sample, not only command submission.
                previous, matches = None, 0
                def stable():
                    nonlocal previous, matches
                    value = pixel(10, 950)
                    matches = matches + 1 if value == previous and value != 0 else 0
                    previous = value
                    return value if matches >= 4 else None
                return wait_for_predicate('stable wallpaper sample', stable,
                    time.monotonic() + 10, process=core)

            palettes = json.loads((root / 'palettes.json').read_text())
            def bar_color(name, appearance):
                expected = int(palettes[name][appearance]['surface'][1:], 16)
                wait_for_predicate('native bar palette', lambda: pixel(5, 5) == expected,
                    time.monotonic() + 10, process=core)
                return expected

            state = user / 'desktop-state.json'
            initial = wallpaper()
            base_bar = bar_color(palette, mode)
            change({'wallpaper_palette': 'grove', 'wallpaper_appearance': 'light'})
            wait_for_predicate('independent wallpaper pixels',
                lambda: pixel(10, 950) != initial, time.monotonic() + 10, process=core)
            independent = wallpaper()
            assert pixel(5, 5) == base_bar
            assert json.loads(state.read_text())['wallpaper'] == {'palette': 'grove', 'appearance': 'light'}
            opposite = 'ink' if palette != 'ink' else 'slate'
            change({'palette': opposite})
            bar_color(opposite, mode)
            assert wallpaper() == independent
            change({'keep_wallpaper': True, 'wallpaper_palette': 'dune',
                'wallpaper_appearance': 'dark'})
            held = snapshot['settings']['wallpaper_palette']
            assert held['resolved'] == 'grove' and held['resolution'] == 'retained'
            assert held['runtime_constraint'] == 'wallpaper-retained'
            assert wallpaper() == independent
            snapshot = request('action', operation='reload', provider=snapshot['provider'],
                generation=snapshot['generation'])
            barrier()
            assert wallpaper() == independent
            change({'keep_wallpaper': False})
            wait_for_predicate('released wallpaper choice',
                lambda: pixel(10, 950) != independent, time.monotonic() + 10, process=core)
            released = wallpaper()
            snapshot = request('action', operation='reset', provider=snapshot['provider'],
                generation=snapshot['generation'], scope='group:wallpaper')
            barrier()
            assert set(snapshot['settings']) == {'wallpaper_palette', 'wallpaper_appearance'}
            assert json.loads(state.read_text())['schema_version'] == 3
            assert json.loads(state.read_text())['wallpaper'] == {'palette': opposite, 'appearance': mode}
            snapshot = request(scope='all')
            change({'palette': palette, 'wallpaper_palette': 'grove', 'wallpaper_appearance': 'light'})
            bar_color(palette, mode)
            wallpaper()
            evidence['paint_observations'] = {'initial': initial, 'independent': independent,
                'released': released, 'base_bar': base_bar,
                'wallpaper_only_preserves_bar': True, 'ui_change_preserves_explicit_wallpaper': True}
            control.send(b'SendToModule FvwmDesktop edit-settings group:wallpaper\n')
            barrier()

            def find_form(window):
                name = C.c_void_p()
                found = errors.read(window, 20,
                    lambda: x.XFetchName(d, window, C.byref(name)))
                if name.value:
                    try:
                        if found and C.string_at(name) == b'Desktop settings':
                            return window
                    finally:
                        x.XFree(name)
                root_, parent, count = C.c_ulong(), C.c_ulong(), C.c_uint()
                children = C.POINTER(C.c_ulong)()
                try:
                    ok = errors.read(window, 15, lambda: x.XQueryTree(d, window,
                        C.byref(root_), C.byref(parent), C.byref(children), C.byref(count)))
                    if ok:
                        for child in children[:count.value]:
                            if (result := find_form(child)):
                                return result
                finally:
                    if children:
                        x.XFree(children)
                return None

            form = wait_for_predicate('composition editor', lambda: find_form(client.root),
                time.monotonic() + 10, process=core)
            time.sleep(.1)
            result = subprocess.run([str(build / 'bin/fvwm-desktop-capture'), str(out)],
                env=env, capture_output=True, text=True, timeout=35)
            assert result.returncode == 0, result.stderr
            Path(result.stdout.strip().removeprefix('Screenshot saved: ')).replace(out / 'composition-editor.png')
            control.send(('WindowId ' + str(form) + ' Close\n').encode())
            barrier()
            before = state.read_bytes()
            control.send(b'*FvwmDesktop: wallpaper_palette ink\n')
            # Module configuration is acquired at startup, as documented; a
            # core command barrier does not make a running helper reread it.
            old_provider = snapshot['provider']
            control.send(b'KillModule FvwmDesktop\n'
                b'ModuleSynchronous Timeout 15 FvwmDesktop\n')
            deadline = time.monotonic() + 15
            while True:
                serial += 1
                token = 'override-ready-' + str(serial)
                rpc.send(frame(token, resource='settings', scope='group:wallpaper'))
                response = rpc.record()['reply']
                if response['status'] == 'ok':
                    observed = response['result']
                    if (observed['provider'] != old_provider and
                            observed['settings']['wallpaper_palette']['source'] ==
                            'module-configuration'):
                        break
                assert time.monotonic() < deadline, response
                time.sleep(.02)
            assert observed['settings']['wallpaper_palette']['source'] == 'module-configuration'
            assert observed['settings']['wallpaper_palette']['effective'] == 'ink'
            assert state.read_bytes() == before
            evidence['module_override_no_write'] = observed
            snapshot = request(scope='all')
            change({'keep_wallpaper': True})
            control.send(b'SendToModule FvwmDesktop edit-settings group:wallpaper\n')
            barrier()
            wait_for_predicate('retained composition editor', lambda: find_form(client.root),
                time.monotonic() + 10, process=core)
            time.sleep(.1)
            result = subprocess.run([str(build / 'bin/fvwm-desktop-capture'), str(out)],
                env=env, capture_output=True, text=True, timeout=35)
            assert result.returncode == 0, result.stderr
            Path(result.stdout.strip().removeprefix('Screenshot saved: ')).replace(
                out / 'composition-retained.png')
            evidence['state'] = json.loads(state.read_text())
            evidence['configuration_sha256'] = hashlib.sha256(config.read_bytes()).hexdigest()
            evidence['palettes_sha256'] = hashlib.sha256((root/'palettes.json').read_bytes()).hexdigest()
        finally:
            for connection in (control, rpc):
                if connection:
                    connection.close()
            if errors:
                errors.close_display()
            elif client:
                client.close()
            for log in runtime.root.glob('*.log'):
                shutil.copy2(log, out / log.name)
            (out / 'evidence.json').write_text(json.dumps(evidence, indent=2) + '\n')
    assert not evidence['observer_unexpected_x_errors'], evidence


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--artifacts-dir', type=Path, required=True)
    parser.add_argument('--profiles', nargs='+', default=['slate-dark', 'dune-light', 'classic-large'],
        choices=['slate-dark', 'dune-light', 'classic-large'])
    options = parser.parse_args()
    for selected in options.profiles:
        run(options, selected)
