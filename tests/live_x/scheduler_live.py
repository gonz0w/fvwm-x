#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
"""Exercise scheduled waits, cancellation, periodic jobs and overdue fairness."""

import argparse
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
    runtime = HarnessRuntime(prefix="fvwm-scheduler-")
    args.artifacts_dir.mkdir(parents=True, exist_ok=True)
    evidence = {}
    connection = client = process = None
    try:
        display = args.display
        if not display:
            _, display = start_display_server(
                runtime, "Xvfb", ["Xvfb", "-screen", "0", "800x600x24", "-noreset"],
                "xserver.log", time.monotonic() + 10)
        process = runtime.start(
            "fvwm", [str(args.fvwm), "-d", display,
                     "-c", f"ModulePath {args.mfl.parent}",
                     "-c", "AddToFunc StartFunction I Module FvwmMFL",
                     "-c", "Style * TileCascadePlacement"],
            "fvwm.log", runtime.environment({"FVWMMFL_SOCKET_PATH": str(runtime.root)}))
        endpoint = runtime.root / f"fvwm_mfl_{display.replace(':', '_')}.sock"
        wait_for_predicate("MFL endpoint", endpoint.exists,
                           time.monotonic() + 10, process=process)
        connection = Connection(endpoint)
        connection.record()
        connection.send(b"set echo\n")
        connection.echo("ready")
        messages = []

        def next_echo():
            deadline = time.monotonic() + 5
            while time.monotonic() < deadline:
                message = connection.record().get("echo", {}).get("message")
                if message is not None:
                    messages.append(message)
                    return message
            raise AssertionError("missing scheduler Echo")

        connection.send(b"AddToFunc ScheduledWait I Echo entered\n"
                        b"+ I Schedule 0 Echo nested-timer\n"
                        b"+ I Wait \"FVWM transition probe\"\n"
                        b"+ I Echo finished\n"
                        b"Schedule 0 ScheduledWait\n")
        assert next_echo() == "entered"
        assert next_echo() == "nested-timer", "scheduled wait reexecuted itself"
        client = Client(display)
        client.create()
        assert next_echo() == "finished"
        connection.send(b"Echo after-wait\n")
        assert next_echo() == "after-wait"
        assert messages.count("entered") == messages.count("finished") == 1
        evidence["nested_wait_exactly_once"] = messages.copy()

        # Install and cancel before returning to the multiplexer. Separate
        # module commands can cross the 40-ms deadline before Deschedule runs.
        cancellation = runtime.root / "cancellation.fvwm"
        cancellation.write_text("Schedule 40 777 Echo cancelled-a\n"
                                "Schedule 40 777 Echo cancelled-b\n"
                                "Deschedule 777\n"
                                "Schedule 80 Echo cancellation-barrier\n")
        connection.send(f'Read "{cancellation}"\n'.encode())
        assert next_echo() == "cancellation-barrier"
        connection.send(b"Schedule Periodic 10 778 Echo periodic\n")
        for _ in range(3):
            assert next_echo() == "periodic"
        connection.send(b"Deschedule 778\nEcho periodic-stopped\n")
        while next_echo() != "periodic-stopped":
            assert messages[-1] == "periodic"
        connection.send(b"Schedule 60 Echo periodic-barrier\n")
        assert next_echo() == "periodic-barrier"
        evidence["group_cancellation_and_periodic_stop"] = "passed"

        # Stop inside Read before returning to the multiplexer. Unlike stopping
        # an idle select, this guarantees overdue work precedes its next poll.
        # MFL remains running and queues an independent module command.
        batch = runtime.root / "backlog.fvwm"
        paused = runtime.root / "paused"
        pause_script = runtime.root / "pause.sh"
        pause_script.write_text(f"kill -STOP {process.process.pid}\n"
                                f": > {paused}\nprintf 'Nop\\n'\n")
        batch.write_text("".join(f"Schedule 500 Echo burst-{i}\n" for i in range(256))
                         + f"PipeRead 'sh {pause_script}'\n")
        connection.send(f'Read "{batch}"\n'.encode())
        wait_for_predicate("WM stopped inside Read", paused.exists,
                           time.monotonic() + 5, process=process)
        try:
            connection.send(b"Echo module-during-backlog\n")
            time.sleep(0.6)
        finally:
            os.kill(process.process.pid, signal.SIGCONT)
        burst = []
        module_position = None
        while len(burst) < 256 or module_position is None:
            message = next_echo()
            if message == "module-during-backlog":
                module_position = len(burst)
            else:
                assert message == f"burst-{len(burst)}", message
                burst.append(message)
        assert 0 < module_position < 256, "timers did not yield within the backlog"
        evidence["overdue_backlog"] = {"ordered_jobs": len(burst),
                                      "module_after_jobs": module_position}
        connection.echo("healthy-after-scheduler")
    except BaseException as error:
        evidence["error"] = str(error)
        evidence["wm_exit_status"] = process.poll() if process else None
        raise
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
        (args.artifacts_dir / "evidence.json").write_text(
            json.dumps(evidence, indent=2) + "\n")
    print(json.dumps(evidence))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--fvwm", required=True, type=Path)
    parser.add_argument("--mfl", required=True, type=Path)
    parser.add_argument("--display")
    parser.add_argument("--artifacts-dir", required=True, type=Path)
    run(parser.parse_args())


if __name__ == "__main__":
    main()
