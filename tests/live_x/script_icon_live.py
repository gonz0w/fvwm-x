#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or any later version.
"""Check actual Script image replacement, refusal, retry and expose pixels."""
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
from script_transport_live import packet, WORD


def bitmap(path, filled):
    path.write_text('#define icon_width 8\n#define icon_height 8\n'
        'static unsigned char icon_bits[] = {\n' +
        ','.join(['0xff' if filled else '0x00'] * 8) + '\n};\n')


def run(args):
    args.artifacts_dir.mkdir(parents=True, exist_ok=True)
    evidence = {}
    with HarnessRuntime(prefix='fvwm-script-icon-') as runtime:
        client = None
        try:
            display = args.display
            if display is None:
                _, display = start_display_server(runtime, 'Xvfb',
                    ['Xvfb', '-screen', '0', '800x600x24', '-noreset'],
                    'xserver.log', time.monotonic() + 10)
            client = Client(display)
            x, d = client.x, client.display
            p, w, i = C.c_void_p, C.c_ulong, C.c_int
            signatures = {
                'XFetchName': ([p, w, C.POINTER(p)], i),
                'XGetImage': ([p, w, i, i, C.c_uint, C.c_uint, w, i], p),
                'XGetPixel': ([p, i, i], w),
                'XDestroyImage': ([p], i),
                'XClearArea': ([p, w, i, i, C.c_uint, C.c_uint, i], i),
                'XDefaultScreen': ([p], i),
                'XBlackPixel': ([p, i], w),
                'XWhitePixel': ([p, i], w),
            }
            for name, (arguments, result) in signatures.items():
                getattr(x, name).argtypes = arguments
                getattr(x, name).restype = result

            def children(window):
                root, parent, items, count = (w(), w(),
                    C.POINTER(w)(), C.c_uint())
                assert x.XQueryTree(d, window, C.byref(root), C.byref(parent),
                    C.byref(items), C.byref(count))
                result = [items[n] for n in range(count.value)]
                if items:
                    x.XFree(items)
                return result

            def named(title):
                for window in children(client.root):
                    value = p()
                    if x.XFetchName(d, window, C.byref(value)) and value:
                        text = C.string_at(value).decode('utf-8', 'replace')
                        x.XFree(value)
                        if text == title:
                            return window
                return None

            def pixel(window, px, py):
                image = x.XGetImage(d, window, px, py, 1, 1,
                    C.c_ulong(-1).value, 2)
                assert image
                try:
                    return x.XGetPixel(image, 0, 0)
                finally:
                    x.XDestroyImage(image)

            black, white, retry = (runtime.root / name for name in
                ('black.xbm', 'white.xbm', 'retry.xbm'))
            bitmap(black, True)
            bitmap(white, False)
            retry.write_text('deliberately invalid image\n')
            report = runtime.root / 'icon.report'
            actions = {1: white, 2: runtime.root / 'missing.xbm',
                3: retry, 4: retry, 5: black}
            cases = ''.join(f'{n} :\nBegin\n ChangeIcon 1 {path}\n'
                f' WriteToFile $file {{event:{n}}}\nEnd\n'
                for n, path in actions.items())
            script = runtime.root / 'IconProbe'
            script.write_text('WindowTitle {IconProbe}\nWindowSize 110 100\n'
                'Font {fixed}\nBackColor {green}\nForeColor {black}\nInit\nBegin\n'
                f' Set $file={{{report}}}\n WriteToFile $file {{ready}}\nEnd\n'
                'Widget 1\nProperty\n Type ItemDraw\n Position 10 10\n'
                f' Size 64 48\n Icon {black}\nMain\nCase message of\n'
                + cases + '6 :\nBegin\n Quit\nEnd\nEnd\n'
                'Widget 2\nProperty\n Type ItemDraw\n Position 10 65\n'
                ' Size 90 20\n Title {Icon owner}\nMain\nCase message of\nEnd\n')

            def start(label, source):
                incoming, producer = os.pipe()
                consumer, outgoing = os.pipe()
                process = runtime.start(label, [str(args.module.resolve()),
                    str(outgoing), str(incoming), '/dev/null', '0', '0',
                    str(source)], label + '.log', pass_fds=(incoming, outgoing),
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
                    os.write(producer, packet(1 << 19))
                    return process, producer, consumer
                except BaseException:
                    os.close(producer)
                    os.close(consumer)
                    raise

            if args.case in ('all', 'replacement'):
                process, producer, consumer = start('icons', script)
                try:
                    wait_for_predicate('icon startup',
                        lambda: report.exists() and named('IconProbe'),
                        time.monotonic() + 10, process=process)
                    window = named('IconProbe')
                    wait_for_predicate('icon widgets', lambda: len(children(window)) == 2,
                        time.monotonic() + 10, process=process)
                    widget = children(window)[0]
                    screen = x.XDefaultScreen(d)
                    dark, light = x.XBlackPixel(d, screen), x.XWhitePixel(d, screen)
                    background = pixel(widget, 2, 2)
                    assert background not in (dark, light)
                    label = children(window)[1]
                    wait_for_predicate('text widget rendering',
                        lambda: any(pixel(label, px, py) == dark
                            for py in range(3, 17) for px in range(3, 87)),
                        time.monotonic() + 10, process=process)
                    evidence['existing-text-rendering'] = 'passed'

                    def check(label, expected):
                        try:
                            wait_for_predicate(label,
                                lambda: pixel(widget, 32, 24) == expected,
                                time.monotonic() + 10, process=process)
                        except BaseException:
                            evidence[label + '-observation'] = {
                                'expected': expected,
                                'actual': pixel(widget, 32, 24),
                            }
                            if args.capture:
                                destination = args.artifacts_dir.resolve() / 'failure'
                                destination.mkdir()
                                capture = runtime.start('capture-failure',
                                    [str(args.capture.resolve()), str(destination)],
                                    'capture-failure.log',
                                    environment=runtime.environment({'DISPLAY': display}))
                                capture.wait_until_exit(time.monotonic() + 35)
                            raise
                        evidence[label] = 'passed'
                        if args.capture and label in ('initial-bitmap-conversion',
                                'missing-image-clears-pixels', 'same-path-retry'):
                            destination = args.artifacts_dir.resolve() / label
                            destination.mkdir(exist_ok=True)
                            capture = runtime.start('capture-' + label,
                                [str(args.capture.resolve()), str(destination)],
                                'capture-' + label + '.log',
                                environment=runtime.environment({'DISPLAY': display}))
                            assert capture.wait_until_exit(time.monotonic() + 35) == 0
                            assert list(destination.glob('*.png'))

                    def event(number):
                        os.write(producer, packet(1 << 22, (0, 0, 0),
                            f'SendString 1 {number}'))
                        wait_for_predicate(f'icon event {number}',
                            lambda: f'event:{number}' in report.read_text(),
                            time.monotonic() + 10, process=process)

                    check('initial-bitmap-conversion', dark)
                    event(1)
                    check('replace-with-white-bitmap', light)
                    event(2)
                    check('missing-image-clears-pixels', background)
                    event(3)
                    check('decoder-refusal-clears-pixels', background)
                    bitmap(retry, True)
                    event(4)
                    check('same-path-retry', dark)
                    x.XClearArea(d, widget, 0, 0, 0, 0, True)
                    x.XSync(d, False)
                    check('expose-redraw', dark)
                    event(5)
                    check('repeated-replacement', dark)
                    os.write(producer, packet(1 << 22, (0, 0, 0), 'SendString 1 6'))
                    assert process.wait_until_exit(time.monotonic() + 10) == 0
                    evidence['normal-shutdown'] = 'passed'
                finally:
                    os.close(producer)
                    os.close(consumer)

            if args.case in ('all', 'partial-startup'):
                # Both widgets load icons before the second widget refuses layout.
                # Its destructor has not been registered as initialized at that point.
                # Explicit sizes are valid; measuring the long fixed-font title
                # exceeds the X11 width limit during preparation after icon load.
                refused = runtime.root / 'RefusedIcon'
                refused.write_text('WindowTitle {RefusedIcon}\nWindowSize 100 70\n'
                    'Font {fixed}\n' + ''.join(
                        f'Widget {n}\nProperty\n Type ItemDraw\n Position 0 0\n'
                        f' Size 32 32\n Icon {black}\n' +
                        (' Title {' + 'W' * 12000 + '}\n'
                            if n == 2 else '') +
                        'Main\nCase message of\nEnd\n' for n in (1, 2)))
                process, producer, consumer = start('partial-startup', refused)
                try:
                    assert process.wait_until_exit(time.monotonic() + 10) == 1
                    assert 'invalid geometry' in process.log_path.read_text()
                    evidence['partial-startup-refusal'] = 'passed'
                finally:
                    os.close(producer)
                    os.close(consumer)
            for log in runtime.root.glob('*.log'):
                text = log.read_text()
                assert not any(s in text for s in
                    ('AddressSanitizer', 'runtime error:', 'Assertion',
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
    parser.add_argument('--capture', type=Path)
    parser.add_argument('--case', choices=('all', 'replacement',
        'partial-startup'), default='all')
    run(parser.parse_args())
