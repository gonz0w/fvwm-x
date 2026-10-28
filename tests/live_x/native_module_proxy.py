#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
"""Bounded verbatim native-module relay for fixture-owned child processes.

The standalone pinboard entry imports only the Python standard library. Existing
fixtures retain their module_proxy import through native_bindings_live.
"""
import json
import os
from pathlib import Path
import select
import signal
import struct
import subprocess
import sys
import time

WORD = struct.calcsize('@L')
MAX_RECORD_BYTES = 8 * 1024 * 1024


def _atomic_json(path, value):
    temporary = path.with_suffix('.tmp')
    temporary.write_text(json.dumps(value, indent=2) + '\n')
    temporary.replace(path)


def terminate(child):
    if child.poll() is None:
        child.terminate()
        try:
            child.wait(timeout=.7)
        except subprocess.TimeoutExpired:
            child.kill()
            child.wait(timeout=.7)


def module_proxy(real, directory, arguments, *, record_end_config=False):
    """Relay both pipes verbatim; optionally record config-END headers only."""
    from contextlib import ExitStack

    outgoing, incoming = map(int, arguments[:2])
    from_child, child_out = os.pipe()
    child_in, to_child = os.pipe()
    child = subprocess.Popen([real, str(child_out), str(child_in), *arguments[2:]],
        pass_fds=(child_out, child_in))
    os.close(child_out)
    os.close(child_in)
    root = Path(directory)
    token = str(os.getpid())
    _atomic_json(root / ('module-' + token + '.json'),
        {'proxy': os.getpid(), 'child': child.pid, 'real': real,
         'arguments': arguments[2:], 'started': time.monotonic()})
    saved = signal.getsignal(signal.SIGTERM)

    def stopped(signum, stack):
        raise KeyboardInterrupt('owned module proxy stopped')

    signal.signal(signal.SIGTERM, stopped)
    # Each direction has a fixed 1 MiB relay/parser cap. Never reorder, modify,
    # replay or generate module commands, including during shutdown.
    channels = {from_child: [outgoing, bytearray(), bytearray(), 'out', False],
                incoming: [to_child, bytearray(), bytearray(), 'in', False]}
    recorded = 0
    try:
        for fd in (from_child, incoming, outgoing, to_child):
            os.set_blocking(fd, False)
        with ExitStack() as logs:
            log = logs.enter_context((root / ('module-' + token + '.jsonl')).open('w', buffering=1))
            end_log = logs.enter_context((root / ('end-config-' + token + '.jsonl')).open(
                'w', buffering=1)) if record_end_config else None
            while channels:
                readers = [fd for fd, (_, pending, _, _, eof) in channels.items()
                           if not eof and len(pending) < 1024 * 1024 - 65536]
                writers = [v[0] for v in channels.values() if v[1]]
                ready, writable, _ = select.select(readers, writers, [], .05)
                for fd in ready:
                    target, pending, parsing, direction, _ = channels[fd]
                    data = os.read(fd, 65536)
                    if not data:
                        channels[fd][4] = True
                        continue
                    pending.extend(data)
                    parsing.extend(data)
                    assert len(parsing) <= 1024 * 1024
                    while True:
                        if direction == 'out':
                            if len(parsing) < 2 * WORD:
                                break
                            _, size = struct.unpack('@LL', parsing[:2 * WORD])
                            assert size <= 4096
                            length = 3 * WORD + size
                            if len(parsing) < length:
                                break
                            continuation = struct.unpack('@L', parsing[length-WORD:length])[0]
                            # SendQuitNotification uses the native terminal
                            # continuation 0. Relay it unchanged and drain the
                            # ordinary EOF path; other values are malformed.
                            assert continuation in (0, 1)
                            text = bytes(parsing[2 * WORD:2 * WORD + size])
                            kind = None
                        else:
                            if len(parsing) < 4 * WORD:
                                break
                            start, kind, words, _ = struct.unpack('@LLLL', parsing[:4 * WORD])
                            assert start == 0xffffffff and 4 <= words <= 4096
                            length = words * WORD
                            if len(parsing) < length:
                                break
                            text = (bytes(parsing[7 * WORD:length]).split(b'\0', 1)[0]
                                    if kind in (1 << 18, 1 << 22) else None)
                        del parsing[:length]
                        if end_log is not None and direction == 'in' and kind == 1 << 19:
                            row = json.dumps({'time': time.monotonic(), 'kind': kind}) + '\n'
                            recorded += len(row.encode())
                            assert recorded <= MAX_RECORD_BYTES
                            end_log.write(row)
                        if text is not None:
                            row = json.dumps({'time': time.monotonic(), 'direction': direction,
                                'kind': kind, 'text': text.decode('utf-8', 'replace'),
                                **({'continuation': continuation} if direction == 'out' else {})}) + '\n'
                            recorded += len(row.encode())
                            assert recorded <= MAX_RECORD_BYTES
                            log.write(row)
                for target in writable:
                    pending = next(v[1] for v in channels.values() if v[0] == target)
                    del pending[:os.write(target, pending)]
                # Preserve the final partial write before propagating EOF.
                # Closing a relay endpoint releases the opposite reader too.
                for fd, values in list(channels.items()):
                    if values[4] and not values[1]:
                        os.close(values[0])
                        os.close(fd)
                        del channels[fd]
            return 0
    except (KeyboardInterrupt, BrokenPipeError):
        return 0
    finally:
        # KillModule can begin teardown before the fixture stops the core's
        # whole process group. A repeated TERM must not interrupt the real
        # child Wait or its receipt after ownership has entered this unwind.
        signal.signal(signal.SIGTERM, signal.SIG_IGN)
        try:
            terminate(child)
            for fd in (outgoing, incoming, from_child, to_child):
                try:
                    os.close(fd)
                except OSError:
                    pass
            _atomic_json(root / ('stopped-' + token + '.json'),
                {'child': child.pid, 'returncode': child.returncode})
        finally:
            signal.signal(signal.SIGTERM, saved)


if __name__ == '__main__':
    raise SystemExit(module_proxy(sys.argv[1], sys.argv[2], sys.argv[3:],
        record_end_config=True))
