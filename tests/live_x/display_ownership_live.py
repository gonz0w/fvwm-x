#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or any later version.

"""Check overlapping owned X servers and independent teardown."""

import argparse
import json
from pathlib import Path
import shutil
import sys
import time

from geometry_transition_live import Client
from live_x_harness import (HarnessRuntime, start_display_server,
                           wait_for_predicate, wait_for_randr)


def run(artifact):
    artifact.mkdir(parents=True, exist_ok=True)
    runtimes = [HarnessRuntime(prefix="fvwm-display-owner-") for _ in range(2)]
    displays = []
    evidence = {}
    try:
        for index in range(3):
            runtime = runtimes[0 if index < 2 else 1]
            server, display = start_display_server(
                runtime, "Xvfb", ["Xvfb", "-screen", "0", "80x60x24", "-noreset"],
                "server-" + str(index) + ".log", time.monotonic() + 10)
            assert display not in [entry["display"] for entry in displays]
            client = Client(display)
            client.close()
            displays.append({"display": display, "pid": server.process.pid})
        evidence["three_independent_overlapping_displays"] = "passed"
        for log in runtimes[0].root.glob("*.log"):
            shutil.copy2(log, artifact / ("0-" + log.name))
        runtimes[0].close()
        client = Client(displays[2]["display"])
        client.close()
        evidence["other_runtime_survives_owned_teardown"] = "passed"
        if sys.platform.startswith("openbsd"):
            class RacingRuntime(HarnessRuntime):
                raced = False
                rival_display = None

                def start(self, name, argv, log_name, environment=None,
                          pass_fds=()):
                    if (Path(argv[0]).name == "Xvfb" and not self.raced):
                        self.raced = True
                        self.rival_display = argv[1]
                        rival = runtimes[1].start(
                            "owned-rival", argv, "rival.log", environment)
                        lock = Path("/tmp/.X" + argv[1][1:] + "-lock")

                        def rival_owns_lock():
                            try:
                                return int(lock.read_text()) == rival.process.pid
                            except (OSError, ValueError):
                                return False

                        deadline = time.monotonic() + 10
                        wait_for_predicate("owned rival lock", rival_owns_lock,
                                           deadline, process=rival)
                        wait_for_randr(runtimes[1], argv[1], deadline)
                    return super().start(name, argv, log_name, environment,
                                         pass_fds)

            race = RacingRuntime(prefix="fvwm-display-race-")
            runtimes.append(race)
            _, display = start_display_server(
                race, "Xvfb", ["Xvfb", "-screen", "0", "80x60x24", "-noreset"],
                "race.log", time.monotonic() + 10)
            assert race.raced and display != race.rival_display
            diagnostics = list(race.root.glob("race-occupied-*.log"))
            assert diagnostics and all(log.stat().st_size for log in diagnostics)
            for log in race.root.glob("*.log"):
                shutil.copy2(log, artifact / ("2-" + log.name))
            race.close()
            client = Client(race.rival_display)
            client.close()
            evidence["raced_slot_refusal_preserves_other_owner"] = "passed"
    finally:
        for index, runtime in enumerate(runtimes):
            for log in runtime.root.glob("*.log"):
                shutil.copy2(log, artifact / (str(index) + "-" + log.name))
            runtime.close()
        (artifact / "evidence.json").write_text(json.dumps(evidence, indent=2) + "\n")
        (artifact / "display-identities.json").write_text(json.dumps(displays, indent=2) + "\n")
    print(json.dumps(evidence))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--artifacts-dir", type=Path, required=True)
    run(parser.parse_args().artifacts_dir.resolve())
