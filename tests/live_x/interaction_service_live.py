#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
"""Verify held-pointer servicing, per-producer barriers and sync deferral."""

import argparse
import ctypes as C
import json
from pathlib import Path
import shutil
import time

from geometry_transition_live import Client, Pointer
from mfl_stream_live import Connection
from live_x_harness import HarnessRuntime, start_display_server, wait_for_predicate


def run_case(args, operation):
    runtime = HarnessRuntime(prefix="fvwm-interaction-service-")
    client = connection = pointer = None
    pressed = False
    release_wait = getattr(args, "release_wait", False)
    root_interaction = operation in ("scroll", "reverse", "menu")
    context = "R" if root_interaction else "W"
    artifact = args.artifacts_dir / operation
    artifact.mkdir(parents=True, exist_ok=True)
    evidence = {"operation": operation, "release_wait": release_wait}
    try:
        display = args.display
        if display is None:
            _, display = start_display_server(
                runtime, "Xvfb", ["Xvfb", "-screen", "0", "800x600x24", "-noreset"],
                "xserver.log", time.monotonic() + 10)
        log = runtime.root / "core.log"
        command = {"move": "Move", "resize": "Resize direction SE fixeddirection",
                   "scroll": "Scroll", "reverse": "Scroll reverse", "menu": "Popup ServiceMenu"}[operation]
        process = runtime.start(
            "fvwm", [str(args.fvwm), "-d", display, "-v", "-o", str(log),
                     "-c", f"ModulePath {args.mfl.parent}:{args.producer.parent}",
                     "-c", "AddToFunc StartFunction I Module FvwmMFL",
                     "-c", "OpaqueMoveSize unlimited",
                     "-c", "TitleStyle Height 24",
                     "-c", "Style * ResizeOpaque, BorderWidth 3, HandleWidth 3, SnapAttraction 0, EdgeMoveDelay 0, EdgeResizeDelay 0, TileCascadePlacement, FPAllowFocusClickFunction, FPAllowRaiseClickFunction"],
            "fvwm.log", runtime.environment({"FVWMMFL_SOCKET_PATH": str(runtime.root)}))
        endpoint = runtime.root / f"fvwm_mfl_{display.replace(':', '_')}.sock"
        wait_for_predicate("MFL endpoint", endpoint.exists, time.monotonic() + 10, process=process)
        connection = Connection(endpoint)
        connection.record()
        connection.send(b"set echo\n")
        connection.echo("service-ready")
        client = Client(display)
        pointer = Pointer(client)
        client.create()
        wait_for_predicate("managed client", client.frame, time.monotonic() + 5, process=process)
        if operation == "menu":
            connection.send(b'AddToMenu ServiceMenu "Service" Title\n+ "Item" Nop\n')
        for directive in ("DestroyFunc HeldService",
                          "AddToFunc HeldService I Schedule 100 Echo service-timer",
                          "+ I Echo service-entered", f"+ I {command}",
                          "+ I Echo service-continuation", f"Mouse 1 {context} N HeldService"):
            connection.send((directive + "\n").encode())
        connection.echo("service-configured")
        prefix = runtime.root / "producer"

        def observed(message):
            return f"CMD_Echo: {message}" in log.read_text()

        def await_message(message):
            wait_for_predicate(message, lambda: observed(message), time.monotonic() + 5, process=process)

        def producer(mode, commands=""):
            Path(f"{prefix}.{mode}.commands").write_text(commands)
            connection.send(f"Module {args.producer.name} {prefix} {mode}\n".encode())
            await_message(f"service-producer-{mode}-ready")

        def gate(mode):
            Path(f"{prefix}.{mode}.gate").touch()
            wait_for_predicate(f"{mode} commands queued",
                               lambda: Path(f"{prefix}.{mode}.queued").exists(),
                               time.monotonic() + 5, process=process)

        def position():
            connection.send(f"WindowId {client.window} Move 100p 100p\n".encode())
            connection.send(f"WindowId {client.window} Focus\n".encode())
            connection.echo("service-positioned")
            pointer.motion(20, 40) if root_interaction else pointer.motion(250, 210)

        def begin():
            nonlocal pressed
            previous = log.read_text().count("CMD_Echo: service-entered")
            pointer.button(True)
            pressed = True
            wait_for_predicate("interactive command entered",
                               lambda: log.read_text().count("CMD_Echo: service-entered") > previous,
                               time.monotonic() + 5, process=process)
            wait_for_predicate("pointer grab", pointer.grabbed, time.monotonic() + 5, process=process)
            if release_wait:
                escape()

        def escape():
            client.x.XKeysymToKeycode.argtypes = [C.c_void_p, C.c_ulong]
            client.x.XKeysymToKeycode.restype = C.c_uint
            pointer.x.XTestFakeKeyEvent.argtypes = [C.c_void_p, C.c_uint,
                                                   C.c_int, C.c_ulong]
            code = client.x.XKeysymToKeycode(client.display, 0xff1b)
            assert code
            for down in (True, False):
                assert pointer.x.XTestFakeKeyEvent(client.display, code, down, 0)
            client.x.XSync(client.display, 0)
            # Enter the final wait before submitting independent commands.
            # The enclosing function must still wait for physical release.
            time.sleep(0.05)
            if pressed:
                assert pointer.grabbed()

        def release():
            nonlocal pressed
            if operation == "menu" and pressed:
                # FVWM's sticky Popup may legitimately remain open after a
                # release outside the menu. End it explicitly while held.
                escape()
            pointer.button(False)
            pressed = False
            wait_for_predicate("pointer release", lambda: not pointer.grabbed(),
                               time.monotonic() + 5, process=process)

        producer("async")
        producer("deferred", "Style * !Title\nUpdateStyles\nEcho service-deferred-tail\n")
        position()
        original_frame = client.frame()
        begin()
        gate("deferred")
        time.sleep(0.1)
        connection.send(b"Silent KeepRc Echo service-prefix\n"
                        b"SendToModule interaction-producer* ping\n"
                        b"Schedule 20 Echo service-extra-timer\n")
        started = time.monotonic()
        iteration = 0
        while time.monotonic() - started < 0.2:
            pointer.motion(270 + iteration % 2, 230 + iteration % 2)
            time.sleep(0.002)
            iteration += 1
        await_message("service-prefix")
        await_message("service-async-reply")
        await_message("service-extra-timer")
        await_message("service-timer")
        assert pointer.grabbed() and client.frame() == original_frame
        assert not observed("service-deferred-tail")
        assert not observed("service-continuation")
        # An unsafe command also holds this same producer's later safe input.
        connection.send(b"Colorset 7 bg #113355\nEcho service-mfl-tail\n")
        time.sleep(0.1)
        assert not observed("service-mfl-tail")
        release()
        for message in ("service-continuation", "service-deferred-tail", "service-mfl-tail"):
            await_message(message)
        assert client.geometry(client.frame())[3] - client.geometry(client.window)[3] < 24
        evidence["independent_input_timer_and_async_module"] = "passed"
        evidence["continuous_motion_service"] = "passed"
        evidence["deferred_style_update_and_same_producer_tail"] = "passed"
        connection.echo("service-after-deferred")

        # A producer's own active command includes its complex-function
        # continuation; its later safe input cannot overtake that command.
        connection.send(f"Mouse 1 {context} N {'-' if root_interaction else 'Nop'}\n".encode())
        connection.echo("service-active-configured")
        producer("active", f"WindowId {client.window} Function HeldService\nEcho service-active-tail\n")
        position()
        before = log.read_text().count("CMD_Echo: service-continuation")
        entered_before = log.read_text().count("CMD_Echo: service-entered")
        if not root_interaction:
            pointer.button(True)
            pressed = True
        # Root interactions can start from the module without a physical click.
        # Avoid a root click's own release wait preceding the active command.
        gate("active")
        wait_for_predicate("active producer interaction entered",
                           lambda: log.read_text().count("CMD_Echo: service-entered") > entered_before,
                           time.monotonic() + 5, process=process)
        wait_for_predicate("active producer pointer grab", pointer.grabbed,
                           time.monotonic() + 5, process=process)
        if release_wait:
            escape()
        connection.send(b"Echo service-active-independent\n")
        await_message("service-active-independent")
        time.sleep(0.15)
        assert not observed("service-active-tail")
        assert log.read_text().count("CMD_Echo: service-continuation") == before
        if root_interaction:
            escape()
        release()
        await_message("service-active-tail")
        contents = log.read_text()
        assert contents.rindex("CMD_Echo: service-continuation") < contents.index("CMD_Echo: service-active-tail")
        evidence["active_producer_continuation_order"] = "passed"

        # A dynamic command name and a complex function are deferred as a
        # whole. Safe-looking later input from the same producer stays behind.
        connection.send(b"SetEnv SERVICE_COMMAND Echo\n"
                        b"DestroyFunc ServiceMutation\n"
                        b"AddToFunc ServiceMutation I Style * Title\n"
                        b"+ I UpdateStyles\n"
                        b"+ I Echo service-complex-ran\n" +
                        f"Mouse 1 {context} N HeldService\n".encode())
        connection.echo("service-dynamic-configured")
        position()
        begin()
        connection.send(b"Schedule 60 Style * BorderWidth 7\n"
                        b"Schedule 70 Echo service-deferred-timer\n"
                        b"$[SERVICE_COMMAND] service-dynamic-name\n"
                        b"Function ServiceMutation\nEcho service-dynamic-tail\n")
        time.sleep(0.15)
        assert not observed("service-deferred-timer")
        assert not observed("service-dynamic-name")
        assert not observed("service-complex-ran")
        assert not observed("service-dynamic-tail")
        release()
        for message in ("service-dynamic-name", "service-complex-ran", "service-dynamic-tail"):
            await_message(message)
        await_message("service-deferred-timer")
        assert client.geometry(client.frame())[3] - client.geometry(client.window)[3] >= 24
        evidence["dynamic_name_and_complex_function_deferred"] = "passed"
        evidence["unsafe_timer_head_preserves_deadline_order"] = "passed"

        producer("sync")
        producer("independent", "Echo service-sync-independent\n")
        connection.send(f"Mouse 1 {context} N HeldService\n".encode())
        connection.echo("service-sync-configured")
        position()
        original_frame = client.frame()
        begin()
        connection.send(b"SendToModule interaction-producer* synchronous-ping\nEcho service-sync-tail\n")
        gate("independent")
        await_message("service-sync-independent")
        time.sleep(0.15)
        assert pointer.grabbed() and client.frame() == original_frame
        assert not observed("service-sync-reply") and not observed("service-sync-tail")
        release()
        await_message("service-sync-reply")
        await_message("service-sync-tail")
        connection.echo("service-healthy-after-sync")
        evidence["synchronous_receiver_deferred_and_unlocked"] = "passed"
    finally:
        if pressed and pointer is not None:
            pointer.button(False)
        if connection is not None:
            connection.close()
        if client is not None:
            client.close()
        for source in runtime.root.glob("*.log"):
            shutil.copy2(source, artifact / source.name)
        runtime.close()
        (artifact / "evidence.json").write_text(json.dumps(evidence, indent=2) + "\n")
    return evidence


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("fvwm", "mfl", "producer", "artifacts-dir"):
        parser.add_argument(f"--{name}", type=Path, required=True)
    parser.add_argument("--display")
    parser.add_argument("--release-wait", action="store_true")
    parser.add_argument("--viewport", action="store_true")
    parser.add_argument("--menu", action="store_true")
    args = parser.parse_args()
    operations = ("menu",) if args.menu else ("scroll", "reverse") if args.viewport else ("move", "resize")
    print(json.dumps([run_case(args, operation) for operation in operations]))


if __name__ == "__main__":
    main()
