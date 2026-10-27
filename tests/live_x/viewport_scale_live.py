#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
"""Observe actual monitor scaling of opt-in viewport Forms and the browser.

Uses real RandR millimetres, public Form status/selection replies, real X window
rectangles and transparent native proxies. Font failure is a separately linked
test wrapper's injected acquisition failure, never a missing-font attestation.
This checkpoint does not qualify physical hardware or assistive technology.
"""
import argparse
import base64
import ctypes as C
import hashlib
import json
import os
from pathlib import Path
import re
import shlex
import shutil
import sys
import tempfile
import time
import traceback

from desktop_live import connect_client
from geometry_transition_live import SizeHints
from inspector_live import NativeView, atomic_json
from live_x_harness import (HarnessRuntime, start_xorg_dummy, wait_for_randr,
    run_required_xrandr, wait_for_authoritative_state, wait_for_predicate)
from mfl_request_live import frame
from mfl_stream_live import Connection
from module_ui_scale_live import wait_children
from native_bindings_live import module_proxy, rows
from native_capture import NativeCapture
from session_x_errors import SessionXErrors

PROFILES = ('slate-dark', 'dune-light', 'classic-large')
ENTRIES = 4096
MM = {4: (677, 572), 5: (542, 458), 6: (452, 381), 8: (339, 286)}


class ClassHint(C.Structure):
    _fields_ = [('name', C.c_void_p), ('class_', C.c_void_p)]


class ResizeRequest(C.Structure):
    _fields_ = [('type',C.c_int),('serial',C.c_ulong),('send_event',C.c_int),
        ('display',C.c_void_p),('window',C.c_ulong),('width',C.c_int),('height',C.c_int)]


class ResizeEvent(C.Union):
    _fields_ = [('resize',ResizeRequest),('pad',C.c_long*24)]


def run(args, profile, destination):
    source, build = Path(__file__).resolve().parents[2], args.build.resolve()
    out = destination/profile
    out.mkdir()
    evidence = dict(status='running', profile=profile, screenshots=[], operations=[],
        failures=[], cleanup_failures=[], scale_samples=[],
        limits={'entries': ENTRIES, 'children': 64, 'physical_or_assistive_qualified': False},
        source_sha256={name: hashlib.sha256((source/name).read_bytes()).hexdigest() for name in
            ['tests/live_x/viewport_scale_live.py', 'tests/live_x/native_bindings_live.py',
             'tests/live_x/module_ui_scale_live.py',
             'modules/FvwmForm/FvwmForm.c', 'modules/FvwmForm/ReadXServer.c',
             'modules/FvwmForm/form_scale.c', 'modules/FvwmForm/form_scale_policy.c',
             'modules/FvwmForm/list_widget.c', 'tests/form_scale_refuse_probe.c',
             'bin/FvwmPrompt/cmd/fvwmfilebrowser/native.go',
             'bin/FvwmPrompt/cmd/fvwmfilebrowser/native_feedback.go',
             'bin/FvwmPrompt/cmd/fvwmfilebrowser/view.go']},
        binary_sha256={str(p.relative_to(build)): hashlib.sha256(p.read_bytes()).hexdigest() for p in
            [build/'fvwm3', build/'modules/FvwmForm/FvwmForm', build/'bin/FvwmPrompt/fvwm-file-browser']})
    with HarnessRuntime(prefix='fvwm-viewport-scale-') as runtime:
        core = client = errors = control = rpc_peer = browser = None
        try:
            # Large actual monitor modes keep 18pt at 2x and fixed browser
            # controls visible. 30 Hz stays within the dummy driver's clock.
            xorg = runtime.root/'xorg.conf'
            original = args.xorg_config.resolve().read_text()
            original = original.replace('    Modeline "1920x1080"',
                '    Modeline "2560x2160" 225.00 2560 2720 2992 3424 2160 2163 2173 2190\n'
                '    Modeline "1920x1080"')
            original = original.replace('Modes "1920x1080"', 'Modes "2560x2160" "1920x1080"')
            original = original.replace('Virtual 4096 2160', 'Virtual 8192 4096')
            xorg.write_text(original)
            _, display = start_xorg_dummy(runtime, xorg, time.monotonic()+10)
            wait_for_randr(runtime, display, time.monotonic()+10)

            def mutate(arguments, label):
                run_required_xrandr(runtime, display, arguments, label, evidence['operations'])

            mutate(['--addmode', 'DUMMY1', '2560x2160'], 'second-large-mode')
            mutate(['--output', 'DUMMY0', '--mode', '2560x2160', '--pos', '0x0', '--primary',
                '--set', 'WIDTH_MM', '677', '--set', 'HEIGHT_MM', '572',
                '--output', 'DUMMY1', '--mode', '2560x2160', '--pos', '2560x0',
                '--set', 'WIDTH_MM', '542', '--set', 'HEIGHT_MM', '458',
                '--fb', '5120x2160', '--fbmm', '1355x572'], 'mixed-1-and-1.25')

            def facts(quarters):
                width, height = MM[quarters]
                return wait_for_authoritative_state(runtime, display, 'actual scaled-output millimetres',
                    lambda s: set(s['monitors']) == {'DUMMY0','DUMMY1'} and
                        s['monitors']['DUMMY1']['mm_width'] == width and
                        s['monitors']['DUMMY1']['mm_height'] == height,
                    time.monotonic()+6, 'facts-'+str(quarters))

            evidence['baseline_server'] = facts(5)
            modules, native, user, folder = [runtime.root/n for n in ('modules','native-records','user','files')]
            for p in (modules,native,user,folder): p.mkdir(mode=0o700)
            for i in range(ENTRIES): (folder/('entry-%04d' % i)).touch()
            for p in (build/'modules').iterdir():
                if (p/p.name).is_file() and p.name not in ('FvwmForm','FvwmMFL'):
                    (modules/p.name).symlink_to(p/p.name)
            (modules/'FvwmDesktop').symlink_to(build/'bin/FvwmPrompt/FvwmDesktop')
            binaries = [('FvwmForm',build/'modules/FvwmForm/FvwmForm'),
                        ('FvwmMFL',build/'modules/FvwmMFL/FvwmMFL')]
            if args.fault_form:
                binaries.append(('ViewportScaleFaultForm',args.fault_form.resolve()))
                evidence['fault_binary_sha256'] = hashlib.sha256(args.fault_form.read_bytes()).hexdigest()
            for name,binary in binaries:
                wrapper = modules/name
                invocation=[sys.executable,str(Path(__file__).resolve()),'--module-proxy',str(binary),str(native)]
                if name=='ViewportScaleFaultForm':
                    invocation=['env','ASAN_OPTIONS=detect_leaks=0:abort_on_error=1',
                        'UBSAN_OPTIONS=halt_on_error=1',*invocation]
                    evidence['fault_child_environment']={'ASAN_OPTIONS':'detect_leaks=0:abort_on_error=1',
                        'UBSAN_OPTIONS':'halt_on_error=1','leak_freedom_claimed':False,
                        'failure_hold':'second armed non1x acquisition and later non1x attempts while armed'}
                wrapper.write_text('#!/bin/sh\nexec '+shlex.join(invocation)+' "$@" 2>>'+
                    shlex.quote(str(runtime.root/(name+'.log')))+'\n')
                wrapper.chmod(0o700)
            assets = source/'default-config'
            palette, appearance = ('dune','light') if profile == 'dune-light' else ('slate','dark')
            (user/'local.config').write_text('*FvwmDesktop: palette '+palette+'\n'
                '*FvwmDesktop: appearance '+appearance+'\n*FvwmDesktop: motion Off\n'
                '*FvwmDesktop: transparency Off\n')
            body = (assets/'config').read_text().replace('$[FVWM_DATADIR]/default-config',str(assets))
            if profile == 'classic-large':
                body = body.replace('Sans:size=10:antialias=True','DejaVu Sans Mono:size=18:antialias=True')
                body += ('\n*FvwmFormDefault: Fore wheat\n*FvwmFormDefault: Back navy\n'
                    '*FvwmFormDefault: ItemFore black\n*FvwmFormDefault: ItemBack ivory\n')
            config = runtime.root/'config'
            config.write_text('ModulePath '+str(modules)+'\nRead '+str(source/'fvwm/ConfigFvwmDefaults')+
                '\n'+body+'\nStyle * PositionPlacement 100p 100p\n'
                'Style FvwmFiles-* PositionPlacement screen DUMMY1 100p 100p, StartsOnScreen DUMMY1\n'
                'AddToFunc StartFunction I Module FvwmMFL\n')
            evidence['config_sha256'] = hashlib.sha256(config.read_bytes()).hexdigest()
            evidence['font'] = 'DejaVu Sans Mono 18' if profile == 'classic-large' else 'Sans 10'
            arm = runtime.root/'fail-font'
            env = runtime.environment({'DISPLAY':display,'HOME':str(runtime.root),'FVWM_USERDIR':str(user),
                'FVWM_DESKTOP_ROOT':str(assets),'FVWMMFL_SOCKET_PATH':str(runtime.root),
                'FVWM_TEST_FORM_SCALE_FAIL_FILE':str(arm), 'LANG':'C.UTF-8','LC_ALL':'C.UTF-8',
                'XDG_DATA_HOME':str(runtime.root/'data'),'XDG_DATA_DIRS':str(runtime.root/'absent')})
            core = runtime.start('core',[str(build/'fvwm3'),'-d',display,'-f',str(config)],'core.log',env)
            endpoint = runtime.root/('fvwm_mfl_'+display.replace(':','_')+'.sock')
            wait_for_predicate('scale fixture MFL',endpoint.exists,time.monotonic()+12,process=core)
            control,rpc_peer = Connection(endpoint),Connection(endpoint)
            control.record(); rpc_peer.record()
            control.send(b'set echo\n'); control.echo('viewport-scale-ready')
            client = connect_client(display)
            errors = SessionXErrors(client,evidence).install()
            view = NativeView(client,errors,core)
            x,d,pointer = client.x,client.display,view.pointer
            owners={}
            for name,arguments,result in [
                ('XSetInputFocus',[C.c_void_p,C.c_ulong,C.c_int,C.c_ulong],C.c_int),
                ('XMoveWindow',[C.c_void_p,C.c_ulong,C.c_int,C.c_int],C.c_int),
                ('XGetClassHint',[C.c_void_p,C.c_ulong,C.POINTER(ClassHint)],C.c_int),
                ('XGetImage',[C.c_void_p,C.c_ulong,C.c_int,C.c_int,C.c_uint,C.c_uint,
                    C.c_ulong,C.c_int],C.c_void_p),
                ('XGetPixel',[C.c_void_p,C.c_int,C.c_int],C.c_ulong),
                ('XDestroyImage',[C.c_void_p],C.c_int),
                ('XInternAtom',[C.c_void_p,C.c_char_p,C.c_int],C.c_ulong),
                ('XTranslateCoordinates',[C.c_void_p,C.c_ulong,C.c_ulong,C.c_int,C.c_int,
                    C.POINTER(C.c_int),C.POINTER(C.c_int),C.POINTER(C.c_ulong)],C.c_int),
                ('XGetWMNormalHints',[C.c_void_p,C.c_ulong,C.POINTER(SizeHints),C.POINTER(C.c_long)],C.c_int)]:
                getattr(x,name).argtypes,getattr(x,name).restype = arguments,result
            x.XCheckTypedWindowEvent.argtypes=[C.c_void_p,C.c_ulong,C.c_int,C.POINTER(ResizeEvent)]
            x.XCheckTypedWindowEvent.restype=C.c_int
            atoms = {n:x.XInternAtom(d,n.encode(),1) for n in ('DUMMY0','DUMMY1')}
            assert all(atoms.values())

            def command(text):
                control.send((text+'\n').encode()); control.echo('vs-'+os.urandom(4).hex())

            def receipt(prefix):
                deadline = time.monotonic()+8
                while time.monotonic()<deadline:
                    text = control.record().get('echo',{}).get('message','')
                    if text.startswith(prefix): return text
                raise AssertionError('missing actual native reply '+prefix)

            def configured_theme():
                control.send(b'SendToModule FvwmDesktop query-settings scale-theme all\n')
                values={}
                while True:
                    raw=receipt('desktop-settings-scale-theme ')
                    if raw.endswith(' done'): break
                    _,name,value=raw.split(' ',2); values[name]=json.loads(value)
                return values if all(values.get(n,{}).get('effective')==v and
                    values[n].get('application')=='submitted-to-fvwm' for n,v in
                    [('palette',palette),('appearance',appearance),('transparency',False)]) else None

            evidence['theme_settings']=wait_for_predicate('actual requested desktop theme',configured_theme,
                time.monotonic()+12,process=core)

            def scale(alias):
                token = 's'+os.urandom(8).hex()
                control.send(('SendToModule '+alias+' UIScaleStatus '+token+'\n').encode())
                raw = receipt('FVWM_FORM_UI_SCALE '+token+' ')
                f = raw.split()
                assert len(f)==15 and f[2] in ('0','1') and f[3] in (
                    'disabled','unsupported','waiting','deferred','refused','ready'),raw
                assert re.fullmatch('[0-9a-f]{16}',f[6]) and re.fullmatch('[0-9a-f]{16}',f[7]),raw
                value = dict(alias=alias,raw=raw,enabled=int(f[2]),state=f[3],current_q=int(f[4]),
                    desired_q=int(f[5]),current_epoch=f[6],desired_epoch=f[7],current_atom=int(f[8]),
                    desired_atom=int(f[9]),resource_gen=int(f[10]),min_w=int(f[11]),min_h=int(f[12]),
                    required_w=int(f[13]),required_h=int(f[14]))
                assert len(evidence['scale_samples'])<4096
                evidence['scale_samples'].append(value)
                return value

            def geometry(window):
                a = view.attributes(window); assert a and a.map_state==2
                return [a.x,a.y,a.width,a.height]

            def root_rect(window):
                rx,ry,child = C.c_int(),C.c_int(),C.c_ulong()
                assert x.XTranslateCoordinates(d,window,client.root,0,0,C.byref(rx),C.byref(ry),C.byref(child))
                return [rx.value,ry.value,*geometry(window)[2:]]

            def ready(alias,window,q,monitor):
                def observed():
                    value = scale(alias)
                    rect = root_rect(window)
                    left = 0 if monitor=='DUMMY0' else 2560
                    if not (left <= rect[0] < left+2560): return None
                    if value['state']!='ready' or value['current_q']!=q or value['desired_q']!=q or \
                            value['current_atom']!=atoms[monitor] or value['desired_atom']!=atoms[monitor]: return None
                    assert value['current_epoch']==value['desired_epoch'] and int(value['current_epoch'],16)>0,value
                    assert rect[2]>=value['min_w'] and rect[3]>=value['min_h'],(value,rect)
                    hints,supplied = SizeHints(),C.c_long()
                    assert x.XGetWMNormalHints(d,window,C.byref(hints),C.byref(supplied))
                    assert [hints.min_width,hints.min_height]==[value['min_w'],value['min_h']],value
                    return dict(value,root_rect=rect)
                return wait_for_predicate('actual ready scale '+str(q),observed,time.monotonic()+10,process=core)

            def list_reply(raw,token):
                f=raw.split()
                assert len(f)==6 and f[:2]==['FVWM_FORM_LIST',token] and f[4]=='0',raw
                packet=base64.urlsafe_b64decode(f[5][1:]+'==')
                assert int.from_bytes(packet[:4],'big')==int(f[2]),raw
                return dict(generation=int(f[2]),pending=int(f[3]),packet=packet.hex(),
                    focus=int.from_bytes(packet[4:6],'big'),selected=[i for i in range((len(packet)-6)*8)
                        if packet[6+i//8] & (1 << (i%8))],raw=raw)

            def list_status(alias,pending=False):
                def observed():
                    token = 'l'+os.urandom(8).hex()
                    control.send(('SendToModule '+alias+' ListStatus picks '+token+' selection\n').encode())
                    value=list_reply(receipt('FVWM_FORM_LIST '+token+' '),token)
                    return None if not pending and value['pending'] else value
                return wait_for_predicate('actual native collection state',observed,time.monotonic()+8,process=core)

            def changed(token,since=0):
                return [r for r in rows(native) if r['time']>=since and r['direction']=='out' and
                    r['text']=='Echo FVWM_FORM_CHANGED '+token]

            def watch_status(alias,watch):
                token='a'+os.urandom(8).hex()
                control.send(('SendToModule '+alias+' ListWatchStatus picks '+watch+' '+token+'\n').encode())
                return list_reply(receipt('FVWM_FORM_LIST '+token+' '),token)

            def focus_owner(window,force=False):
                owner=owners.get(window,window)
                if force or view.focus()!=owner:
                    command('WindowId %d Raise' % owner)
                    command('WindowId %d Focus' % owner)
                    wait_for_predicate('actual managed Form keyboard focus',lambda:view.focus()==owner,
                        time.monotonic()+5,process=core)
                assert view.focus()==owner
                return owner

            def key(window,symbol,shift=False,alt=False,control_key=False):
                focus_owner(window)
                codes = [x.XKeysymToKeycode(d,x.XStringToKeysym(n.encode())) for n,b in
                    [('Shift_L',shift),('Alt_L',alt),('Control_L',control_key)] if b]
                for code in codes: assert pointer.x.XTestFakeKeyEvent(d,code,1,0)
                view.key(symbol)
                for code in reversed(codes): assert pointer.x.XTestFakeKeyEvent(d,code,0,0)
                x.XSync(d,0)

            def click(window,px=8,py=8,button=1):
                owner=focus_owner(window,force=True)
                rect=root_rect(window); pointer.motion(rect[0]+px,rect[1]+py)
                target=client.root; path=[]
                for _ in range(16):
                    root,child=C.c_ulong(),C.c_ulong()
                    rx,ry,wx,wy,mask=C.c_int(),C.c_int(),C.c_int(),C.c_int(),C.c_uint()
                    assert x.XQueryPointer(d,target,C.byref(root),C.byref(child),
                        C.byref(rx),C.byref(ry),C.byref(wx),C.byref(wy),C.byref(mask))
                    path.append(target)
                    if not child.value: break
                    target=child.value
                else: raise AssertionError('pointer hit-test exceeded owned tree depth')
                assert target==window,(window,path)
                evidence.setdefault('pointer_hits',[]).append(dict(target=window,owner=owner,path=path,
                    root_point=[rect[0]+px,rect[1]+py],focus=view.focus()))
                assert pointer.x.XTestFakeButtonEvent(d,button,1,0)
                assert pointer.x.XTestFakeButtonEvent(d,button,0,0)
                x.XSync(d,0)
                wait_for_predicate('actual pointer grab released after click',lambda:not pointer.grabbed(),
                    time.monotonic()+5,process=core)

            def type_text(window,field,text):
                click(field); key(window,'u',control_key=True)
                symbols={' ':'space','/':'slash','-':'minus','_':'underscore','.':'period',
                    ';':'semicolon','$':'dollar','%':'percent','[':'bracketleft',']':'bracketright'}
                for c in text: key(window,symbols.get(c,c.lower()),shift=c in '_$%' or c.isupper())

            def move(window,monitor):
                left = 100 if monitor=='DUMMY0' else 2660
                command('WindowId %d Move %dp 100p' % (window,left))
                wait_for_predicate('actual root-pixel move',lambda: left <= root_rect(window)[0] < left+60,
                    time.monotonic()+5,process=core)

            def physical(q):
                width,height=MM[q]
                mutate(['--output','DUMMY1','--set','WIDTH_MM',str(width),'--set','HEIGHT_MM',str(height)],
                    'output-factor-'+str(q)+'-'+str(len(evidence['operations'])))
                return facts(q)

            def capture(name):
                previous=os.environ.get('DISPLAY'); os.environ['DISPLAY']=display
                try:
                    p=Path(NativeCapture(build/'bin/fvwm-desktop-capture').screenshot(out)
                        .removeprefix('Screenshot saved: ')); p.replace(out/name)
                    evidence['screenshots'].append(name)
                finally:
                    if previous is None: os.environ.pop('DISPLAY',None)
                    else: os.environ['DISPLAY']=previous

            def pixels(window,left,top,width,height):
                # Read the real server drawable. Small focus strips and the
                # two label lines are bounded independently of desktop size.
                assert 0<width<=2560 and 0<height<=256 and width*height<=524288
                image=x.XGetImage(d,window,left,top,width,height,C.c_ulong(-1).value,2)
                assert image
                try:
                    return tuple(x.XGetPixel(image,a,b)
                        for b in range(height) for a in range(width))
                finally:
                    x.XDestroyImage(image)

            def caret_pixels(window,field,q):
                box=(3*q+2)//4; stroke=(q+2)//4
                width,height=geometry(field)[2:]
                def observed():
                    line=pixels(field,box,height//2,width-2*box,1)
                    background=line[-1]
                    ink=[i+box for i,p in enumerate(line) if p!=background]
                    return dict(columns=ink,background=background) if len(ink)==stroke and \
                        ink==list(range(ink[0],ink[0]+stroke)) else None
                click(field); key(window,'u',control_key=True)
                # Tab away/back exercises RedrawItem's caret path. Inserting
                # a space exercises the KeyPress path without glyph pixels.
                key(window,'Tab'); key(window,'Tab',shift=True)
                redraw=wait_for_predicate('actual redraw caret stroke',observed,
                    time.monotonic()+5,process=core)
                key(window,'space')
                typed=wait_for_predicate('actual typed caret stroke and advance',lambda:
                    (s if s and s['columns'][0]>redraw['columns'][0] else None)
                    if (s:=observed()) else None,time.monotonic()+5,process=core)
                key(window,'u',control_key=True)
                cleared=wait_for_predicate('actual cleared caret pixels',lambda:
                    (s if s==redraw else None) if (s:=observed()) else None,
                    time.monotonic()+5,process=core)
                return dict(field=field,quarters=q,stroke=stroke,redraw=redraw,
                    typed=typed,cleared=cleared,geometry=[width,height])

            def focus_pixels(alias,window,field,collection,q):
                box=(3*q+2)//4; stroke=(q+2)//4
                # The first cell's top margin contains no label/icon ink.
                # Compare focused/unfocused pixels at the same selection,
                # so palette inversion cannot imitate a focus indicator.
                sample_x=geometry(collection)[2]//2
                def strip(): return pixels(collection,sample_x,box,1,stroke+1)
                click(field); key(window,'Tab'); key(window,'Home')
                initial=list_status(alias)
                assert initial['focus']==0 and not initial['selected'],initial
                samples=[]
                for selected in (False,True):
                    status=wait_for_predicate('actual selection before focus pixels',lambda:
                        (s if s['focus']==0 and s['selected']==([0] if selected else []) else None)
                        if (s:=list_status(alias)) else None,time.monotonic()+5,process=core)
                    active=wait_for_predicate('actual list focus top stroke',lambda:
                        (p if len(set(p:=strip()))==2 and len(set(p[:stroke]))==1 and
                            p[0]!=p[-1] and (not selected or p[-1]!=samples[0]['active'][-1])
                            else None),time.monotonic()+5,process=core)
                    assert status['focus']==0 and status['selected']==([0] if selected else []),status
                    if alias=='ScaleReadable' and q==8:
                        capture('readability-narrow-'+('selected' if selected else 'unselected')+'-focus-2x.png')
                    key(window,'Tab',shift=True)
                    inactive=wait_for_predicate('actual list focus removed',lambda:
                        (p if len(set(p:=strip()))==1 else None),time.monotonic()+5,process=core)
                    assert all(a!=b for a,b in zip(active[:stroke],inactive[:stroke]))
                    assert active[-1]==inactive[-1]
                    samples.append(dict(selected=selected,status=status,active=active,
                        inactive=inactive,origin=[sample_x,box],stroke=stroke))
                    key(window,'Tab'); key(window,'space')
                assert list_status(alias)['packet']==initial['packet']
                return samples

            def form(alias,extra=(),enabled=True,binary='FvwmForm',label_viewport=False,watch=None):
                lines=['Title "'+alias+'"']+(['UIScale monitor'] if enabled else [])+list(extra)+[
                    'Line left','Label title 32 "Literal viewport scale test"',
                    *(['LabelViewport title'] if label_viewport else []),'Line left',
                    'Input draft 24 ""','Button continue "Observe (Alt-F)" Alt-F',
                    'Command Echo VIEWPORT_DRAFT '+alias+' x$(draft:Base64) x$(picks:Base64)',
                    'Button continue "Replay (F9)" F9','Command Send_ConfigInfo',
                    'Line left','List picks 40 4','ListViewport 18 1','ListVersioned',
                    *(['ListWatch '+watch,'ListNotify '+watch+'_start'] if watch else []),
                    *['ListItem off "entry-%02d" file' % i for i in range(32)]]
                for line in lines: control.send(('*'+alias+': '+line+'\n').encode())
                command('Module '+binary+' '+alias)
                window=view.wait(alias); ids=sorted(view.children(window)); assert len(ids)<=64
                owners.update({child:window for child in [window,*ids]})
                return window,ids

            def label_raster(window,field,committed=None):
                actual_width=geometry(window)[2]; height=geometry(field)[1]
                width=actual_width if committed is None else committed['width']
                if committed is not None:
                    # Core-owned parent enlargement can precede adoption.
                    # Compare the complete old label region at its committed
                    # coordinates, including its old blank area; held fonts
                    # and child geometry must not follow the parent's size.
                    assert actual_width>=width and height==committed['height']
                # Enlarged browser headers span several fixed label lines.
                # Hash the identical committed coordinates in bounded strips;
                # do not expand an old crop to a newly enlarged parent.
                assert 0<height<=2160
                digest=hashlib.sha256()
                for top in range(0,height,128):
                    digest.update(b''.join(int(p).to_bytes(8,'little')
                        for p in pixels(window,0,top,width,min(128,height-top))))
                return dict(width=width,height=height,sha256=digest.hexdigest())

            def label_pixels(window,field):
                width=geometry(window)[2]; height=geometry(field)[1]
                data=pixels(window,0,0,width,height)
                background=data[0]
                occupied=[y for y in range(height)
                    if any(p!=background for p in data[y*width:(y+1)*width])]
                assert occupied,'actual label glyphs missing'
                # Two dedicated Label lines share a font. The largest blank
                # gap between their ink rows separates them without predicting
                # glyph dimensions or substituting a font measurement.
                gaps=[(b-a,a,b) for a,b in zip(occupied,occupied[1:])]
                gap,above,below=max(gaps)
                assert gap>1,(height,occupied)
                bands=[]
                for top,bottom in [(occupied[0],above),(below,occupied[-1])]:
                    ink=[(i%width,i//width+top) for i,p in
                        enumerate(data[top*width:(bottom+1)*width]) if p!=background]
                    left,right=min(p[0] for p in ink),max(p[0] for p in ink)
                    bands.append(dict(bounds=[left,top,right,bottom],
                        ink_sha256=hashlib.sha256(b''.join(int(p).to_bytes(8,'little')
                            for y in range(top,bottom+1)
                            for p in data[y*width+left:y*width+right+1])).hexdigest(),
                        sha256=hashlib.sha256(b''.join(int(p).to_bytes(8,'little')
                            for p in data[top*width:(bottom+1)*width])).hexdigest()))
                return dict(width=width,height=height,bands=bands,background=background,
                    sha256=hashlib.sha256(b''.join(int(p).to_bytes(8,'little') for p in data)).hexdigest())

            def kill_form(alias,binary='FvwmForm'):
                command('KillModule '+binary+' '+alias)
                wait_for_predicate('owned '+alias+' window withdrawn',lambda:not view.named(alias),
                    time.monotonic()+5,process=core)
                started=[json.loads(p.read_text()) for p in native.glob('module-*.json')
                    if alias in json.loads(p.read_text())['arguments']]
                assert len(started)==1,started
                path=native/('stopped-%d.json' % started[0]['proxy'])
                wait_for_predicate('owned '+alias+' actual child Wait',path.exists,
                    time.monotonic()+5,process=core)
                waited=json.loads(path.read_text())
                assert waited['child']==started[0]['child'] and waited['returncode']==0,waited
                evidence.setdefault('generic_child_cleanup',[]).append(dict(alias=alias,
                    proxy=started[0]['proxy'],waited=waited))

            # Native wakeups are opt-in and remain latched until the exact
            # subscription ACK. Ordinary status is an observer/barrier only.
            watch='viewport_'+profile.replace('-','_')
            watched,watched_ids=form('ScaleWatched',watch=watch)
            ready('ScaleWatched',watched,4,'DUMMY0')
            initial_watch=list_status('ScaleWatched')
            notify=[r for r in rows(native) if r['direction']=='out' and
                r['text'].startswith('Echo FVWM_FORM_LIST '+watch+'_start ')]
            assert len(notify)==1 and not changed(watch),notify
            watched_field=view.children(watched)[0]
            watched_list=view.children(watched)[-1]
            click(watched_field); key(watched,'Tab'); key(watched,'Home')
            assert not changed(watch)
            key(watched,'Down'); key(watched,'space'); key(watched,'Down',shift=True)
            burst=list_status('ScaleWatched')
            assert burst['focus']==2 and burst['selected']==[1,2],burst
            assert len(changed(watch))==1
            key(watched,'Down'); observed_only=list_status('ScaleWatched')
            assert observed_only['focus']==3 and len(changed(watch))==1
            wrong_request='wrong_'+os.urandom(4).hex()
            command('SendToModule ScaleWatched ListWatchStatus picks '+watch+'_retired '+wrong_request)
            key(watched,'Down'); wrong_watch=list_status('ScaleWatched')
            assert wrong_watch['focus']==4 and len(changed(watch))==1
            assert not any(r['direction']=='out' and r['text'].startswith(
                'Echo FVWM_FORM_LIST '+wrong_request+' ') for r in rows(native))
            command('SendToModule ScaleWatched ListWatchStatus picks '+watch+' invalid.request')
            key(watched,'Down'); malformed_watch=list_status('ScaleWatched')
            assert malformed_watch['focus']==5 and len(changed(watch))==1
            assert not any(r['direction']=='out' and r['text'].startswith(
                'Echo FVWM_FORM_LIST invalid.request ') for r in rows(native))
            acknowledged=watch_status('ScaleWatched',watch)
            assert acknowledged['packet']==malformed_watch['packet']
            key(watched,'Down'); after_ack=list_status('ScaleWatched')
            assert after_ack['focus']==6 and len(changed(watch))==2
            # Publishing a new generation while dirty keeps the latch and
            # reports the latest snapshot, without manufacturing interaction.
            command('SendToModule ScaleWatched ListBegin picks 1 2')
            for index in range(32):
                command('SendToModule ScaleWatched ListEntry picks 2 %d "entry-%02d" file keep' %
                    (index+1,index))
            command('SendToModule ScaleWatched ListCommit picks 2')
            command('SendToModule ScaleWatched ListFocus picks 2 1')
            published=list_status('ScaleWatched')
            assert published['generation']==2 and published['focus']==0
            assert published['selected']==[1,2] and len(changed(watch))==2
            current_ack=watch_status('ScaleWatched',watch)
            assert current_ack['packet']==published['packet']
            # Clamped Home and metadata replay/redraw cannot dirty the list.
            key(watched,'Home'); key(watched,'F9'); list_status('ScaleWatched')
            assert len(changed(watch))==2
            assert len([r for r in rows(native) if r['direction']=='out' and
                r['text'].startswith('Echo FVWM_FORM_LIST '+watch+'_start ')])==1
            click(watched_list,button=5); wheel_watch=list_status('ScaleWatched')
            assert wheel_watch['packet']==published['packet'] and len(changed(watch))==3
            watch_status('ScaleWatched',watch)
            assert sorted(view.children(watched))==watched_ids
            evidence['native_change_latch']=dict(initial=initial_watch,notify=notify,burst=burst,
                observer_only=observed_only,wrong_watch=wrong_watch,malformed_watch=malformed_watch,
                acknowledged=acknowledged,
                post_ack=after_ack,published=published,current_ack=current_ack,wheel=wheel_watch,
                markers=changed(watch),children=watched_ids)
            kill_form('ScaleWatched')
            legacy_started=time.monotonic()
            legacy,legacy_ids=form('ScaleWatchOff')
            click(view.children(legacy)[0]); key(legacy,'Tab'); key(legacy,'Down'); key(legacy,'space')
            legacy_packet=list_status('ScaleWatchOff')
            assert legacy_packet['focus']==1 and legacy_packet['selected']==[1]
            assert sorted(view.children(legacy))==legacy_ids
            # A new opt-out lifetime cannot acquire another list's watch.
            assert not any(r['time']>=legacy_started and r['direction']=='out' and
                r['text'].startswith('Echo FVWM_FORM_CHANGED ') for r in rows(native))
            evidence['native_change_latch']['legacy_off']=legacy_packet
            kill_form('ScaleWatchOff')

            # An empty timeout's live clock must not restart during adoption.
            clock_token='VIEWPORT_CLOCK_'+profile
            clock_start=time.monotonic()
            clock,clock_ids=form('ScaleClock',['Timeout 40 "Echo '+clock_token+'" ""'])
            clock_ready=ready('ScaleClock',clock,4,'DUMMY0')
            assert sorted(view.children(clock))==clock_ids
            wait_for_predicate('clock approaches original deadline',lambda: time.monotonic()>=clock_start+2.5,
                clock_start+3.5,process=core)
            move(clock,'DUMMY1'); ready('ScaleClock',clock,5,'DUMMY1')
            assert sorted(view.children(clock))==clock_ids
            move(clock,'DUMMY0'); ready('ScaleClock',clock,4,'DUMMY0')
            assert sorted(view.children(clock))==clock_ids
            def clock_finished():
                found=[r for r in rows(native) if r['direction']=='out' and r['text']=='Echo '+clock_token]
                return found[-1] if found else None
            fired=wait_for_predicate('original empty timeout deadline',clock_finished,clock_start+5.5,process=core)
            assert fired['time']-clock_start>=3,dict(started=clock_start,receipt=fired)
            evidence['empty_timeout_clock']=dict(initial=clock_ready,elapsed=fired['time']-clock_start,receipt=fired)
            # Ordinary Timeout handling withdraws the Form and sends its quit
            # notification. Identity belongs to the pre-expiry adoption proof;
            # the expired window must follow that established close lifecycle.
            wait_for_predicate('ordinary Timeout withdraws its Form',lambda:not view.named('ScaleClock'),
                time.monotonic()+5,process=core)
            clock_owner=[json.loads(p.read_text()) for p in native.glob('module-*.json')
                if 'ScaleClock' in json.loads(p.read_text())['arguments']]
            assert len(clock_owner)==1,clock_owner
            clock_wait=native/('stopped-%d.json' % clock_owner[0]['proxy'])
            wait_for_predicate('ordinary Timeout child Wait receipt',clock_wait.exists,
                time.monotonic()+5,process=core)
            waited=json.loads(clock_wait.read_text())
            assert waited['child']==clock_owner[0]['child'] and waited['returncode']==0,waited
            evidence['empty_timeout_clock']['ordinary_child_wait']=waited
            clock_wire=native/('module-%d.jsonl' % clock_owner[0]['proxy'])
            terminal=[json.loads(line) for line in clock_wire.read_text().splitlines()
                if json.loads(line).get('direction')=='out' and json.loads(line).get('continuation')==0]
            assert terminal and terminal[-1]['text']=='NOP UNLOCK',terminal
            evidence['empty_timeout_clock']['native_terminal_continuation']=terminal[-1]

            # A genuine pending collection prevents font publication; only its
            # latest desired monitor factor can adopt after the real Abort.
            pending,pending_ids=form('ScalePending',label_viewport=True)
            initial=ready('ScalePending',pending,4,'DUMMY0'); packet=list_status('ScalePending')
            pending_field=view.children(pending)[0]
            pending_labels=label_raster(pending,pending_field)
            pending_parent=geometry(pending)
            pending_boxes={i:geometry(i) for i in pending_ids}
            command('SendToModule ScalePending ListBegin picks 1 2')
            physical(8); move(pending,'DUMMY1')
            deferred8=wait_for_predicate('pending collection defers 2x',lambda:
                (s if s['state']=='deferred' and s['current_q']==4 and s['desired_q']==8 else None)
                if (s:=scale('ScalePending')) else None,time.monotonic()+8,process=core)
            physical(5)
            deferred5=wait_for_predicate('pending collection retains latest 1.25x intent',lambda:
                (s if s['state']=='deferred' and s['current_q']==4 and s['desired_q']==5 else None)
                if (s:=scale('ScalePending')) else None,time.monotonic()+8,process=core)
            assert deferred8['resource_gen']==deferred5['resource_gen']==initial['resource_gen']
            assert int(deferred5['desired_epoch'],16)>int(deferred8['desired_epoch'],16)
            assert deferred5['current_epoch']==deferred8['current_epoch']==initial['current_epoch']
            assert label_raster(pending,pending_field,pending_labels)==pending_labels
            assert {i:geometry(i) for i in pending_ids}==pending_boxes
            deferred_parent=geometry(pending)
            command('SendToModule ScalePending ListAbort picks 2')
            adopted=ready('ScalePending',pending,5,'DUMMY1')
            assert list_status('ScalePending')['packet']==packet['packet']
            assert sorted(view.children(pending))==pending_ids
            replay_started=time.monotonic(); key(pending,'F9')
            wait_for_predicate('actual same-epoch metadata replay',lambda:any(r['time']>=replay_started and
                r['direction']=='in' and r['text'].startswith('MonitorUI 1 END '+adopted['current_epoch'])
                for r in rows(native)),time.monotonic()+6,process=core)
            replayed=ready('ScalePending',pending,5,'DUMMY1')
            assert replayed['current_epoch']==adopted['current_epoch'] and \
                replayed['resource_gen']==adopted['resource_gen']
            assert list_status('ScalePending')['packet']==packet['packet']
            command('SendToModule ScalePending ListLayout picks 1 3 18 8')
            assert list_status('ScalePending')['generation']==3
            evidence['pending_latest_target']=dict(initial=initial,deferred8=deferred8,deferred5=deferred5,
                adopted=adopted,replayed=replayed,collection_generation_preserved=True,next_layout_generation=3,
                label_viewport_deferred_raster=pending_labels,
                before_parent_geometry=pending_parent,deferred_parent_geometry=deferred_parent)
            capture('scale-pending-latest.png')
            kill_form('ScalePending')

            # This private unmanaged Form isolates actual minimum adoption.
            # The observer owns ResizeRedirect on its real window; requests
            # remain real X events, and no ConfigureNotify is synthesized.
            command('Style ScaleWaiting Unmanaged')
            waiting,waiting_ids=form('ScaleWaiting',['Geometry +100+100','Line left',
                'Label wide 90 "A wide fixed label requires a larger actual parent before adoption"'],
                label_viewport=True)
            waiting_before=ready('ScaleWaiting',waiting,4,'DUMMY0')
            assert client.parent(waiting)==client.root
            waiting_boxes={i:geometry(i) for i in waiting_ids}
            waiting_field=view.children(waiting)[0]
            waiting_labels=label_raster(waiting,waiting_field)
            waiting_packet=list_status('ScaleWaiting'); old_geometry=geometry(waiting)
            old_mask=view.attributes(waiting).your_event_mask
            before_errors=len(errors.unexpected)
            x.XSelectInput(d,waiting,old_mask|(1<<18)); x.XSync(d,0)
            assert len(errors.unexpected)==before_errors,errors.unexpected
            held_requests=[]

            def resize_requests():
                values=[]; event=ResizeEvent()
                while x.XCheckTypedWindowEvent(d,waiting,25,C.byref(event)):
                    assert len(values)<1024
                    e=event.resize
                    assert e.type==25 and not e.send_event and e.window==waiting and e.width>0 and e.height>0
                    values.append(dict(serial=e.serial,window=e.window,width=e.width,height=e.height))
                return values

            try:
                x.XMoveWindow(d,waiting,2660,100); x.XSync(d,0)
                waiting5=wait_for_predicate('real resize redirection holds 1.25x adoption',lambda:
                    (s if s['state']=='waiting' and s['current_q']==4 and s['desired_q']==5 and
                        (s['required_w']>old_geometry[2] or s['required_h']>old_geometry[3]) else None)
                    if (s:=scale('ScaleWaiting')) else None,time.monotonic()+8,process=core)
                first_requests=wait_for_predicate('actual first ResizeRequest witness',resize_requests,
                    time.monotonic()+5,process=core)
                assert len(first_requests)==1,first_requests
                held_requests.extend(first_requests)
                assert first_requests[-1]['width']==max(old_geometry[2],waiting5['required_w'])
                assert first_requests[-1]['height']==max(old_geometry[3],waiting5['required_h'])
                physical(6)
                waiting6=wait_for_predicate('newer target supersedes held minimum',lambda:
                    (s if s['state']=='waiting' and s['current_q']==4 and s['desired_q']==6 and
                        int(s['desired_epoch'],16)>int(waiting5['desired_epoch'],16) else None)
                    if (s:=scale('ScaleWaiting')) else None,time.monotonic()+8,process=core)
                latest_requests=wait_for_predicate('actual newer ResizeRequest witness',resize_requests,
                    time.monotonic()+5,process=core)
                assert len(latest_requests)==1,latest_requests
                held_requests.extend(latest_requests); latest=latest_requests[-1]
                assert latest['width']==max(old_geometry[2],waiting6['required_w'])
                assert latest['height']==max(old_geometry[3],waiting6['required_h'])
                assert geometry(waiting)[2:]==old_geometry[2:]
                assert {i:geometry(i) for i in waiting_ids}==waiting_boxes
                assert label_raster(waiting,waiting_field,waiting_labels)==waiting_labels
                assert list_status('ScaleWaiting')['packet']==waiting_packet['packet']
                assert waiting6['resource_gen']==waiting_before['resource_gen']
                for held in (waiting5,waiting6):
                    assert held['current_epoch']==waiting_before['current_epoch']
                    assert held['current_atom']==waiting_before['current_atom']==atoms['DUMMY0']
                    assert held['desired_atom']==atoms['DUMMY1']
                quiet=time.monotonic()+.15
                wait_for_predicate('bounded no-resize-retry observation',lambda:time.monotonic()>=quiet,
                    quiet+1,process=core)
                command('Echo viewport-waiting-core-alive')
                assert not resize_requests(),'same target repeated its resize request'
                x.XSelectInput(d,waiting,old_mask); x.XSync(d,0)
                assert geometry(waiting)[2:]==old_geometry[2:],'removing redirection replayed a held request'
                x.XResizeWindow(d,waiting,latest['width'],latest['height']); x.XSync(d,0)
                waiting_adopted=ready('ScaleWaiting',waiting,6,'DUMMY1')
                assert sorted(view.children(waiting))==waiting_ids
                assert list_status('ScaleWaiting')['packet']==waiting_packet['packet']
                evidence['minimum_waiting_latest']=dict(unmanaged_fixture=True,before=waiting_before,
                    held5=waiting5,held6=waiting6,actual_resize_requests=held_requests,
                    quiet_seconds=.15,adopted=waiting_adopted,synthetic_configure_events=False)
                evidence['minimum_waiting_latest']['label_viewport_held_raster']=waiting_labels
                capture('scale-minimum-real-configure.png')
            finally:
                x.XSelectInput(d,waiting,old_mask); x.XSync(d,0)
            kill_form('ScaleWaiting'); physical(5)

            # Personal Forms do not opt in; unsupported compositions retain a
            # complete, diagnostic 1x presentation on the scaled output.
            for alias,enabled,extra,state_name in [('ScaleLegacy',False,[],'disabled'),
                ('ScaleUnsupported',True,['Line left','Selection option single','Choice pick yes on "Legacy choice"'],'unsupported'),
                ('ScaleBitmap',True,['Font fixed','InputFont fixed','ButtonFont fixed'],'ready')]:
                window,ids=form(alias,extra,enabled)
                before=scale(alias); before_boxes={i:geometry(i) for i in ids}
                viewport=max(ids,key=lambda i:before_boxes[i][3])
                move(window,'DUMMY1')
                after=wait_for_predicate('native '+state_name,lambda:
                    (s if s['state']==state_name else None) if (s:=scale(alias)) else None,
                    time.monotonic()+8,process=core)
                assert sorted(view.children(window))==ids
                if state_name!='ready':
                    assert after['current_q']==4
                    assert all(geometry(i)[2:]==before_boxes[i][2:] for i in ids if i!=viewport)
                else:
                    after=ready(alias,window,5,'DUMMY1')
                evidence.setdefault('fallbacks',[]).append(dict(alias=alias,before=before,after=after,
                    before_boxes=before_boxes,after_boxes={i:geometry(i) for i in ids},
                    bitmap_glyph_growth_claimed=False))
                if state_name in ('unsupported','ready'): capture(alias+'-fallback.png')
                kill_form(alias)

            # One existing viewport Form, with an opted-in Label followed by
            # the ordinary fixed Label control. Actual resize/Configure cycles
            # must expand only the opted-in raster and must never ratchet the
            # configured minimum. Keep source text longer than narrow space.
            ascii_label='READABLE '+('Wm-12345 '*24)+'original-tail'
            unicode_label='READABLE '+('café Δ 漢字 '*24)+'original-tail'
            for alias,fonts in [('ScaleReadable',[]),('ScaleReadableBitmap',
                ['Font fixed','InputFont fixed','ButtonFont fixed'])]:
                command('Style '+alias+' PositionPlacement screen DUMMY0 100p 100p, StartsOnScreen DUMMY0')
                window,ids=form(alias,[*fonts,'Line left',
                    'Label fit 12 "'+ascii_label+'"','LabelViewport fit'])
                ordered=view.children(window)
                assert len(ordered)==4,ordered
                field,collection=ordered[0],ordered[-1]
                assert geometry(field)[1]<geometry(collection)[1]
                packet=list_status(alias)
                samples=[]
                for q in (4,5,6,8):
                    move(window,'DUMMY0'); ready(alias,window,4,'DUMMY0')
                    physical(q); move(window,'DUMMY1')
                    initial=ready(alias,window,q,'DUMMY1')
                    minimum=[initial['min_w'],initial['min_h']]
                    # A fresh same-q baseline follows actual minimum sizing,
                    # since the preceding larger factor may own a larger frame.
                    x.XResizeWindow(d,window,*minimum); x.XSync(d,0)
                    wait_for_predicate('actual narrow label parent',lambda:
                        geometry(window)[2:]==minimum,time.monotonic()+5,process=core)
                    narrow_status=ready(alias,window,q,'DUMMY1')
                    narrow_boxes={i:geometry(i) for i in ids}
                    values=[]
                    for value in (ascii_label,unicode_label):
                        previous=label_pixels(window,field)
                        command('SendToModule '+alias+' SetLabel fit "'+value+'"')
                        # Changing the ordinary Label supplies a default-off
                        # control with the same long source and font.
                        command('SendToModule '+alias+' SetLabel title "'+value+'"')
                        scale(alias)  # Ordered public reply after both updates.
                        narrow=wait_for_predicate('actual narrow label repaint',lambda:
                            (s if s!=previous else None) if (s:=label_pixels(window,field)) else None,
                            time.monotonic()+5,process=core)
                        wide_size=[min(2400,minimum[0]+900),minimum[1]]
                        assert wide_size[0]>minimum[0]+200,(minimum,wide_size)
                        x.XResizeWindow(d,window,*wide_size); x.XSync(d,0)
                        wait_for_predicate('actual wide label parent',lambda:
                            geometry(window)[2:]==wide_size,time.monotonic()+5,process=core)
                        wide=wait_for_predicate('available parent width reaches label raster',lambda:
                            (s if s['bands'][0]['bounds'][2]>narrow['bands'][0]['bounds'][2]+100
                                else None) if (s:=label_pixels(window,field)) else None,
                            time.monotonic()+5,process=core)
                        assert wide['bands'][1]['bounds']==narrow['bands'][1]['bounds'],(narrow,wide)
                        assert wide['bands'][1]['ink_sha256']==narrow['bands'][1]['ink_sha256']
                        margin=(10*q+2)//4
                        assert wide['bands'][0]['bounds'][2]<wide_size[0]-margin,wide
                        wide_status=ready(alias,window,q,'DUMMY1')
                        assert [wide_status['min_w'],wide_status['min_h']]==minimum
                        if alias=='ScaleReadable' and value==unicode_label and q==8:
                            capture('readability-wide-2x.png')
                        x.XResizeWindow(d,window,*minimum); x.XSync(d,0)
                        wait_for_predicate('actual original parent shrink',lambda:
                            geometry(window)[2:]==minimum,time.monotonic()+5,process=core)
                        shrunk=wait_for_predicate('same-q retained text raster after shrink',lambda:
                            (s if s==narrow else None) if (s:=label_pixels(window,field)) else None,
                            time.monotonic()+5,process=core)
                        assert {i:geometry(i) for i in ids}==narrow_boxes
                        values.append(dict(source_utf8_sha256=hashlib.sha256(value.encode()).hexdigest(),
                            source_bytes=len(value.encode()),narrow=narrow,wide=wide,shrunk=shrunk,
                            wide_status=wide_status))
                    final=ready(alias,window,q,'DUMMY1')
                    assert final['resource_gen']==narrow_status['resource_gen']
                    assert [final['min_w'],final['min_h']]==minimum
                    assert list_status(alias)['packet']==packet['packet']
                    assert sorted(view.children(window))==ids
                    caret=caret_pixels(window,field,q)
                    focus=focus_pixels(alias,window,field,collection,q)
                    assert list_status(alias)['packet']==packet['packet']
                    samples.append(dict(quarters=q,status=final,minimum=minimum,
                        labels=values,caret=caret,focus=focus,children=ids,packet=packet))
                evidence.setdefault('readability',[]).append(dict(alias=alias,samples=samples,
                    bitmap_glyph_growth_claimed=False,
                    ellipsis_identity='pure actual-font fit policy plus separate manual capture review',
                    source_retention='same-q original-width raster restoration after SetLabel and real resize'))
                kill_form(alias)
            physical(5)

            if args.fault_form:
                assert not arm.exists()
                unsupported,unsupported_ids=form('ScaleUnsupportedASAN',['Line left',
                    'Selection option single','Choice pick yes on "Legacy choice"'],
                    binary='ViewportScaleFaultForm')
                unsupported_state=wait_for_predicate('sanitized unsupported composition remains complete 1x',lambda:
                    (s if s['state']=='unsupported' and s['current_q']==4 else None)
                    if (s:=scale('ScaleUnsupportedASAN')) else None,time.monotonic()+8,process=core)
                move(unsupported,'DUMMY1')
                assert scale('ScaleUnsupportedASAN')['current_q']==4
                assert sorted(view.children(unsupported))==unsupported_ids
                evidence['unsupported_sanitized']=dict(status=unsupported_state,arm_present=False)
                kill_form('ScaleUnsupportedASAN','ViewportScaleFaultForm')

            if args.fault_form:
                # The preceding real focus probes leave the pointer on the
                # scaled output. This fixture's baseline explicitly owns 1x.
                command('Style ScaleFontFault PositionPlacement screen DUMMY0 100p 100p, StartsOnScreen DUMMY0')
                fault,ids=form('ScaleFontFault',['Font "xft:Sans:size=10"',
                    'InputFont "xft:DejaVu Sans Mono:size=11"','ButtonFont "xft:Sans:size=12"'],
                    binary='ViewportScaleFaultForm',label_viewport=True)
                fault_first_map=scale('ScaleFontFault')
                # Placement hints do not establish the actual starting monitor.
                # The fault proof begins only after this observed baseline move.
                move(fault,'DUMMY0')
                old=ready('ScaleFontFault',fault,4,'DUMMY0'); boxes={i:geometry(i) for i in ids}
                field=min(ids,key=lambda i:geometry(i)[1]); type_text(fault,field,'retained-font-draft')
                fault_labels=label_raster(fault,field)
                fault_packet=list_status('ScaleFontFault')
                arm.touch(); move(fault,'DUMMY1')
                refused=wait_for_predicate('injected acquisition failure is atomic',lambda:
                    (s if s['state']=='refused' and s['current_q']==4 and s['desired_q']==5 else None)
                    if (s:=scale('ScaleFontFault')) else None,time.monotonic()+8,process=core)
                assert refused['resource_gen']==old['resource_gen']
                refused_boxes={i:geometry(i) for i in ids}
                refused_packet=list_status('ScaleFontFault')
                assert refused_boxes==boxes,(boxes,refused_boxes)
                assert refused_packet['packet']==fault_packet['packet'],(fault_packet,refused_packet)
                assert label_raster(fault,field,fault_labels)==fault_labels
                injected=json.loads(Path(str(arm)+'.failed').read_text()); assert injected['attempt']==2
                owned=[json.loads(p.read_text()) for p in native.glob('module-*.json')
                    if 'ScaleFontFault' in json.loads(p.read_text())['arguments']]
                assert len(owned)==1 and injected['pid']==owned[0]['child'] and injected['quarters']==5
                capture('scale-font-refused.png')
                key(fault,'f',alt=True); raw=receipt('VIEWPORT_DRAFT ScaleFontFault ')
                assert base64.urlsafe_b64decode(raw.split()[2][1:]+'==')==b'retained-font-draft',raw
                arm.unlink(); move(fault,'DUMMY0'); ready('ScaleFontFault',fault,4,'DUMMY0')
                move(fault,'DUMMY1'); recovered=ready('ScaleFontFault',fault,5,'DUMMY1')
                assert sorted(view.children(fault))==ids
                evidence['font_acquisition_failure']=dict(before=old,refused=refused,injection=injected,
                    first_map_status=fault_first_map,baseline_move='actual root-pixel move to DUMMY0',
                    draft_receipt=raw,recovered=recovered,parent_dimensions_core_owned=True,
                    before_child_geometry=boxes,refused_child_geometry=refused_boxes,
                    before_packet=fault_packet,refused_packet=refused_packet,
                    label_viewport_refused_raster=fault_labels)
                capture('scale-font-recovered.png'); kill_form('ScaleFontFault','ViewportScaleFaultForm')
            else:
                evidence['font_acquisition_failure']={'status':'not-run','reason':'no test wrapper supplied'}

            serial=0
            def rpc(scope='state',**fields):
                nonlocal serial
                serial+=1; token='vs-'+str(serial)
                rpc_peer.send(frame(token,resource='files',**fields,
                    **({'scope':scope} if 'provider' in fields else {})))
                reply=rpc_peer.record()['reply']; assert reply['id']==token,reply
                if reply['status']=='error' and reply['error']['code']=='not_ready': return None
                assert reply['status']=='ok',reply
                return reply['result']

            browser=runtime.start('browser',[str(build/'bin/FvwmPrompt/fvwm-file-browser'),
                '--socket',str(endpoint),'--dir',str(folder),'--view','list','--watch','off',
                '--opener','/bin/true','--terminal','/bin/true'],'browser.log',env)
            providers=wait_for_predicate('browser provider',lambda: rpc()['providers'],time.monotonic()+10,process=browser)
            provider=providers[0]['provider']
            def browser_ready():
                s=rpc(provider=provider)
                return s if s and s['availability']['ready'] and not s['busy'] else None
            state=wait_for_predicate('full browser snapshot',browser_ready,time.monotonic()+12,process=browser)
            assert state['loaded']==ENTRIES
            window=view.wait('File browser'); hint=ClassHint(); assert x.XGetClassHint(d,window,C.byref(hint))
            try: alias=C.string_at(hint.name).decode()
            finally:
                if hint.name: x.XFree(hint.name)
                if hint.class_: x.XFree(hint.class_)
            first_map=ready(alias,window,5,'DUMMY1')
            # OpenWindows creates one child per declared Input/List/Button,
            # in declaration order, and never raises individual controls.
            # XQueryTree supplies that bottom-to-top creation order. A legal
            # one-row viewport can be shorter than an Input, so dimensions do
            # not identify control types. Later callbacks/hits prove semantics.
            ordered_children=view.children(window)
            ids=sorted(ordered_children); assert len(ids)<=64 and len(set(ids))==len(ids)
            owners.update({child:window for child in [window,*ids]})
            declarations=[]
            pattern=re.compile(r'^\*'+re.escape(alias)+r':\s*(InputHex|Input|List|Button)\s+(.+)$')
            for row in rows(native):
                if row['direction']!='out': continue
                match=pattern.fullmatch(row['text'])
                if match:
                    kind,arguments=match.groups()
                    declarations.append(dict(kind=kind,name=arguments.split()[0]
                        if kind!='Button' else None,raw=row['text']))
            assert len(declarations)==len(ordered_children),(declarations,ordered_children)
            mapping=[dict(declaration,window=child,geometry=geometry(child))
                for declaration,child in zip(declarations,ordered_children)]
            named={item['name']:item['window'] for item in mapping if item['name'] is not None}
            assert set(named)=={'filter','picks','path'} and len(set(named.values()))==3,mapping
            filter_input,list_window,path_input=[named[name] for name in ('filter','picks','path')]
            assert geometry(filter_input)[1]<geometry(list_window)[1]<geometry(path_input)[1],mapping
            evidence['browser_first_map_scaled']=dict(status=first_map,window=window,children=ids,
                control_declaration_mapping=mapping,
                mapping_basis='original native config declaration order and XQueryTree child creation order')
            lifetime=int(alias.rsplit('-',2)[1])
            browser_watch=provider+'_f'+format(lifetime,'x')
            assert any(r['direction']=='out' and r['text']=='*'+alias+': ListWatch '+browser_watch
                for r in rows(native))

            def feedback_roundtrip(since,packet):
                def observed():
                    actual=[r for r in rows(native) if r['time']>=since and r['direction']=='out']
                    markers=changed(browser_watch,since)
                    requests=[r for r in actual if r['text'].startswith('SendToModule '+alias+
                        ' ListWatchStatus picks '+browser_watch+' ')]
                    completed=[]
                    for request in requests:
                        token=request['text'].split()[-1]
                        assert re.fullmatch(re.escape(provider)+r'_w[1-9a-f][0-9a-f]*',token),request
                        replies=[r for r in actual if r['time']>=request['time'] and
                            r['text'].startswith('Echo FVWM_FORM_LIST '+token+' ')]
                        if not replies: continue
                        reply=list_reply(replies[-1]['text'][5:],token)
                        causal=[m for m in markers if m['time']<=request['time']]
                        renewed=[r for r in actual if r['time']>=replies[-1]['time'] and
                            r['text']=='SendToModule '+alias+' RenewTimeout 3000']
                        # The post-snapshot renewal proves the consumer has
                        # finished this ACK, rather than exposing an older
                        # dirty ACK still racing a later input/observer.
                        if causal and renewed and reply['packet']==packet:
                            completed.append(dict(marker=causal[-1],request=request,
                                reply=reply,renewed=renewed[0]))
                    return completed[-1] if completed else None
                return wait_for_predicate('native interaction wakeup and exact watch snapshot',observed,
                    time.monotonic()+8,process=browser)

            def feedback_only(since,before,selected=None,hidden=0,visible=ENTRIES,
                    unavailable=False,readiness=False):
                # Observe public labels BEFORE querying the provider: a query
                # itself snapshots native state and cannot prove notification.
                expected={}
                if selected is not None:
                    expected['counts']='Selected %d; hidden %d; limit 256; visible %d/%d' % (
                        selected,hidden,visible,ENTRIES)
                if unavailable:
                    expected['readiness']='Selection unavailable; adjust selection or retry an action.'
                    expected['status']='Selection unavailable: native selection exceeds limit'
                elif readiness:
                    expected['readiness']='Open %d; Properties %s; Terminal yes; Custom no' % (
                        visible,'yes' if selected else 'no')
                def labels_observed():
                    actual=[r for r in rows(native) if r['time']>=since and r['direction']=='out']
                    found={name:[r for r in actual if r['text']=='SendToModule '+alias+
                        ' SetLabel '+name+' "'+value+'"'] for name,value in expected.items()}
                    return {name:items[-1] for name,items in found.items()} if all(found.values()) else None
                labels=wait_for_predicate('immediate native selection feedback labels',labels_observed,
                    time.monotonic()+8,process=browser) if expected else {}
                after=list_status(alias)
                transaction=feedback_roundtrip(since,after['packet'])
                assert after['generation']==before['generation']
                assert sorted(view.children(window))==ids
                actual=[r for r in rows(native) if r['time']>=since and r['direction']=='out']
                forbidden=[r for r in actual if r['text'].startswith((
                    'Echo FVWM_FILES_3 '+provider+' ', 'Echo FVWM_FILES_RENDER '+provider+' ',
                    'Echo FVWM_FILES_WATCH '+provider+' ', 'Module FvwmForm '+alias,
                    'KillModule FvwmForm '+alias, 'SendToModule '+alias+' ListBegin ',
                    'SendToModule '+alias+' ListCommit ', 'SendToModule '+alias+' ListLayout ',
                    'SendToModule '+alias+' ListFocus '))]
                assert not forbidden,forbidden
                label_commands=[r for r in actual if r['text'].startswith('SendToModule '+alias+' SetLabel ')]
                assert all(r['text'].split()[3] in ('counts','readiness','status') for r in label_commands)
                result=dict(since=since,transaction=transaction,labels=labels,all_labels=label_commands,
                    before=before,after=after,children=ids,no_action_or_publication=True,
                    provider_queried_before_label_observation=False)
                evidence.setdefault('immediate_feedback',[]).append(result)
                return result

            def feedback_capture(name):
                if profile!='classic-large':
                    capture(name)
                    return
                unfenced_status=scale(alias)
                time.sleep(.05)
                unfenced=label_raster(window,filter_input)
                unfenced_name=name.removesuffix('.png')+'-before-fence.png'
                capture(unfenced_name)
                initial=ready(alias,window,8,'DUMMY1')
                packet=list_status(alias)
                transitions=[]
                previous=initial
                for width in (338,339):
                    mutate(['--output','DUMMY1','--set','WIDTH_MM',str(width)],
                        'classic-feedback-paint-fence-'+str(width)+'-'+str(len(evidence['operations'])))
                    server=wait_for_authoritative_state(runtime,display,
                        'actual same-factor paint-fence millimetres',lambda s:
                            s['monitors']['DUMMY1']['mm_width']==width and
                            s['monitors']['DUMMY1']['mm_height']==286,
                        time.monotonic()+6,'paint-fence-'+str(width))
                    def observed_epoch():
                        current=ready(alias,window,8,'DUMMY1')
                        return current if (int(current['current_epoch'],16)>
                            int(previous['current_epoch'],16)) else None
                    current=wait_for_predicate('producer round trip before newer same-factor epoch',
                        observed_epoch,time.monotonic()+10,process=browser)
                    assert current['resource_gen']==initial['resource_gen']
                    assert list_status(alias)['packet']==packet['packet']
                    assert sorted(view.children(window))==ids
                    transitions.append(dict(server=server,status=current))
                    previous=current
                # Same-factor adoption takes form_scale_retry's observed path:
                # its producer X round trips flush preceding label drawing,
                # without replacing fonts or repairing the frame by redraw.
                first=label_raster(window,filter_input)
                capture(name)
                time.sleep(.05)
                second=label_raster(window,filter_input)
                quiet_name=name.removesuffix('.png')+'-quiet.png'
                capture(quiet_name)
                evidence.setdefault('feedback_paint_fences',[]).append(dict(
                    screenshots=[unfenced_name,name,quiet_name],unfenced_status=unfenced_status,
                    unfenced_quiet_raster=unfenced,initial=initial,native_packet=packet,
                    transitions=transitions,first_header=first,quiet_header=second,
                    quiet_seconds=.05,children=ids))
                assert unfenced==first==second,(unfenced,first,second)

            capture('browser-first-map-1.25.png')
            move(window,'DUMMY0'); baseline_scale=ready(alias,window,4,'DUMMY0')
            click(filter_input); key(window,'Tab'); key(window,'Home')
            single_before=list_status(alias); single_started=time.monotonic(); key(window,'space')
            single=feedback_only(single_started,single_before,1,readiness=True)
            selected_state=rpc(provider=provider)
            assert selected_state['selected']==1 and selected_state['availability']['properties']
            assert selected_state['progress']==state['progress'] and selected_state['bindings']==state['bindings']
            evidence['immediate_feedback'][-1]['provider_after_labels']=selected_state
            capture('browser-immediate-selection-available.png')
            clear_started=time.monotonic(); key(window,'space')
            empty_selection=feedback_only(clear_started,single['after'],0,readiness=True)
            empty_state=rpc(provider=provider)
            assert empty_state['selected']==0 and not empty_state['availability']['properties']
            evidence['immediate_feedback'][-1]['provider_after_labels']=empty_state
            range_started=time.monotonic()
            for _ in range(3): key(window,'Down',shift=True)
            key(window,'space')
            before_range=wait_for_predicate('native range setup',lambda:
                (s if s['focus']==3 and s['selected']==[0,1,2] else None)
                if (s:=list_status(alias)) else None,time.monotonic()+5,process=browser)
            feedback_only(range_started,empty_selection['after'],3)
            range_state=rpc(provider=provider)
            assert range_state['selected']==3 and range_state['availability']['properties']
            evidence['immediate_feedback'][-1]['provider_after_labels']=range_state
            capture('browser-immediate-range-counts.png')
            heights={4:geometry(filter_input)[3]}
            for q in (5,6,8,5,8):
                move(window,'DUMMY0'); ready(alias,window,4,'DUMMY0')
                server=physical(q)
                move(window,'DUMMY1'); current=ready(alias,window,q,'DUMMY1')
                assert list_status(alias)['packet']==before_range['packet']
                assert sorted(view.children(window))==ids
                heights[q]=geometry(filter_input)[3]
                evidence.setdefault('browser_moves',[]).append(dict(status=current,server=server,
                    filter_geometry=geometry(filter_input),list_geometry=geometry(list_window)))
                if q==8 and len(evidence['browser_moves'])==3:
                    original_size=geometry(window)[2:]
                    original_boxes={i:geometry(i) for i in ids}
                    narrow_size=[current['min_w'],original_size[1]]
                    wide_size=[min(2400,narrow_size[0]+500),original_size[1]]
                    assert wide_size[0]>narrow_size[0]+100,(narrow_size,wide_size)
                    sizes=[]
                    for name,size in [('narrow',narrow_size),('wide',wide_size),('shrink',original_size)]:
                        x.XResizeWindow(d,window,*size); x.XSync(d,0)
                        wait_for_predicate('actual browser '+name+' width',lambda:
                            geometry(window)[2:]==size,time.monotonic()+5,process=browser)
                        observed=ready(alias,window,q,'DUMMY1')
                        assert observed['min_w']==current['min_w'] and observed['min_h']==current['min_h']
                        assert observed['resource_gen']==current['resource_gen']
                        assert list_status(alias)['packet']==before_range['packet']
                        assert sorted(view.children(window))==ids
                        sizes.append(dict(name=name,status=observed,collection=geometry(list_window)))
                        if name!='shrink': capture('browser-readability-'+name+'-2x.png')
                    assert {i:geometry(i) for i in ids}==original_boxes
                    evidence['browser_readability_widths']=sizes
                    evidence['browser_input_carets']=[caret_pixels(window,field,q)
                        for field in (filter_input,path_input)]
                    click(filter_input); key(window,'Tab')
                    assert list_status(alias)['packet']==before_range['packet']
                if q in (6,8): capture('browser-scale-'+str(q)+'-'+str(len(evidence['browser_moves']))+'.png')
            assert heights[4]<heights[5]<heights[6]<heights[8],heights
            range_after_started=time.monotonic(); key(window,'Up',shift=True)
            range_after=wait_for_predicate('range anchor after scaling',lambda:
                (s if s['focus']==2 and s['selected']==[0,1,2] else None)
                if (s:=list_status(alias)) else None,time.monotonic()+5,process=browser)
            feedback_only(range_after_started,before_range)
            evidence['range_focus_geometry']=dict(before=before_range,after=range_after,input_heights=heights)
            home_started=time.monotonic(); key(window,'Home')
            feedback_only(home_started,range_after)
            wheel_started=time.monotonic()
            for _ in range(6): click(list_window,button=5)
            feedback_only(wheel_started,range_after)
            before_top=list_status(alias); move(window,'DUMMY0'); ready(alias,window,4,'DUMMY0')
            assert list_status(alias)['packet']==before_top['packet']
            top_started=time.monotonic(); click(list_window)
            top=wait_for_predicate('same first visible identity after scale',lambda:
                (s if s['focus']==18 else None) if (s:=list_status(alias)) else None,
                time.monotonic()+5,process=browser)
            feedback_only(top_started,before_top,4)
            evidence['top_identity']=dict(before=before_top,observed=top,
                measurement='post-scale first-cell click changes focus/selection/anchor')

            path_draft=str(runtime.root/'unsent scale ; $[literal] 100%')
            type_text(window,path_input,path_draft); type_text(window,filter_input,'entry-00')
            key(window,'a',control_key=True)
            for _ in range(3): key(window,'Right')
            draft_packet=list_status(alias)
            for monitor,q in [('DUMMY1',8),('DUMMY0',4),('DUMMY1',8)]:
                move(window,monitor); ready(alias,window,q,monitor)
                assert list_status(alias)['packet']==draft_packet['packet']
                assert sorted(view.children(window))==ids
            # Wheel input changes top without transferring input focus. Its
            # feedback must retain both unsent drafts and the insertion cursor
            # at index three; the existing literal callback proves them below.
            draft_feedback_started=time.monotonic(); click(list_window,button=5)
            feedback_only(draft_feedback_started,draft_packet)
            restore_feedback_started=time.monotonic(); click(list_window,button=4)
            feedback_only(restore_feedback_started,draft_packet)
            assert list_status(alias)['packet']==draft_packet['packet']
            key(window,'x'); key(window,'f',alt=True)
            def browser_callback(operation):
                prefix='Echo FVWM_FILES_3 '+provider+' '
                def found():
                    values=[r['text'] for r in rows(native) if r['direction']=='out' and
                        r['text'].startswith(prefix) and r['text'].split()[4]==operation]
                    return values[-1] if values else None
                return wait_for_predicate('actual '+operation+' callback',found,time.monotonic()+6,process=browser)
            filtered=browser_callback('filter')
            assert base64.urlsafe_b64decode(filtered.split()[5][1:]+'==')==b'entxry-00',filtered
            click(path_input); key(window,'g',alt=True); navigated=browser_callback('path')
            assert base64.urlsafe_b64decode(navigated.split()[5][1:]+'==')==path_draft.encode(),navigated
            evidence['draft_cursor_focus']=dict(filter_callback=filtered,path_callback=navigated,
                cursor_insertion_index=3,children_unchanged=True)
            wait_for_predicate('invalid literal draft reports unavailable',lambda:
                (s if s and not s['busy'] and not s['availability']['ready'] else None)
                if (s:=rpc(provider=provider)) else None,time.monotonic()+8,process=browser)
            key(window,'F5'); wait_for_predicate('refresh recovers scale browser',browser_ready,time.monotonic()+10,process=browser)
            key(window,'c',alt=True)
            wait_for_predicate('clear native filter',lambda:
                (s if s and s['visible']==ENTRIES else None) if (s:=browser_ready()) else None,
                time.monotonic()+10,process=browser)

            # Preserve 157 hidden identities, then select 100 visible rows in
            # one actual range input. The native packet is legal (100 <= 256)
            # but the combined model must refuse 257 without rewriting it.
            key(window,'d',alt=True)
            wait_for_predicate('clear selection before hidden limit setup',lambda:
                (s if s and s['selected']==0 else None) if (s:=browser_ready()) else None,
                time.monotonic()+8,process=browser)
            click(filter_input); key(window,'Tab'); key(window,'Home')
            hidden_before=list_status(alias); hidden_started=time.monotonic()
            for _ in range(156): key(window,'Down',shift=True)
            hidden_selected=feedback_only(hidden_started,hidden_before,157)
            assert hidden_selected['after']['selected']==list(range(157))
            type_text(window,filter_input,'entry-03'); key(window,'f',alt=True)
            hidden_state=wait_for_predicate('hidden selections survive filter publication',lambda:
                (s if s and s['visible']==100 and s['selected']==157 else None)
                if (s:=browser_ready()) else None,time.monotonic()+10,process=browser)
            click(filter_input); key(window,'Tab'); key(window,'Home')
            overflow_before=list_status(alias)
            assert not overflow_before['selected']
            overflow_started=time.monotonic(); key(window,'End',shift=True)
            overflow=feedback_only(overflow_started,overflow_before,unavailable=True)
            assert overflow['after']['focus']==99 and overflow['after']['selected']==list(range(100))
            assert not any(r['text'].startswith('SendToModule '+alias+' SetLabel counts ')
                for r in overflow['all_labels']),overflow
            feedback_capture('browser-immediate-selection-unavailable.png')
            recovery_started=time.monotonic(); key(window,'space')
            recovery=feedback_only(recovery_started,overflow['after'],256,hidden=157,visible=100,readiness=True)
            assert recovery['after']['selected']==list(range(99))
            recovered_state=rpc(provider=provider)
            assert recovered_state['selected']==256 and recovered_state['visible']==100
            assert recovered_state['availability']['properties'] and not recovered_state['busy']
            evidence['hidden_selection_refusal']=dict(hidden_state=hidden_state,overflow=overflow,
                recovery=recovery,recovered_state=recovered_state,combined_refused=257,combined_admitted=256,
                native_limit=256,provider_queries_during_refusal=0)
            feedback_capture('browser-immediate-selection-recovered.png')
            # Clear publishes a new packet generation. Wait for that action's
            # actual native publication/render before emitting Unfilter; an
            # earlier callback correctly retains its old, now stale packet.
            clear_setup_started=time.monotonic(); key(window,'d',alt=True)
            def cleared_selection_settled():
                observed=browser_ready()
                if not observed or observed['visible']!=100 or observed['selected']!=0:
                    return None
                packet=list_status(alias)
                generation=recovery['after']['generation']+1
                if packet['generation']!=generation or packet['selected']:
                    return None
                renders=[r for r in rows(native) if r['time']>=clear_setup_started and
                    r['direction']=='out' and r['text'].startswith('Echo FVWM_FILES_RENDER '+provider+' ')
                    and (fields:=r['text'].split())[3]==str(lifetime) and
                    fields[5:]==[str(generation),'0']]
                return dict(state=observed,native_packet=packet,render=renders[-1]) if renders else None
            evidence['hidden_selection_refusal']['cleared_setup']=wait_for_predicate(
                'selection clear native generation and completed render',cleared_selection_settled,
                time.monotonic()+10,process=browser)
            key(window,'c',alt=True)
            wait_for_predicate('selection limit setup clears without replacing Form',lambda:
                (s if s and s['visible']==ENTRIES and s['selected']==0 else None)
                if (s:=browser_ready()) else None,time.monotonic()+10,process=browser)
            assert sorted(view.children(window))==ids
            before_layout=list_status(alias); font_before=scale(alias)
            before_boxes={i:geometry(i) for i in ids}; before_size=geometry(window)[2:]
            started=time.monotonic(); key(window,'F7')
            def refused_layout_observed():
                actual=[r['text'] for r in rows(native)
                    if r['time']>=started and r['direction']=='out']
                layout='SendToModule %s ListLayout picks %d %d 18 8' % (
                    alias,before_layout['generation'],before_layout['generation']+1)
                unchanged_ack=[c for c in actual if
                    c.startswith('Echo FVWM_FORM_LIST '+provider+'_') and
                    c.split()[2].endswith('_layout') and c.split()[3:6]==[
                        str(before_layout['generation']),'0','0']]
                guidance=[c for c in actual if
                    c.startswith('SendToModule '+alias+' SetLabel status ') and
                    'enlarge' in c.lower() and 'retry' in c.lower()]
                return dict(command=layout,ack=unchanged_ack,status_commands=guidance) if \
                    layout in actual and unchanged_ack and guidance else None
            refusal=wait_for_predicate('real unchanged layout ACK and enlarge/retry guidance',
                refused_layout_observed,time.monotonic()+8,process=browser)
            wait_for_predicate('refused layout retains settled list view',lambda:
                (s if s and s['view']=='list' else None) if (s:=browser_ready()) else None,
                time.monotonic()+8,process=browser)
            assert list_status(alias)['packet']==before_layout['packet']
            assert {i:geometry(i) for i in ids}==before_boxes
            assert sorted(view.children(window))==ids
            # The grid adds two font lines to one list row. Two actual Input
            # heights include those lines and their padding at every profile.
            # Resize the managed client, then observe its actual Configure and
            # child reflow before retrying the original generation transition.
            addition=2*geometry(filter_input)[3]
            requested=[before_size[0],before_size[1]+addition]
            assert root_rect(window)[1]+requested[1]+40<=2160,requested
            x.XResizeWindow(d,window,*requested); x.XSync(d,0)
            wait_for_predicate('actual managed enlargement and list reflow',lambda:
                geometry(window)[2:]==requested and geometry(list_window)[2:]==[
                    before_boxes[list_window][2],before_boxes[list_window][3]+addition],
                time.monotonic()+8,process=browser)
            enlarged=ready(alias,window,8,'DUMMY1')
            assert list_status(alias)['packet']==before_layout['packet']
            assert sorted(view.children(window))==ids
            assert enlarged['resource_gen']==font_before['resource_gen']
            key(window,'F7')
            wait_for_predicate('native grid ACK after real enlargement',lambda:
                (s if s and s['view']=='grid' else None) if (s:=browser_ready()) else None,
                time.monotonic()+10,process=browser)
            after_layout=list_status(alias)
            assert after_layout['generation']==before_layout['generation']+1
            assert scale(alias)['resource_gen']==font_before['resource_gen']
            click(filter_input); key(window,'Tab'); key(window,'Home'); key(window,'Down')
            grid_stride=wait_for_predicate('scaled grid keyboard movement',lambda:
                (s if 1<s['focus']<=8 else None) if (s:=list_status(alias)) else None,
                time.monotonic()+5,process=browser)
            # Down may scroll a one-row grid to its next row. Establish
            # top zero through observed keyboard results before measuring the
            # first cell, with nonzero focus so the click must change it.
            key(window,'Home')
            grid_home=wait_for_predicate('grid Home returns to first row',lambda:
                (s if s['focus']==0 else None) if (s:=list_status(alias)) else None,
                time.monotonic()+5,process=browser)
            key(window,'Right')
            grid_right=wait_for_predicate('grid Right stays within first row',lambda:
                (s if s['focus']==1 else None) if (s:=list_status(alias)) else None,
                time.monotonic()+5,process=browser)
            click(list_window)
            grid_hit=wait_for_predicate('scaled first-cell hit geometry',lambda:
                (s if s['focus']==0 else None) if (s:=list_status(alias)) else None,
                time.monotonic()+5,process=browser)
            (folder/'entry-0000').write_text('changed metadata')
            key(window,'F5')
            refreshed=wait_for_predicate('refresh native generation',lambda:
                (s if s['generation']>after_layout['generation'] else None)
                if (s:=list_status(alias)) else None,time.monotonic()+12,process=browser)
            assert sorted(view.children(window))==ids
            ready(alias,window,8,'DUMMY1')
            evidence['layout_refresh']=dict(before=before_layout,layout=after_layout,refreshed=refreshed,
                minimum_refusal=refusal,before_size=before_size,requested_size=requested,
                actual_enlarged=enlarged,actual_grid_keyboard=grid_stride,
                actual_grid_home=grid_home,actual_grid_right=grid_right,actual_grid_hit=grid_hit)
            capture('browser-scaled-grid-refresh.png')

            old_epoch=scale(alias)['current_epoch']
            mutate(['--output','DUMMY1','--off','--fb','2560x2160','--fbmm','677x572'],'remove-scaled-monitor')
            remaining=wait_for_authoritative_state(runtime,display,'removed scaled monitor',
                lambda s:set(s['monitors'])=={'DUMMY0'},time.monotonic()+6,'removed')
            move(window,'DUMMY0')
            def removal_ready():
                observed=ready(alias,window,4,'DUMMY0')
                return observed if int(observed['current_epoch'],16)>int(old_epoch,16) else None
            removed=wait_for_predicate('ready remaining monitor with newer published topology',
                removal_ready,time.monotonic()+10,process=core)
            assert int(removed['current_epoch'],16)>int(old_epoch,16)
            evidence['removal']=dict(server=remaining,removed=removed)
            capture('browser-after-removal.png')
            old_browser_pid=browser.process.pid
            key(window,'Escape'); assert browser.wait_until_exit(time.monotonic()+5)==0
            wait_for_predicate('old browser Form gone',lambda:not view.named('File browser'),time.monotonic()+5,process=core)
            old_owners=[json.loads(p.read_text()) for p in native.glob('module-*.json')
                if alias in json.loads(p.read_text())['arguments']]
            assert len(old_owners)==1,old_owners
            old_wait_path=native/('stopped-%d.json' % old_owners[0]['proxy'])
            wait_for_predicate('old browser Form actual child Wait',old_wait_path.exists,
                time.monotonic()+5,process=core)
            old_wait=json.loads(old_wait_path.read_text())
            assert old_wait['child']==old_owners[0]['child'] and old_wait['returncode']==0,old_wait
            command('Style FvwmFiles-* PositionPlacement screen DUMMY0 100p 100p, StartsOnScreen DUMMY0')
            browser=runtime.start('browser-successor',[str(build/'bin/FvwmPrompt/fvwm-file-browser'),
                '--socket',str(endpoint),'--dir',str(folder),'--watch','off'],'browser-successor.log',env)
            successor=view.wait('File browser')
            hint=ClassHint(); assert x.XGetClassHint(d,successor,C.byref(hint))
            try: successor_alias=C.string_at(hint.name).decode()
            finally:
                if hint.name: x.XFree(hint.name)
                if hint.class_: x.XFree(hint.class_)
            restarted=ready(successor_alias,successor,4,'DUMMY0')
            assert successor_alias!=alias and browser.process.pid!=old_browser_pid
            identity=re.fullmatch(r'FvwmFiles-([0-9a-f]{32})-([1-9][0-9]*)-ready',successor_alias)
            assert identity and identity[1]!=provider,successor_alias
            def successor_ready():
                observed=rpc(provider=identity[1])
                return observed if observed and observed['availability']['ready'] and \
                    not observed['busy'] and observed['loaded']==ENTRIES else None
            successor_state=wait_for_predicate('fresh successor provider snapshot',successor_ready,
                time.monotonic()+10,process=browser)
            successor_packet=list_status(successor_alias)
            assert successor_packet['generation']>0 and successor_state['generation']
            retired_started=time.monotonic()
            command('Echo FVWM_FORM_CHANGED '+browser_watch)
            command('Echo FVWM_FORM_CHANGED '+identity[1]+'_f01')
            retired_state=rpc(provider=identity[1])
            retired_packet=list_status(successor_alias)
            assert retired_packet['packet']==successor_packet['packet']
            assert retired_state['generation']==successor_state['generation']
            retired_commands=[r for r in rows(native) if r['time']>=retired_started and
                r['direction']=='out' and r['text'].startswith('SendToModule '+successor_alias+' ')]
            assert not any(' ListWatchStatus ' in r['text'] or ' SetLabel ' in r['text']
                for r in retired_commands),retired_commands
            new_owners=[json.loads(p.read_text()) for p in native.glob('module-*.json')
                if successor_alias in json.loads(p.read_text())['arguments']]
            assert len(new_owners)==1 and new_owners[0]['child']!=old_owners[0]['child'],new_owners
            assert browser.poll() is None and not (native/('stopped-%d.json' % new_owners[0]['proxy'])).exists()
            # XIDs can be recycled after the old X connection closes. Lifetime
            # comes from the exited owner, new provider and actual child/status.
            evidence['restart']=dict(status=restarted,old_xid=window,new_xid=successor,
                xid_reused=successor==window,xid_reuse_allowed=True,old_browser_pid=old_browser_pid,
                new_browser_pid=browser.process.pid,old_alias=alias,new_alias=successor_alias,
                old_form_owner=old_owners[0],old_form_wait=old_wait,new_form_owner=new_owners[0],
                provider_state=successor_state,native_packet=successor_packet,
                retired_watch_token=browser_watch,retired_marker_state=retired_state,
                retired_marker_packet=retired_packet,retired_marker_commands=retired_commands)
            key(successor,'Escape'); assert browser.wait_until_exit(time.monotonic()+5)==0
            evidence['status']='passed'
        except BaseException as failure:
            evidence['status']='failed'; evidence['failures'].append(dict(type=type(failure).__name__,
                message=str(failure),traceback=traceback.format_exc())); raise
        finally:
            def cleanup(label,action):
                try: action()
                except BaseException as failure:
                    evidence['cleanup_failures'].append(dict(step=label,type=type(failure).__name__,message=str(failure)))
            if control: cleanup('close control',control.close)
            if rpc_peer: cleanup('close RPC',rpc_peer.close)
            if errors: cleanup('close X observer',errors.close_display)
            elif client: cleanup('close X observer',client.close)
            for process in reversed(runtime.processes):
                cleanup('stop '+process.name,lambda p=process:p.close(time.monotonic()+4))
            evidence['children_after_cleanup']={p.name+'-'+str(p.process.pid):p.poll() for p in runtime.processes}
            def waits(): evidence['module_cleanup']=wait_children(runtime.root/'native-records')
            cleanup('actual module child waits',waits)
            for p in runtime.root.iterdir():
                if p.is_file() and (p.suffix in ('.log','.conf') or p.name=='config' or p.name.endswith('.failed')):
                    cleanup('copy '+p.name,lambda path=p:shutil.copy2(path,out/path.name))
                elif p.name in ('native-records','user'):
                    cleanup('copy '+p.name,lambda path=p:shutil.copytree(path,out/path.name))
            if evidence['cleanup_failures'] or evidence.get('observer_unexpected_x_errors'): evidence['status']='failed'
            atomic_json(out/'evidence.json',evidence)
    assert not evidence['cleanup_failures'],evidence['cleanup_failures']
    assert not evidence.get('observer_unexpected_x_errors'),evidence.get('observer_unexpected_x_errors')
    print('native viewport scale '+profile+' passed: '+str(out))


def main():
    if len(sys.argv)>1 and sys.argv[1]=='--module-proxy':
        return module_proxy(sys.argv[2],sys.argv[3],sys.argv[4:])
    parser=argparse.ArgumentParser(description=__doc__,allow_abbrev=False)
    parser.add_argument('--build',type=Path,required=True)
    parser.add_argument('--artifacts-dir',type=Path,required=True)
    parser.add_argument('--xorg-config',type=Path,default=Path(__file__).with_name('xorg-dummy.conf'))
    parser.add_argument('--fault-form',type=Path)
    parser.add_argument('--profiles',nargs='+',choices=PROFILES,default=list(PROFILES))
    args=parser.parse_args(); args.artifacts_dir.mkdir(parents=True,exist_ok=True)
    destination=Path(tempfile.mkdtemp(prefix='viewport-scale-',dir=args.artifacts_dir.resolve()))
    for profile in args.profiles: run(args,profile,destination)
    return 0


if __name__=='__main__':
    raise SystemExit(main())
