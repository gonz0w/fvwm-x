#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.

"""Exercise real frames, pointer grabs, and client feedback on RandR monitors."""

import argparse
import ctypes as C
import ctypes.util
import json
from pathlib import Path
import re
import shutil
import socket
import tempfile
import time

from live_x_harness import (
    HarnessError, HarnessRuntime, query_xrandr,
    parse_xrandr_monitors, start_display_server, start_xorg_dummy,
    wait_for_predicate, wait_for_randr,
)


class Aspect(C.Structure):
    _fields_ = [("x", C.c_int), ("y", C.c_int)]


class SizeHints(C.Structure):
    _fields_ = [("flags", C.c_long)] + [
        (name, C.c_int) for name in (
            "x", "y", "width", "height", "min_width", "min_height",
            "max_width", "max_height", "width_inc", "height_inc",
        )
    ] + [("min_aspect", Aspect), ("max_aspect", Aspect)] + [
        (name, C.c_int) for name in ("base_width", "base_height", "win_gravity")
    ]


class ConfigureEvent(C.Structure):
    _fields_ = [("type", C.c_int), ("serial", C.c_ulong),
                ("send_event", C.c_int), ("display", C.c_void_p),
                ("event", C.c_ulong), ("window", C.c_ulong)] + [
        (name, C.c_int) for name in ("x", "y", "width", "height", "border_width")
    ] + [("above", C.c_ulong), ("override_redirect", C.c_int)]


class XEvent(C.Union):
    _fields_ = [("configure", ConfigureEvent), ("pad", C.c_long * 24)]


class Client:
    """A real Xlib client; geometry observations come directly from X."""

    def __init__(self, display):
        library = ctypes.util.find_library("X11")
        if library is None:
            raise HarnessError("libX11 is unavailable")
        self.x = C.CDLL(library)
        ptr, win, integer, unsigned = C.c_void_p, C.c_ulong, C.c_int, C.c_uint
        signatures = {
            "XOpenDisplay": ([C.c_char_p], ptr),
            "XDefaultRootWindow": ([ptr], win),
            "XCreateSimpleWindow": (
                [ptr, win, integer, integer, unsigned, unsigned, unsigned, win, win], win),
            "XMapWindow": ([ptr, win], integer),
            "XDestroyWindow": ([ptr, win], integer),
            "XSync": ([ptr, integer], integer),
            "XCloseDisplay": ([ptr], integer),
            "XStoreName": ([ptr, win, C.c_char_p], integer),
            "XSetWMNormalHints": ([ptr, win, C.POINTER(SizeHints)], None),
            "XQueryTree": ([ptr, win, C.POINTER(win), C.POINTER(win),
                            C.POINTER(C.POINTER(win)), C.POINTER(unsigned)], integer),
            "XGetGeometry": ([ptr, win, C.POINTER(win), C.POINTER(integer),
                              C.POINTER(integer), C.POINTER(unsigned),
                              C.POINTER(unsigned), C.POINTER(unsigned),
                              C.POINTER(unsigned)], integer),
            "XFree": ([ptr], integer),
            "XSelectInput": ([ptr, win, C.c_long], integer),
            "XCheckTypedWindowEvent": ([ptr, win, integer, C.POINTER(XEvent)], integer),
            "XResizeWindow": ([ptr, win, unsigned, unsigned], integer),
            "XGrabPointer": ([ptr, win, integer, unsigned, integer, integer,
                              win, win, C.c_ulong], integer),
            "XUngrabPointer": ([ptr, C.c_ulong], integer),
            "XQueryPointer": ([ptr, win, C.POINTER(win), C.POINTER(win),
                               C.POINTER(integer), C.POINTER(integer),
                               C.POINTER(integer), C.POINTER(integer),
                               C.POINTER(unsigned)], integer),
        }
        for name, (args, result) in signatures.items():
            function = getattr(self.x, name)
            function.argtypes, function.restype = args, result
        self.display = self.x.XOpenDisplay(display.encode())
        if not self.display:
            raise HarnessError("cannot open nested display")
        self.root = self.x.XDefaultRootWindow(self.display)
        self.window = None

    def create(self, fixed=False):
        self.window = self.x.XCreateSimpleWindow(
            self.display, self.root, 100, 100, 200, 120, 0, 0, 0)
        self.x.XStoreName(self.display, self.window, b"FVWM transition probe")
        self.x.XSelectInput(self.display, self.window, 1 << 17)
        if fixed:
            hints = SizeHints(flags=(1 << 4) | (1 << 5), min_width=200,
                              min_height=120, max_width=200, max_height=120)
            self.x.XSetWMNormalHints(self.display, self.window, C.byref(hints))
        self.x.XMapWindow(self.display, self.window)
        self.x.XSync(self.display, 0)

    def parent(self, window):
        root, parent = C.c_ulong(), C.c_ulong()
        children, count = C.POINTER(C.c_ulong)(), C.c_uint()
        if not self.x.XQueryTree(self.display, window, C.byref(root),
                                C.byref(parent), C.byref(children), C.byref(count)):
            raise HarnessError("XQueryTree failed")
        if children:
            self.x.XFree(children)
        return parent.value

    def frame(self):
        parent = self.parent(self.window)
        if parent == self.root:
            return None
        return self.parent(parent)

    def geometry(self, window):
        root, x, y = C.c_ulong(), C.c_int(), C.c_int()
        width, height, border, depth = [C.c_uint() for _ in range(4)]
        if not self.x.XGetGeometry(self.display, window, C.byref(root),
                                  C.byref(x), C.byref(y), C.byref(width),
                                  C.byref(height), C.byref(border), C.byref(depth)):
            raise HarnessError("XGetGeometry failed")
        return (x.value, y.value, width.value, height.value)

    def destroy(self):
        if self.window:
            self.x.XDestroyWindow(self.display, self.window)
            self.x.XSync(self.display, 0)
            self.window = None

    def close(self):
        self.destroy()
        self.x.XCloseDisplay(self.display)


class Pointer:
    """Input generated by the X server, including the real button grab."""

    def __init__(self, client):
        self.client = client
        library = ctypes.util.find_library("Xtst")
        if library is None:
            raise HarnessError("libXtst is required for interactive tests")
        self.x = C.CDLL(library)
        self.x.XTestFakeMotionEvent.argtypes = [C.c_void_p, C.c_int,
                                               C.c_int, C.c_int, C.c_ulong]
        self.x.XTestFakeButtonEvent.argtypes = [C.c_void_p, C.c_uint,
                                               C.c_int, C.c_ulong]
        for name in ("XTestFakeMotionEvent", "XTestFakeButtonEvent"):
            getattr(self.x, name).restype = C.c_int

    def motion(self, x, y):
        if not self.x.XTestFakeMotionEvent(self.client.display, -1, x, y, 0):
            raise HarnessError("XTest motion failed")
        self.client.x.XSync(self.client.display, 0)

    def button(self, pressed):
        if not self.x.XTestFakeButtonEvent(self.client.display, 1, pressed, 0):
            raise HarnessError("XTest button failed")
        self.client.x.XSync(self.client.display, 0)

    def grabbed(self):
        status = self.client.x.XGrabPointer(
            self.client.display, self.client.root, 0, 0, 1, 1, 0, 0, 0)
        if status == 0:
            self.client.x.XUngrabPointer(self.client.display, 0)
            self.client.x.XSync(self.client.display, 0)
        elif status != 1:
            raise HarnessError(f"unexpected grab status: {status}")
        return status == 1

    def position(self):
        root, child = C.c_ulong(), C.c_ulong()
        x, y, wx, wy, mask = C.c_int(), C.c_int(), C.c_int(), C.c_int(), C.c_uint()
        self.client.x.XQueryPointer(self.client.display, self.client.root,
                                   C.byref(root), C.byref(child), C.byref(x),
                                   C.byref(y), C.byref(wx), C.byref(wy), C.byref(mask))
        return (x.value, y.value, mask.value)

def run_interactive(client, command, process, evidence, quarters, log, smoke):
    destination_move = "Move 300p 100p" if smoke else "Move 2000p 100p"
    for directive in (
            "OpaqueMoveSize unlimited",
            "AddToFunc ProbeMove I Echo pointer-move-entered",
            "AddToFunc ProbeMove I Move",
            "Mouse 1 W N ProbeMove",
            "Style * SnapAttraction 0, EdgeMoveDelay 0",
            "Style * FPAllowFocusClickFunction, FPAllowRaiseClickFunction"):
        command(directive, window=False)
    pointer = Pointer(client)
    for fixed in (False, True):
        case = {"state": "fixed" if fixed else "normal", "transition": "pointer"}
        evidence["cases"].append(case)
        client.create(fixed=fixed)
        frame = wait_for_predicate("managed drag client", client.frame,
                                   time.monotonic() + 5, process=process)
        command("Move 100p 100p")
        command("Focus")
        source = client.geometry(client.window)[2:]
        case["source_client"] = source
        pointer.motion(200, 180)
        before = log.read_text().count("pointer-move-entered")
        pointer.button(True)
        try:
            wait_for_predicate("move binding",
                               lambda: log.read_text().count("pointer-move-entered") > before,
                               time.monotonic() + 5, process=process)
            wait_for_predicate("move grab", pointer.grabbed,
                               time.monotonic() + 5, process=process)
            case["crossings"] = []
            crossings = ((500, 4), (300, 4), (600, 4), (350, 4)) if smoke else (
                (2100, quarters), (600, 4), (2200, quarters), (800, 4))
            for x, factor in crossings:
                pointer.motion(x, 210)
                expected = source if fixed else tuple(
                    (value * factor + 2) // 4 for value in source)
                offset = ((100 * factor + 2) // 4, (80 * factor + 2) // 4)
                def settled():
                    geometry = client.geometry(frame)
                    case["last_observed"] = {"frame": geometry,
                                             "pointer": pointer.position(),
                                             "parent": client.geometry(client.parent(client.window)),
                                             "client": client.geometry(client.window),
                                             "expected_client": expected,
                                             "expected_origin": [x - offset[0], 210 - offset[1]]}
                    return (client.geometry(client.window)[2:] == expected
                            and geometry[:2] == (x - offset[0], 210 - offset[1]))
                wait_for_predicate("scaled pointer anchor", settled,
                                   time.monotonic() + 5, process=process)
                if not pointer.grabbed():
                    raise HarnessError("WM released its grab during monitor crossing")
                case["crossings"].append({"pointer": [x, 210],
                                           "frame": client.geometry(frame),
                                           "client": client.geometry(client.window)})
        finally:
            pointer.button(False)
        wait_for_predicate("released move grab", lambda: not pointer.grabbed(),
                           time.monotonic() + 5, process=process)
        command("Echo drag-finished", window=False)
        if client.geometry(client.window)[2:] != source:
            raise HarnessError(f"opaque drag round trip drifted: {case}")
        client.destroy()

    # Act like a client that echoes WM ConfigureNotify dimensions. Real
    # ConfigureRequests go through the WM; later genuine requests must still
    # update its logical size. No internal geometry cache is read by this test.
    case = {"transition": "configure-feedback", "notifications": []}
    evidence["cases"].append(case)
    client.create()
    wait_for_predicate("managed feedback client", client.frame,
                       time.monotonic() + 5, process=process)
    command("Move 100p 100p")
    def notifications():
        event = XEvent()
        result = []
        while client.x.XCheckTypedWindowEvent(
                client.display, client.window, 22, C.byref(event)):
            result.append((event.configure.width, event.configure.height))
        return result
    notifications()
    for state in ("normal", "maximized", "fullscreen"):
        maximize = "Maximize fullscreen" if state == "fullscreen" else "Maximize"
        if state != "normal":
            command(f"{maximize} on 100 100")
        command(destination_move)
        actual = client.geometry(client.window)[2:]
        received = notifications()
        if actual not in received:
            raise HarnessError(f"client missed destination ConfigureNotify: {actual}, {received}")
        case["notifications"].append({"state": state, "sizes": received})
        client.x.XResizeWindow(client.display, client.window, *actual)
        client.x.XSync(client.display, 0)
        command("Echo feedback-processed", window=False)
        if state != "normal":
            command(f"{maximize} off")
        expected = (200, 120) if smoke else (50 * quarters, 30 * quarters)
        if client.geometry(client.window)[2:] != expected:
            raise HarnessError(f"configure echo corrupted logical restore: {case}")
        command("Move 100p 100p")
        if client.geometry(client.window)[2:] != (200, 120):
            raise HarnessError(f"configure echo drifted source size: {case}")
        notifications()
    client.x.XResizeWindow(client.display, client.window, 240, 144)
    client.x.XSync(client.display, 0)
    wait_for_predicate("genuine client resize",
                       lambda: client.geometry(client.window)[2:] == (240, 144),
                       time.monotonic() + 5, process=process)
    command(destination_move)
    expected = (240, 144) if smoke else (60 * quarters, 36 * quarters)
    if client.geometry(client.window)[2:] != expected:
        raise HarnessError("genuine ConfigureRequest did not update logical dimensions")
    command("Move 100p 100p")
    if client.geometry(client.window)[2:] != (240, 144):
        raise HarnessError("genuine client resize round trip drifted")
    client.destroy()
    command("Style * ResizeOpaque", window=False)
    command("AddToFunc ProbeResize I Echo pointer-resize-entered", window=False)
    command("AddToFunc ProbeResize I Resize direction SE fixeddirection", window=False)
    command("Mouse 1 W N ProbeResize", window=False)
    case = {"transition": "opaque-resize"}
    evidence["cases"].append(case)
    client.create()
    frame = wait_for_predicate("managed resize client", client.frame,
                               time.monotonic() + 5, process=process)
    command("Move 100p 100p")
    command("Focus")
    pointer.motion(280, 230)
    before = log.read_text().count("pointer-resize-entered")
    pointer.button(True)
    try:
        wait_for_predicate("resize binding",
                           lambda: log.read_text().count("pointer-resize-entered") > before,
                           time.monotonic() + 5, process=process)
        wait_for_predicate("resize grab", pointer.grabbed,
                           time.monotonic() + 5, process=process)
        original = client.geometry(client.window)[2:]
        pointer.motion(360, 290)
        wait_for_predicate("opaque resize before release",
                           lambda: all(a > b for a, b in zip(
                               client.geometry(client.window)[2:], original)),
                           time.monotonic() + 5, process=process)
        if not pointer.grabbed():
            raise HarnessError("WM lost its grab during opaque resize")
        case["while_pressed_client"] = client.geometry(client.window)[2:]
        case["while_pressed_frame"] = client.geometry(frame)
    finally:
        pointer.button(False)
    wait_for_predicate("released resize grab", lambda: not pointer.grabbed(),
                       time.monotonic() + 5, process=process)
    command("Echo resize-finished", window=False)
    resized = client.geometry(client.window)[2:]
    case["released_client"] = resized
    command(destination_move)
    expected = resized if smoke else tuple((value * quarters + 2) // 4 for value in resized)
    case["destination_client"] = client.geometry(client.window)[2:]
    if case["destination_client"] != expected:
        raise HarnessError(f"opaque resize lost logical destination size: {case}")
    command("Move 100p 100p")
    case["restored_client"] = client.geometry(client.window)[2:]
    if client.geometry(client.window)[2:] != resized:
        raise HarnessError(f"opaque resize round trip drifted: {case}")
    client.destroy()
    for operation in ("Resize", "ResizeMove", "shaded-resize"):
        case = {"transition": operation}
        evidence["cases"].append(case)
        client.create()
        wait_for_predicate("managed command resize client", client.frame,
                           time.monotonic() + 5, process=process)
        command(destination_move)
        if operation == "shaded-resize":
            command("WindowShade N")
        resize = "ResizeMove" if operation == "ResizeMove" else "Resize"
        arguments = f"{80 * quarters}p {60 * quarters}p"
        if resize == "ResizeMove":
            arguments += " 300p 100p" if smoke else " 2000p 100p"
        command(f"{resize} {arguments}")
        if operation == "shaded-resize":
            command("WindowShade off")
        case["destination_client"] = client.geometry(client.window)[2:]
        if case["destination_client"] != (80 * quarters, 60 * quarters):
            raise HarnessError(f"command resize did not apply: {case}")
        command("Move 100p 100p")
        case["source_client"] = client.geometry(client.window)[2:]
        restored = (80 * quarters, 60 * quarters) if smoke else (320, 240)
        if case["source_client"] != restored:
            raise HarnessError(f"command resize retained stale logical dimensions: {case}")
        for temporary in ("ResizeMaximize 500p 400p",
                          "ResizeMoveMaximize 500p 400p 100p 100p"):
            command(temporary)
            command("Maximize off")
            if client.geometry(client.window)[2:] != restored:
                raise HarnessError(f"temporary maximize resize corrupted restore: {case}")
        client.destroy()
    print(f"quarters={quarters}: pointer crossings and configure feedback passed", flush=True)


def run_factor(options, quarters):
    runtime = HarnessRuntime(prefix="fvwm-shaded-transition-")
    client = None
    evidence = {"quarters": quarters, "smoke": options.smoke, "cases": []}
    options.artifacts_dir.mkdir(parents=True, exist_ok=True)
    destination = Path(tempfile.mkdtemp(prefix=f"{quarters}-", dir=options.artifacts_dir))
    try:
        if options.display:
            display = options.display
            evidence["external_display"] = display
        elif options.smoke:
            _, display = start_display_server(
                runtime, "Xvfb", ["Xvfb", "-screen", "0", "1280x1024x24", "-noreset"],
                "xserver.log", time.monotonic() + 10)
        else:
            _, display = start_xorg_dummy(runtime, options.xorg_config,
                                         time.monotonic() + 10)
        wait_for_randr(runtime, display, time.monotonic() + 10)
        mutations = () if options.smoke else (
            ["--addmode", "DUMMY1", "1280x1024"],
            ["--output", "DUMMY0", "--mode", "1920x1080", "--pos", "0x0",
             "--primary", "--output", "DUMMY1", "--mode", "1280x1024",
             "--pos", "1920x0", "--fb", "3200x1080"],
        )
        for index, args in enumerate(mutations):
            code, text = query_xrandr(runtime, display, args,
                                     time.monotonic() + 5, f"layout-{index}.log")
            if code:
                raise HarnessError(text)
        code, text = query_xrandr(runtime, display, ["--listmonitors"],
                                 time.monotonic() + 5, "monitors.log")
        if code:
            raise HarnessError(text)
        # Framebuffer drivers can lack gamma support. The command's
        # combined log retains that diagnostic; parse only monitor records.
        lines = text.splitlines()
        diagnostics = [line for line in lines if re.fullmatch(
            r"(?:.*/)?xrandr: Failed to get size of gamma for output \S+", line)]
        if diagnostics:
            evidence["xrandr_diagnostics"] = diagnostics
            text = "\n".join(line for line in lines if line not in diagnostics)
        monitors = parse_xrandr_monitors(text)
        root_fallback = options.smoke and (not monitors or options.empty_monitors)
        if root_fallback:
            root_client = Client(display)
            try:
                x, y, width, height = root_client.geometry(root_client.root)
            finally:
                root_client.close()
            monitors = {"FVWM-X-Root": {
                "x": x, "y": y, "width": width, "height": height}}
            evidence["expected_root_fallback"] = True
        if not options.smoke and set(monitors) != {"DUMMY0", "DUMMY1"}:
            raise HarnessError(f"two real monitors required: {monitors}")
        if options.smoke and len(monitors) != 1:
            raise HarnessError(f"one monitor required for session smoke: {monitors}")
        evidence["monitors"] = monitors
        log = runtime.root / "fvwm.log"
        factor = str(quarters / 4).rstrip("0").rstrip(".")
        scales = tuple(f"MonitorScale {json.dumps(name)} " +
                       (factor if options.smoke or name == "DUMMY1" else "1")
                       for name in monitors)
        commands = scales + (
            "DefaultFont xft:DejaVu Sans:size=10",
            "Style * BorderWidth 3, HandleWidth 3, WindowShadeSteps 0, TileCascadePlacement",
            f"ModulePath {options.mfl.parent}",
            "AddToFunc StartFunction I Module FvwmMFL",
            "AddToFunc StartFunction I Echo transition-startup-complete",
        )
        arguments = [str(options.fvwm), "-d", display, "-o", str(log), "-v"]
        for directive in commands:
            arguments.extend(("-c", directive))
        environment = {"FVWMMFL_SOCKET_PATH": str(runtime.root)}
        if options.empty_monitors:
            environment["FVWM_TEST_XRR_EMPTY_MONITORS"] = "1"
        process = runtime.start("fvwm", arguments, "process.log",
                                runtime.environment(environment))
        endpoint = runtime.root / f"fvwm_mfl_{display.replace(':', '_')}.sock"
        wait_for_predicate("MFL socket", endpoint.exists, time.monotonic() + 10,
                           process=process)
        wait_for_predicate("FVWM startup",
                           lambda: "transition-startup-complete" in log.read_text(),
                           time.monotonic() + 10, process=process)
        if root_fallback and "Using root-sized single-screen fallback" not in log.read_text():
            raise HarnessError("WM did not commit the expected root fallback")
        client = Client(display)
        sequence = 0

        def command(text, window=True):
            nonlocal sequence
            sequence += 1
            marker = f"transition-command-{sequence}-done"
            prefix = f"WindowId {client.window} " if window else ""
            with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as connection:
                connection.settimeout(5)
                connection.connect(str(endpoint))
                connection.sendall(f"{prefix}{text}\nEcho {marker}\n".encode())
                wait_for_predicate(marker,
                                   lambda: log.exists() and marker in log.read_text(),
                                   time.monotonic() + 5, process=process)
            client.x.XSync(client.display, 0)

        command("Echo transition-probe-ready", window=False)
        with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as connection:
            connection.settimeout(5)
            connection.connect(str(endpoint))
            connection.sendall(b"set echo\nEcho transition-client-echo\n")
            with connection.makefile("rb") as replies:
                while True:
                    reply = replies.readline()
                    if not reply:
                        raise HarnessError("MFL closed before Echo delivery")
                    if json.loads(reply).get("echo", {}).get("message") == "transition-client-echo":
                        break
        evidence["mfl_echo_round_trip"] = True
        if options.interactive:
            run_interactive(client, command, process, evidence, quarters, log, options.smoke)
            return
        states = ("normal", "fixed", "maximized")
        if not options.smoke:
            states += ("workarea",)
        for state in states:
            maximized = state in ("maximized", "workarea")
            if state == "workarea":
                command("Style * EWMHMaximizeUseWorkingArea", window=False)
                command("EwmhBaseStruts screen DUMMY1 0 0 40 0", window=False)
            for direction in ("N", "S", "E", "W", "NE", "NW", "SE", "SW"):
                case = {"state": state, "direction": direction}
                evidence["cases"].append(case)
                client.create(fixed=state == "fixed")
                frame = wait_for_predicate("managed client", client.frame,
                                           time.monotonic() + 5, process=process)
                command("Move 100p 100p")
                if maximized:
                    command("Maximize on 100 100")
                source = client.geometry(client.window)[2:]
                source_frame = client.geometry(frame)
                case["source_client"] = source
                case["source_frame"] = source_frame
                command(f"WindowShade {direction}")
                if not (options.smoke and maximized):
                    command("Move 300p 100p" if options.smoke else "Move 2000p 100p")
                strip = client.geometry(frame)
                case["destination_strip"] = strip
                command("WindowShade off")
                dest_client = client.geometry(client.window)[2:]
                dest_frame = client.geometry(frame)
                case.update(destination_client=dest_client, destination_frame=dest_frame)
                if maximized:
                    if options.smoke:
                        monitor = next(iter(monitors.values()))
                        work_area = tuple(monitor[key] for key in
                                          ("x", "y", "width", "height"))
                    else:
                        work_area = (1920, 0, 1280, 1024)
                    if state == "workarea":
                        work_area = (1920, 40, 1280, 984)
                    if dest_frame != work_area:
                        raise HarnessError(f"maximized unshade missed work area: {case}")
                else:
                    expected = source if options.smoke or state == "fixed" else tuple(
                        (value * quarters + 2) // 4 for value in source)
                    if dest_client != expected:
                        raise HarnessError(f"unshade lost destination size: {case}, expected={expected}")
                if strip[2] >= dest_frame[2] and strip[3] >= dest_frame[3]:
                    raise HarnessError(f"transition expanded shaded window: {case}")
                command(f"WindowShade {direction}")
                if not (options.smoke and maximized):
                    command("Move 100p 100p")
                command("WindowShade off")
                restored = client.geometry(client.window)[2:]
                case["restored_client"] = restored
                if restored != source:
                    raise HarnessError(f"round trip drifted: {case}")
                if maximized and client.geometry(frame) != source_frame:
                    raise HarnessError(f"maximized return missed source work area: {case}")
                client.destroy()
        print(f"quarters={quarters}: {len(evidence['cases'])} shaded round trips passed",
              flush=True)
        if not options.smoke:
            for state in ("maximized", "fullscreen"):
                case = {"state": state, "transition": "restore"}
                evidence["cases"].append(case)
                client.create()
                frame = wait_for_predicate("managed restore client", client.frame,
                                           time.monotonic() + 5, process=process)
                command("Move 100p 100p")
                source = client.geometry(client.window)[2:]
                maximize = "Maximize fullscreen" if state == "fullscreen" else "Maximize"
                command(f"{maximize} on 100 100")
                command("Move 2000p 100p")
                target = (1920, 0, 1280, 1024) if state == "fullscreen" else (1920, 40, 1280, 984)
                case["destination_frame"] = client.geometry(frame)
                if case["destination_frame"] != target:
                    raise HarnessError(f"unshaded transition missed destination: {case}")
                command(f"{maximize} off")
                expected = tuple((value * quarters + 2) // 4 for value in source)
                case["restored_client"] = client.geometry(client.window)[2:]
                if case["restored_client"] != expected:
                    raise HarnessError(f"restore lost logical size: {case}, expected={expected}")
                command("Move 100p 100p")
                if client.geometry(client.window)[2:] != source:
                    raise HarnessError(f"restore round trip drifted: {case}")
                client.destroy()
            print(f"quarters={quarters}: maximize/fullscreen restores passed", flush=True)
    except BaseException as error:
        evidence["error"] = str(error)
        if client and client.window:
            evidence["client_window"] = client.window
            evidence["root_window"] = client.root
            evidence["client_parent"] = client.parent(client.window)
            evidence["client_geometry"] = client.geometry(client.window)
            if client.frame():
                evidence["frame_geometry"] = client.geometry(client.frame())
        raise
    finally:
        for path in runtime.root.glob("*.log"):
            try:
                shutil.copy2(path, destination / path.name)
            except OSError as capture_error:
                evidence.setdefault("capture_errors", []).append(str(capture_error))
        (destination / "scenario.json").write_text(json.dumps(evidence, indent=2) + "\n")
        try:
            if client:
                client.close()
        finally:
            runtime.close()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--smoke", action="store_true")
    parser.add_argument("--interactive", action="store_true")
    parser.add_argument("--smoke-quarters", type=int, choices=range(4, 17), default=4)
    parser.add_argument("--display", help="use an existing dedicated X display")
    parser.add_argument("--empty-monitors", action="store_true")
    parser.add_argument("--xorg-config", type=Path)
    for name in ("fvwm", "mfl", "artifacts-dir"):
        parser.add_argument(f"--{name}", type=Path, required=True)
    options = parser.parse_args()
    if options.display and not options.smoke:
        parser.error("--display requires --smoke")
    if options.empty_monitors and not options.smoke:
        parser.error("--empty-monitors requires --smoke")
    paths = ("fvwm", "mfl") if options.smoke else ("fvwm", "mfl", "xorg_config")
    for name in paths:
        if getattr(options, name) is None:
            parser.error(f"{name} is required")
        path = getattr(options, name).resolve()
        if not path.is_file():
            parser.error(f"{name} must name an existing file")
        setattr(options, name, path)
    for quarters in ((options.smoke_quarters,) if options.smoke else (5, 6, 8)):
        run_factor(options, quarters)


if __name__ == "__main__":
    main()
