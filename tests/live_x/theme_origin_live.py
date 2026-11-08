#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or any later version.
"""Combined static-theme source, copy/recovery and native viewer checkpoint.

Reuse owned display setup, not the already accepted binding/policy scenarios.
Run all requested configurations in one admitted checkpoint, after compilation.
"""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import time

from binding_origin_live import expect_source, location, run
from inspector_live import PROFILES, TITLE, form_text, inspector_frame
from live_x_harness import wait_for_predicate
from mfl_request_live import frame, reply
from mfl_stream_live import Connection
from settings_snapshot import expanded_settings


def svg_gate_worker(worker, root, arguments):
    """Hold only this fixture's actual completed SVG reply, without faking it."""
    if len(arguments) != 5 or arguments[0] != '--stdio' or \
            Path(arguments[1]).name != 'origin-delayed.svg':
        os.execv(worker, [worker, *arguments])
    data = sys.stdin.buffer.read(65537)
    assert len(data) <= 65536
    result = subprocess.run([worker, *arguments], input=data,
                            capture_output=True, timeout=2, check=False)
    assert result.returncode == 0 and len(result.stdout) <= 1048576
    (root / 'held').write_text('actual worker reply ready\n')
    deadline = time.monotonic() + 2
    while not (root / 'release').exists():
        if time.monotonic() >= deadline:
            raise TimeoutError('bounded theme source callback gate')
        time.sleep(.005)
    sys.stdout.buffer.write(result.stdout)
    sys.stderr.buffer.write(result.stderr)


def theme_scene(*, runtime, out, user, probe, query, snapshot,
                before_settings, view, core, evidence, required, records, env):
    def page(resource, key):
        result = query(resource, limit=16)
        generation, rows = result['generation'], list(result[key])
        while result['next_offset'] is not None:
            result = query(resource, limit=16, offset=result['next_offset'],
                           generation=generation)
            rows.extend(result[key])
            assert len(rows) <= 4096
        return rows

    def definitions():
        return dict(colorsets=page('colorsets', 'colorsets'),
                    menus=page('menu-styles', 'menu_styles'),
                    decorations=page('decorations', 'decorations'))

    def find(rows, key, value):
        return next(item for item in rows if item[key] == value)

    def selected():
        current = definitions()
        return dict(colorset=find(current['colorsets'], 'id', 0),
                    menu=find(current['menus'], 'name', 'OriginTarget'),
                    copied=find(current['menus'], 'name', 'OriginCopy'),
                    decoration=find(current['decorations'], 'name', 'OriginDecor'))

    initial = definitions()
    for rows in initial.values():
        for item in rows:
            facts = item['provenance']
            assert set(facts) == {'version', 'basis', 'source', 'operation',
                                  'retention', 'replaces_definition', 'copied_from'}
            assert type(facts['version']) is int and facts['version'] == 1
            assert type(facts['replaces_definition']) is bool
    original_async = find(initial['colorsets'], 'id', 1)
    svg = user / 'origin-delayed.svg'
    svg.write_text('<svg xmlns="http://www.w3.org/2000/svg" width="2" height="2">'
                   '<rect width="2" height="2" fill="#753c98"/></svg>\n')
    entry, leaf = user / 'entry.config', user / 'theme-origin.config'
    entry.write_text('# independent include coordinates\nRead "' + str(leaf) + '"\n')
    foreground = '#202020' if evidence['profile'] == 'dune-light' else '#e0e0e0'
    leaf.write_text('# aggregate definition operations, not field histories\n'
                    'Colorset 0 fg ' + foreground + '\n'
                    'MenuStyle OriginTarget Translucent 31, BorderWidth 2\n'
                    'CopyMenuStyle OriginTarget OriginCopy\n'
                    'AddToDecor OriginDecor TitleStyle Height 30\n'
                    '+ TitleStyle LeftJustified\n'
                    '+ TitleStyle LeftJustified\n'
                    'MenuStyle OriginTarget Translucent 31\n'
                    'MenuStyle OriginTarget UnknownX1Clause\n'
                    'Colorset 1 Pixmap "' + str(svg) + '"\n')
    for path in (entry, leaf):
        shutil.copy2(path, out / path.name)
    probe.command('Read "' + str(entry) + '"')
    probe.barrier()
    entry.unlink()
    leaf.unlink()
    wait_for_predicate('held actual image reply',
        (runtime.root / 'svg-gate/held').exists,
        time.monotonic() + 5, process=core)
    pending = query('colorsets', target='1')['colorsets'][0]
    assert pending['update_state'] == 'pending'
    assert pending['provenance'] == original_async['provenance']
    (runtime.root / 'svg-gate/release').write_text('release after source closure\n')
    def settled():
        current = query('colorsets', target='1')['colorsets'][0]
        return current if current['update_state'] == 'idle' else None
    published = wait_for_predicate('source-bound async publication', settled,
                                  time.monotonic() + 5, process=core)
    expect_source(published['provenance']['source'], leaf, 10, 10,
                  [location(entry, 2)])
    evidence['async_source_publication'] = dict(before=original_async,
                                               pending=pending, published=published)
    captured = selected()
    for key, line, operation in (('colorset', 2, 'merge'), ('menu', 8, 'merge'),
                                 ('copied', 4, 'copy'), ('decoration', 7, 'merge')):
        facts = captured[key]['provenance']
        assert facts['basis'] == 'last-definition' and facts['operation'] == operation
        assert facts['retention'] == 'retained'
        expect_source(facts['source'], leaf, line, line, [location(entry, 2)])
        assert facts['copied_from'] == ('OriginTarget' if key == 'copied' else None)
    evidence['retained_nested_theme_definitions'] = captured
    evidence['cases']['nested_theme_origins_survive_source_closure'] = True

    mutation = user / 'decoration-operations.config'
    mutation.write_text('AddToDecor OriginDecor BorderStyle Active -- Flat\n'
                        '+ BorderStyle Active -- Flat\n'
                        '+ BorderStyle Active -- UnknownX1Flag\n')
    shutil.copy2(mutation, out / 'decoration-before-reset.config')
    probe.command('Read "' + str(mutation) + '"')
    probe.barrier()
    mutation.unlink()
    border = selected()['decoration']['provenance']
    expect_source(border['source'], mutation, 2, 2, [])
    assert border['operation'] == 'merge' and border['replaces_definition']
    mutation.write_text('DestroyDecor recreate OriginDecor\n')
    shutil.copy2(mutation, out / 'decoration-reset.config')
    probe.command('Read "' + str(mutation) + '"')
    probe.barrier()
    mutation.unlink()
    reset = selected()['decoration']['provenance']
    expect_source(reset['source'], mutation, 1, 1, [])
    assert reset['operation'] == 'reset' and reset['replaces_definition']
    evidence['decoration_operations'] = dict(border=border, reset=reset)

    # Copy attribution belongs to the copy invocation; no borrowed donor record
    # or transitive source chain may be needed after donor destruction.
    copied = captured['copied']
    probe.command('DestroyMenuStyle OriginTarget')
    probe.barrier()
    assert find(definitions()['menus'], 'name', 'OriginCopy') == copied
    probe.command('MenuStyle OriginTarget Translucent 31')
    probe.barrier()
    before_preview = selected()['menu']
    endpoint = runtime.root / ('fvwm_mfl_' + env['DISPLAY'].replace(':', '_') + '.sock')
    owner = Connection(endpoint)
    owner.record()
    serial = 0

    def action(operation, lease=None, value=73, timeout=30000):
        nonlocal serial
        serial += 1
        fields = dict(operation=operation)
        if lease is None:
            fields.update(target='OriginTarget', values={'opacity': value},
                          timeout_ms=timeout, generation=query('appearance')['generation'])
        else:
            fields.update(target=lease['lease'], generation=lease['generation'])
        token = 'theme-preview-' + str(serial)
        owner.send(frame(token, method='action', resource='preview', **fields))
        return reply(owner, token)

    try:
        lease = action('start')
        previewed = selected()['menu']
        assert previewed['opacity_percent'] == 73
        facts = previewed['provenance']
        assert facts['basis'] == 'runtime-preview' and facts['operation'] == 'preview'
        assert facts['source']['kind'] == 'unavailable'
        assert action('revert', lease)['state'] == 'reverted'
        assert selected()['menu'] == before_preview
        lease = action('start', value=85)
        assert action('confirm', lease)['state'] == 'confirmed'
        confirmed = selected()['menu']
        assert confirmed['opacity_percent'] == 85
        assert confirmed['provenance']['basis'] == 'runtime-preview'
        lease = action('start', value=65, timeout=1000)
        wait_for_predicate('theme preview expiry restores source',
            lambda: selected()['menu'] == confirmed,
            time.monotonic() + 5, process=core)
        lease = action('start', value=75)
        probe.command('MenuStyle OriginTarget Translucent 42')
        probe.barrier()
        authoritative = selected()['menu']
        assert authoritative['opacity_percent'] == 42
        assert authoritative['provenance']['basis'] == 'last-definition'
        owner.close()
        owner = None
        assert selected()['menu'] == authoritative
    finally:
        if owner is not None:
            owner.close()
    evidence['cases']['copy_donor_destruction_and_preview_recovery_origins'] = True

    before_view = selected()
    bindings_before = snapshot()
    local = user / 'local.config'
    local_bytes = local.read_bytes()
    probe.command('SendToModule FvwmDesktop inspect theme')

    def projection_ready():
        for path in sorted(records.glob('w*.json')):
            value = json.loads(path.read_text())
            request = value['request'].get('inspect')
            if request and request['scope'] == 'theme':
                assert value['returncode'] == 0
                return value['response']['result']
    projection = wait_for_predicate('genuine theme worker', projection_ready,
                                   time.monotonic() + 10, process=core)
    rows = projection['sections'][0]['rows']
    assert str(runtime.root) not in json.dumps(projection)
    assert any('latest aggregate definition' in row['value'].lower() for row in rows)
    source = 'user:theme-origin.config:'
    assert any(row['value'].startswith(source) for row in rows)

    def native_page(previous=None):
        def ready():
            lines = probe.snapshot()
            token = inspector_frame(lines)
            window = view.named(TITLE)
            if token and token != previous and window and 'Snapshot scope: theme' in form_text(lines):
                return dict(window=window, token=token, lines=lines)
        return wait_for_predicate('published theme inspector', ready,
                                 time.monotonic() + 10, process=core)

    current = native_page()
    wanted = next(i for i, row in enumerate(rows) if row['value'].startswith(source)) // 2
    for _ in range(wanted):
        probe.command('WindowId ' + str(current['window']) + ' Focus')
        wait_for_predicate('theme Form focus', lambda: view.focus() == current['window'],
                           time.monotonic() + 5, process=core)
        view.key('F2')
        current = native_page(current['token'])
    assert source in form_text(current['lines'])
    filename = 'theme-origin-' + evidence['profile'] + '.png'
    result = subprocess.run([str(required['capture']), str(out)], env=env,
                            text=True, capture_output=True, timeout=35, check=False)
    assert result.returncode == 0 and result.stdout.startswith('Screenshot saved: ')
    Path(result.stdout.strip().removeprefix('Screenshot saved: ')).replace(out / filename)
    evidence['screenshots'].append(filename)
    evidence['projection'], evidence['capture_form'] = projection, current['lines']
    probe.command('WindowId ' + str(current['window']) + ' Focus')
    wait_for_predicate('theme closing focus', lambda: view.focus() == current['window'],
                       time.monotonic() + 5, process=core)
    view.key('Escape')
    wait_for_predicate('theme Escape closes inspector', lambda: view.named(TITLE) is None,
                       time.monotonic() + 5, process=core)
    assert selected() == before_view and snapshot() == bindings_before
    assert expanded_settings(query('settings', scope='all')) == before_settings
    assert local.read_bytes() == local_bytes
    evidence['cases']['native_theme_sources_navigation_read_only'] = True


if __name__ == '__main__':
    if len(sys.argv) > 1 and sys.argv[1] == '--svg-gate-worker':
        svg_gate_worker(sys.argv[2], Path(sys.argv[3]), sys.argv[4:])
        raise SystemExit(0)
    parser = argparse.ArgumentParser(description=__doc__, allow_abbrev=False)
    parser.add_argument('--build', required=True, type=Path)
    parser.add_argument('--artifacts-dir', required=True, type=Path)
    parser.add_argument('--svg-worker', type=Path)
    parser.add_argument('--backer', type=Path)
    parser.add_argument('--profiles', nargs='+', choices=PROFILES, default=list(PROFILES))
    args = parser.parse_args()
    args.svg_gate = True
    for profile in args.profiles:
        run(argparse.Namespace(**{**vars(args), 'profile': profile,
            'artifacts_dir': args.artifacts_dir / profile}), scenario=theme_scene)
