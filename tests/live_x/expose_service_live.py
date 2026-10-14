#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.

"""Verify actual-core bounded redraw delivery and continuation."""
import argparse
import json
from pathlib import Path
import shutil
import time
from live_x_harness import HarnessRuntime, start_display_server


def run(args):
    out = args.artifacts_dir.resolve()
    out.mkdir(parents=True, exist_ok=True)
    evidence = {}
    for partial in (False,):
        label = "partial" if partial else "normal"
        with HarnessRuntime(prefix="fvwm-expose-service-") as runtime:
            try:
                display = args.display
                if display is None:
                    _, display = start_display_server(
                        runtime, "Xvfb", ["Xvfb", "-screen", "0", "800x600x24",
                                          "-noreset"], "server.log",
                        time.monotonic() + 10)
                process = runtime.start(
                    "core", [str(args.fvwm), "-d", display, "-f", "/dev/null"],
                    "core.log", runtime.environment(
                        {}))
                assert process.wait_until_exit(time.monotonic() + 15) == 0
                text = process.log_path.read_text()
                assert "expose-service:" in text
                evidence[label] = text.strip().splitlines()
            finally:
                for log in runtime.root.glob("*.log"):
                    shutil.copy2(log, out / (label + "-" + log.name))
    (out / "evidence.json").write_text(json.dumps(evidence, indent=2) + "\n")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--fvwm", type=Path, required=True)
    parser.add_argument("--artifacts-dir", type=Path, required=True)
    parser.add_argument("--display")
    run(parser.parse_args())
