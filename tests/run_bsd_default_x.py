#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or any later version.
"""Check server startup or client fixtures on an isolated BSD VM's default Xorg."""
import argparse
import grp
import hashlib
import json
import os
import re
from pathlib import Path
import secrets
import shutil
import struct
import subprocess
import sys
import time

sys.path.insert(0, str(Path(__file__).resolve().parent / 'live_x'))
from live_x_harness import (HarnessCancelled, HarnessError, HarnessRuntime,
                            wait_for_predicate)


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


def log_identity(path):
    """Distinguish this invocation's vendor log from an unchanged old log."""
    try:
        status = path.stat()
    except FileNotFoundError:
        return None
    return (status.st_dev, status.st_ino, status.st_size,
            status.st_mtime_ns, status.st_ctime_ns)


def file_identity(path):
    digest = hashlib.sha256()
    with path.open('rb') as stream:
        for chunk in iter(lambda: stream.read(128 * 1024), b''):
            digest.update(chunk)
    return {'path': str(path), 'resolved': str(path.resolve()),
            'sha256': digest.hexdigest(), 'size': path.stat().st_size}


def dependency_identity(system, server_log):
    """Observe the installed server and loaded modules; never change packages."""
    result = {'files': [], 'errors': []}
    executable = shutil.which('Xorg')
    paths = {Path(executable)} if executable else set()
    if not executable:
        result['errors'].append('Xorg executable not found')
    if system == 'FreeBSD':
        # The vendor's /usr/local/bin/Xorg shell launcher selects Xorg.wrap
        # before the real server. Observe both; do not parse or execute scripts.
        paths.update(path for path in (Path('/usr/local/libexec/Xorg'),
                     Path('/usr/local/libexec/Xorg.wrap')) if path.is_file())
    if server_log.is_file():
        # Only absolute vendor module paths reported by this owned server.
        paths.update(Path(value) for value in re.findall(
            r'Loading (/usr/(?:local|X11R6)/[^\r\n]+\.so)',
            server_log.read_text(errors='replace')))
    for path in sorted(paths):
        try:
            result['files'].append(file_identity(path))
        except OSError as error:
            result['errors'].append(str(error))
    if system == 'OpenBSD':
        try:
            patches = subprocess.run(['syspatch', '-l'], capture_output=True,
                                     text=True, timeout=5)
            result['syspatch'] = {'status': patches.returncode,
                                  'stdout': patches.stdout,
                                  'stderr': patches.stderr}
        except (OSError, subprocess.TimeoutExpired) as error:
            result['errors'].append(str(error))
    return result


def fixture_commands(args, build, source):
    if args.server_only:
        return []
    if (getattr(args, 'complete_preview_only', False) or
            getattr(args, 'complete_preview_tail_only', False)):
        paths = {'fvwm': args.fvwm or build / 'fvwm3',
            'mfl': args.mfl or build / 'modules/FvwmMFL/FvwmMFL',
            'backer': args.backer or build / 'modules/FvwmBacker/FvwmBacker',
            'probe': args.preview_probe or build / 'tests/image-owner-probe',
            'worker': args.image_worker or build / 'bin/fvwm-image-svg-worker',
            'config-header': args.preview_config_header or build / 'config.h'}
        options = ['--complete-bundle-only', '--execution-profile',
                   args.execution_profile]
        if getattr(args, 'complete_preview_tail_only', False):
            options.append('--complete-bundle-tail-only')
        for name, path in paths.items():
            options.extend(['--' + name, str(path.resolve())])
        return [('complete-preview', options)]
    if getattr(args, 'tray_only', False):
        paths = {'fvwm': args.fvwm or build / 'fvwm3',
            'tray': args.tray or build / 'modules/FvwmTray/FvwmTray',
            'buttons': args.buttons or
                build / 'modules/FvwmButtons/FvwmButtons',
            'form': args.form or build / 'modules/FvwmForm/FvwmForm',
            'desktop': args.desktop or build / 'bin/FvwmPrompt/FvwmDesktop',
            'session': args.session or build / 'bin/FvwmPrompt/fvwm-session',
            'worker': args.worker or
                build / 'bin/FvwmPrompt/fvwm-desktop-worker',
            'mfl': args.mfl or build / 'modules/FvwmMFL/FvwmMFL',
            'client': args.tray_client or build / 'tests/tray-client',
            'sni-client': args.sni_client or build / 'tests/tray-sni-client',
            'capture': args.capture or build / 'bin/fvwm-desktop-capture'}
        options = ['--build', str(build), '--phase', 'full',
            '--execution-profile', args.execution_profile]
        for name, path in paths.items():
            options.extend(['--' + name, str(path.resolve())])
        if args.expect_modern_unavailable:
            options.append('--expect-modern-unavailable')
        if args.expect_no_container:
            options.append('--expect-no-container')
        return [('tray', options)]
    if args.notify_only:
        paths = {'fvwm': args.fvwm or build / 'fvwm3',
            'notify': args.notify or build / 'modules/FvwmNotify/FvwmNotify',
            'mfl': args.mfl or build / 'modules/FvwmMFL/FvwmMFL',
            'client': args.notify_client or build / 'tests/notify-client',
            'capture': args.capture or build / 'bin/fvwm-desktop-capture'}
        options = ['--build', str(build), '--execution-profile',
                   args.execution_profile]
        for name, path in paths.items():
            options.extend(['--' + name, str(path.resolve())])
        return [('notify', options)]
    if args.desktop_controller_only:
        stage = build / 'install-stage'
        prefix = stage / 'usr/local'
        controllers = sorted(path for path in
            prefix.glob('libexec/fvwm3/*/FvwmDesktop') if path.is_file())
        if len(controllers) != 1:
            raise RuntimeError('Expected exactly one staged FvwmDesktop under ' +
                               str(prefix / 'libexec/fvwm3') +
                               '; found ' + str(len(controllers)) + '.')
        controller = controllers[0]
        data = prefix / 'share/fvwm3'
        if not all(path.resolve().is_relative_to(stage.resolve())
                   for path in (controller, data)):
            raise RuntimeError('Desktop controller/data must remain inside '
                               'the install stage.')
        if not (data / 'default-config/config').is_file():
            raise RuntimeError('Missing staged desktop configuration: ' +
                               str(data / 'default-config/config'))
        return [('desktop-controller', ['--build', str(build),
            '--installed-desktop', str(controller), '--installed-data', str(data),
            '--settings-only'])]
    clients = build / 'install-stage/usr/local/bin' if build else Path()
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
    return fixtures


def run_once(args):
    build = args.build_directory.resolve() if args.build_directory else None
    source = Path(__file__).resolve().parent.parent
    artifacts = args.artifacts_dir.resolve()
    # Never overwrite evidence from an earlier invocation.
    artifacts.mkdir(parents=True, exist_ok=False)
    display = ':' + str(args.display_number)
    evidence = {'system': list(os.uname()), 'display': display,
                'transport': 'local Unix socket with private Xauthority',
                'fixtures': {}, 'phase': 'preflight', 'status': 'running'}
    server = None
    tray_fixture = None
    server_log = Path('/var/log') / ('Xorg.%d.log' % args.display_number)
    previous_log = log_identity(server_log)
    with HarnessRuntime(prefix='bsd-default-x-') as runtime:
        try:
            preflight(os.uname().sysname, args.disposable_vm, args.display_number)
            evidence['phase'] = 'server-start'
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

            evidence['phase'] = 'server-readiness'
            evidence['randr'] = wait_for_predicate('default Xorg readiness',
                lambda: query_ready(display, env), time.monotonic() + 15,
                process=server)
            evidence['phase'] = 'authentication'
            unauthenticated = subprocess.run(
                ['xrandr', '--display', display, '--query'], capture_output=True,
                env=dict(env, XAUTHORITY=str(runtime.root / 'absent-authority')),
                timeout=5)
            if unauthenticated.returncode == 0:
                raise RuntimeError('Default Xorg accepted a client without its cookie.')
            evidence['unauthenticated_client_refused'] = True
            evidence['phase'] = 'fixtures'
            fixtures = fixture_commands(args, build, source)
            for name, extra in fixtures:
                script = {'command': 'command_client_live.py',
                          'prompt': 'prompt_protocol_live.py',
                          'xlock': 'xlock_menu_live.py', 'directory': 'directory_menu_live.py',
                          'script-com': 'script_com_example_live.py',
                          'desktop-menu': 'xdg_menu_live.py',
                          'notify': 'notify_live.py',
                          'tray': 'tray_modern_live.py',
                          'complete-preview': 'contrast_resource_live.py',
                          'desktop-controller': 'desktop_live.py'}[name]
                native_options = [] if name in (
                    'desktop-controller', 'notify', 'tray',
                    'complete-preview') else [
                    '--fvwm', str(build / 'fvwm3'),
                    '--mfl', str(build / 'modules/FvwmMFL/FvwmMFL')]
                argv = [sys.executable, str(source / 'tests/live_x' / script),
                    *native_options,
                    '--display', display, '--artifacts-dir',
                    str(artifacts / name), *extra]
                child = runtime.start(name, argv, name + '.log', env)
                if name in ('tray', 'complete-preview'):
                    tray_fixture = child
                timeout = (240 if name in ('tray', 'complete-preview') else
                    300 if name in ('desktop-controller', 'notify') else 90)
                status = child.wait_until_exit(time.monotonic() + timeout)
                evidence['fixtures'][name] = {'command': argv, 'status': status}
                if status != 0:
                    raise RuntimeError(name + ' default-X fixture failed.')
            if server.poll() is not None:
                raise RuntimeError('Default Xorg exited during the fixtures.')
            evidence['phase'] = 'complete'
            evidence['status'] = 'passed'
        except BaseException as error:
            evidence['status'] = 'failed'
            evidence['error'] = str(error)
            raise
        finally:
            try:
                if server is not None:
                    evidence['server_status_before_cleanup'] = server.poll()
                cleanup_error = None
                for child in reversed(runtime.processes):
                    try:
                        cleanup_seconds = 40 if child is tray_fixture else 2
                        child.close(time.monotonic() + cleanup_seconds)
                    except BaseException as error:
                        if cleanup_error is None:
                            cleanup_error = error
                if server is not None:
                    evidence['server_status_after_cleanup'] = server.poll()
                if cleanup_error is not None:
                    raise cleanup_error
            except BaseException as error:
                evidence['status'] = 'failed'
                evidence['cleanup_error'] = str(error)
                raise
            finally:
                save_evidence(runtime, evidence, artifacts, server_log,
                              previous_log, server is not None)
    return evidence


def save_evidence(runtime, evidence, artifacts, server_log, previous_log, started):
    for log in runtime.root.glob('*.log'):
        shutil.copy2(log, artifacts / log.name)
    current_log = log_identity(server_log)
    evidence['vendor_log_copied'] = bool(
        started and current_log is not None and current_log != previous_log)
    if evidence['vendor_log_copied']:
        shutil.copy2(server_log, artifacts / 'Xorg-server.log')
    evidence['dependencies'] = dependency_identity(
        os.uname().sysname, artifacts / 'Xorg-server.log')
    (artifacts / 'results.json').write_text(
        json.dumps(evidence, indent=2) + '\n')


def run(args):
    if not 1 <= args.repeat <= 32:
        raise ValueError('Repeat count must be between 1 and 32.')
    if args.repeat != 1 and not args.server_only:
        raise ValueError('Repeat is restricted to --server-only.')
    if not args.server_only and args.build_directory is None:
        raise ValueError('--build-directory is required for client fixtures.')
    tray_only = getattr(args, 'tray_only', False)
    preview_only = (getattr(args, 'complete_preview_only', False) or
                    getattr(args, 'complete_preview_tail_only', False))
    if not (args.notify_only or tray_only or preview_only) and any((args.fvwm, args.notify,
                            args.mfl, args.notify_client, args.capture)):
        raise ValueError('Notification path overrides require --notify-only.')
    if preview_only and any((args.notify, args.notify_client, args.capture)):
        raise ValueError('Notification inputs require --notify-only.')
    preview_inputs = ('backer', 'image_worker', 'preview_probe',
                      'preview_config_header')
    if not preview_only and any(getattr(args, name, None)
                                for name in preview_inputs):
        raise ValueError('Preview path overrides require --complete-preview-only '
                         'or --complete-preview-tail-only.')
    if tray_only and any((args.notify, args.notify_client)):
        raise ValueError('Notification path overrides require --notify-only.')
    tray_inputs = ('tray', 'buttons', 'form', 'desktop', 'session', 'worker',
        'tray_client', 'sni_client', 'expect_modern_unavailable',
        'expect_no_container')
    if not tray_only and any(getattr(args, name, None) for name in tray_inputs):
        raise ValueError('Tray path/availability overrides require '
                         '--tray-only.')
    if args.repeat == 1:
        run_once(args)
    else:
        # Repetition measures startup reliability, never retries a failed gate
        # into a pass. Each attempt owns its logs, authentication and teardown.
        root = args.artifacts_dir.resolve()
        root.mkdir(parents=True, exist_ok=False)
        summary = {'status': 'running', 'requested': args.repeat, 'attempts': []}
        try:
            for index in range(args.repeat):
                attempt = argparse.Namespace(**vars(args))
                attempt.artifacts_dir = root / ('attempt-%02d' % (index + 1))
                try:
                    run_once(attempt)
                    summary['attempts'].append({'status': 'passed'})
                except (HarnessCancelled, KeyboardInterrupt):
                    summary['status'] = 'cancelled'
                    raise
                except (RuntimeError, OSError, subprocess.SubprocessError,
                        HarnessError) as error:
                    summary['attempts'].append({'status': 'failed',
                                                 'error': str(error)})
            summary['status'] = 'passed' if all(
                item['status'] == 'passed' for item in summary['attempts']
            ) else 'failed'
        except BaseException:
            if summary['status'] == 'running':
                summary['status'] = 'interrupted'
            raise
        finally:
            (root / 'results.json').write_text(
                json.dumps(summary, indent=2) + '\n')
        if summary['status'] != 'passed':
            raise RuntimeError('At least one default-X startup failed; all attempts retained.')
    print('Default BSD Xorg checks passed:', args.artifacts_dir)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__, allow_abbrev=False)
    parser.add_argument('--build-directory', type=Path)
    modes = parser.add_mutually_exclusive_group()
    modes.add_argument('--server-only', action='store_true',
                       help='Check server readiness/authentication without any WM or application.')
    parser.add_argument('--repeat', type=int, default=1,
                        help='One to 32 independent server-only attempts; any failure fails the run.')
    parser.add_argument('--artifacts-dir', type=Path, required=True)
    modes.add_argument('--script-com-only', action='store_true',
                        help='Run only the installed Script communication example.')
    modes.add_argument('--desktop-menu-only', action='store_true',
                        help='Run only the installed XDG menu and native settings Form.')
    modes.add_argument('--desktop-controller-only', action='store_true',
                        help='Run installed desktop settings checks with '
                             'staged configuration data.')
    modes.add_argument('--directory-only', action='store_true',
                        help='Run only the installed directory menu fixture.')
    modes.add_argument('--xlock-only', action='store_true',
                        help='Run only the installed xlock menu fixture.')
    modes.add_argument('--notify-only', action='store_true',
                        help='Run the native notification fixture on this authenticated display.')
    modes.add_argument('--complete-preview-only', action='store_true',
                        help='Run owned root/resource preview recovery on this '
                             'authenticated display.')
    modes.add_argument('--complete-preview-tail-only', action='store_true',
                        help='Run only named launcher conflict, actor loss and '
                             'external-root preservation through core shutdown.')
    for name in ('backer', 'image-worker', 'preview-probe',
                 'preview-config-header'):
        parser.add_argument('--' + name, type=Path,
                            help='Explicit path for either complete-preview mode.')
    modes.add_argument('--tray-only', action='store_true',
                        help='Run modern tray checks on this authenticated '
                             'display without Linux-only container cases.')
    for name in ('fvwm', 'mfl', 'capture'):
        parser.add_argument('--' + name, type=Path,
                            help='Explicit built/installed path for '
                                 '--notify-only or --tray-only.')
    for name in ('notify', 'notify-client'):
        parser.add_argument('--' + name, type=Path,
                            help='Explicit built/installed path for --notify-only.')
    for name in ('tray', 'buttons', 'form', 'desktop', 'session', 'worker',
                 'tray-client', 'sni-client'):
        parser.add_argument('--' + name, type=Path,
                            help='Explicit built/installed path for '
                                 '--tray-only.')
    parser.add_argument('--expect-modern-unavailable', action='store_true',
                        help='Expect disabled modern support with --tray-only.')
    parser.add_argument('--expect-no-container', action='store_true',
                        help='Forward missing containment expectation to '
                             '--tray-only; container phase is not run.')
    parser.add_argument('--execution-profile', default='unspecified',
                        choices=('unspecified', 'native-cpu', 'emulated-cpu'),
                        help='Declare CPU execution separately from observed OS/architecture.')
    parser.add_argument('--display-number', type=int, default=197)
    parser.add_argument('--disposable-vm', action='store_true')
    run(parser.parse_args())
