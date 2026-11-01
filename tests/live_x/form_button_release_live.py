#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or any later version.
"""Actual Form button ownership and release regression, on owned real X.

--fast-click-only is the deterministic initial regression: stop only the
proxy-identified Form child, inject real XTest press/release and XSync, resume
in finally, then require exactly one actual Form command and core Echo within
one second. Core, module proxy, MFL and X server are never stopped.

The default additionally checks ordinary clicks, orphan/wrong-button/outside
releases, cross-button release, keyboard, unmap/remap, ActivateOnPress,
GrabServer restoration and one observed 2x UI-scaled click. No command callback
or X button event is synthesized. Negative observations use the same one-second
receipt window. Linux/FreeBSD/OpenBSD only; broader UI/platform gates remain.
--lifecycle-tail-only skips the first seven groups explicitly and runs the four
lifecycle/activation/scale groups on a fresh Normal Form. It excludes fast-only.
"""
import argparse
import ctypes as C
import hashlib
import json
import os
from pathlib import Path
import select
import shlex
import shutil
import signal
import socket
import subprocess
import sys
import time

from browser_continuity_live import cleanup_receipts
from geometry_transition_live import Client
from inspector_live import NativeView, atomic_json
from live_x_harness import HarnessRuntime, start_display_server, wait_for_predicate
from mfl_stream_live import Connection
from native_bindings_live import rows
from native_module_proxy import module_proxy
from session_x_errors import SessionXErrors

PREFIX = 'FORM_BUTTON_RELEASE '
LIFECYCLE_SKIPPED = {
    'stopped-form-fast-click-exactly-once',
    'ordinary-fast-click-exactly-once',
    'orphan-release-after-other-client-grab',
    'other-button-release-does-not-activate-or-disarm',
    'release-outside-armed-button',
    'release-over-different-button',
    'actual-keyboard-button-activation',
}


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


class Probe:
    def __init__(self, args, runtime, display, native, endpoint, core, evidence):
        self.args, self.runtime, self.native = args, runtime, native
        self.core, self.evidence = core, evidence
        self.client = self.errors = self.other = self.connection = None
        self.pointer = None
        self.buttons_down = set()
        self.stopped, self.forms, self.messages = set(), [], []
        self.serial = 0
        self.client = Client(display)
        self.errors = SessionXErrors(self.client, evidence).install()
        self.view = NativeView(self.client, self.errors, core)
        self.pointer = self.view.pointer
        self.other = Client(display)
        x = self.client.x
        p, w, i = C.c_void_p, C.c_ulong, C.c_int
        for name, arguments, result in [
            ('XSetInputFocus', [p, w, i, w], i),
            ('XTranslateCoordinates', [p, w, w, i, i, C.POINTER(i),
                C.POINTER(i), C.POINTER(w)], i),
        ]:
            getattr(x, name).argtypes = arguments
            getattr(x, name).restype = result
        self.connection = Connection(endpoint)
        assert 'connection_profile' in self.connection.record()
        self.connection.socket.settimeout(0.05)
        self.connection.send(b'set echo\n')
        self.command('Nop')

    def read(self):
        for _ in range(64):
            if b'\n' not in self.connection.pending and not select.select(
                    [self.connection.socket], [], [], 0)[0]:
                break
            try:
                value = self.connection.record()
            except socket.timeout:
                return
            assert len(self.connection.pending) <= 1024 * 1024
            text = value.get('echo', {}).get('message')
            if text is not None:
                self.messages.append(dict(time=time.monotonic(), text=text))
                assert len(self.messages) <= 4096

    def wait(self, predicate, timeout=5):
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            self.read()
            value = predicate()
            if value:
                return value
            assert self.core.poll() is None, 'owned core exited'
            select.select([], [], [], 0.005)
        raise AssertionError('bounded Form button observation expired')

    def command(self, text):
        self.serial += 1
        token = 'form-button-barrier-%d' % self.serial
        self.connection.send((text + '\nEcho ' + token + '\n').encode())
        self.wait(lambda: any(value['text'] == token for value in self.messages))

    def module(self, alias):
        def observed():
            records = [json.loads(path.read_text())
                for path in self.native.glob('module-*.json')]
            matches = [record for record in records
                if record['real'] == str(self.args.form) and
                alias in record['arguments']]
            assert len(matches) <= 1
            return matches[0] if matches else None
        return self.wait(observed)

    def focus(self, form):
        self.command('WindowId %d Raise\nWindowId %d Focus' %
            (form['window'], form['window']))
        self.client.x.XSetInputFocus(self.client.display, form['window'], 1, 0)
        self.client.x.XSync(self.client.display, 0)
        assert self.view.focus() == form['window']

    def barrier(self, form):
        self.serial += 1
        token = 'fbscale%d' % self.serial
        prefix = 'FVWM_FORM_UI_SCALE ' + token + ' '
        self.connection.send(('SendToModule %s UIScaleStatus %s\n' %
            (form['alias'], token)).encode())
        value = self.wait(lambda: next((value['text'] for value in self.messages
            if value['text'].startswith(prefix)), None))
        form['last_native_barrier'] = value
        return value

    def create(self, name, activate=False, grab=False, scaled=False):
        alias = 'FormButtonRelease' + name
        lines = ['Title "' + alias + '"', 'Position 120 140',
            'Font fixed', 'ButtonFont fixed', 'Fore black', 'Back white',
            'ItemFore black', 'ItemBack white']
        if activate:
            lines.append('ActivateOnPress on')
        if grab:
            lines.append('GrabServer')
        if scaled:
            lines.extend(['UIScale monitor',
                'Font "xft:DejaVu Sans:size=10"',
                'ButtonFont "xft:DejaVu Sans:size=10"',
                'InputFont "xft:DejaVu Sans:size=10"'])
        lines.extend(['Line left', 'Button continue "Left (F9)" F9',
            'Command Echo ' + PREFIX + alias + ' left',
            'Button continue "Right (F10)" F10',
            'Command Echo ' + PREFIX + alias + ' right'])
        if scaled:
            lines.extend(['Line left', 'List picks 32 4', 'ListViewport 18 1',
                'ListVersioned', 'ListItem off "scale fixture" file'])
        for line in lines:
            self.connection.send(('*' + alias + ': ' + line + '\n').encode())
        self.command('Module FvwmForm ' + alias)
        window = self.view.wait(alias)
        owner = self.module(alias)
        children = self.view.children(window)
        assert len(children) == (3 if scaled else 2), children
        form = dict(alias=alias, window=window, left=children[0],
            right=children[1], module=owner, scaled=scaled,
            activate_on_press=activate, grab_server=grab)
        self.forms.append(form)
        self.evidence['forms'].append(form)
        self.focus(form)
        self.barrier(form)
        if scaled:
            def adopted():
                value = self.barrier(form).split()
                assert len(value) == 15
                return value if value[2:5] == ['1', 'ready', '8'] else None
            form['observed_scale'] = self.wait(adopted)
        return form

    def point(self, window):
        actual = self.view.attributes(window)
        assert actual and actual.map_state == 2
        x, y, child = C.c_int(), C.c_int(), C.c_ulong()
        assert self.client.x.XTranslateCoordinates(self.client.display, window,
            self.client.root, actual.width // 2, actual.height // 2,
            C.byref(x), C.byref(y), C.byref(child))
        self.evidence.setdefault('button_observations', []).append(dict(
            window=window, size=[actual.width, actual.height],
            root_point=[x.value, y.value],
            all_event_masks=int(actual.all_event_masks)))
        return x.value, y.value

    def button(self, number, down):
        assert self.pointer.x.XTestFakeButtonEvent(
            self.client.display, number, int(down), 0)
        self.client.x.XSync(self.client.display, 0)
        if down:
            self.buttons_down.add(number)
        else:
            self.buttons_down.discard(number)

    def before(self):
        self.read()
        return len(self.messages), time.monotonic()

    def expect(self, form, before, expected):
        deadline = time.monotonic() + 1
        while time.monotonic() < deadline:
            self.read()
            select.select([], [], [], 0.005)
        self.read()
        prefix = PREFIX + form['alias'] + ' '
        echoes = [value['text'][len(prefix):] for value in
            self.messages[before[0]:] if value['text'].startswith(prefix)]
        commands = [record['text'][len('Echo ' + prefix):]
            for record in rows(self.native) if record['direction'] == 'out' and
            record['time'] >= before[1] and
            record['text'].startswith('Echo ' + prefix)]
        self.evidence.setdefault('receipt_observations', []).append(dict(
            alias=form['alias'], expected=expected, actual_core_echoes=echoes,
            actual_form_commands=commands, receipt_window_seconds=1))
        assert echoes == expected and commands == expected, (echoes, commands)

    def stopped_click(self, form):
        owner = form['module']
        assert owner['real'] == str(self.args.form)
        assert form['alias'] in owner['arguments']
        assert not (self.native / ('stopped-%d.json' % owner['proxy'])).exists()
        self.focus(form)
        self.pointer.motion(*self.point(form['left']))
        before = self.before()
        child = owner['child']
        # The proxy owns this exact still-live child. ps provides a portable
        # kernel-state/parent check before injecting while it is stopped.
        self.stopped.add(child)
        try:
            os.kill(child, signal.SIGSTOP)
            def stopped():
                result = subprocess.run(['ps', '-o', 'ppid=', '-o', 'stat=',
                    '-p', str(child)], capture_output=True, text=True, timeout=0.5)
                assert result.returncode == 0 and len(result.stdout) <= 1024
                fields = result.stdout.split()
                assert len(fields) == 2 and int(fields[0]) == owner['proxy']
                return fields if 'T' in fields[1] else None
            state = self.wait(stopped, timeout=1)
            self.button(1, True)
            self.button(1, False)
            self.client.x.XSync(self.client.display, 0)
            self.evidence.setdefault('stopped_injections', []).append(dict(
                alias=form['alias'], child=child, proxy=owner['proxy'],
                kernel_state=state, real_xtest_press_release_synced=True))
        finally:
            os.kill(child, signal.SIGCONT)
            self.stopped.discard(child)
        self.expect(form, before, ['left'])

    def retire(self, form):
        self.command('KillModule FvwmForm ' + form['alias'])
        path = self.native / ('stopped-%d.json' % form['module']['proxy'])
        self.wait(path.exists)
        waited = json.loads(path.read_text())
        assert waited['child'] == form['module']['child']
        assert waited['returncode'] is not None
        form['actual_wait'] = waited

    def case(self, name, operation):
        if self.args.lifecycle_tail_only and name in LIFECYCLE_SKIPPED:
            self.evidence['cases'][name] = dict(status='skipped',
                reason='explicit lifecycle-tail-only selection; no execution')
            return
        value = dict(status='running', started=time.monotonic())
        self.evidence['cases'][name] = value
        try:
            operation()
        except BaseException as error:
            value.update(status='failed', failure=repr(error))
            raise
        else:
            value.update(status='passed', seconds=time.monotonic() - value['started'])

    def exercise(self):
        form = self.create('Normal')
        self.case('stopped-form-fast-click-exactly-once',
            lambda: self.stopped_click(form))
        if self.args.fast_click_only:
            return

        def normal():
            self.focus(form)
            self.pointer.motion(*self.point(form['left']))
            before = self.before()
            self.button(1, True)
            self.button(1, False)
            self.expect(form, before, ['left'])
        self.case('ordinary-fast-click-exactly-once', normal)

        def orphan():
            self.focus(form)
            self.pointer.motion(*self.point(form['left']))
            before = self.before()
            other = self.other
            assert other.x.XGrabPointer(other.display, other.root, 0,
                (1 << 2) | (1 << 3) | (1 << 6), 1, 1, 0, 0, 0) == 0
            try:
                self.button(1, True)
            finally:
                other.x.XUngrabPointer(other.display, 0)
                other.x.XSync(other.display, 0)
            self.button(1, False)
            self.expect(form, before, [])
        self.case('orphan-release-after-other-client-grab', orphan)

        def wrong_button():
            self.focus(form)
            self.pointer.motion(*self.point(form['left']))
            before = self.before()
            self.button(1, True)
            self.barrier(form)
            self.button(2, True)
            self.button(2, False)
            self.expect(form, before, [])
            assert self.pointer.grabbed()
            before = self.before()
            self.button(1, False)
            self.expect(form, before, ['left'])
        self.case('other-button-release-does-not-activate-or-disarm', wrong_button)

        def released_elsewhere(other_button=False):
            self.focus(form)
            self.pointer.motion(*self.point(form['left']))
            before = self.before()
            self.button(1, True)
            self.barrier(form)
            self.pointer.motion(*(self.point(form['right']) if other_button
                else (20, 700)))
            self.button(1, False)
            self.expect(form, before, [])
        self.case('release-outside-armed-button', released_elsewhere)
        self.case('release-over-different-button',
            lambda: released_elsewhere(True))

        def keyboard():
            self.focus(form)
            self.pointer.motion(*self.point(form['left']))
            before = self.before()
            self.button(1, True)
            self.barrier(form)
            self.view.key('F10')
            self.expect(form, before, ['right'])
            before = self.before()
            self.button(1, False)
            self.expect(form, before, [])
        self.case('actual-keyboard-button-activation', keyboard)

        def remap():
            self.focus(form)
            self.pointer.motion(*self.point(form['left']))
            before = self.before()
            self.button(1, True)
            self.barrier(form)
            self.command('*' + form['alias'] + ': UnMap')
            self.wait(lambda: self.view.attributes(form['window']).map_state == 0)
            self.command('*' + form['alias'] + ': Map')
            self.wait(lambda: self.view.attributes(form['window']).map_state == 2)
            self.pointer.motion(*self.point(form['left']))
            self.button(1, False)
            self.expect(form, before, [])
        self.case('unmap-remap-cancels-armed-button', remap)
        self.retire(form)

        def grab_server():
            grabbed = self.create('GrabServer', grab=True)
            self.wait(self.pointer.grabbed)
            self.pointer.motion(*self.point(grabbed['left']))
            before = self.before()
            self.button(1, True)
            self.barrier(grabbed)
            self.button(1, False)
            self.expect(grabbed, before, ['left'])
            assert self.pointer.grabbed(), 'configured grab was not restored'
            self.retire(grabbed)
            self.wait(lambda: not self.pointer.grabbed())
        self.case('grab-server-restoration-after-owned-release', grab_server)

        def activate_on_press():
            activated = self.create('ActivateOnPress', activate=True)
            self.pointer.motion(*self.point(activated['left']))
            before = self.before()
            self.button(1, True)
            self.expect(activated, before, ['left'])
            before = self.before()
            self.button(1, False)
            self.expect(activated, before, [])
            self.retire(activated)
        self.case('activate-on-press-has-no-second-release-command', activate_on_press)

        def scaled():
            enlarged = self.create('Scaled', scaled=True)
            self.stopped_click(enlarged)
            self.retire(enlarged)
        self.case('observed-two-times-scaled-button-click', scaled)

    def close(self):
        for child in list(self.stopped):
            try:
                os.kill(child, signal.SIGCONT)
            except ProcessLookupError:
                pass
        self.stopped.clear()
        if self.client is not None and self.pointer is not None:
            for number in list(self.buttons_down):
                self.button(number, False)
        if self.other is not None:
            self.other.x.XUngrabPointer(self.other.display, 0)
            self.other.close()
        if self.connection is not None:
            self.connection.close()
        if self.errors is not None:
            self.errors.close_display()
            assert not self.errors.unexpected, self.errors.unexpected
        elif self.client is not None:
            self.client.close()


def run(args):
    source = Path(__file__).resolve().parents[2]
    out = args.artifacts_dir
    out.mkdir(parents=True, exist_ok=False)
    paths = [Path(__file__).resolve(), source / 'modules/FvwmForm/FvwmForm.c',
        source / 'modules/FvwmForm/ReadXServer.c',
        source / 'modules/FvwmForm/file_transfer_form.c']
    paths.extend(Path(__file__).with_name(name) for name in (
        'native_module_proxy.py', 'native_bindings_live.py', 'live_x_harness.py',
        'geometry_transition_live.py', 'inspector_live.py', 'session_x_errors.py'))
    evidence = dict(status='running', cases={}, forms=[], cleanup_failures=[],
        fast_click_only=args.fast_click_only, execution_profile=args.execution_profile,
        lifecycle_tail_only=args.lifecycle_tail_only,
        platform=sys.platform, argv=sys.argv,
        source_sha256={str(path.relative_to(source)): digest(path) for path in paths},
        binary_sha256={name: digest(getattr(args, name))
            for name in ('fvwm', 'mfl', 'form')},
        config_header=dict(path=str(args.config_header),
            sha256=digest(args.config_header)),
        limits=dict(physical_hardware='not exercised',
            native_sanitizers='separate gate',
            six_target_qualification='separate gate',
            postcases='not run' if args.fast_click_only else 'attempted below'))
    runtime = HarnessRuntime(prefix='fvwm-form-button-')
    native = runtime.root / 'native-records'
    probe = core = None
    failure = None
    try:
        native.mkdir(mode=0o700)
        modules = runtime.root / 'modules'
        modules.mkdir(mode=0o700)
        fixture = Path(__file__).resolve()
        for name, real in (('FvwmForm', args.form), ('FvwmMFL', args.mfl)):
            wrapper = modules / name
            wrapper.write_text('#!/bin/sh\nexec ' + shlex.join([sys.executable,
                str(fixture), '--module-proxy', str(real), str(native)]) +
                ' "$@" 2>>' + shlex.quote(str(runtime.root / (name + '.log'))) + '\n')
            wrapper.chmod(0o700)
        _, display = start_display_server(runtime, 'Xvfb',
            ['Xvfb', '-screen', '0', '1200x900x24', '-noreset'],
            'xserver.log', time.monotonic() + 10)
        config = runtime.root / 'config'
        config.write_text('ModulePath ' + str(modules) +
            '\nKillModule FvwmMFL\nSchedule 1000 Module FvwmMFL\n'
            'MonitorScale "screen" 2\nMonitorScaleFallback 2\n'
            'DefaultFont fixed\nStyle * ClickToFocus, ClickToFocusPassesClickOn\n')
        evidence['config_sha256'] = digest(config)
        core = runtime.start('core', [str(args.fvwm), '-d', display,
            '-f', str(config)], 'core.log', runtime.environment({
                'DISPLAY': display, 'FVWMMFL_SOCKET_PATH': str(runtime.root)}))
        endpoint = runtime.root / ('fvwm_mfl_' + display.replace(':', '_') + '.sock')
        def mfl_ready():
            records = [json.loads(path.read_text())
                for path in native.glob('module-*.json')]
            return endpoint.exists() and any(record['real'] == str(args.mfl)
                for record in records)
        wait_for_predicate('actual proxied MFL', mfl_ready,
            time.monotonic() + 12, process=core)
        probe = Probe.__new__(Probe)
        probe.__init__(args, runtime, display, native, endpoint, core, evidence)
        probe.exercise()
        evidence['status'] = 'passed'
    except BaseException as error:
        evidence.update(status='failed', failure=repr(error))
        failure = error
    finally:
        def clean(label, operation):
            nonlocal failure
            try:
                return operation()
            except BaseException as error:
                evidence['status'] = 'failed'
                evidence['cleanup_failures'].append(dict(component=label,
                    failure=repr(error)))
                if failure is None:
                    failure = error
        if probe is not None:
            clean('input/observers', probe.close)
        if core is not None:
            clean('core Wait', lambda: core.close(time.monotonic() + 4))
        if native.exists():
            evidence['actual_child_waits'] = clean('module child Wait receipts',
                lambda: cleanup_receipts(native))
        for process in reversed(runtime.processes):
            clean(process.name, lambda process=process:
                process.close(time.monotonic() + 3))
        evidence['process_waits'] = [dict(name=process.name, pid=process.process.pid,
            returncode=process.process.returncode) for process in runtime.processes]
        if native.exists():
            clean('native evidence', lambda: shutil.copytree(native, out / native.name))
        for path in runtime.root.glob('*.log'):
            clean(path.name, lambda path=path: shutil.copyfile(path, out / path.name))
        if (runtime.root / 'config').exists():
            clean('actual core config', lambda: shutil.copyfile(
                runtime.root / 'config', out / 'config'))
        if probe is not None:
            evidence['actual_core_echoes'] = probe.messages
        clean('runtime', runtime.close)
        evidence['actual_groups'] = sum(value['status'] in ('passed', 'failed')
            for value in evidence['cases'].values())
        evidence['skipped_groups'] = [name for name, value in evidence['cases'].items()
            if value['status'] == 'skipped']
        evidence['all_groups_exercised'] = evidence['actual_groups'] == 11
        atomic_json(out / 'results.json', evidence)
    if failure is not None:
        raise failure
    print(json.dumps(dict(status='passed', groups=evidence['actual_groups'],
        skipped_groups=len(evidence['skipped_groups']),
        all_groups_exercised=evidence['all_groups_exercised'],
        artifacts=str(out))))


def main():
    if len(sys.argv) > 1 and sys.argv[1] == '--module-proxy':
        return module_proxy(sys.argv[2], sys.argv[3], sys.argv[4:])
    parser = argparse.ArgumentParser(description=__doc__, allow_abbrev=False)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--config-header', type=Path)
    parser.add_argument('--artifacts-dir', type=Path, required=True)
    parser.add_argument('--execution-profile', required=True)
    selection = parser.add_mutually_exclusive_group()
    selection.add_argument('--fast-click-only', action='store_true')
    selection.add_argument('--lifecycle-tail-only', action='store_true')
    for name in ('fvwm', 'mfl', 'form'):
        parser.add_argument('--' + name, type=Path)
    args = parser.parse_args()
    if not sys.platform.startswith(('linux', 'freebsd', 'openbsd')):
        parser.error('fixture requires Linux/FreeBSD/OpenBSD X11 execution')
    args.build = args.build.resolve(strict=True)
    args.config_header = (args.config_header or args.build /
        'config.h').resolve(strict=True)
    for name, relative in (('fvwm', 'fvwm3'), ('mfl', 'modules/FvwmMFL/FvwmMFL'),
            ('form', 'modules/FvwmForm/FvwmForm')):
        setattr(args, name, (getattr(args, name) or args.build /
            relative).resolve(strict=True))
    args.artifacts_dir = args.artifacts_dir.resolve()
    run(args)
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
