#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or any later version.
"""Exercise the actual Event module over owned native pipes."""
import argparse
import fcntl
import os
from pathlib import Path
import selectors
import signal
import struct
import subprocess
import sys
import tempfile
import time

WORD = struct.calcsize('@L')


def packet(kind, words=(), text=None):
    body = struct.pack('@' + 'L' * len(words), *words)
    if text is not None:
        raw = text.encode() + b'\0'
        body += raw + b'\0' * (-len(raw) % WORD)
    return struct.pack('@LLLL', 0xffffffff, kind, 4 + len(body) // WORD, 0) + body


class Module:
    def __init__(self, binary, root, arguments=(), incoming_minimum=0):
        incoming, self.producer = os.pipe()
        if incoming_minimum:
            replacement = fcntl.fcntl(incoming, fcntl.F_DUPFD_CLOEXEC,
                incoming_minimum)
            os.close(incoming)
            incoming = replacement
        self.consumer, outgoing = os.pipe()
        self.log = (root / 'event.log').open('wb')
        self.process = subprocess.Popen([str(binary), str(outgoing),
            str(incoming), '/dev/null', '0', '0'] + list(arguments),
            pass_fds=(incoming, outgoing),
            stdout=self.log, stderr=self.log)
        os.close(incoming)
        os.close(outgoing)
        os.set_blocking(self.consumer, False)
        self.buffer = bytearray()
        self.commands = []

    def receive(self, predicate, timeout=5):
        deadline = time.monotonic() + timeout
        with selectors.DefaultSelector() as ready:
            ready.register(self.consumer, selectors.EVENT_READ)
            while not predicate():
                remaining = deadline - time.monotonic()
                assert remaining > 0, self.commands
                assert ready.select(remaining), self.commands
                data = os.read(self.consumer, 65536)
                assert data, (self.process.poll(), self.commands)
                self.buffer.extend(data)
                while len(self.buffer) >= 2 * WORD:
                    window, count = struct.unpack('@LL', self.buffer[:2 * WORD])
                    assert count <= 1000
                    size = 3 * WORD + count
                    if len(self.buffer) < size:
                        break
                    text = bytes(self.buffer[2 * WORD:2 * WORD + count]).decode()
                    continuation, = struct.unpack('@L',
                        self.buffer[2 * WORD + count:size])
                    assert continuation == 1
                    self.commands.append((window, text))
                    del self.buffer[:size]

    def config(self, fragmented=False, pass_id=False):
        # The core strips the configuration colon before packet delivery.
        lines = ['', '*FvwmEventstartup "Echo startup-proof"',
                 '*FvwmEventshutdown "Echo shutdown-proof"',
                 '*FvwmEventfocus_change "Echo focus-proof"']
        if pass_id:
            lines.append('*FvwmEventPassID')
        data = b''.join(packet(1 << 18, (0, 0, 0), line) for line in lines)
        data += packet(1 << 19)
        if fragmented:
            for offset in range(0, len(data), 5):
                os.write(self.producer, data[offset:offset + 5])
                time.sleep(0.001)
        else:
            os.write(self.producer, data)
        self.receive(lambda: any(text == 'NOP FINISHED STARTUP'
            for _, text in self.commands))
        assert any('startup-proof' in text for _, text in self.commands)

    def close_input(self):
        os.close(self.producer)
        self.producer = -1

    def close(self):
        if self.process.poll() is None:
            self.process.terminate()
            try:
                self.process.wait(timeout=2)
            except subprocess.TimeoutExpired:
                self.process.kill()
                self.process.wait(timeout=2)
        for descriptor in (self.producer, self.consumer):
            if descriptor >= 0:
                os.close(descriptor)
        self.log.close()


def run(binary):
    with tempfile.TemporaryDirectory(prefix='fvwm-event-transport-') as directory:
        root = Path(directory)
        for label in ['fragmented', 'pass-id', 'short-body', 'tail',
                      'count-small', 'count-large', 'truncated',
                      'configuration-term', 'active-term', 'eof']:
            case_root = root / label
            case_root.mkdir()
            module = Module(binary, case_root)
            try:
                if label == 'configuration-term':
                    module.receive(lambda: any('Send_ConfigInfo' in text
                        for _, text in module.commands))
                    module.process.send_signal(signal.SIGTERM)
                else:
                    module.config(label == 'fragmented', label == 'pass-id')
                    if label in ['fragmented', 'pass-id', 'short-body', 'tail']:
                        window = 0xf0000001 if label == 'tail' else 23
                        if label == 'short-body':
                            os.write(module.producer, packet(1 << 6))
                        body = packet(1 << 6, (window, 0, 0), 'native-tail')
                        if label == 'tail':
                            body = packet(1 << 6, (window, 0, 0,
                                int.from_bytes(b'x' * WORD,
                                    byteorder=sys.byteorder)))
                        for offset in range(0, len(body), 3):
                            os.write(module.producer, body[offset:offset + 3])
                            time.sleep(0.001)
                        module.receive(lambda: any('focus-proof' in text
                            for _, text in module.commands))
                        actions = [(window, text) for window, text in module.commands
                                   if 'focus-proof' in text]
                        assert len(actions) == 1 and actions[0][0] == window
                        if label == 'pass-id':
                            assert actions[0][1].strip().endswith('0x17'), actions
                        elif label == 'tail':
                            assert 'xxxxxxxx' not in actions[0][1]
                        else:
                            assert actions[0][1].strip().endswith('native-tail')
                        module.close_input()
                    elif label.startswith('count-'):
                        count = 3 if label == 'count-small' else 257
                        os.write(module.producer,
                            struct.pack('@LLLL', 0xffffffff, 1 << 6, count, 0))
                    elif label == 'truncated':
                        data = packet(1 << 6, (23, 0, 0), 'native-tail')
                        os.write(module.producer, data[:4 * WORD + 3])
                        module.close_input()
                    elif label == 'active-term':
                        module.process.send_signal(signal.SIGTERM)
                    else:
                        module.close_input()
                assert module.process.wait(timeout=2) == 0, label
                if label == 'eof':
                    module.receive(lambda: any('shutdown-proof' in text
                        for _, text in module.commands))
                module.log.flush()
                text = (case_root / 'event.log').read_text()
                assert not any(marker in text for marker in
                    ['AddressSanitizer', 'runtime error:', 'Assertion']), text
                print(label, 'passed')
            finally:
                module.close()


if __name__ == '__main__':
    parser = argparse.ArgumentParser(allow_abbrev=False)
    parser.add_argument('--module', type=Path, required=True)
    run(parser.parse_args().module)
