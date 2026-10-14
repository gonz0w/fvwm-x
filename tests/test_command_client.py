#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or any later version.
"""Exercise the installed Go command CLI against an independent local peer."""
import argparse
import json
import os
from pathlib import Path
import socket
import subprocess
import tempfile
import threading
import time

PROFILE = b'{"connection_profile":{"version":"fixture","id":9007199254740993}}\n'


def run(command):
    results = {}
    with tempfile.TemporaryDirectory(prefix='fc-') as temporary:
        root = Path(temporary)
        endpoint = root / 's'
        env = dict(os.environ)
        for key in ('FVWMMFL_SOCKET', 'FVWMMFL_SOCKET_PATH', 'XDG_RUNTIME_DIR', 'TMPDIR', 'DISPLAY'):
            env.pop(key, None)
        env['FVWMMFL_SOCKET'] = str(endpoint)

        def check(label, args, incoming=(PROFILE,), stdin=None, expected=b'Echo test\n',
                  status=0, delay=False, feedback=None, environment=None):
            listener = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
            listener.bind(str(endpoint))
            listener.listen(1)
            listener.settimeout(12)
            received = bytearray()
            failures = []

            def peer():
                try:
                    conn, _ = listener.accept()
                    with conn:
                        conn.settimeout(12)
                        if delay:
                            # Wait until the client's finite response deadline closes it.
                            while conn.recv(4096):
                                pass
                            return
                        if label == 'event-burst-during-input':
                            # The real producer reads commands while output is
                            # pending. A one-shot client need not consume every
                            # unsolicited event before closing; command receipt
                            # is still asserted byte for byte below.
                            def send_events():
                                try:
                                    for chunk in incoming:
                                        conn.sendall(chunk)
                                except (BrokenPipeError, ConnectionResetError):
                                    pass
                                except Exception as error:
                                    failures.append(repr(error))
                            sender = threading.Thread(target=send_events)
                            sender.start()
                            try:
                                while True:
                                    try:
                                        data = conn.recv(4096)
                                    except ConnectionResetError:
                                        break
                                    if not data:
                                        break
                                    received.extend(data)
                            finally:
                                sender.join(12)
                                if sender.is_alive():
                                    failures.append('event sender did not finish')
                        else:
                            for chunk in incoming:
                                conn.sendall(chunk)
                            while True:
                                data = conn.recv(4096)
                                if not data:
                                    break
                                received.extend(data)
                except (BrokenPipeError, ConnectionResetError):
                    if status == 0:
                        failures.append('unexpected peer disconnect')
                except Exception as error:
                    failures.append(repr(error))

            thread = threading.Thread(target=peer)
            thread.start()
            start = time.monotonic()
            try:
                result = subprocess.run([str(command), *args], input=stdin,
                                        capture_output=True, env=environment or env, timeout=12)
            finally:
                thread.join(timeout=13)
                listener.close()
                endpoint.unlink()
            assert not thread.is_alive() and not failures, (label, failures)
            assert result.returncode == status, (label, result.returncode, result.stdout, result.stderr)
            if expected is not None:
                assert bytes(received) == expected, (label, bytes(received))
            if feedback is not None:
                assert json.loads(result.stdout) == feedback, (label, result.stdout)
            if delay:
                assert 4.5 <= time.monotonic() - start < 12
                assert b'timed out' in result.stderr
            results[label] = 'passed'

        check('one-shot-framing', ['Echo', 'test'])
        check('explicit-path-no-display', ['-f', str(endpoint), 'Echo', 'test'])
        check('legacy-options', ['-FmSr', '-w', '10', 'Echo', 'test'])
        check('options-stop-at-command', ['Echo', '-d'], expected=b'Echo -d\n')
        check('piped-order-crlf-final-line', ['-c', 'ignored'],
              stdin=b'Echo first\r\n\nEcho caf\xc3\xa9\nEcho final',
              expected=b'Echo first\nEcho caf\xc3\xa9\nEcho final\n')
        check('command-limit', ['x' * 1000], expected=b'x' * 1000 + b'\n')
        check('stdin-limit-crlf-final', ['-c'], stdin=b'x' * 1000 + b'\r\n' + b'y' * 1000,
              expected=b'x' * 1000 + b'\n' + b'y' * 1000 + b'\n')
        check('event-burst-during-input', ['-c'], stdin=b'Echo test\n' * 1000,
              incoming=(PROFILE + b'{"event":"' + b'x' * 8000 + b'"}\n',) * 20,
              expected=b'Echo test\n' * 1000)
        check('fragmented-json-numeric-precision', ['-i', '1', 'Echo', 'test'],
              incoming=(PROFILE[:4], PROFILE[4:19], PROFILE[19:]), feedback=json.loads(PROFILE))
        check('coalesced-json-first-only', ['--info=1', 'Echo', 'test'],
              incoming=(PROFILE + b'{"echo":{"message":"later"}}\n',), feedback=json.loads(PROFILE))
        check('large-complete-json', ['-i', '1', 'Echo', 'test'],
              incoming=(b'{"large":"' + b'x' * 40000 + b'"}\n',), feedback={'large': 'x' * 40000})
        check('empty-input', ['-c'], stdin=b'', expected=b'')
        check('overlong-command', ['x' * 1001], expected=None, status=1)
        check('overlong-input', ['-c'], stdin=b'x' * 1001 + b'\n', expected=None, status=1)
        check('nul-refused', ['-c'], stdin=b'Echo bad\x00input\n', expected=None, status=1)
        check('argument-newline-refused', ['Echo bad\nEcho second'], expected=None, status=1)
        check('invalid-json', ['Echo', 'test'], incoming=(b'{bad}\n',), expected=None, status=1)
        check('invalid-utf8', ['Echo', 'test'], incoming=(b'{"x":"\xff"}\n',), expected=None, status=1)
        check('oversized-record', ['Echo', 'test'], incoming=(b'{' + b'x' * (256 * 1024),), expected=None, status=1)
        check('silent-peer-deadline', ['Echo', 'test'], incoming=(), delay=True, expected=None, status=1)
        for label, incoming in [('truncated-record', b'{'), ('early-eof', b'')]:
            # A real EOF is required to distinguish a truncated record from idle input.
            listener = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
            listener.bind(str(endpoint)); listener.listen(1); listener.settimeout(12)
            def close_peer():
                with listener.accept()[0] as conn:
                    conn.sendall(incoming)
                    conn.shutdown(socket.SHUT_WR)
                    conn.settimeout(12)
                    while conn.recv(4096):
                        pass
            thread = threading.Thread(target=close_peer); thread.start()
            result = subprocess.run([str(command), 'Echo', 'test'], capture_output=True, env=env, timeout=12)
            thread.join(13); listener.close(); endpoint.unlink()
            assert not thread.is_alive() and result.returncode == 1, (label, result.stderr)
            results[label] = 'passed'
        for label, args, status in [('help', ['--help'], 0), ('version', ['--version'], 0),
                                    ('unknown-option', ['--no-such-option'], 2), ('missing-value', ['-f'], 2),
                                    ('missing-socket', ['Echo', 'test'], 1)]:
            result = subprocess.run([str(command), *args], env=env, capture_output=True, timeout=12)
            assert result.returncode == status, (label, result)
            results[label] = 'passed'
    print(json.dumps(results, sort_keys=True))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--command', type=Path, required=True)
    run(parser.parse_args().command.resolve())
