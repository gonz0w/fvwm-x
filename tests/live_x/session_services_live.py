#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License, version 2 or later.
"""Observe real session services, cooperative clients and native Session forms.

Source-only authoring does not establish a pass. Run against one coherent built
or installed prefix. Optional bus cases require a wrapper already linked with
its actual installed optional helper; this fixture does not replace adapters.
"""
import argparse
import ctypes as C
import hashlib
import json
import os
from settings_snapshot import expanded_settings
from pathlib import Path
import shutil
import subprocess
import sys
import time

from colorset_live import WindowAttributes
from desktop_live import connect_client
from geometry_transition_live import Pointer
from legacy_property_live import Event
from live_x_harness import HarnessRuntime, start_display_server, wait_for_predicate
from mfl_request_live import frame, reply
from mfl_stream_live import Connection
from session_x_errors import SessionXErrors


COVERAGE = 'managed-applications-excluding-active-module-classes-and-dock-desktop'
PROFILES = ['slate-dark', 'dune-light', 'classic-large']


class ClassHint(C.Structure):
    _fields_ = [('res_name', C.c_char_p), ('res_class', C.c_char_p)]


class Session:
    def __init__(self, runtime, options, out, profile, bus='off', address=None,
                 power='portable', extra_env=None):
        self.runtime, self.options, self.out = runtime, options, out
        self.profile = profile
        self.connections, self.probes = [], []
        self.client = self.errors = None
        self.serial = 0
        self.evidence = {'status': 'running', 'profile': profile, 'bus_policy': bus, 'cases': {},
            'captures': [], 'limits': [
                'Nested Xvfb only; no physical hardware or full platform claim.',
                'Eligible application classification is descriptive, not process authentication.',
                'Core action replies attest requests; wrapper exit and client observations are separate.',
                'External XSMP cancellation and passive notification-server UI remain out of scope.']}
        self.home = runtime.root / profile
        self.home.mkdir(mode=0o700)
        self.user, self.private = self.home / 'user', self.home / 'runtime'
        self.user.mkdir(mode=0o700)
        self.private.mkdir(mode=0o700)
        modules = self.home / 'modules'
        modules.mkdir()
        if options.modules_dir:
            for executable in options.modules_dir.iterdir():
                if executable.is_file() and os.access(executable, os.X_OK):
                    (modules / executable.name).symlink_to(executable.resolve())
        else:
            for directory in (options.build / 'modules').iterdir():
                executable = directory / directory.name
                if executable.is_file():
                    (modules / directory.name).symlink_to(executable.resolve())
        for name, executable in [('FvwmMFL', options.mfl),
                                 ('FvwmForm', options.form),
                                 ('FvwmDesktop', options.desktop)]:
            alias = modules / name
            if alias.is_symlink():
                alias.unlink()
            alias.symlink_to(executable)
        # A caller may pin additional modules to this same candidate build.
        # Names stay private basenames; never replace another profile's files.
        links = getattr(options, 'module_links', {})
        assert len(links) <= 8
        for name, executable in links.items():
            assert name and len(name) <= 64 and all(
                char.isascii() and (char.isalnum() or char in '_-')
                for char in name), name
            executable = Path(executable).resolve()
            assert executable.is_file(), executable
            alias = modules / name
            if alias.is_symlink():
                alias.unlink()
            assert not alias.exists(), alias
            alias.symlink_to(executable)
        # A private sibling stage makes the selected core unambiguous without
        # changing its build-configured fallback or the user's installation.
        binaries = self.home / 'bin'
        binaries.mkdir()
        shutil.copyfile(options.session, binaries / 'fvwm-session')
        (binaries / 'fvwm-session').chmod(0o700)
        (binaries / 'fvwm3').symlink_to(options.fvwm)
        self.config = self.home / 'configuration.fvwm'
        body = (options.data_root / 'config').read_text().replace(
            '$[FVWM_DATADIR]/default-config', str(options.data_root))
        if profile == 'classic-large':
            body = body.replace('Sans:size=10:antialias=True',
                                'DejaVu Sans Mono:size=18:antialias=True')
        source = Path(__file__).resolve().parents[2]
        self.config.write_text('ModulePath ' + str(modules) + '\nRead ' +
            str(source / 'fvwm/ConfigFvwmDefaults') + '\n' + body +
            '\nAddToFunc StartFunction I Module FvwmMFL\n')
        self.environment_marker = self.home / 'application-environment.json'
        recorder = self.home / 'record-environment.py'
        recorder.write_text('import json, os\nfrom pathlib import Path\nPath(' +
            repr(str(self.environment_marker)) + ').write_text(json.dumps({' +
            "'bus': os.environ.get('DBUS_SESSION_BUS_ADDRESS'), " +
            "'control': bool(os.environ.get('FVWM_SESSION_CONTROL_SOCKET'))}))\n")
        autostart = self.home / 'config' / 'autostart'
        autostart.mkdir(parents=True)
        (autostart / 'environment.desktop').write_text(
            '[Desktop Entry]\nType=Application\nName=Session environment witness\n'
            'Exec="' + sys.executable + '" "' + str(recorder) + '"\n')
        if bus != 'off' or power != 'portable':
            state = self.user / 'desktop-state.json'
            saved = {'session_bus': bus, 'notification_dnd': False,
                'session_notifications': 'external' if bus != 'off' else 'off'}
            if power != 'portable':
                saved['session_power'] = power
            state.write_text(json.dumps(saved) + '\n')
            state.chmod(0o600)
        self.env = runtime.environment({'DISPLAY': self.display,
            'HOME': str(self.home), 'FVWM_USERDIR': str(self.user),
            'FVWM_DESKTOP_ROOT': str(options.data_root),
            'FVWMMFL_SOCKET_PATH': str(self.home),
            'XDG_RUNTIME_DIR': str(self.private),
            'XDG_CONFIG_HOME': str(autostart.parent),
            'XDG_CONFIG_DIRS': str(self.home / 'absent-config'),
            'XDG_DATA_HOME': str(self.home / 'data'),
            'XDG_DATA_DIRS': str(self.home / 'absent-data')})
        for key in ['SESSION_MANAGER', 'DBUS_SESSION_BUS_ADDRESS',
                    'FVWM_SESSION_CONTROL_SOCKET', 'FVWM_SESSION_CONTROL_CAPABILITY',
                    'FVWM_SESSION_READY_FD']:
            self.env.pop(key, None)
        if address:
            self.env['DBUS_SESSION_BUS_ADDRESS'] = address
        if extra_env:
            self.env.update(extra_env)
        self.process = runtime.start(profile, [str(binaries / 'fvwm-session'),
            '--', '-d', self.display, '-f', str(self.config)], profile + '.log', self.env)
        endpoint = self.home / ('fvwm_mfl_' + self.display.replace(':', '_') + '.sock')
        wait_for_predicate('session MFL endpoint', endpoint.exists,
            time.monotonic() + 15, process=self.process)
        self.control, self.rpc = [self.connect(endpoint) for _ in range(2)]
        self.control.send(b'set echo\n')
        self.control.echo('session-native-connected')
        self.client = connect_client(self.display)
        self.errors = SessionXErrors(self.client, self.evidence).install()
        self.x, self.d = self.client.x, self.client.display
        self.configure_xlib()
        self.pointer = Pointer(self.client)
        wait_for_predicate('desktop settings ready', self.ready,
            time.monotonic() + 15, process=self.process)
        wait_for_predicate('real session-owner ready', self.owner_ready,
            time.monotonic() + 15, process=self.process)
        self.provider = self.settings['provider']
        self.change({'palette': 'dune' if profile == 'dune-light' else 'slate',
            'appearance': 'light' if profile == 'dune-light' else 'dark',
            'transparency': False})
        self.evidence['configuration_sha256'] = hashlib.sha256(self.config.read_bytes()).hexdigest()
        if profile == 'classic-large':
            # Load legacy colors through normal FVWM configuration after the
            # shared theme projection. The separate Form deliberately uses
            # legacy colors, while the Session consumer keeps semantic roles.
            overlay = self.home / 'classic-customization.fvwm'
            overlay.write_text('Style * BorderWidth 3, HandleWidth 6\n'
                '*FvwmFormDefault: Fore #f8f8f2\n'
                '*FvwmFormDefault: Back #303640\n'
                '*FvwmFormDefault: ItemFore #161c24\n'
                '*FvwmFormDefault: ItemBack #e8dfc9\n'
                '*ClassicSessionColors: Title "Legacy colors"\n'
                '*ClassicSessionColors: Colorset -1\n'
                '*ClassicSessionColors: ItemColorset -1\n'
                '*ClassicSessionColors: Position 1110 80\n'
                '*ClassicSessionColors: Line left\n'
                '*ClassicSessionColors: Text "Legacy colors"\n'
                '*ClassicSessionColors: Line left\n'
                '*ClassicSessionColors: Button quit "Close" ^[\n'
                '*ClassicSessionColors: Command Nop\n'
                'Module FvwmForm ClassicSessionColors\n')
            self.command('Read ' + str(overlay))
            self.form('Legacy colors')
            shutil.copyfile(overlay, self.out / overlay.name)
            self.evidence['classic_customization'] = {
                'file': overlay.name,
                'sha256': hashlib.sha256(overlay.read_bytes()).hexdigest(),
                'font': 'DejaVu Sans Mono:size=18:antialias=True',
                'legacy_colors': 'Fore/Back/ItemFore/ItemBack; separate no-colorset Form',
                'session_colors': 'explicit semantic colorsets take precedence',
                'borders': 'BorderWidth3 HandleWidth6'}

    @property
    def display(self):
        return self.options.display

    def connect(self, endpoint):
        connection = Connection(endpoint)
        connection.socket.settimeout(8)
        assert 'connection_profile' in connection.record()
        self.connections.append(connection)
        return connection

    def request(self, method='query', resource='settings', **fields):
        self.serial += 1
        token = 'session-' + str(self.serial)
        self.rpc.send(frame(token, method=method, resource=resource, **fields))
        return reply(self.rpc, token)

    def ready(self):
        self.serial += 1
        token = 'session-ready-' + str(self.serial)
        self.rpc.send(frame(token, resource='settings', scope='all'))
        packet = self.rpc.record()['reply']
        if packet['status'] != 'ok':
            return False
        snapshot = expanded_settings(packet['result'])
        if snapshot.get('settings', {}).get('palette', {}).get('application') != 'submitted-to-fvwm':
            return False
        expected = {'palette', 'appearance', 'day', 'night', 'transparency',
            'motion', 'keep_wallpaper', 'launcher_opacity', 'wallpaper_palette',
            'wallpaper_appearance', 'contrast', 'session_bus',
            'session_notifications', 'notification_dnd', 'session_power',
            'tray_enabled', 'tray_gap', 'tray_padding', 'tray_orientation',
            'tray_modern', 'tray_icon_size', 'native_settings_enabled',
            'application_font', 'icon_theme', 'cursor_theme', 'cursor_size'}
        assert set(snapshot['settings']) == expected, snapshot
        self.settings = snapshot
        return snapshot

    def owner_ready(self):
        snapshot = self.request(resource='session', provider=self.settings['provider'])
        return snapshot if snapshot['state'] == 'observed' else None

    def status(self):
        return self.request(resource='session', provider=self.provider)

    def change(self, values):
        deadline = time.monotonic() + 8
        for attempt in range(64):
            snapshot = self.request(resource='settings', scope='all')
            self.serial += 1
            token = 'session-set-' + str(self.serial)
            self.rpc.send(frame(token, method='action', resource='settings',
                operation='set', provider=snapshot['provider'],
                generation=snapshot['generation'], values=values))
            packet = self.rpc.record()['reply']
            assert packet['id'] == token, packet
            code = packet.get('error', {}).get('code')
            self.evidence.setdefault('settings_change_admissions', []).append({
                'id': token, 'attempt': attempt + 1, 'status': packet['status'],
                'error': code, 'generation': snapshot['generation']})
            if packet['status'] == 'ok':
                self.settings = packet['result']
                break
            # A rejected CAS performed no mutation. Only that explicit reply
            # permits rereading the generation and resubmitting these values.
            # Disconnects, timeouts and every other error remain failures.
            assert packet['status'] == 'error' and code == 'stale_generation', packet
            assert time.monotonic() < deadline, packet
            time.sleep(.05)
        else:
            raise AssertionError('settings CAS admission retry limit exhausted')
        self.barrier()
        return self.settings

    def barrier(self):
        self.serial += 1
        token = 'session-barrier-' + str(self.serial)
        self.control.send(('SendToModule FvwmDesktop barrier ' + token + '\n').encode())
        self.control.expect_echo(token)

    def command(self, text):
        self.control.send((text + '\n').encode())
        self.barrier()

    def operation(self, target):
        deadline = time.monotonic() + 8
        while True:
            snapshot = self.request(resource='settings', scope='all')
            self.serial += 1
            token = 'session-start-' + str(self.serial)
            self.rpc.send(frame(token, method='action', resource='operations',
                operation='start', provider=self.provider,
                generation=snapshot['generation'], target=target))
            packet = self.rpc.record()['reply']
            assert packet['id'] == token, packet
            if packet['status'] == 'ok':
                return packet['result']['operation']['id']
            # Retry only an explicit refused admission. Never retry an
            # accepted logout or notification operation automatically.
            assert packet['error']['code'] in ['operation_busy', 'stale_generation'], packet
            assert time.monotonic() < deadline, packet
            time.sleep(.05)

    def operation_state(self, identifier):
        return self.request(resource='operations',
            provider=self.provider, target=identifier)['operation']

    def cancel(self, identifier):
        record = self.operation_state(identifier)
        return self.request('action', resource='operations', operation='cancel',
            provider=self.provider, target=identifier, generation=record['generation'])

    def clients(self):
        clients, offset, generation, total = [], 0, None, None
        while True:
            fields = {'offset': offset, 'limit': 128}
            if generation:
                fields['generation'] = generation
            page = self.request(resource='session-clients', **fields)
            assert page['scope'] == 'session-clients' and page['coverage'] == COVERAGE
            assert not page['truncated'] and page['offset'] == offset and page['total'] <= 512
            if generation:
                assert page['generation'] == generation and page['total'] == total
            generation, total = page['generation'], page['total']
            clients += page['clients']
            next_ = page['next_offset']
            if next_ is None:
                assert len(clients) == total
                return clients, generation
            assert next_ == len(clients) and next_ > offset
            offset = next_

    def configure_xlib(self):
        signatures = {
            'XInternAtom': ([C.c_void_p, C.c_char_p, C.c_int], C.c_ulong),
            'XSetClassHint': ([C.c_void_p, C.c_ulong, C.POINTER(ClassHint)], C.c_int),
            'XCheckTypedWindowEvent': ([C.c_void_p, C.c_ulong, C.c_int, C.POINTER(Event)], C.c_int),
            'XSetWMProtocols': ([C.c_void_p, C.c_ulong, C.POINTER(C.c_ulong), C.c_int], C.c_int),
            'XFetchName': ([C.c_void_p, C.c_ulong, C.POINTER(C.c_void_p)], C.c_int),
            'XGetWindowAttributes': ([C.c_void_p, C.c_ulong, C.POINTER(WindowAttributes)], C.c_int),
            'XTranslateCoordinates': ([C.c_void_p, C.c_ulong, C.c_ulong, C.c_int,
                C.c_int, C.POINTER(C.c_int), C.POINTER(C.c_int), C.POINTER(C.c_ulong)], C.c_int),
            'XStringToKeysym': ([C.c_char_p], C.c_ulong),
            'XKeysymToKeycode': ([C.c_void_p, C.c_ulong], C.c_uint),
            'XGetInputFocus': ([C.c_void_p, C.POINTER(C.c_ulong), C.POINTER(C.c_int)], C.c_int),
        }
        for name, (arguments, result) in signatures.items():
            function = getattr(self.x, name)
            function.argtypes, function.restype = arguments, result
        self.protocols = self.x.XInternAtom(self.d, b'WM_PROTOCOLS', 0)
        self.delete = self.x.XInternAtom(self.d, b'WM_DELETE_WINDOW', 0)
        self.pointer_library_configured = False

    def create(self, title, deletable=True, close=False):
        window = self.x.XCreateSimpleWindow(self.d, self.client.root, 100, 120,
            240, 100, 0, 0, 0x394653)
        self.x.XStoreName(self.d, window, title.encode())
        hint = ClassHint(b'session-fixture', b'SessionFixture')
        self.x.XSetClassHint(self.d, window, C.byref(hint))
        self.x.XSelectInput(self.d, window, 1 << 17)
        if deletable:
            atoms = (C.c_ulong * 1)(self.delete)
            assert self.x.XSetWMProtocols(self.d, window, atoms, 1)
        self.x.XMapWindow(self.d, window)
        self.x.XSync(self.d, 0)
        probe = {'window': window, 'title': title, 'live': True,
                 'close': close, 'messages': 0, 'deletable': deletable}
        self.probes.append(probe)
        wait_for_predicate('eligible ordinary X client',
            lambda: any(c['title'] == title for c in self.clients()[0]),
            time.monotonic() + 8, process=self.process)
        return probe

    def pump(self):
        for probe in self.probes:
            if not probe['live']:
                continue
            event = Event()
            while self.x.XCheckTypedWindowEvent(self.d, probe['window'], 33, C.byref(event)):
                assert event.message.message_type == self.protocols
                assert event.message.format == 32 and event.message.data[0] == self.delete
                probe['messages'] += 1
                if probe['close']:
                    self.destroy(probe)
                    break
        self.x.XSync(self.d, 0)
        return True

    def destroy(self, probe):
        if probe['live']:
            self.x.XDestroyWindow(self.d, probe['window'])
            self.x.XSync(self.d, 0)
            probe['live'] = False

    def attrs(self, window):
        result = WindowAttributes()
        return result if self.errors.read(window, 3, lambda:
            self.x.XGetWindowAttributes(self.d, window, C.byref(result))) else None

    def children(self, window):
        root, parent, count = C.c_ulong(), C.c_ulong(), C.c_uint()
        items = C.POINTER(C.c_ulong)()
        try:
            if self.errors.read(window, 15, lambda: self.x.XQueryTree(self.d,
                    window, C.byref(root), C.byref(parent), C.byref(items), C.byref(count))):
                return list(items[:min(count.value, 512)])
            return []
        finally:
            if items:
                self.x.XFree(items)

    def named_windows(self, title):
        found = []
        queue, seen = [(self.client.root, 0)], 0
        while queue and seen < 2048:
            window, depth = queue.pop()
            seen += 1
            name = C.c_void_p()
            try:
                if self.errors.read(window, 20, lambda:
                        self.x.XFetchName(self.d, window, C.byref(name))) and name.value:
                    if C.string_at(name).decode('utf-8', 'replace') == title:
                        attributes = self.attrs(window)
                        if attributes and attributes.map_state == 2:
                            found.append(window)
            finally:
                if name.value:
                    self.x.XFree(name)
            if depth < 4:
                queue.extend((child, depth + 1) for child in self.children(window))
        return found

    def named(self, title):
        windows = self.named_windows(title)
        return windows[0] if windows else None

    def form(self, title):
        def managed():
            windows = self.named_windows(title)
            if title == 'Session and services' and len(windows) != 1:
                return None
            window = windows[0] if windows else None
            if window and self.client.parent(window) != self.client.root:
                attrs = self.attrs(window)
                buttons = any(child_attrs and child_attrs.map_state == 2 and
                    child_attrs.all_event_masks & (1 << 2)
                    for child_attrs in (self.attrs(child) for child in self.children(window)))
                if attrs and attrs.all_event_masks & 1 and buttons:
                    return window
            return None
        window = wait_for_predicate('managed native ' + title, managed,
            time.monotonic() + 10, process=self.process)
        attributes = self.attrs(window)
        root_x, root_y, child = C.c_int(), C.c_int(), C.c_ulong()
        assert self.x.XTranslateCoordinates(self.d, window, self.client.root, 0, 0,
            C.byref(root_x), C.byref(root_y), C.byref(child))
        bounds = {'title': title, 'x': root_x.value, 'y': root_y.value,
            'width': attributes.width, 'height': attributes.height}
        assert 0 <= bounds['x'] and 0 <= bounds['y']
        assert bounds['x'] + bounds['width'] <= 1440
        assert bounds['y'] + bounds['height'] <= 1000, bounds
        for widget in self.children(window):
            item = self.attrs(widget)
            if item and item.map_state == 2:
                assert item.x >= 0 and item.y >= 0
                assert item.x + item.width <= attributes.width
                assert item.y + item.height <= attributes.height
        parent = self.client.parent(window)
        decorated = self.client.parent(parent)
        if decorated != self.client.root:
            frame_attrs = self.attrs(decorated)
            assert frame_attrs and 0 <= frame_attrs.x and 0 <= frame_attrs.y
            assert frame_attrs.x + frame_attrs.width <= 1440
            assert frame_attrs.y + frame_attrs.height <= 1000
            bounds['frame'] = {'x': frame_attrs.x, 'y': frame_attrs.y,
                'width': frame_attrs.width, 'height': frame_attrs.height}
        if title == 'Session and services':
            panels = self.named_windows(title)
            assert panels == [window], panels
            bounds['mapped_session_panels'] = len(panels)
        self.evidence.setdefault('form_geometry', []).append(bounds)
        return window

    def key(self, window, symbol):
        self.command('WindowId ' + str(window) + ' Raise\nWindowId ' + str(window) + ' Focus NoWarp')
        def focused():
            actual, revert = C.c_ulong(), C.c_int()
            self.x.XGetInputFocus(self.d, C.byref(actual), C.byref(revert))
            return actual.value == window
        wait_for_predicate('actual native Form focus', focused,
            time.monotonic() + 5, process=self.process)
        if not self.pointer_library_configured:
            self.pointer.x.XTestFakeKeyEvent.argtypes = [C.c_void_p, C.c_uint, C.c_int, C.c_ulong]
            self.pointer.x.XTestFakeKeyEvent.restype = C.c_int
            self.pointer_library_configured = True
        code = self.x.XKeysymToKeycode(self.d, self.x.XStringToKeysym(symbol.encode()))
        assert code
        for pressed in (1, 0):
            assert self.pointer.x.XTestFakeKeyEvent(self.d, code, pressed, 0)
        self.x.XSync(self.d, 0)

    def dismiss_notice(self, window):
        # The changed contract is the actual F6 fallback's visibility. The
        # existing NeverFocus notice button is outside this focused gate;
        # retire the fixture notice through its advertised WM_DELETE path.
        self.command('WindowId ' + str(window) + ' Delete')
        wait_for_predicate('fallback notice cooperative cleanup',
            lambda: self.named('FVWM-X') is None,
            time.monotonic() + 5, process=self.process)
        self.evidence.setdefault('notice_cleanup', []).append({
            'window': window, 'witness': 'core-engine WM_DELETE cleanup',
            'focus_policy': 'shipped NeverFocus preserved',
            'legacy_dismiss_button': 'not exercised by this focused gate'})

    def assert_notice_above_panel(self, notice):
        panel = self.form('Session and services')
        def root_child(window):
            for _ in range(5):
                parent = self.client.parent(window)
                if parent == self.client.root:
                    return window
                window = parent
            raise AssertionError('native Form ancestry exceeded fixture bound')
        notice_frame, panel_frame = root_child(notice), root_child(panel)
        stacking = self.children(self.client.root)
        assert notice_frame in stacking and panel_frame in stacking, stacking
        assert stacking.index(notice_frame) > stacking.index(panel_frame), {
            'notice_frame': notice_frame, 'panel_frame': panel_frame,
            'root_bottom_to_top': stacking}
        self.evidence['fallback_stacking'] = {'notice_frame': notice_frame,
            'panel_frame': panel_frame, 'root_bottom_to_top': stacking,
            'raised_by_fixture_before_capture': False}


    def capture(self, filename):
        result = subprocess.run([str(self.options.capture), str(self.out)],
            env=self.env, capture_output=True, text=True, timeout=35)
        assert result.returncode == 0, result.stderr
        path = Path(result.stdout.strip().removeprefix('Screenshot saved: '))
        destination = self.out / filename
        path.replace(destination)
        self.evidence['captures'].append({'file': filename,
            'bytes': destination.stat().st_size,
            'sha256': hashlib.sha256(destination.read_bytes()).hexdigest()})

    def wait_operation(self, identifier, state, timeout=12):
        def observed():
            self.pump()
            record = self.operation_state(identifier)
            return record if record['state'] == state else None
        return wait_for_predicate('session operation ' + state, observed,
            time.monotonic() + timeout, process=self.process)

    def close(self):
        for probe in self.probes:
            self.destroy(probe)
        for connection in self.connections:
            connection.close()
        if self.errors:
            self.errors.close_display()
            assert not self.errors.unexpected, self.errors.unexpected
        elif self.client:
            self.client.close()
        self.out.mkdir(parents=True, exist_ok=True)
        (self.out / 'configuration.fvwm').write_bytes(self.config.read_bytes())
        for path in self.runtime.root.glob('*.log'):
            shutil.copyfile(path, self.out / path.name)
        (self.out / 'evidence.json').write_text(json.dumps(self.evidence, indent=2) + '\n')


def dismiss_panel(session):
    window = session.named('Session and services')
    if window:
        session.key(window, 'Escape')
        wait_for_predicate('native Session panel closed',
            lambda: session.named('Session and services') is None,
            time.monotonic() + 5, process=session.process)


def focused_cases(session):
    s = session
    initial = s.status()
    assert initial['active_bus_policy'] == 'off'
    inventory = {item['id']: item for item in initial['items']}
    assert inventory['bus']['state'] in ['stopped', 'unavailable', 'installed']
    assert inventory['bus']['ownership'] == 'none'
    assert not initial['notification_server']
    s.evidence['cases']['default-no-bus'] = initial
    # Open the actual native consumer and use real Form keys, not guessed
    # callbacks or private controller tokens. Query checks public state only.
    s.command('SendToModule FvwmDesktop session-panel')
    window = s.form('Session and services')
    s.capture('session-panel.png')
    s.key(window, 'F5')
    status_refresh = s.operation('session-status')
    s.wait_operation(status_refresh, 'completed')
    window = s.form('Session and services')
    s.key(window, 'F6')
    fallback = s.form('FVWM-X')
    # Capture production stacking before the pointer helper raises anything:
    # panel reconciliation must precede the new NeverFocus fallback notice.
    s.assert_notice_above_panel(fallback)
    s.capture('notification-unavailable-fallback.png')
    s.dismiss_notice(fallback)
    s.evidence['cases']['native-notification-unavailable-fallback'] = {'status': 'passed',
        'witness': 'real F6 request returned a native fallback notice'}
    window = s.form('Session and services')
    s.key(window, 'F2')
    editor = s.form('Session settings')
    s.capture('session-settings.png')
    s.key(editor, 'Escape')
    dismiss_panel(s)
    assert s.clients()[0] == [], s.clients()[0]
    ordinary = s.create('Session cooperative before refusal')
    refusing = s.create('Session non-delete blocker', deletable=False)
    identifier = s.operation('logout')
    s.wait_operation(identifier, 'failed')
    s.pump()
    assert ordinary['messages'] == 0 and refusing['messages'] == 0
    assert s.process.poll() is None
    s.evidence['cases']['unsupported-before-any-delete'] = {'status': 'passed',
        'ordinary_messages': 0, 'non_delete_messages': 0}
    s.destroy(refusing)
    s.destroy(ordinary)
    waiting = s.create('Session cooperative waiting')
    identifier = s.operation('logout')
    wait_for_predicate('one ordinary WM_DELETE', lambda: s.pump() and waiting['messages'] == 1,
        time.monotonic() + 10, process=s.process)
    wait_for_predicate('waiting status before native Cancel',
        lambda: s.status().get('logout', {}).get('state') == 'waiting',
        time.monotonic() + 5, process=s.process)
    panel = s.form('Session and services')
    s.capture('session-waiting.png')
    s.key(panel, 'F4')
    s.wait_operation(identifier, 'cancelled')
    s.control.echo('desktop-usable-after-cancel')
    s.pump()
    assert waiting['live'] and waiting['messages'] == 1
    s.evidence['cases']['native-cancel-waiting'] = {'status': 'passed',
        'delete_messages': waiting['messages'], 'application_alive': True}
    s.destroy(waiting)
    dismiss_panel(s)
    if s.options.deadline and s.profile == s.options.profiles[0]:
        deadline_probe = s.create('Session thirty-second cooperative deadline')
        identifier = s.operation('logout')
        started = time.monotonic()
        wait_for_predicate('deadline close request',
            lambda: s.pump() and deadline_probe['messages'] == 1,
            time.monotonic() + 8, process=s.process)
        s.wait_operation(identifier, 'failed', timeout=35)
        elapsed = time.monotonic() - started
        s.control.echo('desktop-usable-after-logout-deadline')
        assert 28 <= elapsed <= 38 and deadline_probe['messages'] == 1
        s.evidence['cases']['thirty-second-deadline'] = {'status': 'passed',
            'seconds': elapsed, 'delete_messages': 1, 'desktop_alive': True}
        s.destroy(deadline_probe)
        dismiss_panel(s)
    else:
        s.evidence['cases']['thirty-second-deadline'] = {'status': 'skipped',
            'reason': 'select --deadline for one real30-second timeout in the first profile'}
    blocker = s.create('Session original waiting for new client')
    identifier = s.operation('logout')
    wait_for_predicate('initial close before new allocation',
        lambda: s.pump() and blocker['messages'] == 1,
        time.monotonic() + 10, process=s.process)
    newcomer = s.create('Session newly mapped blocker', close=True)
    s.wait_operation(identifier, 'failed')
    s.pump()
    assert newcomer['messages'] == 0 and newcomer['live'] and s.process.poll() is None
    s.evidence['cases']['new-allocation-blocks-final-quit'] = {'status': 'passed',
        'new_application_messages': 0}
    s.destroy(blocker)
    s.destroy(newcomer)
    dismiss_panel(s)
    # Independently freeze an empty snapshot, map a new real client, and prove
    # the core's final atomic guard refuses that stale generation without Quit.
    empty, generation = s.clients()
    assert not empty, empty
    stale_blocker = s.create('Session stale empty-check blocker')
    guarded = s.request('action', resource='session-clients',
        operation='quit-if-empty', generation=generation)
    assert guarded['state'] in ['stale', 'refused'], guarded
    assert s.process.poll() is None
    s.evidence['cases']['core-stale-final-empty-check'] = guarded
    s.destroy(stale_blocker)
    # End each profile with a cooperative client that actually processes and
    # accepts Delete, plus observed wrapper process completion. EOF alone is
    # deliberately never the success witness.
    closing = s.create('Session closes cooperatively', close=True)
    identifier = s.operation('logout')
    wait_for_predicate('cooperative client accepted WM_DELETE',
        lambda: s.pump() and not closing['live'],
        time.monotonic() + 10)
    wait_for_predicate('owned wrapper normal exit', lambda: s.process.poll() is not None,
        time.monotonic() + 10)
    assert closing['messages'] == 1 and s.process.poll() == 0
    s.evidence['cases']['cooperative-session-exit'] = {'status': 'passed',
        'delete_messages': 1, 'client_destroyed_by_itself': True,
        'wrapper_exit_status': s.process.poll(), 'operation': identifier,
        'witness': 'actual WM_DELETE + client destroy + owned wrapper wait status'}


def optional_bus_cases(options, runtime, out):
    evidence = {'status': 'skipped', 'reason': 'optional bus fixture not requested'}
    if not options.optional_bus:
        return evidence
    daemon, server = shutil.which('dbus-daemon'), shutil.which('dunst')
    if not daemon or not server:
        return {'status': 'skipped', 'reason': 'installed dbus-daemon and dunst are required'}
    results = {}
    for mode in ['reuse', 'start']:
        destination = out / ('optional-' + mode)
        destination.mkdir()
        directory = runtime.root / ('bus-' + mode)
        directory.mkdir(mode=0o700)
        address, borrowed = None, None
        if mode == 'reuse':
            address = 'unix:path=' + str(directory / 'bus')
            borrowed = runtime.start('borrowed-bus', [daemon, '--session', '--nofork',
                '--address=' + address], 'borrowed-bus.log', runtime.environment())
            wait_for_predicate('private inherited bus socket', (directory / 'bus').exists,
                time.monotonic() + 5, process=borrowed)
        session = Session(runtime, options, destination, 'optional-' + mode,
                          bus=mode, address=address)
        try:
            state = session.status()
            bus_item = next(item for item in state['items'] if item['id'] == 'bus')
            # Missing or disabled compiled helper is a qualification skip,
            # never silently substituted with dbus-send or another adapter.
            if bus_item['state'] != 'ready':
                results[mode] = {'status': 'skipped', 'reason': bus_item['detail'],
                    'scope': 'wrapper optional adapter was not ready'}
                session.control.send(b'Quit\n')
                wait_for_predicate('optional fallback normal exit',
                    lambda: session.process.poll() is not None, time.monotonic() + 8)
                continue
            assert bus_item['ownership'] == ('borrowed' if mode == 'reuse' else 'owned')
            wait_for_predicate('actual child bus environment', session.environment_marker.exists,
                time.monotonic() + 8, process=session.process)
            inherited = json.loads(session.environment_marker.read_text())['bus']
            assert inherited and inherited.startswith('unix:')
            env = dict(session.env, DBUS_SESSION_BUS_ADDRESS=inherited)
            notification_server = runtime.start('existing-notification-' + mode,
                [server], 'notification-' + mode + '.log', env)
            def ready():
                identifier = session.operation('session-reprobe')
                session.wait_operation(identifier, 'completed')
                return next(item for item in session.status()['items']
                            if item['id'] == 'notifications')['state'] == 'ready'
            wait_for_predicate('existing notification server handshake', ready,
                time.monotonic() + 10, process=notification_server)
            identifier = session.operation('notification-test')
            sent = session.wait_operation(identifier, 'completed')
            assert sent['result']['outcome'] == 'sent', sent
            identifier = session.operation('notification-close')
            closed = session.wait_operation(identifier, 'completed')
            assert closed['result']['outcome'] == 'closed', closed
            session.change({'notification_dnd': True})
            identifier = session.operation('notification-test')
            suppressed = session.wait_operation(identifier, 'completed')
            assert suppressed['result']['outcome'] == 'suppressed', suppressed
            notification_identity = next(item for item in session.status()['items']
                if item['id'] == 'notifications')['operation']
            notification_server.process.terminate()
            notification_server.process.wait(timeout=3)
            identifier = session.operation('session-reprobe')
            session.wait_operation(identifier, 'completed')
            absent = next(item for item in session.status()['items']
                if item['id'] == 'notifications')
            assert absent['state'] == 'unavailable', absent
            notification_server = runtime.start('replacement-notification-' + mode,
                [server], 'replacement-notification-' + mode + '.log', env)
            wait_for_predicate('replacement notification server handshake', ready,
                time.monotonic() + 10, process=notification_server)
            replacement = next(item for item in session.status()['items']
                if item['id'] == 'notifications')
            assert replacement['operation'] != notification_identity, replacement
            # Restart just the public consumer. The private owner, bus and
            # capability survive; the new provider must obtain fresh controls.
            old_provider = session.provider
            old_generation = session.status()['session_generation']
            session.control.send(b'KillModule FvwmDesktop\nModule FvwmDesktop\n')
            def new_desktop():
                observed = session.ready()
                return observed if observed and observed['provider'] != old_provider else None
            wait_for_predicate('new desktop provider', new_desktop,
                time.monotonic() + 10, process=session.process)
            session.provider = session.settings['provider']
            wait_for_predicate('reconnected private session owner', session.owner_ready,
                time.monotonic() + 10, process=session.process)
            assert session.status()['session_generation'] == old_generation
            session.control.send(b'Quit\n')
            wait_for_predicate('optional session normal completion',
                lambda: session.process.poll() is not None, time.monotonic() + 8)
            assert session.process.poll() == 0
            if borrowed:
                assert borrowed.poll() is None, 'session stopped a borrowed bus'
            results[mode] = {'status': 'passed', 'bus': bus_item,
                'notify': sent, 'close': closed, 'dnd': suppressed,
                'borrowed_bus_survives_session': borrowed.poll() is None if borrowed else None,
                'provider_absent_after_stop': absent,
                'replacement_provider': replacement,
                'desktop_reconnect': {'old_provider': old_provider,
                    'new_provider': session.provider, 'parent_generation': old_generation}}
            if mode == 'start':
                probe = subprocess.run([str(options.build / 'bin/fvwm-session-bus'), 'probe'],
                    env=env, capture_output=True, text=True, timeout=3)
                assert probe.returncode == 0, probe.stderr
                stopped = json.loads(probe.stdout)
                assert stopped['state'] in ['failed', 'unavailable'], stopped
                results[mode]['owned_bus_stopped_after_session'] = stopped
        finally:
            session.close()
        if mode == 'reuse' and results[mode]['status'] == 'passed':
            lost_out = out / 'optional-lost-bus'
            lost_out.mkdir()
            lost = Session(runtime, options, lost_out, 'optional-lost-bus',
                           bus='reuse', address=address)
            try:
                borrowed.process.terminate()
                borrowed.process.wait(timeout=3)
                identifier = lost.operation('session-reprobe')
                lost.wait_operation(identifier, 'failed')
                failed_bus = next(item for item in lost.status()['items']
                    if item['id'] == 'bus')
                assert failed_bus['state'] == 'failed', failed_bus
                lost.control.echo('desktop-usable-after-private-borrowed-bus-loss')
                lost.control.send(b'Quit\n')
                wait_for_predicate('lost-bus session normal completion',
                    lambda: lost.process.poll() is not None, time.monotonic() + 8)
                assert lost.process.poll() == 0
                results['borrowed-loss'] = {'status': 'passed', 'bus': failed_bus,
                    'desktop_alive_after_loss': True, 'wrapper_exit_status': 0}
            finally:
                lost.close()
    return {'status': 'passed' if all(item['status'] == 'passed' for item in results.values())
            else 'partial', 'cases': results}


def run(options):
    out = options.artifacts_dir.resolve()
    out.mkdir(parents=True, exist_ok=False)
    evidence = {'status': 'running', 'profiles': {}, 'optional_bus': {'status': 'not-run'}}
    paths = [options.session, options.fvwm, options.mfl, options.desktop,
             options.form, options.capture, Path(__file__).resolve()]
    evidence['inputs'] = {str(path): hashlib.sha256(path.read_bytes()).hexdigest() for path in paths}
    try:
        # Short private runtime roots also keep the parent's Unix control socket
        # inside sockaddr_un bounds independently of long evidence/build paths.
        with HarnessRuntime(prefix='fvwm-svc-') as runtime:
            _, options.display = start_display_server(runtime, 'Xvfb',
                ['Xvfb', '-screen', '0', '1440x1000x24', '-noreset'],
                'xserver.log', time.monotonic() + 10)
            for profile in options.profiles:
                destination = out / profile
                destination.mkdir()
                session = None
                try:
                    session = Session(runtime, options, destination, profile)
                    focused_cases(session)
                    session.evidence['status'] = 'passed'
                    evidence['profiles'][profile] = {'status': 'passed'}
                except BaseException as error:
                    failure = {'status': 'failed', 'type': type(error).__name__,
                        'message': str(error)}
                    evidence['profiles'][profile] = failure
                    if session:
                        session.evidence.update(failure)
                    raise
                finally:
                    if session:
                        session.close()
                    for log in runtime.root.glob('*.log'):
                        shutil.copyfile(log, destination / log.name)
            evidence['optional_bus'] = optional_bus_cases(options, runtime, out)
            evidence['status'] = ('partial' if options.optional_bus and
                evidence['optional_bus']['status'] != 'passed' else 'passed')
    except BaseException as error:
        evidence['status'] = 'failed'
        evidence['failure'] = {'type': type(error).__name__, 'message': str(error)}
        raise
    finally:
        (out / 'evidence.json').write_text(json.dumps(evidence, indent=2) + '\n')


def arguments():
    parser = argparse.ArgumentParser(description=__doc__, allow_abbrev=False)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--artifacts-dir', type=Path, required=True)
    for name in ['session', 'fvwm', 'mfl', 'desktop', 'form', 'capture', 'data-root', 'modules-dir']:
        parser.add_argument('--' + name, type=Path)
    parser.add_argument('--profiles', nargs='+', choices=PROFILES, default=PROFILES)
    parser.add_argument('--deadline', action='store_true',
        help='Run one real30-second cooperative deadline in the first profile')
    parser.add_argument('--optional-bus', action='store_true',
        help='Exercise installed optional adapter with real dbus-daemon/dunst when available')
    parser.add_argument('--optional-only', action='store_true',
        help='Run only requested optional bus cases, reusing prior UI evidence')
    options = parser.parse_args()
    if options.optional_only:
        options.optional_bus = True
        options.profiles = []
    options.build = options.build.resolve()
    source = Path(__file__).resolve().parents[2]
    defaults = {'session': options.build / 'bin/FvwmPrompt/fvwm-session',
        'fvwm': options.build / 'fvwm3',
        'mfl': options.build / 'modules/FvwmMFL/FvwmMFL',
        'desktop': options.build / 'bin/FvwmPrompt/FvwmDesktop',
        'form': options.build / 'modules/FvwmForm/FvwmForm',
        'capture': options.build / 'bin/fvwm-desktop-capture',
        'data_root': source / 'default-config'}
    for name, default in defaults.items():
        setattr(options, name, (getattr(options, name) or default).resolve())
    if options.modules_dir:
        options.modules_dir = options.modules_dir.resolve()
    return options


if __name__ == '__main__':
    run(arguments())
