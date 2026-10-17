#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.

"""Validate bounded properties and descriptor/lock ownership on real X."""

import argparse
import ctypes as C
import json
import os
from pathlib import Path
import shutil
import signal
import subprocess
import time

from geometry_transition_live import Client
from mfl_stream_live import Connection
from live_x_harness import HarnessRuntime, start_display_server, wait_for_predicate


def run(options):
    runtime = HarnessRuntime(prefix="fvwm-hardening-")
    evidence = {}
    client = None
    connection = None
    artifact = options.artifacts_dir.resolve()
    artifact.mkdir(parents=True, exist_ok=True)
    try:
        display = options.display
        if display is None:
            _, display = start_display_server(
                runtime, "Xvfb", ["Xvfb", "-screen", "0", "800x600x24", "-noreset"],
                "xserver.log", time.monotonic() + 10)
        log = runtime.root / "core.log"
        # Rejected lockfiles must not prevent the core managing windows.
        victim = runtime.root / "victim"
        victim.write_text("unchanged\n")
        lock = runtime.root / f"fvwmmfl_lockfile_{display.replace(':', '_')}.lock"
        lock.symlink_to(victim)
        process = runtime.start(
            "fvwm", [str(options.fvwm), "-d", display, "-v", "-o", str(log),
                     "-c", f"ModulePath {options.mfl.parent}:{options.module_probe.parent}:{runtime.root}",
                     "-c", "AddToFunc StartFunction I Module FvwmMFL",
                     "-c", "AddToFunc StartFunction I Schedule 5000 Module FvwmMFL",
                     "-c", "Style * TileCascadePlacement"],
            "fvwm.log", runtime.environment({"FVWMMFL_SOCKET_PATH": str(runtime.root)}))
        endpoint = runtime.root / f"fvwm_mfl_{display.replace(':', '_')}.sock"

        def connect_ready():
            nonlocal connection
            try:
                connection = Connection(endpoint)
                return True
            except (FileNotFoundError, ConnectionRefusedError):
                # bind() publishes the pathname before listen() is ready.
                # Connection closes failed sockets; keep the existing bound.
                return False
        client = Client(display)
        client.create()
        wait_for_predicate("managed window despite rejected lock",
                           lambda: client.frame() is not None,
                           time.monotonic() + 7, process=process)
        assert not endpoint.exists(), "MFL accepted a symlink lockfile"
        assert victim.read_text() == "unchanged\n"
        lock.unlink()
        wait_for_predicate("MFL retry with private lock", connect_ready,
                           time.monotonic() + 7, process=process)
        connection.record()
        connection.send(b"set echo\n")
        connection.echo("hardening-ready")
        evidence["lock_symlink_rejected_and_retry"] = "passed"

        probe = runtime.start("property-probe", [str(options.property_probe)],
                              "property-probe.log",
                              runtime.environment({"DISPLAY": display}))
        assert probe.wait_until_exit(time.monotonic() + 10) == 0
        evidence["xlib_packed_property_reader"] = "passed"

        x = client.x
        x.XInternAtom.argtypes = [C.c_void_p, C.c_char_p, C.c_int]
        x.XInternAtom.restype = C.c_ulong
        x.XChangeProperty.argtypes = [C.c_void_p, C.c_ulong, C.c_ulong,
                                     C.c_ulong, C.c_int, C.c_int, C.c_void_p, C.c_int]
        x.XChangeProperty.restype = C.c_int
        atom = lambda name: x.XInternAtom(client.display, name.encode(), 0)

        def property_value(name, kind, fmt, values, count):
            x.XChangeProperty(client.display, client.window, atom(name),
                              atom(kind), fmt, 0, values, count)
            x.XSync(client.display, 0)

        def rejection(name, kind, fmt, values, count, label, diagnostic=None):
            diagnostic = diagnostic or f"Ignoring invalid property {name}"
            before = log.read_text().count(diagnostic)
            property_value(name, kind, fmt, values, count)
            wait_for_predicate(label,
                               lambda: log.read_text().count(
                                   diagnostic) > before,
                               time.monotonic() + 5, process=process)
            connection.echo(label)

        rejection("_NET_WM_STRUT", "CARDINAL", 32,
                  (C.c_ulong * 1)(999), 1, "short-strut-rejected")
        rejection("_NET_WM_STRUT", "CARDINAL", 8,
                  C.create_string_buffer(b"x" * 16), 16, "wrong-format-rejected")
        # ICCCM names are bounded in every build. EWMH names are deliberately
        # ignored before reading their properties when iconv support is absent.
        rejection("WM_NAME", "STRING", 8,
                  C.create_string_buffer(b"x" * 65537), 65537,
                  "oversized-icccm-name-rejected",
                  f"Ignoring invalid text property {atom('WM_NAME')}")
        property_value("WM_NAME", "STRING", 8,
                       C.create_string_buffer(b"valid-title"), 11)
        if not options.without_ewmh_names:
            rejection("_NET_WM_NAME", "UTF8_STRING", 8,
                      C.create_string_buffer(b"x" * 65537), 65537, "oversized-name-rejected")
            rejection("_NET_WM_NAME", "CARDINAL", 32,
                      (C.c_ulong * 1)(10), 1, "wrong-type-rejected")
            property_value("_NET_WM_NAME", "UTF8_STRING", 8,
                           C.create_string_buffer(b"valid-title"), 11)
        evidence["ewmh_name_properties_enabled"] = not options.without_ewmh_names
        property_value("_NET_WM_STRUT", "CARDINAL", 32,
                       (C.c_ulong * 4)(0, 0, 0, 0), 4)
        connection.echo("healthy-after-valid-properties")
        assert client.frame() is not None
        evidence["malformed_properties_isolated"] = "passed"

        for command in ("Module", "ModuleListenOnly"):
            report = runtime.root / f"{command}-fds.txt"
            connection.send(
                f"{command} {options.module_probe.name} {report} {log}\n".encode())
            wait_for_predicate("module descriptor report",
                               lambda: report.exists() and report.stat().st_size > 0,
                               time.monotonic() + 5, process=process)
            assert report.read_text().strip() == "0", report.read_text()
            connection.echo(f"healthy-after-{command}")
        evidence["only_module_and_shared_log_descriptors_inherited"] = "passed"

        failed = runtime.root / "fail-exec-module"
        failed.write_text("#!/fvwm-test-nonexistent-interpreter\n")
        failed.chmod(0o700)
        connection.send(b"Module fail-exec-module\n")
        wait_for_predicate("failed module exec",
                           lambda: "Execution of module failed" in log.read_text(),
                           time.monotonic() + 5, process=process)
        connection.echo("healthy-after-failed-exec")
        assert process.poll() is None
        evidence["failed_exec_core_survives"] = "passed"

        inode = lock.stat().st_ino
        before = (runtime.root / "fvwm.log").read_text().count("FvwmMFL lock:")
        connection.send(b"Module FvwmMFL\n")
        wait_for_predicate("duplicate module rejected",
                           lambda: (runtime.root / "fvwm.log").read_text().count(
                               "FvwmMFL lock:") > before,
                           time.monotonic() + 5, process=process)
        connection.echo("healthy-after-duplicate-module")
        assert lock.stat().st_ino == inode
        connection.send(b"Schedule 1000 Module FvwmMFL\nKillModule FvwmMFL\n")
        connection.expect_closed()
        connection.close()
        connection = None
        wait_for_predicate("module socket removed", lambda: not endpoint.exists(),
                           time.monotonic() + 5, process=process)
        assert lock.exists() and lock.stat().st_ino == inode
        wait_for_predicate("module restart", connect_ready,
                           time.monotonic() + 7, process=process)
        connection.record()
        connection.send(b"set echo\n")
        connection.echo("healthy-after-module-restart")
        assert lock.stat().st_ino == inode
        evidence["duplicate_rejected_restart_retains_lock_inode"] = "passed"

        # Signal the owned child directly, without closing its core pipe.
        children = subprocess.run(["ps", "-ax", "-o", "pid=", "-o", "ppid=",
                                   "-o", "comm="],
                                  check=True, capture_output=True, text=True)
        (runtime.root / "processes.log").write_text(children.stdout)
        pids = [int(fields[0]) for line in children.stdout.splitlines()
                if len(fields := line.split(maxsplit=2)) == 3
                and int(fields[1]) == process.process.pid
                and Path(fields[2]).name == options.mfl.name]
        assert len(pids) == 1, pids
        connection.send(b"Schedule 1000 Module FvwmMFL\n")
        connection.echo("direct-signal-restart-scheduled")
        os.kill(pids[0], signal.SIGTERM)
        connection.expect_closed()
        connection.close()
        connection = None
        wait_for_predicate("signal removes socket", lambda: not endpoint.exists(),
                           time.monotonic() + 5, process=process)
        assert lock.stat().st_ino == inode
        wait_for_predicate("restart after direct signal", connect_ready,
                           time.monotonic() + 7, process=process)
        connection.record()
        connection.send(b"set echo\n")
        connection.echo("healthy-after-direct-signal-restart")
        evidence["direct_sigterm_exits_and_restart_succeeds"] = "passed"
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
    for name in ("fvwm", "mfl", "module-probe", "property-probe", "artifacts-dir"):
        parser.add_argument(f"--{name}", required=True, type=Path)
    parser.add_argument("--display")
    parser.add_argument("--without-ewmh-names", action="store_true",
                        help="EWMH names are unavailable in a build without iconv")
    run(parser.parse_args())


if __name__ == "__main__":
    main()
