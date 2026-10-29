#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License, version 2 or later.
"""Observe installed session startup and real core exec-Restart on nested X."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import sys
import subprocess
import time

from live_x_harness import HarnessRuntime, start_display_server, wait_for_predicate
from mfl_stream_live import Connection


def run(args):
    out = args.artifacts_dir.resolve()
    out.mkdir(parents=True, exist_ok=False)
    evidence = {'status': 'running', 'cases': {}, 'limits': [
        'Focused nested-X startup, not full login-manager/XSMP/service qualification.',
        'Application marker proves literal invocation/environment, not readiness.',
        'No UI change or new screenshot set; no physical/native-CPU claim.'],
        'artifacts': {str(p): hashlib.sha256(p.read_bytes()).hexdigest()
            for p in [args.session, args.fvwm, args.mfl]}}
    source = Path(__file__).resolve().parents[2]
    runtime = HarnessRuntime(prefix='fvwm-session-startup-')
    connections = []
    try:
        _, display = start_display_server(runtime, 'Xvfb',
            ['Xvfb', '-screen', '0', '800x600x24', '-noreset'],
            'xserver.log', time.monotonic() + 10)
        helper = runtime.root / 'record application'
        helper.write_text('#!' + sys.executable + '\n' + '''import json, os, sys
with open(sys.argv[1], 'a') as output:
    output.write(json.dumps({'argv':sys.argv[2:], 'pid':os.getpid(),
        'environment':{k:os.environ.get(k) for k in ['DISPLAY','XAUTHORITY',
        'XDG_CURRENT_DESKTOP','XDG_SESSION_TYPE','SESSION_MANAGER',
        'FVWM_SESSION_READY_FD','FVWM_SESSION_TEST_PRESERVE']}}) + '\\n')
''')
        helper.chmod(0o700)

        def entry(marker, extra=''):
            return ('[Desktop Entry]\nType=Application\nName=Native session fixture\n'
                f'Exec="{helper}" "{marker}" "spaces retained" "literal;pipe|&"\n'
                + extra)

        def rows(path):
            return [json.loads(s) for s in path.read_text().splitlines()] if path.exists() else []

        def start(name, mode='owned', stalled=False):
            home = runtime.root / name
            home.mkdir(mode=0o700)
            user, system, private = [home / n for n in ['user', 'system', 'runtime']]
            for p in [user, system, private]:
                p.mkdir(mode=0o700)
            for p in [user / 'autostart', system / 'autostart']:
                p.mkdir()
            marker, forbidden = home / 'application.jsonl', home / 'forbidden.jsonl'
            (system / 'autostart/a-visible.desktop').write_text(entry(marker,
                'NoDisplay=true\nOnlyShowIn=FVWM3;FVWM;\n'))
            (system / 'autostart/b-hidden.desktop').write_text(entry(forbidden))
            (user / 'autostart/b-hidden.desktop').write_text(
                '[Desktop Entry]\nHidden=true\n')
            (system / 'autostart/c-malformed.desktop').write_text(entry(forbidden))
            (user / 'autostart/c-malformed.desktop').write_text('malformed override\n')
            (system / 'autostart/d-filter.desktop').write_text(entry(forbidden,
                'NotShowIn=FVWM3;\n'))
            (system / 'autostart/e-missing.desktop').write_text(entry(forbidden,
                'TryExec=fvwm-session-fixture-program-is-missing\n'))
            config = home / 'configuration.fvwm'
            config.write_text(f'ModulePath {args.mfl.parent}\nRead '
                + str(source / 'fvwm/ConfigFvwmDefaults') + '\n'
                'DefaultFont fixed\nEdgeScroll 0 0\nEdgeThickness 0\n'
                'AddToFunc StartFunction I Module FvwmMFL\n'
                + ('+ I Wait SessionStartupNeverReady\n' if stalled else ''))
            if mode == 'packaged':
                modules = home / 'modules'
                modules.mkdir()
                build = args.mfl.parents[2]
                for directory in (build / 'modules').iterdir():
                    executable = directory / directory.name
                    if executable.is_file():
                        (modules / directory.name).symlink_to(executable)
                (modules / 'FvwmDesktop').symlink_to(build / 'bin/FvwmPrompt/FvwmDesktop')
                body = (source / 'default-config/config').read_text().replace(
                    '$[FVWM_DATADIR]/default-config', str(source / 'default-config'))
                config.write_text('ModulePath ' + str(modules) + '\nRead ' +
                    str(source / 'fvwm/ConfigFvwmDefaults') + '\n' + body +
                    '\nAddToFunc StartFunction I Module FvwmMFL\n')
            env = runtime.environment({'DISPLAY': 'fvwm-session-wrong-inherited:9', 'HOME': str(home),
                'FVWM_USERDIR': str(home), 'FVWMMFL_SOCKET_PATH': str(home),
                'FVWM_DESKTOP_ROOT': str(source / 'default-config'),
                'XDG_CONFIG_HOME': str(user), 'XDG_CONFIG_DIRS': str(system),
                'XDG_RUNTIME_DIR': str(private), 'FVWM_SESSION_TEST_PRESERVE': 'kept'})
            env.pop('SESSION_MANAGER', None)
            env.pop('XDG_CURRENT_DESKTOP', None)
            env.pop('XDG_SESSION_TYPE', None)
            env.pop('FVWM_SESSION_READY_FD', None)
            command = [str(args.session)]
            if mode == 'no-autostart':
                command.append('--no-autostart')
            elif mode == 'external':
                env['SESSION_MANAGER'] = 'invalid-session-fixture'
            elif mode == 'direct':
                command = [str(args.fvwm)]
            if mode != 'direct':
                command.append('--')
            command += ['-d', display, '-f', str(config)]
            process = runtime.start(name, command, name + '.log', env)
            endpoint = home / ('fvwm_mfl_' + display.replace(':', '_') + '.sock')
            wait_for_predicate(name + ' MFL', endpoint.exists,
                time.monotonic() + 12, process=process)
            return process, endpoint, marker, forbidden, home

        def connect(endpoint):
            c = Connection(endpoint)
            connections.append(c)
            assert 'connection_profile' in c.record()
            c.send(b'set echo\n')
            c.echo('session-startup-observed')
            return c

        def finish(process, control):
            control.send(b'Quit\n')
            wait_for_predicate('owned session normal completion',
                lambda: process.poll() is not None, time.monotonic() + 8)
            assert process.poll() == 0, process.poll()

        process, endpoint, marker, forbidden, home = start('owned')
        c = connect(endpoint)
        wait_for_predicate('one literal NoDisplay launch', lambda: len(rows(marker)) == 1,
            time.monotonic() + 10, process=process)
        observed = rows(marker)[0]
        assert observed['argv'] == ['spaces retained', 'literal;pipe|&'], observed
        env = observed['environment']
        assert env['XDG_CURRENT_DESKTOP'] == 'FVWM3:FVWM' and env['XDG_SESSION_TYPE'] == 'x11', env
        assert env['DISPLAY'] == display and env['FVWM_SESSION_TEST_PRESERVE'] == 'kept', env
        assert env['FVWM_SESSION_READY_FD'] is None and not forbidden.exists(), env
        log = runtime.root / 'owned.log'
        before = log.read_text()
        assert 'core startup acknowledged' in before, before
        child = int(re.search(r'core started pid=(\d+)', before).group(1))
        def child_identity():
            result = subprocess.run(['ps', '-o', 'pid=', '-o', 'ppid=', '-p', str(child)],
                capture_output=True, text=True, timeout=2, check=True)
            identity = [int(field) for field in result.stdout.split()]
            assert identity == [child, process.process.pid], identity
            return identity
        before_identity = child_identity()
        wrapper = process.process.pid
        c.send(b'Restart\n')
        c.expect_closed()
        c.close()
        # A successful new connection with an independent echo is the restart
        # witness; do not reuse a stale socket file as completion evidence.
        def restarted():
            if not endpoint.exists():
                return None
            try:
                peer = connect(endpoint)
            except (OSError, AssertionError):
                return None
            return peer
        c = wait_for_predicate('real WM Restart reconnect', restarted,
            time.monotonic() + 12, process=process)
        c.echo('session-after-real-restart')
        after_identity = child_identity()
        assert before_identity == after_identity
        assert process.process.pid == wrapper and process.poll() is None
        assert len(rows(marker)) == 1 and not forbidden.exists()
        after = log.read_text()
        assert after.count('core started pid=') == 1 and after.count('core startup acknowledged') == 1
        evidence['cases']['owned-restart'] = dict(status='passed', wrapper_pid=wrapper,
            wm_pid=child, process_identity_before=before_identity,
            process_identity_after=after_identity, independent_application=observed,
            autostart_count=1, reconnect='new MFL connection after old EOF')
        finish(process, c)

        for mode in ['no-autostart', 'external', 'direct']:
            process, endpoint, marker, forbidden, home = start(mode, mode)
            c = connect(endpoint)
            c.echo('core-usable-without-autostart')
            finish(process, c)
            assert not marker.exists() and not forbidden.exists(), mode
            evidence['cases'][mode] = {'status': 'passed', 'autostart_count': 0}

        process, endpoint, marker, forbidden, home = start('packaged', 'packaged')
        c = connect(endpoint)
        wait_for_predicate('packaged desktop autostart after core startup',
            lambda: len(rows(marker)) == 1, time.monotonic() + 15, process=process)
        assert not forbidden.exists()
        assert 'core startup acknowledged' in (runtime.root / 'packaged.log').read_text()
        finish(process, c)
        evidence['cases']['packaged-desktop-startup'] = {'status': 'passed',
            'autostart_count': 1, 'source_config': 'default-config/config',
            'scope': 'source-owned default config and reusable built modules in private stage'}

        process, endpoint, marker, forbidden, home = start('stalled', stalled=True)
        c = connect(endpoint)
        log = runtime.root / 'stalled.log'
        wait_for_predicate('real startup acknowledgment deadline',
            lambda: 'autostart unavailable' in log.read_text(),
            time.monotonic() + 18, process=process)
        assert not marker.exists() and not forbidden.exists() and process.poll() is None
        c.echo('WM-still-usable-after-readiness-deadline')
        finish(process, c)
        evidence['cases']['stalled-core-startup'] = {'status': 'passed',
            'autostart_count': 0, 'core_usable_after_deadline': True}
        evidence['status'] = 'passed'
    finally:
        for c in connections:
            c.close()
        for path in runtime.root.rglob('*'):
            if path.is_file() and not path.is_symlink() and path.suffix in ['.log', '.jsonl', '.fvwm', '.desktop']:
                target = out / path.relative_to(runtime.root)
                target.parent.mkdir(parents=True, exist_ok=True)
                shutil.copyfile(path, target)
        (out / 'evidence.json').write_text(json.dumps(evidence, indent=2) + '\n')
        runtime.close()


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--session', type=Path, required=True)
    parser.add_argument('--fvwm', type=Path, required=True)
    parser.add_argument('--mfl', type=Path, required=True)
    parser.add_argument('--artifacts-dir', type=Path, required=True)
    run(parser.parse_args())
