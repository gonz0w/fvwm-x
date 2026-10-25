#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
"""Installed default-session actions with an optional Python-free chroot."""

import argparse
import ctypes as C
import json
import os
from pathlib import Path
import shutil
import signal
import sys
import subprocess
import time

from desktop_live import connect_client
from geometry_transition_live import Pointer
from live_x_harness import HarnessRuntime, start_display_server, wait_for_predicate
from mfl_stream_live import Connection
from mfl_request_live import frame


def process_tree(pid):
    parents = {}
    for p in Path('/proc').glob('[0-9]*/stat'):
        try:
            tail = p.read_text().rsplit(')', 1)[1].split()
            parents[int(p.parent.name)] = int(tail[1])
        except (OSError, ValueError, IndexError):
            pass
    selected = {pid}
    while True:
        more = {p for p, parent in parents.items() if parent in selected}
        if more <= selected:
            return selected
        selected |= more


def sample(pid):
    records = {}
    for item in process_tree(pid):
        try:
            root = Path('/proc') / str(item)
            tail = (root / 'stat').read_text().rsplit(')', 1)[1].split()
            status = dict(line.split(':', 1) for line in (root / 'status').read_text().splitlines())
            records[item] = {'name': status['Name'].strip(),
                'rss_kib': int(status.get('VmRSS', '0 kB').split()[0]),
                'hwm_kib': int(status.get('VmHWM', '0 kB').split()[0]),
                'cpu_ticks': int(tail[11]) + int(tail[12]),
                'switches': int(status.get('voluntary_ctxt_switches', 0)) + int(status.get('nonvoluntary_ctxt_switches', 0))}
        except (OSError, ValueError, KeyError, IndexError):
            pass
    return records


def observe(pid, client, connection, evidence):
    # Let initial discovery settle; neither profile starts external applications.
    time.sleep(.7)
    first = sample(pid)
    peak = sum(r['rss_kib'] for r in first.values())
    began = time.monotonic()
    while time.monotonic() - began < 1.5:
        time.sleep(.05)
        peak = max(peak, sum(r['rss_kib'] for r in sample(pid).values()))
    elapsed = time.monotonic() - began
    last = sample(pid)
    shared = first.keys() & last.keys()
    evidence['observation'] = {'idle_seconds': elapsed, 'processes': last,
        'steady_rss_sum_kib': sum(r['rss_kib'] for r in last.values()),
        'sampled_idle_peak_rss_sum_kib': peak,
        'retained_process_hwm_sum_kib': sum(r['hwm_kib'] for r in last.values()),
        'retained_idle_cpu_ms': sum(last[p]['cpu_ticks'] - first[p]['cpu_ticks'] for p in shared) * 1000 / os.sysconf('SC_CLK_TCK'),
        'retained_idle_context_switches': sum(last[p]['switches'] - first[p]['switches'] for p in shared),
        'wakeups': None, 'wakeups_limit': 'Kernel scheduler tracing is not available; context switches are a separate proxy.'}
    pointer = Pointer(client)
    client.x.XStringToKeysym.argtypes = [C.c_char_p]
    client.x.XStringToKeysym.restype = C.c_ulong
    client.x.XKeysymToKeycode.argtypes = [C.c_void_p, C.c_ulong]
    client.x.XKeysymToKeycode.restype = C.c_ubyte
    pointer.x.XTestFakeKeyEvent.argtypes = [C.c_void_p, C.c_uint, C.c_int, C.c_ulong]
    code = client.x.XKeysymToKeycode(client.display, client.x.XStringToKeysym(b'F12'))
    assert code
    connection.echo('input-ready')
    samples = []
    for _ in range(12):
        start = time.monotonic()
        for pressed in (1, 0):
            pointer.x.XTestFakeKeyEvent(client.display, code, pressed, 0)
        client.x.XSync(client.display, 0)
        connection.expect_echo('session-key-response')
        samples.append((time.monotonic() - start) * 1000)
    evidence['observation']['xtest_key_to_core_echo_ms'] = samples


def run(options):
    artifact = options.artifacts_dir.resolve()
    artifact.mkdir(parents=True, exist_ok=True)
    root = options.root.resolve() if options.root else Path('/')
    prefix = options.prefix
    modules = prefix / 'libexec/fvwm3' / options.version
    core = prefix / 'bin/fvwm3'
    data = prefix / 'share/fvwm3'
    capture = modules / 'fvwm-desktop-capture'
    if not sys.platform.startswith('linux'):
        raise RuntimeError('This installed-namespace/process observation needs Linux /proc and subreaping')
    libc = C.CDLL(None, use_errno=True)
    libc.prctl.argtypes = [C.c_int, C.c_ulong, C.c_ulong, C.c_ulong, C.c_ulong]
    if libc.prctl(36, 1, 0, 0, 0) != 0:  # PR_SET_CHILD_SUBREAPER
        raise RuntimeError('Cannot own detached fixture children for cleanup')
    evidence = {'root': str(root), 'prefix': str(prefix), 'python_free_namespace': bool(options.root)}
    with HarnessRuntime(prefix='fvwm-session-runtime-') as runtime:
        _, display = start_display_server(runtime, 'Xvfb',
            ['Xvfb', '-screen', '0', '1440x900x24', '-noreset'],
            'xserver.log', time.monotonic() + 10)
        # The root is disposable and owned by this test. The observer remains
        # outside it; Xlib uses the same Linux abstract Unix-socket namespace.
        inside = Path('/tmp/fvwm-session-runtime') if options.root else runtime.root
        outside = root / str(inside).lstrip('/') if options.root else inside
        outside.mkdir(parents=True, exist_ok=True)
        outside.chmod(0o700)
        def path(value):
            return root / str(value).lstrip('/') if options.root else value
        def execute(argv):
            return [shutil.which('chroot'), str(root), *map(str, argv)] if options.root else list(map(str, argv))
        if options.root:
            assert os.geteuid() == 0
            assert not any(p.name.startswith(('python', 'pypy')) for p in root.rglob('*'))
            assert all(path(modules / n).read_bytes().startswith(b'\x7fELF')
                for n in ('FvwmDesktop', 'fvwm-desktop-worker', 'fvwm-desktop-capture'))
        user = inside / 'user'
        path(user).mkdir(exist_ok=True)
        tools = inside / 'tools'
        path(tools).mkdir(exist_ok=True)
        # Backend peers never invoke real power/authentication commands.
        for name, content in {
            'systemctl': 'echo "$*" >> "$FVWM_SESSION_POWER_LOG"\n/bin/sleep .3\nexit 1\n',
            'shutdown': 'echo "$*" >> "$FVWM_SESSION_POWER_LOG"\n/bin/sleep .3\nexit 1\n',
            'amixer': '/bin/sleep .4\nexit 0\n',
            'xlock': 'echo requested >> "$FVWM_SESSION_LOCK_LOG"\necho $$ >> "$FVWM_SESSION_LOCK_PIDS"\nif test -f "$FVWM_SESSION_LOCK_HOLD"; then\n while test -f "$FVWM_SESSION_LOCK_HOLD"; do /bin/sleep .05; done\n exit 0\nfi\n/bin/sleep .3\nexit 7\n',
        }.items():
            target = path(tools / name)
            target.write_text('#!/bin/sh\n' + content)
            target.chmod(0o700)
        path(inside / 'data/applications').mkdir(parents=True, exist_ok=True)
        path(inside / 'data/applications/fixture.desktop').write_text(
            '[Desktop Entry]\nType=Application\nName=Native test app\nExec=/bin/true\n')
        path(inside / 'configxdg/menus').mkdir(parents=True, exist_ok=True)
        path(inside / 'configxdg/menus/applications.menu').write_text(
            '<Menu><Name>Applications</Name><AppDir>' + str(inside / 'data/applications') +
            '</AppDir><Include><All/></Include></Menu>')
        (path(user) / 'local.config').write_text('*FvwmDesktop: motion false\n')
        config = inside / 'config'
        path(config).write_text('ModulePath ' + str(modules) + '\n' +
            path(data / 'default-config/config').read_text().replace('$[FVWM_DATADIR]/default-config', str(data / 'default-config')) +
            ('\nAddToFunc StartFunction I Module FvwmMFL\n' if not options.root else '\n') +
            'Key F12 A N Echo session-key-response\n')
        env = runtime.environment({'DISPLAY': display, 'HOME': str(inside), 'FVWM_USERDIR': str(user),
            'FVWMMFL_SOCKET_PATH': str(inside), 'FVWM_DESKTOP_ROOT': str(data / 'default-config'),
            'FVWM_SESSION_POWER_LOG': str(inside / 'power.log'), 'FVWM_SESSION_LOCK_LOG': str(inside / 'lock.log'),
            'FVWM_SESSION_LOCK_PIDS': str(inside / 'lock-pids.log'), 'FVWM_SESSION_LOCK_HOLD': str(inside / 'hold-locker'),
            'XDG_CONFIG_HOME': str(inside / 'configxdg'), 'XDG_CONFIG_DIRS': str(inside / 'configxdg'),
            'XDG_DATA_HOME': str(inside / 'data'), 'XDG_DATA_DIRS': str(inside / 'data'),
            'PATH': str(tools) + ':' + str(prefix / 'bin') + ':/usr/bin:/bin', 'SESSION_MANAGER': ''})
        for name in ('FVWM_DESKTOP_WORKER', 'FVWM_DESKTOP_CAPTURE'):
            env.pop(name, None)
        began = time.monotonic()
        process = runtime.start('installed-core', execute([core, '-d', display, '-f', config,
            '-v', '-o', inside / 'core.log']), 'core.log', env)
        endpoint = outside / ('fvwm_mfl_' + display.replace(':', '_') + '.sock')
        connection = client = None
        retained_locker = None
        hold = outside / 'hold-locker'
        try:
            wait_for_predicate('installed MFL', endpoint.exists, time.monotonic() + 15, process=process)
            connection = Connection(endpoint)
            connection.record()
            connection.send(b'set echo\n')
            def ready():
                connection.send(frame('ready', resource='settings', scope='motion'))
                value = connection.record()['reply']
                assert value['id'] == 'ready', value
                if value['status'] == 'ok':
                    return value['result']
                assert value['error']['code'] == 'provider_not_ready', value
                return None
            initial = wait_for_predicate('installed desktop ready', ready,
                time.monotonic() + 15, process=process)
            assert initial['settings']['motion']['effective'] is False, initial
            client = connect_client(display)
            client.x.XFetchName.argtypes = [C.c_void_p, C.c_ulong, C.POINTER(C.c_void_p)]
            client.x.XFetchName.restype = C.c_int
            def windows():
                found = {}
                def visit(window):
                    name = C.c_void_p()
                    if client.x.XFetchName(client.display, window, C.byref(name)) and name.value:
                        found[C.string_at(name.value).decode(errors='replace')] = window
                        client.x.XFree(name)
                    root_return, parent = C.c_ulong(), C.c_ulong()
                    items, count = C.POINTER(C.c_ulong)(), C.c_uint()
                    if client.x.XQueryTree(client.display, window, C.byref(root_return),
                            C.byref(parent), C.byref(items), C.byref(count)):
                        children = [items[i] for i in range(min(count.value, 256))]
                        if items:
                            client.x.XFree(items)
                        for child in children:
                            visit(child)
                visit(client.root)
                return found
            wait_for_predicate('installed bar', lambda: windows().get('SlateBar'),
                time.monotonic() + 10, process=process)
            evidence['startup_ready_ms'] = (time.monotonic() - began) * 1000
            if options.observe_only:
                observe(process.process.pid, client, connection, evidence)
            else:
                connection.send(b'SendToModule FvwmDesktop volume up\nSendToModule FvwmDesktop session poweroff\n'
                                b'SendToModule FvwmDesktop session reboot\n')
                wait_for_predicate('safe power peer', lambda: (outside / 'power.log').exists(),
                    time.monotonic() + 5, process=process)
                assert (outside / 'power.log').read_text().splitlines() == ['poweroff']
                connection.send(b'SendToModule FvwmDesktop lock\nSendToModule FvwmDesktop lock\n')
                wait_for_predicate('safe locker peer', lambda: (outside / 'lock.log').exists(),
                    time.monotonic() + 5, process=process)
                time.sleep(.5)
                assert (outside / 'lock.log').read_text() == 'requested\n'
                evidence['safe_power_and_locker_peers'] = 'passed'
                connection.send(b'SendToModule FvwmDesktop screenshot\n')
                pictures = outside / 'Pictures/Screenshots'
                wait_for_predicate('installed screenshot', lambda: pictures.is_dir() and any(
                    f.read_bytes().endswith(b'IEND\xaeB`\x82') for f in pictures.glob('*.png')),
                    time.monotonic() + 10, process=process)
                screenshot = next(pictures.glob('*.png'))
                shutil.copy2(screenshot, artifact / 'installed-session.png')
                # Both offline checking and current command/menu utilities run in
                # the same interpreter-free namespace as the installed desktop.
                run_env = dict(env, DISPLAY=display)
                for label, argv, expected in [
                    ('configuration-check', [modules / 'FvwmDesktop', '--check-config', user / 'local.config', '--json'], 0),
                    ('command', [prefix / 'bin/FvwmCommand', '-f', inside / endpoint.name, 'Echo installed-command'], 0),
                    ('directory-menu', [prefix / 'bin/fvwm-menu-directory', '--dir', inside, '--reuse'], 0),
                    ('browser-inspection', [prefix / 'bin/fvwm-file-browser', '--inspect', '--dir', inside], 0),
                    ('desktop-menu', [prefix / 'bin/fvwm-menu-desktop', '--set-menus', inside / 'configxdg/menus/applications.menu'], 0),
                ]:
                    result = subprocess.run(execute(argv), env=run_env, capture_output=True, timeout=15)
                    (artifact / (label + '.stdout')).write_bytes(result.stdout)
                    (artifact / (label + '.stderr')).write_bytes(result.stderr)
                    assert result.returncode == expected, (label, result.returncode, result.stderr)
                    if label == 'desktop-menu':
                        assert b'Native test app' in result.stdout, result.stdout
                    if label == 'command':
                        connection.expect_echo('installed-command')
                evidence['installed_utility_commands'] = 'passed'
                themes = {}
                for profile, palette, appearance in [('slate-dark', 'slate', 'dark'),
                        ('dune-light', 'dune', 'light'), ('classic-large', 'grove', 'dark')]:
                    connection.send(('SendToModule FvwmDesktop set palette ' + palette +
                        '\nSendToModule FvwmDesktop set appearance ' + appearance + '\n').encode())
                    if profile == 'classic-large':
                        connection.send(b'DefaultFont 9x15\nStyle * BorderWidth 3, HandleWidth 6\n'
                            b'*FvwmFormDefault: Font 9x15\n*FvwmFormDefault: ButtonFont 9x15\n'
                            b'*FvwmFormDefault: InputFont 9x15\n')
                    connection.send(b'SendToModule FvwmDesktop lock\nSendToModule FvwmDesktop lock\n')
                    time.sleep(.6)
                    # A title can exist before Map/Expose completes. Observe
                    # the actual notice mapping rather than capture wallpaper.
                    from colorset_live import WindowAttributes
                    client.x.XGetWindowAttributes.argtypes = [C.c_void_p, C.c_ulong,
                        C.POINTER(WindowAttributes)]
                    def notice_visible():
                        window = windows().get('FVWM-X')
                        attrs = WindowAttributes()
                        return window and client.x.XGetWindowAttributes(client.display,
                            window, C.byref(attrs)) and attrs.map_state == 2
                    wait_for_predicate(profile + ' mapped locker notice', notice_visible,
                        time.monotonic() + 10, process=process)
                    time.sleep(.1)
                    result = subprocess.run(execute([capture, inside / 'review']),
                        env=env, capture_output=True, text=True, timeout=10)
                    assert result.returncode == 0, result.stderr
                    filename = Path(result.stdout.strip().removeprefix('Screenshot saved: '))
                    shutil.copy2(path(filename), artifact / (profile + '-locker-notice.png'))
                    themes[profile] = profile + '-locker-notice.png'
                evidence['locker_failure_theme_captures'] = themes
                hold.touch()
                previous = (outside / 'lock-pids.log').read_text().splitlines()
                connection.send(b'SendToModule FvwmDesktop lock\n')
                wait_for_predicate('held locker peer', lambda: len((outside / 'lock-pids.log').read_text().splitlines()) > len(previous),
                    time.monotonic() + 5, process=process)
                retained_locker = int((outside / 'lock-pids.log').read_text().splitlines()[-1])
                def locker_identity():
                    status = Path('/proc') / str(retained_locker) / 'stat'
                    fields = status.read_text().rsplit(')', 1)[1].split()
                    assert fields[0] != 'Z', 'locker exited during retirement'
                    return fields[19]
                identity = locker_identity()
                connection.send(b'KillModule FvwmDesktop\nModuleSynchronous Timeout 15 FvwmDesktop\n')
                wait_for_predicate('restarted installed desktop', ready, time.monotonic() + 15, process=process)
                assert not (outside / 'power.log').read_text().splitlines()[1:]
                assert locker_identity() == identity
                evidence['controller_restart_without_power_replay'] = 'passed'
                evidence['same_locker_survives_controller_restart'] = retained_locker
            connection.send(b'SendToModule FvwmDesktop session logout\n')
            assert process.wait_until_exit(time.monotonic() + 10) == 0
            evidence['existing_logout_core_quit'] = 'passed'
            if retained_locker:
                assert locker_identity() == identity
                evidence['same_locker_survives_core_retirement'] = retained_locker
        finally:
            hold.unlink(missing_ok=True)
            if retained_locker:
                deadline = time.monotonic() + 3
                while True:
                    try:
                        pid, status = os.waitpid(retained_locker, os.WNOHANG)
                    except ChildProcessError:
                        break
                    if pid:
                        evidence['fixture_locker_reaped'] = os.waitstatus_to_exitcode(status)
                        break
                    if time.monotonic() >= deadline:
                        # This is our still-unreaped adopted child; never signal
                        # a released PID or use a global process-name cleanup.
                        os.kill(retained_locker, signal.SIGKILL)
                        os.waitpid(retained_locker, 0)
                        raise AssertionError('fixture locker did not honor release')
                    time.sleep(.02)
            if client:
                client.close()
            if connection:
                connection.close()
            for log in runtime.root.glob('*.log'):
                shutil.copy2(log, artifact / ('observer-' + log.name))
            core_log = outside / 'core.log'
            if core_log.exists():
                shutil.copy2(core_log, artifact / 'installed-core.log')
            evidence['core_exit'] = process.poll()
            (artifact / 'evidence.json').write_text(json.dumps(evidence, indent=2) + '\n')
    print(json.dumps(evidence))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--prefix', type=Path, required=True)
    parser.add_argument('--version', required=True)
    parser.add_argument('--root', type=Path)
    parser.add_argument('--observe-only', action='store_true')
    parser.add_argument('--artifacts-dir', type=Path, required=True)
    run(parser.parse_args())
