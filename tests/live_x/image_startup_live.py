#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
"""Exercise normal startup and restart with pending image work."""
import argparse
import json
from pathlib import Path
import shutil
import time

from live_x_harness import HarnessRuntime, start_display_server, wait_for_predicate
from mfl_stream_live import Connection
from geometry_transition_live import Client
from image_colorsets_live import records


def run(args):
    args.artifacts_dir.mkdir(parents=True, exist_ok=True)
    evidence = {}
    with HarnessRuntime(prefix='fvwm-image-startup-') as runtime:
        core = connection = client = None
        try:
            _, display = start_display_server(runtime, 'Xvfb',
                ['Xvfb', '-screen', '0', f'800x600x{args.depth}', '-noreset'],
                'xserver.log', time.monotonic() + 10)
            stall = runtime.root / 'stall.svg'
            blue = runtime.root / 'blue.svg'
            stall.write_text('red')
            blue.write_text('blue')
            config = runtime.root / 'config'
            config.write_text(f'ModulePath {args.mfl.parent}:{args.probe.parent}\n'
                'Style * TileCascadePlacement\nColorset 0 bg black\n'
                f'Colorset 1 bg black, Pixmap {stall}\n'
                'AddToFunc StartFunction I Module FvwmMFL\n')
            core = runtime.start('core', [str(args.fvwm), '-d', display,
                '-f', str(config), '-v', '-o', str(runtime.root / 'debug.log')],
                'core.log', runtime.environment({
                    'FVWMMFL_SOCKET_PATH': str(runtime.root),
                    'FVWM_IMAGE_SVG_WORKER': str(args.worker)}))
            endpoint = runtime.root / f"fvwm_mfl_{display.replace(':', '_')}.sock"

            def connect():
                wait_for_predicate('startup module endpoint', endpoint.exists,
                    time.monotonic() + 10, process=core)
                result = Connection(endpoint)
                result.record()
                result.send(b'set echo\n')
                result.echo('startup-ready')
                return result

            connection = connect()
            client = Client(display)
            sequence = 0

            def snapshot():
                nonlocal sequence
                sequence += 1
                report = runtime.root / f'colorsets-{sequence}.txt'
                connection.send(f'Module {args.probe.name} colorsets {report}\n'.encode())
                connection.echo(f'snapshot-{sequence}')
                wait_for_predicate('startup colorset packet',
                    lambda: any(row[0] == 1 for row in records(report)),
                    time.monotonic() + 5, process=core)
                return [row for row in records(report) if row[0] == 1][-1]

            assert snapshot()[8] == 0
            client.create()
            wait_for_predicate('startup client management',
                lambda: client.frame() is not None,
                time.monotonic() + 2, process=core)
            connection.send(b'Schedule 30 Echo startup-scheduled\n')
            connection.expect_echo('startup-scheduled')
            evidence['pending-startup-image-client-and-timer-service'] = 'passed'
            connection.send(f'Colorset 1 Pixmap {blue}\n'.encode())
            report = runtime.root / 'replacement.txt'
            connection.send(f'Module {args.probe.name} colorsets {report}\n'.encode())
            wait_for_predicate('startup image replacement',
                lambda: any(row[0] == 1 and row[8] != 0 for row in records(report)),
                time.monotonic() + 8, process=core)
            evidence['startup-supersession-and-publication'] = 'passed'
            # Restart while another image is pending. Reuse the actual executable
            # and config; the new process must own a fresh loader and retain clients.
            connection.send(f'Colorset 1 Pixmap {stall}\n'.encode())
            connection.echo('restart-pending')
            connection.send(b'Restart\n')
            connection.expect_closed()
            connection.close()
            connection = connect()
            assert client.frame() is not None
            assert snapshot()[8] == 0
            connection.send(f'Colorset 1 Pixmap {blue}\n'.encode())
            report = runtime.root / 'restart-replacement.txt'
            connection.send(f'Module {args.probe.name} colorsets {report}\n'.encode())
            wait_for_predicate('restarted image publication',
                lambda: any(row[0] == 1 and row[8] != 0 for row in records(report)),
                time.monotonic() + 8, process=core)
            evidence['restart-pending-image-client-retention-and-fresh-loader'] = 'passed'
            connection.send(b'Quit\n')
            assert core.wait_until_exit(time.monotonic() + 5) == 0
            log = (runtime.root / 'debug.log').read_text()
            for failure in ('X error', 'Unbound deferred image',
                    'AddressSanitizer', 'runtime error:'):
                assert failure not in log, log
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
    print('Normal image startup and restart remain responsive')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(allow_abbrev=False)
    for name in ('fvwm', 'mfl', 'probe', 'worker', 'artifacts-dir'):
        parser.add_argument('--' + name, type=Path, required=True)
    parser.add_argument('--depth', type=int, choices=(8, 24), default=24)
    options = parser.parse_args()
    for key, value in vars(options).items():
        if isinstance(value, Path):
            setattr(options, key, value.resolve())
    run(options)
