#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or any later version.
"""Exercise directory submenus and remembered navigation through real FVWM commands."""
import argparse
import ctypes as C
import json
import os
from pathlib import Path
import shlex
import shutil
import sys
import subprocess
import time

from colorset_live import WindowAttributes
from geometry_transition_live import Client, Pointer
from live_x_harness import HarnessRuntime, start_display_server, wait_for_predicate
from mfl_stream_live import Connection


def run(args):
    args.artifacts_dir.mkdir(parents=True, exist_ok=False)
    evidence = {}
    with HarnessRuntime(prefix='dm-live-') as runtime:
        client = pointer = connection = None
        pressed = False
        try:
            display = args.display
            if display is None:
                _, display = start_display_server(runtime, 'Xvfb',
                    ['Xvfb', '-screen', '0', '800x600x24', '-noreset'],
                    'xserver.log', time.monotonic() + 10)
            directory = runtime.root / 'listing'
            directory.mkdir()
            (directory / 'folder').mkdir()
            (directory / 'folder' / 'file').write_text('fixture')
            quoted = directory / "literal ' quote \" dollar$ backtick`"
            quoted.mkdir()
            (quoted / 'child').write_text('fixture')
            config = runtime.root / 'config'
            config.write_text('DefaultFont fixed\nMenuStyle * Font fixed\n'
                'ModulePath ' + str(args.mfl.parent) + '\n'
                'AddToFunc StartFunction I Module FvwmMFL\n'
                'AddToFunc FuncFvwmMenuDirectory\n'
                '+ I PipeRead "' + str(args.directory) +
                ' --dir \'$0\' --command-title Nop --command-file Nop"\n')
            env = runtime.environment({'FVWMMFL_SOCKET_PATH': str(runtime.root),
                'FVWM_USERDIR': str(runtime.root), 'DISPLAY': display,
                'PATH': str(runtime.root) + os.pathsep + os.environ['PATH']})
            core = runtime.start('core', [str(args.fvwm), '-d', display,
                '-f', str(config)], 'core.log', env)
            endpoint = runtime.root / ('fvwm_mfl_' + display.replace(':', '_') + '.sock')
            wait_for_predicate('MFL endpoint', endpoint.exists,
                time.monotonic() + 10, process=core)
            connection = Connection(endpoint)
            connection.record()
            connection.send(b'set echo\n')
            client = Client(display)
            pointer = Pointer(client)
            x, d = client.x, client.display
            x.XGetWindowAttributes.argtypes = [C.c_void_p, C.c_ulong, C.POINTER(WindowAttributes)]
            x.XKeysymToKeycode.argtypes = [C.c_void_p, C.c_ulong]
            x.XKeysymToKeycode.restype = C.c_uint
            pointer.x.XTestFakeKeyEvent.argtypes = [C.c_void_p, C.c_uint, C.c_int, C.c_ulong]

            x.XGrabServer.argtypes = [C.c_void_p]
            x.XUngrabServer.argtypes = [C.c_void_p]

            def mapped():
                # Keep this brief inventory coherent while menus are destroyed.
                x.XGrabServer(d)
                root, parent, count = C.c_ulong(), C.c_ulong(), C.c_uint()
                children = C.POINTER(C.c_ulong)()
                assert x.XQueryTree(d, client.root, C.byref(root), C.byref(parent),
                    C.byref(children), C.byref(count))
                result = []
                try:
                    for window in children[:count.value]:
                        attrs = WindowAttributes()
                        assert x.XGetWindowAttributes(d, window, C.byref(attrs))
                        if attrs.map_state == 2 and attrs.save_under and attrs.width > 20:
                            result.append(client.geometry(window))
                finally:
                    if children:
                        x.XFree(children)
                    x.XUngrabServer(d)
                    x.XSync(d, 0)
                return result

            def dismiss():
                code = x.XKeysymToKeycode(d, 0xff1b)
                assert code
                for _ in range(2):
                    for down in (True, False):
                        assert pointer.x.XTestFakeKeyEvent(d, code, down, 0)
                x.XSync(d, 0)
                wait_for_predicate('generated menu dismissed', lambda: not mapped(),
                    time.monotonic() + 5, process=core)
                connection.echo('menu-dismissed')

            def load(extra):
                argv = [str(args.directory), '--dir', str(directory),
                    '--title', '', '--command-title', 'Nop', '--command-file', 'Nop'] + extra
                result = subprocess.run(argv, env=env, capture_output=True, timeout=9)
                assert result.returncode == 0, result
                command = ' '.join(shlex.quote(v) for v in argv)
                connection.send(('PipeRead ' + json.dumps(command) + '\n').encode())
                connection.echo('menu-loaded')
                return result.stdout.decode().splitlines(), command

            pointer.motion(200, 200)
            lines, _ = load([])
            connection.send(('Menu ' + json.dumps(str(directory)) + '\n').encode())
            geometry = wait_for_predicate('directory menu maps', mapped,
                time.monotonic() + 5, process=core)
            for symbol in (0xff50, 0xff53):  # Home, Right: enter first folder.
                code = x.XKeysymToKeycode(d, symbol)
                assert code
                for down in (True, False):
                    assert pointer.x.XTestFakeKeyEvent(d, code, down, 0)
            x.XSync(d, 0)
            child = wait_for_predicate('missing submenu callback maps',
                lambda: mapped() if len(mapped()) == 2 else None,
                time.monotonic() + 5, process=core)
            dismiss()
            evidence['normal'] = {'mapped_geometry': geometry,
                'submenu_geometry': child, 'dismissed': True}
            lines, command = load(['--reuse', '--name=Browser', '--title=%p'])
            connection.send(('AddToMenu Browser\n+ DynamicPopupAction PipeRead ' +
                json.dumps(command) + '\n').encode())
            connection.echo('reuse-callback-installed')
            connection.send(b'Menu Browser\n')
            before = wait_for_predicate('reuse parent maps', mapped,
                time.monotonic() + 5, process=core)
            dismiss()
            line = next(line for line in lines if '--fvwm-remember' in line and 'literal' in line)
            action = line[line.index(' PipeRead ') + 1:]
            evidence['navigation_action'] = action
            connection.send((action + '\n').encode())
            reused = wait_for_predicate('remembered navigation menu maps', mapped,
                time.monotonic() + 5, process=core)
            dismiss()
            state = runtime.root / '.fvwm-menu-directory.dir'
            assert state.read_text() == str(quoted) + '\n', state.read_text()
            assert state.stat().st_mode & 0o777 == 0o600
            assert len(reused) == 1 and reused[0][3] < before[0][3], (before, reused)
            evidence['reuse'] = {'mapped_geometry': reused,
                'literal_path_preserved': True, 'private_state': True,
                'dynamic_reload': True, 'parent_geometry': before, 'dismissed': True}
            assert core.poll() is None
            evidence['actions_executed'] = ['submenu callback', 'navigation helper']
            evidence['file_or_shell_actions_executed'] = False
        finally:
            if pressed and pointer:
                pointer.button(False)
            if connection:
                connection.close()
            if client:
                client.close()
            state = runtime.root / '.fvwm-menu-directory.dir'
            if state.exists():
                evidence['state_before_cleanup'] = state.read_text()
            for log in runtime.root.glob('*.log'):
                shutil.copy2(log, args.artifacts_dir / log.name)
            (args.artifacts_dir / 'results.json').write_text(json.dumps(evidence, indent=2) + '\n')
    print(json.dumps(evidence, sort_keys=True))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('fvwm', 'mfl', 'directory', 'artifacts-dir'):
        parser.add_argument('--' + name, type=Path, required=True)
    parser.add_argument('--display')
    options = parser.parse_args()
    for name, value in vars(options).items():
        if isinstance(value, Path):
            setattr(options, name, value.resolve())
    run(options)
