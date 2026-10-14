#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.

"""Exercise real ICE authentication, literal paths and descriptor inheritance."""

import argparse
import json
import os
from pathlib import Path
import shutil
import signal
import socket
import sys
import time

from geometry_transition_live import Client
from mfl_stream_live import Connection
from live_x_harness import HarnessRuntime, start_display_server, wait_for_predicate


def run(options):
    runtime = HarnessRuntime(prefix="fvwm-session-")
    client = connection = None
    artifact = options.artifacts_dir.resolve()
    artifact.mkdir(parents=True, exist_ok=True)
    evidence = {}
    try:
        display = options.display
        if display is None:
            _, display = start_display_server(
                runtime, "Xvfb", ["Xvfb", "-screen", "0", "800x600x24", "-noreset"],
                "xserver.log", time.monotonic() + 10)
        iceauth = shutil.which("iceauth")
        assert iceauth, "iceauth is required for the session integration test"
        # A shell would interpret these characters; exec must pass them literally.
        save_dir = runtime.root / "save ' ; $(touch UNEXPECTED)"
        save_dir.mkdir()
        authority = runtime.root / "ICEauthority"
        calls = runtime.root / "iceauth-calls.jsonl"
        bin_dir = runtime.root / "bin"
        bin_dir.mkdir()
        wrapper = bin_dir / "iceauth"
        wrapper.write_text(f"#!{sys.executable}\n" + '''
import fcntl, json, os, sys
leaks = []
for fd in range(3, 1024):
    try:
        fcntl.fcntl(fd, fcntl.F_GETFD)
        leaks.append(fd)
    except OSError:
        pass
with open(os.environ['FVWM_ICEAUTH_CALLS'], 'a') as output:
    output.write(json.dumps({'argv': sys.argv[1:], 'leaks': leaks}) + '\\n')
os.execv(os.environ['FVWM_REAL_ICEAUTH'], ['iceauth'] + sys.argv[1:])
''')
        wrapper.chmod(0o700)
        environment = runtime.environment({
            "DISPLAY": display, "SM_SAVE_DIR": str(save_dir),
            "ICEAUTHORITY": str(authority), "FVWM_ICEAUTH_CALLS": str(calls),
            "FVWM_REAL_ICEAUTH": iceauth,
            "PATH": str(bin_dir) + os.pathsep + os.environ["PATH"],
        })
        manager_file = runtime.root / "manager.txt"
        server = runtime.start("session-probe", [str(options.session_probe),
                                                str(manager_file)],
                               "session-probe.log", environment)
        wait_for_predicate("authenticated session listener",
                           lambda: manager_file.exists() and manager_file.stat().st_size > 0,
                           time.monotonic() + 10, process=server)
        manager = manager_file.read_text().strip()
        assert ("session-watch-range-and-borrowed-close: passed"
                in server.log_path.read_text())
        evidence["watched_descriptor_boundaries_and_cleanup"] = "passed"
        unix_addresses = [item.split(":", 1)[1] for item in manager.split(",")
                          if item.startswith(("unix/", "local/"))]
        assert unix_addresses, manager
        # Disconnect before protocol registration as well as after a valid session.
        for _ in range(3):
            with socket.socket(socket.AF_UNIX) as pending:
                address = unix_addresses[0]
                pending.connect("\0" + address[1:] if address.startswith("@") else address)
            time.sleep(0.03)
            assert server.poll() is None, "early ICE disconnect crashed the listener"
        evidence["early_ice_disconnects_survive"] = "passed"
        log = runtime.root / "core.log"
        environment.update({"SESSION_MANAGER": manager,
                            "FVWMMFL_SOCKET_PATH": str(runtime.root)})
        process = runtime.start(
            "fvwm", [str(options.fvwm), "-d", display, "-v", "-o", str(log),
                     "-c", f"ModulePath {options.mfl.parent}:{options.module_probe.parent}",
                     "-c", "AddToFunc SessionInitFunction I Echo session-connected",
                     "-c", "AddToFunc StartFunction I Module FvwmMFL",
                     "-c", "Style * TileCascadePlacement"],
            "fvwm.log", environment)
        wait_for_predicate("real XSMP registration",
                           lambda: log.exists() and "session-connected" in log.read_text(),
                           time.monotonic() + 10, process=process)
        endpoint = runtime.root / f"fvwm_mfl_{display.replace(':', '_')}.sock"
        wait_for_predicate("MFL endpoint", endpoint.exists,
                           time.monotonic() + 10, process=process)
        connection = Connection(endpoint)
        connection.record()
        connection.send(b"set echo\n")
        client = Client(display)
        client.create()
        wait_for_predicate("managed session client", client.frame,
                           time.monotonic() + 5, process=process)
        report = runtime.root / "module-fds.txt"
        connection.send(f"Module {options.module_probe.name} {report} {log}\n".encode())
        wait_for_predicate("session descriptor inheritance",
                           lambda: report.exists() and report.stat().st_size > 0,
                           time.monotonic() + 5, process=process)
        assert report.read_text().strip() == "0", report.read_text()
        connection.echo("healthy-with-session")
        evidence["real_session_registration_and_no_module_fd_leaks"] = "passed"
        connection.send(b"Quit\n")
        assert process.wait_until_exit(time.monotonic() + 5) == 0
        os.kill(server.process.pid, signal.SIGTERM)
        assert server.wait_until_exit(time.monotonic() + 5) == 0, server.poll()
        assert ("session-repeated-authentication-cleanup: passed"
                in server.log_path.read_text())
        records = [json.loads(line) for line in calls.read_text().splitlines()]
        assert len(records) == 2, records
        for record in records:
            assert len(record["argv"]) == 2 and record["argv"][0] == "source"
            assert Path(record["argv"][1]).parent == save_dir
            assert record["leaks"] == [], record
        assert not list(save_dir.iterdir()), "temporary authentication files survived cleanup"
        assert not (runtime.root / "UNEXPECTED").exists()
        evidence["literal_authentication_paths_no_exec_fd_leaks_and_cleanup"] = "passed"
        shutil.copy2(calls, artifact / calls.name)
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
    for name in ("fvwm", "mfl", "session-probe", "module-probe", "artifacts-dir"):
        parser.add_argument(f"--{name}", required=True, type=Path)
    parser.add_argument("--display")
    run(parser.parse_args())


if __name__ == "__main__":
    main()
