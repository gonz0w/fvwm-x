#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
"""Compare already built checkpoints without rebuilding or running a test matrix.

This is an opt-in observation tool, not a timing acceptance gate. Preserve the
output directory with the source/build receipts that identify both revisions.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import shutil
import statistics
import subprocess
import sys
import time

sys.path.insert(0, str(Path(__file__).resolve().parent / "live_x"))
from live_x_harness import HarnessRuntime


def run_profile(command, log_path, timeout=900):
    # TERM lets the profiler's HarnessRuntime unwind evidence and owned X
    # children before the existing bounded session-kill fallback is needed.
    with HarnessRuntime(prefix="fvwm-comparison-") as runtime:
        process = runtime.start("performance-profile", command, "profile.log",
                                dict(os.environ))
        try:
            return process.wait_until_exit(time.monotonic() + timeout)
        finally:
            try:
                process.close(time.monotonic() + 15)
            finally:
                shutil.copyfile(process.log_path, log_path)


def identity(path):
    return {"path": str(path), "bytes": path.stat().st_size,
            "sha256": hashlib.sha256(path.read_bytes()).hexdigest()}


def binary_size(path):
    tool = shutil.which("size")
    if tool is None:
        return {"available": False, "reason": "size utility unavailable"}
    result = subprocess.run([tool, str(path)], capture_output=True, text=True,
                            timeout=15)
    record = {"command": [tool, str(path)], "status": result.returncode,
              "stdout": result.stdout, "stderr": result.stderr}
    # GNU and BSD size default to text/data/bss, excluding debug sections.
    rows = result.stdout.splitlines()
    if result.returncode == 0 and len(rows) == 2:
        values = rows[1].split()
        if len(values) >= 3 and all(v.isdecimal() for v in values[:3]):
            record.update(zip(("text_bytes", "data_bytes", "bss_bytes"),
                              map(int, values[:3])))
    return record


def summarize(runs):
    result = {}
    for mode in ("idle", "title", "resize"):
        cases = [next(c for c in run["cases"] if c["mode"] == mode)
                 for run in runs]
        row = {}
        for key in ("commands", "scheduled_10ms"):
            medians = [c[key]["median_ms"] for c in cases]
            row[key] = {"median_ms": statistics.median(medians),
                        "median_range_ms": [min(medians), max(medians)],
                        "p95_ms": statistics.median(c[key]["p95_ms"]
                                                    for c in cases)}
        for key in ("wm_cpu_percent", "wm_peak_rss_kib"):
            values = [c[key] for c in cases if c.get(key) is not None]
            row[key] = statistics.median(values) if values else None
        if mode != "idle":
            row["achieved_requests_per_second"] = statistics.median(
                c["workload"]["requests_per_second"] for c in cases)
        result[mode] = row
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for side in ("baseline", "candidate"):
        parser.add_argument(f"--{side}-fvwm", type=Path, required=True)
        parser.add_argument(f"--{side}-mfl", type=Path, required=True)
        parser.add_argument(f"--{side}-revision", required=True)
    parser.add_argument("--profile", required=True,
                        help="target, compiler and feature profile identity")
    parser.add_argument("--artifacts-dir", type=Path, required=True,
                        help="new directory; previous observations are preserved")
    parser.add_argument("--samples", type=int, default=128)
    parser.add_argument("--repetitions", type=int, default=3)
    parser.add_argument("--rate", type=int, default=10000)
    args = parser.parse_args()
    if not 2 <= args.samples <= 4096 or not 1 <= args.repetitions <= 9:
        parser.error("samples must be 2..4096 and repetitions 1..9")
    if not 1 <= args.rate <= 1000000:
        parser.error("rate must be 1..1000000")
    profiler = Path(__file__).resolve().parent / "live_x/responsiveness_profile.py"
    binaries = {side: {name: getattr(args, f"{side}_{name}").resolve(strict=True)
                       for name in ("fvwm", "mfl")}
                for side in ("baseline", "candidate")}
    args.artifacts_dir.mkdir(parents=True, exist_ok=False)
    record = {"status": "incomplete", "profile": args.profile,
              "platform": platform.platform(), "architecture": platform.machine(),
              "samples": args.samples, "repetitions": args.repetitions,
              "requested_rate": args.rate, "runs": [], "checkpoints": {},
              "measurement_scope": "same-host owned Xvfb; no rebuild or timing gate"}
    record["profiler_inputs"] = [identity(profiler.parent / name) for name in
        ("responsiveness_profile.py", "geometry_transition_live.py",
         "live_x_harness.py")]
    collected = {side: [] for side in binaries}
    try:
        for side, paths in binaries.items():
            record["checkpoints"][side] = {
                "revision": getattr(args, f"{side}_revision"),
                "binaries": {name: identity(path) for name, path in paths.items()},
                "wm_size": binary_size(paths["fvwm"])}
        for repetition in range(args.repetitions):
            order = ("baseline", "candidate") if repetition % 2 == 0 else (
                "candidate", "baseline")
            for side in order:
                output = args.artifacts_dir / f"{repetition + 1}-{side}"
                command = [sys.executable, str(profiler),
                    "--fvwm", str(binaries[side]["fvwm"]),
                    "--mfl", str(binaries[side]["mfl"]),
                    "--samples", str(args.samples), "--rate", str(args.rate),
                    "--artifacts-dir", str(output)]
                status = run_profile(command, args.artifacts_dir /
                                     f"{output.name}.log")
                record["runs"].append({"side": side, "repetition": repetition + 1,
                                       "command": command, "status": status})
                if status != 0:
                    raise RuntimeError(f"{output.name} failed; preserve its logs")
                profiles = list(output.glob("run-*/profile.json"))
                if len(profiles) != 1:
                    raise RuntimeError("Expected exactly one profile per run")
                profile = json.loads(profiles[0].read_text())
                if profile.get("error") or profile.get("teardown_errors"):
                    raise RuntimeError("Incomplete profile; preserve its logs")
                if (profile["platform"] != record["platform"] or
                    profile["architecture"] != record["architecture"] or
                    profile["rate"] != args.rate or
                    profile["samples_per_case"] != args.samples or
                    profile["server_scope"] != "owned Xvfb 1280x720x24"):
                    raise RuntimeError("Profile environment differs from comparison")
                collected[side].append(profile)
        record["summary"] = {side: summarize(runs)
                             for side, runs in collected.items()}
        record["status"] = "observed"
    except BaseException as error:
        record["error"] = str(error)
        raise
    finally:
        (args.artifacts_dir / "comparison.json").write_text(
            json.dumps(record, indent=2) + "\n")
    print(json.dumps(record["summary"], indent=2))


if __name__ == "__main__":
    main()
