#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.

"""Check legacy decoration records and ICCCM selection-pair safety on real X."""

import argparse
import ctypes as C
import json
from pathlib import Path
import shutil
import time

from geometry_transition_live import Client
from mfl_stream_live import Connection
from live_x_harness import HarnessRuntime, start_display_server, wait_for_predicate


class SelectionNotify(C.Structure):
    _fields_ = [("type", C.c_int), ("serial", C.c_ulong),
                ("send_event", C.c_int), ("display", C.c_void_p)] + [
        (name, C.c_ulong) for name in
        ("requestor", "selection", "target", "property", "time")]


class ClientMessage(C.Structure):
    _fields_ = [("type", C.c_int), ("serial", C.c_ulong),
                ("send_event", C.c_int), ("display", C.c_void_p),
                ("window", C.c_ulong), ("message_type", C.c_ulong),
                ("format", C.c_int), ("data", C.c_long * 5)]


class Event(C.Union):
    _fields_ = [("selection", SelectionNotify), ("message", ClientMessage),
                ("pad", C.c_long * 24)]


def run(options):
    runtime = HarnessRuntime(prefix="fvwm-legacy-property-")
    client = connection = None
    adoption_clients = []
    evidence = {}
    artifact = options.artifacts_dir.resolve()
    artifact.mkdir(parents=True, exist_ok=True)
    try:
        display = options.display
        if display is None:
            _, display = start_display_server(
                runtime, "Xvfb", ["Xvfb", "-screen", "0", "800x600x24", "-noreset"],
                "xserver.log", time.monotonic() + 10)
        # Existing mapped clients exercise the startup adoption path, where a
        # format-8 WM_STATE previously reached a native-long dereference.
        for fmt, values in ((8, [1]), (32, []), (32, [1] * 4),
                            (32, [1]), (32, [1, 0]), (32, [1, 0, 0])):
            existing = Client(display)
            adoption_clients.append(existing)
            existing.window = existing.x.XCreateSimpleWindow(
                existing.display, existing.root, 20, 20, 100, 60, 0, 0, 0)
            existing.x.XInternAtom.argtypes = [C.c_void_p, C.c_char_p, C.c_int]
            existing.x.XInternAtom.restype = C.c_ulong
            state = existing.x.XInternAtom(existing.display, b"WM_STATE", 0)
            existing.x.XChangeProperty.argtypes = [C.c_void_p, C.c_ulong, C.c_ulong,
                                                  C.c_ulong, C.c_int, C.c_int,
                                                  C.c_void_p, C.c_int]
            data = ((C.c_ulong * len(values))(*values) if fmt == 32 else
                    C.create_string_buffer(bytes(values)))
            existing.x.XChangeProperty(existing.display, existing.window, state,
                                        state, fmt, 0, data, len(values))
            existing.x.XMapWindow(existing.display, existing.window)
            existing.x.XSync(existing.display, 0)
        root_state = adoption_clients[0].x.XInternAtom(
            adoption_clients[0].display, b"WM_DESKTOP", 0)
        adoption_clients[0].x.XChangeProperty(
            adoption_clients[0].display, adoption_clients[0].root, root_state,
            root_state, 8, 0, C.create_string_buffer(b"x"), 1)
        adoption_clients[0].x.XSync(adoption_clients[0].display, 0)
        log = runtime.root / "core.log"
        process = runtime.start(
            "fvwm", [str(options.fvwm), "-d", display, "-v", "-o", str(log),
                     "-c", f"ModulePath {options.mfl.parent}",
                     "-c", "AddToFunc StartFunction I Module FvwmMFL",
                     "-c", "AddToFunc StartFunction I Echo adoption-start",
                     "-c", "AddToFunc RestartFunction I Echo unexpected-restart",
                     "-c", "TitleStyle Height 24",
                     "-c", "Style * BorderWidth 3, HandleWidth 3, MwmDecor, OLDecor, TileCascadePlacement"],
            "fvwm.log", runtime.environment({"FVWMMFL_SOCKET_PATH": str(runtime.root)}))
        endpoint = runtime.root / f"fvwm_mfl_{display.replace(':', '_')}.sock"
        wait_for_predicate("MFL endpoint", endpoint.exists,
                           time.monotonic() + 10, process=process)
        connection = Connection(endpoint)
        connection.record()
        connection.send(b"set echo\n")
        connection.echo("legacy-property-ready")
        for existing in adoption_clients:
            wait_for_predicate("existing client safely adopted", existing.frame,
                               time.monotonic() + 5, process=process)
        assert log.read_text().count("Ignoring invalid WM_STATE") >= 3
        assert "adoption-start" in log.read_text()
        assert "unexpected-restart" not in log.read_text()
        for existing in adoption_clients:
            existing.close()
        adoption_clients.clear()
        evidence["startup_scalar_validation_and_legacy_state_lengths"] = "passed"
        client = Client(display)
        x = client.x
        x.XInternAtom.argtypes = [C.c_void_p, C.c_char_p, C.c_int]
        x.XInternAtom.restype = C.c_ulong
        x.XChangeProperty.argtypes = [C.c_void_p, C.c_ulong, C.c_ulong,
                                     C.c_ulong, C.c_int, C.c_int, C.c_void_p, C.c_int]
        atom = lambda name: x.XInternAtom(client.display, name.encode(), 0)

        def change(name, kind, fmt, values):
            data = ((C.c_ulong * len(values))(*values) if fmt == 32 else
                    C.create_string_buffer(bytes(values)))
            x.XChangeProperty(client.display, client.window, atom(name), atom(kind),
                              fmt, 0, data, len(values))
            x.XSync(client.display, 0)

        def decorated(name, kind, fmt, values, height, rejected=False):
            client.destroy()
            before = log.read_text().count("Ignoring invalid")
            client.window = x.XCreateSimpleWindow(
                client.display, client.root, 100, 100, 200, 120, 0, 0, 0)
            change(name, kind, fmt, values)
            x.XMapWindow(client.display, client.window)
            x.XSync(client.display, 0)
            wait_for_predicate("legacy decoration geometry",
                               lambda: client.frame() is not None and
                               client.geometry(client.frame())[3] == height,
                               time.monotonic() + 5, process=process)
            if rejected:
                assert log.read_text().count("Ignoring invalid") > before
            connection.echo("healthy-after-decoration")

        decorated("_MOTIF_WM_HINTS", "_MOTIF_WM_HINTS", 8, [2, 0, 0, 0], 150, True)
        decorated("_MOTIF_WM_HINTS", "_MOTIF_WM_HINTS", 32, [2], 150, True)
        decorated("_MOTIF_WM_HINTS", "CARDINAL", 32, [2, 0, 0, 0], 150, True)
        decorated("_OL_WIN_ATTR", "_OL_WIN_ATTR", 32, [0], 150, True)
        decorated("_OL_WIN_ATTR", "_OL_WIN_ATTR", 32, [0, 0, 0, 0], 150, True)
        decorated("_OL_DECOR_ADD", "ATOM", 8, [1, 2, 3, 4], 150, True)
        decorated("_OL_DECOR_DEL", "ATOM", 32, [0] * 33, 150, True)
        decorated("_MOTIF_WM_HINTS", "_MOTIF_WM_HINTS", 32, [2, 0, 0, 0], 120)
        decorated("_MOTIF_WM_HINTS", "_MOTIF_WM_HINTS", 32, [2, 0, 1, 0, 0], 150)
        decorated("_OL_WIN_ATTR", "_OL_WIN_ATTR", 32, [atom("_OL_WT_BASE"), 0, 0], 150)
        decorated("_OL_WIN_ATTR", "_OL_WIN_ATTR", 32,
                  [0, atom("_OL_WT_BASE"), 0, 0, 0], 150)
        evidence["malformed_decorations_rejected_valid_legacy_records_preserved"] = "passed"

        x.XConvertSelection.argtypes = [C.c_void_p] + [C.c_ulong] * 5
        x.XCheckTypedWindowEvent.argtypes = [C.c_void_p, C.c_ulong,
                                            C.c_int, C.POINTER(Event)]
        x.XGetWindowProperty.argtypes = [C.c_void_p, C.c_ulong, C.c_ulong,
                                         C.c_long, C.c_long, C.c_int, C.c_ulong,
                                         C.POINTER(C.c_ulong), C.POINTER(C.c_int),
                                         C.POINTER(C.c_ulong), C.POINTER(C.c_ulong),
                                         C.POINTER(C.c_void_p)]

        # Bad protocols must not prevent management or contaminate other clients.
        for kind, fmt, values in (("ATOM", 8, [1, 2, 3, 4]),
                                  ("CARDINAL", 32, [atom("WM_DELETE_WINDOW")]),
                                  ("ATOM", 32, [0] * 4097)):
            client.destroy()
            before = log.read_text().count("Ignoring invalid WM_PROTOCOLS")
            client.window = x.XCreateSimpleWindow(
                client.display, client.root, 100, 100, 200, 120, 0, 0, 0)
            change("WM_PROTOCOLS", kind, fmt, values)
            x.XMapWindow(client.display, client.window)
            x.XSync(client.display, 0)
            wait_for_predicate("rejected protocol with healthy management",
                               lambda: client.frame() is not None and
                               log.read_text().count("Ignoring invalid WM_PROTOCOLS") > before,
                               time.monotonic() + 5, process=process)
            connection.echo("healthy-after-invalid-protocol")
        # The ATOM contract and historical Corel property type both retain Close.
        for kind in ("ATOM", "WM_PROTOCOLS"):
            client.destroy()
            client.window = x.XCreateSimpleWindow(
                client.display, client.root, 100, 100, 200, 120, 0, 0, 0)
            change("WM_PROTOCOLS", kind, 32, [atom("WM_DELETE_WINDOW")])
            x.XMapWindow(client.display, client.window)
            x.XSync(client.display, 0)
            wait_for_predicate("protocol client management", client.frame,
                               time.monotonic() + 5, process=process)
            connection.send(f"WindowId {client.window} Close\n".encode())
            event = Event()
            wait_for_predicate("WM_DELETE_WINDOW client message",
                               lambda: x.XCheckTypedWindowEvent(
                                   client.display, client.window, 33, C.byref(event)) != 0,
                               time.monotonic() + 5, process=process)
            assert event.message.message_type == atom("WM_PROTOCOLS")
            assert event.message.format == 32
            assert event.message.data[0] == atom("WM_DELETE_WINDOW")
            assert client.frame() is not None
        evidence["bounded_protocols_preserve_standard_and_corel_close"] = "passed"
        for name in ("WM_NAME", "WM_ICON_NAME"):
            for fmt, values in ((8, b"x" * 65537), (32, [1])):
                before = log.read_text().count("Ignoring invalid text property")
                change(name, "STRING", fmt, values)
                wait_for_predicate("invalid legacy name rejected",
                                   lambda: log.read_text().count(
                                       "Ignoring invalid text property") > before,
                                   time.monotonic() + 5, process=process)
                connection.echo("healthy-after-legacy-name")
            change(name, "STRING", 8, b"valid legacy name")
            connection.echo("valid-legacy-name-processed")
        evidence["legacy_name_limits_and_recovery"] = "passed"

        def read_values(name, kind):
            actual, count, remaining = C.c_ulong(), C.c_ulong(), C.c_ulong()
            fmt, data = C.c_int(), C.c_void_p()
            assert x.XGetWindowProperty(
                client.display, client.window, atom(name), 0, 256, 0, atom(kind),
                C.byref(actual), C.byref(fmt), C.byref(count), C.byref(remaining),
                C.byref(data)) == 0
            try:
                assert actual.value == atom(kind) and fmt.value == 32
                assert remaining.value == 0
                return list((C.c_ulong * count.value).from_address(data.value))
            finally:
                x.XFree(data)

        def selection(kind, fmt, values, expected_property):
            change("FVWM_TEST_PAIRS", kind, fmt, values)
            event = Event()
            x.XConvertSelection(client.display, atom("WM_S0"), atom("MULTIPLE"),
                                atom("FVWM_TEST_PAIRS"), client.window, 0)
            x.XSync(client.display, 0)
            wait_for_predicate("selection reply",
                               lambda: x.XCheckTypedWindowEvent(
                                   client.display, client.window, 31, C.byref(event)) != 0,
                               time.monotonic() + 5, process=process)
            assert event.selection.property == expected_property, event.selection.property
            connection.echo("healthy-after-selection")

        for kind, fmt, values in (("ATOM_PAIR", 32, [atom("TARGETS")]),
                                  ("ATOM_PAIR", 8, [1] * 8),
                                  ("ATOM", 32, [atom("TARGETS"), atom("FVWM_RESULT")]),
                                  ("ATOM_PAIR", 32, [0] * 258)):
            before = log.read_text().count("Ignoring invalid MULTIPLE pairs")
            selection(kind, fmt, values, 0)
            assert log.read_text().count("Ignoring invalid MULTIPLE pairs") > before
        selection("ATOM_PAIR", 32,
                  [atom("TARGETS"), atom("FVWM_RESULT"),
                   atom("UNSUPPORTED_TARGET"), atom("FVWM_BAD_RESULT"),
                   atom("TIMESTAMP"), 0], atom("FVWM_TEST_PAIRS"))
        assert read_values("FVWM_TEST_PAIRS", "ATOM_PAIR") == [
            atom("TARGETS"), atom("FVWM_RESULT"),
            atom("UNSUPPORTED_TARGET"), 0, atom("TIMESTAMP"), 0]
        assert read_values("FVWM_RESULT", "ATOM") == [
            atom("TARGETS"), atom("MULTIPLE"), atom("TIMESTAMP"), atom("VERSION")]
        selection("ATOM_PAIR", 32, [], atom("FVWM_TEST_PAIRS"))
        evidence["invalid_multiple_rejected_valid_multiple_acknowledged"] = "passed"
    finally:
        for existing in adoption_clients:
            existing.close()
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
    for name in ("fvwm", "mfl", "artifacts-dir"):
        parser.add_argument(f"--{name}", required=True, type=Path)
    parser.add_argument("--display")
    run(parser.parse_args())


if __name__ == "__main__":
    main()
