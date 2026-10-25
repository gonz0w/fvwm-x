#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or any later version.
"""Run real Script lifecycle cases, optionally observing scoped frees.

For ownership metrics pass the private SCRIPT_BLOCK_MODULE_PROBE executable
built from test_script_block_owner.c and real module objects as --module.
An ordinary production module still establishes the lifecycle behavior.
"""
import argparse
import json
import os
from pathlib import Path
import shutil
import signal
import time

from live_x_harness import (HarnessRuntime, start_display_server,
    wait_for_predicate)
from script_transport_live import packet
from script_widget_owner_live import start


MIXED = '''Do {Echo ordinary}
If 1==1 Then
Begin
 Do {Echo then}
End
Else
Begin
 Do {Echo else}
End
For $counter=1 To 2 Do
Begin
 Do {Echo loop}
End
While 0==1 Do
Begin
 If 1==1 Then
 Begin
  Do {Echo nested}
 End
End
'''


class ProbeRuntime(HarnessRuntime):
    probe_environment = None

    def environment(self, overrides=None):
        additions = dict(self.probe_environment or {})
        additions.update(overrides or {})
        return super().environment(additions)


def source_text(label, report):
    def write(message):
        return f' WriteToFile {report} {{{message}}}\n'

    initial = MIXED + write('init')
    periodic = write('periodic')
    event = write('event')
    if label == 'init-quit':
        initial += ' Quit\n'
    if label == 'periodic-quit':
        periodic += ' Quit\n'
    if label == 'event-quit':
        event += ' Quit\n'
    refused = '32768 0' if label == 'partial-startup' else '25 25'
    return ('WindowTitle {BlockOwners}\nWindowSize 200 100\nFont {fixed}\n'
        'Init\nBegin\n' + initial + 'End\n'
        'PeriodicTasks\nBegin\n' + periodic + 'End\n'
        'QuitFunc\nBegin\n' + write('quit') + 'End\n'
        'Widget 1\nProperty\nType ItemDraw\nPosition 1 1\nSize 20 20\n'
        'Main\nCase message of\n1 :\nBegin\n' + event +
        'End\n2 :\nBegin\nEnd\nEnd\n'
        'Widget 2\nProperty\nType ItemDraw\nPosition ' + refused + '\n'
        'Size 20 20\nMain\nCase message of\nEnd\n')


def run(args):
    args.artifacts_dir.mkdir(parents=True, exist_ok=True)
    evidence = {}
    with ProbeRuntime(prefix='fvwm-script-blocks-') as runtime:
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
                    'SCRIPT_BLOCK_SCENARIO': label,
                    'SCRIPT_BLOCK_OWNER_REPORT': str(ownership),
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
                                'init' in report.read_text(),
                            time.monotonic() + 10, process=process)
                        # Successful Init and its full release precede this
                        # module's final startup notification in the probe.
                        if label == 'event-quit':
                            os.write(producer, packet(1 << 22, (0, 0, 0),
                                'SendString 1 1'))
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
                        assert 'init' in lines and 'quit' in lines
                    if label == 'event-quit':
                        assert 'event' in lines
                    if label == 'periodic-quit':
                        assert 'periodic' in lines
                    entry = {'lifecycle': 'passed'}
                    if args.require_ownership:
                        observed = json.loads(ownership.read_text())
                        if label in ('init-quit', 'event-quit',
                                'periodic-quit'):
                            assert observed['retained'] > 0
                        else:
                            assert observed['retained'] == 0
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
