#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
"""Exercise production Status collision, restart, pressure and Quit cleanup."""
import argparse
import json
from pathlib import Path
import shutil
import stat
import sys
import time

from geometry_transition_live import Client
from live_x_harness import (HarnessRuntime, StatusCollector,
    start_display_server, wait_for_predicate)
from mfl_stream_live import Connection


def checks(args):
    connection = Connection(args.endpoint)
    collector = client = None
    fifo = args.root / 'fvwm3.pipe'
    evidence = {'status': 'running', 'cases': []}
    try:
        connection.record()
        connection.send(b'set echo\nStatus On\n')
        connection.echo('collision-refused')
        assert fifo.is_file() and fifo.read_bytes() == b'keep-user-data'
        evidence['cases'].append('existing-file-preserved')
        fifo.unlink()
        connection.send(b'Status On\n')
        connection.echo('status-created')
        assert stat.S_ISFIFO(fifo.stat().st_mode)
        assert stat.S_IMODE(fifo.stat().st_mode) & 0o077 == 0
        collector = StatusCollector.open(fifo, time.monotonic() + 5)
        collector.wait_for('initial JSON', lambda row: row['version'] == 4,
                           time.monotonic() + 5)
        connection.send(b'Status Off\n')
        connection.echo('status-off')
        assert not fifo.exists()
        collector.close()
        collector = None
        connection.send(b'Status On\n')
        connection.echo('status-reopened')
        collector = StatusCollector.open(fifo, time.monotonic() + 5)
        evidence['cases'].append('off-on-retry')
        client = Client(args.display)
        client.create()
        wait_for_predicate('managed status client', client.frame,
                           time.monotonic() + 5)
        connection.send(f'WindowId 0x{client.window:x} Focus\n'.encode())
        title = b'status-pressure-' + b'x' * 1024
        client.x.XStoreName(client.display, client.window, title)
        client.x.XSync(client.display, 0)
        connection.echo('title-published')
        for desk in range(96):
            name = f'pressure-{desk}-' + 'x' * 700
            connection.send(f'DesktopName {desk} {name}\n'.encode())
            connection.echo(f'desktop-{desk}-published')
        # Do not read the FIFO while sending far more than its capacity.
        # Echo observes core progress despite a stalled status reader.
        connection.send(b'Status On\n' * 64)
        connection.echo('healthy-under-status-pressure')
        collector.wait_for('idle writable continuation',
            lambda row: len(json.dumps(row)) > 50000,
            time.monotonic() + 5)
        evidence['cases'].append('complete-JSON-after-backpressure')
        evidence['parsed_records'] = len(collector.records)
        client.x.XStoreName(client.display, client.window, b'status-recovered')
        client.x.XSync(client.display, 0)
        connection.echo('title-recovered')
        collector.wait_for('latest status after pressure',
            lambda row: any(s.get('current_client') == 'status-recovered'
                            for s in row['screens'].values()),
            time.monotonic() + 5)
        evidence['cases'].append('latest-state-recovery')
        client.close()
        client = None
        connection.send(b'Quit\n')
        wait_for_predicate('Quit removes owned FIFO', lambda: not fifo.exists(),
                           time.monotonic() + 5)
        evidence['cases'].append('quit-removes-owned-FIFO')
        evidence['status'] = 'passed'
    finally:
        if client:
            client.close()
        if collector:
            evidence['parsed_records'] = len(collector.records)
            evidence['max_record_bytes'] = max(
                (len(json.dumps(row)) for row in collector.records), default=0)
            collector.close()
        connection.close()
        (args.artifacts_dir / 'results.json').write_text(
            json.dumps(evidence, indent=2) + '\n')


def run(args):
    args.artifacts_dir.mkdir(parents=True, exist_ok=True)
    with HarnessRuntime(prefix='fvwm-status-stream-') as runtime:
        receipt = {'status': 'running', 'full_log': args.full_log}
        try:
            display = args.display
            if display is None:
                _, display = start_display_server(runtime, 'Xvfb',
                    ['Xvfb', '-screen', '0', '800x600x24', '-noreset'],
                    'xserver.log', time.monotonic() + 10)
            (runtime.root / 'fvwm3.pipe').write_bytes(b'keep-user-data')
            config = runtime.root / 'config'
            config.write_text(f'ModulePath {args.mfl.parent}\n'
                'Style * TileCascadePlacement\nEdgeScroll 0 0\n'
                'AddToFunc StartFunction I Module FvwmMFL\n')
            log = Path('/dev/full') if args.full_log else runtime.root / 'debug.log'
            core = runtime.start('core', [str(args.fvwm), '-d', display,
                '-f', str(config), '-v', '-o', str(log)], 'core.log',
                runtime.environment({'FVWMMFL_SOCKET_PATH': str(runtime.root),
                    'FVWM_USERDIR': str(runtime.root)}))
            endpoint = runtime.root / f"fvwm_mfl_{display.replace(':', '_')}.sock"
            wait_for_predicate('status endpoint', endpoint.exists,
                               time.monotonic() + 15, process=core)
            worker = runtime.start('status-checks', [sys.executable, __file__,
                '--worker', '--endpoint', str(endpoint), '--root', str(runtime.root),
                '--display', display, '--fvwm', str(args.fvwm),
                '--mfl', str(args.mfl), '--artifacts-dir', str(args.artifacts_dir)],
                'checks.log', runtime.environment())
            result = worker.wait_until_exit(time.monotonic() + 45)
            assert result == 0
            assert core.wait_until_exit(time.monotonic() + 5) == 0
            receipt['status'] = 'passed'
        finally:
            for process in reversed(runtime.processes):
                process.close(time.monotonic() + 5)
            receipt['children_after_cleanup'] = {
                process.name: process.poll() for process in runtime.processes}
            for path in runtime.root.iterdir():
                if path.is_file():
                    shutil.copy2(path, args.artifacts_dir / path.name)
            (args.artifacts_dir / 'supervisor.json').write_text(
                json.dumps(receipt, indent=2) + '\n')
    print('Status ownership/continuation and core shutdown passed')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('fvwm', 'mfl', 'artifacts-dir'):
        parser.add_argument('--' + name, type=Path, required=True)
    parser.add_argument('--display')
    parser.add_argument('--full-log', action='store_true')
    parser.add_argument('--worker', action='store_true')
    parser.add_argument('--endpoint', type=Path)
    parser.add_argument('--root', type=Path)
    args = parser.parse_args()
    args.fvwm, args.mfl = args.fvwm.resolve(), args.mfl.resolve()
    args.artifacts_dir = args.artifacts_dir.resolve()
    if args.worker:
        checks(args)
    else:
        run(args)
