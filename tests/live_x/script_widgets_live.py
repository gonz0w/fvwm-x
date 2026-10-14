#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.

"""Observe real widget pixels, signed values, wrapping and input completion."""
import argparse
import ctypes as C
import json
import os
from pathlib import Path
import shutil
import signal
import sys
import time

from geometry_transition_live import Client
from live_x_harness import HarnessRuntime, start_display_server, wait_for_predicate
from mfl_stream_live import Connection
from selection_live import Button, Event, Key


def run(options):
    options.artifacts_dir.mkdir(parents=True, exist_ok=True)
    for name in ("results.json", "repeat-profile.json", "widget-report",
                 "inherited-fds.json", "core.log", "xserver.log"):
        (options.artifacts_dir / name).unlink(missing_ok=True)
    evidence = {}
    with HarnessRuntime(prefix="fvwm-script-widgets-") as runtime:
        client = connection = None
        try:
            display = options.display
            if display is None:
                _, display = start_display_server(runtime, "Xvfb",
                    ["Xvfb", "-screen", "0", "800x600x24", "-noreset"],
                    "xserver.log", time.monotonic() + 10)
            wrapper_dir = runtime.root / "module"
            wrapper_dir.mkdir()
            wrapper = wrapper_dir / "FvwmScript"
            # Keep every descriptor through 80 occupied across exec; the module
            # retains these inherited slots and opens its X connection later.
            wrapper.write_text("#!" + sys.executable + "\n"
                "import json, os, sys\nheld=[]\n"
                "while not held or held[-1] < 80:\n"
                " fd=os.open('/dev/null', os.O_RDONLY)\n"
                " os.set_inheritable(fd, True)\n held.append(fd)\n"
                "open(" + repr(str(runtime.root / "inherited-fds.json")) +
                ", 'w').write(json.dumps(held))\n"
                "os.execv(" + repr(str(options.script)) + ", [" +
                repr(str(options.script)) + ", *sys.argv[1:]])\n")
            wrapper.chmod(0o755)
            config = runtime.root / "config"
            config.write_text(f"ModulePath {wrapper_dir}:{options.mfl.parent}\n"
                "Style * TileCascadePlacement\n"
                "AddToFunc StartFunction I Module FvwmMFL\n")
            core = runtime.start("core", [str(options.fvwm), "-d", display,
                "-f", str(config)], "core.log", runtime.environment({
                    "FVWMMFL_SOCKET_PATH": str(runtime.root)}))
            endpoint = runtime.root / f"fvwm_mfl_{display.replace(':', '_')}.sock"
            wait_for_predicate("widget endpoint", endpoint.exists,
                time.monotonic()+10, process=core)
            connection = Connection(endpoint)
            connection.record()
            connection.send(b"set echo\n")
            connection.echo("widgets-ready")
            client = Client(display)
            x, d = client.x, client.display
            x.XFetchName.argtypes = [C.c_void_p, C.c_ulong, C.POINTER(C.c_void_p)]
            x.XGetImage.argtypes = [C.c_void_p, C.c_ulong, C.c_int, C.c_int,
                C.c_uint, C.c_uint, C.c_ulong, C.c_int]
            x.XGetImage.restype = C.c_void_p
            x.XGetPixel.argtypes = [C.c_void_p, C.c_int, C.c_int]
            x.XGetPixel.restype = C.c_ulong
            x.XDestroyImage.argtypes = [C.c_void_p]
            x.XSendEvent.argtypes = [C.c_void_p, C.c_ulong, C.c_int,
                C.c_long, C.POINTER(Event)]
            x.XKeysymToKeycode.argtypes = [C.c_void_p, C.c_ulong]
            x.XKeysymToKeycode.restype = C.c_uint
            x.XWarpPointer.argtypes = [C.c_void_p, C.c_ulong, C.c_ulong,
                C.c_int, C.c_int, C.c_uint, C.c_uint, C.c_int, C.c_int]
            x.XClearArea.argtypes = [C.c_void_p, C.c_ulong, C.c_int, C.c_int,
                C.c_uint, C.c_uint, C.c_int]
            x.XCheckTypedWindowEvent.argtypes = [C.c_void_p, C.c_ulong,
                C.c_int, C.POINTER(Event)]

            def children(window):
                root, parent, items, count = C.c_ulong(), C.c_ulong(), C.POINTER(C.c_ulong)(), C.c_uint()
                assert x.XQueryTree(d, window, C.byref(root), C.byref(parent),
                    C.byref(items), C.byref(count))
                values = [items[i] for i in range(min(count.value, 128))]
                if items:
                    x.XFree(items)
                return values

            def named(title):
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
                        queue.extend((w, depth+1) for w in children(window))
                return None

            report = runtime.root / "widget-report"
            script = runtime.root / "WidgetMathProbe"
            if options.legacy_control:
                script.write_text("WindowTitle {WidgetMathProbe}\nWindowSize 160 70\n"
                    "Init\nBegin\n Set $file={"+str(report)+"}\n"
                    " WriteToFile $file {ready}\nEnd\n"
                    "Widget 1\nProperty\n Type HDipstick\n Position 10 10\n"
                    " Size 120 20\n MinValue 0\n MaxValue 100\n Value 25\n"
                    "Main\nCase message of\n1 :\nBegin\n ChangeMaxValue 1 0\n"
                    " WriteToFile $file {after-equal-range}\nEnd\nEnd\n")
                connection.send(("Module FvwmScript "+str(script)+"\n").encode())
                wait_for_predicate("control ready", lambda: report.exists() and
                    "ready" in report.read_text() and named("WidgetMathProbe"),
                    time.monotonic()+10, process=core)
                connection.send(b"SendToModule FvwmScript SendString 1 1\n")
                wait_for_predicate("legacy equal-range module exit",
                    lambda: named("WidgetMathProbe") is None,
                    time.monotonic()+10, process=core)
                assert "after-equal-range" not in report.read_text()
                connection.echo("wm-after-legacy-widget-exit")
                evidence["legacy-equal-range-module-exit-before-next-instruction"] = "observed"
                return

            def changes(command, value):
                return "".join(f" {command} {i} {value}\n" for i in range(1, 5))

            bodies = {
                1: changes("ChangeValue", "0"),
                2: changes("ChangeValue", "100"),
                3: changes("ChangeValue", "-20"),
                4: changes("ChangeValue", "200"),
                5: changes("ChangeMinValue", "10") + changes("ChangeMaxValue", "10") + changes("ChangeValue", "99"),
                6: changes("ChangeMinValue", "20") + changes("ChangeMaxValue", "10") + changes("ChangeValue", "99"),
                7: changes("ChangeMinValue", "-2147483648") + changes("ChangeMaxValue", "2147483647") + changes("ChangeValue", "0"),
                8: " Set $bad={999999999999999999999999}\n Set $badMin={-2147483649}\n Set $badMax={2147483648}\n" +
                   changes("ChangeValue", "$bad") + changes("ChangeMinValue", "$badMin") + changes("ChangeMaxValue", "$badMax") +
                   " ChangePosition 1 $bad 10\n ChangeSize 1 120 $badMax\n" +
                   "".join(f" WriteToFile $file {{bounds:{i}:}} (GetMinValue {i}) {{:}} (GetMaxValue {i})\n" for i in range(1, 5)),
                9: " Set $prefix={26tail}\n" + changes("ChangeValue", "$prefix"),
                10: " Set $prefix={text}\n" + changes("ChangeValue", "$prefix"),
                11: " ChangeMinValue 5 -2147483648\n ChangeMaxValue 5 2147483647\n ChangeValue 5 2147483647\n",
                12: " ChangeMinValue 5 0\n ChangeMaxValue 5 10\n ChangeValue 5 4\n",
                13: " ChangeValue 3 2147483646\n ChangeValue 4 2147483646\n",
                14: " ChangeMinValue 3 0\n ChangeMaxValue 3 60\n ChangeValue 3 0\n"
                    " ChangeMinValue 4 0\n ChangeMaxValue 4 60\n ChangeValue 4 0\n",
                15: " Set $a=-100000\n Set $b=-350000\n Set $c=-2147483648\n Set $d=2147483647\n"
                    " If $c==-2147483648 Then\n WriteToFile $file {numeric-tags:} $a {:} $b {:} $c {:} $d\n",
                17: changes("ChangeValue", "-1"),
                18: changes("ChangeValue", "1"),
                19: changes("ChangeValue", "5"),
                16: changes("ChangeMinValue", "0") + changes("ChangeMaxValue", "100") + changes("ChangeValue", "50"),
            }
            snapshot = " ".join(f"(GetValue {i}) {{:}}" for i in range(1, 6))
            cases = "".join(f"{n} :\nBegin\n{body} Set $barrier=(GetFore 1)\n WriteToFile $file {{event:{n}:}} {snapshot}\nEnd\n"
                for n, body in bodies.items())
            header = ("WindowTitle {WidgetMathProbe}\nWindowSize 600 360\n"
                "ForeColor {white}\nBackColor {black}\nHilightColor {gray}\nShadowColor {gray}\n"
                "Init\nBegin\n Set $file={"+str(report)+"}\n WriteToFile $file {ready}\n"
                " WriteToFile $file {startup:} (GetMaxValue 6) {:} (GetValue 6) {:} (GetMaxValue 7) {:} (GetValue 7)\nEnd\n")
            widgets = [(1, "HDipstick", 10, 10, 120, 20, 0, 100, 25),
                (2, "VDipstick", 150, 10, 20, 120, 0, 100, 25),
                (3, "HScrollBar", 10, 200, 150, 40, 0, 100, 25),
                (4, "VScrollBar", 200, 160, 40, 150, 0, 100, 25),
                (5, "MiniScroll", 400, 10, 19, 34, 0, 10, 4),
                (6, "HDipstick", 300, 50, 120, 20, 2147483647, -2147483648, 2147483647),
                (7, "HScrollBar", 300, 200, 150, 40, 2147483647, -2147483648, 2147483647)]
            text = header
            for i, kind, px, py, w, h, minimum, maximum, value in widgets:
                text += (f"Widget {i}\nProperty\n Type {kind}\n Position {px} {py}\n"
                    f" Size {w} {h}\n MinValue {minimum}\n MaxValue {maximum}\n Value {value}\n")
                if i == 1:
                    text += "Main\nCase message of\n" + cases + "End\n"
                elif i in (3, 4, 5):
                    text += ("Main\nCase message of\n SingleClic :\nBegin\n"
                        f" Set $barrier=(GetFore {i})\n WriteToFile $file {{key:{i}:}} (GetValue {i})\nEnd\nEnd\n")
                else:
                    text += "End\n"
            script.write_text(text)
            shutil.copy2(script, options.artifacts_dir / script.name)
            connection.send(("Module FvwmScript "+str(script)+"\n").encode())
            wait_for_predicate("widget script ready", lambda: report.exists() and
                "ready" in report.read_text() and named("WidgetMathProbe"),
                time.monotonic()+10, process=core)
            window = named("WidgetMathProbe")
            expected_positions = {(v[2], v[3]): v[0] for v in widgets}
            def mapped_widgets():
                return {expected_positions[client.geometry(child)[:2]]: child
                    for child in children(window) if client.geometry(child)[:2] in expected_positions}
            wait_for_predicate("all widget children", lambda: len(mapped_widgets()) == 7,
                time.monotonic()+10, process=core)
            child = mapped_widgets()
            fds = json.loads((runtime.root / "inherited-fds.json").read_text())
            assert max(fds) >= 80
            wait_for_predicate("startup endpoint getters", lambda:
                "startup:2147483647:2147483647:2147483647:2147483647\n" in report.read_text(),
                time.monotonic()+10, process=core)
            evidence["startup-overflow-fallback-and-inherited-descriptors"] = "passed"

            def event(number, expected=None):
                marker = f"event:{number}:"
                count = report.read_text().count(marker)
                connection.send((f"SendToModule FvwmScript SendString 1 {number}\n").encode())
                wait_for_predicate(marker, lambda: report.read_text().count(marker) > count,
                    time.monotonic()+10, process=core)
                row = [line for line in report.read_text().splitlines() if line.startswith(marker)][-1]
                if expected is not None:
                    assert row == marker + expected, row
                return row

            def pixels(i):
                w, h = client.geometry(child[i])[2:4]
                px, py, width, height = 0, 0, w, h
                if i == 3:
                    py, height = h//2-10, 20
                elif i == 4:
                    px, width = w//2-8, 16
                image = x.XGetImage(d, child[i], px, py, width, height, C.c_ulong(-1).value, 2)
                assert image
                try:
                    return tuple(x.XGetPixel(image, a, b) for b in range(height) for a in range(width))
                finally:
                    x.XDestroyImage(image)

            def pictures():
                x.XSync(d, 0)
                return {i: pixels(i) for i in range(1, 5)}

            event(1, "0:0:0:0:4:")
            low = pictures()
            event(2, "100:100:100:100:4:")
            high = pictures()
            assert all(low[i] != high[i] for i in low)
            event(3, "-20:-20:-20:-20:4:")
            assert pictures() == low
            event(4, "200:200:200:200:4:")
            assert pictures() == high
            evidence["clamped-pixels-preserve-stored-values"] = "passed"
            x.XWhitePixel.argtypes = [C.c_void_p, C.c_int]
            x.XWhitePixel.restype = C.c_ulong
            white = x.XWhitePixel(d, 0)
            event(18, "1:1:1:1:4:")
            tiny = pictures()
            assert all(tiny[i].count(white) == 0 for i in (1, 2))
            event(19, "5:5:5:5:4:")
            small = pictures()
            assert all(small[i].count(white) > 0 for i in (1, 2))
            evidence["tiny-progress-fill-does-not-wrap-unsigned"] = "passed"
            event(5, "99:99:99:99:4:")
            equal = pictures()
            assert equal[1] == low[1] and equal[2] == low[2]
            event(6, "99:99:99:99:4:")
            inverted = pictures()
            assert inverted == equal
            evidence["equal-and-inverted-runtime-ranges-remain-usable"] = "passed"
            event(16, "50:50:50:50:4:")
            normal_middle = pictures()
            event(7, "0:0:0:0:4:")
            extreme_middle = pictures()
            assert all(extreme_middle[i] == normal_middle[i] for i in (1, 2, 3))
            # The asymmetric int32 endpoints put reverse zero just below half.
            event(17, "-1:-1:-1:-1:4:")
            assert pictures()[4] == normal_middle[4]
            event(7, "0:0:0:0:4:")
            evidence["full-signed-range-proportions"] = "passed"
            geometry = client.geometry(child[1])
            event(8, "0:0:0:0:4:")
            assert client.geometry(child[1]) == geometry
            assert all(f"bounds:{i}:-2147483648:2147483647\n" in report.read_text() for i in range(1, 5))
            event(9, "26:26:26:26:4:")
            event(10, "0:0:0:0:4:")
            evidence["overflow-refusal-and-prefix-compatible-mutations"] = "passed"
            event(15)
            assert "numeric-tags:-100000:-350000:-2147483648:2147483647\n" in report.read_text()
            evidence["full-signed-literals-and-comparison"] = "passed"

            def key(i, symbol, expected):
                marker = f"key:{i}:"
                previous = report.read_text().count(marker)
                ev = Event()
                ev.key = Key(type=2, display=d, window=window, root=client.root,
                    subwindow=child[i], keycode=x.XKeysymToKeycode(d, symbol), same_screen=1)
                assert x.XSendEvent(d, window, 0, 1, C.byref(ev))
                x.XSync(d, 0)
                def completed():
                    text = report.read_text()
                    rows = [line for line in text.splitlines() if line.startswith(marker)]
                    return text.count(marker) > previous and rows[-1] == marker+str(expected)
                wait_for_predicate("widget key completion", completed,
                    time.monotonic()+10, process=core)

            event(11)
            key(5, 0xff52, -2147483648)  # Up
            key(5, 0xff54, 2147483647)   # Down
            event(12)
            key(5, 0xff52, 5)
            key(5, 0xff54, 4)
            evidence["mini-scroll-ordinary-and-extreme-wrap"] = "passed"
            x.XWarpPointer(d, 0, child[5], 0, 0, 0, 0, 9, 8)
            x.XSync(d, 0)
            key(5, 0xff0d, 5)
            evidence["mini-scroll-return-selects-once"] = "passed"
            event(13)
            key(3, 0xff53, 2147483647)  # Right
            key(4, 0xff52, 2147483647)
            evidence["scrollbar-extreme-key-increment"] = "passed"
            repeat_profile = []
            pid = int(next(line for line in report.read_text().splitlines()
                if line.startswith("#" + str(script) + ",")).rsplit(",", 1)[1])

            def cpu_seconds():
                stat = Path(f"/proc/{pid}/stat")
                if sys.platform != "linux":
                    return None
                fields = stat.read_text().rsplit(")", 1)[1].split()
                return (int(fields[11]) + int(fields[12])) / os.sysconf("SC_CLK_TCK")

            # Keep the immediate action and legacy held-repeat cadence, but
            # release cancels a pending step and returns to module processing.
            for held_callbacks in (1, 3):
                event(12)
                marker = "key:5:"
                previous = report.read_text().count(marker)
                x.XWarpPointer(d, 0, child[5], 0, 0, 0, 0, 9, 8)
                x.XSync(d, 0)
                ev = Event()
                ev.button = Button(type=4, display=d, window=window,
                    root=client.root, subwindow=child[5], button=1, same_screen=1)
                before_cpu = cpu_seconds()
                started = time.monotonic()
                assert x.XSendEvent(d, window, 0, 1 << 2, C.byref(ev))
                x.XSync(d, 0)
                wait_for_predicate("initial MiniScroll callback", lambda:
                    report.read_text().count(marker) > previous,
                    time.monotonic()+10, process=core)
                if held_callbacks == 3:
                    for _ in range(25):
                        x.XClearArea(d, child[5], 0, 0, 19, 34, 1)
                    x.XSync(d, 0)
                wait_for_predicate("held MiniScroll callbacks", lambda:
                    report.read_text().count(marker) >= previous + held_callbacks,
                    time.monotonic()+10, process=core)
                if held_callbacks == 1:
                    time.sleep(0.05)
                released = time.monotonic()
                ev.button.type = 5
                assert x.XSendEvent(d, window, 0, 1 << 3, C.byref(ev))
                x.XSync(d, 0)
                event(12)  # A following module command proves loop completion.
                finished = time.monotonic()
                after_cpu = cpu_seconds()
                rows = [line for line in report.read_text().splitlines()
                    if line.startswith(marker)][previous:]
                cpu = None if before_cpu is None else after_cpu - before_cpu
                repeat_profile.append({"release_after_callbacks": held_callbacks,
                    "callbacks": len(rows), "elapsed_seconds": finished-started,
                    "release_to_module_command_seconds": finished-released,
                    "module_cpu_seconds": cpu})
                (options.artifacts_dir / "repeat-profile.json").write_text(
                    json.dumps(repeat_profile, indent=2)+"\n")
                assert len(rows) == held_callbacks, rows
                assert rows == [marker+str(5+i) for i in range(len(rows))], rows
                if held_callbacks == 3:
                    assert released-started >= 0.53
                if cpu is not None:
                    assert cpu < 0.1, cpu
            evidence["mini-scroll-release-cancels-pending-repeat"] = "passed"
            evidence["unrelated-x-input-preserves-held-repeat-cadence"] = "passed"
            event(12)
            marker = "key:5:"
            previous = report.read_text().count(marker)
            unpressed = pixels(5)
            ev = Event()
            ev.button = Button(type=4, display=d, window=window, root=client.root,
                subwindow=child[5], button=1, same_screen=1)
            assert x.XSendEvent(d, window, 0, 1 << 2, C.byref(ev))
            ev.button.type = 5
            assert x.XSendEvent(d, window, 0, 1 << 3, C.byref(ev))
            x.XSync(d, 0)
            event(12)
            assert report.read_text().count(marker) == previous + 1
            assert pixels(5) == unpressed
            evidence["mini-scroll-queued-release-restores-arrow-pixels"] = "passed"
            event(14)
            for i in (3, 4):
                w, h = client.geometry(child[i])[2:4]
                pointer = 47
                extent = w if i == 3 else h
                expected = ((pointer-15)*extent//(extent-36))*60//extent
                if i == 4:
                    expected = 60 - expected
                x.XWarpPointer(d, 0, child[i], 0, 0, 0, 0,
                    pointer if i == 3 else w//2, h//2 if i == 3 else pointer)
                x.XSync(d, 0)
                key(i, 0xff0d, expected)  # Return must not wait for a release.
            evidence["return-selects-once-with-legacy-pointer-rounding"] = "passed"
            # Send a press and release together. XQueryPointer can enqueue the
            # release in Xlib; waiting first would then miss readable-FD work.
            for i in (3, 4):
                ev = Event()
                ev.button = Button(type=4, display=d, window=window, root=client.root,
                    subwindow=child[i], button=1, same_screen=1)
                assert x.XSendEvent(d, window, 0, 1 << 2, C.byref(ev))
                ev.button.type = 5
                assert x.XSendEvent(d, window, 0, 1 << 3, C.byref(ev))
                x.XSync(d, 0)
            # A following module command proves both mouse loops returned.
            event(1)
            evidence["queued-release-and-high-descriptor-mouse-completion"] = "passed"
            connection.echo("wm-after-widget-checks")
            # Termination interrupts the owned module's held repeat; no mouse
            # release is sent, and the WM remains responsive after module exit.
            x.XWarpPointer(d, 0, child[5], 0, 0, 0, 0, 9, 8)
            x.XSync(d, 0)
            previous = report.read_text().count("key:5:")
            ev = Event()
            ev.button = Button(type=4, display=d, window=window, root=client.root,
                subwindow=child[5], button=1, same_screen=1)
            assert x.XSendEvent(d, window, 0, 1 << 2, C.byref(ev))
            x.XSync(d, 0)
            wait_for_predicate("held module before termination", lambda:
                report.read_text().count("key:5:") > previous,
                time.monotonic()+10, process=core)
            x.XSelectInput(d, window, 1 << 17)  # StructureNotifyMask
            x.XSync(d, 0)
            os.kill(pid, signal.SIGTERM)
            destroyed = Event()
            wait_for_predicate("module terminates during held repeat", lambda:
                x.XCheckTypedWindowEvent(d, window, 17, C.byref(destroyed)),
                time.monotonic()+10, process=core)
            connection.echo("wm-after-held-module-termination")
            evidence["mini-scroll-held-repeat-termination"] = "passed"
        finally:
            if connection:
                connection.close()
            if client:
                client.close()
            for path in runtime.root.glob("*.log"):
                shutil.copy2(path, options.artifacts_dir / path.name)
            for name in ("widget-report", "inherited-fds.json", "WidgetMathProbe"):
                path = runtime.root / name
                if path.exists():
                    shutil.copy2(path, options.artifacts_dir / name)
            (options.artifacts_dir / "results.json").write_text(json.dumps(evidence, indent=2)+"\n")
    print("FvwmScript widget arithmetic, pixels and input completion passed")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(allow_abbrev=False)
    for name in ("fvwm", "mfl", "script", "artifacts-dir"):
        parser.add_argument("--"+name, type=Path, required=True)
    parser.add_argument("--display")
    parser.add_argument("--legacy-control", action="store_true")
    args = parser.parse_args()
    for name in ("fvwm", "mfl", "script", "artifacts_dir"):
        setattr(args, name, getattr(args, name).resolve())
    run(args)
