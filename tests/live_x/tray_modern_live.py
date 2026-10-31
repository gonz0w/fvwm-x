#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the GNU General Public License, version 2 or any later version.
"""Independent private-bus/actual-X StatusNotifier and native-container checks.

Synthetic protocol peers do not establish real-toolkit or full release coverage.
Use one coherent candidate on a caller-owned disposable display. Original failed
assertions stay failed; no alternate activation can turn them into a pass.
"""
import argparse
import ctypes as C
import hashlib
import json
import os
from pathlib import Path
import select
import signal
import struct
import subprocess
import time
import traceback
import zlib

from live_x_harness import (HarnessRuntime, start_display_server,
    start_xorg_dummy, wait_for_randr, run_required_xrandr,
    wait_for_predicate)
from mfl_stream_live import Connection
from tray_bar_live import Bar, wait
from tray_live import digest, wait_state, released

SERVICE_FIELDS = ('compiled', 'requested', 'connected', 'modern_count',
    'helper_available', 'helper_busy', 'container_capable', 'attached',
    'slot', 'offer', 'menu_open', 'focus_pending', 'focus_id')


class BusPeer:
    def __init__(self, executable, address, environment, out, name):
        self.log = (out / (name + '.stderr')).open('wb')
        self.process = subprocess.Popen([str(executable), '--private-bus', address],
            stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=self.log,
            env=environment, start_new_session=True)
        self.pending = b''
        self.records, self.events = [], []
        self.unique = self.request('hello')['unique']

    def request(self, verb, **fields):
        request = dict(verb=verb, **fields)
        wire = (json.dumps(request) + '\n').encode()
        assert len(wire) <= 16384 and len(self.records) < 2048
        self.process.stdin.write(wire)
        self.process.stdin.flush()
        deadline = time.monotonic() + 4
        while b'\n' not in self.pending:
            assert self.process.poll() is None, self.process.returncode
            ready = select.select([self.process.stdout], [], [],
                max(0, deadline - time.monotonic()))[0]
            assert ready, 'private SNI peer response deadline'
            data = os.read(self.process.stdout.fileno(), 65536)
            assert data, 'private SNI peer disconnected'
            self.pending += data
            assert len(self.pending) <= 262144
        line, self.pending = self.pending.split(b'\n', 1)
        response = json.loads(line)
        self.records.append(dict(request=request, response=response))
        return response

    def observed(self, kind, path=None, identifier=None):
        self.events.extend(self.request('signals')['events'])
        assert len(self.events) <= 4096
        for index, event in enumerate(self.events):
            if event['event'] == kind and (path is None or
                    event['path'] == path) and (identifier is None or
                    event['id'] == identifier):
                return self.events.pop(index)
        return None

    def close(self):
        forced = False
        if self.process.poll() is None:
            self.process.stdin.close()
            try:
                self.process.wait(timeout=3)
            except subprocess.TimeoutExpired:
                forced = True
                self.process.kill()
                self.process.wait(timeout=2)
        self.process.stdout.close()
        self.log.close()
        return dict(exit=self.process.returncode, forced_cleanup=forced)


class ToolkitPeer(BusPeer):
    def __init__(self, source, mode, icon, environment, out):
        self.log = (out / ('toolkit-' + mode + '.stderr')).open('wb')
        self.process = subprocess.Popen(['/usr/bin/python3', str(source),
            '--mode', mode, '--icon', str(icon)], stdin=subprocess.PIPE,
            stdout=subprocess.PIPE, stderr=self.log, env=environment,
            start_new_session=True)
        self.pending = b''
        self.records, self.events = [], []
        self.identity = self.request('hello')

    def observed(self, kind, label=None):
        self.events.extend(self.request('events')['events'])
        for index, event in enumerate(self.events):
            if event['event'] == kind and (label is None or event['label'] == label):
                return self.events.pop(index)
        return None


class Modern(Bar):
    def show_centered(self):
        self.tray('Hide')
        self.fence()
        self.tray('ToggleAt 300 250 40 24')
        self.fence()

    def service(self):
        endpoint = self.home / ('fvwm_mfl_' +
            self.display.replace(':', '_') + '.sock')
        connection = Connection(endpoint)
        previous = connection.socket.gettimeout()
        try:
            connection.send(b'set echo\n')
            connection.echo('tray-service-observer')
            connection.send(b'SendToModule SlateTray ServiceStatus\n')
            deadline = time.monotonic() + 4
            for _ in range(128):
                connection.socket.settimeout(max(.001,
                    deadline - time.monotonic()))
                record = connection.record()
                message = record.get('echo', {}).get('message', '')
                if message.startswith('FVWM_TRAY_SERVICE_STATUS '):
                    values = list(map(int, message.split()[1:]))
                    assert len(values) == len(SERVICE_FIELDS), message
                    return dict(zip(SERVICE_FIELDS, values))
            raise AssertionError('service Status record budget')
        finally:
            connection.socket.settimeout(previous)
            connection.close()

    def keyboard(self, alias):
        endpoint = self.home / ('fvwm_mfl_' +
            self.display.replace(':', '_') + '.sock')
        connection = Connection(endpoint)
        try:
            connection.send(b'set echo\n')
            connection.echo('buttons-keyboard-observer')
            connection.send(('SendToModule %s KeyboardStatus\n' % alias).encode())
            deadline = time.monotonic() + 4
            for _ in range(128):
                connection.socket.settimeout(max(.001, deadline - time.monotonic()))
                record = connection.record()
                message = record.get('echo', {}).get('message', '')
                if message.startswith('FVWM_BUTTONS_KEYBOARD_STATUS '):
                    values = list(map(int, message.split()[1:]))
                    assert len(values) == 9, message
                    return dict(zip(('window', 'enabled', 'focused', 'selected',
                        'pressed', 'keycode', 'external', 'leave_requests',
                        'effective'), values))
            raise AssertionError('keyboard Status record budget')
        finally:
            connection.close()

    def buttons_focused(self, alias):
        state = self.keyboard(alias)
        if not state['focused'] or self.focus() != state['effective']:
            return None
        if state['effective'] != state['window']:
            assert self.client.parent(state['effective']) == state['window']
            attributes = self.attrs(state['effective'])
            assert attributes and attributes.window_class == 2
            assert attributes.map_state == 2
        return state

    def cells(self):
        result = []
        for window in self.children(self.state['host']):
            values = self.property(window, '_FVWM_TRAY_SNI_ITEM', 'CARDINAL')
            if not values:
                continue
            assert len(values) == 5 and values[0] == 1, values
            result.append(dict(window=window, id=values[1] | values[2] << 32,
                generation=values[3] | values[4] << 32,
                parent=self.client.parent(window), actual=self.rect(window)))
        return result

    def cell(self, title):
        # Retained membership is independent of page/host visibility. Callers
        # that require a painted target must also check its actual rectangle.
        for cell in self.cells():
            name = C.c_void_p()
            try:
                if self.errors.read(cell['window'], 20, lambda:
                        self.x.XFetchName(self.d, cell['window'], C.byref(name))) and name.value:
                    if C.string_at(name).decode('utf-8', 'replace') == title:
                        return cell
            finally:
                if name.value:
                    self.x.XFree(name)
        return None

    def menu_view(self, parent=0, rows=None):
        window = self.named('Tray application menu')
        if not window:
            return None
        view = self.property(window, '_FVWM_TRAY_MENU_VIEW', 'CARDINAL')
        if len(view) != 7 or view[0] != 1 or view[3] != parent or not view[6]:
            return None
        if rows is not None and view[4] != rows:
            return None
        return dict(window=window, generation=view[1] | view[2] << 32,
            parent=view[3], rows=view[4], selected=view[5], actual=self.rect(window))

    def click(self, window, button=1):
        rect = wait('actual target viewable rectangle', lambda:
            self.rect(window), self)
        x, y = rect['x'] + rect['width'] // 2, rect['y'] + rect['height'] // 2
        self.pointer.motion(x, y)
        for pressed in (1, 0):
            assert self.pointer.x.XTestFakeButtonEvent(self.d, button, pressed, 0)
        self.x.XSync(self.d, 0)
        return dict(x=x, y=y, button=button, actual=rect)

    def sample_pixels(self, window, points=None):
        attributes = self.attrs(window)
        assert attributes and attributes.map_state == 2
        self.x.XGetImage.argtypes = [C.c_void_p, C.c_ulong, C.c_int, C.c_int,
            C.c_uint, C.c_uint, C.c_ulong, C.c_int]
        self.x.XGetImage.restype = C.c_void_p
        self.x.XGetPixel.argtypes = [C.c_void_p, C.c_int, C.c_int]
        self.x.XGetPixel.restype = C.c_ulong
        self.x.XDestroyImage.argtypes = [C.c_void_p]
        self.x.XDestroyImage.restype = C.c_int
        image = self.x.XGetImage(self.d, window, 0, 0, attributes.width,
            attributes.height, C.c_ulong(-1), 2)
        assert image
        try:
            if points is None:
                points = [(attributes.width * x // 4,
                    attributes.height * y // 4)
                    for y in (1, 2, 3) for x in (1, 2, 3)]
            return [int(self.x.XGetPixel(image, x, y)) for x, y in points]
        finally:
            self.x.XDestroyImage(image)

    def snapshot(self):
        return dict(native=self.fence(), service=self.service(),
            cells=self.cells(), host=self.rect(self.state['host']),
            host_parent=self.client.parent(self.state['host']), focus=self.focus())


def png(path, rgba):
    def chunk(kind, payload):
        return (struct.pack('>I', len(payload)) + kind + payload +
            struct.pack('>I', zlib.crc32(kind + payload) & 0xffffffff))
    data = b'\x89PNG\r\n\x1a\n'
    data += chunk(b'IHDR', struct.pack('>IIBBBBB', 2, 2, 8, 6, 0, 0, 0))
    data += chunk(b'IDAT', zlib.compress(b'\0' + bytes(rgba * 2) +
        b'\0' + bytes(rgba * 2)))
    data += chunk(b'IEND', b'')
    path.write_bytes(data)


def install_icon_fixture(session):
    root = session.home / 'data/icons/hicolor'
    directory = root / '24x24/apps'
    directory.mkdir(parents=True, exist_ok=True)
    (root / 'index.theme').write_text('[Icon Theme]\nName=Fixture\n'
        'Directories=24x24/apps\n[24x24/apps]\nSize=24\nType=Fixed\n'
        'Context=Applications\n')
    path = directory / 'tray-fixture.png'
    png(path, [10, 220, 70, 255])
    xpm = directory / 'tray-fixture-xpm.xpm'
    xpm.write_text('/* XPM */\nstatic char *fixture[] = {\n'
        '"3 3 1 1", "b c #1450dc", "bbb", "bbb", "bbb"};\n')
    return dict(root=str(root), file=str(path), sha256=digest(path),
        xpm=str(xpm), xpm_sha256=digest(xpm))


def cost_observation(session, evidence, label):
    if not session.core_handle:
        evidence.setdefault('unavailable', []).append(
            label + ': authenticated Linux process cost observation unavailable')
        return
    handle = session.process_handle(session.options.tray, 'SlateTray')
    def sample(owned, executable):
        identity = owned.sample()
        pid = identity['pid']
        root = Path('/proc/%d' % pid)
        fields = (root / 'stat').read_text().rsplit(') ', 1)[1].split()
        status = {}
        for line in (root / 'status').read_text().splitlines():
            key, _, value = line.partition(':')
            if key in ('VmRSS', 'VmHWM', 'VmSize', 'Threads'):
                status[key] = value.strip()
        actual, expected = (root / 'exe').stat(), executable.stat()
        assert (actual.st_dev, actual.st_ino) == (expected.st_dev, expected.st_ino)
        owned.sample()
        return dict(identity=identity, status=status, user_ticks=int(fields[11]),
            system_ticks=int(fields[12]), descriptors=len(list((root / 'fd').iterdir())),
            executable_bytes=executable.stat().st_size)
    before = dict(tray=sample(handle, session.options.tray),
        core=sample(session.core_handle, session.options.fvwm))
    begin = time.monotonic()
    # Deliberately no bus/X/status polling in this idle observation interval.
    time.sleep(1)
    after = dict(tray=sample(handle, session.options.tray),
        core=sample(session.core_handle, session.options.fvwm))
    evidence.setdefault('cost_observations', {})[label] = dict(before=before,
        after=after, interval_seconds=time.monotonic() - begin,
        clock_ticks=os.sysconf('SC_CLK_TCK'),
        limits='Sequential proc samples; no X-server/GPU or helper peak RSS bound')


def protocol_cases(session, evidence, watcher, peers):
    peer = peers[0]
    seed = peers[2]
    for index in range(10):
        assert seed.request('add', path='/Seed%d' % index,
            title='Seed %d' % index)['ok']
    registry = watcher.request('registry')
    assert len(registry['items']) == 10, registry
    began = time.monotonic()
    session.change({'tray_enabled': True, 'tray_modern': False})
    session.observe_owner()
    evidence['modern_off_first_ready_seconds'] = time.monotonic() - began
    cost_observation(session, evidence, 'modern-off-empty')
    began = time.monotonic()
    session.change({'tray_enabled': True, 'tray_modern': True,
        'tray_icon_size': 24})
    session.observe_owner()
    service = wait('requested modern connection', lambda:
        (lambda state: state if state['connected'] else None)(session.service()),
        session)
    assert service['compiled'] and service['requested'], service
    evidence['modern_enable_connected_seconds'] = time.monotonic() - began
    wait('existing watcher receives host registration', lambda:
        watcher.observed('HostRegistered'), session)
    evidence['cases']['incumbent_watcher_host_registration'] = dict(
        watcher_unique=watcher.unique, registry=watcher.request('registry'),
        observed=service)
    seeded = wait('registry seed continues beyond eight pending calls', lambda:
        session.cells() if len(session.cells()) == 10 else None, session)
    seed_exit = seed.close()
    assert seed_exit == dict(exit=0, forced_cleanup=False), seed_exit
    wait('seed producer owner loss retires all ten paths', lambda:
        session.service()['modern_count'] == 0 and not session.cells(), session)
    evidence['cases']['registry_seed_continuation_and_owner_loss'] = dict(
        registry=registry, cells=seeded, producer_exit=seed_exit)
    assert peer.request('add', path='/ItemA', title='Tray KDE alpha')['ok']
    assert peer.request('add', path='/ItemB', title='Tray published beta',
        dialect='freedesktop')['ok']
    for invalid in (':.1/Bad', ':1./Bad', ':1..2/Bad'):
        assert watcher.request('emit', entry=invalid)['ok']
    session.fence()
    assert session.service()['connected']
    cells = wait('two actual modern cell windows', lambda:
        session.cells() if len(session.cells()) == 2 else None, session)
    assert len({cell['id'] for cell in cells}) == 2
    session.tray('Show')
    session.fence(lambda state: not state['pending'])
    first = wait('actual KDE title cell', lambda:
        (lambda cell: cell if cell and cell['actual'] else None)(
            session.cell('Tray KDE alpha')), session)
    second = wait('actual published title cell', lambda:
        (lambda cell: cell if cell and cell['actual'] else None)(
            session.cell('Tray published beta')), session)
    assert all(cell['parent'] == session.state['host'] for cell in cells)
    session.show_centered()
    session.focus_app()
    focus = session.focus()
    click = session.click(first['window'])
    action = wait('actual primary activation', lambda:
        peer.observed('Activate', '/ItemA'), session)
    assert (action['x'], action['y']) == (click['x'], click['y']), action
    assert session.focus() == focus
    session.click(second['window'], 2)
    wait('actual secondary activation', lambda:
        peer.observed('SecondaryActivate', '/ItemB'), session)
    session.click(first['window'], 4)
    scroll = wait('actual scroll method', lambda:
        peer.observed('Scroll', '/ItemA'), session)
    assert scroll['x'] and scroll['y'] == 1, scroll
    evidence['cases']['both_dialects_multiple_paths_and_pointer_actions'] = dict(
        cells=cells, primary=action, scroll=scroll, focus_preserved=True)
    session.pointer.motion(click['x'], click['y'])
    tooltip = wait('actual plain tooltip', lambda:
        session.named('Tray tooltip'), session)
    assert session.focus() == focus
    session.capture('modern-tooltip.png')
    session.pointer.motion(20, 300)
    wait('tooltip retires on pointer leave', lambda:
        session.named('Tray tooltip') is None, session)
    evidence['cases']['bounded_plain_tooltip_no_focus'] = dict(window=tooltip)
    return first


def menu_cases(session, evidence, peer, cell):
    session.show_centered()
    session.click(cell['window'], 3)
    menu = wait('actual application menu', lambda:
        (lambda view: view['window'] if view else None)(session.menu_view()), session)
    wait('real AboutToShow call', lambda:
        peer.observed('AboutToShow', '/ItemAMenu'), session)
    session.capture('modern-application-menu.png')
    session.press('Return')
    selected = wait('actual menu click ID10', lambda:
        peer.observed('MenuEvent', '/ItemAMenu', 10), session)
    wait('menu closed after activation', lambda:
        session.named('Tray application menu') is None, session)
    session.click(cell['window'], 3)
    wait('menu reopened for toggle', lambda:
        (lambda view: view['window'] if view else None)(session.menu_view()), session)
    session.press('Down')
    session.press('Return')
    wait('actual toggle menu ID11', lambda:
        peer.observed('MenuEvent', '/ItemAMenu', 11), session)
    previous = session.cell('Tray KDE alpha')['generation']
    assert peer.request('update', path='/ItemA', title='Tray KDE alpha',
        checked=True)['ok']
    wait('checked item refresh generation committed', lambda:
        session.cell('Tray KDE alpha')['generation'] != previous, session)
    session.click(cell['window'], 3)
    wait('menu reopened for nested action', lambda:
        (lambda view: view['window'] if view else None)(session.menu_view()), session)
    session.press('Down')
    session.press('Down')
    session.press('Right')
    wait('actual painted nested menu', lambda: session.menu_view(12, 1), session)
    session.press('Return')
    nested = wait('actual nested menu ID13', lambda:
        peer.observed('MenuEvent', '/ItemAMenu', 13), session)
    session.click(cell['window'], 3)
    menu = wait('menu reopened for pointer action', lambda:
        (lambda view: view['window'] if view else None)(session.menu_view()), session)
    rect = session.rect(menu)
    row = max(24, session.fence()['font_height'])
    session.pointer.motion(rect['x'] + rect['width'] // 2,
        rect['y'] + 4 + row // 2)
    session.pointer.button(True)
    session.pointer.button(False)
    wait('pointer menu click ID10', lambda:
        peer.observed('MenuEvent', '/ItemAMenu', 10), session)
    session.click(cell['window'], 3)
    wait('menu opened before layout mutation', lambda:
        (lambda view: view['window'] if view else None)(session.menu_view()), session)
    assert peer.request('update', path='/ItemA', title='Tray KDE alpha')['ok']
    # A changed tree cannot dispatch a previously stale numeric action. The
    # renderer may close or refresh it; both require a new current layout.
    session.press('Escape')
    wait('menu closes after escape/mutation', lambda:
        session.named('Tray application menu') is None, session)
    evidence['cases']['menu_nested_toggle_pointer_update_and_escape'] = dict(
        first=selected, nested=nested, observed_menu=rect)

    # A second item without an exported menu must dispatch ContextMenu and
    # retire the old native tree instead of presenting it as the second item.
    assert peer.request('update', path='/ItemA', title='Tray fallback item',
        no_menu=True)['ok']
    fallback = wait('fallback item refreshed', lambda:
        session.cell('Tray fallback item'), session)
    other = session.cell('Tray published beta')
    session.click(other['window'], 3)
    wait('other native menu before fallback', lambda: session.menu_view(), session)
    click = session.click(fallback['window'], 3)
    event = wait('external ContextMenu submitted once', lambda:
        peer.observed('ContextMenu', '/ItemA'), session)
    assert (event['x'], event['y']) == (click['x'], click['y'])
    wait('fallback retires native menu/input scope', lambda:
        not session.service()['menu_open'] and
            session.named('Tray application menu') is None, session)
    session.press('Escape')
    wait('fallback restores managed application focus', lambda:
        session.focus() == session.app, session)
    assert peer.request('update', path='/ItemA', title='Tray KDE alpha')['ok']
    session.fence()
    evidence['cases']['external_context_menu_retires_prior_native_tree'] = event


def lifecycle_cases(session, evidence, peers):
    peer = peers[0]
    first = session.cell('Tray KDE alpha')
    assert peer.request('update', path='/ItemA', title='Tray KDE alpha',
        passive=True)['ok']
    wait('passive modern item not viewable', lambda:
        (lambda cell: not cell or not cell['actual'])(
            session.cell('Tray KDE alpha')), session)
    assert peer.request('update', path='/ItemA', title='Tray KDE alpha',
        attention=True)['ok']
    active = wait('attention modern item restored', lambda:
        (lambda cell: cell if cell and cell['actual'] else None)(
            session.cell('Tray KDE alpha')), session)
    assert active['id'] == first['id']
    session.tray('Focus')
    wait('modern keyboard proxy focus', lambda:
        session.focus() == session.state['proxy'], session)
    session.press('Return')
    wait('keyboard activation method', lambda:
        peer.observed('Activate'), session)
    session.press('Escape')
    wait('keyboard release to managed application', lambda:
        session.focus() == session.app, session)
    # Original snapshot must survive a refused malformed replacement and
    # recover when a later valid snapshot arrives.
    assert peer.request('update', path='/ItemA', title='Tray KDE alpha',
        invalid=True)['ok']
    session.fence()
    assert peer.request('update', path='/ItemA', title='Tray KDE alpha')['ok']
    wait('valid modern snapshot after malformed pixmap', lambda:
        session.cell('Tray KDE alpha'), session)
    slow = peers[1]
    assert slow.request('add', path='/Slow', title='Slow modern item', hold=True)['ok']
    held_first = wait('specific retained property-call serial', lambda:
        slow.observed('HeldGetAll', '/Slow'), session)
    # Another healthy item remains interactive while one properties call is
    # retained by its producer. Deadlines expire independently.
    session.tray('Show')
    session.fence()
    healthy = wait('healthy actual cell during slow producer', lambda:
        (lambda value: value if value and value['actual'] else None)(
            session.cell('Tray KDE alpha')), session)
    session.click(healthy['window'])
    wait('healthy activation during held properties', lambda:
        peer.observed('Activate', '/ItemA'), session)
    expiry = time.monotonic() + 2.2
    wait('held property request crosses its actual two-second deadline', lambda:
        time.monotonic() >= expiry, session)
    held = slow.request('reply-held', serial=held_first['id'])
    assert held['released'] == 1
    session.fence()
    assert session.cell('Slow modern item') is None, 'expired reply created a cell'
    assert slow.request('update', path='/Slow', title='Recovered slow item')['ok']
    wait('later fresh property update recovers after expired reply', lambda:
        session.cell('Recovered slow item'), session)
    evidence['cases']['passive_attention_keyboard_malformed_and_late_reply'] = dict(
        before=first, attention=active, held_serial=held_first, held=held)


def capacity_cases(session, evidence, watcher, peers):
    slow = peers[2]
    slow.close()
    wait('slow producer release leaves two healthy items', lambda:
        session.service()['modern_count'] == 2, session)
    foreign = session.peer_client.create(width=20, height=20)
    session.peer_client.request('dock', id=foreign)
    wait_state(session, session.peer_client, foreign, lambda value:
        value['exists'] and value['parent'] != session.client.root,
        'foreign capacity witness admitted')
    producer = BusPeer(session.options.sni_client, session.options.session_address,
        session.env, session.out, 'capacity-producer')
    peers.append(producer)
    for index in range(30):
        assert producer.request('add', path='/Capacity%d' % index,
            title='Capacity %d' % index)['ok']
    capped = wait('combined foreign and modern admission reaches32', lambda:
        (lambda value: value if value['modern_count'] == 31 and
            len(session.cells()) == 31 else None)(
            session.service()), session)
    assert len(session.cells()) == 31
    cost_observation(session, evidence, 'modern31-plus-xembed1')
    assert session.peer_client.state(foreign)['exists']
    missing = [index for index in range(30) if
        session.cell('Capacity %d' % index) is None]
    assert len(missing) == 1, missing
    session.peer_client.request('destroy', id=foreign)
    session.fence()
    watcher.request('emit', entry=producer.unique + '/Capacity%d' % missing[0])
    wait('released foreign capacity admits a fresh modern registration', lambda:
        session.service()['modern_count'] == 32 and len(session.cells()) == 32,
        session)
    producer.close()
    wait('capacity producer retirement releases all thirty items', lambda:
        session.service()['modern_count'] == 2, session)
    evidence['cases']['combined_foreign_modern_capacity_refusal_and_recovery'] = dict(
        capped=capped, foreign=foreign, initially_refused=missing)


def own_watcher_cases(session, evidence, watcher, peer, peers):
    alias = 'org.fvwm.TrayFixtureAlias'
    assert peer.request('alias', name=alias)['result'] == 1
    assert peer.request('add', title='Well-known alias item', register=False)['ok']
    assert peer.request('forge', service=alias)['ok']
    wait('well-known alias item visible', lambda:
        session.cell('Well-known alias item'), session)
    assert peer.request('unalias', name=alias)['result'] == 1
    wait('well-known loss retires item while its unique owner stays alive', lambda:
        session.cell('Well-known alias item') is None, session)
    retained = session.cells()
    assert peer.request('hello')['unique'] == peer.unique
    assert watcher.request('unwatch')['ok']
    wait('native watcher takes both released names', lambda:
        all(peer.request('has-owner', name=name)['owned'] for name in
            ('org.kde.StatusNotifierWatcher',
                'org.freedesktop.StatusNotifierWatcher')), session)
    for path in ('/ItemA', '/ItemB', '/StatusNotifierItem'):
        assert peer.request('register', path=path)['ok']
    wait('native watcher takes absent names and retains registered paths', lambda:
        session.service()['connected'] and len(session.cells()) == 3, session)
    attacker = BusPeer(session.options.sni_client, session.options.session_address,
        session.env, session.out, 'native-watcher-registration-observer')
    peers.append(attacker)
    refused = attacker.request('forge', service=peer.unique)
    assert 'error' in refused, refused
    evidence['cases']['alias_reference_and_native_watcher_takeover'] = dict(
        retained=retained, owner=peer.unique, cells=session.cells(),
        refused_foreign_registration=refused)


def theme_cases(session, evidence, peer, all_peers):
    icon = install_icon_fixture(session)
    assert peer.request('update', path='/ItemA', title='Tray named icon',
        named=True, icon='tray-fixture')['ok']
    session.tray('Show')
    wait('named native cell present', lambda:
        session.cell('Tray named icon'), session)
    results = []
    initial_id = session.cell('Tray named icon')['id']
    profiles = [('slate', 'dark', 'xft:Sans:size=11', '#e5edf5', '#253140'),
        ('dune', 'light', 'xft:Serif:size=12', '#322a20', '#eee4cb'),
        ('slate', 'dark', 'xft:DejaVu Sans Mono:size=18', '#ffffe0', '#252550')]
    for index, (palette, appearance, font, foreground, background) in enumerate(profiles):
        session.change({'palette': palette, 'appearance': appearance,
            'tray_icon_size': 24 + index * 8, 'tray_gap': 4 + index,
            'tray_padding': 4 + index})
        session.button_rect()
        session.setting_snapshot()
        session.button_rect()
        overlay = session.out / ('modern-theme-%d.fvwm' % index)
        overlay.write_text('Colorset 55 fg %s, bg %s\n'
            '*SlateTray Colorset 55\n*SlateTray Font "%s"\n'
            '*SlateTray IconTheme hicolor\nSendToModule SlateTray Reload\n' %
            (foreground, background, font))
        session.command('Read "%s"' % overlay)
        session.fence(lambda state: not state['pending'])
        session.tray('Show')
        session.fence()
        cell = wait('themed native modern cell', lambda:
            (lambda value: value if value and value['actual'] else None)(
                session.cell('Tray named icon')), session)
        assert cell['id'] == initial_id, 'theme reload replaced a healthy bus item'
        state = wait('named helper completes bounded batch', lambda:
            (lambda value: value if not value['helper_busy'] else None)(
                session.service()), session)
        assert state['helper_available'], state
        pixels = wait('named icon is actually painted green', lambda:
            (lambda values: values if any(((value >> 8) & 255) >
                ((value >> 16) & 255) + 80 and ((value >> 8) & 255) >
                (value & 255) + 80 for value in values) else None)(
                    session.sample_pixels(cell['window'])), session)
        session.command('SendToModule FvwmDesktop edit-settings group:tray')
        editor = wait('modern tray preferences editor', lambda:
            session.named('Tray settings'), session)
        session.show_centered()
        session.capture('modern-theme-%d.png' % index)
        session.command('WindowId %d Delete' % editor)
        wait('modern preferences editor closed', lambda:
            session.named('Tray settings') is None, session)
        results.append(dict(overlay=overlay.name, sha256=digest(overlay),
            font=font, cell=cell, actual_pixels=pixels,
            native=session.fence(), service=state))
    assert results[2]['native']['font_height'] > results[0]['native']['font_height']
    # This fresh owner has never painted a normal icon. The positive pixels
    # must come from fallback after its missing attention name, not a retained
    # previous-generation image or raw bus pixmap.
    previous_count = session.service()['modern_count']
    fallback_peer = BusPeer(session.options.sni_client,
        session.options.session_address, session.env, session.out,
        'attention-fallback-producer')
    all_peers.append(fallback_peer)
    for path, attention in [('/RawAttentionEmpty', ''),
            ('/RawAttentionMissing', 'tray-fixture-absent-attention')]:
        assert fallback_peer.request('add', path=path, title=path,
            named=True, attention=True, icon='tray-fixture',
            attention_icon=attention)['ok']
    assert fallback_peer.request('add', path='/AttentionFallback',
        title='Tray normal attention fallback', named=True, attention=True,
        icon='tray-fixture', attention_icon='tray-fixture-absent-attention',
        no_pixmaps=True)['ok']
    fallback = wait('fresh attention-only-name native cell', lambda:
        (lambda value: value if value and value['actual'] else None)(
            session.cell('Tray normal attention fallback')), session)
    green = wait('normal named icon painted after missing attention name', lambda:
        (lambda values: values if any(((value >> 8) & 255) >
            ((value >> 16) & 255) + 80 and ((value >> 8) & 255) >
            (value & 255) + 80 for value in values) else None)(
                session.sample_pixels(fallback['window'])), session)
    # Both raw-attention items precede the positive normal fallback in the
    # bounded icon batch. Wait for the completed helper and actual blue paint,
    # so an initial blue image that is later replaced by green cannot pass.
    wait('attention and normal icon batch finishes', lambda:
        not session.service()['helper_busy'], session)
    session.fence()
    attention_pixels = []
    for title in ['/RawAttentionEmpty', '/RawAttentionMissing']:
        cell = wait('raw attention item published', lambda:
            session.cell(title), session)
        blue = wait('attention pixmap precedes normal named icon', lambda:
            (lambda values: values if any((value & 255) >
                ((value >> 8) & 255) + 80 and (value & 255) >
                ((value >> 16) & 255) + 80 for value in values) else None)(
                    session.sample_pixels(cell['window'])), session)
        attention_pixels.append(dict(title=title, cell=cell,
            actual_pixels=blue))
    named_xpm = None
    xpm_enabled = any(line.startswith('#define XPM') for line in
        (session.options.build / 'config.h').read_text().splitlines())
    if xpm_enabled:
        assert fallback_peer.request('add', path='/NamedXpm',
            title='Tray scaled named XPM', named=True, no_pixmaps=True,
            icon='tray-fixture-xpm')['ok']
        xpm_cell = wait('named XPM native cell', lambda:
            session.cell('Tray scaled named XPM'), session)
        pixels = wait('small XPM covers target icon interior', lambda:
            (lambda values: values if all((value & 255) >
                ((value >> 8) & 255) + 80 for value in values) else None)(
                    session.sample_pixels(xpm_cell['window'])), session)
        named_xpm = dict(cell=xpm_cell, actual_pixels=pixels)
    else:
        evidence.setdefault('unavailable', []).append(
            'Named XPM positive scaling: optional XPM decoder disabled')
    # At nine actual pixels the four-pixel overlay must not retain the
    # worker's minimum eight-pixel image and crop over the final base pixel.
    session.change({'tray_icon_size': 9})
    session.show_centered()
    assert fallback_peer.request('add', path='/NamedOverlay',
        title='Tray exact overlay target', named=xpm_enabled,
        no_pixmaps=xpm_enabled,
        icon='tray-fixture-xpm' if xpm_enabled else 'tray-fixture',
        overlay_icon='tray-fixture')['ok']
    overlay_cell = wait('named overlay native cell', lambda:
        session.cell('Tray exact overlay target'), session)
    overlay_geometry = session.rect(overlay_cell['window'])
    assert (overlay_geometry['width'], overlay_geometry['height']) == (24, 24)
    glyph_x = (overlay_geometry['width'] - 9) // 2
    glyph_y = (overlay_geometry['height'] - 9) // 2
    def observe_overlay():
        values = session.sample_pixels(overlay_cell['window'],
            points=[(glyph_x + 6, glyph_y + 6),
                (glyph_x + 8, glyph_y + 8)])
        evidence['overlay_observation'] = dict(pixels=values,
            geometry=session.rect(overlay_cell['window']),
            service=session.service())
        if (((values[0] >> 8) & 255) >
                ((values[0] >> 16) & 255) + 80 and not
                (((values[1] >> 8) & 255) >
                    ((values[1] >> 16) & 255) + 80)):
            return values
        return None
    overlay_pixels = wait('overlay paints its exact half-size target',
        observe_overlay, session)
    overlay_geometry = session.rect(overlay_cell['window'])
    session.change({'tray_icon_size': 40})
    session.show_centered()
    retired = fallback_peer.close()
    assert retired == dict(exit=0, forced_cleanup=False), retired
    wait('temporary attention owner fully retired', lambda:
        session.service()['modern_count'] == previous_count and
            session.cell('Tray normal attention fallback') is None, session)
    evidence['cases']['missing_attention_name_uses_normal_named_icon'] = dict(
        cell=fallback, actual_pixels=green, producer_exit=retired,
        normal='tray-fixture', attention='tray-fixture-absent-attention',
        raw_pixmaps=False, fresh_owner=True,
        attention_pixmap_precedence=attention_pixels, named_xpm=named_xpm,
        overlay_pixels=overlay_pixels, overlay_geometry=overlay_geometry)
    cost_observation(session, evidence, 'modern-two-named-icon-ready')
    evidence['cases']['shared_named_icons_sizing_and_three_themes'] = dict(
        icon=icon, profiles=results)


def toolkit_cases(session, evidence, peers):
    if not session.options.toolkit_env:
        evidence.setdefault('unavailable', []).append(
            'Real toolkit producer bundle not selected for this synthetic gate.')
        return
    receipt = json.loads(session.options.toolkit_env.read_text())
    assert receipt['status'] == 'prepared'
    environment = dict(session.env, **receipt['environment'])
    environment['LD_LIBRARY_PATH'] += ':' + session.env.get('LD_LIBRARY_PATH', '')
    # GTK's PNG loader needs shared MIME data. Only the private toolkit
    # producers inherit these base paths; the desktop and worker stay isolated.
    base_data = ['/usr/local/share', '/usr/share']
    data_dirs = [value for value in environment.get('XDG_DATA_DIRS', '').split(':')
        if value]
    environment['XDG_DATA_DIRS'] = ':'.join(dict.fromkeys(data_dirs + base_data))
    mime_caches = []
    for directory in base_data:
        cache = Path(directory) / 'mime/mime.cache'
        if cache.is_file():
            mime_caches.append(dict(path=str(cache), sha256=digest(cache),
                bytes=cache.stat().st_size))
    source = Path(__file__).with_name('tray_toolkit_client.py')
    icon = session.home / 'toolkit-icon.png'
    blue, orange = [30, 120, 230, 255], [230, 90, 30, 255]
    png(icon, blue)
    diagnostics = dict(stages=[], initial_rgba=blue,
        source_sha256=digest(source), icon_sha256=digest(icon),
        producer_xdg_data_dirs=environment['XDG_DATA_DIRS'],
        base_mime_caches=mime_caches)
    # Populate the shared evidence before any assertion, including on failure.
    evidence['gtk_paint_diagnostics'] = diagnostics
    assert mime_caches, 'real GTK producer needs a base shared MIME cache'
    gtk = ToolkitPeer(source, 'xembed', icon, environment, session.out)
    peers.append(gtk)

    def observe_gtk(stage, expected_rgba=blue, require_paint=True):
        state = gtk.request('state')
        result = dict(stage=stage, toolkit=state, samples=0, changes=[],
            expected_rgba=expected_rgba)
        diagnostics['stages'].append(result)
        window = state['xid']

        def painted():
            rect = session.rect(window) if window else None
            observation = dict(actual=rect)
            if rect:
                point = (rect['width'] // 2, rect['height'] // 2)
                value = session.sample_pixels(window, points=[point])[0]
                observation.update(local_center=list(point), pixel=value,
                    rgb=[(value >> 16) & 255, (value >> 8) & 255, value & 255])
            result['samples'] += 1
            result.setdefault('first', observation)
            previous = result.get('last')
            if observation != previous and len(result['changes']) < 16:
                result['changes'].append(observation)
            result['last'] = observation
            if not rect:
                return None
            matches = all(abs(actual - expected) <= 12 for actual, expected
                in zip(observation['rgb'], expected_rgba[:3]))
            return observation if matches else None

        if require_paint:
            try:
                result['positive_paint'] = wait(
                    'GTK actual client expected-color pixels: ' + stage,
                    painted, session)
                result['status'] = 'passed'
            except Exception as error:
                # Continue only to collect the remaining finite diagnostics.
                # The final paint assertion keeps this stage failed.
                result.update(status='failed', error=repr(error))
        else:
            result.update(status='observation-only', positive_paint=painted())
        result['toolkit_after'] = gtk.request('state')
        pixbuf = result['toolkit_after'].get('image', {}).get('pixbuf', {})
        result['source_loaded'] = (pixbuf.get('width') == 2 and
            pixbuf.get('height') == 2 and
            pixbuf.get('first_rgba') == expected_rgba)
        session.capture('gtk-paint-' + stage + '.png')
        return result

    embedded = wait('actual GTK toolkit XEmbed admission', lambda:
        (lambda value: value if value['embedded'] and value['xid'] else None)(
            gtk.request('state')), session)
    observe_gtk('initial', require_paint=False)
    session.tray('Show')
    session.fence()
    actual = wait('actual GTK plug visible in tray socket', lambda:
        session.rect(embedded['xid']), session)
    parent = session.client.parent(embedded['xid'])
    assert session.client.parent(parent) == session.state['host']
    observe_gtk('host-show')
    assert gtk.request('hide')['visible'] is False
    wait('GTK visibility clears or retires actual plug', lambda:
        (lambda value: not value or value.map_state == 0)(
            session.attrs(embedded['xid'])), session)
    gtk.request('show')
    resumed = wait('GTK toolkit exposes its current embedded plug', lambda:
        (lambda value: value if value['embedded'] and value['xid'] and
            session.rect(value['xid']) else None)(gtk.request('state')), session)
    current_parent = session.client.parent(resumed['xid'])
    assert session.client.parent(current_parent) == session.state['host']
    observe_gtk('toolkit-show')
    click = session.click(resumed['xid'])
    gtk_event = wait('real GTK activation signal', lambda:
        gtk.observed('activate', 'GTK icon'), session)
    session.show_centered()
    observe_gtk('blue-centered-hide-toggle')
    png(icon, orange)
    diagnostics['updated_icon'] = dict(path=str(icon), sha256=digest(icon),
        expected_rgba=orange)
    gtk.request('icon')
    assert session.client.parent(resumed['xid']) == current_parent
    observe_gtk('icon-update', expected_rgba=orange)
    session.show_centered()
    observe_gtk('centered-hide-toggle', expected_rgba=orange)
    diagnostics['indicator_icon'] = dict(path=str(icon),
        sha256=digest(icon), expected_rgba=orange)
    indicator = ToolkitPeer(source, 'indicator', icon, environment, session.out)
    peers.append(indicator)
    native = wait('real Ayatana native SNI cell', lambda:
        (lambda value: value if value and value['actual'] else None)(
            session.cell('Ayatana toolkit indicator')), session)
    connected = wait('real Ayatana connection acknowledgement', lambda:
        (lambda value: value if value.get('connected') else None)(
            indicator.request('state')), session)
    session.show_centered()
    session.click(native['window'], 3)
    wait('real toolkit exported menu opens in native host', lambda:
        (lambda view: view['window'] if view else None)(session.menu_view()), session)
    session.capture('real-gtk-ayatana-clients.png')
    session.press('Return')
    action = wait('actual Ayatana/dbusmenu GTK action callback', lambda:
        indicator.observed('activate', 'Toolkit action'), session)
    indicator.request('hide')
    wait('real passive indicator leaves visible layout', lambda:
        (lambda value: not value or not value['actual'])(
            session.cell('Ayatana toolkit indicator')), session)
    indicator.request('show')
    wait('real indicator returns after state update', lambda:
        (lambda value: value if value and value['actual'] else None)(
            session.cell('Ayatana toolkit indicator')), session)
    required_paint = [stage for stage in diagnostics['stages']
        if stage['status'] != 'observation-only']
    assert required_paint and all(stage['status'] == 'passed' and
        stage['source_loaded'] for stage in required_paint), diagnostics
    evidence['cases']['real_gtk_xembed_and_ayatana_dbusmenu'] = dict(
        bundle_sha256=digest(session.options.toolkit_env),
        source_sha256=digest(source), gtk_identity=gtk.identity,
        embedded=embedded, plug=actual, parent=parent, resumed=resumed,
        resumed_parent=current_parent, click=click,
        gtk_activation=gtk_event, indicator_identity=indicator.identity,
        indicator_connected=connected, actual_sni_cell=native,
        menu_action=action, kind='Real toolkit API producer fixtures; no Qt claim',
        graceful_cleanup=[gtk.close(), indicator.close()])


def container_cases(session, evidence):
    session.tray('Hide')
    session.fence()
    overlay = session.out / 'native-tray-container.fvwm'
    overlay.write_text('DestroyModuleConfig TrayContainer:*\n'
        '*TrayContainer: Geometry 480x64+80+160\n'
        '*TrayContainer: Rows 1\n*TrayContainer: Columns 8\n'
        '*TrayContainer: Colorset 2\n*TrayContainer: Font "xft:Sans:size=11"\n'
        '*TrayContainer: KeyboardNavigation 1\n'
        '*TrayContainer: (1x1, Id before, Title "Before", Action Echo TRAY_BEFORE)\n'
        '*TrayContainer: (6x1, Id native, Title "Tray", Tray)\n'
        '*TrayContainer: (1x1, Id after, Title "After", Action Echo TRAY_AFTER)\n'
        'Style TrayContainer !Title, !Borders, !Handles, UsePPosition\n'
        'Module FvwmButtons TrayContainer\n')
    session.command('Read "%s"' % overlay)
    bar = wait('opt-in native container window', lambda:
        session.named('TrayContainer'), session)
    if session.options.expect_no_container:
        state = session.service()
        assert not state['container_capable'] and not state['attached'], state
        assert session.client.parent(session.state['host']) == session.client.root
        evidence['cases']['missing_xfixes_preserves_root_host'] = state
        session.command('KillModule FvwmButtons TrayContainer')
        return
    state = wait('validated native container attachment', lambda:
        (lambda value: value if value['attached'] else None)(session.service()),
        session)
    assert session.client.parent(session.state['host']) == state['slot']
    session.tray('Show')
    session.fence()
    wait('attached host viewable', lambda: session.rect(session.state['host']), session)
    assert session.client.parent(state['slot']) == bar
    plug = session.peer_client.create(width=24, height=20)
    session.peer_client.request('dock', id=plug)
    wait_state(session, session.peer_client, plug, lambda value:
        value['map'] == 2, 'foreign client viewable inside container')
    before = session.peer_client.state(plug)
    host_before = session.rect(session.state['host'])
    assert host_before['width'] <= session.rect(state['slot'])['width']
    # A second container may remain empty but cannot take over the host.
    second = session.out / 'second-tray-container.fvwm'
    second.write_text(overlay.read_text().replace('TrayContainer', 'OtherTray'))
    session.command('Read "%s"' % second)
    wait('competing container window', lambda: session.named('OtherTray'), session)
    assert session.service()['slot'] == state['slot']
    session.command('KillModule FvwmButtons OtherTray')
    session.command('WindowId %d Resize 620p 80p' % bar)
    wait('actual resized slot allocation', lambda:
        (lambda value: value if value and value['width'] != host_before['width']
            else None)(session.rect(session.state['host'])), session)
    assert session.peer_client.state(plug)['width'] == 24
    trace = evidence['container_keyboard_trace'] = []
    trace.append(session.keyboard('TrayContainer'))
    session.command('WindowId %d Focus' % bar)
    wait('managed container keyboard scope', lambda:
        session.buttons_focused('TrayContainer'), session)
    trace.append(session.keyboard('TrayContainer'))
    # Roving selection persists across focus changes. Establish Before with
    # real pointer/action evidence instead of assuming an initial selection.
    endpoint = session.home / ('fvwm_mfl_' +
        session.display.replace(':', '_') + '.sock')
    observer = Connection(endpoint)
    try:
        observer.send(b'set echo\n')
        observer.echo('container-before-selection-observer')
        rect = session.rect(bar)
        session.pointer.motion(rect['x'] + rect['width'] // 16,
            rect['y'] + rect['height'] // 2)
        session.pointer.button(True)
        session.pointer.button(False)
        deadline = time.monotonic() + 4
        for _ in range(128):
            observer.socket.settimeout(max(.001, deadline - time.monotonic()))
            message = observer.record().get('echo', {}).get('message', '')
            if message == 'TRAY_BEFORE':
                break
        else:
            raise AssertionError('actual Before action not observed')
    finally:
        observer.close()
    selected = session.keyboard('TrayContainer')
    trace.append(selected)
    assert selected['selected'] == 1, selected
    session.press('Tab')
    trace.append(session.keyboard('TrayContainer'))
    session.press('Return')
    trace.append(session.keyboard('TrayContainer'))
    wait('container delegates keyboard proxy', lambda:
        session.focus() == session.state['proxy'], session)
    session.press('Escape')
    wait('container Escape returns to Buttons scope', lambda:
        session.buttons_focused('TrayContainer'), session)
    handle = session.process_handle(session.options.buttons, 'TrayContainer')
    if handle:
        handle.send(signal.SIGKILL)
        exited = dict(method='authenticated pidfd SIGKILL', **handle.exited(seconds=5))
    else:
        session.command('KillModule FvwmButtons TrayContainer')
        wait('normal container termination', lambda:
            session.named('TrayContainer') is None, session)
        exited = dict(method='core KillModule normal termination')
    wait('bar crash roots tray host', lambda:
        session.client.parent(session.state['host']) == session.client.root,
        session)
    wait('bar crash leaves tray hidden', session.hidden, session)
    after_bar = session.peer_client.state(plug)
    assert after_bar['exists'] and after_bar['parent'] == before['parent']
    session.command('Read "%s"' % overlay)
    wait('container restart reattaches native owner', lambda:
        session.service()['attached'], session)
    session.tray('Show')
    session.fence()
    tray = session.process_handle(session.options.tray, 'SlateTray')
    if tray:
        tray.send(signal.SIGKILL)
        tray_exit = dict(method='authenticated pidfd SIGKILL', **tray.exited(seconds=5))
    else:
        session.command('KillModule FvwmTray SlateTray')
        wait('normal contained Tray termination', lambda:
            session.peer_client.request('owner')['owner'] == 0, session)
        tray_exit = dict(method='core KillModule normal termination')
    after_tray = wait_state(session, session.peer_client, plug, lambda value:
        value['exists'] and value['parent'] == session.client.root and
        value['map'] == 0, 'tray crash roots and unmaps foreign plug')
    session.command('KillModule FvwmButtons TrayContainer')
    wait('container retired after tray crash', lambda:
        session.named('TrayContainer') is None, session)
    assert session.peer_client.state(plug)['exists']
    evidence['cases']['container_attach_resize_focus_competition_both_termination_orders'] = dict(
        overlay=overlay.name, sha256=digest(overlay), attached=state,
        before=before, after_bar=after_bar, bar_exit=exited,
        after_tray=after_tray, tray_exit=tray_exit)
    session.change({'tray_enabled': False})
    session.change({'tray_enabled': True})
    session.observe_owner()


def bus_loss_cases(session, evidence, runtime, bus, peers):
    plug = session.peer_client.create(width=24, height=20)
    session.peer_client.request('dock', id=plug)
    session.tray('Show')
    session.fence()
    before = wait_state(session, session.peer_client, plug, lambda value:
        value['map'] == 2, 'bus-loss foreign witness visible')
    session.focus_app()
    bus.close(time.monotonic() + 4)
    disconnected = wait('broker loss retires only modern state', lambda:
        (lambda value: value if not value['connected'] and
            not value['modern_count'] and not session.cells() else None)(
                session.service()), session)
    after = session.peer_client.state(plug)
    assert after['exists'] and after['parent'] == before['parent'] and after['map'] == 2
    assert session.focus() == session.app
    assert session.peer_client.request('owner')['owner'] == session.state['owner']
    replacement = runtime.start('replacement-private-tray-bus', ['dbus-daemon',
        '--session', '--nofork', '--nopidfile', '--print-address=1',
        '--address=' + session.options.session_address], 'replacement-tray-bus.log')
    wait_for_predicate('replacement owned broker at same explicit path', lambda:
        replacement.log_path.read_text().startswith('unix:'),
        time.monotonic() + 8, process=replacement)
    assert not session.service()['connected'], 'bus reconnected without explicit retry'
    peer = BusPeer(session.options.sni_client, session.options.session_address,
        session.env, session.out, 'replacement-bus-producer')
    peers.append(peer)
    session.tray('Reload')
    wait('explicit reload reconnects native bus watcher', lambda:
        session.service()['connected'] and peer.request('has-owner',
            name='org.kde.StatusNotifierWatcher')['owned'], session)
    assert peer.request('add', title='Replacement broker item')['ok']
    session.fence(lambda value: not value['pending'])
    session.tray('Show')
    session.fence()
    wait('replacement owner yields fresh visible native item', lambda:
        (lambda value: value if value and value['actual'] else None)(
            session.cell('Replacement broker item')), session)
    assert session.peer_client.state(plug)['parent'] == before['parent']
    evidence['cases']['broker_loss_xembed_survival_and_explicit_recovery'] = dict(
        disconnected=disconnected, foreign_before=before, foreign_after=after,
        recovered=session.service(), focus_preserved=True)
    session.peer_client.request('destroy', id=plug)


def run(options):
    runtime = HarnessRuntime(prefix='fvwm-tray-modern-')
    out = options.artifacts_dir.resolve()
    out.mkdir(exist_ok=False, parents=True)
    session = None
    peers = []
    evidence = dict(status='running', cases={}, captures=[], operations=[],
        limits=['Synthetic bus/X clients; real-toolkit qualification is separate.',
            'Owned private bus/display only; no physical input qualification.',
            'Cooperative budgets do not preempt one library/kernel/X call.'])
    try:
        busdir = runtime.root / 'bus'
        busdir.mkdir(mode=0o700)
        bus = runtime.start('private-tray-bus', ['dbus-daemon', '--session',
            '--nofork', '--nopidfile', '--print-address=1',
            '--address=unix:path=' + str(busdir / 'socket')], 'private-tray-bus.log')
        address = wait_for_predicate('owned tray bus address', lambda:
            (lambda data: data.splitlines()[0] if data.startswith('unix:')
                else None)(bus.log_path.read_text()), time.monotonic() + 8,
            process=bus)
        if options.xorg_config:
            _, options.display = start_xorg_dummy(runtime, options.xorg_config,
                time.monotonic() + 10)
            wait_for_randr(runtime, options.display, time.monotonic() + 10)
            run_required_xrandr(runtime, options.display,
                ['--addmode', 'DUMMY1', '1280x1024'], 'modern-second-mode',
                evidence['operations'])
            run_required_xrandr(runtime, options.display,
                ['--output', 'DUMMY0', '--mode', '1920x1080', '--pos', '0x0',
                    '--primary', '--output', 'DUMMY1', '--mode', '1280x1024',
                    '--pos', '1920x0', '--fb', '3200x1080'],
                'modern-two-outputs', evidence['operations'])
        elif not options.display:
            _, options.display = start_display_server(runtime, 'Xvfb',
                ['Xvfb', '-screen', '0', '1440x1000x24', '-noreset'],
                'modern-x.log', time.monotonic() + 10)
        evidence['broker_printed_address'] = address
        address = address.split(',guid=', 1)[0]
        options.initial_owner = 0
        options.session_address = address
        options.session_bus = 'reuse'
        options.session_extra_env = {'FVWM_DESKTOP_WORKER': str(options.worker)}
        watcher = BusPeer(options.sni_client, address,
            runtime.environment({'DISPLAY': options.display}), out, 'incumbent-watcher')
        peers.append(watcher)
        assert watcher.request('watch') == {'kde': True, 'freedesktop': True}
        session = Modern.__new__(Modern)
        Modern.__init__(session, runtime, options, out)
        session.evidence.update(evidence)
        evidence = session.evidence
        evidence['inputs'] = {name: dict(path=str(getattr(options, name)),
            sha256=digest(getattr(options, name))) for name in
            ('fvwm', 'session', 'desktop', 'buttons', 'form', 'tray', 'mfl',
                'client', 'sni_client', 'worker', 'capture')}
        evidence['execution_profile'] = options.execution_profile
        evidence['named_icon_fixture'] = install_icon_fixture(session)
        for index in range(3):
            peers.append(BusPeer(options.sni_client, address, session.env, out,
                'item-producer-%d' % index))
        if options.expect_modern_unavailable:
            session.change({'tray_enabled': True, 'tray_modern': True})
            session.observe_owner()
            state = session.service()
            assert not state['compiled'] and state['requested'] and not state['connected']
            evidence['cases']['feature_disabled_explicit_unavailable'] = state
        else:
            first = protocol_cases(session, evidence, watcher, peers[1:])
            group = options.case_group
            evidence['selected_case_group'] = group
            if group in ('full', 'tooltip'):
                from tray_tooltip_cases import run as tooltip_cases
                tooltip_peer = BusPeer(options.sni_client, address, session.env,
                    out, 'tooltip-lifetime-producer')
                peers.append(tooltip_peer)
                tooltip_cases(session, evidence, tooltip_peer)
            if group in ('full', 'menu'):
                menu_cases(session, evidence, peers[1], first)
            if group in ('full', 'lifecycle'):
                lifecycle_cases(session, evidence, peers[1:])
            if group in ('full', 'appearance'):
                theme_cases(session, evidence, peers[1], peers)
            if group == 'full':
                capacity_cases(session, evidence, watcher, peers)
            if group in ('full', 'toolkit'):
                own_watcher_cases(session, evidence, watcher, peers[1], peers)
                toolkit_cases(session, evidence, peers)
        if options.case_group in ('full', 'container'):
            from tray_focus_clock_cases import run as focus_clock_cases
            focus_clock_cases(session, evidence)
        if options.phase in ('container', 'full') and options.case_group in (
                'full', 'container'):
            container_cases(session, evidence)
            if (session.core_handle and not (options.expect_modern_unavailable or
                    options.expect_no_container)):
                from tray_container_cases import run as extra_container_cases
                extra_container_cases(session, evidence)
        if not options.expect_modern_unavailable and options.case_group in (
                'full', 'bus'):
            bus_loss_cases(session, evidence, runtime, bus, peers)
        session.change({'tray_enabled': False})
        wait('final reserved owner released', lambda:
            session.peer_client.request('owner')['owner'] == 0, session)
        evidence['status'] = 'passed'
    except BaseException as error:
        evidence.update(status='failed', error=repr(error),
            traceback=traceback.format_exc())
        if session:
            try:
                evidence['failure_snapshot'] = session.snapshot()
                session.capture('failure-observation.png')
            except BaseException as diagnostic:
                evidence['failure_observation_error'] = repr(diagnostic)
        raise
    finally:
        evidence['bus_peer_cleanup'] = []
        for index, peer in enumerate(peers):
            (out / ('bus-peer-%d-wire.json' % index)).write_text(
                json.dumps(peer.records, indent=2) + '\n')
            try:
                evidence['bus_peer_cleanup'].append(peer.close())
            except BaseException as error:
                evidence.update(status='failed', peer_cleanup_error=repr(error))
        if session:
            try:
                session.close()
            except BaseException as error:
                evidence.update(status='failed', cleanup_error=repr(error))
        try:
            runtime.close()
        except BaseException as error:
            evidence.update(status='failed', runtime_cleanup_error=repr(error))
        (out / 'result.json').write_text(json.dumps(evidence, indent=2) + '\n')
    assert evidence['status'] == 'passed', evidence


def main():
    parser = argparse.ArgumentParser(description=__doc__, allow_abbrev=False)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--artifacts-dir', type=Path, required=True)
    for name in ('fvwm', 'session', 'desktop', 'form', 'buttons', 'tray', 'mfl',
            'client', 'sni-client', 'worker', 'capture', 'data-root', 'modules-dir'):
        parser.add_argument('--' + name, type=Path)
    parser.add_argument('--display')
    parser.add_argument('--xorg-config', type=Path)
    parser.add_argument('--toolkit-env', type=Path)
    parser.add_argument('--phase', choices=('modern', 'container', 'full'), default='full')
    parser.add_argument('--case-group', default='full',
        choices=('full', 'menu', 'lifecycle', 'appearance', 'toolkit', 'container', 'bus', 'tooltip'),
        help='Select an affected live case group; only full combines acceptance.')
    parser.add_argument('--expect-modern-unavailable', action='store_true')
    parser.add_argument('--expect-no-container', action='store_true')
    parser.add_argument('--execution-profile', default='unspecified',
        choices=('unspecified', 'native-cpu', 'emulated-cpu'))
    options = parser.parse_args()
    options.build = options.build.resolve()
    source = Path(__file__).resolve().parents[2]
    defaults = dict(fvwm=options.build / 'fvwm3',
        session=options.build / 'bin/FvwmPrompt/fvwm-session',
        desktop=options.build / 'bin/FvwmPrompt/FvwmDesktop',
        form=options.build / 'modules/FvwmForm/FvwmForm',
        buttons=options.build / 'modules/FvwmButtons/FvwmButtons',
        tray=options.build / 'modules/FvwmTray/FvwmTray',
        mfl=options.build / 'modules/FvwmMFL/FvwmMFL',
        client=options.build / 'tests/tray-client',
        sni_client=options.build / 'tests/tray-sni-client',
        worker=options.build / 'bin/FvwmPrompt/fvwm-desktop-worker',
        capture=options.build / 'bin/fvwm-desktop-capture',
        data_root=source / 'default-config')
    for name, default in defaults.items():
        value = (getattr(options, name) or default).resolve()
        assert value.exists(), value
        setattr(options, name, value)
    options.module_links = dict(FvwmTray=options.tray, FvwmButtons=options.buttons)
    run(options)


if __name__ == '__main__':
    main()
