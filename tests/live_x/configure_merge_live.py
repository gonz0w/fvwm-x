#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.

"""Check final geometry after queued client requests under explicit policies."""

import argparse
import ctypes as C
import json
import os
from pathlib import Path
import shutil
import signal
import time

from geometry_transition_live import Client, SizeHints
from mfl_stream_live import Connection
from live_x_harness import HarnessRuntime, start_display_server, wait_for_predicate


def run(options):
    runtime = HarnessRuntime(prefix="fvwm-configure-merge-")
    evidence = {}
    artifact = options.artifacts_dir.resolve()
    artifact.mkdir(parents=True, exist_ok=True)
    client = None
    connection = None
    process = None
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
        connection = Connection(endpoint)
        connection.record()
        connection.send(b"set echo\n")
        for method in ("AutoDetect", "UseGravity", "IgnoreGravity"):
            connection.send(f"Style * MoveByProgramMethod {method}\n".encode())
            connection.echo(f"style-{method}")
            client = Client(display)
            client.create()
            wait_for_predicate("managed frame", lambda: client.frame() is not None,
                               time.monotonic() + 5, process=process)
            # Own the server before stopping the WM. Frame existence alone
            # does not mean management has released its server grab; stopping
            # that owner would deadlock our subsequent XSync.
            for name in ("XGrabServer", "XUngrabServer"):
                function = getattr(client.x, name)
                function.argtypes, function.restype = [C.c_void_p], C.c_int
            client.x.XGrabServer(client.display)
            client.x.XSync(client.display, 0)
            try:
                os.kill(process.process.pid, signal.SIGSTOP)
                for width in range(220, 341):
                    client.x.XResizeWindow(client.display, client.window, width, 180)
                client.x.XSync(client.display, 0)
            finally:
                os.kill(process.process.pid, signal.SIGCONT)
                client.x.XUngrabServer(client.display)
                client.x.XSync(client.display, 0)
            wait_for_predicate("latest configure size",
                               lambda: client.geometry(client.window)[2:] == (340, 180),
                               time.monotonic() + 5, process=process)
            # Relevant property changes and subsequent constrained sizes must
            # leave the newest requested geometry usable.
            hints = SizeHints(flags=(1 << 4) | (1 << 6), min_width=100,
                              min_height=100, width_inc=10, height_inc=10)
            client.x.XSetWMNormalHints(client.display, client.window, C.byref(hints))
            client.x.XResizeWindow(client.display, client.window, 350, 190)
            client.x.XSync(client.display, 0)
            wait_for_predicate("geometry after size-hint change",
                               lambda: client.geometry(client.window)[2:] == (350, 190),
                               time.monotonic() + 5, process=process)
            evidence[method] = {"burst_final": [340, 180], "hint_final": [350, 190]}
            client.close()
            client = None
        connection.echo("healthy-after-configure-bursts")
    finally:
        if process is not None and process.poll() is None:
            os.kill(process.process.pid, signal.SIGCONT)
        if client is not None:
            client.close()
        if connection is not None:
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
    parser.add_argument("--artifacts-dir", required=True, type=Path)
    run(parser.parse_args())


if __name__ == "__main__":
    main()
