#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or any later version.
"""Exercise real Script bindings and message owners through module shutdown.

The optional private SCRIPT_BINDING_MODULE_PROBE build of
test_script_binding_owner.c records exact constructor/copy/message lifetimes.
"""
import argparse
import ctypes as C
import json
import os
from pathlib import Path
import shutil
import signal
import time

from geometry_transition_live import Client
from live_x_harness import start_display_server, wait_for_predicate
from script_block_owner_live import ProbeRuntime
from script_transport_live import packet
from script_widget_owner_live import start
from selection_live import Event, Key


def source_text(label, report):
    initial = (' Set $ready={0}\n'
        ' Key F12 N 1 1 {old payload}\n'
        ' Key F12 N 1 1 {new payload}\n'
        ' Key F11 S 1 1 {shift payload}\n'
        f' WriteToFile {report} {{init}}\n')
    if label == 'init-quit':
        initial += ' Quit\n'
    periodic = ''
    if label == 'periodic-quit':
        periodic = (' If $ready==1 Then\n Begin\n'
            f' WriteToFile {report} {{periodic:}} (LastString)\n'
            ' Quit\n End\n')
    event = f' WriteToFile {report} {{event:}} (LastString)\n'
    if label == 'event-quit':
        event += ' Quit\n'
    refused = '32768 0' if label == 'partial-startup' else '25 25'
    return ('WindowTitle {BindingOwners}\nWindowSize 200 100\nFont {fixed}\n'
        'Init\nBegin\n' + initial + 'End\n'
        'PeriodicTasks\nBegin\n' + periodic + 'End\n'
        'QuitFunc\nBegin\n'
        f' WriteToFile {report} {{quit:}} (LastString)\n'
        ' Key F10 N 1 1 {quit binding}\nEnd\n'
        'Widget 1\nProperty\nType ItemDraw\nPosition 1 1\nSize 20 20\n'
        'Main\nCase message of\n1 :\nBegin\n' + event + 'End\n'
        '2 :\nBegin\n Set $ready={1}\n'
        f' WriteToFile {report} {{armed:}} (LastString)\nEnd\nEnd\n'
        'Widget 2\nProperty\nType ItemDraw\nPosition ' + refused + '\n'
        'Size 20 20\nMain\nCase message of\nEnd\n')


def run(args):
    args.artifacts_dir.mkdir(parents=True, exist_ok=True)
    evidence = {}
    with ProbeRuntime(prefix='fvwm-script-bindings-') as runtime:
        client = None
        try:
            display = args.display
            if display is None:
                _, display = start_display_server(runtime, 'Xvfb',
                    ['Xvfb', '-screen', '0', '800x600x24', '-noreset'],
                    'xserver.log', time.monotonic() + 10)
            client = Client(display)
            x, d = client.x, client.display
            for name, (arguments, result) in {
                'XFetchName': ([C.c_void_p, C.c_ulong,
                    C.POINTER(C.c_void_p)], C.c_int),
                'XKeysymToKeycode': ([C.c_void_p, C.c_ulong], C.c_uint),
                'XSendEvent': ([C.c_void_p, C.c_ulong, C.c_int, C.c_long,
                    C.POINTER(Event)], C.c_int),
            }.items():
                getattr(x, name).argtypes = arguments
                getattr(x, name).restype = result

            def named():
                root, parent, items, count = (C.c_ulong(), C.c_ulong(),
                    C.POINTER(C.c_ulong)(), C.c_uint())
                assert x.XQueryTree(d, client.root, C.byref(root),
                    C.byref(parent), C.byref(items), C.byref(count))
                windows = [items[i] for i in range(count.value)]
                if items:
                    x.XFree(items)
                for window in windows:
                    value = C.c_void_p()
                    if x.XFetchName(d, window, C.byref(value)) and value:
                        title = C.string_at(value).decode('utf-8', 'replace')
                        x.XFree(value)
                        if title == 'BindingOwners':
                            return window
                return None

            def key(window, keysym, state=0):
                event = Event()
                event.key = Key(type=2, display=d, window=window,
                    root=client.root, keycode=x.XKeysymToKeycode(d, keysym),
                    state=state, same_screen=1)
                assert x.XSendEvent(d, window, 0, 1, C.byref(event))
                x.XSync(d, 0)

            for label in ('final-shutdown', 'partial-startup', 'dead-pipe',
                    'init-quit', 'event-quit', 'periodic-quit'):
                report = runtime.root / (label + '.report')
                ownership = runtime.root / (label + '.ownership.json')
                runtime.probe_environment = {
                    'SCRIPT_BINDING_SCENARIO': label,
                    'SCRIPT_BINDING_OWNER_REPORT': str(ownership),
                }
                source = runtime.root / label
                source.write_text(source_text(label, report))
                process, producer, consumer = start(runtime, args, display,
                    label, source)
                try:
                    def lines():
                        return report.read_text() if report.exists() else ''

                    def wait_line(text):
                        wait_for_predicate(label + ' ' + text,
                            lambda: text in lines(), time.monotonic() + 10,
                            process=process)

                    if label in ('partial-startup', 'init-quit'):
                        expected = 1 if label == 'partial-startup' else 0
                    else:
                        wait_line('init')
                        if label in ('final-shutdown', 'dead-pipe'):
                            wait_for_predicate('binding window', named,
                                time.monotonic() + 10, process=process)
                            window = named()
                            # Most recent duplicate wins; caps lock is ignored.
                            key(window, 0xffc9, 2)  # F12 + LockMask
                            wait_line('event:new payload')
                            key(window, 0xffc9, 1)  # No Shift-F12 binding.
                            key(window, 0xffc8, 1)  # F11 + ShiftMask
                            wait_line('event:shift payload')
                            # The matching F11 event acknowledges the preceding
                            # unmatched F12 event in the same X connection.
                            assert lines().count('event:new payload') == 1
                        signal_number = 2 if label == 'periodic-quit' else 1
                        payload = 'latest message with spaces'
                        os.write(producer, packet(1 << 22, (0, 0, 0),
                            f'SendString 1 {signal_number} {payload}'))
                        wait_line(('armed:' if signal_number == 2 else
                            'event:') + payload)
                        if label == 'dead-pipe':
                            os.close(producer)
                            producer = None
                        elif label == 'final-shutdown':
                            os.kill(process.process.pid, signal.SIGTERM)
                        expected = 0
                    status = process.wait_until_exit(time.monotonic() + 10)
                    assert status == expected
                    text = lines()
                    if label == 'partial-startup':
                        assert text == ''
                        log = process.log_path.read_text()
                        assert 'invalid geometry' in log
                    elif label == 'init-quit':
                        assert 'init' in text and 'quit:' in text
                    else:
                        assert 'quit:latest message with spaces' in text
                    if label == 'periodic-quit':
                        assert 'periodic:latest message with spaces' in text
                    entry = {'lifecycle': 'passed'}
                    if args.require_ownership:
                        observed = json.loads(ownership.read_text())
                        assert (observed['constructors'] ==
                            observed['constructors_freed'])
                        if label in ('init-quit', 'event-quit',
                                'periodic-quit'):
                            assert observed['retained'] > 0
                        else:
                            assert observed['retained'] == 0
                        if label not in ('init-quit', 'partial-startup'):
                            assert observed['messages'] > 0
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
    parser.add_argument('--require-ownership', action='store_true')
    run(parser.parse_args())
