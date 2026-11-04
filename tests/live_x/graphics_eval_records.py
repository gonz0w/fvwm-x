# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License, version 2 or later.
"""Bound the private JSONL observer and reject incomplete qualification evidence."""
import json

MAX_LOG_BYTES = 8 * 1024 * 1024
CAPACITY = 256
COUNTERS = frozenset(('idle', 'wait', 'timeout_queries', 'service_paints',
    'synchronous_draws', 'service_draws', 'issued', 'paint_issued',
    'native_fallbacks'))
STATS = frozenset(('attempts', 'refused', 'empty', 'committed',
    'pixmap_requests', 'pixmap_retires', 'gc_requests', 'gc_retires',
    'surface_requests', 'surface_retires', 'context_requests', 'context_retires',
    'pattern_requests', 'pattern_retires', 'preserve_copies', 'commit_copies',
    'pixels_copied', 'scratch_pixels_requested', 'peak_scratch_pixels'))
COMMON = frozenset(('graphics_eval', 'pid', 'module', 'backend', 'type'))
RECORD = frozenset(('seq', 'ms', 'clock', 'event', 'reason', 'operation',
    'token', 'next_token', 'identity', 'revision', 'active_before', 'active_after',
    'service_paints', 'timeout', 'issued', 'paint_issued', 'outcome', 'service',
    'from_rgb', 'to_rgb', 'last_rgb'))
TRAILER = frozenset(('records', 'sequence', 'dropped', 'overflow', 'saturated',
    'clock_failed', 'complete'))


class TraceError(ValueError):
    """No complete, source-bound observation follows from the supplied log."""


def require(condition, message):
    if not condition:
        raise TraceError(message)


def integer(value, low=0, high=(1 << 64) - 1):
    require(type(value) is int and low <= value <= high, 'invalid integer')


def unique(pairs):
    result = {}
    for key, value in pairs:
        require(key not in result, 'duplicate JSON key')
        result[key] = value
    return result


def parse_records(raw, expected):
    """Read only exited owned PIDs. Ordinary/observer-off expects an empty map."""
    require(isinstance(raw, bytes) and len(raw) <= MAX_LOG_BYTES,
        'observer log byte budget')
    require(len(expected) <= 8, 'observer process budget')
    groups = {}
    for line in raw.splitlines(keepends=True):
        if not line.startswith(b'{"graphics_eval":'):
            continue
        require(line.endswith(b'\n') and len(line) <= 1024,
            'truncated/oversized observer line')
        try:
            row = json.loads(line, object_pairs_hook=unique)
        except (ValueError, UnicodeError) as error:
            raise TraceError('malformed observer JSON') from error
        require(isinstance(row, dict) and COMMON <= row.keys(), 'missing identity')
        integer(row['graphics_eval'], 1, 1)
        integer(row['pid'], 1, (1 << 31) - 1)
        pid = row['pid']
        require(pid in expected, 'unowned observer PID')
        require(row['module'] in ('form', 'buttons') and
            row['backend'] in ('native', 'cairo') and
            (row['module'], row['backend']) == expected[pid], 'observer identity')
        kind = row['type']
        require(kind in ('header', 'record', 'counter', 'stat', 'trailer'),
            'unknown observer line')
        fields = {'header': {'capacity'}, 'record': RECORD,
            'counter': {'name', 'value'}, 'stat': {'name', 'value'},
            'trailer': TRAILER}[kind]
        require(set(row) == COMMON | fields, 'observer schema drift')
        if kind == 'header':
            require(pid not in groups and row['capacity'] == CAPACITY and
                type(row['capacity']) is int, 'duplicate/invalid header')
            groups[pid] = dict(header=row, records=[], counters={}, stats={},
                phase='record', trailer=None)
            continue
        require(pid in groups, 'observer line before header')
        group = groups[pid]
        require(group['trailer'] is None, 'observer line after trailer')
        if kind == 'record':
            require(group['phase'] == 'record' and
                len(group['records']) < CAPACITY, 'late/excess observer record')
            for name in RECORD:
                integer(row[name], -1 if name == 'timeout' else 0)
            require(row['seq'] == len(group['records']) + 1, 'observer sequence')
            for name in ('clock', 'active_before', 'active_after', 'service'):
                integer(row[name], 0, 1)
            integer(row['event'], 0, 7)
            integer(row['reason'], 0, 14)
            integer(row['operation'], 0, 4)
            integer(row['outcome'], 0, 15)
            integer(row['service_paints'], 0, 8)
            integer(row['timeout'], -1, 120)
            for name in ('from_rgb', 'to_rgb', 'last_rgb'):
                integer(row[name], 0, 0xffffff)
            require(row['clock'] == 1 and row['event'] != 6,
                'failed clock/overflow cannot qualify')
            if group['records']:
                require(group['records'][-1]['event'] != 7 and
                    row['ms'] >= group['records'][-1]['ms'],
                    'record after terminal/clock regression')
            group['records'].append(row)
        elif kind in ('counter', 'stat'):
            allowed = COUNTERS if kind == 'counter' else STATS
            destination = group['counters' if kind == 'counter' else 'stats']
            require(group['phase'] != 'stat' or kind == 'stat', 'late counter')
            require(type(row['name']) is str and row['name'] in allowed and
                row['name'] not in destination,
                'unknown/duplicate counter or statistic')
            integer(row['value'])
            group['phase'] = kind
            destination[row['name']] = row['value']
        else:
            for name in TRAILER:
                integer(row[name])
            records = group['records']
            require(records and records[-1]['event'] == 7,
                'missing terminal observation')
            require(row['records'] == row['sequence'] == len(records),
                'incomplete observer sequence')
            require(row['complete'] == 1 and all(row[name] == 0 for name in
                ('dropped', 'overflow', 'saturated', 'clock_failed')),
                'incomplete observer export')
            require(set(group['counters']) == COUNTERS and
                set(group['stats']) == STATS, 'missing counters/statistics')
            group['trailer'] = row
    require(set(groups) == set(expected), 'missing owned observer stream')
    for group in groups.values():
        require(group['trailer'] is not None, 'missing observer trailer')
        del group['phase']
    return groups


def qualify_lifecycle(trace, require_positive=False, require_interruption=False,
        require_native=False, require_cairo=False, immediate=False,
        positive_bounds=None, interruption_bounds=None, expected_identity=None,
        interruption_reasons=None, retirement_exit=False):
    """Correlate admitted tokens and the owned stimulus; pixels stay independent."""
    closed, finished = set(), set()
    admissions, progress = {}, {}
    frames, finishes, interruptions, draws, paints = [], [], [], [], []

    def within(row, bounds):
        return bounds is None or bounds[0] <= row['ms'] <= bounds[1]

    def matches(row, admission):
        return all(row[key] == admission[key] for key in
            ('identity', 'revision', 'from_rgb', 'to_rgb'))

    for row in trace['records']:
        event, token = row['event'], row['token']
        if event in (2, 3, 4, 5):
            require(token not in closed, 'paint/sample after token cancellation')
        if event in (4, 5) and row['service']:
            require(token not in finished, 'service paint after completion')
        if event == 0 and row['active_after']:
            next_token = row['next_token']
            require(next_token and next_token not in closed and row['identity'] and
                row['from_rgb'] != row['to_rgb'], 'invalid active admission')
            if next_token in admissions:
                require(row['active_before'] and token == next_token and
                    matches(row, admissions[next_token]), 'reused transition token')
            else:
                admissions[next_token] = row
                progress[next_token] = 0
        if event == 1:
            if token != row['next_token']:
                closed.add(token)
            if row['active_before'] and row['reason'] != 14:
                require(token in admissions and matches(row, admissions[token]) and
                    not row['active_after'], 'cancel lacks matching admission')
                interruptions.append(row)
        if event in (2, 3):
            require(token in admissions and token not in finished and
                matches(row, admissions[token]) and
                row['service_paints'] > progress[token],
                'frame/finish lacks matching admitted transition')
            progress[token] = row['service_paints']
        if event == 2:
            require(row['active_before'] and row['active_after'] and
                1 <= row['service_paints'] <= 8, 'invalid delivered frame')
            frames.append(row)
        if event == 3:
            require(row['active_before'] and not row['active_after'] and
                row['timeout'] == -1 and row['last_rgb'] == row['to_rgb'] and
                1 <= row['service_paints'] <= 8, 'invalid final target')
            finished.add(token)
            finishes.append(row)
        if event == 4:
            draws.append(row)
        if event == 5:
            paints.append(row)
    terminal = trace['records'][-1]
    require(not terminal['active_after'] and terminal['timeout'] == -1,
        'terminal animation still scheduled')
    identity_matches = lambda row: expected_identity is None or row['identity'] == expected_identity
    positive_tokens = {token for token, admission in admissions.items()
        if within(admission, positive_bounds) and identity_matches(admission)}
    positive = any(row['token'] in positive_tokens and within(row, positive_bounds)
        for row in finishes)
    require(not require_positive or positive, 'positive full motion unobserved')
    # Bind to the FIRST admission caused by this module/control's stimulus.
    # Warmup, a replacement's second token and later exit cannot satisfy it.
    targeted = [(token, row) for token, row in admissions.items()
        if interruption_bounds is not None and within(row, interruption_bounds) and
        identity_matches(row)]
    target = min(targeted, key=lambda item: item[1]['seq'])[0] if targeted else None
    selected_interruptions = [row for row in interruptions
        if (interruption_bounds is None or row['token'] == target) and
        within(row, interruption_bounds) and
        (interruption_reasons is None or row['reason'] in interruption_reasons)]
    exited_active = bool(retirement_exit and target is not None and
        terminal['token'] == target and terminal['active_before'] and
        within(terminal, interruption_bounds))
    require(not require_interruption or selected_interruptions or exited_active,
        'interruption missed targeted active work; case is inconclusive')
    if immediate:
        require(not admissions and not frames and not finishes and
            trace['counters']['service_paints'] == 0,
            'immediate control scheduled animation')
    if require_native:
        require(any(row['outcome'] & 8 and row['issued'] > 0 for row in draws),
            'native fallback drawing unobserved')
    if require_cairo:
        require(any(row['outcome'] & 1 for row in paints) and
            trace['stats']['committed'] > 0, 'Cairo commit unobserved')
        for owner in ('pixmap', 'gc', 'surface', 'context', 'pattern'):
            require(trace['stats'][owner + '_requests'] ==
                trace['stats'][owner + '_retires'], 'unbalanced own resources')
    return dict(positive=positive, positive_tokens=sorted(positive_tokens),
        delivered_frames=len(frames), finishes=len(finishes),
        interruptions=len(selected_interruptions), stimulus_token=target,
        retirement_exit_instead_of_callback=exited_active,
        draw_operations=len(draws), issued_requests=trace['counters']['issued'],
        request_scope='selected paint operation including native fallback',
        implicit_roundtrips=None, final_timeout=terminal['timeout'])


def parse_layout_record(raw, pid, backend, case, metadata_expected=None):
    """One once-exit private layout witness; absence is required when off."""
    require(isinstance(raw, bytes) and len(raw) <= MAX_LOG_BYTES,
        'layout log byte budget')
    fields = {'layout_eval', 'pid', 'backend', 'calls', 'native', 'clay',
        'fallback', 'errors', 'arena_bytes', 'refused', 'complete', 'metadata'}
    rows = []
    for line in raw.splitlines(keepends=True):
        if not line.startswith(b'{"layout_eval":'):
            continue
        require(line.endswith(b'\n') and len(line) <= 1024,
            'truncated/oversized layout line')
        try:
            row = json.loads(line, object_pairs_hook=unique)
        except (ValueError, UnicodeError) as error:
            raise TraceError('malformed layout JSON') from error
        require(type(row) is dict and set(row) == fields, 'layout schema drift')
        for key in fields - {'backend'}:
            integer(row[key])
        integer(row['layout_eval'], 1, 1)
        integer(row['complete'], 1, 1)
        integer(row['pid'], 1, (1 << 31) - 1)
        require(pid is not None and row['pid'] == pid and
            row['backend'] == backend and backend in ('native', 'clay'),
            'unowned layout observation')
        require(row['calls'] > 0 and row['errors'] == 0 and
            row['arena_bytes'] <= 2 * 1024 * 1024, 'layout failure/capacity')
        rows.append(row)
    if pid is None:
        require(not rows, 'unexpected layout observation')
        return None
    require(len(rows) == 1, 'missing/duplicate layout retirement')
    row = rows[0]
    if backend == 'native':
        require(row['native'] > 0 and row['clay'] == 0 and
            row['arena_bytes'] == 0, 'native layout control')
    elif case in ('weighted', 'compressed', 'refuse'):
        require(row['native'] > 0 and row['fallback'] > 0 and
            row['clay'] == 0, 'required native layout fallback not observed')
    else:
        require(case in ('fit', 'update') and row['clay'] > 0 and
            row['arena_bytes'] > 0, 'independent Clay layout not committed')
    integer(row['metadata'], 0, (1 << 32) - 1)
    if case == 'update':
        require(type(metadata_expected) is int and
            row['metadata'] == metadata_expected,
            'changed metadata was not used by a successful reflow')
    return row


def layout_metadata(titles):
    """Fixed private test tag, not a security digest or content disclosure."""
    require(1 <= len(titles) <= 256, 'layout metadata count')
    value = 2166136261

    def consume(byte):
        nonlocal value
        value = ((value ^ byte) * 16777619) & 0xffffffff

    for byte in len(titles).to_bytes(4, 'little'):
        consume(byte)
    for row in titles:
        require(len(row) == 3, 'layout metadata fields')
        for text in row:
            if text is None:
                consume(0)
                continue
            require(type(text) is str and '\0' not in text, 'layout metadata text')
            raw = text.encode('utf-8')
            require(len(raw) <= 4095, 'layout metadata byte budget')
            consume(1)
            for byte in raw:
                consume(byte)
            consume(0)
    return value
