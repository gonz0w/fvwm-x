# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License, version 2 or later.
"""Actual X geometry for scoped desktop metrics; no submission-only pass."""
import ctypes as C
import json
import time

from geometry_transition_live import Pointer
from image_colorsets_live import records
from live_x_harness import wait_for_predicate


def run(*, profile, evidence, change, request, barrier, control, client, core,
        attributes, children, named_windows, capture, bar_color, palette, mode,
        state, native, desktop, updates):
    pointer = Pointer(client)
    x, display = client.x, client.display
    x.XStringToKeysym.argtypes = [C.c_char_p]
    x.XStringToKeysym.restype = C.c_ulong
    x.XKeysymToKeycode.argtypes = [C.c_void_p, C.c_ulong]
    x.XKeysymToKeycode.restype = C.c_ubyte
    pointer.x.XTestFakeKeyEvent.argtypes = [C.c_void_p, C.c_uint, C.c_int,
        C.c_ulong]
    pointer.x.XTestFakeKeyEvent.restype = C.c_int
    case = dict(profile=profile, status='running', observations={},
        oracle='actual X menu/Form/control geometry; independent IPC metadata')
    evidence['component_metrics'] = case
    editor = None
    retained = None

    def desktop_rows(since):
        result = []
        identities = list(native.glob('module-*.json'))
        assert len(identities) <= 64
        for identity in identities:
            assert identity.stat().st_size <= 16384
            metadata = json.loads(identity.read_text())
            if metadata['real'] != str(desktop):
                continue
            path = identity.with_suffix('.jsonl')
            if not path.exists():
                continue
            assert path.stat().st_size <= 16777216
            for line in path.read_text().splitlines(keepends=True):
                if not line.endswith('\n'):
                    continue
                row = json.loads(line)
                if row.get('direction') == 'out' and row['time'] >= since:
                    result.append(row)
                    assert len(result) <= 16384
        return sorted(result, key=lambda row: row['time'])

    def terminal(since, message):
        prefix = '*SlateNoticeForm: TextHex x'
        expected = [prefix + message[offset:offset + 72].encode().hex()
            for offset in range(0, len(message), 72)]
        rows = [row for row in desktop_rows(since)
            if row.get('text', '').startswith(prefix)]
        for offset in range(len(rows) - len(expected) + 1):
            chunk = rows[offset:offset + len(expected)]
            if [row['text'] for row in chunk] == expected:
                return dict(chunk[0], notice_text=message, chunks=chunk,
                    completed_time=chunk[-1]['time'])
        return None

    def wait(label, predicate):
        return wait_for_predicate(label, predicate, time.monotonic() + 8,
            process=core)

    def key(symbol):
        code = x.XKeysymToKeycode(display, x.XStringToKeysym(symbol))
        assert code
        for pressed in (1, 0):
            assert pointer.x.XTestFakeKeyEvent(display, code, pressed, 0)
        x.XSync(display, 0)

    def geometry(window, controls=False):
        attrs = attributes(window)
        if not attrs or attrs.map_state != 2:
            return None
        result = dict(width=attrs.width, height=attrs.height)
        if controls:
            items = []
            contained = True
            for child in children(window):
                item = attributes(child)
                if not item or item.map_state != 2:
                    continue
                contained = contained and (item.x >= 0 and item.y >= 0 and
                    item.x + item.width + 2 * item.border_width <=
                    attrs.width and item.y + item.height +
                    2 * item.border_width <= attrs.height)
                items.append(dict(x=item.x, y=item.y, width=item.width,
                    height=item.height, border=item.border_width))
            if not items:
                return None  # Do not sample a partially constructed Form.
            result['contained'] = contained
            result['controls'] = sorted(items,
                key=lambda row: (row['y'], row['x'], row['width']))
        return result

    def settled(window, controls=False):
        previous, repeats = None, 0

        def sample():
            nonlocal previous, repeats
            current = geometry(window, controls)
            repeats = repeats + 1 if current and current == previous else 0
            previous = current
            return current if repeats >= 3 else None

        result = wait('stable native component geometry', sample)
        assert 0 < result['width'] <= 1440 and 0 < result['height'] <= 1000
        if controls:
            assert result['contained'], result
        return result

    def open_editor():
        nonlocal editor
        assert not named_windows().get('Desktop component settings')
        control.send(b'SendToModule FvwmDesktop edit-settings group:desktop\n')
        editor = wait('native desktop component editor',
            lambda: named_windows().get('Desktop component settings'))
        return settled(editor, True)

    def close_editor():
        nonlocal editor
        if editor is not None:
            control.send(('WindowId %d Close\n' % editor).encode())
            wait('component editor closed',
                lambda: not named_windows().get('Desktop component settings'))
            editor = None
            barrier()

    def menu_geometry(label, screenshot=False, launcher=False):
        def mapped():
            found = []
            for window in children(client.root):
                attrs = attributes(window)
                if (attrs and attrs.map_state == 2 and attrs.save_under and
                        attrs.width > 20 and attrs.height > 20):
                    found.append(window)
            return found

        before = set(mapped())
        pointer.motion(1050, 100)
        control.send(b'SlateLauncher\n' if launcher else
            b'Menu ComponentMetricsProbe\n')
        try:
            wait('component menu grab', pointer.grabbed)

            def target():
                found = [window for window in mapped() if window not in before]
                return found[0] if len(found) == 1 else None

            window = wait('actual component menu surface', target)
            observed = settled(window)
            if screenshot:
                capture('component-' + label + '-menu.png')
            return observed
        finally:
            key(b'Escape')
            wait('component menu dismissed', lambda: not pointer.grabbed())

    def settings(values):
        change(values)
        result = request(scope='group:desktop')
        for name in ('desktop_font', 'desktop_density', 'desktop_border'):
            item = result['settings'][name]
            assert item['component_version'] == 1, item
            assert item['application'] == (
                'next-owned-form-start; launcher-submitted'), item
        assert json.loads(state.read_text())['schema_version'] == 11
        bar_color(palette, mode)  # Component edits preserve the panel palette.
        case.setdefault('metadata', []).append(result)

    def sample(label, screenshot=False):
        form = open_editor()
        if screenshot:
            capture('component-' + label + '-form.png')
        close_editor()
        result = dict(form=form, menu=menu_geometry(label, screenshot))
        case['observations'][label] = result
        return result

    def recovery(conflict=False):
        settings(dict(desktop_font='xft:Sans:size=12',
            desktop_density='compact', desktop_border='thin',
            desktop_icon_theme='probe-recovery'))
        baseline = sample('recovery-baseline' if not conflict else
            'conflict-baseline')
        current = request(scope='all')
        control.send(('SendToModule FvwmDesktop preview-resource ' +
            current['provider'] + ' contrast high\n').encode())
        wait('actual active resource preview Form',
            lambda: named_windows().get('Preview contrast'))
        if conflict:
            control.send(b'Colorset 14 fg #135724\n')
            wait('actual external role publication', lambda:
                any(row[0] == 14 and row[1] == 0x135724
                    for row in records(updates)))
        since = time.monotonic()
        theme = 'probe-blocked' if conflict else 'probe-resumed'
        pending = change(dict(desktop_font='xft:Sans:size=18',
            desktop_density='spacious', desktop_border='strong',
            desktop_icon_theme=theme))
        for name in ('desktop_font', 'desktop_density', 'desktop_border'):
            assert pending['settings'][name]['application'] == (
                'next-owned-form-start; launcher-not-submitted'), pending
        assert pending['settings']['desktop_icon_theme']['application'] == (
            'not-submitted'), pending
        message = ('Appearance changed elsewhere; automatic restore refused. '
            'Theme updates are paused until controller restart.' if conflict else
            'Resource preview abandoned; previous resources restored.')
        recovered = wait('actual Desktop terminal resource consumer',
            lambda: terminal(since, message))
        wait('resource preview Form retired',
            lambda: not named_windows().get('Preview contrast'))
        notice = named_windows().get('FVWM-X')
        if notice:
            control.send(('WindowId %d Close\n' % notice).encode())
            barrier()

        def publication():
            current = request(scope='group:desktop')
            applied = current['settings']['desktop_icon_theme']['application']
            return current if applied == ('not-submitted' if conflict else
                'tray-configuration-submitted') else None

        after = wait('retained or resumed component publication', publication)
        barrier()
        rows = desktop_rows(since)
        changed = [row for row in rows if (
            row.get('text', '').startswith('MenuStyle SlateLauncherStyle Font ')
            or '*SlateTray IconTheme ' + theme in row.get('text', ''))]
        if conflict:
            assert not changed, changed
            retained = open_editor()
            assert retained == baseline['form'], (retained, baseline)
            capture('component-conflict-retained-form.png')
            close_editor()
        else:
            assert changed and all(row['time'] > recovered['completed_time']
                for row in changed), (recovered, changed)
            applied = sample('recovered')
            assert applied['form']['height'] > baseline['form']['height']
            assert applied['menu']['height'] > baseline['menu']['height']
        case['conflict' if conflict else 'recovery'] = dict(
            immediate_response=pending, terminal_consumer=recovered,
            final_metadata=after, publication_rows=changed,
            retained_old_tuple=conflict)

    try:
        # Fixed short content makes one-axis geometry comparisons independent of
        # application inventory, menu pagination and screen-height clamping.
        control.send(b'DestroyMenu ComponentMetricsProbe\n'
            b'AddToMenu ComponentMetricsProbe "Component metrics" Title\n'
            b'AddToMenu ComponentMetricsProbe "Alpha action" Nop\n'
            b'AddToMenu ComponentMetricsProbe "Bravo action" Nop\n'
            b'AddToMenu ComponentMetricsProbe "Close" Nop\n'
            b'ChangeMenuStyle SlateLauncherStyle ComponentMetricsProbe\n')
        barrier()
        settings(dict(desktop_font='', desktop_density='inherit',
            desktop_border='inherit'))
        inherited = sample('inherited')
        settings(dict(desktop_font='xft:Sans:size=12',
            desktop_density='compact', desktop_border='thin'))
        compact = sample('compact')
        open_editor()
        old_editor = editor
        control.send(b'SendToModule FvwmDesktop schedule\n')
        retained = wait('existing owned schedule Form',
            lambda: named_windows().get('Day / night schedule'))
        old = settled(retained, True)
        settings(dict(desktop_density='spacious'))
        # Settings drafts retire on generation change. The independent schedule
        # Form is the retained-resource witness, not a deliberately stale draft.
        wait('settings draft destroyed after generation change',
            lambda: attributes(old_editor) is None)
        editor = None
        assert named_windows().get('Day / night schedule') == retained
        assert settled(retained, True) == old
        control.send(('WindowId %d Close\n' % retained).encode())
        wait('retained schedule Form closed',
            lambda: not named_windows().get('Day / night schedule'))
        retained = None
        control.send(b'SendToModule FvwmDesktop schedule\n')
        retained = wait('reopened owned schedule Form',
            lambda: named_windows().get('Day / night schedule'))
        reopened = settled(retained, True)
        assert reopened['height'] > old['height'], (old, reopened)
        case['schedule_metrics'] = dict(old=old, reopened=reopened,
            stale_editor_retired=True)
        control.send(('WindowId %d Close\n' % retained).encode())
        wait('reopened schedule Form closed',
            lambda: not named_windows().get('Day / night schedule'))
        retained = None
        spacious = sample('spacious')
        assert spacious['menu']['height'] > compact['menu']['height']
        assert spacious['form']['height'] > compact['form']['height']
        settings(dict(desktop_border='strong'))
        bordered = sample('bordered')
        assert bordered['menu']['width'] > spacious['menu']['width']
        assert bordered['menu']['height'] > spacious['menu']['height']
        assert bordered['form'] == spacious['form']
        settings(dict(desktop_font='xft:Sans:size=18'))
        enlarged = sample('enlarged', True)
        assert enlarged['menu']['width'] > bordered['menu']['width']
        assert enlarged['menu']['height'] > bordered['menu']['height']
        assert enlarged['form']['height'] > bordered['form']['height']
        assert max(row['height'] for row in enlarged['form']['controls']) > max(
            row['height'] for row in bordered['form']['controls'])
        # Also retain the actual product launcher; the short diagnostic menu is
        # the relational oracle, not a substitute for product screenshot review.
        case['product_launcher'] = menu_geometry('product', True, True)
        recovery()
        settings(dict(desktop_font='', desktop_density='inherit',
            desktop_border='inherit'))
        restored = sample('restored')
        assert restored == inherited
        recovery(conflict=True)
        case.update(status='passed', existing_form_unchanged=True,
            reopened_form_adopted=True, inherited_geometry_restored=True)
    except BaseException as failure:
        case.update(status='failed; retained', failure=repr(failure))
        raise
    finally:
        # The caller owns process termination/reap even when this cleanup fails.
        try:
            close_editor()
            if retained is not None:
                control.send(('WindowId %d Close\n' % retained).encode())
                wait('retained Form cleanup',
                    lambda: not named_windows().get('Day / night schedule'))
            if pointer.grabbed():
                key(b'Escape')
                wait('component cleanup releases grab',
                    lambda: not pointer.grabbed())
            control.send(b'DestroyMenu ComponentMetricsProbe\n')
            barrier()
            case['cleanup'] = 'editor/menu closed; private menu retired'
        except BaseException as failure:
            case.update(status='failed; retained', cleanup_failure=repr(failure))
            raise
