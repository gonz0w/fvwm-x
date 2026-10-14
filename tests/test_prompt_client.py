#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or any later version.
"""Exercise the built Prompt CLI with a controlled local MFL peer."""
import argparse
import json
import os
from pathlib import Path
import socket
import subprocess
import tempfile
import threading

PROFILE = b'{"connection_profile":{"version":"fixture","version_info":"test"}}\n'


def run(prompt):
    results = {}
    with tempfile.TemporaryDirectory(prefix='fp-') as temporary:
        root = Path(temporary)
        endpoint = root / 's'
        listener = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        listener.bind(str(endpoint))
        listener.listen(1)
        listener.settimeout(8)
        env = dict(os.environ, FVWMMFL_SOCKET=str(endpoint), FVWM_USERDIR=str(root), NO_COLOR='1')

        def check(label, args, incoming, stdin=None, expected=None, status=0, hold=False):
            received = bytearray()
            failures = []
            def peer():
                try:
                    conn, _ = listener.accept()
                    with conn:
                        conn.settimeout(8)
                        for chunk in incoming:
                            conn.sendall(chunk)
                        if hold:
                            # Deliberately leave stdin open: peer EOF must wake readline.
                            return
                        while True:
                            data = conn.recv(4096)
                            if not data:
                                break
                            received.extend(data)
                except Exception as error:
                    failures.append(error)
            thread = threading.Thread(target=peer)
            thread.start()
            process = subprocess.Popen([str(prompt), *args], stdin=subprocess.PIPE,
                                       stdout=subprocess.PIPE, stderr=subprocess.PIPE, env=env)
            try:
                if hold:
                    process.wait(timeout=8)
                    stdout, stderr = process.communicate(timeout=2)
                else:
                    stdout, stderr = process.communicate(stdin, timeout=10)
            finally:
                if process.poll() is None:
                    process.kill()
                    process.communicate()
                thread.join(timeout=10)
            if label == 'oversized-record-refused':
                # The client may reject before sendall finishes. A peer-side
                # broken pipe is expected only with the exact size refusal.
                assert b'bufio.Scanner: token too long' in stderr, stderr
                failures = [error for error in failures if not isinstance(
                    error, (BrokenPipeError, ConnectionResetError))]
            assert not thread.is_alive() and not failures, (label, failures)
            assert process.returncode == status, (label, process.returncode, stdout, stderr)
            if expected is not None:
                assert bytes(received) == expected, (label, bytes(received))
            if not args or args == ['-p', 'custom> ']:
                assert stderr.count(b'*FvwmPrompt') <= 1, (label, stderr)
            results[label] = 'passed'

        check('one-shot-complete-frame', ['Echo', 'one shot'], [], expected=b'Echo one shot\n')
        check('prompt-option-with-command', ['-p', 'custom> ', 'Echo', 'option'], [], expected=b'Echo option\n')
        assert not (root / '.FvwmConsole-History').exists(), 'one-shot opened interactive history'
        check('interactive-fragmented-coalesced-profile', [],
              [PROFILE[:3], PROFILE[3:23], PROFILE[23:] + b'{"echo":{"message":"ignored"}}\n'],
              b'Echo first\nEcho caf\xc3\xa9\nexit\n', b'Echo first\nEcho caf\xc3\xa9\n')
        check('quit-local-confirmation', [], [PROFILE], b'Quit\nn\nQuit\ny\nexit\n', b'Quit\n')
        check('local-invalid-command-keeps-session', [], [PROFILE],
              b'x' * 1001 + b'\nEcho after-refusal\nexit\n', b'Echo after-refusal\n')
        check('peer-eof-wakes-input', [], [PROFILE], hold=True)
        check('incomplete-json-refused', [], [PROFILE + b'{'], status=1, hold=True)
        check('invalid-json-refused', [], [PROFILE + b'{bad}\n'], status=1, hold=True)
        check('oversized-record-refused', [], [PROFILE + b'{' + b'x' * (256 * 1024)], status=1, hold=True)
        listener.close()
        result = subprocess.run([str(prompt), 'Echo', 'unavailable'], env=env, capture_output=True, timeout=8)
        assert result.returncode == 1 and b'unable to connect' in result.stderr
        results['connect-failure'] = 'passed'
    print(json.dumps(results, sort_keys=True))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--prompt', type=Path, required=True)
    args = parser.parse_args()
    run(args.prompt.resolve())
