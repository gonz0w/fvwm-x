#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
"""Opt-in benchmark process controls; no WM, server or performance threshold."""
import json
import os
from pathlib import Path
import sys
import tempfile
import time
import unittest
from unittest.mock import patch

from compare_performance import run_profile
from live_x_harness import HarnessError, ManagedProcess


class ComparisonProcesses(unittest.TestCase):
    def test_completed_profile_keeps_final_log(self):
        with tempfile.TemporaryDirectory() as directory:
            log = Path(directory) / "profile.log"
            status = run_profile([sys.executable, "-c",
                                  "print('final profile output')"], log)
            self.assertEqual(status, 0)
            self.assertEqual(log.read_text().strip(), "final profile output")

    def test_timeout_unwinds_profiler_and_its_owned_child(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            marker = root / "ready.json"
            script = """
import json,sys,time
from pathlib import Path
sys.path.insert(0, sys.argv[1])
from live_x_harness import HarnessRuntime
with HarnessRuntime(prefix='comparison-cancel-') as runtime:
    child = runtime.start('owned-child', [sys.executable, '-c',
        'import time;time.sleep(60)'], 'child.log')
    Path(sys.argv[2]).write_text(json.dumps({
        'root':str(runtime.root), 'pid':child.process.pid}))
    while True:
        time.sleep(0.01)
"""
            start = ManagedProcess.start

            def ready_start(*args):
                process = start(*args)
                try:
                    # Start the timeout only after the fake profiler owns a
                    # live child; emulated interpreter startup is not timed.
                    deadline = time.monotonic() + 15
                    while not marker.exists():
                        if process.poll() is not None or time.monotonic() > deadline:
                            raise AssertionError("fake profiler did not become ready")
                        time.sleep(0.01)
                    return process
                except BaseException:
                    process.close(time.monotonic() + 5)
                    raise

            command = [sys.executable, "-c", script,
                       str(Path(__file__).resolve().parent / "live_x"), str(marker)]
            with patch.object(ManagedProcess, "start", side_effect=ready_start):
                with self.assertRaises(HarnessError):
                    run_profile(command, root / "profile.log", timeout=0.05)
            child = json.loads(marker.read_text())
            self.assertFalse(Path(child["root"]).exists())
            with self.assertRaises(ProcessLookupError):
                os.kill(child["pid"], 0)
            self.assertIn("termination requested", (root / "profile.log").read_text())


if __name__ == "__main__":
    unittest.main()
