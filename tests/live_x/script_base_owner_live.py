#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or any later version.
"""Observe actual base ownership through startup, callbacks and shutdown."""
import argparse
import ctypes as C
import json
import os
from pathlib import Path
import selectors
import shutil
import signal
import struct
import time

from geometry_transition_live import Client
from live_x_harness import start_display_server, wait_for_predicate
from script_block_owner_live import ProbeRuntime
from script_transport_live import packet, WORD


CASES = ('father', 'child', 'invalid-cursor', 'colorset',
    'borrowed-colorset', 'partial-child-base', 'partial-child-id',
    'partial-widget', 'partial-prepare', 'init-quit', 'event-quit',
    'periodic-quit', 'dead-pipe')


def start(runtime, args, display, label, source, child):
    incoming, producer = os.pipe()
    consumer, outgoing = os.pipe()
    command = [str(args.module.resolve()), str(outgoing), str(incoming),
        '/dev/null', '0', '0', str(source)]
    if child is not None:
        command.append(os.fsdecode(child))
    command.append('borrowed-script-argument')
    process = runtime.start(label, command, label + '.log',
        pass_fds=(incoming, outgoing),
        environment=runtime.environment({'DISPLAY': display}))
    os.close(incoming)
    os.close(outgoing)
    os.set_blocking(consumer, False)
    buffer, commands = bytearray(), []
    try:
        with selectors.DefaultSelector() as ready:
            ready.register(consumer, selectors.EVENT_READ)
            deadline = time.monotonic() + 10
            while not any('Send_ConfigInfo' in c for c in commands):
                remaining = deadline - time.monotonic()
                assert remaining > 0 and ready.select(remaining)
                data = os.read(consumer, 65536)
                assert data
                buffer.extend(data)
                while len(buffer) >= 2 * WORD:
                    _, length = struct.unpack('@LL', buffer[:2 * WORD])
                    assert length <= 1000
                    size = 3 * WORD + length
                    if len(buffer) < size:
                        break
                    commands.append(bytes(buffer[2 * WORD:
                        2 * WORD + length]).decode())
                    del buffer[:size]
        cursor = 'invalid-cursor' if label == 'invalid-cursor' else 'left_ptr'
        for config in ('*FvwmScriptDefaultFont fixed',
                '*FvwmScriptKeyboardFocus BaseOwners',
                '*FvwmScriptDefaultCursor ' + cursor):
            os.write(producer, packet(1 << 18, (0, 0, 0), config))
        os.write(producer, packet(1 << 19))
        return process, producer, consumer
    except BaseException:
        os.close(producer)
        os.close(consumer)
        raise


def source_text(label, report):
    initial = f' WriteToFile {report} {{init}}\n'
    if not label.startswith('partial-'):
        initial += ' Set $launched=(LaunchScript {UnusedChild})\n'
    if label == 'init-quit':
        initial += ' Quit\n'
    if label == 'colorset':
        initial += ' ChangeColorset 0 0\n ChangeColorset 0 1\n'
    periodic = (' Quit\n' if label == 'periodic-quit' else '')
    event = ' Quit\n' if label == 'event-quit' else ''
    window = '65536 100' if label == 'partial-child-base' else '220 120'
    position = '32768 0' if label == 'partial-widget' else '20 20'
    title = 'W' * 12000 if label == 'partial-prepare' else 'Label'
    return ('WindowTitle {BaseOwners}\nWindowSize ' + window + '\n'
        'Font {fixed}\nIcon owned-base-icon\n'
        'ForeColor {black}\nBackColor {grey85}\nShadowColor {grey55}\n'
        'HilightColor {grey100}\n' +
        ('Colorset 0\n' if label == 'borrowed-colorset' else '') +
        'Init\nBegin\n' + initial + 'End\n'
        'PeriodicTasks\nBegin\n' + periodic + 'End\n'
        f'QuitFunc\nBegin\n WriteToFile {report} {{quit}}\nEnd\n'
        'Widget 1\nProperty\nType SwallowExec\nPosition 1 1\n'
        'Size 32 32\nMain\nCase message of\nEnd\n'
        'Widget 2\nProperty\nType ItemDraw\nPosition ' + position + '\n'
        'Size 60 30\nTitle {' + title + '}\n'
        'Main\nCase message of\n1 :\nBegin\n' + event + 'End\nEnd\n')


def run(args):
    args.artifacts_dir.mkdir(parents=True, exist_ok=True)
    evidence = {}
    with ProbeRuntime(prefix='fvwm-script-base-') as runtime:
        client = None
        try:
            display = args.display
            if display is None:
                _, display = start_display_server(runtime, 'Xvfb',
                    ['Xvfb', '-screen', '0', '800x600x24', '-noreset'],
                    'xserver.log', time.monotonic() + 10)
            if args.color_probe is not None:
                colors = runtime.start('color-values',
                    [str(args.color_probe.resolve()), '--live'],
                    'color-values.log', environment=runtime.environment(
                        {'DISPLAY': display}))
                assert colors.wait_until_exit(time.monotonic() + 10) == 0
                evidence['colorset-rgb-values'] = 'passed'
            client = Client(display)
            x, d = client.x, client.display
            x.XInternAtom.argtypes = [C.c_void_p, C.c_char_p, C.c_int]
            x.XInternAtom.restype = C.c_ulong
            for label in args.case or CASES:
                report = runtime.root / (label + '.report')
                ownership = runtime.root / (label + '.owners.json')
                runtime.probe_environment = {'SCRIPT_BASE_SCENARIO': label,
                    'SCRIPT_BASE_OWNER_REPORT': str(ownership)}
                child = None
                if 'child' in label:
                    child = b'\xa1base-owner10'
                    if label == 'partial-child-id':
                        child = b'\xa1x'
                    else:
                        assert x.XInternAtom(d, child, 0)
                source = runtime.root / label
                source.write_text(source_text(label, report))
                process, producer, consumer = start(runtime, args, display,
                    label, source, child)
                try:
                    if label.startswith('partial-'):
                        expected = 1
                    elif label in ('init-quit', 'periodic-quit', 'dead-pipe'):
                        expected = 0
                    else:
                        wait_for_predicate(label + ' Init',
                            lambda: report.exists() and
                                'init' in report.read_text(),
                            time.monotonic() + 10, process=process)
                        if label == 'event-quit':
                            os.write(producer, packet(1 << 22, (0, 0, 0),
                                'SendString 2 1'))
                        else:
                            os.kill(process.process.pid, signal.SIGTERM)
                        expected = 0
                    assert process.wait_until_exit(time.monotonic() + 10) == expected
                    lines = report.read_text() if report.exists() else ''
                    if label.startswith('partial-'):
                        if label == 'partial-child-id':
                            # WriteToFile frames records with source metadata.
                            # ID validation follows QuitFunc publication.
                            payload = [line for line in lines.splitlines()
                                if line and not line.startswith('#')]
                            assert payload == ['quit']
                        else:
                            assert lines == ''
                            assert 'invalid geometry' in process.log_path.read_text()
                    else:
                        assert 'init' in lines and 'quit' in lines
                    owners = json.loads(ownership.read_text())
                    active = label in ('init-quit', 'event-quit', 'periodic-quit')
                    assert (owners['retained'] > 0) == active
                    assert owners['cursor_create'] == owners['cursor_free']
                    evidence[label] = {'lifecycle': 'passed', 'owners': owners}
                finally:
                    os.close(producer)
                    os.close(consumer)
            for log in runtime.root.glob('*.log'):
                text = log.read_text()
                assert not any(s in text for s in ('AddressSanitizer',
                    'runtime error:', 'Assertion', 'X Error of failed request')), text
        finally:
            if client is not None:
                client.close()
            for path in runtime.root.iterdir():
                if path.is_file():
                    shutil.copy2(path, args.artifacts_dir / path.name)
            (args.artifacts_dir / 'evidence.json').write_text(
                json.dumps(evidence, indent=2) + '\n')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(allow_abbrev=False)
    parser.add_argument('--module', type=Path, required=True)
    parser.add_argument('--artifacts-dir', type=Path, required=True)
    parser.add_argument('--display')
    parser.add_argument('--color-probe', type=Path)
    parser.add_argument('--case', action='append', choices=CASES)
    run(parser.parse_args())
