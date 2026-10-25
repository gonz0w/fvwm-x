#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or any later version.
"""Fixed GC trace contracts independent of a running Go or X process."""
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).parent / 'live_x'))
from session_gc import parse_gc


class GCTraceTests(unittest.TestCase):
    def test_clock_pauses_and_concurrent_time_remain_distinct(self):
        result = parse_gc('noise\ngc 2 @1.25s 0%: 0.01+3.4+0.02 ms clock, '
                          '0.04+1/2/3+0.04 ms cpu, 2->2->1 MB\n')
        self.assertEqual(result['cycles'][0]['clock_phases_ms'], [.01, 3.4, .02])
        self.assertAlmostEqual(result['max_observed_stop_the_world_clock_ms'], .03)
        self.assertTrue(result['complete_parse'])
        self.assertFalse(result['cycles'][0]['forced'])

    def test_absent_cycles_are_not_zero_pause(self):
        result = parse_gc('ordinary core diagnostics\n')
        self.assertEqual(result['observed_cycles'], 0)
        self.assertIsNone(result['max_observed_stop_the_world_clock_ms'])

    def test_invalid_negative_and_nonfinite_lines_refuse_complete_parse(self):
        for line in ['gc invalid', 'gc 0 @1s 0%: 1+2+3 ms clock, ',
                     'gc 1 @1s 0%: -1+2+3 ms clock, ',
                     'gc 1 @1e999s 0%: 1+2+3 ms clock, ']:
            with self.subTest(line=line):
                result = parse_gc(line)
                self.assertFalse(result['complete_parse'])
                self.assertEqual(result['observed_cycles'], 0)

    def test_forced_and_truncated_data_remain_explicit(self):
        line = 'gc 1 @.1s 0%: 1+2+3 ms clock, rest (forced)\n'
        self.assertTrue(parse_gc(line)['cycles'][0]['forced'])
        result = parse_gc(line, maximum=12)
        self.assertTrue(result['input_truncated'])
        self.assertFalse(result['complete_parse'])


if __name__ == '__main__':
    unittest.main()
