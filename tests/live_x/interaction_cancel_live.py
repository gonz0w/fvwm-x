#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.

"""Real-pointer cancellation with lifecycle dispatch deferred during a drag."""

import argparse
import ctypes as C
import json
from pathlib import Path
import shutil
import signal
import sys
import time

from geometry_transition_live import Client, Pointer
from mfl_stream_live import Connection
from live_x_harness import HarnessRuntime, start_display_server, wait_for_predicate


def run_case(options, operation, cancellation, wireframe, title=False):
    runtime = HarnessRuntime(prefix="fvwm-interaction-cancel-")
    client = other = connection = pointer = None
    pressed = False
    label = f"{operation}-{cancellation}-{'wireframe' if wireframe else 'opaque'}"
    if title:
        label += "-title"
    artifact = options.artifacts_dir.resolve() / label
    artifact.mkdir(parents=True, exist_ok=True)
    evidence = {"case": label}
    try:
        display = options.display
        if display is None:
            _, display = start_display_server(
                runtime, "Xvfb", ["Xvfb", "-screen", "0", "800x600x24", "-noreset"],
                "xserver.log", time.monotonic() + 10)
        queue_probe = runtime.start(
            "queue-probe", [str(options.interaction_probe), display], "queue-probe.log")
        assert queue_probe.wait_until_exit(time.monotonic() + 5) == 0
        evidence["bounded_motion_and_lifecycle_queue_probe"] = True
        log = runtime.root / "core.log"
        command = "Move" if operation == "move" else "Resize direction SE fixeddirection"
        process = runtime.start(
            "fvwm", [str(options.fvwm), "-d", display, "-v", "-o", str(log),
                     "-c", f"ModulePath {options.mfl.parent}",
                     "-c", "AddToFunc StartFunction I Module FvwmMFL",
                     "-c", "TitleStyle Height 24",
                     "-c", f"OpaqueMoveSize {'0' if wireframe else 'unlimited'}",
                     "-c", "Style * BorderWidth 3, HandleWidth 3, SnapAttraction 0, "
                     "EdgeMoveDelay 0, EdgeResizeDelay 0, TileCascadePlacement, "
                     "ClickToFocus, FPAllowFocusClickFunction, FPAllowRaiseClickFunction, "
                     + ("ResizeOutline" if wireframe else "ResizeOpaque"),
                     "-c", "AddToFunc HeldCancel I Schedule 100 Echo cancel-timer",
                     "-c", "+ I Echo cancel-entered",
                     "-c", f"+ I {command}",
                     "-c", "+ I Echo cancel-continuation",
                     "-c", "Mouse 1 W N HeldCancel"],
            "fvwm.log", runtime.environment({"FVWMMFL_SOCKET_PATH": str(runtime.root)}))
        endpoint = runtime.root / f"fvwm_mfl_{display.replace(':', '_')}.sock"
        wait_for_predicate("MFL endpoint", endpoint.exists,
                           time.monotonic() + 10, process=process)
        connection = Connection(endpoint)
        connection.record()
        # The CLI accepts at most ten -c commands; install the title binding
        # through the module before the readiness barrier.
        connection.send(b"set echo\nMouse 1 T N HeldCancel\n")
        connection.echo("cancel-ready")
        client = Client(display)
        pointer = Pointer(client)
        client.create()
        wait_for_predicate("managed client", client.frame,
                           time.monotonic() + 5, process=process)
        if cancellation != "sigterm":
            other = Client(display)
            other.create()
            wait_for_predicate("other client", other.frame,
                               time.monotonic() + 5, process=process)
        connection.send(f"WindowId {client.window} Move 100p 100p\n".encode())
        connection.send(f"WindowId {client.window} Raise\n".encode())
        connection.send(f"WindowId {client.window} Focus\n".encode())
        if other is not None:
            connection.send(f"WindowId {other.window} Move 500p 300p\n".encode())
            connection.send(f"WindowId {other.window} Focus\n".encode())
        connection.echo("cancel-positioned")
        if wireframe:
            ready, trigger, queued = [runtime.root / name
                                      for name in ("ready", "trigger", "queued")]
            probe = runtime.start(
                "server-grab-probe", [sys.executable, str(Path(__file__).resolve()),
                    "--server-grab-probe", display, str(ready), str(trigger),
                    str(queued)], "server-grab.log")
            wait_for_predicate("server probe connected", ready.exists,
                               time.monotonic() + 5, process=probe)
        if title:
            x, y, width, height = client.geometry(client.frame())
            pointer.motion(x + width // 2, y + 12)
        else:
            pointer.motion(250, 210)
        pointer.button(True)
        pressed = True
        wait_for_predicate("interaction entered",
                           lambda: "CMD_Echo: cancel-entered" in log.read_text(),
                           time.monotonic() + 5, process=process)
        if wireframe:
            # A wireframe owns the server grab: another X client cannot query
            # that grab or generate new input until the WM releases it.
            trigger.write_text("query\n")
            wait_for_predicate("server probe query queued", queued.exists,
                               time.monotonic() + 5, process=probe)
            time.sleep(0.15)
            assert probe.poll() is None, "wireframe did not grab the server"
            connection.send(b"Echo cancel-wireframe-module\n")
            wait_for_predicate(
                "wireframe timer and independent module",
                lambda: all(f"CMD_Echo: {message}" in log.read_text()
                            for message in ("cancel-timer", "cancel-wireframe-module")),
                time.monotonic() + 5, process=process)
            evidence["wireframe_timer_and_module_servicing"] = True
        else:
            wait_for_predicate("pointer grab", pointer.grabbed,
                               time.monotonic() + 5, process=process)
            pointer.motion(280, 235)
        assert "CMD_Echo: cancel-continuation" not in log.read_text()
        if other is not None:
            other.destroy()
            time.sleep(0.1)
            assert pointer.grabbed(), "unrelated destruction cancelled the drag"
            assert "CMD_Echo: cancel-continuation" not in log.read_text()
        started = time.monotonic()
        if cancellation == "sigterm":
            process.process.send_signal(signal.SIGTERM)
            assert process.wait_until_exit(time.monotonic() + 2) == 0, "shutdown did not exit cleanly"
            if wireframe:
                assert probe.wait_until_exit(time.monotonic() + 2) == 0
                evidence["server_grab_verified_and_released"] = True
        else:
            if cancellation == "destroy":
                client.destroy()
            else:
                client.x.XUnmapWindow.argtypes = [C.c_void_p, C.c_ulong]
                client.x.XUnmapWindow(client.display, client.window)
                client.x.XSync(client.display, 0)
            wait_for_predicate("continuation after cancellation",
                               lambda: "CMD_Echo: cancel-continuation" in log.read_text(),
                               time.monotonic() + 2, process=process)
            wait_for_predicate("cancelled grab released", lambda: not pointer.grabbed(),
                               time.monotonic() + 2, process=process)
            connection.echo("cancel-still-responsive")
            assert process.poll() is None
        evidence["cancellation_seconds"] = time.monotonic() - started
        # Button remains physically down until both cancellation and cleanup
        # complete. This detects a hidden WaitForButtonsUp stall.
        evidence["completed_before_button_release"] = True
        pointer.button(False)
        pressed = False
        if cancellation != "sigterm":
            connection.send(b"Quit\n")
            assert process.wait_until_exit(time.monotonic() + 5) == 0
    finally:
        if pressed and pointer is not None:
            # Stop a failing wireframe before querying its grabbed X server.
            for process in runtime.processes:
                if process.name == "fvwm" and process.poll() is None:
                    process.process.kill()
                    process.wait_until_exit(time.monotonic() + 5)
            pointer.button(False)
        if connection is not None:
            connection.close()
        for item in (other, client):
            if item is not None:
                item.close()
        for path in runtime.root.glob("*.log"):
            shutil.copy2(path, artifact / path.name)
        runtime.close()
        (artifact / "result.json").write_text(json.dumps(evidence, indent=2) + "\n")
    return evidence


def run_server_grab_probe(display, ready, trigger, queued):
    client = Client(display)
    try:
        Path(ready).write_text("connected\n")
        wait_for_predicate("server probe trigger", Path(trigger).exists,
                           time.monotonic() + 15)
        Path(queued).write_text("query\n")
        # An already authenticated client avoids connection setup timeouts
        # while the wireframe owns the server. The round trip must block.
        client.x.XSync(client.display, 0)
    finally:
        client.close()


def main():
    if len(sys.argv) == 6 and sys.argv[1] == "--server-grab-probe":
        run_server_grab_probe(*sys.argv[2:])
        return
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("fvwm", "mfl", "interaction-probe", "artifacts-dir"):
        parser.add_argument(f"--{name}", required=True, type=Path)
    parser.add_argument("--display")
    options = parser.parse_args()
    cases = []
    for operation in ("move", "resize"):
        for cancellation in ("destroy", "unmap", "sigterm"):
            cases.append(run_case(options, operation, cancellation, False))
        cases.append(run_case(options, operation, "sigterm", True))
        for cancellation in ("destroy", "unmap"):
            cases.append(run_case(options, operation, cancellation, False, title=True))
    (options.artifacts_dir / "cancellation.json").write_text(
        json.dumps(cases, indent=2) + "\n")
    print(json.dumps(cases))


if __name__ == "__main__":
    main()
