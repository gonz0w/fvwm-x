#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or any later version.
"""Run Go client fixtures on an isolated BSD VM's configured default Xorg."""
import argparse
import grp
import json
import os
from pathlib import Path
import secrets
import shutil
import struct
import subprocess
import sys
import time

sys.path.insert(0, str(Path(__file__).resolve().parent / 'live_x'))
from live_x_harness import HarnessRuntime, wait_for_predicate


def authority_record(number, cookie):
    # Xauthority FamilyWild applies to this private display on either BSD.
    fields = [b'', str(number).encode('ascii'), b'MIT-MAGIC-COOKIE-1', cookie]
    return struct.pack('!H', 65535) + b''.join(
        struct.pack('!H', len(field)) + field for field in fields)


def preflight(system, disposable, number):
    if system not in ('FreeBSD', 'OpenBSD'):
        raise RuntimeError('This driver requires FreeBSD or OpenBSD.')
    if not disposable:
        raise RuntimeError('Use --disposable-vm only in an isolated test VM.')
    if not 1 <= number <= 65535:
        raise RuntimeError('Display number must be between 1 and 65535.')
    existing = subprocess.run(['pgrep', '-x', 'Xorg'], capture_output=True,
                              timeout=5)
    if existing.returncode != 1:
        raise RuntimeError('Existing Xorg or failed process inventory; preserved.')
    if (Path('/tmp') / ('.X%d-lock' % number)).exists() or (
            Path('/tmp/.X11-unix') / ('X%d' % number)).exists():
        raise RuntimeError('Display socket/lock exists; preserved.')


def query_ready(display, env):
    try:
        result = subprocess.run(['xrandr', '--display', display, '--query'],
                                env=env, capture_output=True, text=True,
                                timeout=2)
    except subprocess.TimeoutExpired:
        # Startup can outlast one query; the owning readiness deadline stays
        # authoritative and subprocess.run reaps its timed-out query.
        return None
    return result.stdout if result.returncode == 0 else None


def run(args):
    preflight(os.uname().sysname, args.disposable_vm, args.display_number)
    build = args.build_directory.resolve()
    source = Path(__file__).resolve().parent.parent
    artifacts = args.artifacts_dir.resolve()
    # Never overwrite evidence from an earlier invocation.
    artifacts.mkdir(parents=True, exist_ok=False)
    display = ':' + str(args.display_number)
    evidence = {'system': list(os.uname()), 'display': display,
                'transport': 'local Unix socket with private Xauthority',
                'fixtures': {}}
    with HarnessRuntime(prefix='bsd-default-x-') as runtime:
        try:
            authority = runtime.root / 'Xauthority'
            with authority.open('xb') as output:
                authority.chmod(0o600)
                output.write(authority_record(args.display_number,
                                               secrets.token_bytes(16)))
            if os.uname().sysname == 'OpenBSD':
                # Xenocara rereads authority after dropping to _x11. Share
                # access only with that server group, never world-read it.
                server_group = grp.getgrnam('_x11').gr_gid
                os.chown(runtime.root, -1, server_group)
                runtime.root.chmod(0o710)
                os.chown(authority, -1, server_group)
                authority.chmod(0o640)
            env = runtime.environment({'XAUTHORITY': str(authority),
                                       'DISPLAY': display})
            server = runtime.start('default-xorg',
                ['Xorg', display, '-nolisten', 'tcp', '-noreset',
                 '-auth', str(authority)], 'xorg.log', env)

            evidence['randr'] = wait_for_predicate('default Xorg readiness',
                lambda: query_ready(display, env), time.monotonic() + 15,
                process=server)
            unauthenticated = subprocess.run(
                ['xrandr', '--display', display, '--query'], capture_output=True,
                env=dict(env, XAUTHORITY=str(runtime.root / 'absent-authority')),
                timeout=5)
            if unauthenticated.returncode == 0:
                raise RuntimeError('Default Xorg accepted a client without its cookie.')
            evidence['unauthenticated_client_refused'] = True
            clients = build / 'install-stage/usr/local/bin' 
            fixtures = [
                ('command', ['--command', str(clients / 'FvwmCommand')]),
                ('prompt', ['--prompt', str(clients / 'FvwmPrompt'),
                    '--perl-module', str(build / 'modules/FvwmPerl/FvwmPerl'),
                    '--perllib', str(source / 'perllib'),
                    '--module-library', str(build / 'perllib/Module.pm')]),
            ]
            if args.xlock_only:
                fixtures = [('xlock', ['--xlock', str(clients / 'fvwm-menu-xlock')])]
            if args.directory_only:
                fixtures = [('directory', ['--directory', str(clients / 'fvwm-menu-directory')])]
            if args.script_com_only:
                data = build / 'install-stage/usr/local/share/fvwm3'
                fixtures = [('script-com', ['--script', str(build / 'modules/FvwmScript/FvwmScript'),
                    '--helper', str(data / 'fvwm-script-ComExample'),
                    '--example', str(data / 'FvwmScript-ComExample')])]
            if args.desktop_menu_only:
                fixtures = [('desktop-menu', ['--provider', str(clients / 'fvwm-menu-desktop'),
                    '--form', str(build / 'modules/FvwmForm/FvwmForm')])]
            for name, extra in fixtures:
                script = {'command': 'command_client_live.py',
                          'prompt': 'prompt_protocol_live.py',
                          'xlock': 'xlock_menu_live.py', 'directory': 'directory_menu_live.py',
                          'script-com': 'script_com_example_live.py',
                          'desktop-menu': 'xdg_menu_live.py'}[name]
                argv = [sys.executable, str(source / 'tests/live_x' / script),
                    '--fvwm', str(build / 'fvwm3'),
                    '--mfl', str(build / 'modules/FvwmMFL/FvwmMFL'),
                    '--display', display, '--artifacts-dir',
                    str(artifacts / name), *extra]
                child = runtime.start(name, argv, name + '.log', env)
                status = child.wait_until_exit(time.monotonic() + 90)
                evidence['fixtures'][name] = {'command': argv, 'status': status}
                if status != 0:
                    raise RuntimeError(name + ' default-X fixture failed.')
            if server.poll() is not None:
                raise RuntimeError('Default Xorg exited during the fixtures.')
        finally:
            for log in runtime.root.glob('*.log'):
                shutil.copy2(log, artifacts / log.name)
            server_log = Path('/var/log') / (
                'Xorg.%d.log' % args.display_number)
            if server_log.exists():
                shutil.copy2(server_log, artifacts / 'Xorg-server.log')
            (artifacts / 'results.json').write_text(
                json.dumps(evidence, indent=2) + '\n')
    print('Default BSD Xorg client fixtures passed:', artifacts)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__, allow_abbrev=False)
    parser.add_argument('--build-directory', type=Path, required=True)
    parser.add_argument('--artifacts-dir', type=Path, required=True)
    parser.add_argument('--script-com-only', action='store_true',
                        help='Run only the installed Script communication example.')
    parser.add_argument('--desktop-menu-only', action='store_true',
                        help='Run only the installed XDG menu and native settings Form.')
    parser.add_argument('--directory-only', action='store_true',
                        help='Run only the installed directory menu fixture.')
    parser.add_argument('--xlock-only', action='store_true',
                        help='Run only the installed xlock menu fixture.')
    parser.add_argument('--display-number', type=int, default=197)
    parser.add_argument('--disposable-vm', action='store_true')
    run(parser.parse_args())
