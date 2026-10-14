#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
"""Check profiling evidence ownership without an X server."""

import json
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parent / "live_x"))
from live_x_harness import HarnessError, HarnessRuntime, wait_for_predicate
from responsiveness_profile import finish_profile


class ProfilingEvidence(unittest.TestCase):
    def test_final_child_output_is_collected_before_runtime_removal(self):
        runtime = HarnessRuntime(prefix="profile-log-test-")
        runtime_root = runtime.root
        child = runtime.start("owned-worker", [sys.executable, "-c", """
import signal,time
def stop(signum, frame):
    print('final-child-record', flush=True)
    raise SystemExit(0)
signal.signal(signal.SIGTERM, stop)
print('ready', flush=True)
while True:
    time.sleep(0.01)
"""], "child.log")
        import time
        wait_for_predicate("child ready",
                           lambda: "ready" in child.log_path.read_text(),
                           time.monotonic() + 5, process=child)
        with tempfile.TemporaryDirectory() as directory:
            artifacts = Path(directory)
            finish_profile(runtime, None, {"cases": []}, artifacts)
            self.assertIn("final-child-record", (artifacts / "child.log").read_text())
            self.assertEqual(child.poll(), 0)
            self.assertEqual(json.loads((artifacts / "profile.json").read_text()),
                             {"cases": []})
        self.assertFalse(runtime_root.exists())

    def test_cleanup_failure_keeps_evidence_and_stops_other_children(self):
        runtime = HarnessRuntime(prefix="profile-error-test-")
        child = runtime.start("owned-worker", [sys.executable, "-c",
                              "import time; time.sleep(30)"], "child.log")

        class FailedClient:
            def close(self):
                raise RuntimeError("client teardown control")

        with tempfile.TemporaryDirectory() as directory:
            artifacts = Path(directory)
            with self.assertRaisesRegex(HarnessError, "client teardown control"):
                finish_profile(runtime, FailedClient(), {"cases": []}, artifacts)
            record = json.loads((artifacts / "profile.json").read_text())
            self.assertEqual(record["teardown_errors"], ["client teardown control"])
            self.assertIsNotNone(child.poll())
            self.assertTrue((artifacts / "child.log").exists())

    def test_original_failure_is_preserved_with_teardown_diagnostics(self):
        runtime = HarnessRuntime(prefix="profile-original-error-test-")

        class FailedClient:
            def close(self):
                raise RuntimeError("secondary cleanup control")

        with tempfile.TemporaryDirectory() as directory:
            artifacts = Path(directory)
            result = {"error": "original measurement failure"}
            finish_profile(runtime, FailedClient(), result, artifacts)
            record = json.loads((artifacts / "profile.json").read_text())
            self.assertEqual(record["error"], "original measurement failure")
            self.assertEqual(record["teardown_errors"], ["secondary cleanup control"])


if __name__ == "__main__":
    unittest.main()
