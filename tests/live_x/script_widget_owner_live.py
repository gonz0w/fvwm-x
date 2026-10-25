#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or any later version.
"""Exercise all Script widget owners through normal and refused startup."""
import argparse
import ctypes as C
import json
import os
from pathlib import Path
import selectors
import shutil
import struct
import time

from geometry_transition_live import Client
from live_x_harness import HarnessRuntime, start_display_server, wait_for_predicate
from script_icon_live import bitmap
from script_transport_live import packet, WORD


WIDGETS = ('PushButton', 'RadioButton', 'ItemDraw', 'CheckBox', 'TextField',
    'HScrollBar', 'VScrollBar', 'PopupMenu', 'Rectangle', 'MiniScroll',
    'SwallowExec', 'HDipstick', 'VDipstick', 'List', 'Menu')


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
            while not any('Send_ConfigInfo' in command for command in commands):
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
        os.write(producer, packet(1 << 19))
        return process, producer, consumer
    except BaseException:
        os.close(producer)
        os.close(consumer)
        raise


def run(args):
    args.artifacts_dir.mkdir(parents=True, exist_ok=True)
    evidence = {}
    with HarnessRuntime(prefix='fvwm-script-widgets-') as runtime:
        client = None
        try:
            display = args.display
            if display is None:
                _, display = start_display_server(runtime, 'Xvfb',
                    ['Xvfb', '-screen', '0', '800x600x24', '-noreset'],
                    'xserver.log', time.monotonic() + 10)
            client = Client(display)
            x, d = client.x, client.display
            x.XFetchName.argtypes = [C.c_void_p, C.c_ulong,
                C.POINTER(C.c_void_p)]
            x.XFetchName.restype = C.c_int

            def children(window):
                root, parent, items, count = (C.c_ulong(), C.c_ulong(),
                    C.POINTER(C.c_ulong)(), C.c_uint())
                assert x.XQueryTree(d, window, C.byref(root), C.byref(parent),
                    C.byref(items), C.byref(count))
                result = [items[n] for n in range(count.value)]
                if items:
                    x.XFree(items)
                return result

            def named(title):
                for window in children(client.root):
                    value = C.c_void_p()
                    if x.XFetchName(d, window, C.byref(value)) and value:
                        text = C.string_at(value).decode('utf-8', 'replace')
                        x.XFree(value)
                        if text == title:
                            return window
                return None

            icon = runtime.root / 'widget.xbm'
            bitmap(icon, True)
            report = runtime.root / 'widgets.report'
            operations = {
                1: '\n '.join(f'ChangeFont {n} {{fixed}}'
                    for n in range(1, 16)),
                2: '\n '.join(f'ChangeColorset {n} 0'
                    for n in range(1, 16)),
                3: '\n '.join(f'ChangeForeColor {n} {{black}}'
                    for n in range(1, 16)),
                4: '\n '.join(f'ChangeFont {n} {{Shadow=65536:fixed}}'
                    for n in range(1, 16)),
                5: '\n '.join(f'ChangeFont {n} {{fixed}}'
                    for n in range(1, 16)),
            }
            cases = ''.join(f'{n} :\nBegin\n {body}\n'
                f' WriteToFile $file {{event:{n}}}\nEnd\n'
                for n, body in operations.items())
            widgets = []
            for n, kind in enumerate(WIDGETS, 1):
                title = kind + '|One|Two' if kind in (
                    'List', 'Menu', 'PopupMenu') else kind
                widgets.append(f'Widget {n}\nProperty\n Type {kind}\n'
                    f' Position {10 + ((n - 1) % 3) * 240} '
                    f'{30 + ((n - 1) // 3) * 100}\n Size 180 65\n'
                    f' Title {{{title}}}\n Value 5\n MinValue 0\n'
                    f' MaxValue 10\n Icon {icon}\n'
                    'Main\nCase message of\n' +
                    (cases + '6 :\nBegin\n Quit\nEnd\n' if n == 1 else '') +
                    'End\n')
            source = runtime.root / 'WidgetOwners'
            source.write_text('WindowTitle {WidgetOwners}\nWindowSize 740 550\n'
                'Font {fixed}\nBackColor {grey}\nForeColor {black}\n'
                'Init\nBegin\n Set $file={' + str(report) + '}\n'
                ' WriteToFile $file {ready}\nEnd\n' + ''.join(widgets))
            process, producer, consumer = start(runtime, args, display,
                'widgets', source)
            try:
                wait_for_predicate('all-widget startup',
                    lambda: report.exists() and named('WidgetOwners'),
                    time.monotonic() + 10, process=process)
                window = named('WidgetOwners')
                wait_for_predicate('fifteen widget windows',
                    lambda: len(children(window)) == 15,
                    time.monotonic() + 10, process=process)
                evidence['all-fifteen-startup'] = 'passed'
                for n, label in enumerate(('all-fifteen-font-change',
                        'named-to-colorset', 'colorset-to-named',
                        'font-option-update', 'font-recovery'), 1):
                    os.write(producer, packet(1 << 22, (0, 0, 0),
                        f'SendString 1 {n}'))
                    wait_for_predicate(label,
                        lambda n=n: f'event:{n}' in report.read_text(),
                        time.monotonic() + 10, process=process)
                    assert len(children(window)) == 15
                    evidence[label] = 'passed'
                if args.capture:
                    destination = args.artifacts_dir.resolve() / 'all-widgets'
                    destination.mkdir(exist_ok=True)
                    capture = runtime.start('capture-widgets',
                        [str(args.capture.resolve()), str(destination)],
                        'capture.log', environment=runtime.environment(
                            {'DISPLAY': display}))
                    assert capture.wait_until_exit(time.monotonic() + 35) == 0
                os.write(producer, packet(1 << 22, (0, 0, 0), 'SendString 1 6'))
                assert process.wait_until_exit(time.monotonic() + 10) == 0
                assert named('WidgetOwners') is None
                evidence['normal-exit'] = 'passed'
            finally:
                os.close(producer)
                os.close(consumer)

            for label, tail in (('before-prepare', ' Position 32768 0\n'),
                    ('prepare-refusal', ' Title {' + 'W' * 12000 + '}\n')):
                refused = runtime.root / label
                refused.write_text('WindowTitle {RefusedOwners}\n'
                    'WindowSize 200 100\nFont {fixed}\n' + widgets[0] +
                    'Widget 2\nProperty\n Type ItemDraw\n Position 0 0\n'
                    f' Size 32 32\n Icon {icon}\n' + tail +
                    'Main\nCase message of\nEnd\n')
                process, producer, consumer = start(runtime, args, display,
                    label, refused)
                try:
                    assert process.wait_until_exit(time.monotonic() + 10) == 1
                    assert 'invalid geometry' in process.log_path.read_text()
                    evidence[label] = 'passed'
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
    parser.add_argument('--capture', type=Path)
    run(parser.parse_args())
