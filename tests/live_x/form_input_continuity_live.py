#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
"""Bounded actual-X editing/paint probes used by form_transport_live.py."""
import base64
import ctypes as C
import hashlib
import json
from pathlib import Path
import struct
import time

from colorset_live import WindowAttributes
from live_x_harness import wait_for_predicate
from selection_live import Event


PREFIX = 'HEADMARKABCD'
LONG = PREFIX + '-middle-' * 12 + 'zzzzTAILzzzz'
INITIAL = ['abc', '', 'LOCKED', PREFIX, LONG]
NAMES = ['first', 'empty', 'locked', 'reference', 'long']


def prefix_comparison(reference, candidate):
    """Compare the prefix rectangle, including every internal background pixel.

    The short control establishes its own rendered extent: a fontset's cell
    metrics can make the field much wider than this ASCII prefix. Field width
    therefore cannot locate the boundary before the long value's continuation.
    Both controls use the same font, size and Home cursor. Exclude the caret
    column, then use the last reference ink column as the rectangle's right
    edge. The comparison includes all rows and all pixels inside that edge;
    it is neither an ink-only subset nor a color histogram.
    """
    assert candidate[:2] == reference[:2]
    width, height, pixels = reference
    background = pixels[2 * width + 2]
    ink = [i for i, pixel in enumerate(pixels) if 6 <= i % width < width-2 and
        2 <= i // width < height-2 and pixel != background]
    assert ink, 'reference prefix has no visible text outside the caret'
    right = max(i % width for i in ink) + 1
    points = [py * width + px for py in range(2, height-2) for px in range(6, right)]
    differences = [i for i in points if candidate[2][i] != pixels[i]]
    return dict(rectangle=[6, 2, right, height-2], rectangle_edges='right/bottom exclusive',
        samples=len(points), reference_ink_pixels=len(ink),
        reference_background_pixels=sum(pixels[i] == background for i in points),
        mismatches=len(differences),
        first_mismatches=[[i % width, i // width] for i in differences[:64]])


def continuity_config(font):
    lines = ['Position 0', 'InputFont ' + font,
        'ItemFore black', 'ItemBack white']
    for name, value in zip(NAMES, INITIAL):
        lines.extend(['Line left', 'Input ' + name + ' 12 "' + value + '"'])
    lines.extend(['InputReadonly locked', 'Line left',
        'Button restart "Restart" F11', 'Command Nop input-restarted',
        'Button continue "Observe" F12',
        'Command Nop input-values ' + ' '.join('x$(' + name + ':Base64)' for name in NAMES)])
    return ['*FormTransport' + line for line in lines]


def check_continuity(x, d, root, shown, process, config, commands,
        receive, args, results):
    P, W = C.c_void_p, C.c_ulong
    x.XKeysymToKeycode.argtypes = [P, W]
    x.XKeysymToKeycode.restype = C.c_uint
    x.XSendEvent.argtypes = [P, W, C.c_int, C.c_long, C.POINTER(Event)]
    x.XGetImage.argtypes = [P, W, C.c_int, C.c_int, C.c_uint, C.c_uint, W, C.c_int]
    x.XGetImage.restype = P
    x.XGetPixel.argtypes = [P, C.c_int, C.c_int]
    x.XGetPixel.restype = W
    x.XDestroyImage.argtypes = [P]
    x.XClearArea.argtypes = [P, W, C.c_int, C.c_int, C.c_uint, C.c_uint, C.c_int]
    x.XStoreBuffer.argtypes = [P, C.c_char_p, C.c_int, C.c_int]
    rr, parent, children, count = W(), W(), C.POINTER(W)(), C.c_uint()
    assert x.XQueryTree(d, shown, C.byref(rr), C.byref(parent),
        C.byref(children), C.byref(count))
    try:
        # XQueryTree returns siblings in stacking/creation order here. No
        # controls have been raised or replaced; geometry is not an identity.
        controls = list(children[:count.value])
    finally:
        if children:
            x.XFree(children)
    schema = [line[len('*FormTransport'):].split()[:2] for line in config
        if line.startswith('*FormTransport') and line[len('*FormTransport'):].split()[0]
        in ('Input', 'InputHex', 'Button', 'List')]
    assert len(schema) == len(controls) == 7, (schema, controls)
    names = {parts[1]: target for parts, target in zip(schema, controls) if parts[0] == 'Input'}
    evidence = dict(status='running', module_sha256=hashlib.sha256(args.module.read_bytes()).hexdigest(),
        case=args.editable_case, input_font=args.input_font,
        controls=dict(zip(NAMES, [names[n] for n in NAMES])), samples=[], values=[], checks=[],
        source_sha256={name: hashlib.sha256((Path(__file__).parent / name).read_bytes()).hexdigest()
            for name in ('form_transport_live.py', 'form_input_continuity_live.py')},
        limits=dict(keys=512, pixel_width=800, pixel_height=200,
            font_backend='requested font only; actual backend belongs to build/font evidence',
            native_internal_counts='observed through End/insertion/deletion byte consequences'))

    def save():
        (args.artifacts_dir / 'editable-evidence.json').write_text(json.dumps(evidence, indent=2) + '\n')

    key_count = 0

    def key(symbol, state=0):
        nonlocal key_count
        key_count += 1
        assert key_count <= 512
        event = Event()
        event.key.type, event.key.display = 2, d
        event.key.window, event.key.root = shown, root
        event.key.same_screen, event.key.state = 1, state
        event.key.keycode = x.XKeysymToKeycode(d, symbol)
        assert event.key.keycode
        assert x.XSendEvent(d, shown, 0, 1, C.byref(event))
        x.XSync(d, 0)

    def observe(expected, label):
        prefix = 'Nop input-values '
        before = sum(c.startswith(prefix) for c in commands)
        key(0xffc9)
        receive(lambda: sum(c.startswith(prefix) for c in commands) > before)
        raw = [c for c in commands if c.startswith(prefix)][-1]
        encoded = raw[len(prefix):].split()
        actual = [base64.urlsafe_b64decode(v[1:] + '===').decode('utf-8') for v in encoded]
        evidence['values'].append(dict(label=label, command=raw,
            expected=expected, actual=actual,
            byte_counts=[len(v.encode()) for v in actual], character_counts=[len(v) for v in actual]))
        save()
        assert actual == expected, (label, actual, expected)

    def restart(destination=0):
        before = commands.count('Nop input-restarted')
        key(0xffc8)
        receive(lambda: commands.count('Nop input-restarted') > before)
        for _ in range(destination):
            key(0xff09)
        observe(INITIAL, 'restart')

    def raster(target, label):
        previous, since = None, time.monotonic()

        def sample():
            nonlocal previous, since
            a = WindowAttributes()
            assert x.XGetWindowAttributes(d, target, C.byref(a)) and a.map_state == 2
            assert 0 < a.width <= 800 and 0 < a.height <= 200
            image = x.XGetImage(d, target, 0, 0, a.width, a.height, W(-1).value, 2)
            assert image
            try:
                actual = (a.width, a.height, [x.XGetPixel(image, px, py)
                    for py in range(a.height) for px in range(a.width)])
            finally:
                x.XDestroyImage(image)
            if actual != previous:
                previous, since = actual, time.monotonic()
            return actual if time.monotonic() - since >= .08 else None

        actual = wait_for_predicate('stable positional editable pixels', sample,
            time.monotonic() + 2, process=process)
        width, height, pixels = actual
        rgb = bytes(c for p in pixels for c in ((p >> 16) & 255, (p >> 8) & 255, p & 255))
        (args.artifacts_dir / (label + '.ppm')).write_bytes(
            ('P6\n%d %d\n255\n' % (width, height)).encode() + rgb)
        evidence['samples'].append(dict(label=label, window=target, width=width, height=height,
            sha256=hashlib.sha256(struct.pack('>%dI' % len(pixels), *pixels)).hexdigest()))
        save()
        return actual

    def passed(label):
        evidence['checks'].append(label)
        save()

    save()
    try:
        if args.editable_case in ('home', 'all'):
            restart(3)
            key(0xff50)
            observe(INITIAL, 'reference-home')
            reference = raster(names['reference'], 'reference-home')
            key(0xff09)
            key(0xff57)
            observe(INITIAL, 'long-end')
            tail = raster(names['long'], 'long-end')
            key(0xff50)
            observe(INITIAL, 'long-home')
            head = raster(names['long'], 'long-home')
            width, height = head[:2]
            evidence['prefix_oracle'] = prefix_comparison(reference, head)
            save()
            assert evidence['prefix_oracle']['reference_background_pixels'] > 0
            assert evidence['prefix_oracle']['mismatches'] == 0, \
                'Home did not render the distinctive prefix at reference positions'
            assert head != tail
            passed('home-renders-prefix-not-tail-or-caret-only')
            borders = lambda r: [p for i, p in enumerate(r[2]) if
                i % width < 2 or i % width >= width-2 or i // width < 2 or i // width >= height-2]
            neighbor = raster(names['locked'], 'neighbor-before')
            for _ in range(3):
                key(0xff57)
                key(0xff50)
            observe(INITIAL, 'home-restored')
            restored = raster(names['long'], 'home-restored')
            assert restored == head, 'full Home interior/right-edge restoration differs'
            assert borders(tail) == borders(head)
            x.XClearArea(d, names['long'], 0, 0, 0, 0, 1)
            x.XSync(d, 0)
            observe(INITIAL, 'after-real-expose')
            assert raster(names['long'], 'after-real-expose') == head, 'keyboard/expose paints differ'
            assert raster(names['locked'], 'neighbor-after') == neighbor
            passed('full-interior-right-edge-expose-borders-neighbor')
            key(ord('x'))
            edited = INITIAL[:-1] + ['x' + LONG]
            observe(edited, 'home-insert')
            key(0xff50)
            key(0xff53)
            key(0xffff)
            edited[-1] = 'x' + LONG[1:]
            observe(edited, 'right-delete')
            key(0xff57)
            key(0xff08)
            edited[-1] = edited[-1][:-1]
            observe(edited, 'end-backspace')
            key(0xff50)
            key(0xff53)
            key(ord('k'), 4)
            edited[-1] = 'x'
            observe(edited, 'control-k')
            key(ord('u'), 4)
            edited[-1] = ''
            observe(edited, 'control-u')
            # Valid UTF-8 is inserted through Form's real cut-buffer input.
            # No Tab/newline is used (paste traversal is a separate defect).
            data = 'é終Q'.encode()
            x.XStoreBuffer(d, data, len(data), 0)
            event = Event()
            event.button.type, event.button.display = 4, d
            event.button.window, event.button.root = names['long'], root
            event.button.same_screen, event.button.button = 1, 2
            event.button.x, event.button.y = 5, 5
            assert x.XSendEvent(d, names['long'], 0, 1 << 2, C.byref(event))
            x.XSync(d, 0)
            edited[-1] = 'é終Q'
            observe(edited, 'valid-multibyte-input')
            key(0xff50)
            key(0xff53)
            key(0xffff)
            edited[-1] = 'éQ'
            observe(edited, 'multibyte-right-delete')
            key(0xff57)
            key(0xff08)
            edited[-1] = 'é'
            observe(edited, 'multibyte-end-backspace')
            passed('exact-edits-and-utf8-boundaries')
        if args.editable_case in ('wrap', 'all'):
            for symbol, state, label in [(0xff54, 0, 'down'), (ord('n'), 4, 'control-n'),
                    (0xff09, 0, 'legacy-tab')]:
                restart(4)
                key(0xff57)
                key(symbol, state)
                key(0xff08)
                observe(INITIAL, label + '-home-backspace-noop')
                # Bytes alone miss a decremented n with unchanged bytes. End
                # plus insertion exposes that stale count in native output.
                key(0xff57)
                key(ord('x'))
                expected = ['abcx'] + INITIAL[1:]
                observe(expected, label + '-short-end-insert')
                key(symbol, state)
                key(0xff08)
                key(ord('z'))
                expected[1] = 'z'
                observe(expected, label + '-empty-first-edit')
                key(0xff52)  # backward to first
                key(0xff50)
                key(ord('q'))
                expected[0] = 'qabcx'
                observe(expected, label + '-backward-short-edit')
                key(0xff52)  # backward wrap to long
                key(0xff57)
                key(0xff08)
                expected[-1] = LONG[:-1]
                observe(expected, label + '-backward-long-edit')
                restart()
                key(ord('u'), 4)
                key(0xff52)  # backward wrap from now-empty first to long
                key(0xff57)
                key(symbol, state)
                key(0xff08)
                key(ord('z'))
                observe(['z'] + INITIAL[1:], label + '-empty-first-wrap-edit')
                key(symbol, state)  # empty second field
                key(symbol, state)  # readonly third field
                key(0xff57)
                key(0xff08)
                key(0xffff)
                key(ord('x'))
                observe(['z'] + INITIAL[1:], label + '-readonly-destination-refuses-edits')
                key(0xff52)  # backward from readonly to empty editable
                key(ord('r'))
                observe(['z', 'r'] + INITIAL[2:], label + '-readonly-backward-to-editable')
                passed(label + '-forward-backward-wrap-short-empty-readonly-ring')
        evidence['status'] = 'passed'
        results['editable-continuity'] = evidence['checks']
    except BaseException as failure:
        evidence['status'], evidence['failure'] = 'failed', str(failure)
        raise
    finally:
        evidence['keys_sent'] = key_count
        save()
