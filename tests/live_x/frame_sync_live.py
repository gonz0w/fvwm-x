#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.

"""Synchronous geometry snapshots must not hold an interactive grab open."""
import argparse
import ctypes as C
import json
from pathlib import Path
import shutil
import signal
import time

from geometry_transition_live import Client, Pointer
from mfl_stream_live import Connection
from live_x_harness import HarnessRuntime, start_display_server, wait_for_predicate


def run_case(options, operation, finish):
    runtime = HarnessRuntime(prefix="fvwm-frame-sync-")
    artifact = options.artifacts_dir.resolve() / f"{operation}-{finish}"
    artifact.mkdir(parents=True, exist_ok=True)
    prefix = runtime.root / "receiver"
    armed, unlock, packets = [Path(str(prefix) + "." + suffix)
                              for suffix in ("armed", "unlock", "packets")]
    client = pointer = connection = None
    evidence = {"operation": operation, "finish": finish}
    try:
        display = options.display
        if display is None:
            _, display = start_display_server(
                runtime, "Xvfb", ["Xvfb", "-screen", "0", "800x600x24", "-noreset"],
                "xserver.log", time.monotonic() + 10)
        log = runtime.root / "core.log"
        command = "Move" if operation == "move" else "Resize direction SE fixeddirection"
        process = runtime.start(
            "fvwm", [str(options.fvwm), "-d", display, "-v", "-o", str(log),
                     "-c", f"ModulePath {options.mfl.parent}:{options.producer.parent}",
                     "-c", "AddToFunc StartFunction I Module FvwmMFL",
                     "-c", "OpaqueMoveSize unlimited",
                     "-c", "Style * ResizeOpaque, BorderWidth 3, HandleWidth 3, "
                     "TileCascadePlacement, SnapAttraction 0, EdgeMoveDelay 0, "
                     "EdgeResizeDelay 0, FPAllowFocusClickFunction, FPAllowRaiseClickFunction",
                     "-c", "AddToFunc HeldFrame I Schedule 100 Echo frame-timer",
                     "-c", "+ I Echo frame-entered",
                     "-c", f"+ I {command}",
                     "-c", "+ I Echo frame-continuation",
                     "-c", "Mouse 1 W N HeldFrame"],
            "fvwm.log", runtime.environment({"FVWMMFL_SOCKET_PATH": str(runtime.root)}))
        endpoint = runtime.root / f"fvwm_mfl_{display.replace(':', '_')}.sock"
        wait_for_predicate("MFL endpoint", endpoint.exists,
                           time.monotonic() + 10, process=process)
        connection = Connection(endpoint)
        connection.record()
        connection.send(b"set echo\n")
        connection.echo("frame-ready")
        client = Client(display)
        pointer = Pointer(client)
        client.create()
        wait_for_predicate("client frame", client.frame,
                           time.monotonic() + 5, process=process)
        window = client.window
        connection.send(f"WindowId {window} Move 100p 100p\n"
                        f"WindowId {window} Focus\n"
                        f"Module {options.producer.name} {prefix} "
                        f"{finish if finish in ('async', 'disconnect') else 'stall'}\n".encode())
        wait_for_predicate("sync masks installed",
                           lambda: "CMD_Echo: frame-sync-ready" in log.read_text(),
                           time.monotonic() + 5, process=process)
        connection.echo("frame-configured")
        original = client.geometry(client.frame())
        armed.touch()
        pointer.motion(250, 210)
        pointer.button(True)
        wait_for_predicate("interaction entered",
                           lambda: "CMD_Echo: frame-entered" in log.read_text(),
                           time.monotonic() + 5, process=process)
        wait_for_predicate("pointer held", pointer.grabbed,
                           time.monotonic() + 5, process=process)
        for index in range(24):
            pointer.motion(260 + index * 2, 220 + index)
            time.sleep(.003)
        connection.send(b"Echo frame-independent\n")
        for marker in ("frame-timer", "frame-independent"):
            wait_for_predicate(marker, lambda marker=marker:
                               f"CMD_Echo: {marker}" in log.read_text(),
                               time.monotonic() + 5, process=process)
        if finish == "async":
            wait_for_predicate("async intermediate packets",
                               lambda: packets.exists() and
                               len(packets.read_text().splitlines()) >= 2,
                               time.monotonic() + 5, process=process)
            evidence["async_intermediate_updates_preserved"] = True
        else:
            assert not packets.exists(), "synchronous receiver saw an intermediate packet"
        assert pointer.grabbed() and pointer.position()[2] & (1 << 8)
        assert "CMD_Echo: frame-continuation" not in log.read_text()
        evidence["held_input_timer_and_snapshot_coalescing"] = True
        if finish == "escape":
            client.x.XKeysymToKeycode.argtypes = [C.c_void_p, C.c_ulong]
            client.x.XKeysymToKeycode.restype = C.c_ubyte
            pointer.x.XTestFakeKeyEvent.argtypes = [C.c_void_p, C.c_uint, C.c_int, C.c_ulong]
            key = client.x.XKeysymToKeycode(client.display, 0xff1b)
            for pressed in (1, 0):
                assert pointer.x.XTestFakeKeyEvent(client.display, key, pressed, 0)
            client.x.XSync(client.display, 0)
        elif finish == "destroy":
            client.destroy()
        elif finish == "unmap":
            client.x.XUnmapWindow.argtypes = [C.c_void_p, C.c_ulong]
            client.x.XUnmapWindow(client.display, window)
            client.x.XSync(client.display, 0)
        elif finish == "sigterm":
            process.process.send_signal(signal.SIGTERM)
        pointer.button(False)
        wait_for_predicate("pointer released", lambda: not pointer.grabbed(),
                           time.monotonic() + 5)
        evidence["release_without_receiver_reply"] = True
        if finish == "sigterm":
            assert process.wait_until_exit(time.monotonic() + 5) == 0
            assert not packets.exists()
            evidence["termination_discards_unsent_snapshot"] = True
        else:
            wait_for_predicate("parent continuation",
                               lambda: "CMD_Echo: frame-continuation" in log.read_text(),
                               time.monotonic() + 5, process=process)
            wait_for_predicate("final snapshot",
                               lambda: packets.exists() and bool(packets.read_text().strip()),
                               time.monotonic() + 5, process=process)
            rows = packets.read_text().splitlines()
            if finish != "async":
                assert len(rows) == 1 and rows[0].startswith(f"config {window} "), rows
                evidence["single_final_snapshot_after_parent_continuation"] = True
            if finish in ("release", "escape", "disconnect", "async"):
                actual = client.geometry(client.frame())
                if finish == "async":
                    wait_for_predicate("async final geometry", lambda:
                                       tuple(map(int, packets.read_text().splitlines()[-1].split()[2:])) == actual,
                                       time.monotonic() + 5, process=process)
                    rows = packets.read_text().splitlines()
                final = tuple(map(int, rows[-1].split()[2:]))
                assert final == actual, (final, actual)
                if finish == "escape":
                    assert actual == original, (actual, original)
                evidence["authoritative_final_geometry"] = list(final)
            unlock.touch()
            connection.echo("frame-recovered")
            # A fresh synchronous packet must consume its own unlock, with no
            # stale reply from a discarded intermediate packet.
            if finish in ("release", "escape"):
                connection.send(f"WindowId {window} Move 140p 130p\n".encode())
                connection.echo("frame-next-handshake")
                wait_for_predicate("next geometry snapshot",
                                   lambda: len(packets.read_text().splitlines()) >= 2,
                                   time.monotonic() + 5, process=process)
            evidence["reply_mutation_or_disconnect_and_healthy_recovery"] = True
    finally:
        unlock.touch()
        if pointer:
            pointer.button(False)
        if connection:
            connection.close()
        if client:
            client.close()
        for source in runtime.root.glob("*.log"):
            shutil.copy2(source, artifact / source.name)
        if packets.exists():
            shutil.copy2(packets, artifact / "packets.txt")
        runtime.close()
        (artifact / "evidence.json").write_text(json.dumps(evidence, indent=2) + "\n")
    return evidence


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("fvwm", "mfl", "producer", "artifacts-dir"):
        parser.add_argument(f"--{name}", type=Path, required=True)
    parser.add_argument("--display")
    options = parser.parse_args()
    print(json.dumps([run_case(options, operation, finish)
                      for operation in ("move", "resize")
                      for finish in ("release", "escape", "disconnect", "destroy", "unmap", "sigterm", "async")]))


if __name__ == "__main__":
    main()
