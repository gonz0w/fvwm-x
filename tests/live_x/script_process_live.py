#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or any later version.
"""Exercise real GetOutput termination and cache ownership with native pipes."""
import argparse
import json
import os
from pathlib import Path
import selectors
import shutil
import signal
import struct
import time
from live_x_harness import HarnessRuntime, start_display_server, wait_for_predicate
from script_transport_live import packet, WORD


def run(args):
    args.module = args.module.resolve()
    args.artifacts_dir.mkdir(parents=True, exist_ok=True)
    results = {}
    with HarnessRuntime(prefix='fvwm-script-process-') as runtime:
        try:
            display = args.display
            if display is None:
                _, display = start_display_server(runtime, 'Xvfb',
                    ['Xvfb', '-screen', '0', '800x600x24', '-noreset'],
                    'xserver.log', time.monotonic() + 10)
            cases = ('open-stdout', 'closed-stdout', 'cache-retry')
            for label in cases if args.case == 'all' else (args.case,):
                report = runtime.root / (label + '.report')
                ready_file = runtime.root / (label + '.ready')
                late = runtime.root / (label + '.late')
                script = runtime.root / (label + '.script')
                if label == 'cache-retry':
                    # The first command's counter detects cache loss after the
                    # failing replacement. Retry uses the identical failed
                    # command and must execute again, not return partial text.
                    counter = runtime.root / 'cache-counter'
                    retry = runtime.root / 'retry-counter'
                    good = f'printf x >> {counter}; printf old'
                    bad = (f'if test -e {retry}; then printf recovered; else '
                        f'touch {retry}; printf partial; sleep 2; fi')
                    body = (f' Set $a=(GetOutput {{{good}}} 0 -1)\n'
                        f' Set $b=(GetOutput {{{bad}}} 0 -1)\n'
                        f' Set $c=(GetOutput {{{good}}} 0 -1)\n'
                        f' Set $d=(GetOutput {{{bad}}} 0 -1)\n'
                        ' WriteToFile $file $a {:} $b {:} $c {:} $d\n'
                        ' Quit\n')
                else:
                    close_output = 'exec 1>&-; ' if label == 'closed-stdout' else ''
                    command = (close_output +
                        f'(sleep 1; printf late > {late}) & '
                        f'printf "%s" $$ > {ready_file}; wait')
                    body = (f' Set $value=(GetOutput {{{command}}} 0 -1)\n'
                        ' WriteToFile $file {returned:} $value\n')
                script.write_text('WindowTitle {FvwmScriptProcess}\n'
                    'WindowSize 200 60\nFont {fixed}\nInit\nBegin\n' +
                    f' Set $file={{{report}}}\n' + body +
                    'End\nWidget 1\nProperty\n Type Rectangle\n'
                    ' Position 5 5\n Size 100 30\nMain\nCase message of\n'
                    '1 :\nBegin\n Quit\nEnd\nEnd\n')
                shutil.copy2(script, args.artifacts_dir / script.name)
                incoming, producer = os.pipe()
                consumer, outgoing = os.pipe()
                process = None
                try:
                    process = runtime.start(label, [str(args.module),
                        str(outgoing), str(incoming), '/dev/null', '0', '0',
                        str(script)], label + '.log', pass_fds=(incoming, outgoing),
                        environment=dict(os.environ, DISPLAY=display))
                    os.close(incoming)
                    incoming = -1
                    os.close(outgoing)
                    outgoing = -1
                    os.set_blocking(consumer, False)
                    buffer, commands = bytearray(), []
                    with selectors.DefaultSelector() as selector:
                        selector.register(consumer, selectors.EVENT_READ)
                        deadline = time.monotonic() + 10
                        while not any('Send_ConfigInfo' in c for c in commands):
                            remaining = deadline - time.monotonic()
                            assert remaining > 0 and selector.select(remaining)
                            data = os.read(consumer, 65536)
                            assert data
                            buffer.extend(data)
                            while len(buffer) >= 2 * WORD:
                                _, length = struct.unpack('@LL', buffer[:2 * WORD])
                                assert length <= 1000
                                size = 3 * WORD + length
                                if len(buffer) < size:
                                    break
                                commands.append(bytes(buffer[2 * WORD:
                                    2 * WORD + length]).decode())
                                del buffer[:size]
                    if label == 'cache-retry':
                        os.write(producer, packet(1 << 18, (0, 0, 0),
                            '*FvwmScriptCommandTimeout 200'))
                    os.write(producer, packet(1 << 19))
                    if label == 'cache-retry':
                        assert process.wait_until_exit(time.monotonic() + 5) == 0
                        lines = report.read_text().splitlines()
                        assert lines == ['', f'#{script},{process.process.pid}',
                            'old::old:recovered', '#end', ''], lines
                        assert counter.read_text() == 'x'
                    else:
                        wait_for_predicate('GetOutput shell is running',
                            lambda: ready_file.exists() and ready_file.stat().st_size > 0,
                            time.monotonic() + 10, process=process)
                        started = time.monotonic()
                        process.process.send_signal(signal.SIGTERM)
                        assert process.wait_until_exit(time.monotonic() + .8) == 0
                        results[label + '-exit-seconds'] = time.monotonic() - started
                        # The command's waiting shell and descendant share one
                        # owned group. A surviving descendant writes this marker.
                        time.sleep(1.1)
                        assert not late.exists(), label
                    log = process.log_path.read_text()
                    assert not any(s in log for s in
                        ('AddressSanitizer', 'runtime error:', 'Assertion')), log
                    results[label] = 'passed'
                    if report.exists():
                        shutil.copy2(report, args.artifacts_dir / report.name)
                finally:
                    # A baseline failure leaves the shell waiting for its child;
                    # retain the module as its parent until group cleanup. Never
                    # signal a PID after observing that its module exited.
                    if (process is not None and process.poll() is None and
                            ready_file.exists()):
                        try:
                            os.killpg(int(ready_file.read_text()), signal.SIGKILL)
                        except ProcessLookupError:
                            pass
                    for descriptor in (incoming, producer, consumer, outgoing):
                        if descriptor >= 0:
                            os.close(descriptor)
        finally:
            for path in runtime.root.iterdir():
                if path.is_file() and (path.suffix in ('.log', '.report') or
                        path.name in ('cache-counter', 'retry-counter')):
                    shutil.copy2(path, args.artifacts_dir / path.name)
            (args.artifacts_dir / 'evidence.json').write_text(
                json.dumps(results, indent=2) + '\n')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(allow_abbrev=False)
    parser.add_argument('--module', type=Path, required=True)
    parser.add_argument('--artifacts-dir', type=Path, required=True)
    parser.add_argument('--display')
    parser.add_argument('--case', choices=('all', 'open-stdout',
        'closed-stdout', 'cache-retry'), default='all')
    run(parser.parse_args())
