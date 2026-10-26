#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or any later version.
"""Observe selected Pager and Animate owners in actual production objects."""
import argparse
import json
import os
from pathlib import Path
import shutil
import signal
import time

from live_x_harness import (HarnessRuntime, start_display_server,
    wait_for_predicate)
from module_metadata_owner_live import receive_config
from script_transport_live import packet


def run_case(runtime, args, display, module, scenario):
    label = module + '-' + scenario
    ready = runtime.root / (label + '.ready')
    report = runtime.root / (label + '.json')
    name = 'FvwmPager' if module == 'pager' else 'FvwmAnimate'
    binary = runtime.root / 'modules' / name
    incoming, producer = os.pipe()
    consumer, outgoing = os.pipe()
    environment = {'DISPLAY': display, 'MODULE_LIFECYCLE_READY': str(ready),
        'MODULE_LIFECYCLE_REPORT': str(report),
        'MODULE_LIFECYCLE_SCENARIO': scenario,
        # Shared graphics/font caches are outside the selected owner family.
        'ASAN_OPTIONS': 'detect_leaks=0:halt_on_error=1'}
    if args.baseline:
        environment['MODULE_LIFECYCLE_BASELINE'] = '1'
    icons = []
    for index in range(3):
        icon = runtime.root / f'{label}-{index}.xbm'
        icon.write_text('#define owner_width 8\n#define owner_height 8\n'
            'static unsigned char owner_bits[] = {\n'
            '0xff,0x81,0x81,0x81,0x81,0x81,0x81,0xff};\n')
        icons.append(icon)
    missing = runtime.root / 'expected-missing.xbm'
    arguments = [str(binary), str(outgoing), str(incoming), '/dev/null',
        '0', '0']
    if module == 'pager':
        arguments += ['0', '1']
    try:
        process = runtime.start(label, arguments, label + '.log',
            environment=runtime.environment(environment),
            pass_fds=(incoming, outgoing))
        os.close(incoming)
        incoming = -1
        os.close(outgoing)
        outgoing = -1
        receive_config(process, consumer)
        if module == 'pager':
            if scenario == 'aliases-eof':
                icons = [icons[0]] * 3
            config = ['*FvwmPagerFont fixed', '*FvwmPagerBalloonFont fixed',
                '*FvwmPagerBalloons All',
                f'*FvwmPagerPixmap * {icons[0]}',
                f'*FvwmPagerHilightPixmap * {icons[0]}',
                f'*FvwmPagerPixmap 42 {icons[1]}',
                f'*FvwmPagerHilightPixmap 43 {icons[2]}']
        elif scenario == 'missing-signal':
            config = [f'*FvwmAnimatePixmap {missing}']
        else:
            config = [f'*FvwmAnimatePixmap {icons[0]}',
                '*FvwmAnimateWidth 2']
        for line in config:
            os.write(producer, packet(1 << 18, (0, 0, 0), line))
        os.write(producer, packet(1 << 19))
        if scenario not in ('partial', 'exit'):
            wait_for_predicate(label + ' startup', ready.exists,
                time.monotonic() + 10, process=process)
        if module == 'animate' and scenario != 'missing-signal':
            for line in ('*FvwmAnimateWidth 3',
                    f'*FvwmAnimatePixmap {missing}', '*FvwmAnimateColor red',
                    f'*FvwmAnimatePixmap {icons[0]}',
                    '*FvwmAnimateColor None',
                    f'*FvwmAnimatePixmap {icons[0]}'):
                os.write(producer, packet(1 << 18, (0, 0, 0), line))
            # One ordinary synchronized packet precedes Stop/EOF retirement.
            os.write(producer, packet(1 << 22, (0, 0, 0), 'pause'))
            if scenario == 'tiled-stop':
                os.write(producer, packet(1 << 18, (0, 0, 0),
                    '*FvwmAnimateStop'))
        if scenario.endswith('-eof'):
            os.close(producer)
            producer = -1
        elif scenario.endswith('-signal'):
            process.process.send_signal(signal.SIGTERM)
        assert process.wait_until_exit(time.monotonic() + 10) == 0
        return json.loads(report.read_text())
    finally:
        for descriptor in (incoming, outgoing, producer, consumer):
            if descriptor >= 0:
                os.close(descriptor)


def run(args):
    args.artifacts_dir.mkdir(parents=True, exist_ok=True)
    evidence = {}
    with HarnessRuntime(prefix='fvwm-drawing-owners-') as runtime:
        try:
            display = args.display
            if display is None:
                _, display = start_display_server(runtime, 'Xvfb',
                    ['Xvfb', '-screen', '0', '800x600x24', '-noreset'],
                    'xserver.log', time.monotonic() + 10)
            binaries = runtime.root / 'modules'
            binaries.mkdir()
            for module, name in (('pager', 'FvwmPager'),
                    ('animate', 'FvwmAnimate')):
                (binaries / name).symlink_to(getattr(args, module).resolve())
            cases = [('pager', ('aliases-eof', 'distinct-signal', 'exit',
                'partial')), ('animate', ('tiled-stop', 'tiled-eof',
                'missing-signal'))]
            for module, scenarios in cases:
                for scenario in scenarios:
                    if args.case and module + '-' + scenario not in args.case:
                        continue
                    evidence[module + '-' + scenario] = run_case(runtime,
                        args, display, module, scenario)
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
    for module in ('pager', 'animate'):
        parser.add_argument('--' + module, type=Path, required=True)
    parser.add_argument('--artifacts-dir', type=Path, required=True)
    parser.add_argument('--display')
    parser.add_argument('--baseline', action='store_true')
    parser.add_argument('--case', action='append', choices=[
        'pager-aliases-eof', 'pager-distinct-signal', 'pager-exit',
        'pager-partial', 'animate-tiled-stop', 'animate-tiled-eof',
        'animate-missing-signal'])
    run(parser.parse_args())
