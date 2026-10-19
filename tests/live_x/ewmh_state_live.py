#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
"""Check actual-core EWMH snapshots, work areas and atom dispatch."""
import argparse
import json
from pathlib import Path
import shutil
import time

from live_x_harness import HarnessRuntime, start_display_server


def run(args):
    out = args.artifacts_dir.resolve()
    out.mkdir(parents=True, exist_ok=True)
    with HarnessRuntime(prefix='fvwm-ewmh-state-') as runtime:
        try:
            display = args.display
            if display is None:
                _, display = start_display_server(
                    runtime, 'Xvfb', ['Xvfb', '-screen', '0', '800x600x24',
                                     '-noreset'], 'server.log',
                    time.monotonic() + 15)
            runs = []
            expected = {
                'client-list order/empty/independent membership/refusal/repair',
                'work-area paired differential/filter/invalid/single-mode',
                'destruction publication and query refresh barriers',
                'atom scoped/all priority/miss/None/reinitialization',
                'ewmh state publication',
                'allowed-action local policy/order/override/refresh',
                'strut paired/static/dynamic/zero/missing/filter',
                'desktop-name lengths/UTF8/empty/refusal/ownership',
                'atom names scoped/all/duplicate/unsorted/reentrant',
                'atom duplicate lower-bound priority',
                'atom named property cardinal/UTF8/read/delete/invalid',
            }
            for missing in (False, True):
                label = 'missing' if missing else 'normal'
                process = runtime.start(
                    label, [str(args.fvwm), '-d', display, '-f', '/dev/null'],
                    label + '.log', runtime.environment(
                        {'FVWM_STATE_ATOM_MISSING': '1'} if missing else {}))
                assert process.wait_until_exit(time.monotonic() + 30) == 0
                text = process.log_path.read_text()
                assert {line[:-8] for line in text.splitlines()
                        if line.endswith(': passed')} == expected
                def records(prefix):
                    return [json.loads(line.removeprefix(prefix))
                            for line in text.splitlines()
                            if line.startswith(prefix)]
                clients = records('STATE_CLIENT ')
                atoms = records('STATE_ATOM ')
                work = records('STATE_WORK ')
                names = records('STATE_NAME ')
                coverage = records('STATE_NAME_COVERAGE ')
                duplicates = records('STATE_DUPLICATE ')
                supported = records('STATE_SUPPORTED ')
                actions = records('STATE_ACTION_WORK ')
                struts = records('STATE_STRUT_WORK ')
                desktop_names = records('STATE_DESKTOP_NAME_WORK ')
                assert len(actions) == len(struts) == len(desktop_names) == 1
                assert actions[0]['cases'] == 80 and struts[0]['cases'] == 7
                baseline = actions[0]['baseline']
                assert actions[0]['selected_policy_calls'] == (9 if baseline else 6)
                assert struts[0]['selected_walks'] == (2 if baseline else 1)
                if desktop_names[0]['supported']:
                    assert (desktop_names[0]['selected_scans'] >= 126 if baseline
                            else desktop_names[0]['selected_scans'] == 63)
                assert len(names) == 4 and len(coverage) == 1
                assert all(r['indexed'] < r['legacy'] for r in names if r['key'] < 3)
                assert len(duplicates) == 3 and len(supported) == 3
                assert all(r['allocations'] == 0 for r in supported)
                assert len(clients) == 12 and len(atoms) == 2 and len(work) == 6
                assert {(r['windows'], r['reference']) for r in clients} == {
                    (n, ref) for n in (0, 1, 64, 65, 128, 256)
                    for ref in (0, 1)}
                assert {(r['windows'], r['reference']) for r in work} == {
                    (n, ref) for n in (64, 128, 256) for ref in (0, 1)}
                assert all(r['indexed_calls'] == 64 and
                           r['reference_entry_visits'] > 64 for r in atoms)
                runs.append({'mode': label, 'observations': sorted(expected),
                             'clients': clients, 'atoms': atoms, 'work': work,
                             'names': names, 'name_coverage': coverage,
                             'duplicates': duplicates, 'supported': supported,
                             'actions': actions, 'struts': struts,
                             'desktop_names': desktop_names})
            (out / 'evidence.json').write_text(
                json.dumps({'runs': runs}, indent=2) + '\n')
        finally:
            for log in runtime.root.glob('*.log'):
                shutil.copy2(log, out / log.name)


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--fvwm', type=Path, required=True)
    parser.add_argument('--artifacts-dir', type=Path, required=True)
    parser.add_argument('--display')
    run(parser.parse_args())
