#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or any later version.
"""Load the installed XDG provider, use its real Form, and activate a menu item."""
import argparse
import ctypes as C
import json
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import sys
import time

from colorset_live import WindowAttributes
from geometry_transition_live import Client, Pointer
from live_x_harness import HarnessRuntime, start_display_server, wait_for_predicate
from mfl_stream_live import Connection
from selection_live import Event


def run(args):
    args.artifacts_dir.mkdir(parents=True, exist_ok=False)
    results = {}
    with HarnessRuntime(prefix='xdg-menu-live-') as runtime:
        client = connection = pointer = None
        pressed = False
        try:
            display = args.display
            if display is None:
                _, display = start_display_server(runtime, 'Xvfb',
                    ['Xvfb', '-screen', '0', '800x600x24', '-noreset'],
                    'xserver.log', time.monotonic()+10)
            user = runtime.root/'user'
            menus = runtime.root/'config/menus'
            apps = runtime.root/'data/applications'
            tools = runtime.root/'tools'
            for path in (user, menus, apps, tools):
                path.mkdir(parents=True, mode=0o700)
            shutil.copy2(args.provider, tools/'fvwm-menu-desktop')
            # Only the provider is under retirement testing. The independent
            # application/client fixture retains its absolute Python shebang.
            retired_attempts = runtime.root/'retired-runtime-attempts'
            for name in ('perl', 'python', 'python2', 'python3', 'convert', 'identify'):
                denied = tools/name
                denied.write_text('#!/bin/sh\nprintf "%s\\n" "$0" >> ' +
                                  shlex.quote(str(retired_attempts)) + '\nexit 127\n')
                denied.chmod(0o755)
            report=runtime.root/'application.json'
            app=tools/'fixture-app'
            app.write_text('#!'+sys.executable+'\nimport json,sys\n'
                           'from pathlib import Path\n'
                           'Path('+repr(str(report))+').write_text(json.dumps(sys.argv[1:]))\n')
            app.chmod(0o755)
            caption='Launch "quoted" $[literal] & 100%'
            (apps/'fixture.desktop').write_text('[Desktop Entry]\nType=Application\n'
                'Name='+caption+'\nExec=fixture-app "two words" %c %%\n')
            (menus/'applications.menu').write_text('<Menu><Name>Applications</Name>'
                '<DefaultAppDirs/><Include><All/></Include></Menu>')
            # The long value exercises the additive InputAppend path. It remains
            # preference data; this fixture has no terminal application.
            long_terminal='xterm -e '+('literal "quoted" $[text] ' * 70)
            preferences=user/'.FvwmForm-XDGMenu-Config'
            prefix='*FvwmForm-XDGMenu-ConfigDefault: '
            preferences.write_text(prefix+'Title LiveMenu\n'+prefix+'IncludeNone on\n'+
                prefix+'TermCmd '+long_terminal.strip()+'\n'+prefix+'MENapplications on\n')
            config=runtime.root/'config.fvwm'
            config.write_text('DefaultFont fixed\nMenuStyle * Font fixed\n'
                'Style * TileCascadePlacement\n'
                f'ModulePath {args.mfl.parent}:{args.form.parent}\n'
                'AddToFunc StartFunction I Module FvwmMFL\n'
                'Mouse 1 R N Popup LiveMenu\n')
            env=runtime.environment({'DISPLAY':display,'FVWM_USERDIR':str(user),
                'FVWMMFL_SOCKET_PATH':str(runtime.root),'HOME':str(runtime.root),
                'XDG_CONFIG_HOME':str(menus.parent),'XDG_CONFIG_DIRS':str(runtime.root/'absent-config'),
                'XDG_DATA_HOME':str(apps.parent),'XDG_DATA_DIRS':str(runtime.root/'absent-data'),
                'XDG_MENU_PREFIX':'','LC_ALL':'C',
                'PATH':str(tools)+os.pathsep+os.environ['PATH']})
            core=runtime.start('core',[str(args.fvwm),'-d',display,'-f',str(config)],'core.log',env)
            endpoint=runtime.root/('fvwm_mfl_'+display.replace(':','_')+'.sock')
            wait_for_predicate('MFL endpoint',endpoint.exists,time.monotonic()+10,process=core)
            connection=Connection(endpoint);connection.socket.settimeout(15)
            connection.record();connection.send(b'set echo\n')
            client=Client(display);pointer=Pointer(client);x,d=client.x,client.display
            x.XFetchName.argtypes=[C.c_void_p,C.c_ulong,C.POINTER(C.c_void_p)]
            x.XGetWindowAttributes.argtypes=[C.c_void_p,C.c_ulong,C.POINTER(WindowAttributes)]
            x.XKeysymToKeycode.argtypes=[C.c_void_p,C.c_ulong];x.XKeysymToKeycode.restype=C.c_uint
            x.XSendEvent.argtypes=[C.c_void_p,C.c_ulong,C.c_int,C.c_long,C.POINTER(Event)]
            x.XCheckTypedWindowEvent.argtypes=[C.c_void_p,C.c_ulong,C.c_int,C.POINTER(Event)]
            pointer.x.XTestFakeKeyEvent.argtypes=[C.c_void_p,C.c_uint,C.c_int,C.c_ulong]

            def find(parent):
                root,above,count=C.c_ulong(),C.c_ulong(),C.c_uint()
                children=C.POINTER(C.c_ulong)()
                assert x.XQueryTree(d,parent,C.byref(root),C.byref(above),C.byref(children),C.byref(count))
                try:
                    for window in children[:count.value]:
                        name=C.c_void_p()
                        if x.XFetchName(d,window,C.byref(name)) and name.value:
                            try:
                                if C.string_at(name)==b'Fvwm XDGMenu Config':return window
                            finally:x.XFree(name)
                        found=find(window)
                        if found:return found
                finally:
                    if children:x.XFree(children)
                return None

            def key(window,symbol):
                event=Event();event.key.type=2;event.key.display=d
                event.key.window=window;event.key.root=client.root;event.key.same_screen=1
                event.key.keycode=x.XKeysymToKeycode(d,symbol);assert event.key.keycode
                assert x.XSendEvent(d,window,0,1,C.byref(event));x.XSync(d,0)

            def watch_close(window):
                x.XSelectInput(d,window,1<<17);x.XSync(d,0)

            def closed(window):
                event=Event()
                return bool(x.XCheckTypedWindowEvent(d,window,17,C.byref(event)))

            def root_clear():
                root,child=C.c_ulong(),C.c_ulong()
                rx,ry,wx,wy=C.c_int(),C.c_int(),C.c_int(),C.c_int()
                mask=C.c_uint()
                assert x.XQueryPointer(d,client.root,C.byref(root),C.byref(child),
                    C.byref(rx),C.byref(ry),C.byref(wx),C.byref(wy),C.byref(mask))
                return child.value==0

            produced=subprocess.check_output([str(tools/'fvwm-menu-desktop'),'--configure'],env=env,timeout=15).decode()
            (args.artifacts_dir/'produced-form').write_text(produced)
            lines=[];marker=0
            for line in produced.splitlines():
                lines.append(line)
                if line.startswith('*FvwmForm-XDGMenu-Config: Command PipeRead '):
                    marker+=1
                    lines.append('*FvwmForm-XDGMenu-Config: Command Echo form-job-'+str(marker))
            assert marker==3
            form_config=runtime.root/'instrumented-form'
            form_config.write_text('\n'.join(lines)+'\n')
            shutil.copy2(form_config,args.artifacts_dir/'instrumented-form')
            connection.send(('Read '+json.dumps(str(form_config))+'\n').encode())
            window=wait_for_predicate('native settings Form',lambda:find(client.root),time.monotonic()+15,process=core)
            key(window,0xffc0) # F3: Save.
            connection.expect_echo('form-job-3')
            wait_for_predicate('private saved settings',lambda:preferences.stat().st_mode&0o777==0o600,time.monotonic()+15,process=core)
            saved=preferences.read_text()
            assert prefix+'TermCmd '+long_terminal.strip()+'\n' in saved,saved
            assert prefix+'MENapplications on\n' in saved,saved
            results['native_save_literal_long_input']=True
            key(window,0xffbf) # F2: Regenerate through a completed result.
            connection.expect_echo('form-job-2')
            wait_for_predicate('Form job result consumed',lambda:not list(user.glob('.XDGMenu-result-*')),time.monotonic()+15,process=core)
            key(window,0xffbe) # F1: Export, then read the exported native menu.
            connection.expect_echo('form-job-1')
            exported=user/'.XDGMenu'
            wait_for_predicate('exported menu',exported.exists,time.monotonic()+15,process=core)
            assert exported.stat().st_mode&0o777==0o600
            assert '--launch ' in exported.read_text()
            connection.echo('export-finished')
            results['native_regenerate_export']=True
            watch_close(window)
            connection.send(b'KillModule FvwmForm FvwmForm-XDGMenu-Config\n')
            connection.echo('form-closed')
            # Observe destruction without querying stale child IDs. The
            # ordered command acknowledgement precedes the module's X teardown.
            wait_for_predicate('settings Form window closes',
                lambda:closed(window),time.monotonic()+10,process=core)

            def mapped_menus():
                root,parent,count=C.c_ulong(),C.c_ulong(),C.c_uint()
                children=C.POINTER(C.c_ulong)()
                assert x.XQueryTree(d,client.root,C.byref(root),C.byref(parent),C.byref(children),C.byref(count))
                found=[]
                try:
                    for window in children[:count.value]:
                        attrs=WindowAttributes()
                        if x.XGetWindowAttributes(d,window,C.byref(attrs)) and attrs.map_state==2 and attrs.save_under and attrs.width>20:
                            found.append(client.geometry(window))
                finally:
                    if children:x.XFree(children)
                return found
            pointer.motion(200,200)
            wait_for_predicate('root click target clears',root_clear,
                time.monotonic()+10,process=core)
            pointer.button(True);pressed=True
            shown=wait_for_predicate('generated menu maps',mapped_menus,time.monotonic()+10,process=core)
            assert len(shown)==1,shown
            # FVWM selects the first actionable row on Down. Return activates it;
            # physical release completes the surrounding root binding normally.
            for symbol in (0xff54,0xff0d):
                code=x.XKeysymToKeycode(d,symbol);assert code
                for down in (True,False):assert pointer.x.XTestFakeKeyEvent(d,code,down,0)
            x.XSync(d,0);pointer.button(False);pressed=False
            wait_for_predicate('literal application arguments',report.exists,time.monotonic()+15,process=core)
            assert json.loads(report.read_text())==['two words',caption,'%'],report.read_text()
            assert not retired_attempts.exists(), retired_attempts.read_text()
            results['no_retired_runtime_commands'] = True
            results['actual_menu_action_literal_argv']=True
            connection.echo('application-complete')
            assert core.poll() is None
            results['core_alive']=True
            results['three_test_acknowledgements_added']=True
            shutil.copy2(preferences,args.artifacts_dir/'saved-preferences')
            shutil.copy2(exported,args.artifacts_dir/'exported-menu')
            shutil.copy2(report,args.artifacts_dir/'application.json')
        finally:
            if pressed and pointer:pointer.button(False)
            if connection:connection.close()
            if client:client.close()
            for log in runtime.root.glob('*.log'):shutil.copy2(log,args.artifacts_dir/log.name)
            (args.artifacts_dir/'results.json').write_text(json.dumps(results,indent=2)+'\n')
    print(json.dumps(results,sort_keys=True))


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__,allow_abbrev=False)
    for option in ('fvwm','mfl','form','provider','artifacts-dir'):
        parser.add_argument('--'+option,type=Path,required=True)
    parser.add_argument('--display')
    options=parser.parse_args()
    for name,value in vars(options).items():
        if isinstance(value,Path):
            setattr(options,name,value.resolve())
    run(options)
