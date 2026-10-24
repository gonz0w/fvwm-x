#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or any later version.
"""Owned desktop operations over actual core/MFL/helper transports."""
import argparse
import json
import os
from pathlib import Path
import shlex
import shutil
import sys
import time

from live_x_harness import HarnessRuntime, start_display_server, wait_for_predicate
from mfl_stream_live import Connection
from mfl_request_live import frame, reply


def run(args):
    args.artifacts_dir.mkdir(parents=True, exist_ok=True)
    evidence = {'status': 'running'}
    with HarnessRuntime(prefix='fvwm-operations-') as runtime:
        core, connections = None, []
        gate = runtime.root / 'release'
        started = runtime.root / 'started'
        try:
            _, display = start_display_server(runtime, 'Xvfb',
                ['Xvfb', '-screen', '0', '800x600x24', '-noreset'],
                'xserver.log', time.monotonic() + 10)
            source = Path(__file__).resolve().parents[2]
            user, wrappers = runtime.root / 'user', runtime.root / 'modules'
            user.mkdir(mode=0o700)
            wrappers.mkdir()
            (wrappers / 'FvwmMFL').symlink_to(args.mfl.resolve())
            # Gate acquisition at the private worker boundary; run the actual
            # installed resident binary with no patched controller functions.
            real_worker = os.environ.get('FVWM_DESKTOP_WORKER',
                str(args.desktop.with_name('fvwm-desktop-worker')))
            peer = runtime.root / 'worker-peer'
            peer.write_text('#!' + sys.executable + '\n'
                'import json,sys,subprocess,time\nfrom pathlib import Path\n'
                'data=sys.stdin.buffer.read(); request=json.loads(data)\n'
                f'counter=Path({str(runtime.root / "scan-count")!r})\n'
                'if request["operation"]=="apps":\n'
                ' count=int(counter.read_text())+1 if counter.exists() else 1\n'
                ' counter.write_text(str(count))\n'
                ' if count>1:\n'
                f'  Path({str(started)!r}).touch()\n'
                '  deadline=time.monotonic()+5\n'
                f'  while not Path({str(gate)!r}).exists():\n'
                '   if time.monotonic()>deadline: raise RuntimeError("fixture gate deadline")\n'
                '   time.sleep(.01)\n'
                f'result=subprocess.run([{real_worker!r}],input=data,stdout=sys.stdout.buffer)\n'
                'raise SystemExit(result.returncode)\n')
            peer.chmod(0o700)
            (wrappers / 'FvwmDesktop').symlink_to(args.desktop.resolve())
            config = runtime.root / 'config'
            config.write_text('Read ' + str(source / 'fvwm/ConfigFvwmDefaults') + '\n'
                'ModulePath ' + str(wrappers) + '\n'
                'AddToFunc StartFunction I Module FvwmMFL\n+ I Module FvwmDesktop\n')
            core = runtime.start('core', [str(args.fvwm), '-d', display,
                '-f', str(config), '-v', '-o', str(runtime.root / 'debug.log')],
                'core.log', runtime.environment({'FVWMMFL_SOCKET_PATH': str(runtime.root),
                    'FVWM_USERDIR': str(user), 'FVWM_DESKTOP_ROOT': str(source / 'default-config'),
                    'FVWM_DESKTOP_WORKER': str(peer)}))
            endpoint = runtime.root / f"fvwm_mfl_{display.replace(':', '_')}.sock"
            wait_for_predicate('operation endpoint', endpoint.exists,
                               time.monotonic() + 12, process=core)
            def connect():
                connection = Connection(endpoint)
                connection.socket.settimeout(10)
                connection.record()
                connections.append(connection)
                return connection
            writer, observer = connect(), connect()
            serial = 0
            def request(method='query', resource='operations', expected=None,
                        connection=None, **fields):
                nonlocal serial
                serial += 1
                token = 'operation-' + str(serial)
                c = connection or writer
                if resource is not None:
                    fields['resource'] = resource
                c.send(frame(token, method=method, **fields))
                return reply(c, token, 'error' if expected else 'ok')
            deadline = time.monotonic() + 5
            while True:
                provider = request(resource='provider')
                if provider['lifecycle'] == 'ready':
                    break
                assert time.monotonic() < deadline
                time.sleep(.01)
            identity, generation = provider['provider'], provider['generation']
            assert provider['operation_targets'] == ['applications', 'status']
            request(method='set_mode', resource=None, mode='read-only', connection=observer)
            request(method='watch', resource='operations', provider=identity, connection=observer)
            request(method='action', operation='start', target='applications',
                    provider=identity, generation=generation, connection=observer,
                    expected='read_only')
            # An explicit busy reply means no operation was admitted; only that
            # refusal is retried while the startup discovery drains.
            deadline = time.monotonic() + 5
            while True:
                serial += 1
                token = 'start-' + str(serial)
                writer.send(frame(token, method='action', resource='operations',
                    operation='start', target='applications', provider=identity,
                    generation=generation))
                value = writer.record()['reply']
                assert value['id'] == token
                if value['status'] == 'ok':
                    operation = value['result']['operation']
                    break
                assert value['error']['code'] == 'operation_busy'
                assert time.monotonic() < deadline
                time.sleep(.02)
            assert operation['state'] == 'accepted'
            wait_for_predicate('gated worker started', started.exists,
                               time.monotonic() + 5, process=core)
            assert request(method='action', operation='start', target='applications',
                provider=identity, generation=generation, expected=True) == 'operation_busy'
            operation_id = operation['id']
            def query():
                return request(provider=identity, target=operation_id)['operation']
            wait_for_predicate('running operation observed',
                lambda: query()['state'] == 'running', time.monotonic() + 5, process=core)
            running = query()
            assert running['progress']['total'] is None
            assert request(method='action', operation='cancel', target=operation_id,
                provider=identity, generation=operation['generation'], expected=True) == 'stale_generation'
            cancellation = request(method='action', operation='cancel', target=operation_id,
                provider=identity, generation=running['generation'])['operation']
            assert cancellation['cancel_requested'] and cancellation['state'] == 'running'
            writer.close()
            connections.remove(writer)
            writer = connect()
            assert query()['cancel_requested']  # work/record survives view loss
            gate.touch()
            wait_for_predicate('cancelled output discarded',
                lambda: query()['state'] == 'cancelled', time.monotonic() + 5, process=core)
            terminal = query()
            assert terminal['application'] == 'not-submitted'
            assert terminal['worker_stop'] == 'unknown' and not terminal['rollback']
            notices = []
            while not notices or notices[-1]['state'] != 'cancelled':
                notice = observer.record()['operation_changed']
                assert set(notice) == {'version', 'owner', 'provider', 'id',
                                       'generation', 'state', 'snapshot_required'}
                assert notice['id'] == operation_id and notice['provider'] == identity
                notices.append(notice)
                assert len(notices) <= 4
            request(method='unwatch', resource='operations', connection=observer)
            completed = request(method='action', operation='start', target='applications',
                provider=identity, generation=generation)['operation']
            operation_id = completed['id']
            wait_for_predicate('refresh completion', lambda: query()['state'] == 'completed',
                               time.monotonic() + 5, process=core)
            finished = query()
            assert finished['application'] == 'submitted-to-fvwm'
            assert isinstance(finished['result']['entries'], int)
            records = request(provider=identity)['operations']
            assert {v['state'] for v in records} == {'completed', 'cancelled'}
            assert request(method='action', operation='cancel', target=operation_id,
                provider=identity, generation=finished['generation'], expected=True) == 'operation_finished'
            assert request(provider=identity, target='0' * 32, expected=True) == 'operation_unavailable'
            request(resource='desktop', connection=observer)  # no unwanted events
            evidence.update(status='passed', running_cancellation=terminal,
                completed=finished, notifications=len(notices), disconnect_continues=True,
                readonly_refuses_actions=True, stale_cancel_refused=True,
                busy_not_admitted=True, retained_terminals=len(records),
                backend='first startup scan real; later scans fixture-gated then real')
        finally:
            gate.touch()
            for connection in connections:
                connection.close()
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
    parser.add_argument('--desktop', type=Path, required=True)
    parser.add_argument('--artifacts-dir', type=Path, required=True)
    run(parser.parse_args())
