#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or any later version.
"""Actual core/MFL/desktop owner round trips, plus an independent bad producer."""
import argparse
import json
import os
from pathlib import Path
import shlex
import shutil
import signal
import struct
import sys
import time

from live_x_harness import HarnessRuntime, start_display_server, wait_for_predicate
from mfl_stream_live import Connection
from mfl_request_live import frame, reply


def provider_stub():
    """Only fixture-owned pipes; deliberately send partial/out-of-order records."""
    out_fd, in_fd = map(int, sys.argv[2:4])
    word, header = struct.Struct('@L'), struct.Struct('@LLLL')
    def send(text):
        data = text.encode()
        packet = word.pack(0) + word.pack(len(data)) + data + word.pack(1)
        while packet:
            packet = packet[os.write(out_fd, packet):]
    send('SET_MASK ' + str(1 << 22))
    send('NOP FINISHED STARTUP')
    buffer = bytearray()
    while True:
        data = os.read(in_fd, 65536)
        if not data:
            return
        buffer.extend(data)
        while len(buffer) >= header.size:
            flag, kind, size, _ = header.unpack_from(buffer)
            assert flag == 0xffffffff and 4 <= size <= 4096
            length = size * word.size
            if len(buffer) < length:
                break
            body = bytes(buffer[header.size:length])
            del buffer[:length]
            if kind != 1 << 22:
                continue
            text = body[3 * word.size:].split(b'\0', 1)[0].decode()
            command, cookie, serial, encoded = text.strip().split(' ')
            assert command == 'ipc-request'
            request = json.loads(bytes.fromhex(encoded))
            with Path(os.environ['FVWM_SETTINGS_STUB_REQUESTS']).open('a') as log:
                log.write(json.dumps(dict(cookie=cookie, serial=serial, request=request)) + '\n')
            prefix = 'SendToModule FvwmMFL MFL_SETTINGS_1 ' + cookie + ' ' + serial
            if request['scope'] == 'partial':
                send(prefix + ' part 0 7b')
            elif request['scope'] == 'invalid':
                send(prefix + ' part 1 7b')
            else:
                data = b'{"result":{"owner":"independent-fixture","state":"observed"}}'
                send(prefix + ' part 0 ' + data.hex())
                send(prefix + ' end 1')


def run(args):
    args.artifacts_dir.mkdir(parents=True, exist_ok=True)
    evidence = {'status': 'running'}
    with HarnessRuntime(prefix='fvwm-settings-ipc-') as runtime:
        connections, core, stopped = [], None, False
        try:
            _, display = start_display_server(runtime, 'Xvfb',
                ['Xvfb', '-screen', '0', '800x600x24', '-noreset'],
                'xserver.log', time.monotonic() + 10)
            source = Path(__file__).resolve().parents[2]
            user, wrappers = runtime.root / 'user', runtime.root / 'modules'
            user.mkdir(mode=0o700)
            wrappers.mkdir()
            (wrappers / 'FvwmMFL').symlink_to(args.mfl.resolve())
            helper = wrappers / 'FvwmDesktop'
            helper.symlink_to(args.desktop.resolve())
            stub_log = runtime.root / 'stub-requests.jsonl'
            config = runtime.root / 'config'
            config.write_text('Read ' + str(source / 'fvwm/ConfigFvwmDefaults') + '\n'
                'ModulePath ' + str(wrappers) + '\n'
                '*FvwmDesktop: motion true\n'
                'AddToFunc StartFunction I Module FvwmMFL\n'
                '+ I Module FvwmDesktop\n')
            core = runtime.start('core', [str(args.fvwm), '-d', display,
                '-f', str(config), '-v', '-o', str(runtime.root / 'debug.log')],
                'core.log', runtime.environment({'FVWMMFL_SOCKET_PATH': str(runtime.root),
                    'FVWM_USERDIR': str(user), 'FVWM_DESKTOP_ROOT': str(source / 'default-config'),
                    'FVWM_SETTINGS_STUB_REQUESTS': str(stub_log)}))
            endpoint = runtime.root / f"fvwm_mfl_{display.replace(':', '_')}.sock"
            wait_for_predicate('settings MFL endpoint', endpoint.exists,
                               time.monotonic() + 12, process=core)
            def connect():
                c = Connection(endpoint)
                c.socket.settimeout(10)
                c.record()
                connections.append(c)
                return c
            writer, reader = connect(), connect()
            writer.send(b'set echo\n')
            writer.echo('settings-started')
            serial = 0
            def query(c=writer, scope='all', provider=None, generation=None):
                nonlocal serial
                deadline = time.monotonic() + 5
                while True:
                    serial += 1
                    token = 'query-' + str(serial)
                    fields = {} if provider is None else {'provider': provider}
                    if generation is not None:
                        fields['generation'] = generation
                    c.send(frame(token, resource='settings', scope=scope, **fields))
                    record = c.record()
                    assert set(record) == {'reply'}, record
                    response = record['reply']
                    assert response['version'] == 1 and response['id'] == token, response
                    if response['status'] == 'ok':
                        return response['result']
                    assert response['error']['code'] == 'provider_not_ready', response
                    assert time.monotonic() < deadline, 'provider never configured'
                    time.sleep(.01)
            def action(operation, generation, expected=None, c=writer, **fields):
                nonlocal serial
                serial += 1
                token = 'action-' + str(serial)
                fields.setdefault('provider', snapshot['provider'])
                c.send(frame(token, method='action', resource='settings',
                    operation=operation, generation=generation, **fields))
                result = reply(c, token, 'error' if expected else 'ok')
                if expected:
                    assert result == expected, result
                return result
            snapshot = query()
            assert snapshot['owner'] == 'FvwmDesktop' and not snapshot['preview']
            assert query(provider=snapshot['provider'])['provider'] == snapshot['provider']
            assert len(snapshot['settings']) == 11
            assert set(query(scope='group:wallpaper')['settings']) == {
                'wallpaper_palette', 'wallpaper_appearance'}
            assert snapshot['settings']['wallpaper_palette']['resolved'] == 'slate'
            assert snapshot['settings']['wallpaper_appearance']['resolved'] == 'auto'
            assert snapshot['settings']['wallpaper_palette']['composition_version'] == 2
            assert snapshot['settings']['contrast']['saved'] == 'normal'
            assert snapshot['settings']['contrast']['choices'] == ['normal', 'high']
            assert snapshot['settings']['contrast']['group'] == 'appearance'
            assert not snapshot['settings']['motion']['saved']
            assert snapshot['settings']['motion']['effective']
            assert snapshot['settings']['motion']['source'] == 'module-configuration'
            assert set(query(scope='group:schedule')['settings']) == {'day', 'night'}
            opacity = snapshot['settings']['launcher_opacity']
            assert opacity['type'] == 'integer' and opacity['minimum'] == 0
            assert opacity['maximum'] == 100 and opacity['saved'] == 94
            assert opacity['native_preview'] and not opacity['preview']
            state = user / 'desktop-state.json'
            assert not state.exists()
            writer.send(frame('provider-query', resource='provider', provider=snapshot['provider']))
            provider_state = reply(writer, 'provider-query')
            assert provider_state['lifecycle'] == 'ready'
            assert provider_state['lifecycle_scope'] == 'desktop-preferences'
            assert provider_state['generation'] == snapshot['generation']
            assert not provider_state['service_lifecycle'] and not provider_state['preview']
            assert query(provider=snapshot['provider'], generation=snapshot['generation'])['state'] == 'unchanged'
            watcher, discovery, filtered = connect(), connect(), connect()
            watcher.send(frame('readonly-watch', method='set_mode', mode='read-only'))
            reply(watcher, 'readonly-watch')
            for c, fields in [(watcher, dict(provider=snapshot['provider'])),
                              (discovery, {}), (filtered, dict(provider='0' * 32))]:
                c.send(frame('watch', method='watch', resource='provider', **fields))
                assert reply(c, 'watch')['snapshot_required']
            writer.send(b'Module FvwmDesktop\n'
                        b'SendToModule FvwmDesktop barrier two-settings-owners\n')
            writer.expect_echo('two-settings-owners')
            writer.expect_echo('two-settings-owners')
            ready_notice = discovery.record()['provider_changed']
            assert ready_notice['provider'] != snapshot['provider']
            assert set(ready_notice) == {'version', 'owner', 'provider', 'generation', 'snapshot_required'}
            assert ready_notice['snapshot_required']

            reader.send(frame('restrict', method='set_mode', mode='read-only'))
            reply(reader, 'restrict')
            observed = query(reader)
            action('set', observed['generation'], expected='read_only', c=reader,
                   values={'palette': 'ink'})
            assert not state.exists()
            reader.send(frame('validate', method='validate', resource='settings',
                provider=snapshot['provider'], generation=snapshot['generation'],
                values={'palette': 'grove', 'motion': False}))
            candidate = reply(reader, 'validate')
            assert candidate['state'] == 'validated'
            assert candidate['generation'] == snapshot['generation']
            assert candidate['settings']['palette']['effective'] == 'grove'
            assert not candidate['settings']['motion']['saved']
            assert candidate['settings']['motion']['effective']
            assert candidate['application'] == 'not-submitted'
            assert candidate['persistence'] == 'not-attempted'
            assert not state.exists()
            assert query()['settings']['palette']['effective'] == 'slate'
            changed = action('set', snapshot['generation'], values={
                'day': '19:00', 'night': '07:00', 'palette': 'grove', 'motion': False})
            assert changed['state'] == 'committed'
            assert changed['application'] == 'submitted-to-fvwm'
            assert changed['asynchronous_completion'] == 'unknown'
            notice = watcher.record()['provider_changed']
            assert notice['provider'] == snapshot['provider']
            assert notice['generation'] == changed['generation']
            assert discovery.record()['provider_changed'] == notice
            assert query(provider=snapshot['provider'], generation=snapshot['generation'])['state'] == 'observed'
            filtered.send(frame('filtered-barrier', method='barrier'))
            assert reply(filtered, 'filtered-barrier')['state'] == 'processed'
            watcher.send(frame('unwatch', method='unwatch', resource='provider'))
            assert reply(watcher, 'unwatch')['state'] == 'unwatched'
            discovery.send(frame('unwatch', method='unwatch', resource='provider'))
            reply(discovery, 'unwatch')
            evidence['provider_observation'] = dict(provider=provider_state,
                conditional_unchanged=True, stale_query_refresh=True,
                configuration_ready_discovery=True, notifications_exclude_values=True,
                read_only_watch=True, target_filter_isolated=True)

            numeric = action('set', changed['generation'], values={'launcher_opacity': 73})
            assert numeric['settings']['launcher_opacity']['saved'] == 73
            assert json.loads(state.read_text())['schema_version'] == 4
            before_numeric = state.read_bytes()
            for invalid in [True, 73.5, -1, 101, '73']:
                action('set', numeric['generation'], values={'launcher_opacity': invalid},
                       expected='invalid_request' if invalid == 73.5 else 'invalid_value')
                assert state.read_bytes() == before_numeric
            action('reset', numeric['generation'], scope='launcher_opacity')
            changed = query(provider=snapshot['provider'])
            evidence['typed_integer_setting'] = dict(saved=73, schema=4,
                malformed_refused=True, reset=94)
            saved = state.read_bytes()
            assert json.loads(saved)['palette'] == 'grove'
            assert state.stat().st_mode & 0o777 == 0o600
            assert changed['settings']['motion']['effective']
            action('set', observed['generation'], expected='stale_generation',
                   values={'palette': 'ink'})
            for values in ({'day': '07:00'}, {'palette': 'ink; Exec evil'}):
                action('set', changed['generation'], values=values, expected='invalid_value')
                assert state.read_bytes() == saved
            reset = action('reset', changed['generation'], scope='group:schedule')
            watcher.send(frame('unwatched-barrier', method='barrier'))
            assert reply(watcher, 'unwatched-barrier')['state'] == 'processed'

            watcher.send(frame('rewatch', method='watch', resource='provider',
                               provider=snapshot['provider']))
            reply(watcher, 'rewatch')
            writer.send(b'SendToModule FvwmMFL MFL_PROVIDER_1 bad bad\n')
            writer.echo('malformed-provider-notice')
            invalidated = watcher.record()['provider_invalidated']
            assert invalidated['reason'] == 'malformed_notice'
            assert invalidated['snapshot_required']
            watcher.send(frame('stop-rewatch', method='unwatch', resource='provider'))
            reply(watcher, 'stop-rewatch')
            evidence['provider_observation']['malformed_notice_requires_resnapshot'] = True

            assert set(reset['settings']) == {'day', 'night'}
            assert json.loads(state.read_text())['palette'] == 'grove'
            external = json.loads(state.read_text()) | {'palette': 'ink'}
            state.write_text(json.dumps(external))
            saved = state.read_bytes()
            action('set', reset['generation'], values={'palette': 'dune'},
                   expected='persistence_failed')
            assert state.read_bytes() == saved
            reloaded = action('reload', reset['generation'])
            assert reloaded['state'] == 'reloaded'
            assert reloaded['settings']['palette']['effective'] == 'ink'
            # Cancel a submitted write while the owned core is stopped. Resume
            # proves wait cancellation cannot retract or roll back that command.
            os.kill(core.process.pid, signal.SIGSTOP)
            stopped = True
            watcher.send(b''.join(frame('watch-slot-' + str(i), resource='provider',
                provider=snapshot['provider']) for i in range(8)) +
                frame('full-watch', method='watch', resource='provider') +
                frame('full-unwatch', method='unwatch', resource='provider'))
            assert reply(watcher, 'full-watch')['state'] == 'watching'
            assert reply(watcher, 'full-unwatch')['state'] == 'unwatched'
            for i in range(8):
                watcher.send(frame('clear-watch-' + str(i), method='cancel',
                                   target='watch-slot-' + str(i)))
                assert reply(watcher, 'watch-slot-' + str(i), 'error') == 'wait_cancelled'
                reply(watcher, 'clear-watch-' + str(i))
            evidence['provider_observation']['watch_at_pending_capacity'] = True
            writer.send(frame('cancelled-write', method='action', resource='settings',
                operation='set', provider=snapshot['provider'],
                generation=reloaded['generation'], values={'palette': 'dune'}) +
                frame('cancel-write', method='cancel', target='cancelled-write'))
            assert reply(writer, 'cancelled-write', 'error') == 'wait_cancelled'
            assert reply(writer, 'cancel-write')['execution'] == 'unknown'
            writer.send(frame('empty-after-cancel', resource='pending'))
            assert reply(writer, 'empty-after-cancel')['requests'] == []
            os.kill(core.process.pid, signal.SIGCONT)
            stopped = False
            wait_for_predicate('cancelled write still commits',
                lambda: json.loads(state.read_text())['palette'] == 'dune',
                time.monotonic() + 5, process=core)
            writer.send(frame('cancelled-write', resource='settings',
                              provider=snapshot['provider']))
            reloaded = reply(writer, 'cancelled-write')
            assert reloaded['settings']['palette']['effective'] == 'dune'
            writer.send(b'KillModule FvwmDesktop\nModule FvwmDesktop\n')
            writer.echo('settings-restart-submitted')
            restarted = query()
            assert restarted['generation'] != reloaded['generation']
            action('reset', reloaded['generation'], scope='all', expected='deadline_exceeded')
            evidence['real_provider'] = dict(
                settings=len(snapshot['settings']), scoped_queries=True,
                atomic_changes=True, targeted_multiple_owners=True,
                readonly_validation=True, validation_no_mutation=True,
                cancelled_write_still_commits=True, cancelled_id_reuse=True,
                readonly_refused=True, stale_writer_refused=True,
                scoped_reset=True, external_conflict=True, reload=True,
                restart_generation=True, restart_target_unavailable=True,
                asynchronous_completion='unknown')

            # No provider availability is inferred from a writable core stream.
            writer.send(b'KillModule FvwmDesktop\n')
            writer.echo('settings-stopped')
            writer.send(frame('missing', resource='settings') +
                        frame('missing-provider', resource='provider') +
                        frame('local', resource='capabilities'))
            assert reply(writer, 'local')['settings_provider_availability'] == 'on-request'
            missing = [writer.record()['reply'] for _ in range(2)]
            assert {r['id'] for r in missing} == {'missing', 'missing-provider'}
            assert all(r['status'] == 'error' and
                       r['error']['code'] == 'deadline_exceeded' for r in missing)
            evidence['provider_observation']['absent_owner_times_out_without_cache'] = True

            helper.unlink()
            helper.write_text('#!/bin/sh\nexec ' + shlex.join([
                sys.executable, str(Path(__file__).resolve()), '--provider-stub']) + ' "$@"\n')
            helper.chmod(0o755)
            writer.send(b'Module FvwmDesktop\n')
            writer.echo('stub-submitted')
            assert query()['owner'] == 'independent-fixture'
            reader.send(frame('partial-cancel', resource='settings', scope='partial'))
            def partial_received():
                reader.send(frame('partial-view', resource='pending'))
                pending = reply(reader, 'partial-view')['requests']
                return any(r['id'] == 'partial-cancel' and r['state'] == 'receiving-reply'
                           for r in pending)
            wait_for_predicate('partial reply assembled', partial_received,
                               time.monotonic() + 3, process=core)
            partial = json.loads(stub_log.read_text().splitlines()[-1])
            reader.send(frame('cancel-partial', method='cancel', target='partial-cancel'))
            assert reply(reader, 'partial-cancel', 'error') == 'wait_cancelled'
            assert reply(reader, 'cancel-partial')['state'] == 'wait-cancelled'
            writer.send(('SendToModule FvwmMFL MFL_SETTINGS_1 ' + partial['cookie'] + ' ' +
                         partial['serial'] + ' end 1\n').encode())
            writer.echo('late-cancelled-part-discarded')
            reader.send(frame('partial-timeout', resource='settings', scope='partial'))
            assert query()['owner'] == 'independent-fixture'
            assert reply(reader, 'partial-timeout', 'error') == 'deadline_exceeded'
            reader.send(frame('invalid-part', resource='settings', scope='invalid'))
            assert reply(reader, 'invalid-part', 'error') == 'invalid_provider_reply'
            orphan = connect()
            def requests():
                return [json.loads(line) for line in stub_log.read_text().splitlines()]
            before = len(requests())
            orphan.send(frame('reused', resource='settings', scope='partial'))
            wait_for_predicate('partial provider allocation', lambda: len(requests()) > before,
                               time.monotonic() + 5, process=core)
            late = requests()[-1]
            orphan.close()
            replacement = connect()
            replacement.send(frame('reused', resource='settings'))
            assert reply(replacement, 'reused')['owner'] == 'independent-fixture'
            writer.send(('SendToModule FvwmMFL MFL_SETTINGS_1 ' + late['cookie'] + ' ' +
                         late['serial'] + ' end 1\n').encode())
            writer.echo('late-settings-discarded')
            assert query()['owner'] == 'independent-fixture'
            evidence['independent_producer'] = dict(absent_timeout=True,
                partial_timeout=True, partial_cancel=True, malformed_part_refused=True,
                disconnected_partial=True, late_reply_ignored=True, healthy_client=True)
            assert core.poll() is None
            evidence['status'] = 'passed'
        finally:
            if stopped:
                os.kill(core.process.pid, signal.SIGCONT)
            for connection in connections:
                connection.close()
            if core:
                core.close(time.monotonic() + 5)
            for log in runtime.root.glob('*.log'):
                shutil.copy2(log, args.artifacts_dir / log.name)
            (args.artifacts_dir / 'evidence.json').write_text(json.dumps(evidence, indent=2) + '\n')
    print(json.dumps(evidence))


if __name__ == '__main__':
    if len(sys.argv) > 1 and sys.argv[1] == '--provider-stub':
        provider_stub()
    else:
        parser = argparse.ArgumentParser(description=__doc__)
        parser.add_argument('--fvwm', type=Path, required=True)
        parser.add_argument('--mfl', type=Path, required=True)
        parser.add_argument('--desktop', type=Path, required=True)
        parser.add_argument('--artifacts-dir', type=Path, required=True)
        run(parser.parse_args())
