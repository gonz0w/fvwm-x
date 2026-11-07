#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or any later version.
"""Owned native placement/modifier source checkpoint, reusing display setup."""
import argparse
import json
from pathlib import Path
import shutil
import subprocess
import time

from binding_origin_live import expect_source, location, run
from inspector_live import TITLE, form_text, inspector_frame
from live_x_harness import wait_for_predicate
from settings_snapshot import expanded_settings


def policy_scene(*, runtime, out, user, probe, query, snapshot,
                 before_settings, view, core, evidence, required, records, env):
    def policies():
        policy = query(scope='policy')
        placement = query(scope='placement', generation=policy['generation'])
        assert set(policy['provenance']) == {'version', 'ignored_modifiers'}
        assert set(placement['provenance']) == {'version', 'drag', 'interactive'}
        for parent in (policy, placement):
            assert type(parent['provenance']['version']) is int
            assert parent['provenance']['version'] == 1
            for key, value in parent['provenance'].items():
                if key == 'version':
                    continue
                assert set(value) == {'basis', 'source', 'effect', 'retention'}
                assert value['basis'] in ('builtin-default', 'last-value-change')
                assert value['effect'] in ('builtin-default', 'applied', 'parse-error')
                assert value['retention'] in ('not-applicable', 'retained',
                                              'unavailable', 'budget-exhausted')
        return dict(policy=policy, placement=placement)

    def fields(value):
        return {kind: {key: child for key, child in result.items()
                       if key != 'generation'} for kind, result in value.items()}

    initial = policies()
    evidence['initial_policy'] = initial
    entry, leaf = user / 'entry.config', user / 'leaf.config'
    entry.write_text('# nested policy read\nRead "' + str(leaf) + '"\n')
    leaf.write_text('# last value changes\nMouse 0 P N CancelPlacement\n'
                    'Mouse 1 P N \\\n PlaceWindowDrag\n'
                    'Mouse 2 P N PlaceWindowInteractive\n'
                    'IgnoreModifiers N\nIgnoreModifiers L\n')
    for path in (entry, leaf):
        shutil.copy2(path, out / path.name)
    probe.command('Read "' + str(entry) + '"')
    probe.barrier()
    entry.unlink()
    leaf.unlink()
    baseline = policies()
    p, m = baseline['placement'], baseline['policy']
    assert p['drag_finish_mask'] == 1 and p['interactive_finish_mask'] == 2, p
    assert m['ignored_modifier_mask'] == 2, m
    chain = [location(entry, 2)]
    for source, first, last in ((p['provenance']['drag'], 3, 4),
            (p['provenance']['interactive'], 5, 5),
            (m['provenance']['ignored_modifiers'], 7, 7)):
        assert source['basis'] == 'last-value-change'
        assert source['effect'] == 'applied' and source['retention'] == 'retained'
        expect_source(source['source'], leaf, first, last, chain)
    assert not entry.exists() and not leaf.exists()
    evidence['retained_policy'] = baseline
    evidence['cases']['retained_nested_policy_sources_and_normalized_masks'] = True

    probe.command('Mouse 1 P N PlaceWindowDrag', 'Mouse 2 P N PlaceWindowInteractive',
                  'Mouse 1 P C PlaceWindowDrag')
    unchanged = policies()
    assert fields(unchanged) == fields(baseline)
    assert unchanged['policy']['generation'] != baseline['policy']['generation']
    # Placement retains its existing conservative invalidations. Modifiers
    # have the different, already accepted same-value generation contract.
    probe.command('IgnoreModifiers L')
    assert policies() == unchanged
    bad = user / 'bad.config'
    bad.write_text('# legacy partial modifier parse\nIgnoreModifiers Sz\n')
    shutil.copy2(bad, out / bad.name)
    probe.command('Read "' + str(bad) + '"')
    probe.barrier()
    bad.unlink()
    malformed = policies()
    source = malformed['policy']['provenance']['ignored_modifiers']
    assert malformed['policy']['ignored_modifier_mask'] == 1
    assert source['effect'] == 'parse-error' and source['basis'] == 'last-value-change'
    expect_source(source['source'], bad, 2, 2, [])
    assert fields(malformed)['placement'] == fields(baseline)['placement']
    evidence['malformed_policy'] = malformed
    evidence['cases']['unchanged_origins_conservative_placement_and_modifier_parse_error'] = True

    bindings_before = snapshot()
    local = user / 'local.config'
    forbidden = 'W1_INSPECTION_MUST_NOT_EXECUTE'
    local.write_text(local.read_text() + 'PipeRead "printf \'Echo ' + forbidden + '\'"\n')
    local_before = local.read_bytes()
    seen = set()

    def worker_record(filter_):
        def ready():
            for path in sorted(records.glob('w*.json')):
                if path.name in seen:
                    continue
                value = json.loads(path.read_text())
                request = value['request']['inspect']
                if request['scope'] == 'bindings' and request['filter'] == filter_:
                    seen.add(path.name)
                    assert value['returncode'] == 0 and value['response']['version'] == 1
                    return value['response']['result']
        return wait_for_predicate('genuine policy worker ' + filter_, ready,
                                  time.monotonic() + 10, process=core)

    def native_page(previous=None):
        def ready():
            lines = probe.snapshot()
            token = inspector_frame(lines)
            window = view.named(TITLE)
            if token and token != previous and window and \
                    'Snapshot scope: bindings' in form_text(lines):
                return dict(window=window, token=token, lines=lines)
        return wait_for_predicate('published policy inspector', ready,
                                  time.monotonic() + 10, process=core)

    def focus(page):
        probe.command('WindowId ' + str(page['window']) + ' Focus')
        wait_for_predicate('policy Form focus', lambda: view.focus() == page['window'],
                           time.monotonic() + 5, process=core)

    probe.command('SendToModule FvwmDesktop inspect bindings')
    projection, page = worker_record(''), native_page()
    rows = projection['sections'][0]['rows']
    for value in ('user:bad.config:2-2', 'user:leaf.config:3-4', 'user:leaf.config:5-5'):
        assert any(row['value'] == value for row in rows), value
    assert any('parse error' in row['value'].lower() for row in rows)
    assert str(runtime.root) not in json.dumps(projection)
    focus(page)
    view.key('u', control=True)
    for key in 'policy':
        view.key(key)
    view.key('Return')
    filtered, page = worker_record('policy'), native_page(page['token'])
    rows = filtered['sections'][0]['rows']
    assert all(any(value == row['value'] for row in rows) for value in
               ('user:bad.config:2-2', 'user:leaf.config:3-4', 'user:leaf.config:5-5'))
    evidence['projection'] = projection
    evidence['filtered_projection'] = filtered
    index = 0
    for target, filename in (('user:bad.config:2-2', 'policy-modifiers-slate-dark.png'),
                             ('user:leaf.config:3-4', 'policy-placement-slate-dark.png')):
        wanted = next(i for i, row in enumerate(rows) if row['value'] == target) // 2
        while index != wanted:
            focus(page)
            view.key('F2' if index < wanted else 'F1')
            page = native_page(page['token'])
            index += 1 if index < wanted else -1
        assert target in form_text(page['lines'])
        evidence.setdefault('capture_forms', {})[filename] = page['lines']
        captured = subprocess.run([str(required['capture']), str(out)], env=env,
            text=True, capture_output=True, timeout=35, check=False)
        assert captured.returncode == 0 and captured.stdout.startswith('Screenshot saved: ')
        Path(captured.stdout.strip().removeprefix('Screenshot saved: ')).replace(out / filename)
        evidence['screenshots'].append(filename)
    focus(page)
    view.key('Escape')
    wait_for_predicate('policy Escape closes inspector', lambda: view.named(TITLE) is None,
                       time.monotonic() + 5, process=core)
    assert snapshot() == bindings_before and policies() == malformed
    assert expanded_settings(query('settings', scope='all')) == before_settings
    assert local.read_bytes() == local_before
    assert forbidden not in (runtime.root / 'core.log').read_text(errors='replace')
    evidence['cases']['genuine_policy_projection_filter_navigation_read_only'] = True


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__, allow_abbrev=False)
    parser.add_argument('--build', required=True, type=Path)
    parser.add_argument('--artifacts-dir', required=True, type=Path)
    parser.add_argument('--svg-worker', type=Path)
    parser.add_argument('--backer', type=Path)
    run(parser.parse_args(), scenario=policy_scene)
