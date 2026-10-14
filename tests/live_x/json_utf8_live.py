#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.

"""Check JSON presentation without changing legacy command bytes."""

import argparse
import json
from pathlib import Path
import shutil
import time

from live_x_harness import HarnessRuntime, start_display_server, wait_for_predicate
from mfl_stream_live import Connection


def run(options):
    out = options.artifacts_dir.resolve()
    out.mkdir(parents=True, exist_ok=True)
    with HarnessRuntime(prefix="fvwm-json-utf8-") as runtime:
        connection = None
        try:
            display = options.display
            if display is None:
                _, display = start_display_server(
                    runtime, "Xvfb", ["Xvfb", "-screen", "0", "800x600x24",
                                      "-noreset"], "xserver.log",
                    time.monotonic() + 10)
            core = runtime.start(
                "core", [str(options.fvwm), "-d", display, "-f", "/dev/null",
                         "-c", "ModulePath " + str(options.mfl.parent),
                         "-c", "AddToFunc StartFunction I Module FvwmMFL"],
                "core.log", runtime.environment(
                    {"FVWMMFL_SOCKET_PATH": str(runtime.root)}))
            endpoint = runtime.root / ("fvwm_mfl_" + display.replace(":", "_")
                                       + ".sock")
            wait_for_predicate("MFL endpoint", endpoint.exists,
                               time.monotonic() + 10, process=core)
            connection = Connection(endpoint)
            assert "connection_profile" in connection.record()
            connection.send(b"set echo\n")
            connection.echo("ready")
            cases = [
                (b"legacy-\xff", "legacy-\ufffd"),
                (b"caf\xc3\xa9-\xf0\x9f\x98\x80", "caf\u00e9-\U0001f600"),
                (b"cut-\xe2\x82", "cut-\ufffd\ufffd"),
                (b"surrogate-\xed\xa0\x80", "surrogate-\ufffd\ufffd\ufffd"),
                (b"overlong-\xc0\xaf", "overlong-\ufffd\ufffd"),
                (b"large-\xf4\x90\x80\x80", "large-" + "\ufffd" * 4),
                (b"mixed-\xffcaf\xc3\xa9", "mixed-\ufffdcaf\u00e9"),
            ]
            evidence = {}
            for raw, expected in cases:
                # Store and expand using the command engine before presentation.
                connection.send(b"SetEnv FVWM_JSON_BOUNDARY " + raw + b"\n"
                                b"Echo $[FVWM_JSON_BOUNDARY]\n")
                connection.expect_echo(expected)
                evidence[raw.hex()] = expected
            connection.echo("healthy-after-legacy-bytes")
            assert core.poll() is None
            (out / "evidence.json").write_text(json.dumps({
                "cases": evidence, "strict_json_decode": "passed",
                "command_storage_and_expansion": "passed",
                "subsequent_echo": "passed"}, indent=2) + "\n")
        finally:
            if connection:
                connection.close()
            for log in runtime.root.glob("*.log"):
                shutil.copy2(log, out / log.name)


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--fvwm", required=True, type=Path)
    parser.add_argument("--mfl", required=True, type=Path)
    parser.add_argument("--display")
    parser.add_argument("--artifacts-dir", required=True, type=Path)
    run(parser.parse_args())
