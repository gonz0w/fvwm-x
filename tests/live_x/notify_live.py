#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the GNU General Public License, version 2 or any later version.
"""Independent actual-X/private-bus exercise of the optional native server.

Only caller-owned nested servers, children and synthetic data are controlled.
This fixture does not establish physical, six-target or complete protocol
qualification. Its persistent native peer owns the actual sender connection.
"""
import argparse
import ctypes as C
import hashlib
import json
import os
from pathlib import Path
import select
import shutil
import subprocess
import time
import traceback

from desktop_live import connect_client
from geometry_transition_live import Pointer
from live_x_harness import HarnessRuntime, start_display_server, wait_for_predicate
from mfl_stream_live import Connection
from session_services_live import Session
from session_x_errors import SessionXErrors

PROFILES = {
    'slate-dark': ('#e5edf5', '#253140', 'xft:Sans:size=11', 2),
    'dune-light': ('#322a20', '#eee4cb', 'xft:Serif:size=12', 1),
    'classic-large': ('#ffffe0', '#252550', 'xft:DejaVu Sans Mono:size=18', 5),
}


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


class Peer:
    def __init__(self, executable, address, environment, out, name):
        self.log = (out / (name + '.stderr')).open('wb')
        self.process = subprocess.Popen([str(executable), '--private-bus', address],
            stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=self.log,
            env=environment, start_new_session=True)
        self.pending = b''
        self.records = []
        self.signals = []

    def request(self, verb, **fields):
        record = dict(verb=verb, **fields)
        wire = (json.dumps(record, ensure_ascii=False) + '\n').encode()
        assert len(wire) < 20000
        self.process.stdin.write(wire)
        self.process.stdin.flush()
        deadline = time.monotonic() + 3
        while b'\n' not in self.pending:
            assert self.process.poll() is None, self.process.returncode
            ready = select.select([self.process.stdout], [], [],
                max(0, deadline - time.monotonic()))[0]
            assert ready, 'native notification peer timed out'
            data = os.read(self.process.stdout.fileno(), 32768)
            assert data, 'native notification peer EOF'
            self.pending += data
            assert len(self.pending) <= 65536
        line, self.pending = self.pending.split(b'\n', 1)
        result = json.loads(line)
        self.records.append({'request': record, 'reply': result})
        assert len(self.records) <= 512
        return result

    def notice(self, **fields):
        result = self.request('notify', **fields)
        assert result.get('id', 0) > 0 and 'error' not in result, result
        return result['id']

    def signal(self, kind, identifier, **fields):
        def received():
            self.signals.extend(self.request('signals')['signals'])
            for index, record in enumerate(self.signals):
                if (record['signal'] == kind and record['id'] == identifier and
                        all(record.get(k) == v for k, v in fields.items())):
                    return self.signals.pop(index)
            return None
        return wait_for_predicate('actual notification signal ' + kind,
            received, time.monotonic() + 4)

    def close(self):
        try:
            if self.process.poll() is None:
                self.process.stdin.close()
                try:
                    self.process.wait(timeout=2)
                except subprocess.TimeoutExpired:
                    self.process.kill()
                    self.process.wait(timeout=2)
        finally:
            self.process.stdout.close()
            self.log.close()


class Native(Session):
    """Reuse the bounded independent X observer, without a session wrapper."""
    def __init__(self, runtime, options, out):
        self.runtime, self.options, self.out = runtime, options, out
        self.evidence = {'status': 'running', 'cases': {}, 'captures': [],
            'limits': ['Nested Linux Xvfb/private D-Bus only; not physical or full support acceptance.',
                'Three materially different native configs exercise colors/fonts/borders, not every FVWM theme.',
                'Mixed-DPI/hotplug and slow nonreading wire consumers remain separate qualification.',
                'Shared desktop DND projection runs only when --desktop is supplied; direct DND commands exercise the standalone path.']}
        self.connections, self.probes = [], []
        self.client = self.errors = None
        self.serial = 0
        self.home = runtime.root / 'native-notify'
        self.home.mkdir(mode=0o700)
        busdir = self.home / 'bus'
        busdir.mkdir(mode=0o700)
        self.env = runtime.environment({'DISPLAY': options.display,
            'HOME': str(self.home), 'FVWM_USERDIR': str(self.home),
            'FVWMMFL_SOCKET_PATH': str(self.home)})
        self.bus = runtime.start('private-notify-bus', ['dbus-daemon', '--session',
            '--nofork', '--nopidfile', '--print-address=1',
            '--address=unix:path=' + str(busdir / 'socket')],
            'notify-bus.log', self.env)
        def address():
            data = self.bus.log_path.read_text()
            return data.splitlines()[0] if data.startswith('unix:') else None
        self.address = wait_for_predicate('private bus address', address,
            time.monotonic() + 8, process=self.bus)
        self.env['DBUS_SESSION_BUS_ADDRESS'] = self.address
        self.env['FVWM_DESKTOP_ROOT'] = str(Path(__file__).resolve().parents[2] / 'default-config')
        self.env['FVWM_DESKTOP_WORKER'] = str(options.build / 'bin/FvwmPrompt/fvwm-desktop-worker')
        modules = self.home / 'modules'
        modules.mkdir()
        for name, target in [('FvwmMFL', options.mfl), ('FvwmNotify', options.notify)]:
            (modules / name).symlink_to(target)
        if options.desktop:
            (modules / 'FvwmDesktop').symlink_to(options.desktop)
        source = Path(__file__).resolve().parents[2]
        self.config = self.home / 'base.fvwm'
        self.config.write_text('Read ' + str(source / 'fvwm/ConfigFvwmDefaults') +
            '\nModulePath ' + str(modules) +
            '\nStyle * TileCascadePlacement\nStyle FvwmNotifyCenter ClickToFocus\n'
            '*FvwmNotify Colorset 42\n*FvwmNotify History 16\n'
            'Colorset 42 fg #e5edf5, bg #253140\n'
            'AddToFunc StartFunction I Module FvwmMFL\n')
        shutil.copyfile(self.config, out / 'base.fvwm')
        self.process = runtime.start('notify-core', [str(options.fvwm), '-d',
            options.display, '-f', str(self.config)], 'notify-core.log', self.env)
        endpoint = self.home / ('fvwm_mfl_' + options.display.replace(':', '_') + '.sock')
        wait_for_predicate('native MFL endpoint', endpoint.exists,
            time.monotonic() + 10, process=self.process)
        self.control = Connection(endpoint)
        self.connections.append(self.control)
        assert 'connection_profile' in self.control.record()
        self.control.send(b'set echo\n')
        self.control.echo('notify-core-ready')
        self.client = connect_client(options.display)
        self.errors = SessionXErrors(self.client, self.evidence).install()
        self.x, self.d = self.client.x, self.client.display
        self.configure_xlib()
        self.x.XGetWindowProperty.argtypes = [C.c_void_p, C.c_ulong, C.c_ulong,
            C.c_long, C.c_long, C.c_int, C.c_ulong, C.POINTER(C.c_ulong),
            C.POINTER(C.c_int), C.POINTER(C.c_ulong), C.POINTER(C.c_ulong),
            C.POINTER(C.POINTER(C.c_ubyte))]
        self.x.XGetWindowProperty.restype = C.c_int
        self.pointer = Pointer(self.client)
        self.pointer.x.XTestFakeKeyEvent.argtypes = [C.c_void_p, C.c_uint, C.c_int, C.c_ulong]
        self.pointer.x.XTestFakeKeyEvent.restype = C.c_int
        self.id_atom = self.x.XInternAtom(self.d, b'_FVWM_NOTIFY_ID', 0)
        self.peers = []
        try:
            self.first = self.peer('first')
            self.second = self.peer('second')
            self.command('Module FvwmNotify')
            self.owner = wait_for_predicate('native notification name owner',
                lambda: self.first.request('owner').get('owner'),
                time.monotonic() + 8, process=self.process)
            self.command('SendToModule FvwmNotify DND 0')
        except BaseException:
            self.close()
            raise

    def peer(self, name):
        peer = Peer(self.options.client, self.address, self.env, self.out, name)
        self.peers.append(peer)
        return peer

    def command(self, text):
        assert len(text.encode()) <= 1000 and '\0' not in text
        self.control.send((text + '\n').encode())
        self.serial += 1
        self.control.echo('notify-barrier-' + str(self.serial))

    def status(self):
        self.control.send(b'SendToModule FvwmNotify Status\n')
        deadline = time.monotonic() + 3
        for _ in range(64):
            assert time.monotonic() < deadline, 'native status deadline'
            message = self.control.record().get('echo', {}).get('message', '')
            if message.startswith('FVWM_NOTIFY_STATUS '):
                words = message.split()
                assert len(words) == 9, message
                names = ['dnd', 'active', 'history', 'history_limit',
                    'font_height', 'quarters', 'pending', 'epoch']
                return dict(zip(names, map(int, words[1:])))
        raise AssertionError('native status record limit')

    def property(self, window, atom=None):
        actual, count, remaining = C.c_ulong(), C.c_ulong(), C.c_ulong()
        format_ = C.c_int()
        data = C.POINTER(C.c_ubyte)()
        try:
            status = self.errors.read(window, 20, lambda:
                self.x.XGetWindowProperty(self.d, window, atom or self.id_atom,
                    0, 4, 0, 0, C.byref(actual), C.byref(format_), C.byref(count),
                    C.byref(remaining), C.byref(data)))
            if status == 0 and data and format_.value == 32:
                return list(C.cast(data, C.POINTER(C.c_ulong))[:count.value])
            return []
        finally:
            if data:
                self.x.XFree(data)

    def popups(self):
        return {self.property(w)[0]: w for w in
            self.named_windows('FvwmNotify popup') if self.property(w)}

    def popup(self, identifier):
        return wait_for_predicate('mapped notification ' + str(identifier),
            lambda: self.popups().get(identifier), time.monotonic() + 5,
            process=self.process)

    def absent(self, identifier):
        wait_for_predicate('notification retired ' + str(identifier),
            lambda: identifier not in self.popups(), time.monotonic() + 5,
            process=self.process)

    def focus(self):
        actual, revert = C.c_ulong(), C.c_int()
        self.x.XGetInputFocus(self.d, C.byref(actual), C.byref(revert))
        return actual.value

    def press(self, symbol):
        code = self.x.XKeysymToKeycode(self.d, self.x.XStringToKeysym(symbol.encode()))
        assert code
        for value in (1, 0):
            assert self.pointer.x.XTestFakeKeyEvent(self.d, code, value, 0)
        self.x.XSync(self.d, 0)

    def press_sequence(self, symbols):
        # Queue a complete user sequence without a round trip between keys.
        # The module must advance semantic selection before handling Return.
        for symbol in symbols:
            code = self.x.XKeysymToKeycode(self.d,
                self.x.XStringToKeysym(symbol.encode()))
            assert code
            for value in (1, 0):
                assert self.pointer.x.XTestFakeKeyEvent(self.d, code, value, 0)
        self.x.XSync(self.d, 0)

    def close(self):
        for peer in getattr(self, 'peers', []):
            peer.close()
        for connection in self.connections:
            connection.close()
        if self.errors:
            self.errors.close_display()
        elif self.client:
            self.client.close()


def run(options):
    out = options.artifacts_dir.resolve()
    out.mkdir(parents=True, exist_ok=True)
    runtime, native = HarnessRuntime(prefix='fvwm-notify-'), None
    evidence = {'status': 'running'}
    try:
        if options.display is None:
            _, options.display = start_display_server(runtime, 'Xvfb',
                ['Xvfb', '-screen', '0', '1440x1000x24', '-noreset'],
                'notify-xserver.log', time.monotonic() + 10)
        native = Native(runtime, options, out)
        evidence = native.evidence
        evidence['inputs'] = {name: {'path': str(getattr(options, name)),
            'sha256': digest(getattr(options, name))} for name in
            ('fvwm', 'notify', 'mfl', 'client', 'capture')}
        peer = native.first
        info, caps = peer.request('info'), peer.request('capabilities')
        assert info['signature'] == 'ssss' and info['spec'] == '1.3', info
        assert set(caps['capabilities']) == {'body', 'actions'}, caps
        evidence['cases']['wire_information'] = {'info': info, 'capabilities': caps}
        # A real managed application owns focus before passive arrivals.
        native.client.create()
        app = native.client.window
        native.command('WindowId ' + str(app) + ' Focus NoWarp')
        wait_for_predicate('fixture application focus', lambda: native.focus() == app,
            time.monotonic() + 5, process=native.process)
        identifier = peer.notice(summary='File transfer finished',
            body='Three documents are ready. This text wraps in the native popup.',
            actions=['open-result', 'Show documents', 'default', 'Open'])
        window = native.popup(identifier)
        assert native.focus() == app, 'passive notification stole focus'
        attributes = native.attrs(window)
        assert attributes.override_redirect and attributes.map_state == 2
        assert 0 <= attributes.x and 0 <= attributes.y
        assert attributes.x + attributes.width <= 1440
        assert attributes.y + attributes.height <= 1000
        same = peer.notice(replaces=identifier, summary='Atomic replacement',
            body='The record and window were replaced without recreating the popup.',
            actions=['show', 'Show result'])
        assert same == identifier and native.popup(identifier) == window
        assert native.focus() == app
        evidence['cases']['passive_replacement'] = {'id': identifier, 'window': window,
            'focus_before_after': app, 'bounds': [attributes.x, attributes.y,
                attributes.width, attributes.height]}
        assert native.second.notice(replaces=identifier,
            summary='Replacement from a transient peer',
            actions=['show', 'Show result']) == identifier
        assert native.popup(identifier) == window
        assert 'error' in native.second.request('close', id=4294967295)
        closeable = peer.notice(summary='Known IDs support transient close peers')
        assert 'error' not in native.second.request('close', id=closeable)
        peer.signal('closed', closeable, reason=3)
        evidence['cases']['standard_cross_connection_ids'] = 'passed'
        malformed = peer.request('malformed')
        assert 'error' in malformed, malformed
        for fields in ({'summary': 'x' * 257}, {'body': 'x' * 2049},
                {'timeout': -2}, {'urgency': 3}, {'actions': ['odd']}):
            result = peer.request('notify', **fields)
            assert 'error' in result, (fields, result)
        assert native.popup(identifier) == window
        evidence['cases']['malformed_refusal_and_retention'] = 'passed'
        native.command('SendToModule FvwmNotify Focus')
        center = wait_for_predicate('managed notification center',
            lambda: native.named('FVWM-X notifications'), time.monotonic() + 5,
            process=native.process)
        wait_for_predicate('explicit keyboard center focus',
            lambda: native.focus() == center, time.monotonic() + 5,
            process=native.process)
        assert native.property(center) == [identifier]
        native.press('Return')
        native.second.signal('action', identifier, key='show')
        native.second.signal('closed', identifier, reason=2)
        assert not any(item['signal'] == 'action' and item['id'] == identifier
            for item in peer.request('signals')['signals'])
        native.absent(identifier)
        native.press('Escape')
        evidence['cases']['keyboard_action'] = {'center': center, 'id': identifier,
            'key': 'show', 'close_reason': 2}
        first_choice = peer.notice(summary='First record stays untouched',
            actions=['wrong-record', 'First action'])
        next_choice = peer.notice(summary='Queued navigation selects this record',
            actions=['selected-record', 'Selected action'])
        native.popup(first_choice)
        native.popup(next_choice)
        native.command('SendToModule FvwmNotify Focus')
        center = wait_for_predicate('fresh managed notification center',
            lambda: native.named('FVWM-X notifications'), time.monotonic() + 5,
            process=native.process)
        wait_for_predicate('first keyboard record selected',
            lambda: native.property(center) == [first_choice] and
                native.focus() == center, time.monotonic() + 5,
            process=native.process)
        native.press_sequence(['Down', 'Return'])
        peer.signal('action', next_choice, key='selected-record')
        peer.signal('closed', next_choice, reason=2)
        native.popup(first_choice)
        native.command('SendToModule FvwmNotify DismissAll')
        peer.signal('closed', first_choice, reason=2)
        native.press('Escape')
        evidence['cases']['queued_keyboard_navigation'] = {
            'sequence': ['Down', 'Return'], 'untouched_id': first_choice,
            'invoked_id': next_choice, 'key': 'selected-record',
            'one_final_xsync': True}
        expiring = peer.notice(summary='Expires independently', timeout=150)
        peer.signal('closed', expiring, reason=1)
        native.absent(expiring)
        closing = peer.notice(summary='Explicit close')
        native.popup(closing)
        assert 'error' not in peer.request('close', id=closing)
        peer.signal('closed', closing, reason=3)
        assert 'error' in peer.request('close', id=closing)
        dnd = peer.notice(summary='Retired when DND is enabled')
        native.popup(dnd)
        native.command('SendToModule FvwmNotify DND 1')
        peer.signal('closed', dnd, reason=4)
        suppressed = peer.notice(summary='Suppressed by DND')
        peer.signal('closed', suppressed, reason=4)
        native.absent(suppressed)
        native.command('SendToModule FvwmNotify DND 0')
        assert not native.popups(), 'DND disable replayed suppressed notices'
        evidence['cases']['expiry_close_dnd'] = 'passed'
        ids = [peer.notice(summary='Bounded flood ' + str(i)) for i in range(32)]
        wait_for_predicate('bounded visible flood stack',
            lambda: len(native.popups()) == 3, time.monotonic() + 5,
            process=native.process)
        assert 'error' in peer.request('notify', summary='Capacity refusal')
        assert len(native.popups()) <= 3
        native.command('SendToModule FvwmNotify DismissAll')
        for identifier in ids:
            peer.signal('closed', identifier, reason=2)
        assert not native.popups()
        recovered = peer.notice(summary='Recovered after overload')
        native.popup(recovered)
        native.command('SendToModule FvwmNotify DismissAll')
        peer.signal('closed', recovered, reason=2)
        evidence['cases']['flood_refusal_recovery'] = {'active_limit': 32,
            'visible_limit': 3, 'ids': ids, 'recovered_id': recovered}
        font_heights = {}
        for profile, (foreground, background, font, border) in PROFILES.items():
            overlay = out / (profile + '.fvwm')
            overlay.write_text('Colorset 42 fg ' + foreground + ', bg ' + background +
                '\nStyle * BorderWidth ' + str(border) + '\n'
                '*FvwmNotify Font "' + font + '"\nSendToModule FvwmNotify Reload\n')
            # The source path is caller controlled and contains no whitespace in
            # the owned temporary tree. Copy it for this actual Read invocation.
            current = native.home / (profile + '.fvwm')
            shutil.copyfile(overlay, current)
            native.command('Read ' + str(current))
            identifier = peer.notice(summary='FVWM-X native notifications',
                body='A bounded, passive popup. The palette and font update live.\n'
                'Labels remain text: $(touch never-executed); Echo is not a command.',
                actions=['result', 'Show result', 'dismiss', 'Later'])
            native.popup(identifier)
            def applied_style():
                state = native.status()
                if state['pending'] or state['font_height'] < 15:
                    return None
                if profile == 'classic-large' and state['font_height'] <= font_heights['slate-dark']:
                    return None
                return state
            style = wait_for_predicate('actual native font and configuration',
                applied_style, time.monotonic() + 5, process=native.process)
            font_heights[profile] = style['font_height']
            native.evidence.setdefault('native_styles', {})[profile] = style
            native.capture(profile + '.png')
            native.command('SendToModule FvwmNotify Focus')
            wait_for_predicate('theme keyboard center',
                lambda: native.named('FVWM-X notifications'),
                time.monotonic() + 5, process=native.process)
            native.capture(profile + '-center.png')
            native.press('Escape')
            native.command('SendToModule FvwmNotify DismissAll')
            peer.signal('closed', identifier, reason=2)
            evidence.setdefault('theme_configs', []).append({'profile': profile,
                'configuration': overlay.name, 'sha256': digest(overlay),
                'font': font, 'foreground': foreground, 'background': background,
                'border': border})
        native.command('SendToModule FvwmNotify History')
        history = wait_for_predicate('display-only notification history',
            lambda: native.named('FVWM-X notifications'), time.monotonic() + 5,
            process=native.process)
        assert native.property(history) == [0]
        native.capture('bounded-history.png')
        native.press('Escape')
        evidence['cases']['history_display_only'] = {'window': history,
            'selected_live_id': 0, 'configured_limit': 16}
        if options.desktop:
            native.command('Module FvwmDesktop')
            native.command('SendToModule FvwmDesktop notification-policy')
            native.command('SendToModule FvwmDesktop set notification_dnd true')
            wait_for_predicate('committed desktop DND projection',
                lambda: native.status()['dnd'] == 1, time.monotonic() + 8,
                process=native.process)
            suppressed = peer.notice(summary='Shared committed DND suppresses this')
            peer.signal('closed', suppressed, reason=4)
            native.absent(suppressed)
            native.command('SendToModule FvwmDesktop set notification_dnd false')
            wait_for_predicate('committed desktop DND disabled',
                lambda: native.status()['dnd'] == 0, time.monotonic() + 8,
                process=native.process)
            assert not native.popups(), 'shared DND disabled replayed backlog'
            evidence['cases']['shared_committed_dnd_projection'] = 'passed'
        native.command('Module FvwmNotify')
        native.control.echo('competing-module-core-responsive')
        # An existing owner is positively retained across a new attempt.
        assert peer.request('owner')['owner'] == native.owner
        marker = peer.notice(summary='Existing server retained its name')
        native.popup(marker)
        native.command('SendToModule FvwmNotify DismissAll')
        peer.signal('closed', marker, reason=2)
        evidence['cases']['competing_owner_preserved'] = native.owner
        native.command('KillModule FvwmNotify')
        wait_for_predicate('server exit releases name',
            lambda: 'error' in peer.request('owner'), time.monotonic() + 5,
            process=native.process)
        native.command('Module FvwmNotify')
        new_owner = wait_for_predicate('explicit server restart',
            lambda: peer.request('owner').get('owner'), time.monotonic() + 5,
            process=native.process)
        assert new_owner != native.owner
        native.command('SendToModule FvwmNotify DND 0')
        native.popup(peer.notice(summary='Explicit restart remains usable'))
        native.bus.close(time.monotonic() + 2)
        wait_for_predicate('bus loss retires native popups',
            lambda: not native.popups(), time.monotonic() + 5,
            process=native.process)
        native.control.echo('desktop-usable-after-bus-loss')
        evidence['cases']['explicit_restart_and_bus_loss'] = {'old': native.owner,
            'new': new_owner, 'desktop_echo': 'passed'}
        evidence['status'] = 'passed'
    except BaseException as failure:
        evidence['status'] = 'failed'
        evidence['error'] = str(failure)
        evidence['traceback'] = traceback.format_exc()
        raise
    finally:
        if native:
            for name, peer in [('first', native.first), ('second', native.second)]:
                (out / (name + '-wire.json')).write_text(json.dumps(peer.records, indent=2) + '\n')
            native.close()
            if native.errors.unexpected:
                evidence['status'] = 'failed'
                evidence['cleanup_x_errors'] = native.errors.unexpected
        for process in runtime.processes:
            if process.log_path.exists():
                shutil.copyfile(process.log_path, out / process.log_path.name)
        runtime.close()
        (out / 'result.json').write_text(json.dumps(evidence, indent=2) + '\n')
    assert evidence['status'] == 'passed', evidence


def main():
    parser = argparse.ArgumentParser(allow_abbrev=False)
    parser.add_argument('--build', type=Path, required=True)
    for name in ('fvwm', 'notify', 'client', 'mfl', 'capture', 'desktop'):
        parser.add_argument('--' + name, type=Path)
    parser.add_argument('--display')
    parser.add_argument('--artifacts-dir', type=Path, required=True)
    options = parser.parse_args()
    defaults = {'fvwm': options.build / 'fvwm3',
        'notify': options.build / 'modules/FvwmNotify/FvwmNotify',
        'client': options.build / 'tests/notify-client',
        'mfl': options.build / 'modules/FvwmMFL/FvwmMFL',
        'capture': options.build / 'bin/fvwm-desktop-capture'}
    for name, value in defaults.items():
        setattr(options, name, (getattr(options, name) or value).resolve())
        assert getattr(options, name).is_file(), getattr(options, name)
    run(options)


if __name__ == '__main__':
    main()
