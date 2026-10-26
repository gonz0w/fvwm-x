#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or any later version.
"""Observe temporary owners and real X properties of three actual modules."""
import argparse
import ctypes as C
import json
import os
from pathlib import Path
import selectors
import shutil
import signal
import struct
import subprocess
import time

from geometry_transition_live import Client, SizeHints
from live_x_harness import (HarnessRuntime, start_display_server,
    wait_for_predicate)
from script_transport_live import packet, WORD


def receive_config(process, consumer):
    buffer = bytearray()
    with selectors.DefaultSelector() as ready:
        ready.register(consumer, selectors.EVENT_READ)
        deadline = time.monotonic() + 10
        while True:
            remaining = deadline - time.monotonic()
            assert remaining > 0 and ready.select(remaining)
            data = os.read(consumer, 65536)
            assert data, process.log_path.read_text()
            buffer.extend(data)
            while len(buffer) >= 2 * WORD:
                _, length = struct.unpack('@LL', buffer[:2 * WORD])
                assert length <= 1000
                size = 3 * WORD + length
                if len(buffer) < size:
                    break
                command = bytes(buffer[2 * WORD:2 * WORD + length]).decode()
                del buffer[:size]
                if 'Send_ConfigInfo' in command:
                    return


class Properties:
    def __init__(self, client):
        self.x, self.display = client.x, client.display
        self.x.XInternAtom.argtypes = [C.c_void_p, C.c_char_p, C.c_int]
        self.x.XInternAtom.restype = C.c_ulong
        self.x.XGetWindowProperty.argtypes = [C.c_void_p, C.c_ulong,
            C.c_ulong, C.c_long, C.c_long, C.c_int, C.c_ulong,
            C.POINTER(C.c_ulong), C.POINTER(C.c_int),
            C.POINTER(C.c_ulong), C.POINTER(C.c_ulong),
            C.POINTER(C.c_void_p)]
        self.x.XSetWMSizeHints.argtypes = [C.c_void_p, C.c_ulong,
            C.POINTER(SizeHints), C.c_ulong]

    def atom(self, name):
        return self.x.XInternAtom(self.display, name.encode(), False)

    def get(self, window, name):
        kind, fmt, count, after, data = (C.c_ulong(), C.c_int(),
            C.c_ulong(), C.c_ulong(), C.c_void_p())
        assert self.x.XGetWindowProperty(self.display, window,
            self.atom(name), 0, 1024, False, 0, C.byref(kind), C.byref(fmt),
            C.byref(count), C.byref(after), C.byref(data)) == 0
        try:
            assert after.value == 0
            if kind.value == 0:
                return None
            if fmt.value == 8:
                return C.string_at(data, count.value).decode()
            assert fmt.value == 32
            values = C.cast(data, C.POINTER(C.c_ulong))
            return [values[i] for i in range(count.value)]
        finally:
            if data:
                self.x.XFree(data)


def check_properties(properties, window, module, scenario):
    observed = {name: properties.get(window, name) for name in (
        'WM_NAME', 'WM_ICON_NAME', 'WM_CLASS', 'WM_NORMAL_HINTS', 'WM_HINTS')}
    if module == 'script':
        expected = None if scenario == 'title-refusal' else 'MetadataOwners'
        assert observed['WM_NAME'] == observed['WM_ICON_NAME'] == expected
        assert observed['WM_CLASS'].split('\0') == ['metadata-script',
            'FvwmScript', '']
        hints = observed['WM_NORMAL_HINTS']
        positioned = scenario != 'default-position'
        assert hints[0] == 312 + (4 if positioned else 0)
        assert hints[5:9] == [260, 150, 260, 150]
        assert hints[15:17] == [0, 0]
        assert observed['WM_HINTS'][:3] == [3, 1, 1]
    elif module == 'iconman':
        if scenario == 'success':
            assert observed['WM_NAME'] == 'MetadataIconMan'
            assert observed['WM_ICON_NAME'] == 'MetadataIcon'
            assert observed['WM_CLASS'].split('\0') == ['FvwmIconMan',
                'FvwmIconMan', '']
            assert observed['WM_NORMAL_HINTS'] is not None
            assert observed['WM_HINTS'][:3] == [2, 0, 1]
        else:
            # create_manager publishes normal hints before text conversion.
            assert observed['WM_NORMAL_HINTS'] is not None
            assert all(observed[name] is None for name in (
                'WM_NAME', 'WM_ICON_NAME', 'WM_CLASS', 'WM_HINTS'))
    else:
        assert observed['WM_NAME'] == 'FvwmIdent'
    return observed


def run_case(runtime, args, display, client, properties, module, scenario):
    label = module + '-' + scenario
    ready = runtime.root / (label + '.ready')
    report = runtime.root / (label + '.json')
    environment = {'DISPLAY': display, 'METADATA_SCENARIO': scenario,
        'METADATA_READY': str(ready), 'METADATA_REPORT': str(report)}
    if args.baseline:
        environment['METADATA_BASELINE'] = '1'
    if scenario in ('title-refusal', 'first-refusal', 'second-refusal'):
        environment['METADATA_REFUSE'] = (
            '2' if scenario == 'second-refusal' else '1')
    target = client.x.XCreateSimpleWindow(client.display, client.root,
        10, 10, 320, 240, 0, 0, 0)
    if module == 'ident' and scenario != 'absent':
        hints = SizeHints(flags=(1 << 7) if scenario == 'aspect' else 0)
        if scenario == 'aspect':
            hints.min_aspect.x, hints.min_aspect.y = 4, 3
            hints.max_aspect.x, hints.max_aspect.y = 16, 9
        properties.x.XSetWMSizeHints(client.display, target, C.byref(hints),
            properties.atom('WM_ZOOM_HINTS'))
    client.x.XSync(client.display, False)
    incoming, producer = os.pipe()
    consumer, outgoing = os.pipe()
    try:
        binary = runtime.root / 'modules' / ('Fvwm' + {
            'script': 'Script', 'ident': 'Ident', 'iconman': 'IconMan'}[module])
        command = [str(binary), str(outgoing),
            str(incoming), '/dev/null', f'{target:x}' if module == 'ident'
            else '0', '0']
        if module == 'script':
            source = runtime.root / 'metadata-script'
            position = ('' if scenario == 'default-position' else
                'WindowPosition 31 47\n')
            source.write_text('WindowTitle {MetadataOwners}\n'
                'WindowSize 260 150\n' + position + 'Font fixed\n'
                'Widget 1\nProperty\nType ItemDraw\n'
                'Position 1 1\nSize 20 20\nMain\nCase message of\nEnd\n')
            command.append(str(source))
        process = runtime.start(label, command, label + '.log',
            environment=runtime.environment(environment),
            pass_fds=(incoming, outgoing))
        os.close(incoming)
        incoming = -1
        os.close(outgoing)
        outgoing = -1
        receive_config(process, consumer)
        config = []
        if module == 'ident':
            config = ['*FvwmIdentColorset -1', '*FvwmIdentFont fixed']
        elif module == 'iconman':
            config = ['*FvwmIconMan Title MetadataIconMan',
                '*FvwmIconMan IconName MetadataIcon',
                '*FvwmIconMan Resolution global',
                '*FvwmIconMan ManagerGeometry 1x1+0+0',
                '*FvwmIconMan Font fixed']
        for line in config:
            os.write(producer, packet(1 << 18, (0, 0, 0), line))
        os.write(producer, packet(1 << 19))
        if module != 'script':
            os.write(producer, subprocess.check_output(
                [str(args.fixture.resolve()), str(target)], timeout=5))
            for kind, text in ((1 << 10, 'ordinary window'),
                    (1 << 11, 'ordinary icon'),
                    (1 << 12, 'ordinary class'),
                    (1 << 13, 'ordinary resource')):
                os.write(producer, packet(kind, (target, target, 0), text))
            os.write(producer, packet(1 << 14))
        wait_for_predicate(label + ' mapped', ready.exists,
            time.monotonic() + 10, process=process)
        window = int(ready.read_text())
        observed = check_properties(properties, window, module, scenario)
        process.process.send_signal(signal.SIGTERM)
        assert process.wait_until_exit(time.monotonic() + 10) == 0
        ownership = json.loads(report.read_text())
        return {'properties': observed, 'ownership': ownership}
    finally:
        for descriptor in (incoming, outgoing, producer, consumer):
            if descriptor >= 0:
                os.close(descriptor)
        client.x.XDestroyWindow(client.display, target)
        client.x.XSync(client.display, False)


def run(args):
    args.artifacts_dir.mkdir(parents=True, exist_ok=True)
    evidence = {}
    with HarnessRuntime(prefix='fvwm-metadata-') as runtime:
        client = None
        try:
            display = args.display
            if display is None:
                _, display = start_display_server(runtime, 'Xvfb',
                    ['Xvfb', '-screen', '0', '800x600x24', '-noreset'],
                    'xserver.log', time.monotonic() + 10)
            client = Client(display)
            properties = Properties(client)
            binaries = runtime.root / 'modules'
            binaries.mkdir()
            for module, name in (('script', 'FvwmScript'),
                    ('ident', 'FvwmIdent'), ('iconman', 'FvwmIconMan')):
                (binaries / name).symlink_to(getattr(args, module).resolve())
            cases = {'script': ['success', 'default-position'],
                'ident': ['absent', 'aspect', 'no-aspect'],
                'iconman': ['success', 'first-refusal', 'second-refusal']}
            if not args.baseline:
                cases['script'].append('title-refusal')
            for module, scenarios in cases.items():
                for scenario in scenarios:
                    evidence[module + '-' + scenario] = run_case(runtime,
                        args, display, client, properties, module, scenario)
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
    for module in ('script', 'ident', 'iconman'):
        parser.add_argument('--' + module, type=Path, required=True)
    parser.add_argument('--fixture', type=Path, required=True)
    parser.add_argument('--artifacts-dir', type=Path, required=True)
    parser.add_argument('--display')
    parser.add_argument('--baseline', action='store_true')
    run(parser.parse_args())
