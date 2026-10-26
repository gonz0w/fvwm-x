#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or any later version.
"""Observe selected resource owners in the actual Ident and Buttons modules."""
import argparse
import ctypes as C
import json
import os
from pathlib import Path
import shutil
import signal
import subprocess
import time

from geometry_transition_live import Client
from live_x_harness import (HarnessRuntime, start_display_server,
    wait_for_predicate)
from module_metadata_owner_live import receive_config
from script_transport_live import packet


class Button(C.Structure):
    _fields_ = [('type', C.c_int), ('serial', C.c_ulong),
        ('send_event', C.c_int), ('display', C.c_void_p),
        ('window', C.c_ulong), ('root', C.c_ulong), ('subwindow', C.c_ulong),
        ('time', C.c_ulong), ('x', C.c_int), ('y', C.c_int),
        ('x_root', C.c_int), ('y_root', C.c_int), ('state', C.c_uint),
        ('button', C.c_uint), ('same_screen', C.c_int)]


class Event(C.Union):
    _fields_ = [('button', Button), ('pad', C.c_long * 24)]


def run_case(runtime, args, display, client, module, scenario):
    label = module + '-' + scenario
    ready = runtime.root / (label + '.ready')
    report = runtime.root / (label + '.json')
    target = client.x.XCreateSimpleWindow(client.display, client.root,
        10, 10, 320, 240, 0, 0, 0)
    client.x.XSync(client.display, False)
    incoming, producer = os.pipe()
    consumer, outgoing = os.pipe()
    environment = {'DISPLAY': display, 'MODULE_OWNER_READY': str(ready),
        'MODULE_OWNER_REPORT': str(report), 'MODULE_OWNER_SCENARIO': scenario,
        'ASAN_OPTIONS': 'detect_leaks=0:halt_on_error=1'}
    if args.baseline:
        environment['MODULE_OWNER_BASELINE'] = '1'
    if module == 'buttons' and scenario.endswith('-actual'):
        environment['MODULE_OWNER_ACTUAL_RELEASE'] = '1'
    if module == 'buttons' and scenario == 'missing-actual':
        environment['MODULE_OWNER_EXPECT_REFUSAL'] = '1'
    try:
        name = 'FvwmIdent' if module == 'ident' else 'FvwmButtons'
        binary = runtime.root / 'modules' / name
        process = runtime.start(label, [str(binary), str(outgoing),
            str(incoming), '/dev/null', f'{target:x}', '0'], label + '.log',
            environment=runtime.environment(environment),
            pass_fds=(incoming, outgoing))
        os.close(incoming)
        incoming = -1
        os.close(outgoing)
        outgoing = -1
        receive_config(process, consumer)
        if module == 'ident':
            config = ['*FvwmIdentFont fixed', '*FvwmIdentFont fixed']
            if scenario not in ('defaults', 'back-only'):
                config += ['*FvwmIdentFore white', '*FvwmIdentFore black']
            if scenario not in ('defaults', 'fore-only'):
                config += ['*FvwmIdentBack black', '*FvwmIdentBack white']
            if scenario in ('defaults', 'fore-only', 'back-only'):
                config.append('*FvwmIdentColorset -1')
            elif scenario == 'colorset':
                config.append('*FvwmIdentColorset 0')
        else:
            icons = []
            for index in range(7):
                icon = runtime.root / f'{label}-{index}.xbm'
                icon.write_text('#define owner_width 8\n'
                    '#define owner_height 8\n'
                    'static unsigned char owner_bits[] = {\n'
                    '0xff,0x81,0x81,0x81,0x81,0x81,0x81,0xff};\n')
                icons.append(icon)
            if scenario.startswith('aliases'):
                icons = [icons[0]] * 7
            elif scenario == 'missing-actual':
                icons[3] = runtime.root / 'expected-missing.xbm'
            config = ['*FvwmButtonsFont fixed', '*FvwmButtonsGeometry 240x100',
                '*FvwmButtonsRows 1', '*FvwmButtonsColumns 4',
                f'*FvwmButtonsPixmap {icons[0]}',
                f'*FvwmButtons(Title First, Icon {icons[1]}, '
                f'ActiveIcon {icons[2]}, PressIcon {icons[3]}, '
                f'Back(Icon) {icons[4]})',
                '*FvwmButtons(Container(Rows 1, Columns 1, Back white))',
                f'*FvwmButtonsPixmap {icons[5]}',
                f'*FvwmButtons(Title Nested, Icon {icons[6]})',
                '*FvwmButtons(End)',
                '*FvwmButtons(Title Borrowed, Colorset 0)']
        for line in config:
            os.write(producer, packet(1 << 18, (0, 0, 0), line))
        os.write(producer, packet(1 << 19))
        if module == 'ident' and scenario == 'early-eof':
            os.close(producer)
            producer = -1
        elif module == 'ident':
            for popup in range(1, 4):
                configuration = subprocess.check_output(
                    [str(args.fixture.resolve()), str(target)], timeout=5)
                # Two updates per popup replace the retained monitor owner.
                os.write(producer, configuration + configuration)
                for kind, text in ((1 << 10, 'ordinary window'),
                        (1 << 11, 'ordinary icon'),
                        (1 << 12, 'ordinary class'),
                        (1 << 13, 'ordinary resource')):
                    os.write(producer, packet(kind, (target, target, 0), text))
                os.write(producer, packet(1 << 14))
                def mapped():
                    if not ready.exists():
                        return False
                    data = ready.read_text().split()
                    return len(data) == 2 and int(data[0]) == popup
                wait_for_predicate(label + ' mapped', mapped,
                    time.monotonic() + 10, process=process)
                window = int(ready.read_text().split()[1])
                if popup < 3:
                    client.x.XSendEvent.argtypes = [C.c_void_p, C.c_ulong,
                        C.c_int, C.c_long, C.POINTER(Event)]
                    for kind in (4, 5):
                        event = Event()
                        event.button = Button(type=kind, display=client.display,
                            window=window, root=client.root, button=2,
                            same_screen=1)
                        assert client.x.XSendEvent(client.display, window, 0,
                            0, C.byref(event))
                    client.x.XSync(client.display, False)
                    # The wrapper's target selection writes a readiness marker
                    # only after the previous popup is released.
                    wait_for_predicate(label + ' retarget',
                        lambda: Path(str(ready) + '.retarget').exists(),
                        time.monotonic() + 10, process=process)
                    Path(str(ready) + '.retarget').unlink()
        else:
            wait_for_predicate(label + ' mapped', ready.exists,
                time.monotonic() + 10, process=process)
        if scenario != 'early-eof':
            process.process.send_signal(signal.SIGTERM)
        assert process.wait_until_exit(time.monotonic() + 10) == 0
        return json.loads(report.read_text())
    finally:
        for descriptor in (incoming, outgoing, producer, consumer):
            if descriptor >= 0:
                os.close(descriptor)
        client.x.XDestroyWindow(client.display, target)
        client.x.XSync(client.display, False)


def run(args):
    args.artifacts_dir.mkdir(parents=True, exist_ok=True)
    evidence = {}
    with HarnessRuntime(prefix='fvwm-resource-owners-') as runtime:
        client = None
        try:
            display = args.display
            if display is None:
                _, display = start_display_server(runtime, 'Xvfb',
                    ['Xvfb', '-screen', '0', '800x600x24', '-noreset'],
                    'xserver.log', time.monotonic() + 10)
            client = Client(display)
            binaries = runtime.root / 'modules'
            binaries.mkdir()
            for module, name in (('ident', 'FvwmIdent'),
                    ('buttons', 'FvwmButtons')):
                (binaries / name).symlink_to(getattr(args, module).resolve())
            cases = [('ident', ('named', 'colorset')),
                ('buttons', ('distinct', 'aliases'))]
            if not args.baseline:
                cases += [('ident', ('defaults', 'fore-only', 'back-only',
                    'early-eof')), ('buttons', ('distinct-actual',
                    'aliases-actual', 'missing-actual'))]
            for module, scenarios in cases:
                for scenario in scenarios:
                    if args.case and module + '-' + scenario not in args.case:
                        continue
                    evidence[module + '-' + scenario] = run_case(runtime,
                        args, display, client, module, scenario)
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
    for module in ('ident', 'buttons'):
        parser.add_argument('--' + module, type=Path, required=True)
    parser.add_argument('--fixture', type=Path, required=True)
    parser.add_argument('--artifacts-dir', type=Path, required=True)
    parser.add_argument('--display')
    parser.add_argument('--baseline', action='store_true')
    parser.add_argument('--case', action='append', choices=[
        'ident-named', 'ident-colorset', 'buttons-distinct', 'buttons-aliases',
        'ident-defaults', 'ident-fore-only', 'ident-back-only',
        'ident-early-eof',
        'buttons-distinct-actual', 'buttons-aliases-actual',
        'buttons-missing-actual'])
    run(parser.parse_args())
