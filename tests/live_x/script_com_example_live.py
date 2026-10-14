#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or any later version.
"""Click the paired Script example and observe its actual widget titles."""
import argparse
import ctypes as C
import json
import re
from pathlib import Path
import shlex
import shutil
import time

from geometry_transition_live import Client, Pointer
from live_x_harness import HarnessRuntime, start_display_server, wait_for_predicate
from mfl_stream_live import Connection


def run(args):
    args.artifacts_dir.mkdir(parents=True, exist_ok=False)
    evidence = {}
    with HarnessRuntime(prefix='script-com-live-') as runtime:
        client = connection = None
        try:
            display = args.display
            if display is None:
                _, display = start_display_server(runtime, 'Xvfb',
                    ['Xvfb', '-screen', '0', '800x600x24', '-noreset'],
                    'xserver.log', time.monotonic() + 10)
            report = runtime.root / 'report'
            text = args.example.read_text()
            original = text
            # A DESTDIR stage has a different location from its configured
            # final prefix. Relocate only that literal shell argument.
            launch = re.search(r'    Set \$ComHelper = \{(.*)\}', text)
            assert launch, text
            words = shlex.split(launch[1])
            words[-1] = str(args.helper)
            text = text.replace(launch[0], '    Set $ComHelper = {'+
                shlex.join(words)+'}')
            text = text.replace('    Set $pid = (GetPid)',
                '    Set $Report={'+str(report)+'}\n    Set $pid = (GetPid)\n    WriteToFile $Report {pid:} $pid')
            for widget in [10, 21, 31, 41, 42, 43]:
                pattern = r'(\s*ChangeTitle '+str(widget)+r' [^\n]+)'
                text, count = re.subn(pattern, lambda match: match[1]+
                    '\n    WriteToFile $Report {'+str(widget)+':} (GetTitle '+str(widget)+')', text)
                assert count == 1
            example = runtime.root / 'ComExample'
            example.write_text(text)
            (args.artifacts_dir / 'installed-example').write_text(original)
            shutil.copy2(example, args.artifacts_dir / 'instrumented-example')
            config = runtime.root / 'config'
            config.write_text('DefaultFont fixed\nStyle * TileCascadePlacement\n'
                f'ModulePath {args.mfl.parent}:{args.script.parent}\n'
                'AddToFunc StartFunction I Module FvwmMFL\n')
            env = runtime.environment({'DISPLAY': display,
                'FVWMMFL_SOCKET_PATH': str(runtime.root), 'FVWM_USERDIR': str(runtime.root)})
            core = runtime.start('core', [str(args.fvwm), '-d', display,
                '-f', str(config)], 'core.log', env)
            endpoint = runtime.root / ('fvwm_mfl_'+display.replace(':', '_')+'.sock')
            wait_for_predicate('MFL', endpoint.exists, time.monotonic()+10, process=core)
            connection = Connection(endpoint)
            connection.record()
            connection.send(b'set echo\n')
            connection.echo('example-ready')
            connection.send(('Module FvwmScript '+str(example)+'\n').encode())

            def lines():
                return report.read_text() if report.exists() else ''

            wait_for_predicate('startup reply', lambda: 'is up\n' in lines(),
                time.monotonic()+15, process=core)
            client = Client(display)
            pointer = Pointer(client)
            x, d = client.x, client.display
            x.XFetchName.argtypes = [C.c_void_p, C.c_ulong, C.POINTER(C.c_void_p)]
            x.XTranslateCoordinates.argtypes = [C.c_void_p, C.c_ulong, C.c_ulong,
                C.c_int, C.c_int, C.POINTER(C.c_int), C.POINTER(C.c_int), C.POINTER(C.c_ulong)]

            def find(parent):
                root, above, count = C.c_ulong(), C.c_ulong(), C.c_uint()
                children = C.POINTER(C.c_ulong)()
                assert x.XQueryTree(d, parent, C.byref(root), C.byref(above),
                    C.byref(children), C.byref(count))
                try:
                    for window in children[:count.value]:
                        name = C.c_void_p()
                        if x.XFetchName(d, window, C.byref(name)) and name.value:
                            try:
                                if C.string_at(name) == b'FvwmScript-ComExample':
                                    return window
                            finally:
                                x.XFree(name)
                        result = find(window)
                        if result:
                            return result
                finally:
                    if children: x.XFree(children)
                return None

            window = wait_for_predicate('Script window', lambda: find(client.root),
                time.monotonic()+10, process=core)
            for cx, cy, expected in [(90, 70, '21:Get 1 count messages\n'),
                    (90, 70, '21:Get 2 count messages\n'),
                    (90, 110, '31:Returned value: 0\n'), (90, 150, '43:An other Line (')]:
                dx, dy, child = C.c_int(), C.c_int(), C.c_ulong()
                assert x.XTranslateCoordinates(d, window, client.root, cx, cy,
                    C.byref(dx), C.byref(dy), C.byref(child))
                pointer.motion(dx.value, dy.value)
                pointer.button(True)
                pointer.button(False)
                wait_for_predicate(expected, lambda: expected in lines(),
                    time.monotonic()+10, process=core)
            for widget, pattern in [(41, r'line1 \((\d+)\)'),
                    (42, r'Second Line \((\d+)\)'), (43, r'An other Line \((\d+)\)')]:
                match = re.search(str(widget)+':'+pattern+'\n', lines())
                assert match and 0 <= int(match[1]) < 20, lines()
            dx, dy, child = C.c_int(), C.c_int(), C.c_ulong()
            assert x.XTranslateCoordinates(d, window, client.root, 290, 260,
                C.byref(dx), C.byref(dy), C.byref(child))
            pointer.motion(dx.value, dy.value)
            pointer.button(True); pointer.button(False)
            pid = re.search(r'pid:(\d+)\n', lines())[1]
            wait_for_predicate('helper removes its endpoints', lambda:
                not list(runtime.root.glob('.tmp-com-*-ComExample-'+pid)),
                time.monotonic()+10, process=core)
            # Avoid querying the retired window; inventory only its surviving
            # parents after the protocol's exit request has been delivered.
            connection.echo('example-finished')
            assert core.poll() is None
            evidence = {'real_button_requests': 5, 'count_order': [1, 2],
                'unknown_reply': '0', 'three_parsed_widget_titles': True,
                'startup': 'is up', 'endpoint_cleanup': True,
                'core_alive': True, 'consumer_launch_relocated': True,
                'widget_observations_injected': True}
            shutil.copy2(report, args.artifacts_dir / 'report')
        finally:
            if connection: connection.close()
            if client: client.close()
            for log in runtime.root.glob('*.log'):
                shutil.copy2(log, args.artifacts_dir / log.name)
            (args.artifacts_dir / 'results.json').write_text(json.dumps(evidence, indent=2)+'\n')
    print(json.dumps(evidence, sort_keys=True))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ['fvwm', 'mfl', 'script', 'example', 'helper', 'artifacts-dir']:
        parser.add_argument('--'+name, type=Path, required=True)
    parser.add_argument('--display')
    args = parser.parse_args()
    for name, value in vars(args).items():
        if isinstance(value, Path): setattr(args, name, value.resolve())
    run(args)
