#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License, version 2 or later.
"""Observe native notifications against actual RandR and work-area changes.

Only owned Xorg-dummy outputs, private bus peers, a synthetic dock and this
fixture's windows/processes are controlled. Output disable/re-enable is an
actual RandR topology mutation, not physical monitor hotplug qualification.
The native Status reply is compared with independent core queries and X geometry.
"""
import argparse
import ctypes as C
import hashlib
import json
from pathlib import Path
import shutil
import time
import traceback

from live_x_harness import (HarnessRuntime, start_xorg_dummy, wait_for_randr,
    wait_for_authoritative_state,
    run_required_xrandr, wait_for_predicate)
from mfl_request_live import frame, reply
from mfl_stream_live import Connection
from notify_live import Native, PROFILES, digest


class Topology:
    def __init__(self, runtime, native, out, evidence):
        self.runtime, self.native, self.out = runtime, native, out
        self.evidence = evidence
        self.serial = 0
        endpoint = native.home / ('fvwm_mfl_' + native.options.display.replace(':', '_') + '.sock')
        self.rpc = Connection(endpoint)
        self.rpc.socket.settimeout(3)
        assert 'connection_profile' in self.rpc.record()
        native.connections.append(self.rpc)
        self.dock = None
        native.x.XChangeProperty.argtypes = [C.c_void_p, C.c_ulong,
            C.c_ulong, C.c_ulong, C.c_int, C.c_int, C.c_void_p, C.c_int]
        native.x.XChangeProperty.restype = C.c_int

    def query(self, resource):
        self.serial += 1
        token = 'notify-topology-' + str(self.serial)
        self.rpc.send(frame(token, resource=resource))
        return reply(self.rpc, token)

    def rect(self, window):
        n = self.native
        attributes = n.attrs(window)
        if attributes is None or attributes.map_state != 2:
            return None
        x, y, child = C.c_int(), C.c_int(), C.c_ulong()
        if not n.errors.read(window, 40, lambda: n.x.XTranslateCoordinates(
                n.d, window, n.client.root, 0, 0, C.byref(x), C.byref(y),
                C.byref(child))):
            return None
        return dict(x=x.value, y=y.value, width=attributes.width,
            height=attributes.height, border=attributes.border_width)

    def managed_frame(self, window):
        n = self.native
        current = window
        for _ in range(8):
            root, parent, count = C.c_ulong(), C.c_ulong(), C.c_uint()
            items = C.POINTER(C.c_ulong)()
            try:
                ok = n.errors.read(current, 15, lambda: n.x.XQueryTree(n.d,
                    current, C.byref(root), C.byref(parent), C.byref(items),
                    C.byref(count)))
                if not ok or parent.value == 0:
                    return None
                if parent.value == n.client.root:
                    return current if current != window else None
                current = parent.value
            finally:
                if items:
                    n.x.XFree(items)
        raise AssertionError('managed frame ancestry exceeded bound')

    @staticmethod
    def contains(area, rect):
        return (rect and area['x'] <= rect['x'] and area['y'] <= rect['y'] and
            rect['x'] + rect['width'] + 2 * rect['border'] <= area['x'] + area['width'] and
            rect['y'] + rect['height'] + 2 * rect['border'] <= area['y'] + area['height'])

    def settled(self, label, primary, quarters, ids, center=None,
                after_epoch=None, minimum_top=None):
        n = self.native
        last = {}
        def observed():
            topology = self.query('topology')['topology']
            workspace = self.query('workspaces')
            monitors = {m['name']: m for m in topology['monitors']}
            areas = {m['name']: m for m in workspace['monitors']}
            state = n.status()
            popup_windows = n.popups()
            sample = dict(native=state, topology=topology, workspace=workspace,
                root_work_area=n.property(n.client.root,
                    n.x.XInternAtom(n.d, b'_NET_WORKAREA', 0)),
                popups={str(i): dict(window=w, rect=self.rect(w))
                    for i, w in popup_windows.items()}, primary=primary)
            # Keep this very object: later center/work-area observations must
            # remain visible even if their containment check returns early.
            self.evidence.setdefault('last_observations', {})[label] = sample
            last.clear()
            last.update(sample)
            if primary not in monitors or primary not in areas:
                return None
            epoch = int(topology['epoch'], 16)
            row = monitors[primary]
            if not row['primary'] or (after_epoch is not None and epoch <= after_epoch):
                return None
            if state['pending'] or state['epoch'] != epoch or state['quarters'] != quarters:
                return None
            if areas[primary]['scale_quarters'] != quarters:
                return None
            area = areas[primary]['work_area']
            if area is None or (minimum_top is not None and area['y'] < minimum_top):
                return None
            if set(popup_windows) != set(ids):
                return None
            if not all(self.contains(area, self.rect(popup_windows[i])) for i in ids):
                return None
            if center is not None:
                if n.named('FVWM-X notifications') != center:
                    return None
                frame_window = self.managed_frame(center)
                if frame_window is None:
                    return None
                frame_rect = self.rect(frame_window)
                sample['center'] = dict(window=center, frame=frame_window,
                    client_rect=self.rect(center), frame_rect=frame_rect,
                    selected_id=n.property(center))
                if not self.contains(area, frame_rect):
                    return None
            sample['work_area'] = area
            return sample
        try:
            result = wait_for_predicate(label, observed, time.monotonic() + 8,
                process=n.process)
        finally:
            retained = self.evidence.setdefault('last_observations', {})
            if label not in retained:
                retained[label] = last
        self.evidence.setdefault('samples', {})[label] = result
        return result

    def dock_top(self, height):
        n = self.native
        n.command('Style NotifyTopologyDock Sticky, NoTitle, BorderWidth 0, HandleWidth 0, EWMHUseStrutHints')
        window = n.x.XCreateSimpleWindow(n.d, n.client.root, 0, 0,
            1920, height, 0, 0, 0x445566)
        self.dock = window
        n.x.XStoreName(n.d, window, b'NotifyTopologyDock')
        def prop(name, kind, values):
            data = (C.c_ulong * len(values))(*values)
            assert n.x.XChangeProperty(n.d, window,
                n.x.XInternAtom(n.d, name.encode(), 0), kind, 32, 0,
                C.cast(data, C.c_void_p), len(values))
        atom = n.x.XInternAtom(n.d, b'_NET_WM_WINDOW_TYPE_DOCK', 0)
        prop('_NET_WM_WINDOW_TYPE', 4, [atom])  # XA_ATOM
        prop('_NET_WM_STRUT', 6, [0, 0, height, 0])  # XA_CARDINAL
        prop('_NET_WM_STRUT_PARTIAL', 6,
            [0, 0, height, 0, 0, 0, 0, 0, 0, 1919, 0, 0])
        n.x.XMapWindow(n.d, window)
        n.x.XSync(n.d, 0)
        return window

    def remove_dock(self):
        if self.dock:
            self.native.x.XDestroyWindow(self.native.d, self.dock)
            self.native.x.XSync(self.native.d, 0)
            self.dock = None


def run(options):
    out = options.artifacts_dir.resolve()
    out.mkdir(parents=True, exist_ok=True)
    runtime = HarnessRuntime(prefix='fvwm-notify-topology-')
    native = topology = None
    evidence = dict(status='running', cases={}, operations=[], captures=[],
        limits=['Owned Linux Xorg-dummy/private bus only; no physical hotplug or hardware qualification.',
            'Native server intentionally follows the primary monitor, not arbitrary per-popup monitor selection.',
            'Core cached work-area queries and actual X rectangles are separate observations; no atomic freshness claim.',
            'This fixture does not establish all six target, toolkit, compositor or assistive technology acceptance.'])
    try:
        _, options.display = start_xorg_dummy(runtime,
            options.xorg_config.resolve(), time.monotonic() + 10)
        wait_for_randr(runtime, options.display, time.monotonic() + 10)
        def mutate(arguments, label):
            return run_required_xrandr(runtime, options.display, arguments,
                label, evidence['operations'])
        def facts(label, predicate):
            return wait_for_authoritative_state(runtime, options.display,
                label, predicate, time.monotonic() + 8, label)
        mutate(['--addmode', 'DUMMY1', '1280x1024'], 'second-mode')
        mutate(['--output', 'DUMMY0', '--mode', '1920x1080', '--pos', '0x0', '--primary',
            '--set', 'WIDTH_MM', '508', '--set', 'HEIGHT_MM', '286',
            '--output', 'DUMMY1', '--mode', '1280x1024', '--pos', '1920x0',
            '--set', 'WIDTH_MM', '169', '--set', 'HEIGHT_MM', '135',
            '--fb', '3200x1080', '--fbmm', '846x286'], 'mixed-dpi-baseline')
        evidence['baseline_randr'] = facts('baseline', lambda s:
            set(s['monitors']) == {'DUMMY0', 'DUMMY1'} and
            s['monitors']['DUMMY0']['primary'] and
            s['monitors']['DUMMY1']['mm_width'] == 169 and
            s['monitors']['DUMMY1']['mm_height'] == 135)
        native = Native(runtime, options, out)
        native.evidence = evidence
        native.errors.evidence = evidence
        evidence.setdefault('observer_unexpected_x_errors', native.errors.unexpected)
        topology = Topology(runtime, native, out, evidence)
        evidence['inputs'] = {name: dict(path=str(getattr(options, name)),
            sha256=digest(getattr(options, name))) for name in
            ['fvwm', 'notify', 'client', 'mfl', 'capture', 'xorg_config']}
        native.command('DesktopConfiguration per-monitor')
        native.command('*FvwmNotify Font "xft:Sans:size=11"')
        native.command('SendToModule FvwmNotify Reload')
        peer = native.first
        identifier = peer.notice(summary='Topology continuity',
            body='The same notification follows actual primary monitor changes.',
            actions=['topology-result', 'Show result'])
        popup = native.popup(identifier)
        native.command('SendToModule FvwmNotify Focus')
        center = wait_for_predicate('managed keyboard center',
            lambda: native.named('FVWM-X notifications'),
            time.monotonic() + 5, process=native.process)
        initial = topology.settled('initial-primary', 'DUMMY0', 4,
            [identifier], center)
        assert native.property(center) == [identifier]
        # Work-area changes must update existing views automatically. No Reload
        # is sent after creating/removing this dock: that would hide a missing
        # reservation subscription in the native consumer.
        topology.dock_top(96)
        reserved = topology.settled('automatic-strut-reservation', 'DUMMY0',
            4, [identifier], center, minimum_top=96)
        assert reserved['work_area']['y'] > initial['work_area']['y']
        assert native.popup(identifier) == popup
        native.capture('strut-reservation.png')
        topology.remove_dock()
        def unreserved():
            row = next(m for m in topology.query('workspaces')['monitors']
                if m['name'] == 'DUMMY0')
            return row['work_area'] if row['work_area'] and row['work_area']['y'] == 0 else None
        wait_for_predicate('core reservation revoked', unreserved,
            time.monotonic() + 5, process=native.process)
        restored = topology.settled('automatic-strut-revocation', 'DUMMY0',
            4, [identifier], center)
        assert restored['work_area']['y'] == 0
        assert native.popup(identifier) == popup
        evidence['cases']['automatic_work_area'] = 'passed'
        mutate(['--output', 'DUMMY1', '--primary'], 'second-primary')
        second = facts('second-primary', lambda s:
            s['monitors'].get('DUMMY1', {}).get('primary'))
        scaled = topology.settled('second-primary-2x', 'DUMMY1', 8,
            [identifier], center, after_epoch=initial['native']['epoch'])
        assert scaled['native']['font_height'] > initial['native']['font_height']
        assert native.popup(identifier) == popup
        assert native.property(center) == [identifier]
        evidence['cases']['primary_mixed_scale_continuity'] = dict(
            randr=second, same_popup=popup, same_center=center)
        native.capture('primary-2x.png')
        # Changing actual millimetres updates automatic scale at a new epoch;
        # no unsupported live MonitorScale configuration mutation is used.
        mutate(['--output', 'DUMMY1', '--set', 'WIDTH_MM', '226',
            '--set', 'HEIGHT_MM', '181'], 'second-primary-144dpi')
        facts('physical-144dpi', lambda s:
            s['monitors'].get('DUMMY1', {}).get('mm_width') == 226 and
            s['monitors']['DUMMY1']['mm_height'] == 181)
        changed = topology.settled('second-primary-1.5x', 'DUMMY1', 6,
            [identifier], center, after_epoch=scaled['native']['epoch'])
        assert initial['native']['font_height'] < changed['native']['font_height'] < scaled['native']['font_height']
        assert native.popup(identifier) == popup
        evidence['cases']['actual_dpi_change'] = 'passed'
        mutate(['--output', 'DUMMY0', '--primary', '--output', 'DUMMY1',
            '--off', '--fb', '1920x1080'], 'disable-second-output')
        evidence['disabled_randr'] = facts('disabled-second', lambda s:
            set(s['monitors']) == {'DUMMY0'})
        disabled = topology.settled('disabled-primary-fallback', 'DUMMY0',
            4, [identifier], center, after_epoch=changed['native']['epoch'])
        assert native.popup(identifier) == popup
        evidence['cases']['output_disable_continuity'] = dict(
            actual_randr_disable=True, physical_disconnect_claimed=False)
        mutate(['--output', 'DUMMY1', '--mode', '1280x1024', '--pos', '1920x0',
            '--primary', '--set', 'WIDTH_MM', '169', '--set', 'HEIGHT_MM', '135',
            '--fb', '3200x1080', '--fbmm', '846x286'], 'reenable-second-output')
        facts('reenabled-second', lambda s:
            set(s['monitors']) == {'DUMMY0', 'DUMMY1'} and
            s['monitors']['DUMMY1']['primary'])
        topology.settled('reenabled-primary-2x', 'DUMMY1', 8,
            [identifier], center, after_epoch=disabled['native']['epoch'])
        assert native.popup(identifier) == popup
        # The original ID/action stays usable after all layout changes. No
        # synthetic action is fabricated by the reflow itself.
        signals = peer.request('signals')['signals']
        assert not any(s['signal'] == 'action' for s in signals), signals
        native.press('Return')
        peer.signal('action', identifier, key='topology-result')
        peer.signal('closed', identifier, reason=2)
        native.absent(identifier)
        native.press('Escape')
        evidence['cases']['reflow_action_continuity'] = 'passed'
        # One combined representative palette/font set on the mixed-DPI primary.
        for profile, (fg, bg, font, border) in PROFILES.items():
            overlay = out / (profile + '.fvwm')
            overlay.write_text('Colorset 42 fg ' + fg + ', bg ' + bg + '\n'
                'Style * BorderWidth ' + str(border) + '\n'
                '*FvwmNotify Font "' + font + '"\n'
                'SendToModule FvwmNotify Reload\n')
            local = native.home / overlay.name
            shutil.copyfile(overlay, local)
            native.command('Read ' + str(local))
            identifier = peer.notice(summary='Primary-monitor native notifications',
                body='Live palette, enlarged text and actual monitor scale.',
                actions=['show', 'Show result'])
            native.popup(identifier)
            native.command('SendToModule FvwmNotify Focus')
            center = wait_for_predicate('theme keyboard center',
                lambda: native.named('FVWM-X notifications'),
                time.monotonic() + 5, process=native.process)
            sample = topology.settled(profile + '-2x', 'DUMMY1', 8,
                [identifier], center)
            assert sample['native']['font_height'] >= 25
            native.capture(profile + '-2x.png')
            native.command('SendToModule FvwmNotify DismissAll')
            peer.signal('closed', identifier, reason=2)
            native.press('Escape')
            evidence.setdefault('theme_configs', []).append(dict(profile=profile,
                file=overlay.name, sha256=digest(overlay), observed=sample['native']))
        evidence['cases']['representative_scaled_themes'] = 'passed'
        evidence['status'] = 'passed'
    except BaseException as error:
        evidence.update(status='failed', error=str(error), traceback=traceback.format_exc())
        raise
    finally:
        if topology:
            topology.remove_dock()
        if native:
            for name, peer in [('first', native.first), ('second', native.second)]:
                (out / (name + '-wire.json')).write_text(json.dumps(peer.records, indent=2) + '\n')
            native.close()
            if native.errors.unexpected:
                evidence.update(status='failed', cleanup_x_errors=native.errors.unexpected)
        for process in runtime.processes:
            if process.log_path.exists():
                shutil.copyfile(process.log_path, out / process.log_path.name)
        runtime.close()
        (out / 'result.json').write_text(json.dumps(evidence, indent=2) + '\n')
    assert evidence['status'] == 'passed', evidence


def main():
    parser = argparse.ArgumentParser(allow_abbrev=False)
    parser.add_argument('--build', type=Path, required=True)
    for name in ['fvwm', 'notify', 'client', 'mfl', 'capture', 'desktop']:
        parser.add_argument('--' + name, type=Path)
    parser.add_argument('--xorg-config', type=Path,
        default=Path(__file__).with_name('xorg-dummy.conf'))
    parser.add_argument('--artifacts-dir', type=Path, required=True)
    options = parser.parse_args()
    defaults = dict(fvwm=options.build / 'fvwm3',
        notify=options.build / 'modules/FvwmNotify/FvwmNotify',
        client=options.build / 'tests/notify-client',
        mfl=options.build / 'modules/FvwmMFL/FvwmMFL',
        capture=options.build / 'bin/fvwm-desktop-capture')
    for name, value in defaults.items():
        setattr(options, name, (getattr(options, name) or value).resolve())
        assert getattr(options, name).is_file(), getattr(options, name)
    run(options)


if __name__ == '__main__':
    main()
