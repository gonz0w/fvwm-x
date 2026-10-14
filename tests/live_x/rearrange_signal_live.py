#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or any later version.
"""Check actual FvwmRearrange cleanup through owned module pipes."""
import argparse
import json
import os
from pathlib import Path
import shutil
import signal
import struct
import time
from live_x_harness import HarnessRuntime, start_display_server, wait_for_predicate


def run(args):
    args.artifacts_dir.mkdir(parents=True, exist_ok=True)
    results = {}
    with HarnessRuntime(prefix='fvwm-rearrange-signal-') as runtime:
        try:
            display = args.display
            if display is None:
                _, display = start_display_server(runtime, 'Xvfb',
                    ['Xvfb', '-screen', '0', '800x600x24', '-noreset'],
                    'xserver.log', time.monotonic() + 10)
            cases = [('configuration-term', signal.SIGTERM, False),
                     ('configuration-pipe', signal.SIGPIPE, False),
                     ('active-alarm', signal.SIGALRM, True),
                     ('active-term', signal.SIGTERM, True),
                     ('active-int', signal.SIGINT, True),
                     ('active-pipe', signal.SIGPIPE, True),
                     ('active-eof', None, True),
                     ('blocked-output-term', signal.SIGTERM, False),
                     ('blocked-output-pipe', signal.SIGPIPE, False),
                     ('blocked-output-alarm', signal.SIGALRM, False)]
            for label, stop_signal, active in cases:
                incoming, producer = os.pipe()
                consumer, outgoing = os.pipe()
                try:
                    blocked_output = label.startswith('blocked-output-')
                    if blocked_output:
                        os.set_blocking(outgoing, False)
                        try:
                            while True:
                                os.write(outgoing, b'x')
                        except BlockingIOError:
                            pass
                        os.set_blocking(outgoing, True)
                    process = runtime.start(label, [str(args.probe),
                        str(outgoing), str(incoming), '/dev/null', '0', '0'],
                        label + '.log', pass_fds=(incoming, outgoing),
                        environment=dict(os.environ, DISPLAY=display))
                    os.close(incoming)
                    incoming = -1
                    os.close(outgoing)
                    outgoing = -1
                    if active:
                        # Normal end-of-configuration header, with no body.
                        os.write(producer, struct.pack('@LLLL',
                            0xffffffff, 1 << 19, 4, 0))
                    marker = ('rearrange-original-ten-second-deadline: armed'
                              if active else
                              'rearrange-configuration-read: ready')
                    if blocked_output:
                        marker = 'rearrange-command-write: ready'
                    wait_for_predicate(label,
                        lambda: marker in process.log_path.read_text(),
                        time.monotonic() + 10, process=process)
                    if stop_signal is None:
                        os.close(producer)
                        producer = -1
                    else:
                        os.kill(process.process.pid, stop_signal)
                    assert process.wait_until_exit(time.monotonic() + 2) == 0
                    text = process.log_path.read_text()
                    assert text.count('rearrange-normal-flow-cleanup: passed') == 1
                    for failure in ('AddressSanitizer', 'runtime error:',
                                    'Assertion', 'couldn\'t open display'):
                        assert failure not in text, text
                    results[label] = 'passed'
                finally:
                    for descriptor in (incoming, producer, consumer, outgoing):
                        if descriptor >= 0:
                            os.close(descriptor)
        finally:
            for path in runtime.root.glob('*.log'):
                shutil.copy2(path, args.artifacts_dir / path.name)
    (args.artifacts_dir / 'evidence.json').write_text(
        json.dumps(results, indent=2) + '\n')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(allow_abbrev=False)
    parser.add_argument('--probe', type=Path, required=True)
    parser.add_argument('--artifacts-dir', type=Path, required=True)
    parser.add_argument('--display')
    run(parser.parse_args())
