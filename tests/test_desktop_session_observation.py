#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or any later version.
"""Session failure teardown and bounded optional GC diagnostics, without X."""
from pathlib import Path
import sys
import tempfile
import unittest
from types import SimpleNamespace
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parent / 'live_x'))
import desktop_session_live as session
from live_x_harness import HarnessCancelled


class ObservationOwnershipTests(unittest.TestCase):
    def test_runtime_reaps_before_kernel_removal_on_failure_and_cancellation(self):
        for failure in (AssertionError('readiness'), HarnessCancelled('cancel')):
            events = []
            class Runtime:
                def __init__(self, **kwargs):
                    pass
                def __enter__(self):
                    events.append('session-start')
                    return self
                def __exit__(self, *args):
                    events.append('session-reaped')
            with patch.object(session, 'HarnessRuntime', Runtime):
                with self.assertRaises(type(failure)):
                    with session.session_runtime() as (runtime, owners):
                        owners.callback(lambda: events.append('kernel-removed'))
                        raise failure
            self.assertEqual(events, ['session-start', 'session-reaped',
                                      'kernel-removed'])

    def test_gc_reader_caps_input_before_decoding(self):
        with tempfile.TemporaryDirectory() as directory:
            log = Path(directory) / 'stderr'
            log.write_bytes(b'x' * (2 * 1024 * 1024))
            received = []
            def parser(text):
                received.append(len(text))
                return {'bounded': True}
            with patch.object(session, 'parse_gc', parser):
                self.assertEqual(session.resident_gc(log), {'bounded': True})
            self.assertEqual(received, [1024 * 1024 + 1])

    def test_prelaunch_journal_survives_failure_and_preserves_creation_identity(self):
        class Kernel:
            def ownership(self):
                return {'cgroup': {'path': '/owned/group', 'device': 7, 'inode': 9}}
            def close(self):
                return []
        with tempfile.TemporaryDirectory() as directory:
            artifact = Path(directory)
            runtime = SimpleNamespace(root=artifact / 'runtime')
            server = SimpleNamespace(process=SimpleNamespace(pid=123))
            with patch.object(session, 'process_identity',
                              side_effect=lambda pid: {'pid': pid, 'start_ticks': 4}):
                session.write_ownership(Kernel(), artifact, runtime, server)
            import json
            saved = json.loads((artifact / 'ownership.json').read_text())
            self.assertEqual(saved['launch_state'], 'prepared')
            self.assertIsNone(saved['core'])
            self.assertEqual(saved['kernel']['cgroup']['inode'], 9)
            session.close_kernel(Kernel(), artifact)
            saved = json.loads((artifact / 'ownership.json').read_text())
            self.assertEqual(saved['launch_state'], 'prepared')
            self.assertIsNone(saved['core'])
            self.assertEqual(saved['cleanup'], {'errors': [], 'runtime_closed': True})


if __name__ == '__main__':
    unittest.main()
