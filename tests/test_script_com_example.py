#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or any later version.
"""Exercise owned Script example peers without executing generated commands."""
import argparse
import errno
import json
import os
from pathlib import Path
import re
import select
import signal
import subprocess
import tempfile
import time


def wait(predicate, seconds=10):
    deadline = time.monotonic() + seconds
    while time.monotonic() < deadline:
        value = predicate()
        if value:
            return value
        time.sleep(.02)
    raise AssertionError('fixture deadline')


def changed_inode(path, previous):
    try:
        return path.stat().st_ino != previous
    except FileNotFoundError:
        return False


class Peer:
    def __init__(self, helper, root, legacy=False, timeout=5000):
        self.root = root
        # Legacy failure cleanup can signal its peer. Always use our own
        # disposable child, never the fixture runner or an arbitrary PID.
        self.peer = subprocess.Popen(['sleep', '120'])
        self.name = 'ComExample-' + str(self.peer.pid)
        self.input = root / ('.tmp-com-in-' + self.name)
        self.output = root / ('.tmp-com-out-' + self.name)
        command = (['perl', str(helper)] if legacy else [str(helper)])
        command += ['--com-name', self.name]
        if not legacy and timeout is not None:
            command += ['--timeout-ms', str(timeout)]
        self.process = subprocess.Popen(command, env=dict(os.environ,
            FVWM_USERDIR=str(root)), stdout=subprocess.PIPE, stderr=subprocess.PIPE)

    def writer(self):
        def opened():
            try:
                return (os.open(self.input, os.O_WRONLY | os.O_NONBLOCK),)
            except OSError as error:
                if error.errno in (errno.ENOENT, errno.ENXIO):
                    return None
                raise
        return wait(opened)[0]

    def send(self, command, answer=True):
        fd = self.writer()
        try:
            data = command
            while data:
                try:
                    count = os.write(fd, data)
                    data = data[count:]
                except BlockingIOError:
                    select.select([], [fd], [], 1)
        finally:
            os.close(fd)
        if not answer:
            return None
        wait(self.output.exists)
        fd = os.open(self.output, os.O_RDONLY | os.O_NONBLOCK)
        try:
            value = b''
            deadline = time.monotonic() + 10
            while time.monotonic() < deadline:
                ready = select.select([fd], [], [], .1)[0]
                if not ready:
                    continue
                chunk = os.read(fd, 4096)
                if chunk:
                    value += chunk
                elif value:
                    wait(lambda: not self.output.exists())
                    return value
                else:
                    time.sleep(.01)
            raise AssertionError('reply deadline')
        finally:
            os.close(fd)

    def close(self):
        # Reap only processes that this fixture started.
        for process in (self.process, self.peer):
            if process.poll() is None:
                process.terminate()
            try:
                process.communicate(timeout=10)
            except subprocess.TimeoutExpired:
                process.kill()
                process.communicate()


def multiline(text):
    lines = []
    for _ in range(3):
        assert len(text) >= 4 and text[:4].isdigit(), text
        count = int(text[:4])
        lines.append(text[4:4+count])
        assert len(lines[-1]) == count
        text = text[4+count:]
    assert not text
    for pattern, line in zip([rb'line1 \((\d+)\)',
            rb'Second Line \((\d+)\)', rb'An other Line \((\d+)\)'], lines):
        found = re.fullmatch(pattern, line)
        assert found and 0 <= int(found[1]) < 20, lines



def ordinary(helper, legacy, root):
    peer = Peer(helper, root, legacy, None)
    try:
        startup = peer.send(b'startup')
        assert startup.endswith(b'is up'), startup
        for i in range(1, 4):
            assert peer.send(b'count') == ('Get %d count messages' % i).encode()
        assert peer.send(b'unknown') == b'0'
        multiline(peer.send(b'multilines'))
        peer.send(b'exit', False)
        assert peer.process.wait(timeout=10) == 0
        assert not peer.input.exists() and not peer.output.exists()
        assert peer.peer.poll() is None
    finally:
        peer.close()


def run(args):
    results = {}
    with tempfile.TemporaryDirectory(prefix='script-com-') as directory:
        base = Path(directory)
        for label, helper, legacy in [('go', args.helper, False), ('legacy', args.legacy, True)]:
            root = base / label
            root.mkdir()
            ordinary(helper, legacy, root)
            results[label+'-ordinary-seven-requests'] = 'passed'
        for option in [[], ['--help'], ['--com-name', '0'], ['--com-name', 'ComExample-1'],
                ['--com-name', '../2'], ['--com-name', 'ComExample-2147483648'],
                ['--com-name', '2', '--timeout-ms', '0'],
                ['--com-name', '2', '--timeout-ms', '3600001'], ['--bad-option']]:
            result = subprocess.run([str(args.helper), *option], capture_output=True, timeout=10)
            assert result.returncode == (0 if option == ['--help'] else 1), (option, result)
        results['nine-cli-controls'] = 'passed'
        for label in ['regular-input', 'symlink-input', 'existing-pipe', 'regular-output',
                'symlink-output', 'existing-output', 'renamed-directory',
                'idle-term', 'idle-hup', 'connected-term', 'connected-timeout', 'unread-reply',
                'replaced-input', 'dead-peer', 'nul-command', 'oversize-command']:
            root = base / label
            root.mkdir()
            # Prepare names before helper startup using a fixture-owned peer.
            peer_child = subprocess.Popen(['sleep', '120'])
            name = 'ComExample-' + str(peer_child.pid)
            incoming = root / ('.tmp-com-in-' + name)
            outgoing = root / ('.tmp-com-out-' + name)
            target = root / 'sentinel'
            target.write_text('unchanged')
            if label == 'regular-input': incoming.write_text('unchanged')
            if label == 'symlink-input': incoming.symlink_to(target)
            if label == 'existing-pipe': os.mkfifo(incoming, 0o600)
            if label == 'regular-output': outgoing.write_text('unchanged')
            if label == 'symlink-output': outgoing.symlink_to(target)
            if label == 'existing-output': os.mkfifo(outgoing, 0o600)
            # Reuse the peer API with this child so preexisting endpoint names
            # are observed on the first creation attempt.
            peer = Peer.__new__(Peer)
            peer.root, peer.peer, peer.name = root, peer_child, name
            peer.input, peer.output = incoming, outgoing
            # Keep short overrides only for intentional timeout controls;
            # ordinary transfer/ownership cases use the production deadline.
            timeout = '1000' if label in ['connected-timeout', 'unread-reply'] else '5000'
            peer.process = subprocess.Popen([str(args.helper), '--com-name', name,
                '--timeout-ms', timeout], env=dict(os.environ, FVWM_USERDIR=str(root)),
                stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            try:
                if label in ['regular-input', 'symlink-input', 'existing-pipe']:
                    assert peer.process.wait(timeout=10) == 1
                    assert incoming.lstat()
                elif label in ['regular-output', 'symlink-output', 'existing-output']:
                    peer.send(b'count', False)
                    assert peer.process.wait(timeout=10) == 1
                    assert outgoing.lstat()
                    if label != 'existing-output':
                        assert outgoing.read_text() == 'unchanged'
                elif label in ['connected-term', 'connected-timeout']:
                    fd = peer.writer()
                    identity = incoming.stat().st_ino
                    try:
                        os.write(fd, b'cou')
                        if label == 'connected-term':
                            peer.process.terminate()
                            assert peer.process.wait(timeout=10) == 0
                        else:
                            wait(lambda: changed_inode(incoming, identity))
                            os.close(fd); fd = -1
                            assert peer.send(b'count') == b'Get 1 count messages'
                    finally:
                        if fd >= 0: os.close(fd)
                elif label == 'unread-reply':
                    peer.send(b'count', False)
                    wait(outgoing.exists)
                    wait(lambda: not outgoing.exists())
                    assert peer.send(b'count') == b'Get 2 count messages'
                elif label == 'renamed-directory':
                    wait(incoming.exists)
                    moved = base / 'moved-directory'
                    root.rename(moved)
                    root.mkdir()
                    peer.input, peer.output = moved / incoming.name, moved / outgoing.name
                    target = moved / 'sentinel'
                    assert peer.send(b'count') == b'Get 1 count messages'
                    peer.send(b'exit', False)
                    assert peer.process.wait(timeout=10) == 0
                    assert not peer.input.exists() and not peer.output.exists()
                    assert not list(root.iterdir())
                elif label == 'replaced-input':
                    # Path visibility precedes construction's ownership check.
                    # An open reader acknowledges that check; hold the writer
                    # through cancellation so EOF cannot start another request.
                    fd = peer.writer()
                    try:
                        incoming.unlink()
                        incoming.write_text('replacement')
                        peer.process.terminate()
                        status = peer.process.wait(timeout=10)
                        assert status in (0, 1), status
                        if status == 1:
                            # Keep the explicit replacement diagnostic strict.
                            _, diagnostics = peer.process.communicate(timeout=10)
                            assert b'endpoint was replaced' in diagnostics, diagnostics
                        assert incoming.read_text() == 'replacement'
                    finally:
                        os.close(fd)
                elif label == 'dead-peer':
                    wait(incoming.exists)
                    peer_child.terminate(); peer_child.wait(timeout=10)
                    assert peer.process.wait(timeout=10) == 0
                    assert not incoming.exists()
                elif label in ['nul-command', 'oversize-command']:
                    try: peer.send(b'x\0y' if label == 'nul-command' else b'x'*32001, False)
                    except BrokenPipeError: pass
                    assert peer.process.wait(timeout=10) == 1
                else:
                    wait(incoming.exists)
                    peer.process.send_signal(signal.SIGHUP if label == 'idle-hup' else signal.SIGTERM)
                    assert peer.process.wait(timeout=10) == 0
                assert target.read_text() == 'unchanged'
                if label != 'dead-peer': assert peer_child.poll() is None
                if label not in ['regular-input', 'symlink-input', 'existing-pipe', 'replaced-input']:
                    peer.close()
                    assert not incoming.exists()
                results[label] = 'passed'
            finally:
                peer.close()
    print(json.dumps(results, sort_keys=True))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--helper', type=Path, required=True)
    parser.add_argument('--legacy', type=Path, required=True)
    args = parser.parse_args()
    args.helper = args.helper.resolve()
    args.legacy = args.legacy.resolve()
    run(args)
