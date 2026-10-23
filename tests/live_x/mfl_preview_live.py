#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or any later version.
"""Core-owned runtime preview, conflict and producer-lifetime recovery."""
import argparse
import ctypes as C
import json
from pathlib import Path
import shutil
import time

from geometry_transition_live import Client, Pointer
from live_x_harness import HarnessRuntime, start_display_server, wait_for_predicate
from mfl_stream_live import Connection
from mfl_request_live import frame, reply


def run(args):
    args.artifacts_dir.mkdir(parents=True, exist_ok=True)
    evidence = {'status': 'running'}
    with HarnessRuntime(prefix='fvwm-preview-') as runtime:
        core, clients, observer = None, [], None
        try:
            display = args.display
            if display is None:
                _, display = start_display_server(runtime, 'Xvfb',
                    ['Xvfb', '-screen', '0', '800x600x24', '-noreset'],
                    'xserver.log', time.monotonic() + 10)
            config = runtime.root / 'config'
            config.write_text(f'ModulePath {args.mfl.parent}\n'
                'EdgeScroll 0 0\nEdgeThickness 0\nStyle * TileCascadePlacement\n'
                'MenuStyle PreviewTarget Translucent 31\n'
                'AddToMenu PreviewMenu "Preview deadline" Title\n+ "Close" Nop\n'
                'MenuStyle PreviewMenu PreviewTarget\n'
                'AddToFunc StartFunction I Module FvwmMFL\n')
            core = runtime.start('core', [str(args.fvwm), '-d', display,
                '-f', str(config), '-v', '-o', str(runtime.root / 'debug.log')],
                'core.log', runtime.environment({'FVWMMFL_SOCKET_PATH': str(runtime.root),
                    'FVWM_USERDIR': str(runtime.root)}))
            endpoint = runtime.root / f"fvwm_mfl_{display.replace(':', '_')}.sock"
            wait_for_predicate('preview endpoint', endpoint.exists,
                time.monotonic() + 12, process=core)

            def connect():
                c = Connection(endpoint)
                c.socket.settimeout(10)
                c.record()
                clients.append(c)
                return c
            writer, owner, other = connect(), connect(), connect()
            writer.send(b'set echo\n')
            writer.echo('preview-ready')
            serial = 0
            def rpc(c, resource='preview', error=None, **fields):
                nonlocal serial
                serial += 1
                token = 'preview-' + str(serial)
                c.send(frame(token, resource=resource, **fields))
                return reply(c, token, 'error' if error else 'ok')
            def opacity():
                page = rpc(other, 'menu-styles', limit=16)
                return next(s['opacity_percent'] for s in page['menu_styles']
                            if s['name'] == 'PreviewTarget')
            def start(c=owner, timeout=30000, value=73, **extra):
                generation = rpc(c, 'appearance')['generation']
                return rpc(c, method='action', operation='start',
                           target='PreviewTarget', values={'opacity': value},
                           timeout_ms=timeout, generation=generation, **extra)
            def action(item, operation, c=owner, error=None):
                return rpc(c, method='action', operation=operation,
                           target=item['lease'], generation=item['generation'], error=error)
            assert opacity() == 31
            assert rpc(other)['state'] == 'inactive'
            initial = start()
            assert initial['state'] == 'active' and initial['previous_opacity'] == 31
            assert initial['preview_opacity'] == opacity() == 73
            assert initial['persistence'] == 'not-attempted'
            assert rpc(other)['state'] == 'inactive' and 'lease' not in rpc(other)
            assert action(initial, 'revert', other, 'preview_unavailable') == 'preview_unavailable'
            assert start(other, error='preview_busy') == 'preview_busy'
            reverted = action(initial, 'revert')
            assert reverted['state'] == 'reverted' and opacity() == 31
            assert action(initial, 'confirm', error='preview_finished') == 'preview_finished'
            stale = rpc(owner, 'appearance')['generation']
            writer.send(b'MenuStyle PreviewTarget Translucent 31\n')
            writer.echo('stale-appearance')
            assert rpc(owner, method='action', operation='start', target='PreviewTarget',
                values={'opacity': 73}, generation=stale, error='stale_generation') == 'stale_generation'
            kept = start(value=85)
            assert action(kept, 'confirm')['state'] == 'confirmed' and opacity() == 85
            next_lease = start()
            assert next_lease['previous_opacity'] == 85
            assert action(kept, 'revert', error='preview_unavailable') == 'preview_unavailable'
            action(next_lease, 'revert')
            assert opacity() == 85
            writer.send(b'MenuStyle PreviewTarget Translucent 31\n')
            writer.echo('reset-preview')
            expiry = start(timeout=1000)
            # No owner polling drives recovery: another read observes the scalar.
            time.sleep(1.15)
            assert opacity() == 31
            assert rpc(owner)['state'] == 'expired'
            observer = Client(display)
            pointer = Pointer(observer)
            start(timeout=1000)
            writer.send(b'Schedule 0 Popup PreviewMenu\n')
            wait_for_predicate('preview interaction grab', pointer.grabbed,
                time.monotonic() + 5, process=core)
            time.sleep(1.15)
            assert rpc(owner)['state'] == 'expired' and pointer.grabbed()
            observer.x.XStringToKeysym.argtypes = [C.c_char_p]
            observer.x.XStringToKeysym.restype = C.c_ulong
            observer.x.XKeysymToKeycode.argtypes = [C.c_void_p, C.c_ulong]
            observer.x.XKeysymToKeycode.restype = C.c_ubyte
            pointer.x.XTestFakeKeyEvent.argtypes = [C.c_void_p, C.c_uint, C.c_int, C.c_ulong]
            code = observer.x.XKeysymToKeycode(observer.display,
                observer.x.XStringToKeysym(b'Escape'))
            for pressed in (1, 0):
                assert pointer.x.XTestFakeKeyEvent(observer.display, code, pressed, 0)
            observer.x.XSync(observer.display, 0)
            wait_for_predicate('preview interaction dismissed', lambda: not pointer.grabbed(),
                time.monotonic() + 5, process=core)
            assert opacity() == 31
            abandoned = connect()
            start(abandoned)
            assert opacity() == 73
            abandoned.close()
            clients.remove(abandoned)
            wait_for_predicate('disconnect restores scalar', lambda: opacity() == 31,
                time.monotonic() + 5, process=core)
            conflict = start()
            writer.send(b'MenuStyle PreviewTarget Translucent 42\n')
            writer.echo('external-menu-change')
            assert rpc(owner)['state'] == 'conflicted' and opacity() == 42
            assert action(conflict, 'revert', error='preview_conflict') == 'preview_conflict'
            destroyed = start()
            writer.send(b'DestroyMenuStyle PreviewTarget\n')
            writer.echo('destroy-preview-target')
            assert rpc(owner)['state'] == 'conflicted'
            assert action(destroyed, 'confirm', error='preview_conflict') == 'preview_conflict'
            writer.send(b'MenuStyle PreviewTarget Translucent 31\n')
            writer.echo('recreate-preview-target')
            # Read-only applies to typed actions, while timer/owner-loss recovery
            # remains core-owned even after this connection switches mode.
            start()
            owner.send(frame('readonly', 'set_mode', mode='read-only'))
            assert reply(owner, 'readonly')['read_only']
            current = rpc(owner)
            assert action(current, 'confirm', error='read_only') == 'read_only'
            owner.close()
            clients.remove(owner)
            wait_for_predicate('readonly disconnect restores', lambda: opacity() == 31,
                time.monotonic() + 5, process=core)
            # Kill the native producer with an active lease. A fresh producer
            # observes restoration, proving cleanup does not depend on MFL's
            # disconnect callback or a live reply channel.
            start(other)
            writer.send(b'Schedule 500 Module FvwmMFL\nKillModule FvwmMFL\n')
            for c in clients:
                c.close()
            clients.clear()
            time.sleep(.7)
            wait_for_predicate('replacement producer endpoint', endpoint.exists,
                time.monotonic() + 5, process=core)
            fresh = connect()
            other = fresh
            assert opacity() == 31 and rpc(fresh)['state'] == 'inactive'
            assert core.poll() is None
            evidence.update(status='passed', start=initial, revert=reverted, expiry=expiry,
                confirm=kept, exclusive_lease=True, connection_isolation=True,
                stale_generation_refused=True, old_callback_refused=True,
                disconnect_restored=True, readonly_disconnect_restored=True,
                module_death_restored=True, interaction_expiry_restored=True,
                conflicting_change_preserved=True,
                destroyed_target_not_recreated=True, persistence='not-attempted')
        finally:
            for c in clients:
                c.close()
            if observer:
                observer.close()
            if core:
                core.close(time.monotonic() + 5)
            for log in runtime.root.glob('*.log'):
                shutil.copy2(log, args.artifacts_dir / log.name)
            (args.artifacts_dir / 'evidence.json').write_text(json.dumps(evidence, indent=2) + '\n')
    print(json.dumps(evidence))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--fvwm', type=Path, required=True)
    parser.add_argument('--mfl', type=Path, required=True)
    parser.add_argument('--display')
    parser.add_argument('--artifacts-dir', type=Path, required=True)
    run(parser.parse_args())
