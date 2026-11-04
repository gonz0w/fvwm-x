#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License, version 2 or later.
"""A later owned-runtime cleanup failure must never leave a passed receipt."""
import argparse
import copy
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).parent / 'live_x'))
import graphics_eval_live


class LayoutReflowEndpoint(unittest.TestCase):
    def setUp(self):
        self.client = [20, 30, 880, 64]
        self.root = [100, 200]
        self.boxes = [[100, 200, 280, 64], [380, 200, 300, 64],
            [680, 200, 300, 64]]

    def ready(self, client=None, boxes=None, previous=800, requested=880):
        return graphics_eval_live.layout_reflow_endpoint(previous, requested,
            self.client if client is None else client, self.root,
            self.boxes if boxes is None else boxes)

    def test_requested_client_and_prepared_extent_are_both_required(self):
        old_boxes = [[100, 200, 260, 64], [360, 200, 260, 64],
            [620, 200, 280, 64]]
        self.assertFalse(self.ready(client=[20, 30, 800, 64]))
        self.assertFalse(self.ready(boxes=old_boxes))
        self.assertFalse(self.ready(client=[20, 30, 800, 64], boxes=old_boxes))
        self.assertTrue(self.ready())

    def test_gaps_overlap_and_insets_cannot_finish_the_timer(self):
        for index, field, delta in ((1, 0, 1), (1, 0, -1),
                (0, 0, 1), (2, 2, -1), (1, 1, 1), (1, 3, -1)):
            with self.subTest(index=index, field=field, delta=delta):
                boxes = copy.deepcopy(self.boxes)
                boxes[index][field] += delta
                self.assertFalse(self.ready(boxes=boxes))
        # Child content and the old assumed root inset are not outer geometry.
        for boxes in (
                [[109, 204, 262, 56], [389, 204, 282, 56],
                    [689, 204, 282, 56]],
                [[109, 204, 280, 56], [389, 204, 300, 56],
                    [689, 204, 282, 56]]):
            self.assertFalse(self.ready(boxes=boxes))

    def test_native_and_clay_distribution_may_differ(self):
        equalized = [[100, 200, 293, 64], [393, 200, 294, 64],
            [687, 200, 293, 64]]
        self.assertTrue(self.ready())
        self.assertTrue(self.ready(boxes=equalized))
        smaller = [[100, 200, 260, 64], [360, 200, 260, 64],
            [620, 200, 280, 64]]
        self.assertTrue(self.ready(client=[20, 30, 800, 64], boxes=smaller,
            previous=880, requested=800))

    def test_initial_or_noop_size_cannot_qualify(self):
        with self.assertRaises(graphics_eval_live.TraceError):
            self.ready(previous=880)
        with self.assertRaises(graphics_eval_live.TraceError):
            self.ready(previous=800, requested=800)

    def test_malformed_observations_remain_hard_errors(self):
        for client, boxes in (([20, 30, True, 64], self.boxes),
                ([20, 30, 880], self.boxes), (self.client, self.boxes[:2]),
                (self.client, [[100, 200, 280, 64], [380, 200, -1, 64],
                    [680, 200, 300, 64]])):
            with self.subTest(client=client, boxes=boxes):
                with self.assertRaises(graphics_eval_live.TraceError):
                    self.ready(client=client, boxes=boxes)


class FormActionContract(unittest.TestCase):
    def test_generated_config_observes_the_toggleable_choice(self):
        for scenario in ('positive', 'choice-only', 'resize'):
            with self.subTest(scenario=scenario):
                lines = graphics_eval_live.form_configuration('fixed', scenario)
                self.assertIn('Choice flag yes off "Toggle native choice"', lines)
                self.assertIn('Button continue "Observe (F2)" F2', lines)
                self.assertEqual([line for line in lines if line.startswith('Command ')],
                    ['Command Echo G3_FORM_ACTION x$(flag?yes)'])
                self.assertEqual('List picks 28 2' in lines, scenario != 'choice-only')

    def test_exact_receipts_and_failed_evidence_preserve_selected_state(self):
        for name in ('initial_form_action', 'final_form_action'):
            for selected in (True, False):
                expected = 'G3_FORM_ACTION ' + ('xyes' if selected else 'x')
                evidence = {}
                graphics_eval_live.require_form_action(evidence, name, expected,
                    selected)
                self.assertEqual(evidence[name], expected)
                self.assertEqual(evidence[name + '_expected'], expected)
                stale = 'G3_FORM_ACTION ' + ('x' if selected else 'xyes')
                for actual in (stale, expected + ' ', 'G3_FORM_ACTION'):
                    with self.subTest(name=name, selected=selected, actual=actual):
                        failed = {}
                        with self.assertRaises(graphics_eval_live.TraceError):
                            graphics_eval_live.require_form_action(failed, name,
                                actual, selected)
                        self.assertEqual(failed[name], actual)
                        self.assertEqual(failed[name + '_expected'], expected)


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
