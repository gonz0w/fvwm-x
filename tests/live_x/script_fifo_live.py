#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.

"""Exercise the local named-pipe protocol through a real FvwmScript."""
import argparse
import json
import os
from pathlib import Path
import shutil
import signal
import sys
import time

from live_x_harness import HarnessRuntime, start_display_server, wait_for_predicate
from mfl_stream_live import Connection

HELPER = r'''
import os, pathlib, select, sys, time
root, name, payload = pathlib.Path(sys.argv[1]), sys.argv[2], sys.argv[3]
input = root / ('.tmp-com-in-' + name)
output = root / ('.tmp-com-out-' + name)
os.mkfifo(input, 0o600)
fd = os.open(input, os.O_RDONLY | os.O_NONBLOCK)
(root / (name + '-ready')).write_text('ready')
p = select.poll(); p.register(fd, select.POLLIN)
command = b''
while True:
    if not p.poll(1000): continue
    data = os.read(fd, 4096)
    command += data
    if not data: break
assert command == b'hello', command
os.close(fd)
if payload != '-':
    os.mkfifo(output, 0o600)
    fd = os.open(output, os.O_WRONLY)
    data = pathlib.Path(payload).read_bytes()
    while data:
        count = os.write(fd, data)
        data = data[count:]
    os.close(fd)
(root / (name + '-done')).write_text('done')
'''


def run(args):
    out = args.artifacts_dir
    out.mkdir(parents=True, exist_ok=True)
    for name in ('results.json', 'default-report', 'override-report', 'termination-report'):
        (out / name).unlink(missing_ok=True)
    evidence = {}
    profiles = {}
    with HarnessRuntime(prefix='fvwm-script-fifo-') as runtime:
        connection = None
        try:
            display = args.display
            if display is None:
                _, display = start_display_server(runtime, 'Xvfb',
                    ['Xvfb', '-screen', '0', '800x600x24', '-noreset'],
                    'xserver.log', time.monotonic() + 10)
            env = runtime.environment({'FVWMMFL_SOCKET_PATH': str(runtime.root),
                'FVWM_USERDIR': str(runtime.root)})
            config = runtime.root / 'config'
            config.write_text(f'ModulePath {args.mfl.parent}:{args.script.parent}\n'
                'Style * TileCascadePlacement\n'
                'AddToFunc StartFunction I Module FvwmMFL\n')
            core = runtime.start('core', [str(args.fvwm), '-d', display,
                '-f', str(config)], 'core.log', env)
            endpoint = runtime.root / f"fvwm_mfl_{display.replace(':', '_')}.sock"
            wait_for_predicate('FIFO endpoint', endpoint.exists,
                time.monotonic() + 10, process=core)
            connection = Connection(endpoint)
            connection.record()
            connection.send(b'set echo\n')
            connection.echo('fifo-wm-ready')
            helper_file = runtime.root / 'helper.py'
            helper_file.write_text(HELPER)

            def helper(name, value):
                payload = '-'
                if value is not None:
                    path = runtime.root / (name + '-payload')
                    path.write_bytes(value)
                    payload = str(path)
                return runtime.start('helper-' + name,
                    [sys.executable, str(helper_file), str(runtime.root), name, payload],
                    name + '.log', env)

            def lines(path):
                return path.read_text() if path.exists() else ''

            def observed(path, marker, seconds=15):
                wait_for_predicate(marker, lambda: marker in lines(path),
                    time.monotonic() + seconds, process=core)

            def launch(name, commands, report):
                script = runtime.root / name
                script.write_text('WindowTitle {' + name + '}\nWindowSize 120 60\n'
                    'Init\nBegin\n Set $file={' + str(report) + '}\n'
                    ' WriteToFile $file {pid:} (GetPid)\n' + commands + 'End\n'
                    'Widget 1\nProperty\n Type Rectangle\n Position 5 5\n'
                    ' Size 80 30\n Value 7\nMain\nCase message of\n'
                    '1 :\nBegin\n WriteToFile $file {alive:} (GetValue 1)\n'
                    'End\nEnd\n')
                shutil.copy2(script, out / name)
                connection.send(f'Module FvwmScript {script}\n'.encode())

            for name, value in [('valid', b'answer'), ('empty', b''),
                    ('max', b'x' * 32000), ('nul', b'x\0y'),
                    ('send', None), ('atomic', None)]:
                helper(name, value)
                observed(runtime.root / (name + '-ready'), 'ready')
            regular = runtime.root / '.tmp-com-in-regular'
            regular.write_text('unchanged')
            symlink = runtime.root / '.tmp-com-in-link'
            symlink.symlink_to(regular)
            report = runtime.root / 'default-report'
            commands = ' WriteToFile $file {before-delayed}\n'
            for name, mode in [('delayed', 1), ('valid', 1), ('empty', 1),
                    ('max', 1), ('nul', 1), ('send', 0), ('atomic', 2),
                    ('missing-atomic', 2), ('regular', 0), ('link', 2)]:
                commands += (f' Set $answer=(SendMsgAndGet {{{name}}} {{hello}} {mode})\n'
                    f' WriteToFile $file {{{name}:}} $answer\n')
            commands += (' WriteToFile $file {before-default}\n'
                ' Set $answer=(SendMsgAndGet {missing-default} {hello} 0)\n'
                ' WriteToFile $file {default:} $answer\n')
            launch('FifoDefault', commands, report)
            observed(report, 'before-delayed\n')
            helper('delayed', b'delayed-answer')
            observed(report, 'before-default\n', 20)
            start = time.monotonic()
            observed(report, 'default:0\n', 15)
            elapsed = time.monotonic() - start
            assert 4.5 <= elapsed < 15, elapsed
            text = report.read_text()
            for marker in ['delayed:delayed-answer\n', 'valid:answer\n',
                    'empty:\n', 'max:' + 'x' * 32000 + '\n', 'nul:0\n',
                    'send:1\n', 'atomic:1\n', 'missing-atomic:0\n',
                    'regular:0\n', 'link:0\n']:
                assert marker in text, marker[:80]
            assert regular.read_text() == 'unchanged' and symlink.is_symlink()
            for name in ['valid', 'empty', 'max', 'nul', 'send', 'atomic', 'delayed']:
                observed(runtime.root / (name + '-done'), 'done')
            evidence['legacy-startup-command-and-reply-protocol'] = 'passed'
            evidence['empty-and-complete-32000-byte-terminated-replies'] = 'passed'
            evidence['embedded-nul-refusal'] = 'passed'
            evidence['mode-zero-send-and-mode-two-atomic-send'] = 'passed'
            evidence['mode-two-missing-reader-immediate-recovery'] = 'passed'
            evidence['owned-regular-file-and-symlink-refusal-without-write'] = 'passed'
            evidence['five-second-default-deadline'] = 'passed'
            profiles['default_observed_seconds'] = elapsed
            connection.send(b'SendToModule FvwmScript SendString 1 1\n')
            observed(report, 'alive:7\n')
            evidence['default-timeout-next-widget-event-recovery'] = 'passed'

            # Invalid overrides keep the last valid setting.
            connection.send(b'*FvwmScript: FIFOTimeout 150\n'
                b'*FvwmScript: FIFOTimeout 0\n'
                b'*FvwmScript: FIFOTimeout 3600001\n'
                b'*FvwmScript: FIFOTimeout 150junk\n')
            connection.echo('fifo-option-barrier')
            override = runtime.root / 'override-report'
            launch('FifoOverride', ' WriteToFile $file {before-override}\n'
                ' Set $answer=(SendMsgAndGet {missing-override} {hello} 1)\n'
                ' WriteToFile $file {override:} $answer\n', override)
            observed(override, 'before-override\n')
            start = time.monotonic()
            observed(override, 'override:0\n', 5)
            elapsed = time.monotonic() - start
            assert 0.05 <= elapsed < 3, elapsed
            profiles['override_observed_seconds'] = elapsed
            connection.send(b'SendToModule FvwmScript SendString 1 1\n')
            observed(override, 'alive:7\n')
            evidence['configured-deadline-invalid-option-retention-and-recovery'] = 'passed'

            connection.send(b'*FvwmScript: FIFOTimeout 5000\n')
            connection.echo('fifo-termination-option')
            termination = runtime.root / 'termination-report'
            launch('FifoTermination', ' WriteToFile $file {waiting}\n'
                ' Set $answer=(SendMsgAndGet {missing-termination} {hello} 1)\n', termination)
            observed(termination, 'waiting\n')
            pid = int(next(line[4:] for line in termination.read_text().splitlines()
                if line.startswith('pid:')))
            start = time.monotonic()
            os.kill(pid, signal.SIGTERM)

            def gone():
                try:
                    os.kill(pid, 0)
                    return False
                except ProcessLookupError:
                    return True

            wait_for_predicate('terminated FIFO module reaped', gone,
                time.monotonic() + 3, process=core)
            profiles['termination_observed_seconds'] = time.monotonic() - start
            evidence['termination-cancels-held-communication'] = 'passed'
            connection.echo('fifo-wm-after-checks')
        finally:
            if connection:
                connection.close()
            for path in runtime.root.glob('*.log'):
                shutil.copy2(path, out / path.name)
            for name in ('default-report', 'override-report', 'termination-report'):
                if (runtime.root / name).exists():
                    shutil.copy2(runtime.root / name, out / name)
            (out / 'results.json').write_text(json.dumps(evidence, indent=2) + '\n')
            (out / 'profile.json').write_text(json.dumps(profiles, indent=2) + '\n')
    print('Real Script FIFO protocol, deadlines, endpoint refusal and recovery passed')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(allow_abbrev=False)
    for name in ('fvwm', 'mfl', 'script', 'artifacts-dir'):
        parser.add_argument('--' + name, type=Path, required=True)
    parser.add_argument('--display')
    args = parser.parse_args()
    for name in ('fvwm', 'mfl', 'script', 'artifacts_dir'):
        setattr(args, name, getattr(args, name).resolve())
    run(args)
