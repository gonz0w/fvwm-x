#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License, version 2 or later.
"""Reject incomplete/private drawing observations before treating them as evidence."""
import copy
import json
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).parent / 'live_x'))
from graphics_eval_records import (COUNTERS, STATS, TraceError, parse_records,
    qualify_lifecycle)


def stream(pid=42, module='form', backend='native', records=None):
    common = dict(graphics_eval=1, pid=pid, module=module, backend=backend)
    if records is None:
        records = [dict(seq=1, ms=100, clock=1, event=7, reason=14,
            operation=0, token=0, next_token=1, identity=0, revision=0,
            active_before=0, active_after=0, service_paints=0, timeout=-1,
            issued=0, paint_issued=0, outcome=0, service=0,
            from_rgb=0, to_rgb=0, last_rgb=0)]
    result = [dict(common, type='header', capacity=256)]
    result += [dict(common, type='record', **value) for value in records]
    result += [dict(common, type='counter', name=name, value=0)
               for name in sorted(COUNTERS)]
    result += [dict(common, type='stat', name=name, value=0)
               for name in sorted(STATS)]
    result += [dict(common, type='trailer', records=len(records),
        sequence=len(records), dropped=0, overflow=0, saturated=0,
        clock_failed=0, complete=1)]
    return result


def encode(rows):
    return b''.join(json.dumps(row, separators=(',', ':')).encode() + b'\n'
                    for row in rows)


class Records(unittest.TestCase):
    def parse(self, rows):
        return parse_records(encode(rows), {42: ('form', 'native')})

    def test_interleaved_owned_processes_and_unrelated_log(self):
        a, b = stream(), stream(43, 'buttons', 'cairo')
        rows = [row for pair in zip(a, b) for row in pair]
        result = parse_records(b'ordinary module log\n' + encode(rows),
            {42: ('form', 'native'), 43: ('buttons', 'cairo')})
        self.assertEqual(set(result), {42, 43})
        self.assertEqual(result[42]['records'][-1]['event'], 7)

    def test_missing_terminal_or_trailer(self):
        for rows in (stream()[:-1], [r for r in stream() if r['type'] != 'record']):
            with self.assertRaises(TraceError): self.parse(rows)

    def test_unknown_process_or_identity(self):
        for rows in (stream(43), stream(42, 'buttons'), stream(42, backend='cairo')):
            with self.assertRaises(TraceError): self.parse(rows)
        with self.assertRaises(TraceError):
            parse_records(encode(stream()), {})
        self.assertEqual(parse_records(b'no observer here\n', {}), {})

    def test_duplicate_or_missing_fields_and_duplicate_json_keys(self):
        rows = stream()
        rows[1]['future'] = 1
        with self.assertRaises(TraceError): self.parse(rows)
        rows = stream()
        del rows[1]['token']
        with self.assertRaises(TraceError): self.parse(rows)
        raw = encode(stream()).replace(b'"clock":1', b'"clock":1,"clock":1')
        with self.assertRaises(TraceError):
            parse_records(raw, {42: ('form', 'native')})

    def test_numeric_types_and_bounds(self):
        for name, value in [('token', True), ('ms', -1), ('token', 1 << 64),
                ('event', 8), ('reason', 15), ('operation', 5), ('clock', 2),
                ('service_paints', 9), ('timeout', -2), ('timeout', 121),
                ('outcome', 16), ('last_rgb', 1 << 24), ('issued', 1.0)]:
            rows = stream()
            rows[1][name] = value
            with self.subTest(name=name, value=value), self.assertRaises(TraceError):
                self.parse(rows)

    def test_duplicate_header_counter_stat_or_late_record(self):
        for kind in ('header', 'counter', 'stat', 'record'):
            rows = stream()
            row = next(r for r in rows if r['type'] == kind)
            rows.insert(-1, copy.deepcopy(row))
            with self.subTest(kind=kind), self.assertRaises(TraceError):
                self.parse(rows)

    def test_sequence_and_trailer_counts(self):
        for key, value in [('records', 2), ('sequence', 2), ('dropped', 1),
                ('overflow', 1), ('saturated', 1), ('clock_failed', 1), ('complete', 0)]:
            rows = stream()
            rows[-1][key] = value
            with self.subTest(key=key), self.assertRaises(TraceError):
                self.parse(rows)
        rows = stream()
        rows[1]['seq'] = 2
        with self.assertRaises(TraceError): self.parse(rows)

    def test_truncated_oversized_unknown_version_and_failed_clock(self):
        for raw in (encode(stream())[:-1], b'{"graphics_eval":1,' + b'x' * 1024,
                b'x' * (8 * 1024 * 1024 + 1)):
            with self.assertRaises(TraceError):
                parse_records(raw, {42: ('form', 'native')})
        rows = stream()
        rows[0]['graphics_eval'] = 2
        with self.assertRaises(TraceError): self.parse(rows)
        rows = stream()
        rows[1]['clock'] = 0
        with self.assertRaises(TraceError): self.parse(rows)

    def test_final_service_draw_precedes_finish_and_later_expose_is_valid(self):
        base = stream()[1]
        values = [
            dict(base, seq=1, event=0, reason=0, token=0, next_token=1,
                identity=7, active_after=1, timeout=15, from_rgb=0x505050,
                to_rgb=0xa0a0a0, last_rgb=0x505050),
            dict(base, seq=2, ms=115, event=2, reason=0, token=1, next_token=1,
                identity=7, active_before=1, active_after=1, timeout=15,
                service_paints=1, from_rgb=0x505050, to_rgb=0xa0a0a0,
                last_rgb=0x606060),
            dict(base, seq=3, ms=220, event=4, token=1, next_token=1,
                identity=7, service=1, operation=2, service_paints=2,
                from_rgb=0x505050, to_rgb=0xa0a0a0, last_rgb=0xa0a0a0),
            dict(base, seq=4, ms=220, event=3, reason=0, token=1, next_token=1,
                identity=7, active_before=1, service_paints=2,
                from_rgb=0x505050, to_rgb=0xa0a0a0, last_rgb=0xa0a0a0),
            dict(base, seq=5, ms=250, event=4, token=1, next_token=1,
                identity=7, operation=2, service_paints=2,
                from_rgb=0x505050, to_rgb=0xa0a0a0, last_rgb=0xa0a0a0),
            dict(base, seq=6, ms=260, token=1, next_token=2, service_paints=2)]
        records = [{k: v for k, v in row.items() if k not in
            ('graphics_eval', 'pid', 'module', 'backend', 'type')}
            for row in values]
        trace = self.parse(stream(records=records))[42]
        self.assertTrue(qualify_lifecycle(trace, require_positive=True)['positive'])

    def test_finish_requires_its_own_admitted_token_and_identity(self):
        base = stream()[1]
        begin = dict(base, seq=1, event=0, reason=0, token=0, next_token=1,
            identity=7, active_after=1, timeout=15, from_rgb=0, to_rgb=0xffffff)
        finish = dict(base, seq=2, ms=220, event=3, reason=0, token=1,
            next_token=1, identity=7, active_before=1, service_paints=1,
            from_rgb=0, to_rgb=0xffffff, last_rgb=0xffffff)
        terminal = dict(base, seq=3, ms=230, token=1, next_token=2)
        for field, value in [('token', 2), ('identity', 8), ('revision', 2),
                ('from_rgb', 1)]:
            altered = dict(finish, **{field: value})
            records = [{k: v for k, v in row.items() if k not in
                ('graphics_eval', 'pid', 'module', 'backend', 'type')}
                for row in (begin, altered, terminal)]
            trace = self.parse(stream(records=records))[42]
            with self.subTest(field=field), self.assertRaisesRegex(TraceError,
                    'frame/finish lacks matching admitted transition'):
                qualify_lifecycle(trace, require_positive=True)

    def test_warmup_cancel_cannot_satisfy_targeted_stimulus(self):
        base = stream()[1]
        begin = dict(base, seq=1, event=0, reason=0, token=0, next_token=1,
            identity=7, active_after=1, timeout=15, from_rgb=0, to_rgb=0xffffff)
        cancel = dict(base, seq=2, ms=110, event=1, reason=5, token=1,
            next_token=2, identity=7, active_before=1, from_rgb=0, to_rgb=0xffffff)
        terminal = dict(base, seq=3, ms=300, token=2, next_token=3)
        records = [{k: v for k, v in row.items() if k not in
            ('graphics_eval', 'pid', 'module', 'backend', 'type')}
            for row in (begin, cancel, terminal)]
        trace = self.parse(stream(records=records))[42]
        with self.assertRaisesRegex(TraceError, 'targeted active work'):
            qualify_lifecycle(trace, require_interruption=True,
                interruption_bounds=(200, 250), expected_identity=7,
                interruption_reasons={5})

    def test_stale_paint_after_cancel_is_not_qualification(self):
        base = stream()[1]
        begin = dict(base, seq=1, event=0, reason=0, token=0, next_token=1,
            active_after=1, timeout=15, identity=7, revision=1,
            from_rgb=0x505050, to_rgb=0xa0a0a0, last_rgb=0x505050)
        cancel = dict(base, seq=2, ms=110, event=1, reason=2, token=1,
            next_token=2, active_before=1, identity=7, revision=1,
            from_rgb=0x505050, to_rgb=0xa0a0a0, last_rgb=0x505050)
        stale = dict(base, seq=3, ms=111, event=4, token=1, next_token=1,
            operation=2, outcome=8)
        terminal = dict(base, seq=4, ms=120, token=2, next_token=3)
        records = [{k: v for k, v in r.items() if k not in
            ('graphics_eval', 'pid', 'module', 'backend', 'type')}
            for r in (begin, cancel, stale, terminal)]
        trace = self.parse(stream(records=records))[42]
        with self.assertRaisesRegex(TraceError, 'paint/sample after token cancellation'):
            qualify_lifecycle(trace)


if __name__ == '__main__':
    unittest.main()
