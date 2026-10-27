#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or any later version.
"""Bounded actual-X runtime configuration origins and non-executing reports.

Observe real core/module packets, native Form text and windows. Expected source
coordinates come from fixture literals, never from product-reported locations.
--core-only exercises the core/native packet contract without the desktop theme.
"""
import argparse
import ctypes as C
import json
import os
from pathlib import Path
import re
import select
import shlex
import shutil
import signal
import struct
import subprocess
import sys
import time

from desktop_live import connect_client
from inspector_live import NativeView, atomic_json
from live_x_harness import HarnessRuntime, start_display_server, wait_for_predicate
from mfl_stream_live import Connection
from native_capture import NativeCapture
from session_x_errors import SessionXErrors
from selection_live import Event

PROFILES = ('slate-dark', 'dune-light', 'classic-large')
WORD = struct.calcsize('@L')
CONFIG, END, STRING = 1 << 18, 1 << 19, 1 << 22
SOURCE = (1 << 31) | (1 << 11)


def module_probe(arguments, selected):
    """Record bounded native packets without synthesizing source or config text."""
    output, incoming = map(int, arguments[:2])
    root = Path(arguments[-1])
    def send(command):
        data = command.encode()
        assert len(data) <= 4096 and not any(c in command for c in '\0\r\n')
        wire = struct.pack('@LL', 0, len(data)) + data + struct.pack('@L', 1)
        while wire:
            wire = wire[os.write(output, wire):]
    send('SET_MASK ' + str(CONFIG | END | STRING))
    if selected:
        send('SET_MASK ' + str(SOURCE))
    send('NOP FINISHED STARTUP')
    atomic_json(root / 'ready.json', {'selected': selected})
    buffer, pending, rows, seen = bytearray(), None, [], set()
    while True:
        if pending is None:
            files = sorted(root.glob('command-*.json'))
            assert len(files) <= 256
            for path in files:
                if path.name in seen:
                    continue
                pending = json.loads(path.read_text())
                assert re.fullmatch(r'p[0-9]{5}', pending['token'])
                seen.add(path.name)
                rows = []
                pending['sync'] = bool(pending.get('destroy'))
                if pending['sync']:
                    assert re.fullmatch(r'[A-Za-z0-9]+\*', pending['destroy'])
                    send('SET_SYNC_MASK ' + str(CONFIG))
                match = pending.get('match', '')
                assert not match or re.fullmatch(r'\*[A-Za-z0-9]+', match)
                send('Send_ConfigInfo' + (' ' + match if match else ''))
                break
        if not select.select([incoming], [], [], .01)[0]:
            continue
        block = os.read(incoming, 65536)
        if not block:
            return
        buffer.extend(block)
        assert len(buffer) <= 1024 * 1024
        while len(buffer) >= 4 * WORD:
            start, kind, count, _ = struct.unpack('@LLLL', buffer[:4 * WORD])
            assert start == 0xffffffff and 4 <= count <= 4096
            size = count * WORD
            if len(buffer) < size:
                break
            body = bytes(buffer[4 * WORD:size])
            del buffer[:size]
            if pending is None:
                continue
            if kind == END:
                if pending['sync']:
                    send('SET_SYNC_MASK 0')
                atomic_json(root / (pending['token'] + '.json'), {'packets': rows})
                pending = None
                continue
            assert len(body) >= 3 * WORD
            words = struct.unpack('@LLL', body[:3 * WORD])
            text = body[3 * WORD:].split(b'\0', 1)[0].decode('utf-8', 'strict')
            if pending['sync'] and kind == CONFIG:
                if pending['destroy'] and text.startswith(pending['match']):
                    send('DestroyModuleConfig ' + pending['destroy'])
                    pending['destroy'] = ''
                send('NOP UNLOCK')
            rows.append({'type': kind, 'words': list(words), 'text': text})
            assert len(rows) <= 4096


class Probe:
    def __init__(self, root, core):
        self.root, self.core, self.serial = root, core, 0

    def snapshot(self, match='', destroy=''):
        self.serial += 1
        token = 'p%05d' % self.serial
        atomic_json(self.root / ('command-' + token + '.json'), {'token': token, 'match': match, 'destroy': destroy})
        path = self.root / (token + '.json')
        wait_for_predicate('origin native replay ' + token, path.exists,
            time.monotonic() + 4, process=self.core)
        return json.loads(path.read_text())['packets']


def paired(packets, text):
    matches = [i for i, packet in enumerate(packets)
               if packet['type'] == CONFIG and packet['text'] == text]
    assert matches, (text, packets)
    result = []
    for index in matches:
        assert index > 0
        previous, packet = packets[index - 1], packets[index]
        assert previous['type'] == SOURCE, (previous, packet)
        assert 0 < packet['words'][0] == previous['words'][0]
        assert previous['words'][1:] == [0, 0]
        assert len(previous['text'].encode()) <= 1536
        result.append(json.loads(previous['text']))
    return result


def text_rows(packets, alias):
    rows = []
    for packet in packets:
        if packet['type'] != CONFIG:
            continue
        match = re.fullmatch(r'\*' + alias + r':?\s*TextHex x([0-9a-f]*)', packet['text'])
        if match:
            rows.append(bytes.fromhex(match.group(1)).decode('utf-8', 'strict'))
    return rows


def log_origins(log):
    """Pair each core source JSON with the following diagnostic text."""
    decoder, records = json.JSONDecoder(), []
    matches = list(re.finditer(r'\{\s*"version"\s*:\s*1', log))
    for index, match in enumerate(matches):
        try:
            origin, length = decoder.raw_decode(log[match.start():])
        except ValueError:
            continue
        end = matches[index + 1].start() if index + 1 < len(matches) else len(log)
        records.append((origin, log[match.start() + length:end]))
    return records


def diagnostic_origin(log, message):
    matches = [origin for origin, text in log_origins(log) if message in text]
    assert matches, (message, log)
    return matches


def file_origin(path, first, last, included):
    return {'version': 1, 'kind': 'file', 'path': str(path), 'first': first,
            'last': last, 'include': included, 'reason': ''}


def assert_origin(actual, expected):
    # Empty optional fields may be omitted on the wire. All independently known
    # path/span/chain coordinates must match exactly, including repeat includes.
    for key, value in expected.items():
        assert actual.get(key, '' if key == 'reason' else None) == value, (actual, expected)


def run(args, profile):
    source, build = Path(__file__).resolve().parents[2], args.build.resolve()
    out = args.artifacts_dir.resolve() / profile
    out.mkdir(parents=True, exist_ok=False)
    evidence = {'status': 'running', 'profile': profile, 'core_only': args.core_only,
                'screenshots': [], 'expected_origins': {}}
    started = time.monotonic()
    with HarnessRuntime(prefix='fvwm-config-origin-') as runtime:
        control = client = errors = core = None
        try:
            _, display = start_display_server(runtime, 'Xvfb',
                ['Xvfb', '-screen', '0', '1440x1200x24', '-noreset'],
                'xserver.log', time.monotonic() + 8)
            user, modules = runtime.root / 'user', runtime.root / 'modules'
            user.mkdir(mode=0o700)
            modules.mkdir()
            for directory in (build / 'modules').iterdir():
                executable = directory / directory.name
                if executable.is_file():
                    (modules / directory.name).symlink_to(executable)
            if not args.core_only:
                (modules / 'FvwmDesktop').symlink_to(build / 'bin/FvwmPrompt/FvwmDesktop')
            probes = []
            for name, selected in [('OriginProbe', True), ('LegacyOriginProbe', False)]:
                directory = runtime.root / name
                directory.mkdir()
                module = modules / name
                module.write_text('#!/bin/sh\nexec ' + shlex.join([sys.executable,
                    str(Path(__file__).resolve()), '--probe', str(int(selected))]) +
                    ' "$@" 2>' + shlex.quote(str(runtime.root / (name + '.log'))) + '\n')
                module.chmod(0o700)
                probes.append((name, directory))
            config = runtime.root / 'boot.config'
            parent = user / 'parent $cash "quotes" 100% é.config'
            child = user / 'origin-child'
            setting = '*FvwmDesktop: palette origin-invalid'
            # No EOF newline: source spans must not grow into a fictitious line.
            child.write_text('# source fixture\nOriginUnknown \\\n continued\nDesktopSize invalid\n' + setting)
            parent.write_text('# repeated literal Read\nRead origin-child\nRead origin-child\n')
            body = ''
            if not args.core_only:
                assets = source / 'default-config'
                body = (assets / 'config').read_text().replace(
                    '$[FVWM_DATADIR]/default-config', str(assets))
                if profile == 'classic-large':
                    body = body.replace('Sans:size=10:antialias=True',
                        'DejaVu Sans Mono:size=18:antialias=True')
                palette, appearance = ('dune', 'light') if profile == 'dune-light' else ('slate', 'dark')
                (user / 'local.config').write_text('*FvwmDesktop: palette ' + palette +
                    '\n*FvwmDesktop: appearance ' + appearance +
                    '\n*FvwmDesktop: motion Off\n*FvwmDesktop: transparency Off\n')
            prefix = ('ModulePath ' + str(modules) + '\nRead ' +
                str(source / 'fvwm/ConfigFvwmDefaults') + '\n' + body)
            if not prefix.endswith('\n'):
                prefix += '\n'
            read_line = prefix.count('\n') + 1
            config.write_text(prefix + 'Read ' + shlex.quote(str(parent)) + '\n' +
                ''.join('AddToFunc StartFunction I Module ' + name + ' ' + str(directory) + '\n'
                        for name, directory in probes) +
                'AddToFunc StartFunction I Module FvwmMFL\n')
            chain = [{'path': str(config), 'first': read_line, 'last': read_line},
                     {'path': str(parent), 'first': 2, 'last': 2}]
            expected = file_origin(child, 5, 5, chain)
            second = file_origin(child, 5, 5, [chain[0],
                {'path': str(parent), 'first': 3, 'last': 3}])
            evidence['expected_origins'] = {'first': expected, 'repeat': second,
                'unknown_span': [2, 3], 'malformed_span': [4, 4]}
            for path in (config, parent, child):
                shutil.copy2(path, out / path.name)
            env = runtime.environment({'DISPLAY': display, 'HOME': str(runtime.root),
                'FVWM_USERDIR': str(user), 'FVWM_DESKTOP_ROOT': str(source / 'default-config'),
                'FVWMMFL_SOCKET_PATH': str(runtime.root),
                'XDG_DATA_HOME': str(runtime.root / 'data'),
                'XDG_DATA_DIRS': str(runtime.root / 'absent')})
            core = runtime.start('core', [str(build / 'fvwm3'), '-d', display,
                '-v', '-f', str(config), '-o', '-'], 'core.log', env)
            for _, directory in probes:
                wait_for_predicate('origin observer ready', (directory / 'ready.json').exists,
                    time.monotonic() + 8, process=core)
            probe, legacy = [Probe(directory, core) for _, directory in probes]
            client = connect_client(display)
            errors = SessionXErrors(client, evidence).install()
            view = NativeView(client, errors, core)
            endpoint = runtime.root / ('fvwm_mfl_' + display.replace(':', '_') + '.sock')
            wait_for_predicate('origin MFL', endpoint.exists, time.monotonic() + 8, process=core)
            control = Connection(endpoint)
            control.record()
            control.send(b'set echo\n')
            serial = 0
            def command(text):
                nonlocal serial
                serial += 1
                control.send((text + '\n').encode())
                control.echo('origin-' + str(serial))
            def barrier():
                nonlocal serial
                serial += 1
                token = 'origin-barrier-' + str(serial)
                control.send(('SendToModule FvwmDesktop barrier ' + token + '\n').encode())
                control.expect_echo(token)
            packets = probe.snapshot()
            retained_setting = '*FvwmDesktoppalette origin-invalid'
            origins = paired(packets, retained_setting)
            assert len(origins) == 2, origins
            for actual, wanted in zip(origins, (expected, second)):
                assert_origin(actual, wanted)
            old = legacy.snapshot()
            assert not any(packet['type'] == SOURCE for packet in old)
            old_settings = [packet for packet in old if packet['type'] == CONFIG and packet['text'] == retained_setting]
            assert len(old_settings) == 2 and all(packet['words'][0] == 0 for packet in old_settings), old_settings
            evidence['native_replay'] = {'selected': packets, 'legacy': old}
            log = (runtime.root / 'core.log').read_text(errors='replace')
            assert 'OriginUnknown' in log and 'DesktopSize requires two arguments' in log, log
            for message, first, last in [('OriginUnknown', 2, 3),
                    ('DesktopSize requires two arguments', 4, 4)]:
                actual = diagnostic_origin(log, message)
                assert len(actual) == 2, actual
                for origin, included in zip(actual, (chain, second['include'])):
                    assert_origin(origin, file_origin(child, first, last, included))
            evidence['core_nested_continued_argument_errors'] = True
            # These explicit runtime-only actions run in a known file context.
            # The offline corpus below is never submitted to the command engine.
            actions = user / 'origin-actions'
            actions.write_text('# invocation and generated contexts\n'
                'AddToFunc OriginFunction I OriginFunctionUnknown\n'
                'OriginFunction\n'
                'PipeRead "printf \'OriginGeneratedUnknown\'"\n'
                'Schedule 1 OriginDeferredUnknown\n'
                'Silent OriginSilentUnknown\n'
                'Read\n'
                'Key F12 A N OriginEventUnknown\n')
            command('Read origin-actions')
            command('OriginUnknownFromModule')
            view.key('F12')
            wait_for_predicate('deferred runtime diagnostic', lambda:
                all(message in (runtime.root / 'core.log').read_text(errors='replace')
                    for message in ('OriginDeferredUnknown', 'OriginEventUnknown')),
                time.monotonic() + 3, process=core)
            runtime_log = (runtime.root / 'core.log').read_text(errors='replace')
            generated = diagnostic_origin(runtime_log, 'OriginGeneratedUnknown')[-1]
            assert generated['kind'] == 'generated' and generated['path'] == str(actions), generated
            assert (generated['first'], generated['last']) == (4, 4), generated
            invocation = diagnostic_origin(runtime_log, 'OriginFunctionUnknown')[-1]
            assert invocation['kind'] == 'invocation' and invocation['path'] == str(actions), invocation
            assert (invocation['first'], invocation['last']) == (3, 3), invocation
            assert diagnostic_origin(runtime_log, 'OriginUnknownFromModule')[-1]['kind'] == 'unavailable'
            assert diagnostic_origin(runtime_log, 'OriginEventUnknown')[-1]['kind'] == 'unavailable'
            assert 'OriginSilentUnknown' not in runtime_log, runtime_log
            deferred = diagnostic_origin(runtime_log, 'OriginDeferredUnknown')[-1]
            assert deferred['kind'] == 'unavailable', deferred
            missing = diagnostic_origin(runtime_log, 'missing filename parameter')[-1]
            assert_origin(missing, file_origin(actions, 7, 7, []))
            evidence['runtime_context_log'] = runtime_log
            # A literal child reached through a dynamic parent cannot claim a
            # complete file include chain. Both boundaries must say so honestly.
            function_child, generated_child = user / 'origin-function-child', user / 'origin-generated-child'
            function_child.write_text('# dynamic function parent\nOriginFunctionReadChildUnknown\n')
            generated_child.write_text('# generated parent\nOriginGeneratedReadChildUnknown\n')
            dynamic_parent = user / 'origin-dynamic-parent'
            dynamic_parent.write_text('# known invocation sites\n'
                'AddToFunc OriginReadFunction I Read origin-function-child\n'
                'OriginReadFunction\n'
                'PipeRead "printf \'Read origin-generated-child\'"\n')
            command('Read origin-dynamic-parent')
            dynamic_log = (runtime.root / 'core.log').read_text(errors='replace')
            dynamic_origins = {}
            for message in ('OriginFunctionReadChildUnknown', 'OriginGeneratedReadChildUnknown'):
                origin = diagnostic_origin(dynamic_log, message)[-1]
                assert origin['kind'] == 'unavailable' and origin.get('reason'), origin
                dynamic_origins[message] = origin
            evidence['dynamic_nested_read'] = dynamic_origins
            if args.core_only:
                # Retained provenance has a 1 MiB aggregate allocation cap.
                # Distinct aliases prevent live delivery to either observer;
                # filtered replay avoids creating one oversized output queue.
                budget_files = [user / ('budget-%d-' % i + 'x' * max(16,
                    180 - len(str(user)) - len('/budget-0-'))) for i in range(5)]
                count = 1400
                for index, path in enumerate(budget_files[:-1]):
                    path.write_text('# metadata budget caller\nRead "' + str(budget_files[index + 1]) + '"\n')
                budget_files[-1].write_text(''.join('*OriginBudget%04d: row\n' % i for i in range(count)))
                command('Read "' + str(budget_files[0]) + '"')
                budget_first = paired(probe.snapshot('*OriginBudget0000'), '*OriginBudget0000row')[-1]
                budget_last = paired(probe.snapshot('*OriginBudget1399'), '*OriginBudget1399row')[-1]
                wanted_chain = [{'path': str(path), 'first': 2, 'last': 2}
                                for path in budget_files[:-1]]
                assert_origin(budget_first, file_origin(budget_files[-1], 1, 1, wanted_chain))
                assert budget_last['kind'] == 'unavailable', budget_last
                command('DestroyModuleConfig OriginBudget*')
                restored = user / 'origin-budget-restored'
                restored.write_text('*OriginBudgetRestored: row\n')
                command('Read origin-budget-restored')
                restored_packets = probe.snapshot('*OriginBudgetRestored')
                restored_origin = paired(restored_packets, '*OriginBudgetRestoredrow')[-1]
                assert_origin(restored_origin, file_origin(restored, 1, 1, []))
                evidence['retained_metadata_budget'] = {'cap_bytes': 1024 * 1024,
                    'records': count, 'first': budget_first, 'last': budget_last,
                    'restored_after_destroy': restored_origin}
                command('DestroyModuleConfig OriginBudget*')
                retiring = user / 'origin-retire'
                retiring.write_text('*OriginRetire: first\n*OriginRetire: next\n*OriginRetire: last\n')
                command('Read origin-retire')
                retired_packets = probe.snapshot('*OriginRetire', destroy='OriginRetire*')
                survivors = [packet['text'] for packet in retired_packets if
                    packet['type'] == CONFIG and packet['text'].startswith('*OriginRetire')]
                assert survivors == ['*OriginRetirefirst'], survivors
                assert_origin(paired(retired_packets, '*OriginRetirefirst')[-1],
                    file_origin(retiring, 1, 1, []))
                after_retirement = probe.snapshot('*OriginRetire')
                assert not any(packet['type'] == CONFIG and packet['text'].startswith('*OriginRetire')
                               for packet in after_retirement), after_retirement
                evidence['synchronous_config_retirement'] = {'during': retired_packets,
                    'after': after_retirement, 'deleted_current_and_next': True}
                evidence['desktop_ui'] = 'not requested: core-only affected platform gate'
            else:
                barrier()
                def capture(name):
                    previous = os.environ.get('DISPLAY')
                    os.environ['DISPLAY'] = display
                    try:
                        path = Path(NativeCapture(build / 'bin/fvwm-desktop-capture').screenshot(out).removeprefix('Screenshot saved: '))
                        path.rename(out / name)
                        evidence['screenshots'].append(name)
                    finally:
                        if previous is None:
                            os.environ.pop('DISPLAY', None)
                        else:
                            os.environ['DISPLAY'] = previous
                def native_text(alias):
                    rows = text_rows(probe.snapshot(), alias)
                    assert rows and all(len(row) <= 72 for row in rows), rows
                    return rows
                def form_key(window, symbol):
                    # Existing native Form fixtures deliver KeyPress directly:
                    # Form keyboard handling does not promise WM input focus.
                    attributes = view.attributes(window)
                    assert attributes and attributes.map_state == 2
                    event = Event()
                    event.key.type, event.key.display = 2, client.display
                    event.key.window, event.key.root = window, client.root
                    event.key.same_screen = 1
                    event.key.keycode = client.x.XKeysymToKeycode(client.display,
                        client.x.XStringToKeysym(symbol.encode()))
                    assert event.key.keycode, symbol
                    client.x.XSendEvent.argtypes = [C.c_void_p, C.c_ulong, C.c_int,
                        C.c_long, C.POINTER(Event)]
                    client.x.XSendEvent.restype = C.c_int
                    assert client.x.XSendEvent(client.display, window, 0, 1, C.byref(event))
                    client.x.XSync(client.display, 0)
                def close(window, title):
                    if title == 'FVWM-X':
                        # Notice owns one Dismiss button; click its actual X
                        # child through XTest without assuming Form focus.
                        buttons = [(child, view.attributes(child))
                                   for child in view.children(window)]
                        buttons = [(child, attrs) for child, attrs in buttons
                                   if attrs and attrs.map_state == 2]
                        assert len(buttons) == 1, buttons
                        button, attrs = buttons[0]
                        client.x.XTranslateCoordinates.argtypes = [C.c_void_p, C.c_ulong, C.c_ulong,
                            C.c_int, C.c_int, C.POINTER(C.c_int), C.POINTER(C.c_int), C.POINTER(C.c_ulong)]
                        client.x.XTranslateCoordinates.restype = C.c_int
                        rx, ry, child = C.c_int(), C.c_int(), C.c_ulong()
                        assert client.x.XTranslateCoordinates(client.display, button, client.root,
                            attrs.width // 2, attrs.height // 2, C.byref(rx), C.byref(ry), C.byref(child))
                        assert 0 <= rx.value < 1440 and 0 <= ry.value < 1200
                        command('WindowId ' + str(window) + ' Raise')
                        view.pointer.motion(rx.value, ry.value)
                        try:
                            view.pointer.button(True)
                            time.sleep(.03)  # Let Form establish its press grab.
                        finally:
                            view.pointer.button(False)
                    else:
                        form_key(window, 'Escape')
                    wait_for_predicate('diagnostic closes', lambda: view.named(title) is None,
                        time.monotonic() + 3, process=core)
                window = view.wait('FVWM-X')
                attributes = view.attributes(window)
                assert 0 < attributes.width <= 1440 and 0 < attributes.height <= 1200
                rows = native_text('SlateNoticeForm')
                joined = ''.join(rows)
                assert str(child) + ':5' in joined and str(parent) in joined, rows
                capture('runtime-startup-source.png')
                close(window, 'FVWM-X')
                command('KillModule FvwmDesktop')
                command('ModuleSynchronous Timeout 10 FvwmDesktop')
                barrier()
                window = view.wait('FVWM-X')
                replay = native_text('SlateNoticeForm')
                assert str(child) + ':5' in ''.join(replay), replay
                close(window, 'FVWM-X')
                live = user / 'origin-live'
                live.write_text('# live Read\n*FvwmDesktop: palette \\\n origin-invalid\n')
                command('Read origin-live')
                barrier()
                window = view.wait('FVWM-X')
                live_rows = native_text('SlateNoticeForm')
                assert str(live) + ':2-3' in ''.join(live_rows), live_rows
                capture('runtime-live-source.png')
                close(window, 'FVWM-X')
                desktop_log = (runtime.root / 'core.log').read_text(errors='replace')
                assert 'Source: ' + str(live) + ':2-3' in desktop_log, desktop_log
                evidence['runtime_notice'] = {'startup': rows, 'restart_replay': replay,
                    'live': live_rows, 'dismiss_route': 'XTest click on actual mapped Dismiss button'}
                # Overwrite only test-owned offline source after runtime Read is
                # finished. Dynamic/actions remain inert in CLI and native report.
                marker = runtime.root / 'offline-must-not-execute'
                local, checked_child = user / 'local.config', user / 'offline-child'
                local.write_text('# checker source\nRead offline-child\nExec touch ' + str(marker) +
                    '\nPipeRead "touch ' + str(marker) + '"\nModule /bin/sh -c "touch ' + str(marker) +
                    '"\n*FvwmDesktop: palette $[env.UNSET]\n')
                checked_child.write_text('# exact continued line\n*FvwmDesktop: palette \\\n invalid\n' +
                    '*FvwmDesktop: motion maybe\n*FvwmDesktop: unknown value\n')
                preserved = {path: path.read_bytes() for path in (local, checked_child)}
                offline_env = dict(env)
                offline_env.pop('DISPLAY', None)
                cli = subprocess.run([str(build / 'bin/FvwmPrompt/FvwmDesktop'),
                    '--check-config', str(local), '--json'], env=offline_env,
                    capture_output=True, text=True, timeout=8)
                assert cli.returncode == 1, (cli.returncode, cli.stderr)
                result = json.loads(cli.stdout)
                assert (result['errors'], result['unchecked']) == (3, 4), result
                text_cli = subprocess.run([str(build / 'bin/FvwmPrompt/FvwmDesktop'),
                    '--check-config', str(local)], env=offline_env,
                    capture_output=True, text=True, timeout=8)
                assert text_cli.returncode == 1 and json.dumps(str(checked_child), ensure_ascii=False) + ':2-3' in text_cli.stdout, text_cli
                assert 'Read from ' + json.dumps(str(local), ensure_ascii=False) + ':2' in text_cli.stdout, text_cli.stdout
                diagnostic = result['diagnostics'][0]
                assert (diagnostic['path'], diagnostic['line'], diagnostic['end_line']) == (str(checked_child), 2, 3), diagnostic
                assert diagnostic['included_from'] == [{'path': str(local), 'line': 2, 'end_line': 2}], diagnostic
                command('SendToModule FvwmDesktop check-config')
                window = view.wait('Configuration check')
                report = native_text('SlateConfigCheckForm')
                assert str(checked_child) + ':2-3' in ''.join(report), report
                capture('checker-source-page-1.png')
                def report_page():
                    for packet in probe.snapshot():
                        if packet['type'] == CONFIG and packet['text'].startswith('*SlateConfigCheckForm'):
                            header = re.search(r'page ([0-9]+)/([0-9]+)', packet['text'])
                            if header:
                                return int(header[1])
                    return 0
                form_key(window, 'F2')
                # New Form connections may reuse an X ID. Observe the actual
                # callback's new page rather than treating IDs as generations.
                wait_for_predicate('next native report page', lambda: report_page() == 2,
                    time.monotonic() + 4, process=core)
                window = view.wait('Configuration check')
                page2 = native_text('SlateConfigCheckForm')
                assert page2 != report, (report, page2)
                capture('checker-source-page-2.png')
                close(window, 'Configuration check')
                assert all(path.read_bytes() == value for path, value in preserved.items())
                assert not marker.exists()
                evidence['offline_checker'] = {'json': result, 'text': text_cli.stdout,
                    'native_page_1': report,
                    'native_page_2': page2, 'no_execution_or_source_writes': True,
                    'keyboard_route': 'native Form KeyPress via XSendEvent'}
                # Seven long literal include paths make a single diagnostic
                # cross page boundaries, including in the actual 18pt theme.
                long_paths = [user / ('checker-%d-' % i + 'q' * max(16,
                    220 - len(str(user)) - len('/checker-0-'))) for i in range(7)]
                assert all(200 <= len(str(path)) <= 240 for path in long_paths), long_paths
                local.write_text('# long-chain checker root\nRead "' + str(long_paths[0]) + '"\n')
                for index, path in enumerate(long_paths[:-1]):
                    path.write_text('# long-chain caller\nRead "' + str(long_paths[index + 1]) + '"\n')
                long_paths[-1].write_text('# long-chain leaf\n*FvwmDesktop: palette \\\n invalid\n')
                long_preserved = {path: path.read_bytes() for path in [local] + long_paths}
                wanted_include = [{'path': str(path), 'line': 2, 'end_line': 2}
                                  for path in [local] + long_paths[:-1]]
                long_cli = subprocess.run([str(build / 'bin/FvwmPrompt/FvwmDesktop'),
                    '--check-config', str(local), '--json'], env=offline_env,
                    capture_output=True, text=True, timeout=8)
                assert long_cli.returncode == 1, (long_cli.returncode, long_cli.stderr)
                long_result = json.loads(long_cli.stdout)
                assert long_result['errors'] == 1 and long_result['unchecked'] == 0, long_result
                item = long_result['diagnostics'][0]
                assert (item['path'], item['line'], item['end_line']) == (str(long_paths[-1]), 2, 3), item
                assert item['included_from'] == wanted_include, item
                command('SendToModule FvwmDesktop check-config')
                window, pages = view.wait('Configuration check'), []
                client.x.XTranslateCoordinates.argtypes = [C.c_void_p, C.c_ulong, C.c_ulong,
                    C.c_int, C.c_int, C.POINTER(C.c_int), C.POINTER(C.c_int), C.POINTER(C.c_ulong)]
                client.x.XTranslateCoordinates.restype = C.c_int
                while True:
                    page_packets = probe.snapshot()
                    rows = text_rows(page_packets, 'SlateConfigCheckForm')
                    lines = [re.sub(r'^\*SlateConfigCheckForm:?\s*', '', packet['text'])
                             for packet in page_packets if packet['type'] == CONFIG and
                             packet['text'].startswith('*SlateConfigCheckForm')]
                    headers = [re.search(r'page ([0-9]+)/([0-9]+)', line) for line in lines]
                    headers = [match for match in headers if match]
                    assert len(headers) == 1, lines
                    index, total = map(int, headers[0].groups())
                    assert index == len(pages) + 1 and 2 <= total <= 12, (index, total)
                    assert rows and len(rows) <= 12 and all(len(row) <= 72 for row in rows), rows
                    assert any(line.startswith('Button quit "Close"') for line in lines), lines
                    has_next = any(line.startswith('Button continue "Next (F2)"') for line in lines)
                    assert has_next == (index < total), lines
                    attributes = view.attributes(window)
                    assert attributes and 0 < attributes.width <= 1440 and 0 < attributes.height <= 1200
                    rx, ry, child_window = C.c_int(), C.c_int(), C.c_ulong()
                    assert client.x.XTranslateCoordinates(client.display, window, client.root,
                        0, 0, C.byref(rx), C.byref(ry), C.byref(child_window))
                    assert 0 <= rx.value and 0 <= ry.value
                    assert rx.value + attributes.width <= 1440 and ry.value + attributes.height <= 1200
                    controls = [view.attributes(child) for child in view.children(window)]
                    assert len(controls) == 1 + int(index > 1) + int(has_next), controls
                    for control_attributes in controls:
                        assert control_attributes and control_attributes.map_state == 2
                        assert 0 <= control_attributes.x and 0 <= control_attributes.y
                        assert control_attributes.x + control_attributes.width <= attributes.width
                        assert control_attributes.y + control_attributes.height <= attributes.height
                    pages.append({'page': index, 'total': total, 'rows': rows,
                                  'geometry': [rx.value, ry.value, attributes.width, attributes.height],
                                  'mapped_controls': len(controls)})
                    if index == 1:
                        capture('checker-long-chain.png')
                    if not has_next:
                        close(window, 'Configuration check')
                        break
                    form_key(window, 'F2')
                    wait_for_predicate('long report callback advances', lambda:
                        report_page() == index + 1, time.monotonic() + 4, process=core)
                    window = view.wait('Configuration check')
                complete_text = ''.join(row for page in pages for row in page['rows'])
                assert str(long_paths[-1]) + ':2-3' in complete_text, pages
                positions = []
                for entry in wanted_include:
                    expected_text = 'Read from ' + entry['path'] + ':2'
                    assert complete_text.count(expected_text) == 1, pages
                    positions.append(complete_text.index(expected_text))
                assert positions == sorted(positions), pages
                assert complete_text.count('Read from ') == len(wanted_include), pages
                assert all(path.read_bytes() == content for path, content in long_preserved.items())
                assert len(pages) == pages[0]['total'] and len(pages) >= 3, pages
                assert not marker.exists()
                evidence['long_chain_pagination'] = {'expected_include': wanted_include,
                    'expected_leaf': str(long_paths[-1]) + ':2-3', 'pages': pages}
            control.send(b'Quit\n')
            assert core.wait_until_exit(time.monotonic() + 4) == 0
            if errors:
                assert not errors.unexpected, errors.unexpected
            evidence['status'] = 'passed'
        finally:
            failure = sys.exc_info()[1]
            if failure:
                evidence.update(status='failed', failure=repr(failure))
            if control:
                control.close()
            if core:
                core.close(time.monotonic() + 4)
            if errors:
                errors.close_display()
            elif client:
                client.close()
            for path in runtime.root.glob('*.log'):
                shutil.copy2(path, out / path.name)
            evidence['elapsed_seconds'] = time.monotonic() - started
            (out / 'result.json').write_text(json.dumps(evidence, indent=2) + '\n')
    print(json.dumps({'profile': profile, 'status': evidence['status'],
                      'elapsed_seconds': evidence['elapsed_seconds']}))


if __name__ == '__main__':
    if len(sys.argv) > 1 and sys.argv[1] == '--probe':
        module_probe(sys.argv[3:], bool(int(sys.argv[2])))
        raise SystemExit(0)
    parser = argparse.ArgumentParser(description=__doc__, allow_abbrev=False)
    parser.add_argument('--build', required=True, type=Path)
    parser.add_argument('--artifacts-dir', required=True, type=Path)
    parser.add_argument('--profiles', nargs='+', choices=PROFILES, default=list(PROFILES))
    parser.add_argument('--core-only', action='store_true')
    options = parser.parse_args()
    options.artifacts_dir.resolve().mkdir(parents=True, exist_ok=False)
    def deadline(signum, frame):
        raise TimeoutError('configuration origin checkpoint exceeded 120 seconds')
    signal.signal(signal.SIGALRM, deadline)
    signal.alarm(120)
    for selected_profile in (['core'] if options.core_only else options.profiles):
        run(options, selected_profile)
    signal.alarm(0)
