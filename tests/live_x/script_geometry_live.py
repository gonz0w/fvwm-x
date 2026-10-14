#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.

"""Check actual Script window requests, refusal, cleanup and recovery."""
import argparse
import ctypes as C
import json
import shutil
import sys
import time
from pathlib import Path

from geometry_transition_live import Client
from live_x_harness import HarnessRuntime, start_display_server, wait_for_predicate
from mfl_stream_live import Connection
from selection_live import Button, Event


def run(options):
    options.artifacts_dir.mkdir(parents=True, exist_ok=True)
    for path in options.artifacts_dir.iterdir():
        if path.is_file() and (path.suffix in ('.log', '.json') or path.name == 'geometry-report'):
            path.unlink()
    evidence = {}
    with HarnessRuntime(prefix='fvwm-script-geometry-') as runtime:
        connection = client = None
        try:
            display = options.display
            if display is None:
                _, display = start_display_server(runtime, 'Xvfb',
                    ['Xvfb', '-screen', '0', '800x600x24', '-noreset'],
                    'xserver.log', time.monotonic()+15)
            probe = runtime.start('font-probe', [str(options.font_probe)],
                'font-probe.log', runtime.environment({'DISPLAY': display}))
            assert probe.wait_until_exit(time.monotonic()+30) == 0
            evidence['production-font-advance-and-shadow-overflow'] = 'passed'
            if options.probe_only:
                return
            wrappers = runtime.root/'modules' 
            wrappers.mkdir()
            wrapper = wrappers/'FvwmScript'
            wrapper.write_text('#!'+sys.executable+'\nimport json,os,sys\n'
                'pid=os.fork()\nif pid==0:\n'
                ' fd=os.open('+repr(str(runtime.root))+'+"/"+os.path.basename(sys.argv[-1])+"-module.log",os.O_WRONLY|os.O_CREAT,0o600)\n'
                ' os.dup2(fd,2)\n os.close(fd)\n os.execv('+repr(str(options.script))+
                ', ['+repr(str(options.script))+', *sys.argv[1:]])\n'
                '_,status=os.waitpid(pid,0)\ndest='+repr(str(runtime.root))+
                '+"/"+os.path.basename(sys.argv[-1])+"-exit.json"\n'
                'with open(dest+".tmp","w") as result:\n'
                ' result.write(json.dumps({"wait_status":status,"exit_code":'
                'os.waitstatus_to_exitcode(status)}))\n'
                'os.replace(dest+".tmp",dest)\n')
            wrapper.chmod(0o755)
            config = runtime.root/'config'
            config.write_text(f'ModulePath {wrappers}:{options.mfl.parent}\n'
                'Style * TileCascadePlacement\n'
                'AddToFunc StartFunction I Module FvwmMFL\n')
            core = runtime.start('core', [str(options.fvwm), '-d', display,
                '-f', str(config)], 'core.log', runtime.environment({
                    'FVWMMFL_SOCKET_PATH':str(runtime.root)}))
            endpoint = runtime.root/f"fvwm_mfl_{display.replace(':','_')}.sock"
            wait_for_predicate('geometry endpoint', endpoint.exists,
                time.monotonic()+15, process=core)
            connection = Connection(endpoint)
            connection.record()
            connection.send(b'set echo\n')
            connection.echo('geometry-ready')
            client = Client(display)
            x, d = client.x, client.display
            x.XFetchName.argtypes = [C.c_void_p, C.c_ulong, C.POINTER(C.c_void_p)]

            def children(window):
                root, parent, items, count = C.c_ulong(), C.c_ulong(), C.POINTER(C.c_ulong)(), C.c_uint()
                assert x.XQueryTree(d, window, C.byref(root), C.byref(parent), C.byref(items), C.byref(count))
                result = [items[i] for i in range(count.value)]
                if items:
                    x.XFree(items)
                return result

            def named(title):
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

            def geometry(window):
                root, px, py, w, h, border, depth = C.c_ulong(), C.c_int(), C.c_int(), C.c_uint(), C.c_uint(), C.c_uint(), C.c_uint()
                assert x.XGetGeometry(d, window, C.byref(root), C.byref(px), C.byref(py), C.byref(w), C.byref(h), C.byref(border), C.byref(depth))
                return [px.value, py.value, w.value, h.value]

            report = runtime.root/'geometry-report'
            bodies = {
                1: 'ChangeSize 1 65536 30',
                2: 'ChangeSize 1 0 30',
                3: 'ChangeSize 1 100 -1',
                4: 'ChangeSize 1 2147483647 30',
                5: 'ChangePosition 1 32768 10',
                6: 'ChangePosition 1 10 -32769',
                7: 'ChangePosition 1 -2147483648 2147483647',
                8: 'ChangeSize 1 123 45',
                9: 'ChangePosition 1 -25 27',
                10: 'ChangeFont 1 {Shadow=65536:fixed}',
                11: 'ChangeFont 1 {Shadow=2147483647:fixed}',
                12: 'ChangeFont 1 {fixed}',
                13: 'ChangeSize 1 65535 1',
                14: 'ChangePosition 1 -32768 32767',
                15: 'ChangeSize 1 100 30\n ChangePosition 1 10 10',
            }
            long_options = 'Geometry|'+'|'.join(['a']*3000)
            bodies[16] = '\n '.join(f'ChangeTitle {n} {{{long_options}}}' for n in (4, 5, 6))
            bodies[17] = 'Set $ok={after-popup-refusal}'
            bodies[18] = '\n '.join(f'ChangeTitle {n} {{Geometry|One|Two}}' for n in (4, 5, 6))
            bodies[19] = 'Set $ok={after-valid-popup}'
            cases = ''.join(f'{n} :\nBegin\n {body}\n Set $barrier=(GetFore 1)\n WriteToFile $file {{event:{n}:}}\nEnd\n' for n, body in bodies.items())
            script = runtime.root/'GeometryProbe'
            script.write_text('WindowTitle {GeometryProbe}\nWindowSize 260 150\n'
                'Font {fixed}\nInit\nBegin\n Set $file={'+str(report)+'}\n'
                ' WriteToFile $file {ready}\nEnd\n'
                'Widget 1\nProperty\n Type ItemDraw\n Position 10 10\n'
                ' Size 100 30\n Title {Geometry}\nMain\nCase message of\n'+cases+'End\n'
                'Widget 2\nProperty\n Type Rectangle\n Position 130 10\n Size 0 0\nMain\nCase message of\nEnd\n'
                'Widget 3\nProperty\n Type Rectangle\n Position 140 10\nMain\nCase message of\nEnd\n'
                'Widget 4\nProperty\n Type PopupMenu\n Position 150 70\n Title {Geometry|One|Two}\nMain\nCase message of\nEnd\n'
                'Widget 5\nProperty\n Type Menu\n Position 2 2\n Title {Geometry|One|Two}\nMain\nCase message of\nEnd\n'
                'Widget 6\nProperty\n Type PushButton\n Position 30 70\n Title {Geometry|One|Two}\nMain\nCase message of\nEnd\n')
            connection.send(('Module FvwmScript '+str(script)+'\n').encode())
            wait_for_predicate('geometry module startup', lambda: report.exists() and named('GeometryProbe'), time.monotonic()+20, process=core)
            window = named('GeometryProbe')
            wait_for_predicate('all geometry widget children',
                lambda: len(children(window)) == 6,
                time.monotonic()+20, process=core)
            items = children(window)
            widget = next(w for w in items if geometry(w) == [10, 10, 100, 30])
            assert sorted(geometry(w)[2:] for w in items).count([1, 1]) == 2
            evidence['zero-and-omitted-startup'] = 'passed'

            def event(n):
                connection.send(f'SendToModule FvwmScript SendString 1 {n}\n'.encode())
                wait_for_predicate(f'geometry event {n}', lambda: f'event:{n}:' in report.read_text(), time.monotonic()+15, process=core)
                x.XSync(d, 0)

            for n in range(1, 8):
                event(n)
                assert geometry(widget) == [10, 10, 100, 30], (n, geometry(widget))
            evidence['seven-invalid-requests-preserve-window'] = 'passed'
            event(8)
            assert geometry(widget) == [10, 10, 123, 45]
            event(9)
            assert geometry(widget) == [-25, 27, 123, 45]
            evidence['valid-resize-and-position-after-refusal'] = 'passed'
            event(10)
            assert geometry(widget) == [-25, 27, 123, 45]
            event(11)
            assert geometry(widget) == [-25, 27, 123, 45]
            event(12)
            evidence['invalid-font-refusal-and-valid-font-recovery'] = 'passed'
            event(13)
            assert geometry(widget) == [-25, 27, 65535, 1]
            event(14)
            assert geometry(widget) == [-32768, 32767, 65535, 1]
            event(15)
            assert geometry(widget) == [10, 10, 100, 30]
            evidence['protocol-endpoints-and-recovery'] = 'passed'
            popup_items = [next(w for w in items if geometry(w)[:2] == coords)
                for coords in ([150, 70], [2, 2], [30, 70])]
            x.XSendEvent.argtypes = [C.c_void_p, C.c_ulong, C.c_int, C.c_long, C.POINTER(Event)]

            def click(child, button, release):
                ev = Event()
                ev.button = Button(type=4, display=d, window=window,
                    root=client.root, subwindow=child, button=button, same_screen=1)
                assert x.XSendEvent(d, window, 0, 1 << 2, C.byref(ev))
                if release:
                    ev.button.type = 5
                    assert x.XSendEvent(d, window, 0, 1 << 3, C.byref(ev))
                x.XSync(d, 0)

            event(16)
            for child, button in zip(popup_items, (1, 1, 3)):
                old = report.read_text().count('event:17:')
                click(child, button, False)
                connection.send(b'SendToModule FvwmScript SendString 1 17\n')
                wait_for_predicate('oversized popup returns without a grab', lambda:
                    report.read_text().count('event:17:') > old,
                    time.monotonic()+15, process=core)
            evidence['three-oversized-popup-paths-return-and-preserve-module'] = 'passed'
            event(18)
            for child, button in zip(popup_items, (1, 1, 3)):
                old = report.read_text().count('event:19:')
                click(child, button, True)
                connection.send(b'SendToModule FvwmScript SendString 1 19\n')
                wait_for_predicate('valid popup completes after refusal', lambda:
                    report.read_text().count('event:19:') > old,
                    time.monotonic()+15, process=core)
            evidence['three-valid-popup-paths-recover'] = 'passed'
            for n, header, prop in [
                (1, 'WindowSize 65536 100', 'Size 100 30'),
                (2, 'WindowSize 200 100', 'Size 65536 30'),
                (3, 'WindowSize 200 100', 'Position 32768 10\n Size 100 30'),
                (4, 'WindowSize 200 100', 'Size 100 30\n Font {Shadow=65536:fixed}'),
                (5, 'WindowSize 200 100\n WindowPosition 2147483647 10', 'Size 100 30'),
            ]:
                bad = runtime.root/f'BadGeometry{n}'
                marker = runtime.root/f'bad-ready-{n}'
                bad.write_text(f'WindowTitle {{BadGeometry{n}}}\n{header}\nInit\nBegin\n WriteToFile {{{marker}}} {{unexpected}}\nEnd\nWidget 1\nProperty\n Type ItemDraw\n Title {{Geometry}}\n{prop}\nMain\nCase message of\nEnd\n')
                connection.send(('Module FvwmScript '+str(bad)+'\n').encode())
                status = runtime.root/(bad.name+'-exit.json')
                wait_for_predicate('invalid startup clean exit', status.exists, time.monotonic()+20, process=core)
                assert json.loads(status.read_text())['exit_code'] == 1
                assert not marker.exists() and named(bad.name) is None
                connection.echo(f'core-after-invalid-startup-{n}')
            evidence['five-invalid-startups-clean-refusal-core-service'] = 'passed'
            connection.echo('geometry-finished')
        finally:
            if connection:
                connection.close()
            if client:
                client.close()
            if (runtime.root/'GeometryProbe').exists():
                shutil.copy2(runtime.root/'GeometryProbe', options.artifacts_dir/'GeometryProbe')
            for path in runtime.root.glob('*.log'):
                shutil.copy2(path, options.artifacts_dir/path.name)
            for path in runtime.root.glob('*-exit.json'):
                shutil.copy2(path, options.artifacts_dir/path.name)
            if (runtime.root/'geometry-report').exists():
                shutil.copy2(runtime.root/'geometry-report', options.artifacts_dir/'geometry-report')
            (options.artifacts_dir/'results.json').write_text(json.dumps(evidence, indent=2)+'\n')
    print('FvwmScript geometry refusal, startup cleanup and recovery passed')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(allow_abbrev=False)
    for name in ('fvwm', 'mfl', 'script', 'font-probe', 'artifacts-dir'):
        parser.add_argument('--'+name, type=Path, required=True)
    parser.add_argument('--display')
    parser.add_argument('--probe-only', action='store_true')
    args = parser.parse_args()
    for name in ('fvwm', 'mfl', 'script', 'font_probe', 'artifacts_dir'):
        setattr(args, name, getattr(args, name).resolve())
    run(args)
