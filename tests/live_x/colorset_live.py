#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.

"""Verify sparse theme IDs, startup packets and refusal without state loss."""
import argparse
import ctypes as C
import json
from pathlib import Path
import shutil
import sys
import time

from geometry_transition_live import Client
from live_x_harness import HarnessRuntime, start_display_server, wait_for_predicate
from mfl_stream_live import Connection


class WindowAttributes(C.Structure):
    _fields_ = [(n, t) for n, t in (
        ('x', C.c_int), ('y', C.c_int), ('width', C.c_int), ('height', C.c_int),
        ('border_width', C.c_int), ('depth', C.c_int), ('visual', C.c_void_p),
        ('root', C.c_ulong), ('window_class', C.c_int), ('bit_gravity', C.c_int),
        ('win_gravity', C.c_int), ('backing_store', C.c_int),
        ('backing_planes', C.c_ulong), ('backing_pixel', C.c_ulong),
        ('save_under', C.c_int), ('colormap', C.c_ulong),
        ('map_installed', C.c_int), ('map_state', C.c_int),
        ('all_event_masks', C.c_long), ('your_event_mask', C.c_long),
        ('do_not_propagate_mask', C.c_long), ('override_redirect', C.c_int),
        ('screen', C.c_void_p))]


def run(options):
    out = options.artifacts_dir
    out.mkdir(parents=True, exist_ok=True)
    for name in ('results.json', 'core.log', 'xserver.log', 'widget-report'):
        (out / name).unlink(missing_ok=True)
    evidence = {}
    with HarnessRuntime(prefix='fvwm-colorset-') as runtime:
        connection = client = None
        try:
            display = options.display
            if display is None:
                _, display = start_display_server(runtime, 'Xvfb',
                    ['Xvfb', '-screen', '0', '800x600x24', '-noreset'],
                    'xserver.log', time.monotonic() + 10)
            config = runtime.root / 'config'
            config.write_text(f'ModulePath {options.mfl.parent}:{options.script.parent}:{options.probe.parent}\n'
                'Style * TileCascadePlacement\nColorset 0 bg red\n'
                'Colorset 100000 fg white, bg red\n'
                'Colorset 2147483647 fg black, bg green\n'
                'Style * Colorset 100000, HilightColorset 100000\n'
                'AddToFunc StartFunction I Module FvwmMFL\n')
            core = runtime.start('core', [str(options.fvwm), '-d', display,
                '-f', str(config), '-v', '-o', str(runtime.root / 'core-debug.log')],
                'core.log', runtime.environment({'FVWMMFL_SOCKET_PATH': str(runtime.root)}))
            endpoint = runtime.root / f"fvwm_mfl_{display.replace(':', '_')}.sock"
            wait_for_predicate('colorset endpoint', endpoint.exists,
                time.monotonic() + 10, process=core)
            connection = Connection(endpoint)
            connection.record()
            connection.send(b'set echo\n')
            connection.echo('colorset-ready')

            def probe(name):
                file = runtime.root / (name + '.json')
                connection.send(f'Module {options.probe.name} {file}\n'.encode())
                wait_for_predicate('startup colorset dump',
                    lambda: file.exists() and file.read_text().endswith('}\n'),
                    time.monotonic() + 15, process=core)
                data = json.loads(file.read_text())
                shutil.copy2(file, out / file.name)
                assert data['ids'] == sorted(set(data['ids']))
                assert data['count'] == len(data['ids'])
                return data

            initial = probe('initial-packets')
            assert initial['ids'] == [0, 100000, 2147483647], initial
            connection.send(b'Echo sparse-red:$[bg.cs100000.hash]\n')
            connection.expect_echo('sparse-red:#ff0000')
            evidence['large-id-and-intmax-without-gap-broadcast'] = 'passed'

            report = runtime.root / 'widget-report'
            script = runtime.root / 'ColorsetProbe'
            script.write_text('WindowTitle {ColorsetProbe}\nWindowSize 120 60\n'
                'Colorset 100000\nInit\nBegin\n Set $file={' + str(report) + '}\n'
                ' WriteToFile $file {ready:} (GetBack 1)\n'
                ' WriteToFile $file {undefined:} (GetBack 2)\nEnd\n'
                'Widget 1\nProperty\n Type Rectangle\n Position 5 5\n'
                ' Size 80 30\n Colorset 100000\nMain\nCase message of\n'
                '1 :\nBegin\n WriteToFile $file {snapshot:} (GetBack 1)\nEnd\n'
                '2 :\nBegin\n ChangeColorset 1 2147483647\n'
                ' WriteToFile $file {high:} (GetBack 1)\nEnd\n'
                '3 :\nBegin\n ChangeColorset 1 2147483646\n'
                ' ChangeColorset 0 2147483646\n'
                ' WriteToFile $file {refused:} (GetBack 1)\nEnd\n'
                '4 :\nBegin\n ChangeColorset 1 100000\n'
                ' WriteToFile $file {existing:} (GetBack 1)\nEnd\nEnd\n'
                'Widget 2\nProperty\n Type Rectangle\n Position 90 5\n'
                ' Size 20 20\n Colorset 1\nMain\nCase message of\nEnd\n')
            shutil.copy2(script, out / script.name)
            connection.send(f'Module FvwmScript {script}\n'.encode())
            wait_for_predicate('sparse script ready', lambda: report.exists() and
                'ready:ff0000\n' in report.read_text(), time.monotonic() + 15, process=core)

            wait_for_predicate('undefined set clones zero', lambda:
                'undefined:ff0000\n' in report.read_text(),
                time.monotonic() + 15, process=core)
            evidence['undefined-module-id-clones-zero-despite-higher-core-id'] = 'passed'

            def event(number, expected):
                count = report.read_text().count(expected)
                connection.send(f'SendToModule FvwmScript SendString 1 {number}\n'.encode())
                wait_for_predicate('colorset widget barrier',
                    lambda: report.read_text().count(expected) > count,
                    time.monotonic() + 15, process=core)

            # The script has loaded a complete startup palette, and receives
            # subsequent updates while holding stable record pointers.
            connection.send(b'Colorset 100000 bg blue\n')
            connection.echo('colorset-update-sent')
            event(1, 'snapshot:0000ff\n')
            event(2, 'high:00ff00\n')
            evidence['script-high-id-and-live-colorset-update'] = 'passed'
            client = Client(display)
            client.create()
            wait_for_predicate('sparse themed frame', lambda: client.frame() is not None,
                time.monotonic() + 10, process=core)
            x, d = client.x, client.display
            x.XGetWindowAttributes.argtypes = [C.c_void_p, C.c_ulong,
                C.POINTER(WindowAttributes)]

            def viewable():
                frame = client.frame()
                attrs = WindowAttributes()
                return frame is not None and x.XGetWindowAttributes(d, frame,
                    C.byref(attrs)) and attrs.map_state == 2

            wait_for_predicate('viewable themed frame', viewable,
                time.monotonic() + 10, process=core)
            connection.echo('frame-mapped-barrier')
            x.XGetImage.argtypes = [C.c_void_p, C.c_ulong, C.c_int, C.c_int,
                C.c_uint, C.c_uint, C.c_ulong, C.c_int]
            x.XGetImage.restype = C.c_void_p
            x.XGetPixel.argtypes = [C.c_void_p, C.c_int, C.c_int]
            x.XGetPixel.restype = C.c_ulong
            x.XDestroyImage.argtypes = [C.c_void_p]

            def frame_pixels():
                frame = client.frame()
                width, height = client.geometry(frame)[2:4]
                image = x.XGetImage(d, frame, 0, 0, width, height,
                    C.c_ulong(-1).value, 2)
                assert image
                try:
                    return tuple(x.XGetPixel(image, a, b)
                        for b in range(height) for a in range(width))
                finally:
                    x.XDestroyImage(image)

            themed = frame_pixels()
            assert len(set(themed)) > 1, 'frame must contain decorations and client'


            # Fill only the finite configured number of actual sets. Batching
            # commands does not define any enormous dense gap or request an
            # oversized image; each set uses the ordinary default resources.
            missing = options.limit - initial['count']
            for start in range(1, missing + 1, 64):
                end = min(missing + 1, start + 64)
                connection.send(''.join(f'Colorset {i} fg white, bg gray\n' for i in range(start, end)).encode())
                connection.echo(f'palette-{start}')
            full = probe('full-packets')
            assert full['count'] == options.limit, full
            assert full['ids'] == [0, *range(1, missing + 1), 100000, 2147483647]
            connection.send(b'Colorset 2147483646 bg yellow\n'
                b'Colorset 2147483648 bg yellow\n'
                b'Colorset 99999999999999999999999 bg yellow\n')
            connection.echo('core-after-refusal')
            assert probe('refused-packets') == full
            evidence['core-count-limit-refusal-and-overflow-without-new-packets'] = 'passed'
            connection.send(b'Style * Colorset 2147483646, HilightColorset 2147483646\n')
            connection.echo('style-after-refusal')
            assert frame_pixels() == themed
            evidence['style-count-refusal-preserves-visible-theme'] = 'passed'


            # The already-running script must also reach its own count limit;
            # core broadcasts have filled its table without allocating gaps.
            event(3, 'refused:00ff00\n')
            event(4, 'existing:0000ff\n')
            connection.send(b'Colorset 100000 bg red\n')
            connection.echo('existing-update-at-capacity')
            event(1, 'snapshot:ff0000\n')
            wait_for_predicate('existing colorset repaints frame',
                lambda: frame_pixels() != themed, time.monotonic() + 10, process=core)
            assert probe('after-existing-update') == full
            evidence['module-refusal-retains-assignment-and-existing-update-at-capacity'] = 'passed'
            connection.echo('wm-after-palette-checks')
            log = (runtime.root / 'core-debug.log').read_text() + (runtime.root / 'core.log').read_text()
            assert 'Cannot allocate colorset 2147483646' in log, log
            evidence['refusal-diagnostic-and-wm-availability'] = 'passed'
        finally:
            if connection:
                connection.close()
            if client:
                client.close()
            for path in runtime.root.glob('*.log'):
                shutil.copy2(path, out / path.name)
            if (runtime.root / 'widget-report').exists():
                shutil.copy2(runtime.root / 'widget-report', out / 'widget-report')
            (out / 'results.json').write_text(json.dumps(evidence, indent=2) + '\n')
    print('Sparse colorset compatibility, broadcast and capacity checks passed')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(allow_abbrev=False)
    for name in ('fvwm', 'mfl', 'script', 'probe', 'artifacts-dir'):
        parser.add_argument('--' + name, type=Path, required=True)
    parser.add_argument('--limit', type=int, required=True)
    parser.add_argument('--display')
    args = parser.parse_args()
    for name in ('fvwm', 'mfl', 'script', 'probe', 'artifacts_dir'):
        setattr(args, name, getattr(args, name).resolve())
    assert 1 <= args.limit <= 65536
    if args.limit < 4:
        print('High-ID palette fixture needs at least four sets; policy/API coverage remains enabled')
        sys.exit(77)
    run(args)
