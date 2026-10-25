#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or any later version.
"""Parse opt-in resident GODEBUG=gctrace output, without forcing a collection."""
import math
import re


TRACE = re.compile(r'^gc ([0-9]+) @([0-9.e+-]+)s [0-9]+%: '
                   r'([0-9.e+-]+)\+([0-9.e+-]+)\+([0-9.e+-]+) ms clock, ')


def parse_gc(text, maximum=1024 * 1024):
    limited = len(text.encode('utf-8')) > maximum
    if limited:
        text = text.encode('utf-8')[:maximum].decode('utf-8', errors='ignore')
        text = text.rsplit('\n', 1)[0]
    cycles, malformed = [], []
    for line in text.splitlines():
        if not line.startswith('gc '):
            continue
        match = TRACE.match(line)
        try:
            if not match:
                raise ValueError('unrecognized GC trace')
            cycle = int(match[1])
            at, first, concurrent, final = map(float, match.groups()[1:])
            if cycle <= 0 or not all(math.isfinite(v) and v >= 0
                                     for v in (at, first, concurrent, final)):
                raise ValueError('invalid GC counters')
            cycles.append({'cycle': cycle, 'at_seconds': at,
                           'clock_phases_ms': [first, concurrent, final],
                           'stop_the_world_clock_ms': first + final,
                           'forced': '(forced)' in line})
        except ValueError:
            malformed.append(line[:512])
    return {'scope': 'Resident stderr only; worker stderr is not collected.',
            'enabled': True, 'observed_cycles': len(cycles), 'cycles': cycles,
            'max_observed_stop_the_world_clock_ms': max(
                (v['stop_the_world_clock_ms'] for v in cycles), default=None),
            'malformed_lines': malformed, 'input_truncated': limited,
            'complete_parse': not limited and not malformed,
            'limit': 'No observed cycle does not establish zero GC pauses; '
                     'this workload does not establish worst-case latency.'}
