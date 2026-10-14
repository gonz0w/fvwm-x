#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
"""Check isolated SVG secondary waits, ordered recovery and cancellation."""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import time

from geometry_transition_live import Client
from live_x_harness import (HarnessError, HarnessRuntime, start_display_server,
                            wait_for_predicate)
from mfl_stream_live import Connection


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--probe', required=True)
    parser.add_argument('--fvwm', required=True)
    parser.add_argument('--mfl', type=Path, required=True)
    parser.add_argument('--display')
    parser.add_argument('--artifacts-dir', type=Path, required=True)
    args = parser.parse_args()
    features = json.loads(subprocess.check_output(
        [args.probe, '--features'], text=True))
    if not features['svg']:
        return
    timeout = features['svg_timeout_ms'] / 1000
    args.artifacts_dir.mkdir(parents=True, exist_ok=True)
    artifacts = Path(tempfile.mkdtemp(prefix='run-', dir=args.artifacts_dir))
    checks, observations = {}, {}
    with HarnessRuntime(prefix='fvwm-svg-worker-') as runtime:
        connection = client = None
        try:
            display = args.display
            if display is None:
                _, display = start_display_server(
                    runtime, 'Xvfb', ['Xvfb', '-screen', '0', '800x600x24',
                                     '-noreset'], 'xserver.log',
                    time.monotonic() + 15)
            waiting = runtime.root / 'waiting.png'
            os.mkfifo(waiting, 0o600)
            blocked = runtime.root / 'secondary.svg'
            blocked.write_text(
                '<svg xmlns="http://www.w3.org/2000/svg" '
                'xmlns:xlink="http://www.w3.org/1999/xlink" '
                'width="2" height="2"><image xlink:href="waiting.png" '
                'width="2" height="2"/></svg>')
            valid = runtime.root / 'valid.svg'
            valid.write_text('<svg xmlns="http://www.w3.org/2000/svg" '
                             'width="2" height="2"><rect width="2" '
                             'height="2" fill="red"/></svg>')
            began = time.monotonic()
            probe = runtime.start(
                'probe', [args.probe, 'refuse-pixmap', str(blocked)],
                'probe.log', runtime.environment({'DISPLAY': display}))
            if probe.wait_until_exit(began + timeout + 10) != 0:
                raise HarnessError(probe.log_path.read_text())
            elapsed = time.monotonic() - began
            assert timeout * 0.85 <= elapsed <= timeout + 10, elapsed
            observations['probe_secondary_timeout_seconds'] = elapsed
            checks['secondary-timeout-output-reset-fd-cleanup'] = 'passed'
            config = runtime.root / 'config'
            config.write_text(f'ModulePath {args.mfl.parent}\n'
                              'Style * TileCascadePlacement\n'
                              'AddToFunc StartFunction I Module FvwmMFL\n')

            def core_start(name):
                core = runtime.start(
                    name, [args.fvwm, '-d', display, '-f', str(config),
                           '-v', '-o', str(runtime.root / 'debug.log')],
                    name + '.log', runtime.environment(
                        {'FVWMMFL_SOCKET_PATH': str(runtime.root)}))
                endpoint = runtime.root / (
                    f"fvwm_mfl_{display.replace(':', '_')}.sock")
                wait_for_predicate('SVG endpoint', endpoint.exists,
                                   time.monotonic() + 15, process=core)
                return core, Connection(endpoint)

            core, connection = core_start('core')
            connection.record()
            connection.send(b'set echo\n')
            connection.echo('ready')

            def barrier(tag, budget):
                connection.send(f'Echo {tag}\n'.encode())
                deadline = time.monotonic() + budget
                connection.socket.settimeout(budget)
                while time.monotonic() < deadline:
                    if connection.record().get('echo', {}).get('message') == tag:
                        return
                raise HarnessError(f'missing ordered SVG barrier {tag}')

            connection.send(f'Colorset 7 Pixmap {blocked}\n'.encode())
            began = time.monotonic()
            barrier('during-secondary-wait', timeout + 10)
            elapsed = time.monotonic() - began
            assert elapsed < timeout * 0.85, elapsed
            observations['core_command_during_secondary_wait_seconds'] = elapsed
            checks['core-command-service-during-secondary-wait'] = 'passed'
            client = Client(display)
            client.create()
            wait_for_predicate('client during SVG wait',
                               lambda: client.frame() is not None,
                               time.monotonic() + timeout * 0.85, process=core)
            checks['core-manages-client-during-secondary-wait'] = 'passed'
            wait_for_predicate('asynchronous secondary timeout diagnostic',
                lambda: 'SVG refused:' in (runtime.root / 'debug.log').read_text(),
                began + timeout + 10, process=core)
            observations['core_secondary_timeout_seconds'] = time.monotonic() - began
            checks['core-asynchronous-secondary-timeout'] = 'passed'
            connection.send(f'Colorset 7 Pixmap {valid}\n'.encode())
            barrier('valid-after-timeout', 10)
            checks['core-valid-svg-after-timeout'] = 'passed'
            client.destroy()
            client.create()
            wait_for_predicate('client after SVG timeout',
                               lambda: client.frame() is not None,
                               time.monotonic() + 10, process=core)
            checks['core-manages-client-after-timeout'] = 'passed'
            client.close()
            client = None
            connection.send(f'Colorset 8 Pixmap {blocked}\n'.encode())
            # Shutdown cancels pending work before its total deadline.
            time.sleep(min(0.25, timeout / 4))
            began = time.monotonic()
            core.process.terminate()
            assert core.wait_until_exit(began + 3) == 0
            observations['core_shutdown_seconds'] = time.monotonic() - began
            checks['core-shutdown-cancels-worker'] = 'passed'
        finally:
            if connection:
                connection.close()
            if client:
                client.close()
            for log in runtime.root.glob('*.log'):
                shutil.copyfile(log, artifacts / log.name)
            (artifacts / 'results.json').write_text(json.dumps(
                {'features': features, 'checks': checks,
                 'observations': observations}, indent=2) + '\n')
    print(f'{len(checks)} SVG isolation checks passed')


if __name__ == '__main__':
    main()
