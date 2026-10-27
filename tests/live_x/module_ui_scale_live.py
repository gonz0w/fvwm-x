#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
"""Verify finalized MonitorUI handoffs through an actual native RandR consumer.

One owned Xorg-dummy display supplies authoritative monitor/output/root facts.
The C probe uses the production parser and ownership policy, original config
packets and real root-pixel window rectangles. Its one-shot omitted-END gate
models lost completion bytes; its bounded M_STRING gate proves sync accounting.
No rendering, automatic module UI scaling, physical hardware or owner closure
is inferred from this metadata checkpoint.
"""
import argparse
import ctypes as C
import hashlib
import json
import os
from pathlib import Path
import select
import shlex
import shutil
import sys
import tempfile
import time
import traceback

from desktop_live import connect_client
from inspector_live import atomic_json
from live_x_harness import (HarnessRuntime, start_xorg_dummy, wait_for_randr,
    wait_for_authoritative_state, query_authoritative_state, run_required_xrandr,
    wait_for_predicate)
from mfl_stream_live import Connection
from native_bindings_live import module_proxy
from session_x_errors import SessionXErrors

CAP = 8 * 1024 * 1024


def records(path):
    if not path.exists():
        return []
    assert path.stat().st_size <= CAP, path
    return [json.loads(line) for line in path.read_text().split('\n')[:-1]]


def named_rows(snapshot):
    return {bytes.fromhex(row['name_hex']).decode(): row for row in snapshot['rows']}


def wait_children(directory):
    started = time.monotonic()

    def receipts():
        result = []
        for path in directory.glob('module-*.json'):
            value = json.loads(path.read_text())
            stopped = directory / ('stopped-%d.json' % value['proxy'])
            if not stopped.exists():
                return None
            waited = json.loads(stopped.read_text())
            assert waited['child'] == value['child'] and waited['returncode'] == 0, waited
            result.append(dict(value, waited=waited))
        return dict(children=result, seconds=time.monotonic()-started)

    return wait_for_predicate('actual native module child Wait receipts', receipts, started+5)


def run(args):
    source, build = Path(__file__).resolve().parents[2], args.build.resolve()
    probe_binary = args.probe.resolve() if args.probe else build / 'tests/module-ui-scale-probe'
    args.artifacts_dir.mkdir(parents=True, exist_ok=True)
    out = Path(tempfile.mkdtemp(prefix='module-ui-scale-', dir=args.artifacts_dir.resolve()))
    inputs = ['tests/module_ui_scale_probe.c', 'tests/live_x/module_ui_scale_live.py',
        'tests/live_x/native_bindings_live.py', 'libs/module_ui_scale.c',
        'libs/module_ui_scale.h', 'fvwm/module_interface.c', 'fvwm/fvwm3.c']
    evidence = dict(status='running', failures=[], cleanup_failures=[], operations=[], cases=[],
        source_sha256={name: hashlib.sha256((source/name).read_bytes()).hexdigest() for name in inputs},
        binary_sha256={str(path): hashlib.sha256(path.read_bytes()).hexdigest()
            for path in (build/'fvwm3', probe_binary, build/'modules/FvwmMFL/FvwmMFL')},
        limits={'snapshot_rows': 64, 'report_bytes': CAP, 'probe_lease_seconds': 120,
                'sync_gate_seconds': 1, 'rendering_or_physical_hardware_qualified': False})
    with HarnessRuntime(prefix='fvwm-module-ui-scale-') as runtime:
        core = control = client = errors = None
        try:
            _, display = start_xorg_dummy(runtime, args.xorg_config.resolve(), time.monotonic()+10)
            wait_for_randr(runtime, display, time.monotonic()+10)
            modules, native = runtime.root/'modules', runtime.root/'native-records'
            modules.mkdir()
            native.mkdir()
            fixture = Path(__file__).resolve()
            for name, binary in [('ModuleUIScaleProbe', probe_binary),
                                 ('FvwmMFL', build/'modules/FvwmMFL/FvwmMFL')]:
                wrapper = modules/name
                wrapper.write_text('#!/bin/sh\nexec ' + shlex.join([sys.executable,
                    str(fixture), '--module-proxy', str(binary), str(native)]) +
                    ' "$@" 2>>' + shlex.quote(str(runtime.root/(name+'.log'))) + '\n')
                wrapper.chmod(0o700)
            env = runtime.environment({'DISPLAY': display, 'HOME': str(runtime.root),
                'FVWM_USERDIR': str(runtime.root), 'FVWMMFL_SOCKET_PATH': str(runtime.root)})

            def mutate(arguments, label):
                run_required_xrandr(runtime, display, arguments, label, evidence['operations'])

            mutate(['--addmode', 'DUMMY1', '1280x1024'], 'add-second-output-mode')
            mutate(['--output', 'DUMMY0', '--mode', '1920x1080', '--pos', '0x0', '--primary',
                '--set', 'WIDTH_MM', '508', '--set', 'HEIGHT_MM', '286',
                '--output', 'DUMMY1', '--mode', '1280x1024', '--pos', '1920x0',
                '--set', 'WIDTH_MM', '339', '--set', 'HEIGHT_MM', '271',
                '--fb', '3200x1080', '--fbmm', '846x286'], 'two-monitor-baseline')

            def facts(label, predicate):
                return wait_for_authoritative_state(runtime, display, label, predicate,
                    time.monotonic()+6, label.replace(' ', '-'))

            baseline = facts('baseline authoritative facts', lambda s:
                set(s['monitors']) == {'DUMMY0', 'DUMMY1'} and
                s['monitors']['DUMMY0']['mm_width'] == 508 and
                s['monitors']['DUMMY1']['mm_width'] == 339)
            evidence['baseline_xrandr'] = baseline
            client = connect_client(display)
            errors = SessionXErrors(client, evidence).install()
            client.x.XInternAtom.argtypes = [C.c_void_p, C.c_char_p, C.c_int]
            client.x.XInternAtom.restype = C.c_ulong
            endpoint = runtime.root/('fvwm_mfl_' + display.replace(':', '_') + '.sock')

            def command(text):
                control.send((text+'\n').encode())
                control.echo('scale-command-'+os.urandom(4).hex())

            def matching(report, predicate, label, owner=None):
                return wait_for_predicate(label,
                    lambda: next((r for r in reversed(records(report)) if predicate(r)), None),
                    time.monotonic()+8, process=owner or core)

            def control_probe(report, operation, window=None):
                token = 'p'+os.urandom(8).hex()
                text = operation+' '+token+(' '+str(window) if window is not None else '')
                command('SendToModule ModuleUIScaleProbe '+text)
                return matching(report, lambda r: r['event'] == operation and r['token'] == token,
                    'actual native control '+operation)

            def request(report):
                requested = control_probe(report, 'request')
                return matching(report, lambda r: r['event'] == 'end-config' and
                    r['token'] == requested['token'], 'complete actual native config replay')

            def assert_snapshot(snapshot, authoritative, quarters):
                rows = named_rows(snapshot)
                assert int(snapshot['epoch'], 16) > 0 and not snapshot['root_fallback'], snapshot
                assert set(rows) == set(authoritative['monitors']) == set(quarters), snapshot
                assert len({r['atom'] for r in rows.values()}) == len(rows)
                for name, row in rows.items():
                    actual = authoritative['monitors'][name]
                    assert row['quarters'] == quarters[name] and row['atom'] > 0, row
                    assert row['atom'] == client.x.XInternAtom(client.display, name.encode(), 1), row
                    assert all(row[k] == actual[k] for k in ('x', 'y', 'width', 'height', 'primary'))
                return rows

            def assert_legacy(report, root):
                observed = records(report)
                finalized = {r['token'] for r in observed if r['event'] == 'end-config' and
                    int(r['snapshot']['epoch'],16) > 0}
                legacy = [r for r in observed if r.get('raw', '').startswith('Monitor ')]
                assert legacy, 'no unchanged legacy Monitor records observed'
                assert any(r['token'] in finalized for r in legacy)
                for row in legacy:
                    fields = shlex.split(row['raw'])
                    assert len(fields) == 16 and row['legacy_valid'], row
                    assert list(map(int, fields[2:])) == row['legacy_values']
                    if row['token'] in finalized:
                        assert row['legacy_values'][8:10] == [root['width'], root['height']], row
                return legacy

            def start_case(name, overrides):
                nonlocal core, control
                report = runtime.root/(name+'.jsonl')
                config = runtime.root/(name+'.config')
                # The synchronous native module completes its first config
                # query before later scale declarations and finalization.
                config.write_text('ModulePath '+str(modules)+'\nDefaultFont fixed\n'
                    'DesktopConfiguration global\nDesktopSize 1x1\n'
                    'Style * BorderWidth 2, HandleWidth 2, ClickToFocus, '
                    'PositionPlacement 100 100\n'
                    'ModuleSynchronous ModuleUIScaleProbe '+str(report)+'\n'+
                    '\n'.join(overrides)+'\nAddToFunc StartFunction I Module FvwmMFL\n')
                core = runtime.start(name, [str(build/'fvwm3'), '-d', display, '-f', str(config),
                    '-o', str(runtime.root/(name+'-core.log')), '-v'], name+'-process.log', env)
                wait_for_predicate('MFL for '+name, endpoint.exists, time.monotonic()+10, process=core)
                control = Connection(endpoint)
                control.record()
                control.send(b'set echo\n')
                control.echo('scale-case-ready')
                return report

            def stop_case(report):
                nonlocal control, core
                control_probe(report, 'quit')
                matching(report, lambda r: r['event'] == 'closed', 'native consumer clean close')
                control.send(b'Quit\n')
                assert core.wait_until_exit(time.monotonic()+5) == 0
                control.close()
                control = None
                evidence.setdefault('case_cleanup', []).append(wait_children(native))
                core = None
                wait_for_predicate('retired MFL socket', lambda: not endpoint.exists(), time.monotonic()+5)

            # All startup policies use one authoritative server and fresh core
            # processes; there is deliberately no unsupported live scale reload.
            for quarters in (4, 5, 6, 8):
                factor = str(quarters/4).rstrip('0').rstrip('.')
                report = start_case('override-'+str(quarters),
                    ['MonitorScale DUMMY0 1', 'MonitorScale DUMMY1 '+factor])
                published = matching(report, lambda r: r.get('status') == 'published',
                    'automatic post-finalization publication')
                values = records(report)
                early_end = next(i for i,r in enumerate(values) if r['event'] == 'end-config' and
                    r['token'] == 'startup')
                assert values[early_end]['snapshot']['epoch'] == '0000000000000000'
                assert not any(r.get('raw', '').startswith('MonitorUI ') for r in values[:early_end+1])
                assert_snapshot(published['snapshot'], baseline, {'DUMMY0': 4, 'DUMMY1': quarters})
                replay = request(report)
                again = request(report)
                assert replay['snapshot'] == again['snapshot'] == published['snapshot']
                assert any(r.get('status') == 'unchanged' and r['token'] == again['token']
                    for r in records(report)), 'identical replay was not unchanged'
                evidence['cases'].append(dict(name='startup-'+factor, published=published,
                    initial_query_before_finalization=values[early_end], replay=again,
                    legacy=assert_legacy(report, baseline['root'])))
                stop_case(report)

            report = start_case('automatic-live', [])
            initial = matching(report, lambda r: r.get('status') == 'published', 'automatic 96 DPI snapshot')
            assert_snapshot(initial['snapshot'], baseline, {'DUMMY0': 4, 'DUMMY1': 4})
            client.create()
            wait_for_predicate('managed ownership window', client.frame, time.monotonic()+5, process=core)

            def observe_at(x, y):
                def arrived():
                    value = control_probe(report, 'observe', client.window)
                    # The requested frame position includes ordinary title/
                    # border offsets; observe the actual client rectangle.
                    return value if x <= value['x'] < x+50 and y <= value['y'] < y+80 else None
                return wait_for_predicate('actual root-pixel window destination', arrived,
                    time.monotonic()+5, process=core)

            command('WindowId %d Move 100p 100p' % client.window)
            left = observe_at(100,100)
            assert left['owner_atom'] == named_rows(initial['snapshot'])['DUMMY0']['atom']
            assert left['quarters'] == 4 and left['width'] == 200 and left['height'] == 120

            # Change the actual output millimetres, then compare the consumer
            # with newly queried RandR facts and a strictly newer core epoch.
            control_probe(report, 'omit-end')
            mutate(['--output', 'DUMMY1', '--set', 'WIDTH_MM', '169', '--set', 'HEIGHT_MM', '135'],
                'second-monitor-192-dpi')
            physical = facts('192 DPI authoritative facts', lambda s:
                s['monitors']['DUMMY1']['mm_width'] == 169 and
                s['monitors']['DUMMY1']['mm_height'] == 135)
            omitted = matching(report, lambda r: r['event'] == 'omitted-end', 'one actual completion omitted')
            assert omitted['pending'] and omitted['snapshot'] == initial['snapshot'], omitted
            aborted = control_probe(report, 'abort')
            assert not aborted['pending'] and aborted['snapshot'] == initial['snapshot'], aborted
            current = request(report)
            assert_snapshot(current['snapshot'], physical, {'DUMMY0': 4, 'DUMMY1': 8})
            assert int(current['snapshot']['epoch'],16) > int(initial['snapshot']['epoch'],16)
            command('WindowId %d Move 2100p 100p' % client.window)
            right = observe_at(2100,100)
            assert right['owner_atom'] == named_rows(current['snapshot'])['DUMMY1']['atom']
            assert right['quarters'] == 8
            evidence['automatic_physical_change_and_ownership'] = dict(before=initial, server=physical,
                omitted=omitted, aborted=aborted, after=current, left=left, right=right)

            # Withhold this *real* synchronized string's unlock. A following
            # command from the same producer must not bypass it using spare
            # acknowledgements left by the preceding config bundle.
            token = 'b'+os.urandom(8).hex()
            control.echo('before-native-sync-gate')
            control.send(('SendToModule ModuleUIScaleProbe barrier '+token+'\nEcho AFTER-'+token+'\n').encode())
            gate = matching(report, lambda r: r['event'] == 'barrier' and r['token'] == token,
                'real synchronized string held')
            assert not control.pending and not select.select([control.socket], [], [], .15)[0], \
                'a leftover unlock bypassed the subsequent synchronized string'
            Path(str(report)+'.'+token+'.release').touch()
            control.expect_echo('AFTER-'+token)
            released = matching(report, lambda r: r['event'] == 'barrier-released' and r['token'] == token,
                'real synchronized string released')
            evidence['sync_accounting'] = dict(held=gate, released=released,
                following_command='AFTER-'+token, blocked_observation_seconds=.15)

            mutate(['--output', 'DUMMY1', '--off', '--fb', '1920x1080', '--fbmm', '508x286'],
                'remove-second-monitor')
            remaining = facts('single monitor authoritative facts', lambda s:
                set(s['monitors']) == {'DUMMY0'} and s['root']['width'] == 1920)
            removed = matching(report, lambda r: r.get('status') == 'published' and
                len(r['snapshot']['rows']) == 1, 'coherent removed-monitor handoff')
            assert_snapshot(removed['snapshot'], remaining, {'DUMMY0': 4})
            assert int(removed['snapshot']['epoch'],16) > int(current['snapshot']['epoch'],16)
            command('WindowId %d Move 100p 100p' % client.window)
            after_move = observe_at(100,100)
            assert after_move['owner_atom'] == named_rows(removed['snapshot'])['DUMMY0']['atom']
            assert after_move['quarters'] == 4
            control_probe(report, 'quit')
            matching(report, lambda r: r['event'] == 'closed', 'old consumer closes')
            successor = runtime.root/'consumer-successor.jsonl'
            command('Module ModuleUIScaleProbe '+str(successor))
            fresh = matching(successor, lambda r: r['event'] == 'end-config' and
                r['token'] == 'startup', 'fresh native receiver startup')
            assert fresh['snapshot'] == removed['snapshot'] and fresh['pid'] != removed['pid']
            evidence['removal_and_consumer_restart'] = dict(server=remaining, removed=removed,
                ownership=after_move, successor=fresh,
                legacy=assert_legacy(successor, remaining['root']))
            errors.close_display()
            errors = client = None
            stop_case(successor)
            evidence['status'] = 'passed'
        except BaseException as failure:
            evidence['status'] = 'failed'
            evidence['failures'].append(dict(type=type(failure).__name__, message=str(failure),
                traceback=traceback.format_exc()))
            raise
        finally:
            def cleanup(label, action):
                try: action()
                except BaseException as failure:
                    evidence['cleanup_failures'].append(dict(step=label,
                        type=type(failure).__name__, message=str(failure)))
            if control: cleanup('close MFL observer', control.close)
            if errors: cleanup('close X observer', errors.close_display)
            elif client: cleanup('close X observer', client.close)
            for process in reversed(runtime.processes):
                cleanup('stop '+process.name, lambda p=process: p.close(time.monotonic()+4))
            evidence['children_after_cleanup'] = {p.name+'-'+str(p.process.pid): p.poll()
                for p in runtime.processes}
            def waited(): evidence['final_module_cleanup'] = wait_children(runtime.root/'native-records')
            cleanup('wait real module children', waited)
            for path in runtime.root.iterdir():
                if path.is_file() and path.suffix in ('.jsonl','.log','.config'):
                    cleanup('copy '+path.name, lambda p=path: shutil.copy2(p,out/p.name))
                elif path.name == 'native-records':
                    cleanup('copy native receipts', lambda p=path: shutil.copytree(p,out/p.name))
            if evidence['cleanup_failures'] or evidence.get('observer_unexpected_x_errors'):
                evidence['status'] = 'failed'
            atomic_json(out/'evidence.json', evidence)
    assert not evidence['cleanup_failures'], evidence['cleanup_failures']
    assert not evidence.get('observer_unexpected_x_errors'), evidence.get('observer_unexpected_x_errors')
    print('native module UI scale handoff passed: '+str(out))


def main():
    if len(sys.argv) > 1 and sys.argv[1] == '--module-proxy':
        return module_proxy(sys.argv[2],sys.argv[3],sys.argv[4:])
    parser = argparse.ArgumentParser(description=__doc__,allow_abbrev=False)
    parser.add_argument('--build',type=Path,required=True)
    parser.add_argument('--probe',type=Path)
    parser.add_argument('--xorg-config',type=Path,
        default=Path(__file__).with_name('xorg-dummy.conf'))
    parser.add_argument('--artifacts-dir',type=Path,required=True)
    run(parser.parse_args())
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
