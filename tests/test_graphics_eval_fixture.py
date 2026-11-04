#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License, version 2 or later.
"""A later owned-runtime cleanup failure must never leave a passed receipt."""
import argparse
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).parent / 'live_x'))
import graphics_eval_live


class Publication(unittest.TestCase):
    def test_cleanup_error_after_successful_body_is_failed(self):
        def cleanup_fails(args, evidence, out):
            evidence['status'] = 'body passed; runtime cleanup pending'
            raise RuntimeError('simulated owned runtime cleanup failure')
        with tempfile.TemporaryDirectory() as root:
            args = argparse.Namespace(artifacts_dir=Path(root), profile='slate-dark',
                variant='native')
            with patch.object(graphics_eval_live, '_run', cleanup_fails):
                with self.assertRaisesRegex(RuntimeError, 'cleanup failure'):
                    graphics_eval_live.run(args)
            files = list(Path(root).glob('*/result.json'))
            self.assertEqual(len(files), 1)
            evidence = json.loads(files[0].read_text())
            self.assertEqual(evidence['status'], 'failed; retained')
            self.assertIn('cleanup failure', evidence['error'])

    def test_success_is_published_after_owned_run_returns(self):
        def completed(args, evidence, out):
            self.assertFalse((out / 'result.json').exists())
            evidence['status'] = 'body passed; runtime cleanup pending'
        with tempfile.TemporaryDirectory() as root:
            args = argparse.Namespace(artifacts_dir=Path(root), profile='slate-dark',
                variant='native')
            with patch.object(graphics_eval_live, '_run', completed):
                evidence = graphics_eval_live.run(args)
            self.assertEqual(evidence['status'], 'passed')
            self.assertEqual(json.loads((Path(evidence['artifact_directory']) /
                'result.json').read_text())['status'], 'passed')


if __name__ == '__main__':
    unittest.main()
