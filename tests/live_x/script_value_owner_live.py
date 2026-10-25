#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or any later version.
"""Exercise complete Script values through real callbacks and idle shutdown.

For scoped ownership counts use the private SCRIPT_VALUE_MODULE_PROBE executable
built from test_script_value_owner.c and real module objects as --module.
"""
import argparse
import json
import os
from pathlib import Path
import shutil
import signal
import time

from live_x_harness import (start_display_server, wait_for_predicate)
from script_block_owner_live import ProbeRuntime
from script_transport_live import packet
from script_widget_owner_live import start


def source_text(label, report):
    initial = (f' Set $file={{{report}}}\n'
        ' Set $text={seed}\n Set $text=$text\n Set $empty={}\n'
        ' For $counter=1 To 2 Do\n Begin\n'
        '  Set $counter=$counter\n  Set $text=$text {:} $counter\n'
        '  WriteToFile $file {loop:} $counter\n End\n'
        ' WriteToFile $file {init:} $text\n')
    periodic = ' WriteToFile $file {periodic:} $text\n'
    event = ' WriteToFile $file {event:} $text {:} (LastString)\n'
    if label == 'init-quit':
        initial += ' Quit\n'
    if label == 'periodic-quit':
        periodic += ' Quit\n'
    if label == 'event-quit':
        event += ' Quit\n'
    position = '32768 0' if label == 'partial-startup' else '25 25'
    return ('WindowTitle {ValueOwners}\nWindowSize 200 100\nFont {fixed}\n'
        'Init\nBegin\n' + initial + 'End\n'
        'PeriodicTasks\nBegin\n' + periodic + 'End\n'
        'QuitFunc\nBegin\n Set $text=$text {:quit}\n'
        ' WriteToFile $file {quit:} $text\nEnd\n'
        'Widget 1\nProperty\nType ItemDraw\nPosition 1 1\nSize 20 20\n'
        'Main\nCase message of\n1 :\nBegin\n' + event + 'End\nEnd\n'
        'Widget 2\nProperty\nType ItemDraw\nPosition ' + position + '\n'
        'Size 20 20\nMain\nCase message of\nEnd\n')


def run(args):
    args.artifacts_dir.mkdir(parents=True, exist_ok=True)
    evidence = {}
    with ProbeRuntime(prefix='fvwm-script-values-') as runtime:
        try:
            display = args.display
            if display is None:
                _, display = start_display_server(runtime, 'Xvfb',
                    ['Xvfb', '-screen', '0', '800x600x24', '-noreset'],
                    'xserver.log', time.monotonic() + 10)
            for label in ('final-shutdown', 'partial-startup', 'dead-pipe',
                    'init-quit', 'event-quit', 'periodic-quit'):
                report = runtime.root / (label + '.report')
                ownership = runtime.root / (label + '.ownership.json')
                runtime.probe_environment = {
                    'SCRIPT_VALUE_SCENARIO': label,
                    'SCRIPT_VALUE_OWNER_REPORT': str(ownership),
                }
                source = runtime.root / label
                source.write_text(source_text(label, report))
                process, producer, consumer = start(runtime, args, display,
                    label, source)
                try:
                    if label in ('partial-startup', 'init-quit',
                            'periodic-quit'):
                        expected = 1 if label == 'partial-startup' else 0
                    else:
                        wait_for_predicate(label + ' Init',
                            lambda: report.exists() and
                                'init:seed:1:2' in report.read_text(),
                            time.monotonic() + 10, process=process)
                        if label == 'event-quit':
                            os.write(producer, packet(1 << 22, (0, 0, 0),
                                'SendString 1 1 payload'))
                        elif label == 'dead-pipe':
                            os.close(producer)
                            producer = None
                        else:
                            os.kill(process.process.pid, signal.SIGTERM)
                        expected = 0
                    status = process.wait_until_exit(time.monotonic() + 10)
                    assert status == expected
                    lines = report.read_text() if report.exists() else ''
                    if label == 'partial-startup':
                        assert lines == ''
                        log = process.log_path.read_text()
                        assert 'invalid geometry' in log
                    else:
                        for text in ('loop:1\n', 'loop:2\n',
                                'init:seed:1:2\n', 'quit:seed:1:2:quit\n'):
                            assert text in lines, lines
                    if label == 'event-quit':
                        assert 'event:seed:1:2:payload\n' in lines
                    if label == 'periodic-quit':
                        assert 'periodic:seed:1:2\n' in lines
                    entry = {'lifecycle': 'passed'}
                    if args.require_ownership:
                        observed = json.loads(ownership.read_text())
                        if label in ('init-quit', 'event-quit',
                                'periodic-quit'):
                            assert observed['retained'] == (
                                2 * observed['entries'] + 2)
                            assert observed['names_freed'] == 0
                            assert observed['arrays_freed'] == 0
                            assert observed['clears'] == 0
                            assert observed['used'] > 0
                        else:
                            assert observed['retained'] == 0
                            assert observed['names_freed'] == observed['names']
                            assert (observed['values_freed'] ==
                                observed['values'])
                            assert observed['arrays_freed'] == 2
                            assert observed['clears'] == 1
                            assert observed['used'] == 0
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
    run(parser.parse_args())
