#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
"""Observe font refusal and recovery through actual core/module consumers."""
import argparse
import ctypes as C
import ctypes.util
import json
import shutil
import sys
import time
from pathlib import Path

from colorset_live import WindowAttributes
from geometry_transition_live import Client
from live_x_harness import HarnessRuntime, start_display_server, wait_for_predicate
from mfl_stream_live import Connection


class ClassHint(C.Structure):
    _fields_ = [('name', C.c_void_p), ('class_name', C.c_void_p)]


def run(options):
    out = options.artifacts_dir
    out.mkdir(parents=True, exist_ok=True)
    for path in out.iterdir():
        if path.is_file():
            path.unlink()
    evidence = {}
    invalid = 'Shadow=32768 0 C:fixed'  # Exceeds every supported build limit.
    with HarnessRuntime(prefix='fvwm-font-shadow-') as runtime:
        client = connection = None
        try:
            display = options.display
            if display is None:
                _, display = start_display_server(runtime, 'Xvfb',
                    ['Xvfb', '-screen', '0', '1000x800x24', '-noreset'],
                    'xserver.log', time.monotonic()+15)
            wrappers = runtime.root/'modules'
            wrappers.mkdir()
            for name in ('FvwmScript', 'FvwmPager', 'FvwmForm', 'FvwmButtons',
                         'FvwmIconMan', 'FvwmIdent'):
                target = options.modules/name
                if target.is_dir():
                    target = target/name
                assert target.exists(), target
                wrapper = wrappers/name
                wrapper.write_text('#!'+sys.executable+'\nimport json,os,sys\n'
                    'dest='+repr(str(runtime.root/(name+'-exit.json')))+'\n'
                    'pid=os.fork()\nif pid==0:\n'
                    ' fd=os.open('+repr(str(runtime.root/(name+'-module.log')))+
                    ',os.O_WRONLY|os.O_CREAT|os.O_APPEND,0o600)\n'
                    ' os.dup2(fd,2)\n os.close(fd)\n'
                    ' os.execv('+repr(str(target))+', ['+repr(str(target))+
                    ', *sys.argv[1:]])\n_,status=os.waitpid(pid,0)\n'
                    'with open(dest+".tmp","w") as f:\n'
                    ' f.write(json.dumps({"exit_code":os.waitstatus_to_exitcode(status)}))\n'
                    'os.replace(dest+".tmp",dest)\n')
                wrapper.chmod(0o755)
            config = runtime.root/'config'
            config.write_text(f'ModulePath {wrappers}:{options.mfl.parent}\n'
                'Style * TileCascadePlacement\nDesktopName 0 Geometry\n'
                'DefaultFont "xft:Sans-12"\n'
                'Colorset 0 fg black, bg white, fgsh gray50\n'
                'MenuStyle * Font "xft:Sans-12", TitleFont "xft:Sans-12", Colorset 0\n'
                'AddToMenu ShadowMenu "Geometry" Title\n+ "Entry" Nop\n'
                'AddToFunc StartFunction I Module FvwmMFL\n')
            core = runtime.start('core', [str(options.fvwm), '-d', display,
                '-f', str(config)], 'core.log', runtime.environment({
                    'FVWMMFL_SOCKET_PATH':str(runtime.root),
                    'FVWM_USERDIR':str(runtime.root)}))
            endpoint = runtime.root/f"fvwm_mfl_{display.replace(':','_')}.sock"
            wait_for_predicate('shadow endpoint', endpoint.exists,
                time.monotonic()+20, process=core)
            connection = Connection(endpoint)
            connection.record()
            connection.send(b'set echo\n')
            connection.echo('shadow-ready')
            client = Client(display)
            x, d = client.x, client.display
            x.XFetchName.argtypes = [C.c_void_p, C.c_ulong, C.POINTER(C.c_void_p)]
            x.XGetClassHint.argtypes = [C.c_void_p, C.c_ulong, C.POINTER(ClassHint)]
            x.XGetWindowAttributes.argtypes = [C.c_void_p, C.c_ulong, C.POINTER(WindowAttributes)]
            x.XGetImage.argtypes = [C.c_void_p, C.c_ulong, C.c_int, C.c_int,
                C.c_uint, C.c_uint, C.c_ulong, C.c_int]
            x.XGetImage.restype = C.c_void_p
            x.XGetPixel.argtypes = [C.c_void_p, C.c_int, C.c_int]
            x.XGetPixel.restype = C.c_ulong
            x.XDestroyImage.argtypes = [C.c_void_p]

            x.XInternAtom.argtypes = [C.c_void_p, C.c_char_p, C.c_int]
            x.XInternAtom.restype = C.c_ulong
            x.XGetWindowProperty.argtypes = [C.c_void_p, C.c_ulong, C.c_ulong,
                C.c_long, C.c_long, C.c_int, C.c_ulong, C.POINTER(C.c_ulong),
                C.POINTER(C.c_int), C.POINTER(C.c_ulong), C.POINTER(C.c_ulong),
                C.POINTER(C.c_void_p)]
            clients_atom = x.XInternAtom(d, b'_NET_CLIENT_LIST', 0)

            def no_managed_clients():
                actual, count, remaining = C.c_ulong(), C.c_ulong(), C.c_ulong()
                format_ = C.c_int()
                data = C.c_void_p()
                assert x.XGetWindowProperty(d, client.root, clients_atom, 0, 0,
                    0, 0, C.byref(actual), C.byref(format_), C.byref(count),
                    C.byref(remaining), C.byref(data)) == 0
                if data:
                    x.XFree(data)
                return remaining.value == 0

            def command(text):
                connection.send((text+'\n').encode())
                connection.echo('shadow-command')

            def children(window):
                root, parent = C.c_ulong(), C.c_ulong()
                items, count = C.POINTER(C.c_ulong)(), C.c_uint()
                assert x.XQueryTree(d, window, C.byref(root), C.byref(parent), C.byref(items), C.byref(count))
                result = [items[i] for i in range(count.value)]
                if items:
                    x.XFree(items)
                return result

            def windows():
                pending = [(client.root, 0)]
                result = []
                while pending:
                    window, depth = pending.pop()
                    result.append(window)
                    if depth < 5:
                        pending.extend((w, depth+1) for w in children(window))
                return result

            def named(title):
                for window in windows():
                    value = C.c_void_p()
                    if x.XFetchName(d, window, C.byref(value)) and value:
                        text = C.string_at(value).decode('utf-8', 'replace')
                        x.XFree(value)
                        if text == title:
                            return window
                return None

            def resource(name):
                for window in windows():
                    hint = ClassHint()
                    if x.XGetClassHint(d, window, C.byref(hint)):
                        text = C.string_at(hint.name).decode() if hint.name else ''
                        for value in (hint.name, hint.class_name):
                            if value:
                                x.XFree(value)
                        if text == name:
                            return window
                return None

            def picture(window, height=None):
                _, _, width, full_height = client.geometry(window)
                height = height or full_height
                assert width <= 1000 and height <= 800
                def viewable():
                    attrs = WindowAttributes()
                    assert x.XGetWindowAttributes(d, window, C.byref(attrs))
                    return attrs.map_state == 2
                wait_for_predicate('viewable pixel source', viewable,
                    time.monotonic()+15, process=core)
                previous = None
                equal = 0
                not_before = time.monotonic()+0.15
                def painted():
                    nonlocal previous, equal
                    x.XSync(d, 0)
                    image = x.XGetImage(d, window, 0, 0, width, height,
                        C.c_ulong(-1).value, 2)
                    assert image
                    try:
                        result = [width, height, [x.XGetPixel(image, px, py)
                            for py in range(height) for px in range(width)]]
                    finally:
                        x.XDestroyImage(image)
                    equal = equal+1 if result == previous else 0
                    previous = result
                    return result if equal >= 2 and time.monotonic() >= not_before else None
                return wait_for_predicate('completed stable paint', painted,
                    time.monotonic()+15, process=core)

            client.create()
            frame = wait_for_predicate('managed font client', client.frame,
                time.monotonic()+15, process=core)
            baseline_height = client.geometry(frame)[3]
            title_height = baseline_height-client.geometry(client.window)[3]
            baseline = picture(frame, title_height)
            command(f'DefaultFont "{invalid}"')
            assert picture(frame, title_height) == baseline
            assert client.geometry(frame)[3] == baseline_height
            command('DefaultFont "xft:Sans-24"')
            wait_for_predicate('valid default replacement',
                lambda: client.geometry(frame)[3] != baseline_height,
                time.monotonic()+15, process=core)
            evidence['core-default-font-refusal-and-recovery'] = 'passed'
            command('Style "FVWM transition probe" Font "xft:Sans-12", IconFont "xft:Sans-12"\nUpdateStyles')
            wait_for_predicate('valid style font',
                lambda: client.geometry(frame)[3] == baseline_height,
                time.monotonic()+15, process=core)
            baseline = picture(frame, title_height)
            command(f'Style "FVWM transition probe" Font "{invalid}", IconFont "{invalid}"\nUpdateStyles')
            assert client.geometry(frame)[3] == baseline_height
            assert picture(frame, title_height) == baseline
            command('Style "FVWM transition probe" Font "xft:Sans-24", IconFont "xft:Sans-24"\nUpdateStyles')
            wait_for_predicate('valid style recovery',
                lambda: client.geometry(frame)[3] != baseline_height,
                time.monotonic()+15, process=core)
            evidence['managed-title-font-refusal-and-recovery'] = 'passed'

            # Capture an actual menu while its ordinary modal keyboard grab is
            # active; Escape uses the existing test server extension.
            xtest = C.CDLL(ctypes.util.find_library('Xtst'))
            xtest.XTestFakeKeyEvent.argtypes = [C.c_void_p, C.c_uint, C.c_int, C.c_ulong]
            x.XKeysymToKeycode.argtypes = [C.c_void_p, C.c_ulong]
            x.XKeysymToKeycode.restype = C.c_uint

            menu_serial = 0
            def menu_picture():
                nonlocal menu_serial
                def popup():
                    for window in children(client.root):
                        attrs = WindowAttributes()
                        assert x.XGetWindowAttributes(d, window, C.byref(attrs))
                        if attrs.map_state == 2 and attrs.width > 40 and attrs.height > 20 and window not in before:
                            return window
                    return None
                before = set()
                for candidate in children(client.root):
                    attrs = WindowAttributes()
                    assert x.XGetWindowAttributes(d, candidate, C.byref(attrs))
                    if attrs.map_state == 2:
                        before.add(candidate)
                connection.send(b'Popup ShadowMenu\n')
                window = wait_for_predicate('shadow menu', popup, time.monotonic()+15, process=core)
                result = picture(window)
                (out/f'menu-{menu_serial}.json').write_text(json.dumps(result)+'\n')
                menu_serial += 1
                key = x.XKeysymToKeycode(d, 0xff1b)
                assert xtest.XTestFakeKeyEvent(d, key, 1, 0)
                assert xtest.XTestFakeKeyEvent(d, key, 0, 0)
                x.XSync(d, 0)
                connection.echo('menu-closed')
                return result

            menu_base = menu_picture()
            command(f'MenuStyle * Font "{invalid}", TitleFont "{invalid}"')
            assert menu_picture() == menu_base
            command('MenuStyle * Font "xft:Sans-24", TitleFont "xft:Sans-24"')
            assert menu_picture() != menu_base
            evidence['menu-font-and-titlefont-refusal-and-recovery'] = 'passed'
            client.destroy()
            wait_for_predicate('retired managed client', no_managed_clients,
                time.monotonic()+15, process=core)

            def launch(module, args=''):
                (runtime.root/(module+'-exit.json')).unlink(missing_ok=True)
                command('Module '+module+' '+args)

            def stop(module, alias=''):
                command('KillModule '+module+' '+alias)
                status = runtime.root/(module+'-exit.json')
                wait_for_predicate('module stopped', status.exists,
                    time.monotonic()+15, process=core)
                assert json.loads(status.read_text())['exit_code'] in (0, -15)
                wait_for_predicate('retired module window',
                    no_managed_clients if client.window is None else lambda: True,
                    time.monotonic()+15, process=core)

            report = runtime.root/'shadow-script-report'
            script = runtime.root/'ShadowScript'
            center = 'C' if options.shadow_limit >= 24 else 'N'
            size = 2 if options.shadow_limit >= 24 else 1
            script.write_text('WindowTitle {ShadowScript}\nWindowSize 260 120\n'
                'Font {fixed}\nInit\nBegin\n Set $file={'+str(report)+'}\n'
                ' WriteToFile $file {ready}\nEnd\n'
                'Widget 1\nProperty\n Type ItemDraw\n Position 10 10\n'
                ' Size 200 60\n Colorset 0\n Title {Geometry}\n'
                'Main\nCase message of\n'
                '1 :\nBegin\n ChangeFont 1 {'+invalid+'}\n'
                ' Set $barrier=(GetFore 1)\n WriteToFile $file {event:1:}\nEnd\n'
                '2 :\nBegin\n ChangeFont 1 {Shadow='+str(size)+' 0 '+center+':xft:Sans-12}\n'
                ' Set $barrier=(GetFore 1)\n WriteToFile $file {event:2:}\nEnd\n'
                '3 :\nBegin\n ChangeFont 1 {xft:Sans-12}\n'
                ' Set $barrier=(GetFore 1)\n WriteToFile $file {event:3:}\nEnd\n'
                '4 :\nBegin\n ChangeFont 1 {Shadow=1 0 N:xft:Sans-12}\n'
                ' Set $barrier=(GetFore 1)\n WriteToFile $file {event:4:}\nEnd\nEnd\n')
            launch('FvwmScript', str(script))
            window = wait_for_predicate('shadow Script', lambda: named('ShadowScript'),
                time.monotonic()+20, process=core)
            wait_for_predicate('shadow Script ready', report.exists, time.monotonic()+15, process=core)
            widget = wait_for_predicate('shadow Script widget',
                lambda: next((w for w in children(window)
                    if client.geometry(w)[2:] == (200, 60)), None),
                time.monotonic()+20, process=core)
            before = picture(widget)
            def script_event(index):
                command(f'SendToModule FvwmScript SendString 1 {index}')
                wait_for_predicate('Script font event',
                    lambda: f'event:{index}:' in report.read_text(),
                    time.monotonic()+15, process=core)
                assert client.geometry(widget)[2:] == (200, 60)
                return picture(widget)
            assert script_event(1) == before
            shadow_pixels = script_event(2)
            plain_pixels = script_event(3)
            assert shadow_pixels != plain_pixels
            assert script_event(4) != plain_pixels
            stop('FvwmScript')
            evidence['script-shadow-refusal-rendering-and-recovery'] = 'passed'

            def pager_snapshot(refused):
                command('DestroyModuleConfig ShadowPager*')
                commands = '*ShadowPager: Font "xft:Sans-12"\n*ShadowPager: WindowFont "xft:Sans-12"\n*ShadowPager: BalloonFont "xft:Sans-12"\n*ShadowPager: Geometry 160x80'
                if refused:
                    commands += '\n'+ '\n'.join(f'*ShadowPager: {key} "{invalid}"'
                        for key in ('Font', 'WindowFont', 'BalloonFont'))
                command(commands)
                launch('FvwmPager', 'ShadowPager 0 0')
                window = wait_for_predicate('pager font window', lambda: resource('ShadowPager'),
                    time.monotonic()+20, process=core)
                result = picture(window)
                (out/('pager-refused.json' if refused else 'pager-baseline.json')).write_text(json.dumps(result)+'\n')
                stop('FvwmPager', 'ShadowPager')
                return result

            pager_base = pager_snapshot(False)
            assert pager_snapshot(True) == pager_base
            evidence['pager-three-font-replacements-preserve-rendering'] = 'passed'

            def form_snapshot(refused, first=False):
                command('DestroyModuleConfig ShadowForm*')
                commands = '*ShadowForm: Title "ShadowForm"\n*ShadowForm: Font "xft:Sans-12"\n*ShadowForm: Line left\n*ShadowForm: Text "Geometry"\n*ShadowForm: Line left'
                if refused:
                    commands += f'\n*ShadowForm: Font "{invalid}"'
                commands += '\n*ShadowForm: Text "Geometry"'
                if first:
                    commands = f'*ShadowForm: Title "ShadowForm"\n*ShadowForm: Font "{invalid}"\n*ShadowForm: Line left\n*ShadowForm: Text "Geometry"'
                command(commands)
                launch('FvwmForm', 'ShadowForm')
                window = wait_for_predicate('form font window', lambda: named('ShadowForm'),
                    time.monotonic()+20, process=core)
                result = picture(window)
                stop('FvwmForm', 'ShadowForm')
                return result

            form_base = form_snapshot(False)
            assert form_snapshot(True) == form_base
            evidence['form-table-refusal-retains-preceding-font'] = 'passed'
            form_snapshot(True, first=True)
            evidence['form-first-table-default-fallback'] = 'passed'
            command(f'*ShadowButtons: Font "{invalid}"\n*ShadowButtons: (Title Geometry, Action Nop)')
            launch('FvwmButtons', 'ShadowButtons')
            wait_for_predicate('buttons inherited font', lambda: resource('ShadowButtons'),
                time.monotonic()+20, process=core)
            stop('FvwmButtons', 'ShadowButtons')
            evidence['buttons-startup-refusal-inheritance'] = 'passed'
            client.create()
            wait_for_predicate('ident target', client.frame, time.monotonic()+15, process=core)
            command(f'*FvwmIdent: Font "{invalid}"')
            (runtime.root/'FvwmIdent-exit.json').unlink(missing_ok=True)
            command(f'WindowId {client.window} Module FvwmIdent')
            status = runtime.root/'FvwmIdent-exit.json'
            wait_for_predicate('ident clean refusal', status.exists, time.monotonic()+20, process=core)
            assert json.loads(status.read_text())['exit_code'] == 1
            command('DestroyModuleConfig FvwmIdent*\n*FvwmIdent: Font "fixed"')
            (runtime.root/'FvwmIdent-exit.json').unlink(missing_ok=True)
            command(f'WindowId {client.window} Module FvwmIdent')
            wait_for_predicate('ident recovery', lambda: resource('FvwmIdent'), time.monotonic()+20, process=core)
            stop('FvwmIdent')
            evidence['ident-clean-startup-refusal-and-recovery'] = 'passed'
            command(f'*FvwmIconMan: Font "{invalid}"')
            launch('FvwmIconMan')
            status = runtime.root/'FvwmIconMan-exit.json'
            wait_for_predicate('iconman clean refusal', status.exists, time.monotonic()+20, process=core)
            assert json.loads(status.read_text())['exit_code'] == 1
            command('DestroyModuleConfig FvwmIconMan*\n*FvwmIconMan: Font "fixed"\n*FvwmIconMan: TipsFont "'+invalid+'"')
            launch('FvwmIconMan')
            wait_for_predicate('iconman recovery with refused tooltip', lambda: resource('FvwmIconMan'),
                time.monotonic()+20, process=core)
            stop('FvwmIconMan')
            evidence['iconman-clean-refusal-and-null-tooltip-recovery'] = 'passed'
            command('Nop')
            evidence['core-service-after-all-refusals'] = 'passed'
        finally:
            if client:
                client.close()
            if connection:
                connection.close()
            for path in runtime.root.iterdir():
                if path.is_file():
                    shutil.copy2(path, out/path.name)
            (out/'results.json').write_text(json.dumps(evidence, indent=2)+'\n')
    print('Shared font consumers preserve state or refuse startup cleanly')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(allow_abbrev=False)
    for name in ('fvwm', 'mfl', 'modules', 'artifacts-dir'):
        parser.add_argument('--'+name, type=Path, required=True)
    parser.add_argument('--display')
    parser.add_argument('--shadow-limit', type=int, default=4096)
    options = parser.parse_args()
    for name in ('fvwm', 'mfl', 'modules', 'artifacts_dir'):
        setattr(options, name, getattr(options, name).resolve())
    run(options)
