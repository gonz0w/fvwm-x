#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
"""Stress real module admission with verification-only 16/8 queue limits."""

import argparse
import ctypes as C
import json
import os
from pathlib import Path
import shutil
import signal
import time

from geometry_transition_live import Client
from mfl_stream_live import Connection
from live_x_harness import HarnessRuntime, start_display_server, wait_for_predicate


def run(args):
    runtime = HarnessRuntime(prefix="fvwm-module-queue-")
    args.artifacts_dir.mkdir(parents=True, exist_ok=True)
    evidence = {}
    connection = client = process = None
    try:
        _, display = start_display_server(
            runtime, "Xvfb", ["Xvfb", "-screen", "0", "800x600x24", "-noreset"],
            "xserver.log", time.monotonic() + 10)
        process = runtime.start(
            "fvwm", [str(args.fvwm), "-d", display,
                     "-c", f"ModulePath {args.mfl.parent}:{args.producer.parent}",
                     "-c", "AddToFunc StartFunction I Module FvwmMFL"],
            "fvwm.log", runtime.environment({"FVWMMFL_SOCKET_PATH": str(runtime.root)}))
        endpoint = runtime.root / f"fvwm_mfl_{display.replace(':', '_')}.sock"
        wait_for_predicate("MFL endpoint", endpoint.exists,
                           time.monotonic() + 10, process=process)
        connection = Connection(endpoint)
        connection.record()
        connection.send(b"set echo\n")
        connection.echo("ready")
        gate = runtime.root / "producer"
        connection.send((f"ModuleSynchronous Timeout 5 {args.producer.name} {gate} sync\n"
                         "Echo queue-after-sync\n").encode())
        connection.expect_echo("queue-sync-started")
        connection.expect_echo("queue-after-sync")
        evidence["synchronous_startup"] = "passed"
        producer_count = 17
        for index in range(producer_count):
            connection.send(f"Module {args.producer.name} {gate} {index}\n".encode())
        wait_for_predicate(
            "all producer modules ready",
            lambda: all(Path(f"{gate}.{index}.ready").exists()
                        for index in range(producer_count)),
            time.monotonic() + 10, process=process)
        client = Client(display)
        for name in ("XGrabServer", "XUngrabServer"):
            function = getattr(client.x, name)
            function.argtypes, function.restype = [C.c_void_p], C.c_int
        client.x.XGrabServer(client.display)
        client.x.XSync(client.display, 0)
        try:
            os.kill(process.process.pid, signal.SIGSTOP)
            gate.touch()
            wait_for_predicate(
                "every producer has a buffered burst",
                lambda: all(Path(f"{gate}.{index}.buffered").exists()
                            for index in range(producer_count)),
                time.monotonic() + 5, process=process)
        finally:
            os.kill(process.process.pid, signal.SIGCONT)
            client.x.XUngrabServer(client.display)
            client.x.XSync(client.display, 0)
        seen = {index: 0 for index in range(producer_count)}
        total = 0
        independent_after = None
        independent_sent = False
        deadline = time.monotonic() + 20
        while total < producer_count * 32 or independent_after is None:
            assert time.monotonic() < deadline, "module burst did not complete"
            if not independent_sent and total > 0:
                if "queue-test: producer paused at 16 inputs" in process.log_path.read_text():
                    connection.send(b"Echo queue-independent-input\n")
                    independent_sent = True
                else:
                    assert total < producer_count * 32, "producer limit never paused reads"
            message = connection.record().get("echo", {}).get("message")
            if message is None:
                continue
            if message == "queue-independent-input":
                independent_after = total
                continue
            prefix, producer, sequence = message.split("-")
            assert prefix == "queue", message
            index, sequence = int(producer), int(sequence)
            assert sequence == seen[index] + 1, "module order changed or commands lost"
            seen[index] = sequence
            total += 1
        assert 0 < independent_after < total, "producer burst starved independent input"
        assert "queue-test: producer paused at 16 inputs" in process.log_path.read_text()
        evidence["backpressure"] = {"producers": producer_count,
                                    "commands_per_producer": 512,
                                    "ordered_echo_markers": total,
                                    "independent_after_markers": independent_after,
                                    "read_pause_threshold": 16,
                                    "dispatch_budget": 8}
        connection.send(f"SendToModule {args.producer.name} stop\n".encode())
        wait_for_predicate(
            "all modules receive stop and synchronous unlock completes",
            lambda: all(Path(f"{gate}.{index}.stopped").exists()
                        for index in list(range(producer_count)) + ["sync"]),
            time.monotonic() + 5, process=process)
        connection.echo("healthy-after-module-backpressure")
        evidence["synchronous_unlock_and_recovery"] = "passed"
    finally:
        if process is not None and process.poll() is None:
            os.kill(process.process.pid, signal.SIGCONT)
        if client:
            client.close()
        if connection:
            connection.close()
        for log in runtime.root.glob("*.log"):
            shutil.copy2(log, args.artifacts_dir / log.name)
        runtime.close()
        (args.artifacts_dir / "evidence.json").write_text(json.dumps(evidence, indent=2) + "\n")
    print(json.dumps(evidence))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--fvwm", required=True, type=Path)
    parser.add_argument("--mfl", required=True, type=Path)
    parser.add_argument("--producer", required=True, type=Path)
    parser.add_argument("--artifacts-dir", required=True, type=Path)
    run(parser.parse_args())


if __name__ == "__main__":
    main()
