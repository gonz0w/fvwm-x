#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or any later version.
"""One owned-Xvfb V1 binding-origin checkpoint and Slate native capture.

Expected file spans come from the literals below, not the source formatter.
Probe and worker wrappers forward actual product bytes; inspection never runs
fixture action strings. Run only through the lead's admitted native profile.
"""
import argparse
import ctypes as C
import json
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import sys
import time
import traceback

from desktop_live import connect_client
from inspector_live import NativeView, Probe, TITLE, form_text, inspector_frame
from live_x_harness import HarnessRuntime, start_display_server, wait_for_predicate
from mfl_request_live import frame, reply
from mfl_stream_live import Connection
from session_x_errors import SessionXErrors
from settings_snapshot import expanded_settings

SECRET = 'V1_PRIVATE_ACTION_ARGUMENT'
WINDOW = 'V1_PRIVATE_WINDOW_PATTERN'


def location(path, first, last=None):
    return dict(path=str(path), first=first, last=first if last is None else last)


def expect_source(actual, path, first, last, included, kind='file'):
    wanted = dict(version=1, kind=kind, **location(path, first, last),
                  include=included)
    for key, value in wanted.items():
        assert actual.get(key) == value, (actual, wanted)


def shell_wrapper(path, argv, suffix=''):
    path.unlink(missing_ok=True)
    path.write_text('#!/bin/sh\nexec ' + shlex.join([str(x) for x in argv]) +
                    suffix + '\n')
    path.chmod(0o700)


def retain(runtime, out):
    """Copy diagnostics while native clients and server are still alive."""
    for name in ('native', 'worker-records', 'probe', 'user'):
        path = runtime.root / name
        if path.exists():
            shutil.copytree(path, out / name, dirs_exist_ok=True)
    for path in runtime.root.iterdir():
        if path.is_file() and not path.is_symlink():
            shutil.copy2(path, out / path.name)


def run(args, scenario=None):
    source, build = Path(__file__).resolve().parents[2], args.build.resolve()
    out = args.artifacts_dir.resolve()
    out.mkdir(parents=True, exist_ok=False)
    evidence = dict(status='running', profile='slate-dark', cases={}, screenshots=[])
    runtime = HarnessRuntime(prefix='fvwm-binding-origin-')
    client = errors = core = server = None
    connections = []
    cleanup_failures = []
    started = time.monotonic()
    try:
        required = {'core': build / 'fvwm3',
            'desktop': build / 'bin/FvwmPrompt/FvwmDesktop',
            'worker': build / 'bin/FvwmPrompt/fvwm-desktop-worker',
            'svg': args.svg_worker.resolve() if args.svg_worker else
                build / 'bin/fvwm-image-svg-worker',
            'backer': args.backer.resolve() if args.backer else
                build / 'modules/FvwmBacker/FvwmBacker',
            'form': build / 'modules/FvwmForm/FvwmForm',
            'mfl': build / 'modules/FvwmMFL/FvwmMFL',
            'capture': build / 'bin/fvwm-desktop-capture'}
        for name, executable in required.items():
            assert executable.is_file() and os.access(executable, os.X_OK), (name, executable)
        evidence['executables'] = {key: str(value) for key, value in required.items()}
        server, display = start_display_server(runtime, 'Xvfb',
            ['Xvfb', '-screen', '0', '1440x1200x24', '-noreset'],
            'xserver.log', time.monotonic() + 10)
        user, modules = runtime.root / 'user', runtime.root / 'modules'
        records, native, observer = [runtime.root / name for name in
                                    ('worker-records', 'native', 'probe')]
        for directory in (user, modules, records, native, observer):
            directory.mkdir(mode=0o700)
        for directory in (build / 'modules').iterdir():
            executable = directory / directory.name
            if executable.is_file():
                (modules / directory.name).symlink_to(executable)
        shell_wrapper(modules / 'FvwmBacker', [required['backer']], ' "$@"')
        for name, executable in (('FvwmDesktop', required['desktop']),
                                 ('FvwmForm', required['form'])):
            shell_wrapper(modules / name, [sys.executable, '-E', '-S', '-B',
                source / 'tests/live_x/native_module_proxy.py', executable, native], ' "$@"')
        inspector = source / 'tests/live_x/inspector_live.py'
        shell_wrapper(modules / 'InspectorProbe', [sys.executable, '-B', inspector,
            '--probe'], ' "$@" 2>' + shlex.quote(str(runtime.root / 'probe.log')))
        wrapper = runtime.root / 'worker-probe'
        shell_wrapper(wrapper, [sys.executable, '-B', inspector, '--worker',
                               required['worker'], records])
        local = user / 'local.config'
        local.write_text('*FvwmDesktop: palette slate\n*FvwmDesktop: appearance dark\n'
                         '*FvwmDesktop: transparency Off\n*FvwmDesktop: motion Off\n')
        assets = source / 'default-config'
        body = (assets / 'config').read_text().replace('$[FVWM_DATADIR]/default-config', str(assets))
        config = runtime.root / 'boot.config'
        config.write_text('ModulePath ' + str(modules) + '\nRead ' +
            str(source / 'fvwm/ConfigFvwmDefaults') + '\n' + body +
            '\nAddToFunc StartFunction I Module InspectorProbe ' + str(observer) + '\n')
        env = runtime.environment({'DISPLAY': display, 'HOME': str(runtime.root),
            'FVWM_USERDIR': str(user), 'FVWM_DESKTOP_ROOT': str(assets),
            'FVWM_DESKTOP_WORKER': str(wrapper), 'FVWM_IMAGE_SVG_WORKER': str(required['svg']),
            'FVWMMFL_SOCKET_PATH': str(runtime.root), 'XDG_DATA_HOME': str(runtime.root / 'data'),
            'XDG_DATA_DIRS': str(runtime.root / 'absent')})
        client = connect_client(display)
        errors = SessionXErrors(client, evidence).install()
        x, d = client.x, client.display
        x.XStringToKeysym.argtypes, x.XStringToKeysym.restype = [C.c_char_p], C.c_ulong
        x.XKeysymToKeycode.argtypes = [C.c_void_p, C.c_ulong]
        x.XKeysymToKeycode.restype = C.c_ubyte
        x.XChangeKeyboardMapping.argtypes = [C.c_void_p, C.c_int, C.c_int,
                                           C.POINTER(C.c_ulong), C.c_int]
        x.XChangeKeyboardMapping.restype = C.c_int
        symbol = x.XStringToKeysym(b'F12')
        previous = x.XKeysymToKeycode(d, symbol)
        if previous:
            x.XChangeKeyboardMapping(d, previous, 1, (C.c_ulong * 1)(0), 1)
        x.XChangeKeyboardMapping(d, 250, 1, (C.c_ulong * 2)(symbol, symbol), 2)
        x.XSync(d, 0)
        evidence['keyboard_mapping'] = dict(cleared=previous, added=[250, 251])
        core = runtime.start('core', [str(required['core']), '-d', display,
            '-v', '-f', str(config), '-o', '-'], 'core.log', env)
        wait_for_predicate('real observer ready', (observer / 'ready.json').exists,
                           time.monotonic() + 10, process=core)
        probe = Probe(observer, core)
        probe.barrier()
        view = NativeView(client, errors, core)
        client.create()
        x.XStoreName(d, client.window, b'Binding origin focus probe')
        x.XSync(d, 0)
        view.wait('Binding origin focus probe')
        probe.command('Module FvwmMFL')
        endpoint = runtime.root / ('fvwm_mfl_' + display.replace(':', '_') + '.sock')
        wait_for_predicate('explicit MFL endpoint', endpoint.exists,
                           time.monotonic() + 10, process=core)
        for _ in range(2):
            connection = Connection(endpoint)
            connections.append(connection)
            assert 'connection_profile' in connection.record()
        rpc, control = connections
        rpc.send(frame('readonly', 'set_mode', mode='read-only'))
        assert reply(rpc, 'readonly')['read_only']
        control.send(b'set echo\n')
        control.echo('binding-origin-ready')
        serial = 0

        def query(resource='bindings', status='ok', **fields):
            nonlocal serial
            serial += 1
            token = 'v1-' + str(serial)
            rpc.send(frame(token, resource=resource, **fields))
            return reply(rpc, token, status)

        def snapshot():
            result = {}
            for scope in ('global', 'menu', 'menu-fallback'):
                page = query(scope=scope, limit=16)
                generation, rows = page['generation'], []
                while True:
                    for row in page['bindings']:
                        facts = row['provenance']
                        assert set(facts) == {'version', 'source', 'replaced_existing',
                                             'position', 'selection', 'effective'}
                        assert type(facts['version']) is int and facts['version'] == 1
                        assert type(facts['replaced_existing']) is bool
                        assert type(facts['position']) is int and facts['position'] == len(rows)
                        assert facts['selection'] == ('window-specific-before-global' if
                            scope == 'global' else 'menu-before-fallback')
                        assert facts['effective'] == 'requires-event-context'
                        assert type(facts['source']['version']) is int and facts['source']['version'] == 1
                        assert facts['source']['kind'] in ('file', 'generated', 'invocation', 'unavailable')
                        assert len(json.dumps(facts['source'], separators=(',', ':')).encode()) <= 1536
                        assert row['type'] in ('Key', 'PointerKey', 'MousePress', 'MouseRelease')
                        assert all(type(row[key]) is int for key in ('code', 'context_mask', 'modifier_mask'))
                        rows.append(row)
                    assert len(rows) <= 4096
                    if page['next_offset'] is None:
                        break
                    assert page['next_offset'] == len(rows)
                    page = query(scope=scope, offset=len(rows), generation=generation, limit=16)
                    assert page['generation'] == generation
                result[scope] = dict(generation=generation, entries=rows)
            return result

        def settings_ready():
            nonlocal serial
            serial += 1
            token = 'v1-settings-' + str(serial)
            rpc.send(frame(token, resource='settings', scope='all'))
            response = rpc.record()['reply']
            assert response['id'] == token and response['version'] == 1
            if response['status'] != 'ok':
                return None
            value = expanded_settings(response['result'])
            return value if value['settings']['palette']['application'] == 'submitted-to-fvwm' else None

        before_settings = wait_for_predicate('Slate owner settings', settings_ready,
                                            time.monotonic() + 12, process=core)
        assert before_settings['settings']['palette']['effective'] == 'slate'
        if scenario is not None:
            # Share only owned display/module setup and teardown. Each scene
            # supplies its own acceptance; old binding cases are not replayed.
            scenario(runtime=runtime, out=out, user=user, probe=probe,
                query=query, snapshot=snapshot, before_settings=before_settings,
                view=view, core=core, evidence=evidence, required=required,
                records=records, env=env)
            assert not errors.unexpected, errors.unexpected
            evidence['status'] = 'passed'
            return
        entry, leaf = user / 'entry.config', user / 'leaf.config'
        entry.write_text('# independent nested Read coordinates\nRead "' + str(leaf) + '"\n')
        leaf.write_text('# physical continuation\nKey F12 R N \\\n Echo V1_KEY ' + SECRET + '\n'
            'PointerKey F11 R N Echo V1_POINTER ' + SECRET + '\n'
            'Mouse 2 R N Echo V1_MOUSE ' + SECRET + '\n'
            'Key (' + WINDOW + '*) F10 W N Echo V1_WINDOW ' + SECRET + '\n'
            'Key F9 M N MenuClose\nMouse 2 MI N MenuSelectItem\n'
            'AddToFunc V1BindingOrigin I Key F8 R N Echo V1_FUNCTION\nV1BindingOrigin\n'
            'PipeRead "printf \'Key F7 R N Echo V1_GENERATED\'"\n')
        outside = runtime.root / 'outside.config'
        outside.write_text('Key F6 R N Echo V1_OUTSIDE ' + SECRET + '\n')
        for path in (entry, leaf, outside):
            shutil.copy2(path, out / path.name)
        probe.command('Read "' + str(outside) + '"', 'Read "' + str(entry) + '"')
        probe.barrier()
        for path in (entry, leaf, outside):
            path.unlink()
        baseline = snapshot()
        assert not any(path.exists() for path in (entry, leaf, outside))
        evidence['raw_after_read'] = baseline
        chain = [location(entry, 2)]
        global_rows = baseline['global']['entries']
        own = lambda action: [row for row in global_rows if row['action'].startswith(action)]
        keys = own('Echo V1_KEY ')
        assert len(keys) == 2 and {row['code'] for row in keys} == {250, 251}, keys
        for row in keys:
            assert row['provenance']['replaced_existing'] is False
        for prefix, first, last, kind in [('Echo V1_KEY ', 2, 3, 'file'),
                ('Echo V1_POINTER ', 4, 4, 'file'), ('Echo V1_MOUSE ', 5, 5, 'file'),
                ('Echo V1_WINDOW ', 6, 6, 'file'), ('Echo V1_FUNCTION', 10, 10, 'invocation'),
                ('Echo V1_GENERATED', 11, 11, 'generated')]:
            matches = own(prefix)
            assert matches, prefix
            for row in matches:
                expect_source(row['provenance']['source'], leaf, first, last, chain, kind)
        assert own('Echo V1_POINTER ')[0]['type'] == 'PointerKey'
        assert own('Echo V1_MOUSE ')[0]['type'] == 'MousePress'
        assert own('Echo V1_WINDOW ')[0]['window'] == WINDOW + '*'
        for action, line in [('MenuClose', 7), ('MenuSelectItem', 8)]:
            matches = [row for row in baseline['menu']['entries'] if
                       row['provenance']['source'].get('path') == str(leaf) and row['action'] == action]
            assert matches, action
            for row in matches:
                expect_source(row['provenance']['source'], leaf, line, line, chain)
        assert any(row['provenance']['source']['kind'] == 'unavailable' for row in
                   baseline['menu-fallback']['entries']), 'builtin fallback source must be unavailable'
        evidence['cases']['retained_nested_continued_expanded_and_source_kinds'] = True
        x.XSetInputFocus.argtypes = [C.c_void_p, C.c_ulong, C.c_int, C.c_ulong]

        def press_binding(expected):
            view.pointer.motion(1100, 950)
            x.XSetInputFocus(d, client.root, 1, 0)
            for pressed in (1, 0):
                assert view.pointer.x.XTestFakeKeyEvent(d, 250, pressed, 0)
            x.XSync(d, 0)
            control.expect_echo(expected)

        press_binding('V1_KEY ' + SECRET)
        # This new literal is inspected only; never submit it to Read.
        forbidden = 'V1_INSPECTION_MUST_NOT_EXECUTE'
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
                        assert value['returncode'] == 0 and value['response']['version'] == 1, value
                        return value['response']['result']
            return wait_for_predicate('genuine binding worker ' + filter_, ready,
                                      time.monotonic() + 10, process=core)

        def native_page(previous=None):
            def ready():
                lines = probe.snapshot()
                token = inspector_frame(lines)
                text = form_text(lines)
                window = view.named(TITLE)
                if token and token != previous and window and 'Snapshot scope: bindings' in text and \
                        'Separate point-in-time observations' in text:
                    return dict(window=window, token=token, lines=lines)
            return wait_for_predicate('published binding inspector', ready,
                                      time.monotonic() + 10, process=core)

        def focus(page):
            probe.command('WindowId ' + str(page['window']) + ' Focus')
            wait_for_predicate('Form keyboard focus', lambda: view.focus() == page['window'],
                               time.monotonic() + 5, process=core)

        probe.command('SendToModule FvwmDesktop inspect bindings')
        projection, page = worker_record(''), native_page()
        rows = projection['sections'][0]['rows']
        serialized = json.dumps(projection)
        for hidden in (SECRET, WINDOW, str(runtime.root)):
            assert hidden not in serialized, hidden
        for text in ('requires event context', 'literal definition', 'enclosing context only',
                     'same identity matched at definition=', 'outside:(hidden)'):
            assert any(text in row['value'] for row in rows), text
        assert any(row['value'] == 'user:leaf.config:2-3' for row in rows), rows
        assert any(row['value'] == 'user:entry.config:2-2' for row in rows), rows
        evidence['projection'] = projection
        focus(page)
        view.key('u', control=True)
        for symbol_name in ('f', '1', '2'):
            view.key(symbol_name)
        view.key('Return')
        filtered, page = worker_record('f12'), native_page(page['token'])
        filtered_rows = filtered['sections'][0]['rows']
        assert any(row['value'] == 'user:leaf.config:2-3' for row in filtered_rows)
        assert any('include 1/1' in row['label'] and row['value'] ==
                   'user:entry.config:2-2' for row in filtered_rows)
        evidence['filtered_projection'] = filtered
        # Two physical rows per Form page; navigate with genuine F2 callbacks.
        target = next(i for i, row in enumerate(filtered_rows) if
                      row['value'] == 'user:leaf.config:2-3') // 2
        for _ in range(target):
            focus(page)
            view.key('F2')
            page = native_page(page['token'])
        shown = form_text(page['lines'])
        assert 'user:leaf.config:2-3' in shown
        assert 'user:entry.config:2-2' in shown, shown
        evidence['capture_form'] = page['lines']
        evidence['capture_pixels'] = view.pixels(page['window'])
        captured = subprocess.run([str(required['capture']), str(out)], env=env,
            text=True, capture_output=True, timeout=35, check=False)
        assert captured.returncode == 0 and captured.stdout.startswith('Screenshot saved: '), captured.stderr
        Path(captured.stdout.strip().removeprefix('Screenshot saved: ')).replace(
            out / 'binding-origin-slate-dark.png')
        evidence['screenshots'].append('binding-origin-slate-dark.png')
        focus(page)
        view.key('Escape')
        wait_for_predicate('Escape closes native inspector', lambda: view.named(TITLE) is None,
                           time.monotonic() + 5, process=core)
        assert snapshot() == baseline, 'inspection mutated binding state'
        assert expanded_settings(query('settings', scope='all')) == before_settings
        assert local.read_bytes() == local_before
        assert forbidden not in (runtime.root / 'core.log').read_text(errors='replace')
        evidence['cases']['genuine_projection_filter_f2_escape_nonexecuting'] = True

        replacement = user / 'replacement.config'
        replacement.write_text('# replacement definition\nKey F12 R N Echo V1_REPLACED\n')
        shutil.copy2(replacement, out / replacement.name)
        probe.command('Read "' + str(replacement) + '"')
        replacement.unlink()
        replaced = snapshot()
        assert query(scope='global', offset=1, limit=1,
            generation=baseline['global']['generation'], status='error') == 'stale_generation'
        matching = [row for row in replaced['global']['entries'] if row['key'] == 'F12']
        assert len(matching) == 2, matching
        for row in matching:
            assert row['provenance']['replaced_existing'] is True
            expect_source(row['provenance']['source'], replacement, 2, 2, [])
        press_binding('V1_REPLACED')
        evidence['raw_replaced'] = replaced
        probe.command('Key F12 R N -')
        unbound = snapshot()
        assert not any(row['key'] == 'F12' for row in unbound['global']['entries'])
        for scope in ('menu', 'menu-fallback'):
            assert unbound[scope]['entries'] == baseline[scope]['entries']
        assert expanded_settings(query('settings', scope='all')) == before_settings
        evidence['raw_unbound'] = unbound
        evidence['cases']['replacement_stale_generation_xtest_unbind'] = True
        assert not errors.unexpected, errors.unexpected
        evidence['status'] = 'passed'
    except BaseException as failure:
        evidence.update(status='failed', failure=repr(failure), traceback=traceback.format_exc())
        raise
    finally:
        evidence['elapsed_seconds'] = time.monotonic() - started
        # First preserve live diagnostic state; cleanup must not erase its cause.
        try:
            retain(runtime, out)
        except BaseException as failure:
            cleanup_failures.append('retain: ' + repr(failure))
        (out / 'evidence.json').write_text(json.dumps(evidence, indent=2) + '\n')
        for connection in connections:
            try:
                connection.close()
            except BaseException as failure:
                cleanup_failures.append('connection: ' + repr(failure))
        if (errors or client) and server and server.poll() is None:
            try:
                errors.close_display() if errors else client.close()
            except BaseException as failure:
                cleanup_failures.append('X client: ' + repr(failure))
        elif client:
            # Xlib's fatal I/O path bypasses Python cleanup on a dead server.
            cleanup_failures.append('X client close skipped: owned server exited')
        # Core owns its module children. Terminate it before the private server.
        for process in reversed(runtime.processes):
            try:
                process.close(time.monotonic() + 4)
            except BaseException as failure:
                cleanup_failures.append(process.name + ': ' + repr(failure))
        evidence['children_after_cleanup'] = {p.name: p.poll() for p in runtime.processes}
        try:
            retain(runtime, out)  # Add flushed final proxy/process receipts.
        except BaseException as failure:
            cleanup_failures.append('retain final: ' + repr(failure))
        try:
            runtime.close()
        except BaseException as failure:
            cleanup_failures.append('runtime: ' + repr(failure))
        # close_display synchronizes pending X requests; errors discovered
        # there must fail standalone execution as well as the guest wrapper.
        if errors and errors.unexpected:
            cleanup_failures.append('observer unexpected X errors: ' +
                                    repr(errors.unexpected))
        evidence['cleanup_failures'] = cleanup_failures
        if cleanup_failures:
            evidence['status'] = 'failed'
        (out / 'evidence.json').write_text(json.dumps(evidence, indent=2) + '\n')
        if cleanup_failures and sys.exc_info()[0] is None:
            raise AssertionError(cleanup_failures)
    print('binding origin Slate checkpoint passed')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__, allow_abbrev=False)
    parser.add_argument('--build', required=True, type=Path)
    parser.add_argument('--artifacts-dir', required=True, type=Path)
    parser.add_argument('--svg-worker', type=Path)
    parser.add_argument('--backer', type=Path)
    run(parser.parse_args())
