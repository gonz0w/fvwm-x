#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or any later version.
"""Check property transfer, callbacks and startup refusal on the real module.

The private SCRIPT_PROPERTY_MODULE_PROBE build also observes exact owners.
"""
import argparse
import json
import os
from pathlib import Path
import selectors
import shutil
import signal
import struct
import time

from live_x_harness import start_display_server, wait_for_predicate
from script_block_owner_live import ProbeRuntime
from script_icon_live import bitmap
from script_transport_live import packet, WORD


def start(runtime, args, display, label, source):
    incoming, producer = os.pipe()
    consumer, outgoing = os.pipe()
    process = runtime.start(label, [str(args.module.resolve()), str(outgoing),
        str(incoming), '/dev/null', '0', '0', str(source)], label + '.log',
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
        # Exercise the heap font established by actual ParseOptions.
        os.write(producer, packet(1 << 18, (0, 0, 0),
            '*FvwmScriptDefaultFont fixed'))
        os.write(producer, packet(1 << 19))
        return process, producer, consumer
    except BaseException:
        os.close(producer)
        os.close(consumer)
        raise


def source_text(label, report, icon):
    def write(stage):
        # WriteToFile takes STR/VAR for its pathname, not a brace GSTR.
        return f' WriteToFile $file {{{stage}:}} (GetTitle 1)\n'

    initial = f' Set $file={{{report}}}\n' + write('init')
    if label == 'init-quit':
        initial += ' Quit\n'
    periodic = write('periodic')
    if label == 'periodic-quit':
        periodic += ' Quit\n'
    event = write('event') + (' Quit\n' if label == 'event-quit' else '')
    source = ('WindowTitle {old}\nWindowLocaleTitle {untranslated}\n'
        'WindowTitle {PropertyOwners}\nWindowSize 260 150\n'
        'Font old\nFont ""\nFont fixed\n')
    for key, color in (('ForeColor', 'black'), ('BackColor', 'grey85'),
            ('ShadowColor', 'grey55'), ('HilightColor', 'grey100')):
        source += f'{key} {{old}}\n{key} {{}}\n{key} {{{color}}}\n'
    source += ('Colorset 0\nInit\nBegin\n' + initial + 'End\n'
        'PeriodicTasks\nBegin\n' + periodic + 'End\n'
        'QuitFunc\nBegin\n' + write('quit') + 'End\n')
    for n in range(1, 5):
        position = '32768 0' if n == 4 and label == 'before-prepare' else (
            f'{n * 25} 25')
        source += (f'Widget {n}\nProperty\nType Rectangle\nType ItemDraw\n'
            f'Position {position}\nSize 20 20\n')
        if n == 1:
            source += ('Title {old}\nLocaleTitle {untranslated}\n'
                'Title {explicit}\nSwallowExec {old}\nSwallowExec {}\n'
                f'Icon {icon}\nIcon {icon}\nFont old\nFont fixed\n')
            for key in ('ForeColor', 'BackColor', 'ShadowColor',
                    'HilightColor'):
                source += f'{key} {{old}}\n{key} {{black}}\n'
            # Named colors followed by a colorset preserve colorset precedence.
            source += 'Colorset 0\n'
        elif n == 3:
            # A named override after a colorset disables it.
            source += 'Colorset 0\nForeColor {black}\n'
        elif n == 4 and label == 'prepare-refusal':
            source += 'Title {' + 'W' * 12000 + '}\n'
        source += 'Main\nCase message of\n'
        if n == 1:
            source += '1 :\nBegin\n' + event + 'End\n'
        source += 'End\n'
    return source


def run(args):
    args.artifacts_dir.mkdir(parents=True, exist_ok=True)
    evidence = {}
    with ProbeRuntime(prefix='fvwm-script-properties-') as runtime:
        try:
            display = args.display
            if display is None:
                _, display = start_display_server(runtime, 'Xvfb',
                    ['Xvfb', '-screen', '0', '800x600x24', '-noreset'],
                    'xserver.log', time.monotonic() + 10)
            icon = runtime.root / 'property.xbm'
            bitmap(icon, True)
            for label in ('final-shutdown', 'before-prepare', 'prepare-refusal',
                    'dead-pipe', 'init-quit', 'event-quit', 'periodic-quit'):
                report = runtime.root / (label + '.report')
                ownership = runtime.root / (label + '.ownership.json')
                runtime.probe_environment = {
                    'SCRIPT_PROPERTY_SCENARIO': label,
                    'SCRIPT_PROPERTY_OWNER_REPORT': str(ownership),
                }
                if args.baseline:
                    runtime.probe_environment['SCRIPT_PROPERTY_BASELINE'] = '1'
                source = runtime.root / label
                source.write_text(source_text(label, report, icon))
                process, producer, consumer = start(runtime, args, display,
                    label, source)
                try:
                    refused = label in ('before-prepare', 'prepare-refusal')
                    if not refused and label not in ('init-quit',
                            'periodic-quit'):
                        wait_for_predicate(label + ' Init',
                            lambda: report.exists() and
                                'init:explicit' in report.read_text(),
                            time.monotonic() + 10, process=process)
                        if label in ('event-quit', 'final-shutdown'):
                            os.write(producer, packet(1 << 22, (0, 0, 0),
                                'SendString 1 1'))
                            if label == 'final-shutdown':
                                wait_for_predicate('event',
                                    lambda: 'event:explicit' in
                                        report.read_text(),
                                    time.monotonic() + 10, process=process)
                                os.kill(process.process.pid, signal.SIGTERM)
                        elif label == 'dead-pipe':
                            os.close(producer)
                            producer = None
                    assert process.wait_until_exit(time.monotonic() + 10) == (
                        1 if refused else 0)
                    lines = report.read_text() if report.exists() else ''
                    if refused:
                        assert lines == ''
                        assert 'invalid geometry' in (
                            process.log_path.read_text())
                    else:
                        assert 'init:explicit\n' in lines
                        assert 'quit:explicit\n' in lines
                    if label in ('event-quit', 'final-shutdown'):
                        assert 'event:explicit\n' in lines
                    if label == 'periodic-quit':
                        assert 'periodic:explicit\n' in lines
                    entry = {'lifecycle': 'passed'}
                    if args.require_ownership:
                        observed = json.loads(ownership.read_text())
                        assert observed['defaults_freed'] == (
                            0 if args.baseline else 5)
                        assert observed['chosen'] == (
                            3 if label == 'before-prepare' else 4)
                        assert observed['types_freed'] == (0 if args.baseline
                            else observed['chosen'])
                        assert (observed['widget_strings_freed'] ==
                            observed['widget_strings'])
                        active = label in ('init-quit', 'event-quit', 'periodic-quit')
                        assert observed['base_retained'] == (
                            6 if args.baseline or active else 0)
                        entry['ownership'] = observed
                    evidence[label] = entry
                finally:
                    if producer is not None:
                        os.close(producer)
                    os.close(consumer)
            for log in runtime.root.glob('*.log'):
                text = log.read_text()
                assert not any(marker in text for marker in (
                    'AddressSanitizer', 'runtime error:', 'Assertion',
                    'X Error of failed request')), text
        finally:
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
    parser.add_argument('--require-ownership', action='store_true')
    parser.add_argument('--baseline', action='store_true')
    run(parser.parse_args())
