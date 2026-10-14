#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.

"""Observe timers, module input and function ordering during real pointer grabs.

This is a diagnostic baseline, not a passing responsiveness gate. Use
--require-responsive to assert that timers and module commands run while held.
The enclosing function must always continue only after the interaction ends.
"""

import argparse
import json
from pathlib import Path
import shutil
import time

from geometry_transition_live import Client, Pointer
from mfl_stream_live import Connection
from live_x_harness import HarnessRuntime, start_display_server, wait_for_predicate


def run(options):
    runtime = HarnessRuntime(prefix="fvwm-interaction-profile-")
    client = connection = None
    evidence = {"diagnostic_only": True, "cases": []}
    artifact = options.artifacts_dir.resolve()
    artifact.mkdir(parents=True, exist_ok=True)
    try:
        display = options.display
        if display is None:
            _, display = start_display_server(
                runtime, "Xvfb", ["Xvfb", "-screen", "0", "800x600x24", "-noreset"],
                "xserver.log", time.monotonic() + 10)
        log = runtime.root / "core.log"
        process = runtime.start(
            "fvwm", [str(options.fvwm), "-d", display, "-v", "-o", str(log),
                     "-c", f"ModulePath {options.mfl.parent}",
                     "-c", "AddToFunc StartFunction I Module FvwmMFL",
                     "-c", "TitleStyle Height 24",
                     "-c", "OpaqueMoveSize unlimited",
                     "-c", "Style * ResizeOpaque, BorderWidth 3, HandleWidth 3, SnapAttraction 0, EdgeMoveDelay 0, EdgeResizeDelay 0, TileCascadePlacement, FPAllowFocusClickFunction, FPAllowRaiseClickFunction"],
            "fvwm.log", runtime.environment({"FVWMMFL_SOCKET_PATH": str(runtime.root)}))
        endpoint = runtime.root / f"fvwm_mfl_{display.replace(':', '_')}.sock"
        wait_for_predicate("MFL endpoint", endpoint.exists,
                           time.monotonic() + 10, process=process)
        connection = Connection(endpoint)
        connection.record()
        connection.send(b"set echo\n")
        connection.echo("profile-ready")
        client = Client(display)
        pointer = Pointer(client)
        for operation, command in (("move", "Move"),
                                   ("resize", "Resize direction SE fixeddirection")):
            prefix = f"held-{operation}"
            for directive in (
                    "DestroyFunc HeldInteraction",
                    f"AddToFunc HeldInteraction I Schedule 100 Echo {prefix}-timer",
                    f"+ I Echo {prefix}-entered",
                    f"+ I {command}",
                    f"+ I Echo {prefix}-continuation",
                    "Mouse 1 W N HeldInteraction", "UpdateStyles"):
                connection.send((directive + "\n").encode())
            connection.echo(f"{prefix}-configured")
            client.create()
            wait_for_predicate("interaction client", client.frame,
                               time.monotonic() + 5, process=process)
            connection.send(f"WindowId {client.window} Move 100p 100p\n".encode())
            connection.send(f"WindowId {client.window} Focus\n".encode())
            connection.echo(f"{prefix}-positioned")
            pointer.motion(250, 210)
            pointer.button(True)
            try:
                wait_for_predicate("interaction entered",
                                   lambda: f"CMD_Echo: {prefix}-entered" in log.read_text(),
                                   time.monotonic() + 5, process=process)
                wait_for_predicate("interaction pointer grab", pointer.grabbed,
                                   time.monotonic() + 5, process=process)
                connection.send(f"Echo {prefix}-module\n".encode())
                started = time.monotonic()
                iteration = 0
                while time.monotonic() - started < options.hold_seconds:
                    pointer.motion(270 + iteration % 2, 230 + iteration % 2)
                    time.sleep(0.02)
                    iteration += 1
                contents = log.read_text()
                during = {name: f"CMD_Echo: {prefix}-{name}" in contents
                          for name in ("timer", "module", "continuation")}
                assert not during["continuation"], "function continued before release"
                case = {"operation": operation, "held_seconds": time.monotonic() - started,
                        "during_hold": during}
                evidence["cases"].append(case)
            finally:
                pointer.button(False)
            wait_for_predicate("interaction pointer released", lambda: not pointer.grabbed(),
                               time.monotonic() + 5, process=process)
            for name in ("timer", "module", "continuation"):
                wait_for_predicate(f"{name} after release",
                                   lambda: f"CMD_Echo: {prefix}-{name}" in log.read_text(),
                                   time.monotonic() + 5, process=process)
            contents = log.read_text()
            case["observed_order"] = sorted(
                ("entered", "timer", "module", "continuation"),
                key=lambda name: contents.index(f"CMD_Echo: {prefix}-{name}"))
            case["final_client"] = list(client.geometry(client.window)[2:])
            case["all_work_resumed_after_release"] = True
            connection.echo(f"{prefix}-finished")
            client.destroy()
        if options.require_responsive:
            assert all(case["during_hold"][name] for case in evidence["cases"]
                       for name in ("timer", "module")), evidence
    finally:
        if connection is not None:
            connection.close()
        if client is not None:
            client.close()
        for log in runtime.root.glob("*.log"):
            shutil.copy2(log, artifact / log.name)
        runtime.close()
        (artifact / "profile.json").write_text(json.dumps(evidence, indent=2) + "\n")
    print(json.dumps(evidence))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("fvwm", "mfl", "artifacts-dir"):
        parser.add_argument(f"--{name}", required=True, type=Path)
    parser.add_argument("--display")
    parser.add_argument("--hold-seconds", type=float, default=0.4)
    parser.add_argument("--require-responsive", action="store_true")
    options = parser.parse_args()
    if not 0.2 <= options.hold_seconds <= 10:
        parser.error("hold duration must be between 0.2 and 10 seconds")
    run(options)


if __name__ == "__main__":
    main()
