#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.

"""Exercise MFL stream boundaries and isolation against a real WM."""

import argparse
import json
import os
from pathlib import Path
import select
import shutil
import signal
import socket
import time

from live_x_harness import HarnessRuntime, start_display_server, wait_for_predicate


class Connection:
    def __init__(self, endpoint):
        self.socket = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        self.socket.settimeout(5)
        try:
            self.socket.connect(str(endpoint))
        except BaseException:
            self.socket.close()
            raise
        self.pending = b""

    def send(self, data):
        self.socket.sendall(data)

    def record(self):
        while b"\n" not in self.pending:
            data = self.socket.recv(65536)
            if not data:
                raise AssertionError("unexpected MFL disconnect")
            self.pending += data
        line, self.pending = self.pending.split(b"\n", 1)
        return json.loads(line)

    def echo(self, message):
        self.send(b"Echo " + message.encode() + b"\n")
        self.expect_echo(message)

    def expect_echo(self, message):
        deadline = time.monotonic() + 5
        while time.monotonic() < deadline:
            if self.record().get("echo", {}).get("message") == message:
                return
        raise AssertionError(f"missing Echo {message!r}")

    def expect_closed(self):
        deadline = time.monotonic() + 5
        while time.monotonic() < deadline:
            try:
                data = self.socket.recv(65536)
            except ConnectionResetError:
                return
            if not data:
                return
        raise AssertionError("offending connection remained open")

    def close(self):
        self.socket.close()


def run(options):
    runtime = HarnessRuntime(prefix="fvwm-mfl-stream-")
    connections = []
    evidence = {}
    artifact = options.artifacts_dir.resolve()
    artifact.mkdir(parents=True, exist_ok=True)
    try:
        display = options.display
        if display is None:
            _, display = start_display_server(
                runtime, "Xvfb", ["Xvfb", "-screen", "0", "800x600x24", "-noreset"],
                "xserver.log", time.monotonic() + 10)
        process = runtime.start(
            "fvwm", [str(options.fvwm), "-d", display,
                     "-c", f"ModulePath {options.mfl.parent}",
                     "-c", "AddToFunc StartFunction I Module FvwmMFL",
                     "-c", "Style * TileCascadePlacement"],
            "fvwm.log", runtime.environment({"FVWMMFL_SOCKET_PATH": str(runtime.root)}))
        endpoint = runtime.root / f"fvwm_mfl_{display.replace(':', '_')}.sock"
        wait_for_predicate("MFL endpoint", endpoint.exists,
                           time.monotonic() + 10, process=process)

        def connect(subscribe=True):
            connection = Connection(endpoint)
            connections.append(connection)
            assert "connection_profile" in connection.record()
            if subscribe:
                connection.send(b"set echo\n")
                connection.echo("connection-ready")
            return connection

        healthy = connect()
        healthy.send(b"set enter_window\nunset enter_window\n")
        healthy.echo("echo-survives-other-unsubscribe")
        evidence["independent_extended_subscriptions"] = "passed"
        healthy.send(b"Echo unterminated")
        assert not select.select([healthy.socket], [], [], 0.2)[0], \
            "unterminated command executed"
        healthy.send(b"\n")
        healthy.expect_echo("unterminated")
        healthy.send(b"Ec")
        assert not select.select([healthy.socket], [], [], 0.1)[0]
        healthy.send(b"ho split-command\nEcho second-command\n")
        healthy.expect_echo("split-command")
        healthy.expect_echo("second-command")
        evidence["stream_boundaries"] = "passed"

        healthy.echo("x" * 995)  # Exactly the existing 1000-byte core limit.
        # Typed JSON has its own 4096-byte client frame limit; raw native
        # commands retain the 1000-byte core command limit below.
        boundary = ('request ' + json.dumps(dict(version=1, id='client-boundary',
            method='query', resource='connection'))).encode().ljust(4096, b' ')
        healthy.send(boundary[:1001])
        assert not select.select([healthy.socket], [], [], 0.1)[0]
        healthy.send(boundary[1001:] + b'\n')
        boundary_reply = healthy.record()['reply']
        assert boundary_reply['id'] == 'client-boundary' and boundary_reply['status'] == 'ok'
        healthy.send(('request ' + json.dumps(dict(version=1, id='separate-limits',
            method='query', resource='capabilities')) + '\n').encode())
        limits = healthy.record()['reply']
        assert limits['id'] == 'separate-limits' and limits['status'] == 'ok'
        assert limits['result']['max_line_bytes'] == 4096
        assert limits['result']['max_command_bytes'] == 1000
        evidence['typed_client_boundary_4096_native_boundary_1000'] = 'passed'
        for invalid in (b"Echo " + b"x" * 996 + b"\n", b"Echo\0hidden\n",
                        b"x" * 4097, b"set\nEcho\0hidden\n"):
            offending = connect(False)
            offending.send(invalid)
            offending.expect_closed()
            offending.close()
            healthy.echo("healthy-after-rejection")
            assert endpoint.exists() and process.poll() is None
        partial = connect(False)
        partial.send(b"Echo discarded-partial")
        partial.close()
        healthy.echo("healthy-after-partial-eof")
        evidence["limits_and_isolation"] = "passed"

        # More than both callback budgets in one write; continuation must drain.
        messages = [f"batch-{i}" for i in range(192)]
        healthy.send(b"".join(f"Echo {message}\n".encode() for message in messages))
        for message in messages:
            healthy.expect_echo(message)
        evidence["callback_continuation"] = len(messages)

        slow = connect()
        # Keep the producer draining while the second subscriber reads nothing.
        for batch in range(24):
            messages = [f"slow-{batch}-{i}-" + "x" * 850 for i in range(64)]
            healthy.send(b"".join(f"Echo {message}\n".encode() for message in messages))
            for message in messages:
                healthy.expect_echo(message)
        slow.expect_closed()
        slow.close()
        healthy.echo("healthy-after-slow-reader")
        evidence["slow_reader_isolation"] = "passed"

        subscribers = [connect(False) for _ in range(31)]
        excess = Connection(endpoint)
        connections.append(excess)
        excess.expect_closed()
        for subscriber in subscribers:
            subscriber.close()
        healthy.echo("healthy-after-connection-cap")
        evidence["connection_cap"] = 32
        connection = connect(False)
        healthy.send(b"unset echo\n")
        # Wait until the unsubscribe has been consumed before stopping the WM.
        time.sleep(0.05)
        os.kill(process.process.pid, signal.SIGSTOP)
        try:
            try:
                connection.send((b"Echo " + b"x" * 995 + b"\n") * 400)
            except (BrokenPipeError, ConnectionResetError):
                pass  # The receiver can reject before the entire write finishes.
            connection.expect_closed()
        finally:
            os.kill(process.process.pid, signal.SIGCONT)
        connection.close()
        # Queued commands keep their wire framing despite a full pipe.
        time.sleep(0.1)
        healthy.send(b"set echo\n")
        healthy.echo("healthy-after-core-backpressure")
        evidence["core_backpressure_isolation"] = "passed"
        if options.check_timeouts:
            partial = connect(False)
            partial.send(b"Ec")
            start = time.monotonic()
            sent = set()

            def partial_expired():
                # Trickle data must not extend the absolute per-line deadline.
                elapsed = time.monotonic() - start
                for threshold in (10, 20):
                    if elapsed >= threshold and threshold not in sent:
                        partial.send(b"x")
                        sent.add(threshold)
                return bool(select.select([partial.socket], [], [], 0)[0])

            wait_for_predicate("incomplete command deadline", partial_expired,
                               start + 33, process=process)
            partial.expect_closed()
            healthy.echo("idle-subscriber-survived-partial-timeout")
            evidence["partial_deadline_and_idle_subscription"] = "passed"
        assert process.poll() is None and endpoint.exists()
    finally:
        for connection in connections:
            connection.close()
        for log in runtime.root.glob("*.log"):
            shutil.copy2(log, artifact / log.name)
        runtime.close()
        (artifact / "evidence.json").write_text(json.dumps(evidence, indent=2) + "\n")
    print(json.dumps(evidence))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--fvwm", required=True, type=Path)
    parser.add_argument("--mfl", required=True, type=Path)
    parser.add_argument("--display")
    parser.add_argument("--check-timeouts", action="store_true")
    parser.add_argument("--artifacts-dir", required=True, type=Path)
    run(parser.parse_args())


if __name__ == "__main__":
    main()
