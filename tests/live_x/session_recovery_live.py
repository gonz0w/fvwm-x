#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License, version 2 or later.
"""Observe shared session leases and bounded direct-core stop on nested X.

Use one coherent built/installed wrapper and core. This Linux fixture requires
pidfd support: it never signals a core by an unprotected PID or process name.
Only owned fixture applications and fake lockers run; no real power,
authentication or secure locking is performed. Source authoring is not a pass.
Each invocation requires a fresh artifacts directory. Select affected groups
with --cases; the actual five-second stalled-core grace runs once per group.
"""
import argparse
from copy import deepcopy
import fcntl
import hashlib
import json
import os
from pathlib import Path
import platform
import re
import select
import shutil
import signal
import sys
import time

from geometry_transition_live import Client
from live_x_harness import HarnessRuntime, start_display_server, wait_for_predicate
from mfl_stream_live import Connection
from session_providers_live import NativeTools, operation, outcome, reprobe
from session_services_live import Session


CASES = ['parent-loss', 'no-ack', 'normal-stop', 'stalled-stop']
PRIVATE_ENV = ['FVWM_SESSION_READY_FD', 'FVWM_SESSION_LEASE_FD']


def save(path, value):
    path.write_text(json.dumps(value, indent=2) + '\n')


def rows(path):
    return [json.loads(line) for line in path.read_text().splitlines()] if path.exists() else []


def identity(pid):
    fields = Path('/proc/%d/stat' % pid).read_text().rsplit(') ', 1)[1].split()
    return {'pid': pid, 'state': fields[0], 'ppid': int(fields[1]),
        'birth': fields[19]}


def descriptors(pid):
    result = []
    for path in Path('/proc/%d/fd' % pid).iterdir():
        try:
            info = path.stat()
            result.append({'fd': int(path.name), 'device': info.st_dev,
                'inode': info.st_ino, 'target': os.readlink(path)})
        except FileNotFoundError:
            pass  # /proc enumeration can include its own already closed FD.
    return result


def append(path, value):
    fd = os.open(path, os.O_WRONLY | os.O_CREAT | os.O_APPEND, 0o600)
    try:
        os.write(fd, (json.dumps(value) + '\n').encode())
    finally:
        os.close(fd)


def child(report, release):
    """An independent ordinary X client, ended only by its persistent sentinel."""
    client = Client(os.environ['DISPLAY'])
    client.create()
    append(report, {'event': 'started', **identity(os.getpid()),
        'window': client.window, 'descriptors': descriptors(os.getpid()),
        'environment': {key: os.environ.get(key) for key in PRIVATE_ENV}})
    try:
        deadline = time.monotonic() + 120
        while not release.exists():
            assert time.monotonic() < deadline, 'fixture child emergency deadline'
            client.x.XSync(client.display, 0)
            time.sleep(.02)
    finally:
        client.close()
        append(report, {'event': 'released', **identity(os.getpid())})


def module(report, event='module-executed'):
    append(report, {'event': event, **identity(os.getpid()),
        'descriptors': descriptors(os.getpid()),
        'environment': {key: os.environ.get(key) for key in PRIVATE_ENV}})


class OwnedCore:
    """A pidfd tied to the wrapper's diagnosed direct child and sampled birth."""
    def __init__(self, wrapper, executable):
        self.fd = None
        def diagnosed():
            matches = re.findall(r'core started pid=(\d+)', wrapper.log_path.read_text())
            return int(matches[0]) if matches else None
        pid = wait_for_predicate('direct owned core diagnostic', diagnosed,
            time.monotonic() + 8, process=wrapper)
        before = identity(pid)
        fd = os.pidfd_open(pid)
        try:
            after = identity(pid)
            assert all(before[key] == after[key] for key in ['pid', 'birth', 'ppid']), (before, after)
            assert before['ppid'] == wrapper.process.pid, before
            actual, expected = Path('/proc/%d/exe' % pid).stat(), executable.stat()
            assert (actual.st_dev, actual.st_ino) == (expected.st_dev, expected.st_ino)
            assert not select.select([fd], [], [], 0)[0], 'diagnosed core already exited'
            self.fd, self.initial = fd, before
        except BaseException:
            os.close(fd)
            raise

    def alive(self):
        return not select.select([self.fd], [], [], 0)[0]

    def sample(self):
        assert self.alive(), 'owned core exited'
        current = identity(self.initial['pid'])
        assert current['birth'] == self.initial['birth'], current
        return current

    def send(self, sig):
        self.sample()
        signal.pidfd_send_signal(self.fd, sig)

    def exited(self):
        wait_for_predicate('owned core pidfd exit', lambda: not self.alive(),
            time.monotonic() + 3)

    def close(self):
        if self.fd is None:
            return
        try:
            if self.alive():
                # A previously pending TERM can take effect immediately on
                # CONT. ESRCH on this exact pidfd means teardown already won.
                for sig in [signal.SIGCONT, signal.SIGTERM]:
                    try:
                        signal.pidfd_send_signal(self.fd, sig)
                    except ProcessLookupError:
                        break
                if not select.select([self.fd], [], [], 2)[0]:
                    signal.pidfd_send_signal(self.fd, signal.SIGKILL)
                    self.exited()
        finally:
            os.close(self.fd)
            self.fd = None


def signal_wrapper(process, sig):
    # A live Popen child cannot be recycled while this parent owns its Wait.
    assert process.poll() is None, 'owned wrapper already exited'
    os.kill(process.process.pid, sig)


def connect(endpoint):
    peer = Connection(endpoint)
    try:
        assert 'connection_profile' in peer.record()
        peer.send(b'set echo\n')
        peer.echo('recovery-connection-ready')
        return peer
    except BaseException:
        peer.close()
        raise


def mfl_endpoint(home, display):
    # Match FvwmMFL set_pathnames(), including an explicit screen suffix.
    # Local :D.0 becomes _D_0, not _D.0; the launch argument stays literal.
    name = re.sub(r'[^A-Za-z0-9_-]', '_', display)
    return home / ('fvwm_mfl_' + name + '.sock')


class Recovery:
    """Minimal real core/MFL session with one persistent autostart X client."""
    def __init__(self, runtime, options, out, display, stalled=False):
        self.runtime, self.options, self.out, self.display = runtime, options, out, display
        self.serial, self.peers, self.cores = 0, [], []
        self.marker, self.release = out / 'applications.jsonl', out / 'release-applications'
        self.home = runtime.root / 'home'
        self.home.mkdir(mode=0o700)
        self.private = self.home / 'runtime'
        self.private.mkdir(mode=0o700)
        self.bin = self.home / 'bin'
        self.bin.mkdir()
        shutil.copyfile(options.session, self.bin / 'fvwm-session')
        (self.bin / 'fvwm-session').chmod(0o700)
        (self.bin / 'fvwm3').symlink_to(options.fvwm)
        (self.bin / 'FvwmMFL').symlink_to(options.mfl)
        probe = self.bin / 'RecoveryModule'
        probe.write_text('#!' + sys.executable + '\nimport sys\n' +
            'sys.path.insert(0, ' + repr(str(Path(__file__).resolve().parent)) + ')\n' +
            'from pathlib import Path\nfrom session_recovery_live import module\n' +
            'module(Path(sys.argv[6]))\n')
        probe.chmod(0o700)
        config_home = self.home / 'config'
        autostart = config_home / 'autostart'
        autostart.mkdir(parents=True)
        (autostart / 'one.desktop').write_text(
            '[Desktop Entry]\nType=Application\nName=Owned recovery X client\nExec=' +
            ' '.join('"' + str(value) + '"' for value in [sys.executable,
                Path(__file__).resolve(), '--_child', self.marker, self.release]) + '\n')
        self.config = self.home / 'configuration.fvwm'
        source = Path(__file__).resolve().parents[2]
        self.config.write_text('ModulePath ' + str(self.bin) + '\nRead ' +
            str(source / 'fvwm/ConfigFvwmDefaults') + '\nDefaultFont fixed\n' +
            'EdgeScroll 0 0\nEdgeThickness 0\nStyle * TileCascadePlacement\n' +
            'AddToFunc StartFunction I Module FvwmMFL\n' +
            ('+ I Wait RecoveryNeverReady\n' if stalled else ''))
        self.env = runtime.environment({'DISPLAY': display, 'HOME': str(self.home),
            'FVWM_USERDIR': str(self.home), 'FVWMMFL_SOCKET_PATH': str(self.home),
            'XDG_RUNTIME_DIR': str(self.private), 'XDG_CONFIG_HOME': str(config_home),
            'XDG_CONFIG_DIRS': str(self.home / 'absent'),
            'XDG_CACHE_HOME': str(self.home / 'cache'),
            'DBUS_SYSTEM_BUS_ADDRESS': ''})
        for key in PRIVATE_ENV + ['SESSION_MANAGER', 'DBUS_SESSION_BUS_ADDRESS',
                'FVWM_SESSION_CONTROL_SOCKET', 'FVWM_SESSION_CONTROL_CAPABILITY']:
            self.env.pop(key, None)
        # No optional owned bus is started in the parent-crash case.
        save(self.home / 'desktop-state.json', {'session_bus': 'off'})
        self.process = self.launch('owner', display)
        self.core = self.own(self.process)
        self.endpoint = mfl_endpoint(self.home, display)
        self.control = wait_for_predicate('usable actual MFL', self.reconnect,
            time.monotonic() + 12, process=self.process)

    def launch(self, name, display):
        self.serial += 1
        return self.runtime.start(name, [str(self.bin / 'fvwm-session'), '--',
            '-d', display, '-f', str(self.config)],
            '%02d-%s.log' % (self.serial, name), self.env)

    def own(self, process):
        core = OwnedCore(process, self.options.fvwm)
        self.cores.append(core)
        return core

    def reconnect(self):
        try:
            peer = connect(self.endpoint)
        except (OSError, AssertionError):
            return None
        self.peers.append(peer)
        return peer

    def starts(self):
        return [row for row in rows(self.marker) if row['event'] == 'started']

    def await_app(self, count=1):
        wait_for_predicate('one-shot independent application launch',
            lambda: len(self.starts()) == count, time.monotonic() + 10,
            process=self.process)
        return self.starts()[-1]

    def lease(self, core=None):
        core = core or self.core
        core.sample()
        paths = list(self.private.glob('fvwm-session-*.lock'))
        assert len(paths) == 1, paths
        info = paths[0].stat()
        key = {'device': info.st_dev, 'inode': info.st_ino}
        held = [row for row in descriptors(core.initial['pid'])
            if (row['device'], row['inode']) == (info.st_dev, info.st_ino)]
        assert len(held) == 1, held
        data = Path('/proc/%d/fdinfo/%d' % (core.initial['pid'], held[0]['fd'])).read_text()
        flags = int(re.search(r'flags:\s*([0-7]+)', data).group(1), 8)
        assert flags & os.O_CLOEXEC, data
        with paths[0].open('rb') as independent:
            try:
                fcntl.flock(independent, fcntl.LOCK_EX | fcntl.LOCK_NB)
            except BlockingIOError:
                pass
            else:
                raise AssertionError('independent open acquired a supposedly held lease')
        return {'path': str(paths[0]), **key, 'core_descriptor': held[0],
            'close_on_exec': True, 'independent_flock_refused': True}

    def competitor(self, display):
        contender = self.launch('competitor', display)
        code = contender.wait_until_exit(time.monotonic() + 10)
        assert code != 0, code
        text = contender.log_path.read_text()
        assert 'session owner lock' in text, text
        assert 'core startup acknowledged' not in text and 'autostart scan started' not in text
        self.control.echo('original-core-survives-competitor')
        assert len(self.starts()) == 1
        return {'display': display, 'returncode': code, 'argv': contender.argv,
            'log': contender.log_path.name, 'autostart_count': 1}

    def cleanup(self):
        self.release.write_text('release only owned fixture applications\n')
        for peer in self.peers:
            peer.close()
        for core in reversed(self.cores):
            core.close()
        observations = []
        def all_exited():
            observations.clear()
            events = rows(self.marker)
            for start in self.starts():
                released = any(row['event'] == 'released' and row['pid'] == start['pid']
                    and row['birth'] == start['birth'] for row in events)
                try:
                    now = identity(start['pid'])
                    exited = now['birth'] != start['birth'] or now['state'] == 'Z'
                except FileNotFoundError:
                    exited = True
                observations.append({'pid': start['pid'], 'birth': start['birth'],
                    'released': released, 'exit_observed': exited, 'reaping_attested': False})
            return all(row['released'] and row['exit_observed'] for row in observations)
        try:
            wait_for_predicate('owned application exit before root cleanup', all_exited,
                time.monotonic() + 4)
        finally:
            save(self.out / 'application-cleanup.json', observations)


def app_alive(row):
    current = identity(row['pid'])
    assert current['birth'] == row['birth'] and current['state'] not in ['Z', 'X'], current
    return current


def no_leaks(record, lease):
    assert all(value is None for value in record['environment'].values()), record
    assert not any((fd['device'], fd['inode']) == (lease['device'], lease['inode'])
        for fd in record['descriptors']), record


def parent_loss(s, result):
    app = s.await_app()
    assert s.process.log_path.read_text().count('autostart scan started') == 1
    lease = s.lease()
    no_leaks(app, lease)
    report = s.out / 'module.jsonl'
    s.control.send(('Module RecoveryModule ' + str(report) + '\n').encode())
    wait_for_predicate('executed native module descriptor/environment report',
        lambda: rows(report), time.monotonic() + 5, process=s.process)
    no_leaks(rows(report)[0], lease)
    result['nonleak'] = {'application': app, 'module': rows(report)[0], 'lease': lease}
    alias = s.display + '.0'
    result['equivalent-display'] = s.competitor(alias)
    signal_wrapper(s.process, signal.SIGKILL)
    assert s.process.wait_until_exit(time.monotonic() + 3) == -signal.SIGKILL
    s.control.echo('usable-after-owned-wrapper-parent-loss')
    app_alive(app)
    assert s.lease()['inode'] == lease['inode']
    result['parent-loss'] = {'core': s.core.sample(), 'wrapper_status': -signal.SIGKILL,
        'orphan_reaping_claim': False, 'contender': s.competitor(alias)}
    before = s.core.sample()
    s.control.send(b'Restart\n')
    s.control.expect_closed()
    s.control.close()
    s.control = wait_for_predicate('fresh MFL after native same-PID Restart',
        s.reconnect, time.monotonic() + 12)
    after = s.core.sample()
    assert before['pid'] == after['pid'] and before['birth'] == after['birth']
    assert s.lease()['inode'] == lease['inode'] and len(s.starts()) == 1
    result['native-restart'] = {'before': before, 'after': after,
        'lease': s.lease(), 'contender': s.competitor(s.display)}
    report = s.out / 'exec-after-restart.jsonl'
    s.control.send(('Exec exec ' + ' '.join('"' + str(value) + '"' for value in
        [sys.executable, Path(__file__).resolve(), '--_exec', report]) + '\n').encode())
    wait_for_predicate('executed ordinary core application after Restart',
        lambda: rows(report), time.monotonic() + 5)
    no_leaks(rows(report)[0], lease)
    result['native-restart']['ordinary_exec_nonleak'] = rows(report)[0]
    s.control.send(b'Quit\n')
    s.core.exited()
    app_alive(app)
    # New owner reuses the persistent inode after the last core closes it.
    s.process = s.launch('fresh-owner', alias)
    s.core = s.own(s.process)
    s.endpoint = mfl_endpoint(s.home, alias)
    result['fresh-owner'] = {'display': alias, 'endpoint': str(s.endpoint)}
    s.control = wait_for_predicate('fresh owner MFL', s.reconnect,
        time.monotonic() + 12, process=s.process)
    s.await_app(2)
    assert s.process.log_path.read_text().count('autostart scan started') == 1
    assert s.lease()['inode'] == lease['inode']
    s.control.send(b'Quit\n')
    s.core.exited()
    assert s.process.wait_until_exit(time.monotonic() + 8) == 0
    result['final-quit-release'] = {'new_one_shot_applications': 2,
        'persistent_inode': lease['inode'], 'old_application_alive': app_alive(app)}


def no_ack(s, result):
    text = lambda: s.process.log_path.read_text()
    wait_for_predicate('real startup acknowledgment deadline',
        lambda: 'autostart unavailable: core startup acknowledgment:' in text(),
        time.monotonic() + 18,
        process=s.process)
    assert 'core startup acknowledged' not in text()
    assert 'autostart scan started' not in text() and not s.starts()
    result['lease'] = s.lease()
    s.control.echo('usable-without-startup-acknowledgment')
    s.control.send(b'Quit\n')
    assert s.process.wait_until_exit(time.monotonic() + 8) == 0
    s.core.exited()
    result.update(autostart_scans=0, applications=0, actual_core_usable=True)


def normal_stop(s, result):
    app = s.await_app()
    lease = s.lease()
    started = time.monotonic()
    signal_wrapper(s.process, signal.SIGTERM)
    code = s.process.wait_until_exit(time.monotonic() + 8)
    s.core.exited()
    assert code in [0, 128 + signal.SIGTERM], code
    result.update(wrapper_status=code, seconds=time.monotonic() - started,
        core_exit_observed=True, application_alive=app_alive(app), lease=lease)


def stalled_stop(runtime, options, out, display, result, cleanup):
    # Reuse the real default controller only for public locker admission.
    tools = NativeTools(runtime.root / 'native-tools', out)
    def release_locker():
        try:
            tools.release_and_observe_exit()
        finally:
            save(out / 'locker-cleanup.json', {'release_file': str(tools.release),
                'children': tools.locker_exits})
    cleanup.append(('observe fake locker exit', release_locker))
    options.display = display
    session = Session(runtime, options, out, 'stalled-stop',
        extra_env=tools.environment())
    cleanup.append(('close session observers', session.close))
    core = OwnedCore(session.process, options.fvwm)
    cleanup.append(('close exact owned core', core.close))
    reprobe(session)
    admitted = operation(session, 'lock')
    assert admitted['admission'] == 'accepted' and outcome(admitted) == 'started', admitted
    wait_for_predicate('admitted fake locker foreground child',
        lambda: tools.starts('xlock'), time.monotonic() + 5, process=session.process)
    locker = tools.starts('xlock')[0]
    probe = session.create('independent X client survives bounded stop', deletable=False)
    before = core.sample()
    core.send(signal.SIGSTOP)
    wait_for_predicate('exact owned core stopped',
        lambda: core.sample()['state'] in ['T', 't'], time.monotonic() + 2)
    started = time.monotonic()
    signal_wrapper(session.process, signal.SIGTERM)
    # A delayed repeat would extend a wrongly reset grace by 1.5 seconds.
    wait_for_predicate('repeat signal interval',
        lambda: time.monotonic() - started >= 1.5, time.monotonic() + 2,
        process=session.process)
    signal_wrapper(session.process, signal.SIGTERM)
    signal_wrapper(session.process, signal.SIGINT)
    code = session.process.wait_until_exit(started + 7)
    elapsed = time.monotonic() - started
    core.exited()
    assert code == 128 + signal.SIGKILL, code
    assert 4.8 <= elapsed < 6.35, elapsed
    text = session.process.log_path.read_text()
    assert text.count('WM stop requested; cooperative grace started') == 1, text
    assert text.count('WM stop grace expired; termination requested') == 1, text
    assert 'WM termination unconfirmed' not in text, text
    session.pump()
    assert probe['live'] and probe['messages'] == 0 and session.attrs(probe['window'])
    app_alive(locker)
    result.update(wrapper_status=code, seconds=elapsed, core_identity=before,
        core_exit_observed=True, locker_admission=admitted,
        locker_alive=identity(locker['pid']), independent_window=deepcopy(probe),
        secure_readiness_claim=False, borrowed_bus='not exercised; separate unit coverage')


def group(options, out, name):
    destination = out / name
    destination.mkdir()
    runtime = HarnessRuntime(prefix='fvwm-recovery-')
    result, cleanup = {'status': 'running'}, []
    def attempt(label, action):
        try:
            action()
        except BaseException as error:
            result['status'] = 'failed'
            result.setdefault('cleanup_failures', []).append({'action': label,
                'type': type(error).__name__, 'message': str(error)})
    try:
        _, display = start_display_server(runtime, 'Xvfb',
            ['Xvfb', '-screen', '0', '1440x1000x24', '-noreset'],
            'xserver.log', time.monotonic() + 10)
        result['display'] = display
        if name == 'stalled-stop':
            stalled_stop(runtime, options, destination, display, result, cleanup)
        else:
            # Register before construction so a failed readiness probe still
            # closes pidfds and releases already launched fixture applications.
            session = Recovery.__new__(Recovery)
            cleanup.append(('release children and close exact cores', session.cleanup))
            Recovery.__init__(session, runtime, options, destination, display,
                stalled=name == 'no-ack')
            {'parent-loss': parent_loss, 'no-ack': no_ack,
                'normal-stop': normal_stop}[name](session, result)
        result['status'] = 'passed'
    except BaseException as error:
        result.update(status='failed', failure={'type': type(error).__name__, 'message': str(error)})
    finally:
        # Session construction can fail after launching its wrapper but before
        # returning an object. Capture its exact still-owned child before the
        # generic harness cleanup (whose wrapper group excludes the core).
        if name == 'stalled-stop' and not any(label == 'close exact owned core'
                for label, _ in cleanup):
            for process in runtime.processes:
                if process.name == name and process.poll() is None:
                    def close_partial(process=process):
                        core = OwnedCore(process, options.fvwm)
                        core.close()
                    attempt('close partially constructed direct core', close_partial)
        for label, action in reversed(cleanup):
            attempt(label, action)
        for path in runtime.root.rglob('*'):
            if path.is_file() and not path.is_symlink() and path.suffix in [
                    '.log', '.json', '.jsonl', '.fvwm', '.desktop']:
                target = destination / 'runtime' / path.relative_to(runtime.root)
                target.parent.mkdir(parents=True, exist_ok=True)
                attempt('preserve ' + str(path), lambda p=path, t=target: shutil.copyfile(p, t))
        attempt('close owned runtime', runtime.close)
        save(destination / 'result.json', result)
        save(destination / 'hashes.json', {str(path.relative_to(destination)):
            hashlib.sha256(path.read_bytes()).hexdigest()
            for path in destination.rglob('*') if path.is_file()})
    return result


def run(options):
    out = options.artifacts_dir.resolve()
    out.mkdir(parents=True, exist_ok=False)
    evidence = {'status': 'running', 'groups': {}, 'argv': sys.argv[1:],
        'executor': {'system': platform.system(), 'release': platform.release(),
            'machine': platform.machine(), 'execution_profile': options.execution_profile},
        'limits': ['Focused Linux nested-X fixture; no full platform/release acceptance.',
            'No physical secure locking, authentication, real power or orphan reaping claim.',
            'No UI changes; no theme or screenshot matrix is repeated.',
            'Parent-crash bus guardians and replacement-exec nonleak need separate coverage.']}
    try:
        assert sys.platform == 'linux' and hasattr(os, 'pidfd_open') and hasattr(signal, 'pidfd_send_signal'), \
            'Linux pidfd_open and pidfd_send_signal are required; no PID-only fallback'
        paths = [options.session, options.fvwm, options.mfl, Path(__file__).resolve()]
        if 'stalled-stop' in options.cases:
            paths += [options.desktop, options.form]
        paths += [Path(__file__).with_name(name) for name in [
            'live_x_harness.py', 'session_services_live.py', 'session_providers_live.py',
            'mfl_stream_live.py', 'geometry_transition_live.py']]
        evidence['inputs'] = {str(path): hashlib.sha256(path.read_bytes()).hexdigest() for path in paths}
        for name in options.cases:
            evidence['groups'][name] = group(options, out, name)
            save(out / 'evidence.json', evidence)
        evidence['status'] = 'passed' if all(value['status'] == 'passed'
            for value in evidence['groups'].values()) else 'failed'
    except BaseException as error:
        evidence.update(status='failed', failure={'type': type(error).__name__, 'message': str(error)})
        raise
    finally:
        save(out / 'evidence.json', evidence)
    assert evidence['status'] == 'passed', evidence


def arguments():
    parser = argparse.ArgumentParser(description=__doc__, allow_abbrev=False)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--artifacts-dir', type=Path, required=True)
    parser.add_argument('--execution-profile', required=True,
        help='Actual executor/CPU mode; uname alone does not attest native execution')
    parser.add_argument('--cases', nargs='+', choices=CASES, default=CASES)
    for name in ['session', 'fvwm', 'mfl', 'desktop', 'form', 'data-root', 'modules-dir']:
        parser.add_argument('--' + name, type=Path)
    options = parser.parse_args()
    options.build = options.build.resolve()
    source = Path(__file__).resolve().parents[2]
    defaults = {'session': options.build / 'bin/FvwmPrompt/fvwm-session',
        'fvwm': options.build / 'fvwm3', 'mfl': options.build / 'modules/FvwmMFL/FvwmMFL',
        'desktop': options.build / 'bin/FvwmPrompt/FvwmDesktop',
        'form': options.build / 'modules/FvwmForm/FvwmForm', 'data_root': source / 'default-config'}
    for name, default in defaults.items():
        setattr(options, name, (getattr(options, name) or default).resolve())
    if options.modules_dir:
        options.modules_dir = options.modules_dir.resolve()
    options.cases = list(dict.fromkeys(options.cases))
    return options


if __name__ == '__main__':
    if len(sys.argv) == 4 and sys.argv[1] == '--_child':
        child(Path(sys.argv[2]), Path(sys.argv[3]))
    elif len(sys.argv) == 3 and sys.argv[1] == '--_exec':
        module(Path(sys.argv[2]), 'ordinary-exec')
    else:
        run(arguments())
