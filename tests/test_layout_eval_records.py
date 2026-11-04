#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License, version 2 or later.
"""Private layout evidence must identify one exited owned process and its route."""
import json
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).parent / 'live_x'))
from graphics_eval_records import TraceError, parse_layout_record, layout_metadata


def record(**changes):
    row = dict(layout_eval=1, pid=123, backend='clay', calls=3, native=0,
        clay=3, fallback=0, errors=0, arena_bytes=500000, refused=0, complete=1, metadata=42)
    row.update(changes)
    return (json.dumps(row, separators=(',', ':')) + '\n').encode()


class LayoutRecords(unittest.TestCase):
    def test_owned_committed_candidate(self):
        row = parse_layout_record(b'unrelated\n' + record(), 123, 'clay', 'fit')
        self.assertEqual(row['clay'], 3)

    def test_native_control_and_explicit_fallback(self):
        native = record(backend='native', native=3, clay=0, arena_bytes=0)
        self.assertEqual(parse_layout_record(native, 123, 'native', 'fit')['native'], 3)
        fallback = record(native=3, clay=0, fallback=3, refused=3)
        for reason in ('weighted', 'compressed', 'refuse'):
            self.assertEqual(parse_layout_record(fallback, 123, 'clay', reason)['clay'], 0)

    def test_missing_wrong_owner_route_and_duplicate_rejected(self):
        for raw in (b'', record(pid=124), record(backend='native'), record(clay=0),
                    record() + record(), record(complete=0), record(errors=1),
                    record(arena_bytes=2097153), record(calls=True), record(extra=1)):
            with self.subTest(raw=raw), self.assertRaises(TraceError):
                parse_layout_record(raw, 123, 'clay', 'fit')
        with self.assertRaises(TraceError):
            parse_layout_record(record(), 123, 'clay', 'weighted')

    def test_update_requires_matching_committed_metadata(self):
        old = [('First action', None, None), ('Second action', None, None),
            ('Third action', None, None)]
        updated = [('Open café', 'Long translated action', None), old[1],
            ('Third changed action', None, None)]
        before, after = layout_metadata(old), layout_metadata(updated)
        self.assertNotEqual(before, after)
        self.assertEqual(layout_metadata([(None, None, None)]), 299179820)
        self.assertEqual(parse_layout_record(record(metadata=after), 123,
            'clay', 'update', after)['metadata'], after)
        # Even many successful startup/resize layouts cannot prove an update.
        for raw in (record(metadata=before, calls=100, clay=100),
                    record(metadata=after, clay=0, native=3, fallback=3)):
            with self.assertRaises(TraceError):
                parse_layout_record(raw, 123, 'clay', 'update', after)
        with self.assertRaises(TraceError):
            parse_layout_record(record(metadata=after), 123, 'clay', 'update')

    def test_malformed_truncated_and_duplicate_keys_rejected(self):
        for raw in (record()[:-1], b'{"layout_eval":broken}\n',
                    record().replace(b'"pid":123', b'"pid":123,"pid":123'),
                    b'{"layout_eval":' + b'x' * 1024 + b'\n'):
            with self.subTest(raw=raw), self.assertRaises(TraceError):
                parse_layout_record(raw, 123, 'clay', 'fit')

    def test_off_and_ordinary_reject_unexpected_observation(self):
        self.assertIsNone(parse_layout_record(b'log\n', None, None, 'fit'))
        with self.assertRaises(TraceError):
            parse_layout_record(record(), None, None, 'fit')


if __name__ == '__main__':
    unittest.main()
