#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
"""Exercise native text editing, selections, and list viewport arithmetic."""
import argparse
import ctypes as C
import json
import shutil
from pathlib import Path
import time

from geometry_transition_live import Client
from live_x_harness import HarnessRuntime, start_display_server, wait_for_predicate
from mfl_stream_live import Connection
from selection_live import Button, Event, Key, SelectionNotify


class XError(C.Structure):
    _fields_ = [("type", C.c_int), ("display", C.c_void_p),
        ("resourceid", C.c_ulong), ("serial", C.c_ulong),
        ("error_code", C.c_ubyte), ("request_code", C.c_ubyte),
        ("minor_code", C.c_ubyte)]


def run(options):
    results = {}
    options.artifacts_dir.mkdir(parents=True, exist_ok=True)
    with HarnessRuntime(prefix="fvwm-text-list-") as runtime:
        connection = client = None
        try:
            display = options.display
            if display is None:
                _, display = start_display_server(runtime, "Xvfb",
                    ["Xvfb", "-screen", "0", "800x600x24", "-noreset"],
                    "xserver.log", time.monotonic() + 10)
            config = runtime.root / "config"
            config.write_text(f"ModulePath {options.script.parent}:{options.mfl.parent}\n"
                "Style * TileCascadePlacement\n"
                '*FvwmScript: KeyboardFocus "TextListFocus"\n'
                "AddToFunc StartFunction I Module FvwmMFL\n")
            core = runtime.start("core", [str(options.fvwm), "-d", display,
                "-f", str(config)], "core.log", runtime.environment({
                    "FVWMMFL_SOCKET_PATH": str(runtime.root)}))
            endpoint = runtime.root / f"fvwm_mfl_{display.replace(':', '_')}.sock"
            wait_for_predicate("text/list endpoint", endpoint.exists,
                time.monotonic() + 10, process=core)
            connection = Connection(endpoint)
            connection.record()
            connection.send(b"set echo\n")
            connection.echo("text-list-ready")
            client = Client(display)
            x, d = client.x, client.display
            signatures = {
                "XFetchName": ([C.c_void_p, C.c_ulong, C.POINTER(C.c_void_p)], C.c_int),
                "XKeysymToKeycode": ([C.c_void_p, C.c_ulong], C.c_uint),
                "XSendEvent": ([C.c_void_p, C.c_ulong, C.c_int, C.c_long, C.POINTER(Event)], C.c_int),
                "XWarpPointer": ([C.c_void_p, C.c_ulong, C.c_ulong, C.c_int, C.c_int,
                    C.c_uint, C.c_uint, C.c_int, C.c_int], C.c_int),
                "XInternAtom": ([C.c_void_p, C.c_char_p, C.c_int], C.c_ulong),
                "XSetSelectionOwner": ([C.c_void_p, C.c_ulong, C.c_ulong, C.c_ulong], C.c_int),
                "XCheckTypedEvent": ([C.c_void_p, C.c_int, C.POINTER(Event)], C.c_int),
                "XChangeProperty": ([C.c_void_p, C.c_ulong, C.c_ulong, C.c_ulong,
                    C.c_int, C.c_int, C.c_void_p, C.c_int], C.c_int),
                "XFetchBuffer": ([C.c_void_p, C.POINTER(C.c_int), C.c_int], C.c_void_p),
                "XGetImage": ([C.c_void_p, C.c_ulong, C.c_int, C.c_int,
                    C.c_uint, C.c_uint, C.c_ulong, C.c_int], C.c_void_p),
                "XGetPixel": ([C.c_void_p, C.c_int, C.c_int], C.c_ulong),
                "XDestroyImage": ([C.c_void_p], C.c_int),
                "XClearArea": ([C.c_void_p, C.c_ulong, C.c_int, C.c_int,
                    C.c_uint, C.c_uint, C.c_int], C.c_int),
            }
            for name, (args, restype) in signatures.items():
                getattr(x, name).argtypes = args
                getattr(x, name).restype = restype

            scanning = False
            unexpected_errors = []
            error_callback = C.CFUNCTYPE(C.c_int, C.c_void_p, C.POINTER(XError))

            @error_callback
            def x_error(display, pointer):
                error = pointer.contents
                # A child can disappear between tree enumeration and lookup.
                if not (scanning and error.error_code == 3 and
                        error.request_code in (15, 20)):
                    unexpected_errors.append((error.error_code,
                        error.request_code, error.minor_code, error.resourceid))
                return 0

            x.XSetErrorHandler.argtypes = [error_callback]
            x.XSetErrorHandler.restype = C.c_void_p
            previous_error_handler = x.XSetErrorHandler(x_error)

            def children(window):
                root, parent, items, count = C.c_ulong(), C.c_ulong(), C.POINTER(C.c_ulong)(), C.c_uint()
                valid = x.XQueryTree(d, window, C.byref(root), C.byref(parent),
                    C.byref(items), C.byref(count))
                assert valid or scanning, window
                found = [items[i] for i in range(min(count.value, 128))]
                if items:
                    x.XFree(items)
                return found

            def named(title):
                nonlocal scanning
                scanning = True
                try:
                    queue = [(client.root, 0)]
                    while queue:
                        window, depth = queue.pop()
                        value = C.c_void_p()
                        if x.XFetchName(d, window, C.byref(value)) and value:
                            name = C.string_at(value).decode("utf-8", "replace")
                            x.XFree(value)
                            if name == title:
                                return window
                        if depth < 5:
                            queue.extend((w, depth + 1) for w in children(window))
                    return None
                finally:
                    scanning = False

            def key(parent, field, keysym, state=0):
                event = Event()
                event.key = Key(type=2, display=d, window=parent, subwindow=field,
                    root=client.root, keycode=x.XKeysymToKeycode(d, keysym),
                    state=state, same_screen=1)
                assert x.XSendEvent(d, parent, 0, 1, C.byref(event))
                x.XSync(d, 0)

            def button(parent, field, number, local_x=5, local_y=5, event_type=4):
                px, py, _, _ = client.geometry(field)
                event = Event()
                event.button = Button(type=event_type, display=d, window=parent,
                    subwindow=field, root=client.root, x=px+local_x, y=py+local_y,
                    button=number, same_screen=1)
                mask = {4: 1 << 2, 5: 1 << 3, 6: 1 << 13}[event_type]
                assert x.XSendEvent(d, parent, 0, mask, C.byref(event))
                x.XSync(d, 0)

            def warp(field, xx, yy=5):
                x.XWarpPointer(d, 0, field, 0, 0, 0, 0, xx, yy)
                x.XSync(d, 0)

            def cursor_pixels(field):
                _, _, width, height = client.geometry(field)
                image = x.XGetImage(d, field, 0, 0, width, height, C.c_ulong(-1), 2)
                assert image
                try:
                    return tuple(x.XGetPixel(image, xx, yy)
                        for xx in range(3, 9) for yy in range(max(0, height-10), height-3))
                finally:
                    x.XDestroyImage(image)

            options_title = "|".join(str(i) for i in range(1, 13))
            for focused, limit in ((False, None), (True, None),
                    (False, 128), (True, 128), (False, 256), (True, 256)):
                title = "TextListFocus" if focused else "TextListLegacy"
                if limit is not None:
                    title += str(limit)
                connection.send(f"*FvwmScript: ValueLimit {limit or 1048576}\n".encode())
                connection.send(f'*FvwmScript: KeyboardFocus "{title if focused else "TextListFocus"}"\n'.encode())
                report = runtime.root / (title + "-report")
                script = runtime.root / title
                state_line = (" WriteToFile $file {state:} (GetTitle 1) {|} "
                    "(GetValue 1) {|} (GetMinValue 1) {|} (GetMaxValue 1) {|} "
                    "(GetValue 2) {|} (GetMinValue 2)\n")
                cases = {
                    1: state_line,
                    2: " ChangeTitle 1 {seed}\n ChangeValue 1 4\n ChangeMinValue 1 4\n" + state_line,
                    3: " Set $large={2147483647}\n Set $negative={-2147483648}\n"
                       " ChangeMinValue 1 $large\n ChangeMaxValue 1 $large\n"
                       " ChangeValue 1 $negative\n" + state_line,
                    4: " Set $large={2147483647}\n ChangeMinValue 2 $large\n" + state_line,
                    5: " ChangeTitle 2 {}\n ChangeSize 2 1 1\n" + state_line,
                    6: " ChangeSize 2 160 90\n ChangeTitle 2 {" + options_title + "}\n" + state_line,
                    7: " ChangeSize 2 26 90\n" + state_line,
                }
                full_report = runtime.root / (title + "-full")
                limit_report = runtime.root / (title + "-limit")
                ack_report = runtime.root / (title + "-x-ack")
                # Script checks bindings before widget routing. F12 acknowledges
                # prior events without changing focus, text, or callback counts.
                cases[11] = " WriteToFile $ack {processed}\n"
                cases[12] = " Set $noop={0}\n"
                if limit is not None:
                    cases[8] = (" ChangeTitle 1 {" + "x"*(limit-1) + "}\n"
                        f" ChangeMinValue 1 {limit-1}\n ChangeValue 1 {limit-1}\n")
                    cases[9] = (" WriteToFile $full (GetTitle 1)\n"
                        " WriteToFile $lim {limit:} (GetValue 1) {|} "
                        "(GetMinValue 1) {|} (GetMaxValue 1) {|} $events {|} $seen\n")
                    cases[10] = " ChangeMinValue 1 4\n ChangeValue 1 0\n"
                case_text = "".join(f"{n} :\nBegin\n{body}End\n" for n, body in cases.items())
                case_text += (" SingleClic :\nBegin\n Set $events=(Add $events 1)\n"
                    " Set $seen=(GetValue 1)\nEnd\n")
                script.write_text(f"WindowTitle {{{title}}}\nWindowSize 240 180\nFont {{fixed}}\n"
                    "Init\nBegin\n Set $file={" + str(report) + "}\n"
                    " Set $events={0}\n Set $seen={0}\n Set $full={" + str(full_report) + "}\n"
                    " Set $lim={" + str(limit_report) + "}\n"
                    " Set $ack={" + str(ack_report) + "}\n"
                    " Key F12 N 1 11 {}\n Key F11 N 1 12 {}\n"
                    " WriteToFile $file {ready}\nEnd\n"
                    "Widget 1\nProperty\n Type TextField\n Position 10 10\n"
                    " Size 200 24\n Title {seed}\n Value 4\nMain\nCase message of\n"
                    + case_text + "End\nWidget 2\nProperty\n Type List\n Position 10 60\n"
                    " Size 160 90\n Value 0\n Title {" + options_title + "}\nEnd\n")
                connection.send(("Module FvwmScript " + str(script) + "\n").encode())
                wait_for_predicate(title, lambda: named(title) and report.exists(),
                    time.monotonic()+10, process=core)
                parent = named(title)
                wait_for_predicate('both text/list widget children',
                    lambda: len(children(parent)) == 2,
                    time.monotonic()+15, process=core)
                field, listing = sorted(children(parent), key=lambda w: client.geometry(w)[1])
                row_height = client.geometry(field)[3] - 10 + 3
                visible = (client.geometry(listing)[3] - 8) // row_height
                last_first = 12 - visible + 1

                def x_processed():
                    before = (ack_report.read_bytes().splitlines().count(b"processed")
                        if ack_report.exists() else 0)
                    for _ in range(options.input_backlog):
                        key(parent, field, 0xffc8)  # F11: bound no-op.
                    key(parent, field, 0xffc9)  # F12: application acknowledgement.
                    wait_for_predicate("Script processed prior X input", lambda:
                        ack_report.exists() and ack_report.read_bytes().splitlines()
                        .count(b"processed") == before + 1,
                        time.monotonic()+5, process=core)

                def state(case=1):
                    # XSync only waits for the server, not this module. Queries
                    # travel over a separate MFL/module pipe and can overtake X
                    # input. Wait for a same-X-stream application acknowledgement.
                    x_processed()
                    before = sum(line.startswith(b"state:") for line in report.read_bytes().splitlines())
                    connection.send(f"SendToModule FvwmScript SendString 1 {case}\n".encode())
                    wait_for_predicate("text/list state", lambda: sum(line.startswith(b"state:")
                        for line in report.read_bytes().splitlines()) > before,
                        time.monotonic()+5, process=core)
                    line = [line for line in report.read_bytes().splitlines()
                        if line.startswith(b"state:")][-1]
                    assert line.startswith(b"state:"), line
                    parts = line[6:].split(b"|")
                    return parts[0], *map(int, parts[1:])

                assert state()[:3] == (b"seed", 4, 4)
                if not focused:
                    for keysym, expected, position in [(ord('z'), b"seedz", 5),
                        (0xff51, b"seedz", 4), (0xff08, b"seez", 3),
                        (0xffff, b"sez", 2), (0xff53, b"sez", 3)]:
                        key(parent, field, keysym)
                        assert state()[:3] == (expected, position, position)
                    results["legacy-insert-arrows-backspace-and-delete"] = "passed"
                    before_pixels = cursor_pixels(field)
                    warp(field, 5)
                    button(parent, field, 1)
                    wait_for_predicate("selection anchor draw",
                        lambda: cursor_pixels(field) != before_pixels,
                        time.monotonic()+5, process=core)
                    warp(field, 20)
                    button(parent, field, 0, event_type=6)
                    button(parent, field, 1, event_type=5)
                    assert state()[:3] == (b"sez", 0, 2)
                    length = C.c_int()
                    cut = x.XFetchBuffer(d, C.byref(length), 0)
                    assert cut
                    try:
                        assert C.string_at(cut, length.value) == b"se"
                    finally:
                        x.XFree(cut)
                    key(parent, field, ord('x'))
                    assert state()[:3] == (b"xsez", 1, 1)
                    button(parent, field, 2)
                    assert state()[:3] == (b"xsesez", 3, 3)
                    results["legacy-mouse-selection-cut-buffer-and-middle-paste"] = "passed"
                    assert state(4)[-1] == last_first
                    key(parent, listing, 0xff55)
                    assert state()[-1] == last_first - visible
                    key(parent, listing, 0xff56)
                    assert state()[-1] == last_first
                    warp(listing, 20, 6 + row_height + 1)
                    button(parent, listing, 1, 20, 6 + row_height + 1)
                    assert state()[-2] == last_first + 1
                    results["list-viewport-clamping-page-navigation-and-row-selection"] = "passed"
                else:
                    key(parent, field, ord('a'), 4)
                    assert state()[:3] == (b"seed", 0, 4)
                    key(parent, field, ord('x'))
                    assert state()[:3] == (b"x", 1, 1)
                    key(parent, field, ord('a'))
                    key(parent, field, 0xff50)
                    key(parent, field, 0xffff)
                    assert state()[:3] == (b"a", 0, 0)
                    key(parent, field, 0xff57)
                    key(parent, field, 0xff08)
                    assert state()[:3] == (b"", 0, 0)
                    results["opt-in-selection-replacement-home-end-and-forward-delete"] = "passed"
                    client.create()
                    clipboard = x.XInternAtom(d, b"CLIPBOARD", 0)
                    x.XSetSelectionOwner(d, clipboard, client.window, 0)
                    x.XSync(d, 0)
                    key(parent, field, ord('v'), 4)
                    event = Event()
                    wait_for_predicate("clipboard request", lambda: x.XCheckTypedEvent(d, 30, C.byref(event)),
                        time.monotonic()+5, process=core)
                    request = event.request
                    payload = C.create_string_buffer(b"a\xe2")
                    x.XChangeProperty(d, request.requestor, request.property, request.target,
                        8, 0, payload, 2)
                    reply = Event()
                    reply.selection = SelectionNotify(type=31, display=d,
                        requestor=request.requestor, selection=request.selection,
                        target=request.target, property=request.property, time=request.time)
                    x.XSendEvent(d, request.requestor, 0, 0, C.byref(reply))
                    x.XSync(d, 0)
                    assert state()[:3] == (b"a\xe2", 2, 2)
                    key(parent, field, 0xff08)
                    assert state()[:3] == (b"a", 1, 1)
                    results["truncated-multibyte-paste-and-bounded-backspace"] = "passed"
                if limit is not None:
                    def limit_state():
                        x_processed()
                        before = (sum(line.startswith(b"limit:") for line in
                            limit_report.read_bytes().splitlines()) if limit_report.exists() else 0)
                        connection.send(b"SendToModule FvwmScript SendString 1 9\n")
                        wait_for_predicate("limit state", lambda: limit_report.exists() and
                            sum(line.startswith(b"limit:") for line in limit_report.read_bytes().splitlines()) > before,
                            time.monotonic()+5, process=core)
                        line = [line for line in limit_report.read_bytes().splitlines()
                            if line.startswith(b"limit:")][-1]
                        text = [line for line in full_report.read_bytes().splitlines()
                            if line and not line.startswith(b"#")][-1]
                        return text, *map(int, line[6:].split(b"|"))

                    connection.send(b"SendToModule FvwmScript SendString 1 8\n")
                    initial = limit_state()
                    assert initial[0] == b"x"*(limit-1), initial
                    key(parent, field, ord('z'))
                    exact = limit_state()
                    assert exact[:3] == (b"x"*(limit-1)+b"z", limit, limit), exact
                    assert exact[4] == initial[4]+1, (initial, exact)
                    assert exact[5] == limit-1, exact  # Unselected typing uses legacy callback order.
                    key(parent, field, ord('q'))
                    assert limit_state() == exact
                    connection.send(b"SendToModule FvwmScript SendString 1 10\n")
                    selected = limit_state()
                    assert selected[1:3] == (0, 4), selected
                    if focused:
                        # Existing client window owns CLIPBOARD; answer whole edits.
                        def paste(payload_bytes):
                            key(parent, field, ord('v'), 4)
                            event = Event()
                            wait_for_predicate("limited clipboard request",
                                lambda: x.XCheckTypedEvent(d, 30, C.byref(event)),
                                time.monotonic()+5, process=core)
                            request = event.request
                            payload = C.create_string_buffer(payload_bytes)
                            x.XChangeProperty(d, request.requestor, request.property,
                                request.target, 8, 0, payload, len(payload_bytes))
                            reply = Event()
                            reply.selection = SelectionNotify(type=31, display=d,
                                requestor=request.requestor, selection=request.selection,
                                target=request.target, property=request.property, time=request.time)
                            x.XSendEvent(d, request.requestor, 0, 0, C.byref(reply))
                            x.XSync(d, 0)
                        paste(b"12345")  # 128 - 4 + 5: reject the complete paste.
                        assert limit_state() == selected
                        paste(b"1234")  # Credit the selected bytes: exact limit.
                        replaced = limit_state()
                        assert replaced[0] == b"1234"+selected[0][4:], replaced
                        assert replaced[1:3] == (4, 4) and replaced[4] == selected[4]+1
                    else:
                        key(parent, field, ord('q'))
                        assert limit_state() == selected  # Legacy insertion keeps selection.
                        button(parent, field, 2)
                        assert limit_state() == selected  # Saved two-byte cut buffer would grow.
                    key(parent, field, 0xff53)
                    key(parent, field, 0xff08)
                    smaller = limit_state()
                    assert len(smaller[0]) == limit-1, smaller
                    key(parent, field, ord('r'))
                    recovered = limit_state()
                    assert len(recovered[0]) == limit and recovered[4] == smaller[4]+1
                    results[title+"-exact-refusal-selection-credit-and-recovery"] = "passed"
                assert state(2)[:3] == (b"seed", 4, 4)
                assert state(3)[:4] == (b"seed", 0, 4, 0)
                assert state(5)[-1] == 1
                assert state(6)[-1] == 1
                assert state(7)[-1] == 1
                assert state(6)[-1] == 1
                results[title+"-index-and-degenerate-list-recovery"] = "passed"
                # Partial exposes exercise skipped rows without allocating them.
                for _ in range(32):
                    x.XClearArea(d, listing, 0, 6, 160, 4, 1)
                x.XSync(d, 0)
                assert state()[-1] == 1
                connection.echo(title+"-wm-responsive")
                connection.send(b"KillModule FvwmScript\n")
                wait_for_predicate("script exit", lambda: named(title) is None,
                    time.monotonic()+5, process=core)
            results["partial-list-expose-and-core-recovery"] = "passed"
            x.XSync(d, 0)
            assert not unexpected_errors, unexpected_errors
        finally:
            if connection:
                connection.close()
            if client:
                client.close()
            if 'previous_error_handler' in locals():
                x.XSetErrorHandler.argtypes = [C.c_void_p]
                x.XSetErrorHandler(previous_error_handler)
            for path in runtime.root.iterdir():
                if path.is_file():
                    shutil.copy2(path, options.artifacts_dir / path.name)
            (options.artifacts_dir / "results.json").write_text(json.dumps(results, indent=2)+"\n")
    print("Real text edits, selection copies, list geometry and recovery passed")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("fvwm", "mfl", "script", "artifacts-dir"):
        parser.add_argument("--"+name, type=Path, required=True)
    parser.add_argument("--display")
    parser.add_argument("--input-backlog", type=int, choices=range(0, 257),
        default=0, metavar="0..256",
        help="queue bounded no-op keys before each X acknowledgement")
    args = parser.parse_args()
    for name in ("fvwm", "mfl", "script", "artifacts_dir"):
        setattr(args, name, getattr(args, name).resolve())
    run(args)
