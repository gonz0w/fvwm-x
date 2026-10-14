#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or any later version.
"""Load installed xlock menus through PipeRead and map/dismiss them in the WM."""
import argparse
import ctypes as C
import json
import os
from pathlib import Path
import shlex
import shutil
import sys
import time

from colorset_live import WindowAttributes
from geometry_transition_live import Client, Pointer
from live_x_harness import HarnessRuntime, start_display_server, wait_for_predicate
from mfl_stream_live import Connection


def run(args):
    args.artifacts_dir.mkdir(parents=True, exist_ok=False)
    evidence = {}
    with HarnessRuntime(prefix='xm-live-') as runtime:
        client = pointer = connection = None
        pressed = False
        try:
            display = args.display
            if display is None:
                _, display = start_display_server(runtime, 'Xvfb',
                    ['Xvfb', '-screen', '0', '800x600x24', '-noreset'],
                    'xserver.log', time.monotonic() + 10)
            fake = runtime.root / 'xlock'
            fake.write_text('#!' + sys.executable + '\n'
                'print("where mode is one of:")\n'
                'print("  alpha Shows Alpha pattern")\n'
                'print("  blank Shows Blank screen")\n'
                'print("  random Random mode")\n')
            fake.chmod(0o755)
            config = runtime.root / 'config'
            config.write_text('DefaultFont fixed\nMenuStyle * Font fixed\n'
                'ModulePath ' + str(args.mfl.parent) + '\n'
                'AddToFunc StartFunction I Module FvwmMFL\n'
                'Mouse 1 R N Popup XLockMenu\n')
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

            def mapped():
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
                return result

            for label, extra in [('default', ''), ('special', ' --special-first --item="%n: %D" -- -nolock')]:
                command = shlex.quote(str(args.xlock)) + extra
                connection.send(('PipeRead ' + json.dumps(command) + '\n').encode())
                connection.echo('menu-loaded-' + label)
                pointer.motion(200, 200)
                pointer.button(True)
                pressed = True
                geometry = wait_for_predicate('generated menu maps', mapped,
                    time.monotonic() + 5, process=core)
                assert len(geometry) == 1 and geometry[0][2] > 50 and geometry[0][3] > 40
                code = x.XKeysymToKeycode(d, 0xff1b)
                assert code
                for down in (True, False):
                    assert pointer.x.XTestFakeKeyEvent(d, code, down, 0)
                x.XSync(d, 0)
                pointer.button(False)
                pressed = False
                wait_for_predicate('generated menu dismissed', lambda: not mapped(),
                    time.monotonic() + 5, process=core)
                connection.echo('menu-dismissed-' + label)
                evidence[label] = {'mapped_geometry': geometry, 'dismissed': True}
            assert core.poll() is None
            evidence['generated_actions_executed'] = False
        finally:
            if pressed and pointer:
                pointer.button(False)
            if connection:
                connection.close()
            if client:
                client.close()
            for log in runtime.root.glob('*.log'):
                shutil.copy2(log, args.artifacts_dir / log.name)
            (args.artifacts_dir / 'results.json').write_text(json.dumps(evidence, indent=2) + '\n')
    print(json.dumps(evidence, sort_keys=True))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('fvwm', 'mfl', 'xlock', 'artifacts-dir'):
        parser.add_argument('--' + name, type=Path, required=True)
    parser.add_argument('--display')
    options = parser.parse_args()
    for name, value in vars(options).items():
        if isinstance(value, Path):
            setattr(options, name, value.resolve())
    run(options)
