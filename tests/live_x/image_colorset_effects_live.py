#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
"""Keep published colorsets usable during real core effect fault injection."""
import argparse
import ctypes as C
import json
import os
from pathlib import Path
import shutil
import time

from live_x_harness import HarnessRuntime, start_display_server, wait_for_predicate
from mfl_stream_live import Connection
from geometry_transition_live import Client
from image_owners_live import png
from image_colorsets_live import records, XColor


def run(args):
    args.artifacts_dir.mkdir(parents=True, exist_ok=True)
    evidence = {}
    with HarnessRuntime(prefix='fvwm-colorset-effects-') as runtime:
        core = connection = client = None
        try:
            _, display = start_display_server(runtime, 'Xvfb',
                ['Xvfb', '-screen', '0', f'800x600x{args.depth}', '-noreset'],
                'xserver.log', time.monotonic() + 10)
            control = runtime.root / 'control.txt'
            events = runtime.root / 'workers.txt'
            control.write_text('pass\n')
            image = runtime.root / 'source.png'
            image.write_bytes(png())
            config = runtime.root / 'config'
            config.write_text(f'ModulePath {args.mfl.parent}:{args.probe.parent}\n'
                'Style * TileCascadePlacement\nColorset 0 bg black\n'
                'AddToFunc StartFunction I Module FvwmMFL\n')
            core = runtime.start('core', [str(args.fvwm), '-d', display,
                '-f', str(config), '-v', '-o', str(runtime.root / 'debug.log')],
                'core.log', runtime.environment({
                    'FVWMMFL_SOCKET_PATH': str(runtime.root),
                    'FVWM_IMAGE_EFFECT_WORKER': str(args.worker),
                    'FVWM_TEST_REAL_EFFECT_WORKER': str(args.real_worker),
                    'FVWM_TEST_EFFECT_CONTROL': str(control),
                    'FVWM_TEST_EFFECT_EVENTS': str(events),
                    'FVWM_TEST_COLORSET_SOFTWARE': '1'}))
            endpoint = runtime.root / f"fvwm_mfl_{display.replace(':', '_')}.sock"
            wait_for_predicate('effect fixture endpoint', endpoint.exists,
                time.monotonic() + 10, process=core)
            connection = Connection(endpoint)
            connection.record()
            connection.send(b'set echo\n')
            client = Client(display)
            sequence = 0

            def command(text):
                nonlocal sequence
                sequence += 1
                connection.send((text + '\n').encode())
                connection.echo(f'effect-command-{sequence}')

            report = runtime.root / 'published.txt'
            command(f'Module {args.probe.name} colorsets {report}')
            wait_for_predicate('initial published colorset',
                lambda: bool(records(report)), time.monotonic() + 5, process=core)

            def snapshot(identifier):
                path = runtime.root / f'snapshot-{sequence}.txt'
                command(f'Module {args.probe.name} colorsets {path}')
                wait_for_predicate('module colorset snapshot',
                    lambda: any(row[0] == identifier for row in records(path)),
                    time.monotonic() + 5, process=core)
                return next(row for row in records(path) if row[0] == identifier)

            def changed(identifier, previous):
                def ready():
                    rows = [row for row in records(report) if row[0] == identifier]
                    return bool(rows) and rows[-1][8] != previous

                wait_for_predicate('acknowledged effect publication', ready,
                    time.monotonic() + 8, process=core)
                return [row for row in records(report) if row[0] == identifier][-1]

            def worker_rows():
                return events.read_text().splitlines() if events.exists() else []

            def started(mode, count):
                wait_for_predicate('worker started in ' + mode,
                    lambda: len(worker_rows()) > count and
                        worker_rows()[-1].split()[1] == mode,
                    time.monotonic() + 5, process=core)
                return int(worker_rows()[-1].split()[0])

            def alive(pid):
                try:
                    os.kill(pid, 0)
                    return True
                except ProcessLookupError:
                    return False

            def geometry(pixmap):
                assert client.geometry(pixmap)[2:] == (2, 2)

            for identifier in (1, 2):
                command(f'Colorset {identifier} bg blue, Pixmap {image}, '
                    'Tint green 50, ImageAlpha 50')
                changed(identifier, 0)
            original = snapshot(1)
            shared = snapshot(2)
            control.write_text('stall\n')
            count = len(worker_rows())
            command(f'Colorset 3 bg blue, Pixmap {image}, Tint green 50')
            started('stall', count)
            assert snapshot(3)[8] == 0
            command('Colorset 3 Plain')
            assert snapshot(3)[8] == 0
            evidence['first-load-absent-and-plain-cancellation'] = 'passed'
            image.unlink()  # Subsequent effects must use retained source pixels.
            control.write_text('stall\n')
            count = len(worker_rows())
            command('Colorset 1 Tint red 25')
            pid = started('stall', count)
            assert snapshot(1) == original
            geometry(original[8])
            start = time.monotonic()
            client.create()
            wait_for_predicate('client managed while effect stalled',
                lambda: client.frame() is not None,
                time.monotonic() + 2, process=core)
            command('Schedule 30 Echo effect-scheduled')
            connection.expect_echo('effect-scheduled')
            elapsed = time.monotonic() - start
            assert elapsed < 2, elapsed
            evidence['module-x-and-scheduled-work-during-stall-seconds'] = elapsed
            count = len(worker_rows())
            command('Colorset 1 fg white')
            pid = started('stall', count)
            assert snapshot(1) == original
            control.write_text('pass\n')
            command('Colorset 1 Tint red 75')
            replacement = changed(1, original[8])
            assert not alive(pid)
            assert replacement[17] == 75
            geometry(original[8])
            geometry(replacement[8])
            assert snapshot(2) == shared
            evidence['supersede-fg-only-rebuild-source-retention-and-shared-owner'] = 'passed'

            for mode in ('partial', 'dimensions', 'complete-stall'):
                previous = snapshot(1)
                log = runtime.root / 'debug.log'
                failures = log.read_text().count('Image effect refused:')
                control.write_text(mode + '\n')
                count = len(worker_rows())
                start = time.monotonic()
                command('Colorset 1 Tint blue 37')
                pid = started(mode, count)
                assert snapshot(1) == previous
                wait_for_predicate('effect refused after ' + mode,
                    lambda: log.read_text().count('Image effect refused:') > failures,
                    time.monotonic() + args.timeout + 3, process=core)
                assert not alive(pid)
                assert snapshot(1) == previous
                geometry(previous[8])
                evidence[mode + '-preserves-published-state-seconds'] = time.monotonic() - start
                control.write_text('pass\n')
                command('Colorset 1 Tint red 75')
                changed(1, previous[8])

            command('CleanupColorsets')
            geometry(snapshot(1)[8])
            geometry(shared[8])
            control.write_text('stall\n')
            count = len(worker_rows())
            command('Colorset 1 Tint blue 25')
            pid = started('stall', count)
            connection.send(b'Quit\n')
            assert core.wait_until_exit(time.monotonic() + 5) == 0
            assert not alive(pid)
            evidence['quit-cancels-and-reaps-effect-before-x-close'] = 'passed'
            log = (runtime.root / 'debug.log').read_text()
            assert 'X error' not in log, log
            assert 'AddressSanitizer' not in log and 'runtime error:' not in log, log
        except BaseException as error:
            evidence['error'] = str(error)
            evidence['wm_exit_status'] = core.poll() if core else None
            raise
        finally:
            if client:
                client.close()
            if connection:
                connection.close()
            for path in runtime.root.iterdir():
                if path.suffix in ('.txt', '.log'):
                    shutil.copy2(path, args.artifacts_dir / path.name)
            (args.artifacts_dir / 'evidence.json').write_text(
                json.dumps(evidence, indent=2) + '\n')
    print('Colorset effects preserve publication and responsiveness')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(allow_abbrev=False)
    for name in ('fvwm', 'mfl', 'probe', 'worker', 'real-worker', 'artifacts-dir'):
        parser.add_argument('--' + name, type=Path, required=True)
    parser.add_argument('--timeout', type=int, default=5)
    parser.add_argument('--depth', type=int, choices=(8, 24), default=24)
    options = parser.parse_args()
    for key, value in vars(options).items():
        if isinstance(value, Path):
            setattr(options, key, value.resolve())
    run(options)
