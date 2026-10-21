#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
"""Check fork-time LoggingFD metadata through delayed module requests."""
import argparse
import json
import os
from pathlib import Path
import select
import shutil
import signal
import struct
import sys
import time

from live_x_harness import HarnessRuntime, start_display_server, wait_for_predicate
from mfl_stream_live import Connection

WORD = struct.calcsize('@L')


def module_probe(directory, log_path):
    root, log = Path(directory), Path(log_path)
    output, incoming = int(sys.argv[1]), int(sys.argv[2])
    inherited = -1
    holders = []
    target = log.stat() if log.exists() else None
    for fd in range(3, 128):
        try:
            record = os.fstat(fd)
        except OSError:
            continue
        if target and (record.st_dev, record.st_ino) == (target.st_dev, target.st_ino):
            inherited = fd
    # Occupy vacant low slots with our own storage. A descriptor opened later
    # by the parent must never be advertised as a transfer to this child.
    for _ in range(128):
        fd = os.open(root / 'private-resource', os.O_WRONLY | os.O_CREAT | os.O_APPEND, 0o600)
        holders.append(fd)
        if fd >= 128:
            break
    (root / 'ready.json').write_text(json.dumps({'inherited': inherited, 'pid': os.getpid()}))
    deadline = time.monotonic() + 12
    while not (root / 'request').exists():
        if time.monotonic() >= deadline:
            raise TimeoutError('parent never released delayed request')
        time.sleep(0.01)
    rows = []
    for repeat in range(2):
        command = b'Send_ConfigInfo'
        os.write(output, struct.pack('@LL', 0, len(command)) + command + struct.pack('@L', 1))
        def exact(size):
            data = bytearray()
            while len(data) < size:
                remaining = deadline - time.monotonic()
                if remaining <= 0 or not select.select([incoming], [], [], remaining)[0]:
                    raise TimeoutError('configuration packet')
                block = os.read(incoming, size - len(data))
                if not block:
                    raise EOFError('configuration packet')
                data.extend(block)
            return bytes(data)
        advertised = None
        for _ in range(256):
            start, kind, count, _ = struct.unpack('@LLLL', exact(4 * WORD))
            assert start == 0xffffffff and 4 <= count <= 256
            body = exact((count - 4) * WORD)
            if kind == 1 << 18 and len(body) > 3 * WORD:
                text = body[3 * WORD:].split(b'\0', 1)[0].decode()
                if text.startswith('LoggingFD '):
                    advertised = int(text.split()[1])
            if kind == 1 << 19:
                break
        assert advertised is not None
        # Write only the known inherited sink. A bad old-core reply may
        # name a pipe or another resource; recording the mismatch is enough.
        if advertised >= 0 and advertised == inherited:
            os.write(advertised, b'fork-time-log-marker\n')
        rows.append({'repeat': repeat, 'inherited': inherited, 'advertised': advertised})
    (root / 'result.json').write_text(json.dumps(rows, indent=2) + '\n')
    for fd in holders:
        os.close(fd)
    assert all(row['inherited'] == row['advertised'] for row in rows), rows


def run(args):
    args.artifacts_dir.mkdir(parents=True, exist_ok=True)
    with HarnessRuntime(prefix='fvwm-module-logging-') as runtime:
        connection = None
        evidence = {'status': 'running', 'cases': {}}
        try:
            display = args.display
            if display is None:
                _, display = start_display_server(runtime, 'Xvfb',
                    ['Xvfb', '-screen', '0', '800x600x24', '-noreset'],
                    'xserver.log', time.monotonic() + 10)
            probe = runtime.root / 'LoggingProbe'
            shutil.copyfile(__file__, probe)
            probe.chmod(0o700)
            # The probe imports the same test helpers after the core's exec.
            environment = runtime.environment({'FVWMMFL_SOCKET_PATH': str(runtime.root),
                'FVWM_USERDIR': str(runtime.root), 'PYTHONPATH': str(Path(__file__).parent)})
            config = runtime.root / 'config'
            # This request has no module recipient; it must be a safe no-op.
            config.write_text('Send_ConfigInfo\n' +
                f'ModulePath {runtime.root}:{args.mfl.parent}\n'
                'Style * TileCascadePlacement\nAddToFunc StartFunction I Module FvwmMFL\n')
            log = runtime.root / 'debug.log'
            core = runtime.start('core', [str(args.fvwm), '-d', display,
                '-f', str(config), '-o', str(log)], 'core.log', environment)
            endpoint = runtime.root / f"fvwm_mfl_{display.replace(':', '_')}.sock"
            wait_for_predicate('logging endpoint', endpoint.exists,
                time.monotonic() + 10, process=core)
            connection = Connection(endpoint)
            connection.record()
            connection.send(b'set echo\n')
            connection.echo('logging-ready')
            for label, initially_enabled in [('absent-at-fork', False), ('enabled-at-fork', True)]:
                directory = runtime.root / label
                directory.mkdir()
                connection.send(f'Module LoggingProbe probe {directory} {log}\n'.encode())
                wait_for_predicate('delayed module fork', (directory / 'ready.json').exists,
                    time.monotonic() + 8, process=core)
                ready = json.loads((directory / 'ready.json').read_text())
                assert (ready['inherited'] >= 0) == initially_enabled
                # Toggle after the fork, before either config request.
                core.process.send_signal(signal.SIGUSR2)
                connection.echo('core-logging-toggled')
                if not initially_enabled:
                    wait_for_predicate('parent logging enabled', log.exists,
                        time.monotonic() + 5, process=core)
                if initially_enabled:
                    wait_for_predicate('parent logging disabled',
                        lambda: 'log closed (because of SIGUSR2)' in log.read_text(),
                        time.monotonic() + 5, process=core)
                (directory / 'request').touch()
                wait_for_predicate('delayed configuration result', (directory / 'result.json').exists,
                    time.monotonic() + 8, process=core)
                rows = json.loads((directory / 'result.json').read_text())
                assert all(row['inherited'] == row['advertised'] for row in rows), rows
                assert (directory / 'private-resource').stat().st_size == 0
                if initially_enabled:
                    assert log.read_bytes().count(b'fork-time-log-marker\n') == 2
                evidence['cases'][label] = rows
                connection.echo('delayed-module-done')
            connection.send(b'Quit\n')
            assert core.wait_until_exit(time.monotonic() + 5) == 0
            evidence['status'] = 'passed'
        finally:
            if connection:
                connection.close()
            for process in reversed(runtime.processes):
                process.close(time.monotonic() + 5)
            evidence['children_after_cleanup'] = {p.name: p.poll() for p in runtime.processes}
            for path in runtime.root.rglob('*'):
                if path.is_file() and path.name != 'LoggingProbe':
                    target = args.artifacts_dir / path.relative_to(runtime.root)
                    target.parent.mkdir(parents=True, exist_ok=True)
                    shutil.copy2(path, target)
            (args.artifacts_dir / 'evidence.json').write_text(json.dumps(evidence, indent=2) + '\n')
    print('fork-time module LoggingFD provenance passed')


if __name__ == '__main__':
    if len(sys.argv) > 6 and sys.argv[6] == 'probe':
        module_probe(sys.argv[7], sys.argv[8])
    else:
        parser = argparse.ArgumentParser(description=__doc__)
        for name in ['fvwm', 'mfl', 'artifacts-dir']:
            parser.add_argument('--' + name, type=Path, required=True)
        parser.add_argument('--display')
        options = parser.parse_args()
        options.fvwm, options.mfl = options.fvwm.resolve(), options.mfl.resolve()
        run(options)
