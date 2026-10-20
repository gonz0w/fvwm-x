#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
"""Measure end-to-end module command latency under controlled real X load.

Virtualized CPU timings are evidence about this runtime only. A five-second
socket deadline detects starvation, without gating on performance percentiles.
Workers use separate processes and X connections.
"""

import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import shutil
import signal
import socket
import statistics
import sys
import tempfile
import time

from geometry_transition_live import Client
from live_x_harness import (
    HarnessError, HarnessRuntime, start_display_server, wait_for_predicate,
)


def workload(args):
    client = Client(args.display)
    running = True

    def stop(signum, frame):
        nonlocal running
        running = False

    signal.signal(signal.SIGTERM, stop)
    count = 0
    start = time.monotonic()
    print(json.dumps({"ready": True}), flush=True)
    try:
        while running:
            for _ in range(64):
                if args.mode == "title":
                    client.x.XStoreName(client.display, args.window,
                                        f"latency-load-{count}".encode())
                else:
                    client.x.XResizeWindow(client.display, args.window,
                                           200 + count % 2, 120 + count % 3)
                count += 1
            # Bound outstanding requests and include actual server work.
            client.x.XSync(client.display, 0)
            delay = start + count / args.rate - time.monotonic()
            if delay > 0:
                time.sleep(min(delay, 0.1))
    finally:
        elapsed = time.monotonic() - start
        client.close()
        print(json.dumps({"requests": count, "elapsed_seconds": elapsed,
                          "requests_per_second": count / elapsed}), flush=True)


def cpu_seconds(pid):
    if not Path(f"/proc/{pid}/stat").exists():
        return None
    fields = Path(f"/proc/{pid}/stat").read_text().split(")", 1)[1].split()
    return (int(fields[11]) + int(fields[12])) / os.sysconf("SC_CLK_TCK")


def peak_rss_kib(pid):
    path = Path(f"/proc/{pid}/status")
    if not path.exists():
        return None
    for line in path.read_text().splitlines():
        fields = line.split()
        if len(fields) == 3 and fields[0] == "VmHWM:" and fields[2] == "kB":
            return int(fields[1])
    return None


def summary(samples):
    ordered = sorted(samples)
    return {"samples": len(samples), "median_ms": statistics.median(samples),
            "p95_ms": ordered[int((len(ordered) - 1) * .95)],
            "max_ms": max(samples), "mean_ms": statistics.mean(samples)}


def finish_profile(runtime, client, result, artifacts):
    """Stop owned children before copying their final log records."""
    errors = []
    if client:
        try:
            client.close()
        except Exception as error:
            errors.append(str(error))
    for process in reversed(runtime.processes):
        try:
            process.close(time.monotonic() + 2)
        except Exception as error:
            errors.append(f"{process.name}: {error}")
    try:
        for path in runtime.root.glob("*.log"):
            shutil.copyfile(path, artifacts / path.name)
    finally:
        try:
            runtime.close()
        except Exception as error:
            errors.append(str(error))
        if errors:
            result["teardown_errors"] = errors
        (artifacts / "profile.json").write_text(
            json.dumps(result, indent=2) + "\n")
    if errors and "error" not in result:
        raise HarnessError("profiling teardown failed: " + "; ".join(errors))


def profile(args):
    runtime = HarnessRuntime(prefix="fvwm-latency-")
    args.artifacts_dir.mkdir(parents=True, exist_ok=True)
    artifacts = Path(tempfile.mkdtemp(prefix="run-", dir=args.artifacts_dir))
    result = {"platform": platform.platform(), "architecture": platform.machine(),
              "rate": args.rate, "cases": [], "fvwm": str(args.fvwm),
              "wm_logging": False, "samples_per_case": args.samples,
              "selected_cases": args.cases}
    client = None
    try:
        result["file_sha256"] = {
            str(path): hashlib.sha256(path.read_bytes()).hexdigest()
            for path in (args.fvwm, args.mfl, Path(__file__).resolve())}
        display = args.display
        if not display:
            _, display = start_display_server(
                runtime, "Xvfb", ["Xvfb", "-screen", "0", "1280x720x24", "-noreset"],
                "xserver.log", time.monotonic() + 10)
        result.update(display=display, server_scope=(
            "provided display" if args.display else "owned Xvfb 1280x720x24"))
        process = runtime.start(
            "fvwm", [str(args.fvwm), "-d", display,
                     "-c", f"ModulePath {args.mfl.parent}",
                     "-c", "AddToFunc StartFunction I Module FvwmMFL",
                     "-c", "Style * TileCascadePlacement",
                     "-c", "DefaultFont xft:DejaVu Sans:size=10"],
            "process.log", runtime.environment({"FVWMMFL_SOCKET_PATH": str(runtime.root)}))
        endpoint = runtime.root / f"fvwm_mfl_{display.replace(':', '_')}.sock"
        wait_for_predicate("MFL socket", endpoint.exists, time.monotonic() + 10,
                           process=process)
        client = Client(display)
        client.create()
        wait_for_predicate("managed load client", client.frame, time.monotonic() + 10,
                           process=process)
        with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as connection:
            connection.settimeout(5)
            connection.connect(str(endpoint))
            connection.sendall(b"set echo\n")
            with connection.makefile("rb") as responses:
                sequence = 0

                def ping(scheduled=False):
                    nonlocal sequence
                    sequence += 1
                    marker = f"latency-{sequence}"
                    command = f"Echo {marker}"
                    if scheduled:
                        command = f"Schedule 10 {command}"
                    start = time.monotonic_ns()
                    connection.sendall((command + "\n").encode())
                    while True:
                        line = responses.readline()
                        if not line:
                            raise HarnessError("MFL closed during profiling")
                        message = json.loads(line)
                        if message.get("echo", {}).get("message") == marker:
                            return (time.monotonic_ns() - start) / 1_000_000

                for mode in args.cases:
                    worker = None
                    case = {"mode": mode}
                    result["cases"].append(case)
                    try:
                        if mode != "idle":
                            worker = runtime.start(
                                f"load-{mode}", [sys.executable, str(Path(__file__).resolve()),
                                                "--worker", "--display", display,
                                                "--window", str(client.window), "--mode", mode,
                                                "--rate", str(args.rate)],
                                f"load-{mode}.log")
                            wait_for_predicate(
                                f"{mode} worker ready",
                                lambda: '"ready": true' in worker.log_path.read_text(),
                                time.monotonic() + 10, process=worker)
                        for _ in range(8):
                            ping()
                        start = time.monotonic()
                        cpu_start = cpu_seconds(process.process.pid)
                        commands, timers = [], []
                        case.update(command_samples_ms=commands,
                                    timer_samples_ms=timers)
                        for _ in range(args.samples):
                            commands.append(ping())
                            timers.append(ping(scheduled=True))
                        elapsed = time.monotonic() - start
                        cpu_end = cpu_seconds(process.process.pid)
                        cpu_percent = None if cpu_start is None else (
                            100 * (cpu_end - cpu_start) / elapsed)
                        case.update(commands=summary(commands),
                                    scheduled_10ms=summary(timers),
                                    wm_cpu_percent=cpu_percent,
                                    wm_peak_rss_kib=peak_rss_kib(process.process.pid),
                                    elapsed_seconds=elapsed,
                                    command_samples_ms=commands, timer_samples_ms=timers)
                        print(json.dumps({key: value for key, value in case.items()
                                          if not key.endswith("_samples_ms")}), flush=True)
                    finally:
                        if worker:
                            worker.close(time.monotonic() + 3)
                            if worker.poll() != 0:
                                raise HarnessError(f"{mode} worker failed")
                            case["workload"] = json.loads(
                                worker.log_path.read_text().splitlines()[-1])
    except BaseException as error:
        result["error"] = str(error)
        raise
    finally:
        finish_profile(runtime, client, result, artifacts)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--worker", action="store_true")
    parser.add_argument("--display")
    parser.add_argument("--window", type=int)
    parser.add_argument("--mode", choices=("title", "resize"))
    parser.add_argument("--rate", type=int, default=10000)
    parser.add_argument("--samples", type=int, default=64)
    parser.add_argument("--cases", nargs="+", choices=("idle", "title", "resize"),
                        default=["idle", "title", "resize"],
                        help="workloads to measure, in the requested order")
    parser.add_argument("--fvwm", type=Path)
    parser.add_argument("--mfl", type=Path)
    parser.add_argument("--artifacts-dir", type=Path)
    args = parser.parse_args()
    if args.rate < 1 or args.samples < 2:
        parser.error("rate must be positive and samples must be at least two")
    if args.worker:
        if not args.display or not args.window or not args.mode:
            parser.error("worker requires display, window, and mode")
        workload(args)
    else:
        for name in ("fvwm", "mfl", "artifacts_dir"):
            if getattr(args, name) is None:
                parser.error(f"{name} is required")
        args.fvwm, args.mfl = args.fvwm.resolve(), args.mfl.resolve()
        profile(args)


if __name__ == "__main__":
    main()
