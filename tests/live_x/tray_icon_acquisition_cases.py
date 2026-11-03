#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License, version 2 or later.
"""Optional Linux load-attempt observation; never use pixels as its oracle."""
import hashlib
import os
from pathlib import Path
import socket
import struct
import time

from live_x_harness import wait_for_predicate


class Probe:
    def __init__(self, path, session):
        self.path = path
        self.client_path = path.with_name('client.sock')
        self.records = []
        self.pid = None
        self.socket = socket.socket(socket.AF_UNIX, socket.SOCK_DGRAM)
        bound = False
        try:
            self.socket.setsockopt(socket.SOL_SOCKET, socket.SO_PASSCRED, 1)
            self.socket.settimeout(1)
            self.socket.bind(str(self.client_path))
            bound = True
            wait_for_predicate('private icon observation endpoint', path.exists,
                time.monotonic() + 5, process=session.process)
            hello = self.request('hello')
            fields = self.fields(hello)
            assert fields['version'] == 1 and fields['pid'] == self.pid, hello
            self.owned = session.process_handle(
                session.options.tray, 'SlateTray')
            assert self.owned and self.owned.sample()['pid'] == self.pid
        except BaseException:
            self.socket.close()
            if bound:
                self.client_path.unlink(missing_ok=True)
            raise

    @staticmethod
    def fields(text):
        pairs = [word.split('=', 1) for word in text.split()]
        assert pairs and all(len(row) == 2 for row in pairs)
        result = {}
        for key, value in pairs:
            assert key not in result and value.isascii() and value.isdecimal()
            result[key] = int(value)
        return result

    def request(self, command):
        wire = command.encode('ascii')
        # Reserve two records for unconditional release/disarm on failure.
        limit = 1024 if command in ('release', 'disarm') else 1022
        assert 0 < len(wire) <= 256 and len(self.records) < limit
        record = dict(command=command, status='pending')
        self.records.append(record)
        try:
            self.socket.sendto(wire, str(self.path))
            data, ancillary, flags, address = self.socket.recvmsg(8192,
                socket.CMSG_SPACE(struct.calcsize('iII')))
            assert not flags & (socket.MSG_TRUNC | socket.MSG_CTRUNC)
            assert address == str(self.path) and 0 < len(data) <= 4608
            credentials = [struct.unpack('iII', value)
                for level, kind, value in ancillary
                if level == socket.SOL_SOCKET and
                kind == socket.SCM_CREDENTIALS and len(value) == 12]
            assert len(credentials) == 1
            assert credentials[0][1] == os.geteuid()
            pid = credentials[0][0]
            if self.pid is None:
                self.pid = pid
            assert pid == self.pid
            text = data.decode('ascii')
            record.update(response=text, sender_pid=pid, status='received')
            return text
        except BaseException as error:
            record.update(status='failed; retained', error=repr(error))
            raise

    def state(self):
        result = self.fields(self.request('state'))
        assert not result['overflow'] and not result['errors'], result
        return result

    def wait(self, label, predicate, session):
        def observe():
            state = self.state()
            return state if predicate(state) else None
        return wait_for_predicate(label, observe, time.monotonic() + 6,
            process=session.process)

    def paths(self, kind, count):
        assert kind in ('old', 'load') and 0 <= count <= 64
        result = []
        for index in range(count):
            data = bytes.fromhex(self.request('%s %d' % (kind, index)))
            assert 0 < len(data) <= 2048 and b'\0' not in data
            result.append(os.fsdecode(data))
        return result

    def close(self):
        try:
            self.request('release')
            self.request('disarm')
        finally:
            self.socket.close()
            self.client_path.unlink(missing_ok=True)


def install_theme(session, name, rgba, png):
    root = session.home / 'data/icons' / name
    target = root / '24x24/apps'
    target.mkdir(parents=True)
    (root / 'index.theme').write_text('[Icon Theme]\nName=' + name +
        '\nDirectories=24x24/apps\n[24x24/apps]\nSize=24\nType=Fixed\n'
        'Context=Applications\n')
    png(target / 'tray-fixture.png', rgba)


def run(session, evidence, peer, cell, mode, png, source_refresh=False):
    assert mode in ('positive', 'control')
    path = session.options.icon_probe_socket
    probe = None
    failure = None
    case = dict(mode=mode, source_refresh=source_refresh,
        instrumentation_cost_excluded=True,
        oracle='exact-path loader attempts; not visible stale pixels')
    try:
        probe = Probe(path, session)
        install_theme(session, 'probe-old', [230, 20, 30, 255], png)
        install_theme(session, 'probe-new', [20, 50, 230, 255], png)
        # The existing protocol case leaves item names empty/raw. This request
        # explicitly freezes that premise before holding a menu-only batch.
        before = session.cell('Tray KDE alpha')
        assert before and before['actual']
        assert peer.request('update', path='/ItemA', title='Tray KDE alpha',
            named=False, attention_icon='', overlay_icon='')['ok']
        cell = wait_for_predicate('unnamed item revision committed before hold',
            lambda: (lambda current: current if current and
                current['actual'] and current['generation'] !=
                before['generation'] else None)(
                    session.cell('Tray KDE alpha')), time.monotonic() + 6,
            process=session.process)
        assert probe.fields(probe.request('expect probe-old')) == {
            'expected': 1}
        session.command('*SlateTray IconTheme probe-old\n'
            'SendToModule SlateTray Refresh\n')
        probe.wait('old icon profile synchronized',
            lambda s: s['synced'], session)
        assert probe.fields(probe.request('arm')) == {'armed': 1}
        session.show_centered()
        session.click(cell['window'], 3)
        held = probe.wait('completed menu icon batch held',
            lambda s: s['phase'] == 2 and s['old'] > 0, session)
        old = probe.paths('old', held['old'])
        old_files = {name: hashlib.sha256(Path(name).read_bytes()).hexdigest()
            for name in old}
        assert held['open'] and held['ready'] and not held['stale']
        assert (held['held_item'] and held['held_item_generation']
            and held['held_menu'])
        theme = 'probe-old' if source_refresh else 'probe-new'
        assert probe.fields(probe.request('expect ' + theme)) == {
            'expected': 1}
        if source_refresh:
            # Same path and tuple: only an explicit source refresh can cause a
            # new acquisition. The held private content copy must stay intact.
            source = session.home / 'data/icons/probe-old/24x24/apps/tray-fixture.png'
            png(source, [20, 50, 230, 255])
            session.command('SendToModule SlateTray Refresh\n'
                'SendToModule SlateTray Refresh\n')
            committed = session.fence(lambda s: not s['pending'])
            case['configuration_commit'] = committed
        else:
            session.command('*SlateTray IconTheme probe-new\n'
                'SendToModule SlateTray Refresh\n')
        changed = probe.wait('native sync after configuration completion',
            lambda s: s['synced'], session)
        assert changed['phase'] == 2 and changed['open'] and changed['ready']
        assert not changed['stale']
        identity = ('item', 'item_generation', 'menu')
        assert tuple(changed[key] for key in identity) == tuple(
            held['held_' + key] for key in identity), (held, changed)
        assert all(hashlib.sha256(Path(name).read_bytes()).hexdigest() == pin
            for name, pin in old_files.items()), old_files
        assert probe.fields(probe.request('release')) == {'released': 1}
        fresh = probe.wait('fresh profile image load after release',
            lambda s: s['loads'] > 0, session)
        # A control can load the old path first. Continue until a distinct fresh
        # path is observed, otherwise a stuck old batch could pass the control.
        deadline = time.monotonic() + 6
        old_basenames = {Path(name).name for name in old}
        while True:
            loaded = probe.paths('load', fresh['loads'])
            distinct = [name for name in loaded if name not in old]
            changed_content = any(Path(name).name not in old_basenames
                for name in distinct)
            if distinct and (not source_refresh or changed_content):
                break
            assert time.monotonic() < deadline, 'fresh icon load deadline'
            time.sleep(.02)
            fresh = probe.state()
        rejected = not any(name in old for name in loaded)
        assert rejected == (mode == 'positive'), (mode, old, loaded)
        wait_for_predicate('consumed private batch copies retired',
            lambda: all(not Path(name).exists() for name in old),
            time.monotonic() + 6, process=session.process)
        probe.owned.sample()  # Preserve the same executable/birth identity.
        case.update(held=held, changed=changed, final=fresh,
            old_paths=old, loaded_paths=loaded, old_load_rejected=rejected,
            held_files_sha256=old_files, old_private_files_retired=True,
            fresh_content_basename_changed=changed_content,
            records=probe.records.copy(), status='passed expected ' + mode)
    except BaseException as error:
        failure = error
        case.update(status='failed; retained', error=repr(error))
        raise
    finally:
        cleanup = []
        cleanup_error = None
        if probe is not None:
            try:
                probe.close()
                cleanup.append('gate released and disarmed')
            except BaseException as error:
                cleanup_error = error
                cleanup.append('gate cleanup failed: ' + repr(error))
            try:
                # The caller owns the core/module process and confirmed reap.
                session.command('*SlateTray IconTheme hicolor\n'
                    'SendToModule SlateTray Refresh\n')
                session.press('Escape')
                cleanup.append('fixture profile restored')
            except BaseException as error:
                if cleanup_error is None:
                    cleanup_error = error
                cleanup.append('profile cleanup failed: ' + repr(error))
        if cleanup_error is not None:
            case.update(status='failed; retained',
                cleanup_error=repr(cleanup_error))
        if probe is not None:
            case['records'] = probe.records.copy()
        case['cleanup'] = cleanup
        suffix = '_source_refresh' if source_refresh else ''
        evidence['cases']['icon_acquisition_' + mode + suffix] = case
        if failure is None and cleanup_error is not None:
            raise cleanup_error
