#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the GNU General Public License, version 2 or any later version.
"""Small real GTK/Ayatana producer, controlled only on private fixture stdin.

The toolkit libraries implement the actual XEmbed/SNI/dbusmenu protocols. This
program does not construct protocol messages or pretend to be a toolkit backend.
Optional test-only imports must never become a product dependency.
"""
import argparse
import json
from pathlib import Path
import sys


def main():
    parser = argparse.ArgumentParser(allow_abbrev=False)
    parser.add_argument('--mode', choices=('xembed', 'indicator'), required=True)
    parser.add_argument('--icon', type=Path, required=True)
    options = parser.parse_args()
    assert options.icon.is_absolute() and options.icon.is_file()
    import gi
    gi.require_version('Gtk', '3.0')
    from gi.repository import Gtk, GLib, Gdk
    if not Gtk.init_check()[0]:
        raise RuntimeError('real GTK could not open the disposable X display')
    events = []
    indicator = status_icon = None
    connected = False
    widget_tracks, widget_trace = {}, []
    cairo_binding = None
    # One independent read of the same fixture file exposes a decoder GError
    # that StatusIcon's convenience constructor may otherwise hide. It never
    # replaces the StatusIcon image and is not repeated by polling snapshots.
    try:
        gi.require_version('GdkPixbuf', '2.0')
        from gi.repository import GdkPixbuf
        loaded = GdkPixbuf.Pixbuf.new_from_file(str(options.icon))
        channels = loaded.get_n_channels()
        first = list(loaded.get_pixels()[:channels])
        file_image = dict(width=loaded.get_width(), height=loaded.get_height(),
            channels=channels, bits_per_sample=loaded.get_bits_per_sample(),
            has_alpha=bool(loaded.get_has_alpha()),
            first_rgba=first if len(first) == 4 else first + [255])
        del loaded
    except Exception as error:
        file_image = dict(error=repr(error)[:512])

    def record(kind, label=''):
        assert len(events) < 128
        events.append(dict(event=kind, label=label))

    menu = Gtk.Menu()
    action = Gtk.MenuItem(label='Toolkit action')
    action.connect('activate', lambda *_: record('activate', 'Toolkit action'))
    menu.append(action)
    check = Gtk.CheckMenuItem(label='Toolkit check')
    check.connect('activate', lambda *_: record('activate', 'Toolkit check'))
    menu.append(check)
    parent = Gtk.MenuItem(label='Toolkit submenu')
    nested = Gtk.Menu()
    child = Gtk.MenuItem(label='Toolkit nested')
    child.connect('activate', lambda *_: record('activate', 'Toolkit nested'))
    nested.append(child)
    parent.set_submenu(nested)
    menu.append(parent)
    menu.append(Gtk.SeparatorMenuItem())
    disabled = Gtk.MenuItem(label='Toolkit disabled')
    disabled.set_sensitive(False)
    menu.append(disabled)
    menu.show_all()
    if options.mode == 'xembed':
        status_icon = Gtk.StatusIcon.new_from_file(str(options.icon))
        status_icon.set_title('GTK XEmbed interop')
        status_icon.set_tooltip_text('Real GTK status icon')
        status_icon.connect('activate', lambda *_: record('activate', 'GTK icon'))
        status_icon.connect('popup-menu', lambda *_: record('popup', 'GTK icon'))
        status_icon.set_visible(True)
    else:
        gi.require_version('AyatanaAppIndicator3', '0.1')
        from gi.repository import AyatanaAppIndicator3 as Indicator
        indicator = Indicator.Indicator.new('fvwm-toolkit-interop',
            str(options.icon), Indicator.IndicatorCategory.APPLICATION_STATUS)
        indicator.set_title('Ayatana toolkit indicator')
        indicator.set_menu(menu)
        indicator.set_secondary_activate_target(action)
        def changed(_indicator, value):
            nonlocal connected
            connected = bool(value)
            record('connection', str(connected))
        indicator.connect('connection-changed', changed)
        indicator.set_status(Indicator.IndicatorStatus.ACTIVE)

    def widget_snapshot(widget):
        allocation = widget.get_allocation()
        result = dict(type=widget.__gtype__.name,
            mapped=bool(widget.get_mapped()),
            realized=bool(widget.get_realized()),
            visible=bool(widget.get_visible()),
            allocation=dict(x=allocation.x, y=allocation.y,
                width=allocation.width, height=allocation.height))
        if isinstance(widget, Gtk.Image):
            storage = widget.get_storage_type()
            result.update(storage_type=int(storage),
                storage_name=storage.value_nick)
            if storage == Gtk.ImageType.PIXBUF:
                pixbuf = widget.get_pixbuf()
                if pixbuf:
                    result['pixbuf'] = dict(width=pixbuf.get_width(),
                        height=pixbuf.get_height())
        # SURFACE storage is identified by enum only. Reading it requires a
        # foreign cairo binding and is unnecessary for this diagnostic.
        return result

    def plug_snapshot(xid):
        nonlocal cairo_binding
        toplevels = Gtk.Window.list_toplevels()
        result = dict(toplevel_count=len(toplevels), inspected=min(8,
            len(toplevels)))
        plug = next((window for window in toplevels[:8]
            if isinstance(window, Gtk.Plug) and int(window.get_id()) == xid),
            None)
        if plug is None:
            result['matched'] = False
            return result
        result.update(matched=True, plug=widget_snapshot(plug))
        child = plug.get_child() if isinstance(plug, Gtk.Bin) else None
        if child:
            result['child'] = widget_snapshot(child)
        if xid not in widget_tracks:
            if len(widget_tracks) >= 4:
                result['tracking_limit'] = True
                return result
            track = dict(counts={}, hook_errors=[])
            widget_tracks[xid] = track
            if cairo_binding is None:
                try:
                    gi.require_foreign('cairo')
                    cairo_binding = dict(available=True)
                except Exception as error:
                    cairo_binding = dict(available=False,
                        error=repr(error)[:512])

            def hook(widget, target, signal):
                def observed(*_args):
                    key = target + ':' + signal
                    track['counts'][key] = min(1000000,
                        track['counts'].get(key, 0) + 1)
                    if len(widget_trace) < 32:
                        widget_trace.append(dict(xid=xid, target=target,
                            event=signal))
                    return False
                try:
                    widget.connect(signal, observed)
                except Exception as error:
                    if len(track['hook_errors']) < 8:
                        track['hook_errors'].append(dict(target=target,
                            signal=signal, error=repr(error)[:512]))

            for target, widget in [('plug', plug), ('image', child)]:
                if widget is None:
                    continue
                for signal in ('map-event', 'unmap-event', 'configure-event'):
                    hook(widget, target, signal)
                if cairo_binding['available']:
                    hook(widget, target, 'draw')
        result.update(tracked_xids=list(widget_tracks),
            signals=widget_tracks[xid], trace=list(widget_trace),
            cairo_binding=cairo_binding)
        return result

    def image_snapshot():
        storage = status_icon.get_storage_type()
        result = dict(storage_type=int(storage),
            storage_name=storage.value_nick)
        if storage == Gtk.ImageType.PIXBUF:
            pixbuf = status_icon.get_pixbuf()
            if pixbuf:
                channels = pixbuf.get_n_channels()
                pixels = pixbuf.get_pixels()
                first = list(pixels[:channels])
                result['pixbuf'] = dict(width=pixbuf.get_width(),
                    height=pixbuf.get_height(), channels=channels,
                    rowstride=pixbuf.get_rowstride(),
                    bits_per_sample=pixbuf.get_bits_per_sample(),
                    has_alpha=bool(pixbuf.get_has_alpha()),
                    first_rgba=first if len(first) == 4 else first + [255])
        # This is the toolkit's own plug, not a substitute drawing widget.
        xid = int(status_icon.get_x11_window_id())
        if xid:
            try:
                result['widgets'] = plug_snapshot(xid)
            except Exception as error:
                result['widgets_error'] = repr(error)[:512]
            try:
                gi.require_version('GdkX11', '3.0')
                from gi.repository import GdkX11
                window = GdkX11.X11Window.lookup_for_display(
                    Gdk.Display.get_default(), xid)
                if window:
                    visual = window.get_visual()
                    result['gdk_window'] = dict(
                        geometry=list(window.get_geometry()),
                        visible=bool(window.is_visible()),
                        viewable=bool(window.is_viewable()),
                        depth=visual.get_depth() if visual else None,
                        visual_type=int(visual.get_visual_type())
                            if visual else None)
                else:
                    result['gdk_window'] = None
            except Exception as error:
                result['gdk_window_error'] = repr(error)[:512]
        return result

    def snapshot():
        value = dict(mode=options.mode, file_image=file_image,
            gtk=[Gtk.get_major_version(), Gtk.get_minor_version(),
                Gtk.get_micro_version()])
        if status_icon:
            value.update(embedded=bool(status_icon.is_embedded()),
                xid=int(status_icon.get_x11_window_id()),
                size=int(status_icon.get_size()),
                visible=bool(status_icon.get_visible()),
                image=image_snapshot())
        else:
            value.update(connected=connected, status=int(indicator.get_status()))
        return value

    def control(_source, condition):
        if condition & GLib.IO_HUP:
            Gtk.main_quit()
            return False
        line = sys.stdin.readline(8193)
        if not line:
            Gtk.main_quit()
            return False
        if len(line) > 8192 or not line.endswith('\n'):
            raise RuntimeError('toolkit fixture request bound')
        request = json.loads(line)
        verb = request.get('verb')
        if verb in ('state', 'hello'):
            result = snapshot()
        elif verb == 'events':
            result = dict(events=list(events))
            events.clear()
        elif verb in ('show', 'hide'):
            visible = verb == 'show'
            if status_icon:
                status_icon.set_visible(visible)
            else:
                indicator.set_status(Indicator.IndicatorStatus.ACTIVE if
                    visible else Indicator.IndicatorStatus.PASSIVE)
            result = snapshot()
        elif verb == 'icon':
            if status_icon:
                status_icon.set_from_file(str(options.icon))
            else:
                indicator.set_icon_full(str(options.icon), 'Fixture image')
            result = snapshot()
        elif verb == 'quit':
            Gtk.main_quit()
            result = dict(ok=True)
        else:
            raise RuntimeError('unknown private toolkit fixture verb')
        print(json.dumps(result), flush=True)
        return True

    GLib.io_add_watch(sys.stdin, GLib.IO_IN | GLib.IO_HUP, control)
    try:
        Gtk.main()
    finally:
        if status_icon:
            status_icon.set_visible(False)
        if indicator:
            indicator.set_status(Indicator.IndicatorStatus.PASSIVE)


if __name__ == '__main__':
    main()
