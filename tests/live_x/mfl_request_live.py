#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or any later version.
"""Independent MFL request clients observe core state, bounds and lifetimes."""
import argparse
import ctypes as C
import json
import os
from pathlib import Path
import select
import shutil
import signal
import time

from geometry_transition_live import Client
from live_x_harness import HarnessRuntime, start_display_server, wait_for_predicate
from mfl_stream_live import Connection


def frame(id_, method='query', **fields):
    return ('request ' + json.dumps(dict(version=1, id=id_, method=method,
                                        **fields)) + '\n').encode()


def reply(connection, id_, status='ok'):
    value = connection.record()
    assert set(value) == {'reply'}, value
    value = value['reply']
    assert value['version'] == 1 and value['id'] == id_, value
    assert value['status'] == status, value
    return value.get('result') if status == 'ok' else value['error']['code']


def run(args):
    args.artifacts_dir.mkdir(parents=True, exist_ok=True)
    evidence = {'status': 'running'}
    with HarnessRuntime(prefix='fvwm-mfl-request-') as runtime:
        connections, client, core = [], None, None
        stopped = False
        try:
            display = args.display
            if display is None:
                _, display = start_display_server(runtime, 'Xvfb',
                    ['Xvfb', '-screen', '0', '800x600x24', '-noreset'],
                    'xserver.log', time.monotonic() + 10)
            config = runtime.root / 'config'
            config.write_text(f'ModulePath {args.mfl.parent}\n'
                'DesktopSize 3x2\nEwmhNumberOfDesktops 4\n'
                'EdgeScroll 0 0\nEdgeThickness 0\n'
                'AddToFunc StartFunction I Module FvwmMFL\n')
            core = runtime.start('core', [str(args.fvwm), '-d', display,
                '-f', str(config), '-v', '-o', str(runtime.root / 'debug.log')],
                'core.log', runtime.environment({'FVWMMFL_SOCKET_PATH':
                    str(runtime.root), 'FVWM_USERDIR': str(runtime.root)}))
            endpoint = runtime.root / f"fvwm_mfl_{display.replace(':', '_')}.sock"
            wait_for_predicate('request endpoint', endpoint.exists,
                time.monotonic() + 12, process=core)

            def connect():
                connection = Connection(endpoint)
                connection.socket.settimeout(10)
                connections.append(connection)
                profile = connection.record()['connection_profile']
                assert profile['request_version'] == 1
                assert profile['request_prefix'] == 'request '
                return connection

            legacy, rpc, other = connect(), connect(), connect()
            legacy.send(b'set echo\n')
            legacy.echo('request-ready')
            data = frame('fragmented', resource='capabilities')
            rpc.send(data[:13])
            assert not select.select([rpc.socket], [], [], .05)[0]
            rpc.send(data[13:-1])
            assert not select.select([rpc.socket], [], [], .05)[0]
            rpc.send(b'\n' + frame('connection', resource='connection'))
            capabilities = reply(rpc, 'fragmented')
            assert capabilities['pending_limit'] == 8
            assert capabilities['deadline_ms'] == 5000
            assert not capabilities['applied_command_results']
            assert not capabilities['read_only_security_boundary']
            assert not reply(rpc, 'connection')['read_only']
            for i in range(96):
                rpc.send(frame(str(i), resource='capabilities'))
            for i in range(96):
                reply(rpc, str(i))
            evidence['fragmentation_coalescing_and_callback_continuation'] = 96

            # These observations come from a separate X client, not MFL's parser.
            client = Client(display)
            x, d = client.x, client.display
            x.XInternAtom.argtypes = [C.c_void_p, C.c_char_p, C.c_int]
            x.XInternAtom.restype = C.c_ulong
            x.XGetWindowProperty.argtypes = [C.c_void_p, C.c_ulong, C.c_ulong,
                C.c_long, C.c_long, C.c_int, C.c_ulong, C.POINTER(C.c_ulong),
                C.POINTER(C.c_int), C.POINTER(C.c_ulong), C.POINTER(C.c_ulong),
                C.POINTER(C.c_void_p)]

            def prop(name):
                actual, count, after = C.c_ulong(), C.c_ulong(), C.c_ulong()
                format_, data = C.c_int(), C.c_void_p()
                atom = x.XInternAtom(d, name.encode(), 0)
                assert x.XGetWindowProperty(d, client.root, atom, 0, 128,
                    0, 6, C.byref(actual), C.byref(format_), C.byref(count),
                    C.byref(after), C.byref(data)) == 0
                try:
                    assert actual.value == 6 and format_.value == 32
                    assert after.value == 0 and count.value <= 128
                    values = C.cast(data, C.POINTER(C.c_ulong))
                    return [values[i] for i in range(count.value)]
                finally:
                    if data:
                        x.XFree(data)

            rpc.send(b'GotoDesk 0 1\nGotoPage 1 1\n'
                     b'EwmhBaseStruts 8 12 24 32\n' +
                     frame('ordered', 'barrier'))
            barrier = reply(rpc, 'ordered')
            assert barrier == dict(state='processed', asynchronous_completion='unknown')
            wait_for_predicate('independent desktop publication',
                lambda: prop('_NET_CURRENT_DESKTOP') == [1],
                time.monotonic() + 5, process=core)
            rpc.send(frame('shared', resource='desktop'))
            other.send(frame('shared', resource='desktop'))
            state = reply(rpc, 'shared')
            assert reply(other, 'shared') == state
            width, height = client.geometry(client.root)[2:]
            assert state['desk'] == prop('_NET_CURRENT_DESKTOP')[0] == 1
            assert state['viewport'] == dict(x=width, y=height,
                                             width=width, height=height), state
            assert prop('_NET_DESKTOP_VIEWPORT')[2:4] == [width, height]
            assert state['page'] == dict(x=1, y=1)
            area = state['work_area']
            assert [area[k] for k in ('x', 'y', 'width', 'height')] == \
                prop('_NET_WORKAREA')[4:8]
            assert state['scope'] == 'core-current-monitor'
            assert state['units'] == 'actual-pixels'
            evidence['authoritative_state_correlated_clients_and_barrier'] = state

            rpc.send(frame('ro', 'set_mode', mode='read-only'))
            assert reply(rpc, 'ro')['read_only']
            rpc.send(b'GotoDesk 0 2\nset debug\n')
            assert reply(rpc, None, 'error') == 'read_only'
            assert reply(rpc, None, 'error') == 'read_only'
            rpc.send(frame('unlock', 'set_mode', mode='read-write'))
            assert reply(rpc, 'unlock', 'error') == 'unsupported_mode'
            rpc.send(b'set focus_change\n' + frame('ro-state', resource='connection'))
            connection = reply(rpc, 'ro-state')
            assert connection['read_only'] and connection['subscriptions'] == ['focus_change']
            rpc.send(b'unset focus_change\n')
            rpc.send(frame('ro-query', resource='desktop'))
            assert reply(rpc, 'ro-query')['desk'] == 1
            legacy.send(b'GotoDesk 0 2\n')
            legacy.echo('writable-client-independent')
            rpc.send(frame('updated', resource='desktop'))
            assert reply(rpc, 'updated')['desk'] == 2
            marker = runtime.root / 'must-not-exist'
            rpc.send(frame('unsupported', 'execute', command=f'Exec touch {marker}'))
            assert reply(rpc, None, 'error') == 'invalid_request'
            rpc.send(b'request {"version":2,"id":"version","method":"barrier"}\n')
            assert reply(rpc, 'version', 'error') == 'unsupported_version'
            rpc.send(b'request {"version":1,"id":"bad\\u0000id","method":"barrier"}\n')
            assert reply(rpc, None, 'error') == 'invalid_request'
            assert not marker.exists()
            evidence['readonly_admission_schema_errors_and_no_execution'] = 'passed'

            # Stop only this owned core; MFL must continue servicing local queries
            # and absolute request deadlines, then discard late core replies.
            os.kill(core.process.pid, signal.SIGSTOP)
            stopped = True
            start = time.monotonic()
            rpc.send(b''.join(frame(f'pending-{i}', resource='desktop') for i in range(8)) +
                frame('pending-0', 'barrier') + frame('pending-0', 'unknown') +
                frame('over-limit', 'barrier') +
                frame('local-during-wait', resource='connection'))
            assert reply(rpc, None, 'error') == 'duplicate_id'
            assert reply(rpc, None, 'error') == 'duplicate_id'
            assert reply(rpc, 'over-limit', 'error') == 'too_many_requests'
            assert reply(rpc, 'local-during-wait')['read_only']
            rpc.send(frame('pending-view', resource='pending'))
            waiting = reply(rpc, 'pending-view')
            assert waiting['scope'] == 'connection' and waiting['execution'] == 'unknown'
            assert len(waiting['requests']) == 8
            assert {r['id'] for r in waiting['requests']} == {f'pending-{i}' for i in range(8)}
            assert all(r['state'] == 'awaiting-reply' and 0 < r['remaining_ms'] <= 5000
                       for r in waiting['requests'])
            other.send(frame('private-view', resource='pending') +
                       frame('foreign-cancel', 'cancel', target='pending-0'))
            assert reply(other, 'private-view')['requests'] == []
            assert reply(other, 'foreign-cancel', 'error') == 'request_not_pending'
            rpc.send(frame('cancel-0', 'cancel', target='pending-0') +
                     frame('pending-0', 'barrier'))
            assert reply(rpc, 'pending-0', 'error') == 'wait_cancelled'
            cancelled = reply(rpc, 'cancel-0')
            assert cancelled == dict(state='wait-cancelled', target='pending-0',
                                     execution='unknown', rollback=False)
            # The freed slot is immediately reusable, but keeps a fresh serial.
            expired = [rpc.record()['reply'] for _ in range(8)]
            assert {r['id'] for r in expired} == {f'pending-{i}' for i in range(8)}
            assert all(r['status'] == 'error' and r['error']['code'] == 'deadline_exceeded'
                       for r in expired)
            rpc.send(frame('cancel-again', 'cancel', target='pending-0'))
            assert reply(rpc, 'cancel-again', 'error') == 'request_not_pending'
            elapsed = time.monotonic() - start
            assert 4.5 <= elapsed < 12, elapsed
            rpc.send(frame('pending-0', 'barrier'))
            departed = connect()
            departed.send(frame('reused', resource='desktop') +
                          frame('admitted', resource='connection'))
            reply(departed, 'admitted')
            departed.close()
            replacement = connect()
            replacement.send(frame('reused', resource='desktop'))
            os.kill(core.process.pid, signal.SIGCONT)
            stopped = False
            reply(rpc, 'pending-0')
            assert reply(replacement, 'reused')['desk'] == 2
            rpc.send(frame('after-late', resource='connection'))
            assert reply(rpc, 'after-late')['read_only']
            evidence['pending_duplicate_timeout_late_and_disconnect_isolation'] = {
                'pending_limit': 8, 'deadline_seconds_observed': elapsed,
                'local_pending_view': True, 'cancel_at_capacity_readonly': True,
                'connection_isolation': True, 'slot_and_id_reuse': True}

            slow = connect()
            try:
                slow.send(b''.join(frame(f'slow-{i}', resource='capabilities')
                                   for i in range(4096)))
            except (BrokenPipeError, ConnectionResetError):
                pass
            slow.expect_closed()
            legacy.echo('healthy-after-reply-backpressure')
            other.send(frame('healthy-query', resource='desktop'))
            assert reply(other, 'healthy-query')['desk'] == 2
            evidence['reply_backpressure_and_legacy_isolation'] = 'passed'
            assert core.poll() is None and endpoint.exists()
            evidence['status'] = 'passed'
        finally:
            if stopped:
                os.kill(core.process.pid, signal.SIGCONT)
            for connection in connections:
                connection.close()
            if client:
                client.close()
            if core:
                core.close(time.monotonic() + 5)
            for log in runtime.root.glob('*.log'):
                shutil.copy2(log, args.artifacts_dir / log.name)
            (args.artifacts_dir / 'evidence.json').write_text(
                json.dumps(evidence, indent=2) + '\n')
    print(json.dumps(evidence))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--fvwm', type=Path, required=True)
    parser.add_argument('--mfl', type=Path, required=True)
    parser.add_argument('--display')
    parser.add_argument('--artifacts-dir', type=Path, required=True)
    run(parser.parse_args())
