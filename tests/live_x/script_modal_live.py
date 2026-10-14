#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.

"""Check real Script modal service, mutation cancellation and held termination."""
import argparse
import ctypes as C
import json
import os
from pathlib import Path
import shutil
import signal
import sys
import time

from geometry_transition_live import Client
from live_x_harness import HarnessRuntime, start_display_server, wait_for_predicate
from mfl_stream_live import Connection
from selection_live import Button, Event, Key
from script_text_list_live import XError


def run(options):
    options.artifacts_dir.mkdir(parents=True, exist_ok=True)
    evidence = {}
    with HarnessRuntime(prefix='fvwm-script-modal-') as runtime:
        client = connection = None
        try:
            display = options.display
            if display is None:
                _, display = start_display_server(runtime, 'Xvfb',
                    ['Xvfb', '-screen', '0', '800x600x24', '-noreset'],
                    'xserver.log', time.monotonic()+15)
            modules = runtime.root/'modules'
            modules.mkdir()
            wrapper = modules/'FvwmScript'
            wrapper.write_text('#!'+sys.executable+'\nimport os,sys\nheld=[]\n'
                'while not held or held[-1]<80:\n'
                " fd=os.open('/dev/null',os.O_RDONLY)\n"
                ' os.set_inheritable(fd,True)\n held.append(fd)\n'
                'os.execv('+repr(str(options.script))+', ['+
                repr(str(options.script))+', *sys.argv[1:]])\n')
            wrapper.chmod(0o755)
            config = runtime.root/'config'
            config.write_text(f'ModulePath {modules}:{options.mfl.parent}\n'
                'Style * TileCascadePlacement\n'
                'AddToFunc StartFunction I Module FvwmMFL\n')
            core = runtime.start('core', [str(options.fvwm), '-d', display,
                '-f', str(config)], 'core.log', runtime.environment({
                    'FVWMMFL_SOCKET_PATH':str(runtime.root)}))
            endpoint = runtime.root/f"fvwm_mfl_{display.replace(':','_')}.sock"
            wait_for_predicate('modal endpoint', endpoint.exists,
                time.monotonic()+15, process=core)
            connection = Connection(endpoint)
            connection.record()
            connection.send(b'set echo\n')
            connection.echo('modal-ready')
            client = Client(display)
            x, d = client.x, client.display
            x.XFetchName.argtypes = [C.c_void_p, C.c_ulong, C.POINTER(C.c_void_p)]
            x.XSendEvent.argtypes = [C.c_void_p, C.c_ulong, C.c_int,
                C.c_long, C.POINTER(Event)]
            x.XWarpPointer.argtypes = [C.c_void_p, C.c_ulong, C.c_ulong,
                C.c_int, C.c_int, C.c_uint, C.c_uint, C.c_int, C.c_int]
            x.XKeysymToKeycode.argtypes = [C.c_void_p, C.c_ulong]
            x.XKeysymToKeycode.restype = C.c_uint

            scanning = False
            unexpected_errors = []
            error_callback = C.CFUNCTYPE(C.c_int, C.c_void_p, C.POINTER(XError))

            @error_callback
            def x_error(display, pointer):
                error = pointer.contents
                if not (scanning and error.error_code == 3 and
                        error.request_code in (15, 20)):
                    unexpected_errors.append((error.error_code, error.request_code,
                        error.minor_code, error.resourceid))
                return 0

            x.XSetErrorHandler.argtypes = [error_callback]
            x.XSetErrorHandler.restype = C.c_void_p
            previous_error_handler = x.XSetErrorHandler(x_error)

            def children(window):
                root, parent = C.c_ulong(), C.c_ulong()
                items, count = C.POINTER(C.c_ulong)(), C.c_uint()
                valid = x.XQueryTree(d, window, C.byref(root), C.byref(parent),
                    C.byref(items), C.byref(count))
                assert valid or scanning, window
                assert count.value <= 4096
                result = [items[i] for i in range(count.value)]
                if items:
                    x.XFree(items)
                return result

            def named_impl(title):
                pending = [(client.root, 0)]
                while pending:
                    window, depth = pending.pop()
                    value = C.c_void_p()
                    if x.XFetchName(d, window, C.byref(value)) and value:
                        text = C.string_at(value).decode('utf-8', 'replace')
                        x.XFree(value)
                        if text == title:
                            return window
                    if depth < 5:
                        pending.extend((w, depth+1) for w in children(window))
                return None

            def named(title):
                nonlocal scanning
                scanning = True
                try:
                    return named_impl(title)
                finally:
                    scanning = False

            scenarios = [('PushButton', 1, 20, 10), ('CheckBox', 1, 5, 5),
                ('RadioButton', 1, 5, 10), ('TextField', 1, 20, 10),
                ('Menu', 1, 20, 10), ('PopupMenu', 1, 20, 10),
                ('PushButton', 3, 20, 10), ('HScrollBar', 1, 60, 20),
                ('VScrollBar', 1, 20, 60), ('MiniScroll', 1, 9, 8),
                ('List', 1, 135, 8), ('List', 1, 135, 100),
                ('List', 1, 135, 40), ('PushButton', 1, 20, 10)]
            widget_title = 'one|two|three|four|five|six|seven|eight|nine|ten|eleven|twelve'
            for index, (kind, button, px, py) in enumerate(scenarios):
                title = f'ModalProbe{index}'
                script, report = runtime.root/title, runtime.root/(title+'-report')
                # The final case replaces the title with an identical value:
                # neither content nor pointer identity proves resource lifetime.
                mutation = (' ChangeTitle 1 {'+widget_title+'}\n' if index == 13
                    else ' ChangeFont 1 {fixed}\n ChangeTitle 1 {changed}\n')
                script.write_text('WindowTitle {'+title+'}\nWindowSize 220 180\n'
                    'Init\nBegin\n Set $file={'+str(report)+'}\n'
                    ' WriteToFile $file {ready}\nEnd\n'
                    'PeriodicTasks\nBegin\n WriteToFile $file {tick}\nEnd\n'
                    'Widget 1\nProperty\n Type '+kind+'\n Position 10 10\n'
                    ' Size 150 120\n Title {'+widget_title+'}\n'
                    ' MinValue 0\n MaxValue 100\n Value 1\nMain\nCase message of\n'
                    '99 :\nBegin\n WriteToFile $file {command}\nEnd\n'
                    '98 :\nBegin\n'+mutation+
                    ' WriteToFile $file {mutated}\nEnd\n'
                    'SingleClic :\nBegin\n WriteToFile $file {click}\nEnd\nEnd\n')
                connection.send(('Module FvwmScript '+str(script)+'\n').encode())
                wait_for_predicate(title+' ready', lambda: report.exists() and
                    'ready' in report.read_text() and named(title),
                    time.monotonic()+15, process=core)
                window = named(title)
                wait_for_predicate(title+' child', lambda: bool(children(window)),
                    time.monotonic()+15, process=core)
                child = children(window)[0]
                width, height = client.geometry(child)[2:4]
                px, py = min(px, width-2), min(py, height-2)
                x.XWarpPointer(d, 0, child, 0, 0, 0, 0, px, py)
                x.XSync(d, 0)
                ticks = report.read_text().count('tick')
                ev = Event()
                ev.button = Button(type=4, display=d, window=window,
                    root=client.root, subwindow=child, x=10+px, y=10+py,
                    button=button, same_screen=1)
                assert x.XSendEvent(d, window, 0, 1 << 2, C.byref(ev))
                x.XSync(d, 0)
                connection.send(b'SendToModule FvwmScript SendString 1 99\n')
                wait_for_predicate(title+' command during hold', lambda:
                    'command' in report.read_text(), time.monotonic()+5, process=core)
                wait_for_predicate(title+' periodic during hold', lambda:
                    report.read_text().count('tick') > ticks,
                    time.monotonic()+5, process=core)
                connection.send(b'SendToModule FvwmScript SendString 1 98\n')
                wait_for_predicate(title+' mutation', lambda:
                    'mutated' in report.read_text(), time.monotonic()+5, process=core)
                # No physical release has occurred; the replaced interaction
                # must not commit a stale hover/selection after mutation.
                clicks = report.read_text().count('click')
                ev.button.type = 5
                assert x.XSendEvent(d, window, 0, 1 << 3, C.byref(ev))
                x.XSync(d, 0)
                connection.echo(title+' release')
                if index == 13:
                    commands = report.read_text().count('command')
                    released_ticks = report.read_text().count('tick')
                    connection.send(b'SendToModule FvwmScript SendString 1 99\n')
                    wait_for_predicate(title+' after release service', lambda:
                        report.read_text().count('command') > commands and
                        report.read_text().count('tick') > released_ticks,
                        time.monotonic()+5, process=core)
                assert report.read_text().count('click') == clicks
                pid = int(next(line for line in report.read_text().splitlines()
                    if line.startswith('#'+str(script)+',')).rsplit(',', 1)[1])
                os.kill(pid, signal.SIGTERM)
                wait_for_predicate(title+' termination', lambda: named(title) is None,
                    time.monotonic()+5, process=core)
                evidence[f'{index}:{kind}:{button}'] = 'command/timer/mutation/release/termination passed'
                shutil.copy2(script, options.artifacts_dir/script.name)
                shutil.copy2(report, options.artifacts_dir/report.name)
            connection.echo('wm-after-all-modal-checks')
            x.XSync(d, 0)
            assert not unexpected_errors, unexpected_errors
        finally:
            if connection:
                connection.close()
            if client:
                client.close()
            if 'previous_error_handler' in locals():
                x.XSetErrorHandler.argtypes = [C.c_void_p]
                x.XSetErrorHandler(previous_error_handler)
            for path in runtime.root.glob('*.log'):
                shutil.copy2(path, options.artifacts_dir/path.name)
            (options.artifacts_dir/'results.json').write_text(
                json.dumps(evidence, indent=2)+'\n')
    print('Script modal servicing and mutation cancellation passed')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(allow_abbrev=False)
    for name in ('fvwm', 'mfl', 'script', 'artifacts-dir'):
        parser.add_argument('--'+name, type=Path, required=True)
    parser.add_argument('--display')
    options = parser.parse_args()
    for name in ('fvwm', 'mfl', 'script', 'artifacts_dir'):
        setattr(options, name, getattr(options, name).resolve())
    run(options)
