#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
"""Observe real icon and tear-off redraw pixels across command barriers."""
import argparse
import ctypes as C
import json
from pathlib import Path
import shutil
import time

from colorset_live import WindowAttributes
from geometry_transition_live import Client, Pointer
from live_x_harness import (
    HarnessRuntime, start_display_server, wait_for_predicate,
)
from mfl_stream_live import Connection


def run(args):
    out = args.artifacts_dir.resolve()
    out.mkdir(parents=True, exist_ok=True)
    evidence = {}
    with HarnessRuntime(prefix="fvwm-expose-redraw-") as runtime:
        client = connection = core = None
        try:
            display = args.display
            if display is None:
                _, display = start_display_server(runtime, "Xvfb",
                    ["Xvfb", "-screen", "0", "1000x800x24", "-noreset"],
                    "xserver.log", time.monotonic() + 10)
            config = runtime.root / "config"
            debug = runtime.root / "debug.log"
            config.write_text(f"ModulePath {args.mfl.parent}\nDefaultFont fixed\n"
                "Colorset 0 fg white, bg black\n"
                "Style * Colorset 0, HilightColorset 0, IconTitle, "
                "IconBox 10 400 600 600, TileCascadePlacement\n"
                "MenuStyle * Font fixed, MenuColorset 0\n"
                "Key BackSpace M A MenuTearOff\n"
                "AddToMenu RedrawMenu \"RedrawMenu\" Title\n"
                "+ \"Visible menu text\" Nop\n"
                "AddToFunc StartFunction I Module FvwmMFL\n")
            core = runtime.start("core", [str(args.fvwm), "-d", display,
                "-f", str(config), "-v", "-o", str(debug)], "core.log",
                runtime.environment({"FVWMMFL_SOCKET_PATH": str(runtime.root)}))
            socket_name = f"fvwm_mfl_{display.replace(':', '_')}.sock"
            endpoint = runtime.root / socket_name
            wait_for_predicate("redraw endpoint", endpoint.exists,
                time.monotonic() + 10, process=core)
            connection = Connection(endpoint)
            connection.record()
            connection.send(b"set echo\n")
            sequence = 0

            def command(text):
                nonlocal sequence
                sequence += 1
                connection.send((text + "\n").encode())
                connection.echo(f"redraw-barrier-{sequence}")

            client = Client(display)
            client.create()
            x, d = client.x, client.display
            pointer = Pointer(client)
            x.XStoreName(d, client.window, b"Visible icon text")
            wait_for_predicate("managed redraw client", client.frame,
                time.monotonic() + 5, process=core)
            signatures = {
                "XInternAtom": ([C.c_void_p, C.c_char_p, C.c_int], C.c_ulong),
                "XGetWindowProperty": ([C.c_void_p, C.c_ulong, C.c_ulong,
                    C.c_long, C.c_long, C.c_int, C.c_ulong,
                    C.POINTER(C.c_ulong), C.POINTER(C.c_int),
                    C.POINTER(C.c_ulong), C.POINTER(C.c_ulong),
                    C.POINTER(C.c_void_p)], C.c_int),
                "XGetImage": ([C.c_void_p, C.c_ulong, C.c_int, C.c_int,
                    C.c_uint, C.c_uint, C.c_ulong, C.c_int], C.c_void_p),
                "XGetPixel": ([C.c_void_p, C.c_int, C.c_int], C.c_ulong),
                "XDestroyImage": ([C.c_void_p], C.c_int),
                "XClearArea": ([C.c_void_p, C.c_ulong, C.c_int, C.c_int,
                    C.c_uint, C.c_uint, C.c_int], C.c_int),
                "XFetchName": ([C.c_void_p, C.c_ulong,
                    C.POINTER(C.c_void_p)], C.c_int),
                "XGetWindowAttributes": ([C.c_void_p, C.c_ulong,
                    C.POINTER(WindowAttributes)], C.c_int),
                "XKeysymToKeycode": ([C.c_void_p, C.c_ulong], C.c_uint),
            }
            for name, (arguments, result) in signatures.items():
                getattr(x, name).argtypes = arguments
                getattr(x, name).restype = result
            pointer.x.XTestFakeKeyEvent.argtypes = [C.c_void_p, C.c_uint,
                C.c_int, C.c_ulong]
            wm_state = x.XInternAtom(d, b"WM_STATE", 0)

            def icon_window():
                kind, fmt, count, after, data = (C.c_ulong(), C.c_int(),
                    C.c_ulong(), C.c_ulong(), C.c_void_p())
                assert x.XGetWindowProperty(d, client.window, wm_state, 0, 2,
                    0, wm_state, C.byref(kind), C.byref(fmt), C.byref(count),
                    C.byref(after), C.byref(data)) == 0
                try:
                    if fmt.value == 32 and count.value == 2:
                        values = C.cast(data, C.POINTER(C.c_ulong))
                        if values[0] == 3:
                            evidence["first-icon-WM_STATE"] = list(values[:2])
                            if values[1]:
                                return values[1]

                finally:
                    if data.value:
                        x.XFree(data)

            def pixels(window):
                width, height = client.geometry(window)[2:]
                image = x.XGetImage(d, window, 0, 0, width, height,
                    C.c_ulong(-1).value, 2)
                assert image
                try:
                    return (width, height, tuple(x.XGetPixel(image, px, py)
                        for py in range(height) for px in range(width)))
                finally:
                    x.XDestroyImage(image)

            def children(window):
                root, parent, values, count = (C.c_ulong(), C.c_ulong(),
                    C.POINTER(C.c_ulong)(), C.c_uint())
                assert x.XQueryTree(d, window, C.byref(root), C.byref(parent),
                    C.byref(values), C.byref(count))
                try:
                    return list(values[:count.value])
                finally:
                    if values:
                        x.XFree(values)

            command(f"WindowId {client.window} Iconify on")
            icon = wait_for_predicate("mapped icon title", icon_window,
                time.monotonic() + 5, process=core)
            assert evidence["first-icon-WM_STATE"] == [3, icon]
            initial = wait_for_predicate("painted icon text",
                lambda: pixels(icon) if len(set(pixels(icon)[2])) > 1 else None,
                time.monotonic() + 5, process=core)
            read_file = runtime.root / "redraw-read.conf"
            read_file.write_text("Echo redraw-read-completed\n")
            for label, text in (("Read", f"Read {read_file}"),
                    ("Raise", f"WindowId {client.window} Raise"),
                    ("Refresh", "Refresh")):
                for _ in range(80):
                    x.XClearArea(d, icon, 0, 0, 0, 0, 1)
                x.XSync(d, 0)
                command(text)
                wait_for_predicate(label + " restored icon pixels",
                    lambda: pixels(icon) == initial,
                    time.monotonic() + 5, process=core)
                evidence[label + "-icon-redraw-pixels"] = "passed"
            assert "redraw-read-completed" in debug.read_text()
            command(f"WindowId {client.window} Iconify off")
            wait_for_predicate("deiconified frame", client.frame,
                time.monotonic() + 5, process=core)
            command(f"WindowId {client.window} Iconify on")
            assert wait_for_predicate("reused icon title", icon_window,
                time.monotonic() + 5, process=core) == icon
            assert evidence["first-icon-WM_STATE"] == [3, icon]
            evidence["first-and-reused-icon-WM_STATE-XID"] = "passed"

            connection.send(b"Popup RedrawMenu\n")

            def popup():
                observed = []
                for window in children(client.root):
                    attrs = WindowAttributes()
                    assert x.XGetWindowAttributes(d, window, C.byref(attrs))
                    observed.append([window, attrs.map_state,
                        attrs.override_redirect, attrs.width, attrs.height])
                    if attrs.save_under and attrs.map_state == 2:
                        if attrs.width > 40 and attrs.height > 20:
                            return window
                evidence["popup-root-snapshot"] = observed
                return None

            wait_for_predicate("redraw popup", popup,
                time.monotonic() + 5, process=core)
            key = x.XKeysymToKeycode(d, 0xff08)
            pointer.x.XTestFakeKeyEvent(d, key, 1, 0)
            pointer.x.XTestFakeKeyEvent(d, key, 0, 0)
            pointer.motion(5, client.geometry(client.root)[3] - 10)
            x.XSync(d, 0)
            connection.echo("redraw-tearoff-ready")

            def tearoff_window():
                pending = [(client.root, 0)]
                while pending:
                    window, depth = pending.pop()
                    name = C.c_void_p()
                    if x.XFetchName(d, window, C.byref(name)) and name.value:
                        try:
                            if C.string_at(name) == b"RedrawMenu":
                                attrs = WindowAttributes()
                                x.XGetWindowAttributes(
                                    d, window, C.byref(attrs))
                                if attrs.map_state == 2:
                                    return window
                        finally:
                            x.XFree(name)
                    if depth < 3:
                        pending.extend((w, depth + 1) for w in children(window))
                return None

            tearoff = wait_for_predicate("managed tearoff", tearoff_window,
                time.monotonic() + 5, process=core)
            command("All (RedrawMenu) Move 100p 100p")
            initial = pixels(tearoff)
            assert len(set(initial[2])) > 1
            for _ in range(80):
                x.XClearArea(d, tearoff, 0, 0, 0, 0, 1)
            x.XSync(d, 0)
            command("Refresh")
            wait_for_predicate("tearoff restored pixels",
                lambda: pixels(tearoff) == initial,
                time.monotonic() + 5, process=core)
            command("All (RedrawMenu) Close")
            evidence["tearoff-expose-refresh-pixels-and-close"] = "passed"
            assert core.poll() is None
            assert "X error" not in debug.read_text()
        except BaseException as error:
            evidence["error"] = str(error)
            raise
        finally:
            if client:
                client.close()
            if connection:
                connection.close()
            for log in runtime.root.glob("*.log"):
                shutil.copy2(log, out / log.name)
            (out / "evidence.json").write_text(
                json.dumps(evidence, indent=2) + "\n")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    for name in ("fvwm", "mfl", "artifacts-dir"):
        parser.add_argument("--" + name, type=Path, required=True)
    parser.add_argument("--display")
    run(parser.parse_args())
