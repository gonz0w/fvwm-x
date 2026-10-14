#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.

"""Exercise FVWM themes, Read/PipeRead ordering, functions and real bindings."""

import argparse
import ctypes as C
import json
from pathlib import Path
import shutil
import time

from geometry_transition_live import Client, Pointer
from mfl_stream_live import Connection
from live_x_harness import HarnessRuntime, start_display_server, wait_for_predicate


def run(options):
    runtime = HarnessRuntime(prefix="fvwm-compat-")
    client = connection = None
    evidence = {}
    artifact = options.artifacts_dir.resolve()
    artifact.mkdir(parents=True, exist_ok=True)
    try:
        fixtures = Path(__file__).resolve().parent.parent / "fixtures" / "compat"
        for fixture in fixtures.iterdir():
            shutil.copy2(fixture, runtime.root / fixture.name)
        display = options.display
        if display is None:
            _, display = start_display_server(
                runtime, "Xvfb", ["Xvfb", "-screen", "0", "800x600x24", "-noreset"],
                "xserver.log", time.monotonic() + 10)
        log = runtime.root / "core.log"
        process = runtime.start(
            "fvwm", [str(options.fvwm), "-d", display, "-v", "-o", str(log),
                     "-c", f"ModulePath {options.mfl.parent}:{options.module_probe.parent}",
                     "-c", "AddToFunc StartFunction I Module FvwmMFL"],
            "fvwm.log", runtime.environment({
                "FVWMMFL_SOCKET_PATH": str(runtime.root),
                "FVWM_COMPAT_ROOT": str(runtime.root)}))
        endpoint = runtime.root / f"fvwm_mfl_{display.replace(':', '_')}.sock"
        wait_for_predicate("MFL endpoint", endpoint.exists,
                           time.monotonic() + 10, process=process)
        connection = Connection(endpoint)
        connection.record()
        connection.send(b"set echo\n")
        connection.echo("compat-ready")

        def load_theme():
            for name in ("read-fds.txt", "pipe-fds.txt"):
                (runtime.root / name).unlink(missing_ok=True)
            connection.send(f'Read "{runtime.root}/theme.fvwm"\n'.encode())
            connection.send(b"Echo compat-read-return\n")
            observed = []
            while True:
                message = connection.record().get("echo", {}).get("message")
                if message is not None:
                    observed.append(message)
                if message == "compat-read-return":
                    break
            expected = ["compat-read-begin", "compat-nested-begin",
                        "compat-nested-end", "compat-pipe-begin",
                        "compat-pipe-end", "compat-read-end", "compat-read-return"]
            assert observed == expected, observed
            for name in ("read-fds.txt", "pipe-fds.txt"):
                report = runtime.root / name
                wait_for_predicate("config module descriptor report",
                                   lambda: report.exists() and report.stat().st_size > 0,
                                   time.monotonic() + 5, process=process)
                assert report.read_text().strip() == "0", report.read_text()
            return observed

        evidence["read_piperead_order"] = load_theme()
        client = Client(display)
        client.create()
        wait_for_predicate("themed frame", lambda: client.frame() is not None,
                           time.monotonic() + 5, process=process)
        frame = client.frame()
        wait_for_predicate("theme extents",
                           lambda: client.geometry(frame)[2:] == (206, 150),
                           time.monotonic() + 5, process=process)
        connection.send(f"WindowId {client.window} CompatResize\n".encode())
        connection.expect_echo("compat-function-begin")
        connection.expect_echo("compat-function-end")
        wait_for_predicate("complex function geometry",
                           lambda: client.geometry(client.window)[2:] == (280, 160)
                           and client.geometry(frame) == (120, 90, 286, 190),
                           time.monotonic() + 5, process=process)
        before = client.geometry(frame)
        evidence["theme_reload_order"] = load_theme()
        connection.echo("compat-reload-applied")
        wait_for_predicate("theme reload geometry",
                           lambda: client.geometry(frame) == before,
                           time.monotonic() + 5, process=process)
        evidence["theme_and_function_geometry"] = list(before)

        pointer = Pointer(client)
        pointer.motion(10, 10)
        pointer.button(True)
        pointer.button(False)
        connection.expect_echo("compat-binding")
        client.x.XStringToKeysym.argtypes = [C.c_char_p]
        client.x.XStringToKeysym.restype = C.c_ulong
        client.x.XKeysymToKeycode.argtypes = [C.c_void_p, C.c_ulong]
        client.x.XKeysymToKeycode.restype = C.c_ubyte
        pointer.x.XTestFakeKeyEvent.argtypes = [C.c_void_p, C.c_uint,
                                                C.c_int, C.c_ulong]
        pointer.x.XTestFakeKeyEvent.restype = C.c_int
        def key(name):
            code = client.x.XKeysymToKeycode(
                client.display, client.x.XStringToKeysym(name))
            assert code != 0
            for pressed in (1, 0):
                assert pointer.x.XTestFakeKeyEvent(client.display, code, pressed, 0)
            client.x.XSync(client.display, 0)

        key(b"F8")
        connection.expect_echo("compat-binding")
        connection.send(b"Popup CompatMenu\n")
        wait_for_predicate("menu pointer grab", pointer.grabbed,
                           time.monotonic() + 5, process=process)
        key(b"Escape")
        wait_for_predicate("menu pointer release", lambda: not pointer.grabbed(),
                           time.monotonic() + 5, process=process)
        connection.echo("compat-final")
        evidence["themed_menu_popup_and_dismissal"] = "passed"
        evidence["server_generated_mouse_and_key_bindings"] = "passed"
        evidence["read_piperead_descriptors_not_inherited"] = "passed"
    finally:
        if connection is not None:
            connection.close()
        if client is not None:
            client.close()
        for log in runtime.root.glob("*.log"):
            shutil.copy2(log, artifact / log.name)
        runtime.close()
        (artifact / "evidence.json").write_text(json.dumps(evidence, indent=2) + "\n")
    print(json.dumps(evidence))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("fvwm", "mfl", "module-probe", "artifacts-dir"):
        parser.add_argument(f"--{name}", required=True, type=Path)
    parser.add_argument("--display")
    run(parser.parse_args())


if __name__ == "__main__":
    main()
