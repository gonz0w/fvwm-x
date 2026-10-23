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


def file_provider_checks(connect, rpc, evidence):
    rpc.send(frame('files-empty', resource='files'))
    assert reply(rpc, 'files-empty')['providers'] == []
    rpc.send(frame('files-readonly', 'register', resource='files', provider='a' * 32))
    assert reply(rpc, 'files-readonly', 'error') == 'read_only'
    owners = [connect() for _ in range(8)]
    ids = ['%032x' % (i + 1) for i in range(8)]
    for owner, provider in zip(owners, ids):
        owner.send(frame('register', 'register', resource='files', provider=provider))
        assert reply(owner, 'register') == dict(provider=provider, registered=True)
    extra = connect()
    extra.send(frame('duplicate', 'register', resource='files', provider=ids[0]))
    assert reply(extra, 'duplicate', 'error') == 'provider_exists'
    extra.send(frame('limit', 'register', resource='files', provider='f' * 32))
    assert reply(extra, 'limit', 'error') == 'provider_limit'
    owners[0].send(frame('downgrade', 'set_mode', mode='read-only'))
    assert reply(owners[0], 'downgrade', 'error') == 'provider_active'
    rpc.send(frame('files-list', resource='files'))
    listed = reply(rpc, 'files-list')
    assert listed['provider_limit'] == 8 and not listed['query_only']
    assert {x['provider'] for x in listed['providers']} == set(ids)

    def query(id_, index=0, client=rpc):
        client.send(frame(id_, resource='files', provider=ids[index], scope='items', limit=8))
        value = owners[index].record()['file_query']
        assert value['version'] == 1 and value['provider'] == ids[index]
        assert len(value['serial']) == 16 and int(value['serial'], 16) > 0
        return value['serial']

    def respond(owner, serial, result):
        data = json.dumps(dict(result=result), separators=(',', ':')).encode()
        chunks = [data[i:i+256] for i in range(0, len(data), 256)]
        for i, piece in enumerate(chunks):
            owner.send(('files_reply %s part %d %s\n' % (serial, i, piece.hex())).encode())
        owner.send(('files_reply %s end %d\n' % (serial, len(chunks))).encode())

    def owner_barrier(index=0):
        owners[index].send(frame('producer-barrier', resource='connection'))
        reply(owners[index], 'producer-barrier')

    serial = query('files-answer')
    respond(owners[1], serial, dict(wrong_owner=True))
    owner_barrier(1)
    assert not select.select([rpc.socket], [], [], .05)[0]
    payload = dict(observed=True, label='literal " \\ \n',
        integers='9223372036854775807', padding='v' * 700)
    respond(owners[0], serial, payload)
    assert reply(rpc, 'files-answer') == payload
    rpc.send(frame('files-readonly-action', 'action', resource='files', provider=ids[0],
        generation='a' * 32, operation='selection', values={'mode':'clear'}))
    assert reply(rpc, 'files-readonly-action', 'error') == 'read_only'
    action_client = connect()
    action_client.send(frame('files-action', 'action', resource='files', provider=ids[0],
        generation='a' * 32, operation='filter', values={'text':'literal " $[exec.noop]\n'}))
    action = owners[0].record()['file_query']
    assert action['scope'] == 'action' and action['operation'] == 'filter'
    assert action['values'] == {'text':'literal " $[exec.noop]\n'}
    respond(owners[0], action['serial'], dict(applied=True))
    assert reply(action_client, 'files-action') == dict(applied=True)
    action_client.send(frame('files-action-cancelled', 'action', resource='files', provider=ids[0],
        generation='a' * 32, operation='selection', values={'mode':'clear'}))
    action = owners[0].record()['file_query']
    action_client.send(frame('cancel-action-wait', 'cancel', target='files-action-cancelled'))
    assert reply(action_client, 'files-action-cancelled', 'error') == 'wait_cancelled'
    assert reply(action_client, 'cancel-action-wait')['state'] == 'wait-cancelled'
    rpc.send(frame('cancelled-action-busy', resource='files', provider=ids[0], scope='state'))
    assert reply(rpc, 'cancelled-action-busy', 'error') == 'provider_busy'
    respond(owners[0], action['serial'], dict(applied_after_cancel=True))
    owner_barrier()
    action_client.send(frame('action-client-after-cancel', resource='connection'))
    assert not reply(action_client, 'action-client-after-cancel')['read_only']
    action_client.close()
    serial = query('files-cancelled')
    rpc.send(frame('files-cancel', 'cancel', target='files-cancelled'))
    assert reply(rpc, 'files-cancelled', 'error') == 'wait_cancelled'
    assert reply(rpc, 'files-cancel')['state'] == 'wait-cancelled'
    rpc.send(frame('still-busy', resource='files', provider=ids[0], scope='state'))
    assert reply(rpc, 'still-busy', 'error') == 'provider_busy'
    respond(owners[0], serial, dict(discarded_after_cancel=True))
    owner_barrier()
    serial = query('files-deadline', 1)
    rpc.send(frame('files-pending', resource='pending'))
    pending = reply(rpc, 'files-pending')['requests']
    assert any(x['id'] == 'files-deadline' and x['resource'] == 'files' for x in pending)
    assert reply(rpc, 'files-deadline', 'error') == 'deadline_exceeded'
    rpc.send(frame('deadline-still-busy', resource='files', provider=ids[1], scope='state'))
    assert reply(rpc, 'deadline-still-busy', 'error') == 'provider_busy'
    respond(owners[1], serial, dict(discarded_after_timeout=True))
    owner_barrier(1)
    departed = connect()
    serial = query('files-departed', client=departed)
    departed.close()
    respond(owners[0], serial, dict(discarded_after_disconnect=True))
    owner_barrier()
    serial = query('files-owner-lost')
    owners[0].close()
    assert reply(rpc, 'files-owner-lost', 'error') == 'provider_unavailable'
    owners[0] = connect()
    owners[0].send(frame('replacement', 'register', resource='files', provider=ids[0]))
    reply(owners[0], 'replacement')
    old_serial = serial
    serial = query('files-replaced')
    respond(owners[0], old_serial, dict(stale_incarnation=True))
    respond(owners[0], serial, dict(replacement=True))
    assert reply(rpc, 'files-replaced') == dict(replacement=True)
    serial = query('files-invalid', 7)
    owners[7].send(('files_reply %s end 0\n' % serial).encode())
    owners[7].expect_closed()
    assert reply(rpc, 'files-invalid', 'error') == 'provider_unavailable'
    for owner in owners:
        owner.close()
    extra.close()
    rpc.send(frame('files-after', resource='connection'))
    assert reply(rpc, 'files-after')['read_only']
    evidence['file_providers'] = dict(provider_limit=8, connection_owned=True,
        readonly_query=True, conditional_action_routing=True, readonly_action_refused=True, action_cancel_wait_only=True, wrong_owner_ignored=True, chunked_reply=True,
        cancellation_retains_producer=True, timeout_retains_producer=True,
        disconnected_waiter_discarded=True, owner_disconnect=True,
        fresh_serial_after_replacement=True, malformed_producer_isolated=True)


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

            file_provider_checks(connect, rpc, evidence)

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
