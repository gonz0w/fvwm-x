#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the GNU General Public License as published by the Free Software
# Foundation; either version 2 of the License, or any later version.
"""Real GTK3 settings/notification/font observer; never sets GtkSettings.

The private bounded JSON stdin is fixture control only. GTK/GDK perform their
own XSETTINGS consumption. ctypes permits an existing runtime-only GTK bundle;
no development headers, PyGObject or new production dependency are required.
"""
import ctypes as C
import json
import os
from pathlib import Path
import select
import sys
import time

PROPERTIES = {
    'gtk-font-name': 'string',
    'gtk-icon-theme-name': 'string',
    'gtk-cursor-theme-name': 'string',
    'gtk-cursor-theme-size': 'integer',
    'gtk-enable-animations': 'boolean',
}


class Requisition(C.Structure):
    _fields_ = [('width', C.c_int), ('height', C.c_int)]


def main():
    libraries = {}
    # Use stable Linux SONAMEs directly. find_library may invoke a compiler
    # and cannot reliably discover a runtime-only private bundle.
    sonames = {'gtk-3': 'libgtk-3.so.0', 'gdk-3': 'libgdk-3.so.0',
        'gobject-2.0': 'libgobject-2.0.so.0', 'glib-2.0': 'libglib-2.0.so.0'}
    for name, soname in sonames.items():
        libraries[name] = C.CDLL(soname)
    gtk, obj, glib, gdk = (libraries[n] for n in
        ('gtk-3', 'gobject-2.0', 'glib-2.0', 'gdk-3'))
    signatures = {
        'gtk_init_check': ([C.c_void_p, C.c_void_p], C.c_int),
        'gtk_settings_get_default': ([], C.c_void_p),
        'gtk_window_new': ([C.c_int], C.c_void_p),
        'gtk_window_set_title': ([C.c_void_p, C.c_char_p], None),
        'gtk_container_add': ([C.c_void_p, C.c_void_p], None),
        'gtk_label_new': ([C.c_char_p], C.c_void_p),
        'gtk_widget_show_all': ([C.c_void_p], None),
        'gtk_widget_destroy': ([C.c_void_p], None),
        'gtk_widget_get_window': ([C.c_void_p], C.c_void_p),
        'gtk_widget_get_mapped': ([C.c_void_p], C.c_int),
        'gtk_widget_get_preferred_size': ([C.c_void_p,
            C.POINTER(Requisition), C.POINTER(Requisition)], None),
    }
    for name, (args, result) in signatures.items():
        function = getattr(gtk, name)
        function.argtypes, function.restype = args, result
    for name in ('gtk_get_major_version', 'gtk_get_minor_version',
            'gtk_get_micro_version'):
        getattr(gtk, name).restype = C.c_uint
    glib.g_main_context_iteration.argtypes = [C.c_void_p, C.c_int]
    glib.g_main_context_iteration.restype = C.c_int
    glib.g_free.argtypes = [C.c_void_p]
    # Only the fixed prefix is declared: g_object_get is a varargs function.
    # Every pointer passed through its varargs has an explicit ctypes type.
    obj.g_object_get.argtypes = [C.c_void_p, C.c_char_p]
    obj.g_object_get.restype = None
    callback_type = C.CFUNCTYPE(None, C.c_void_p, C.c_void_p, C.c_void_p)
    obj.g_signal_connect_data.argtypes = [C.c_void_p, C.c_char_p,
        callback_type, C.c_void_p, C.c_void_p, C.c_int]
    obj.g_signal_connect_data.restype = C.c_ulong
    gdk.gdk_x11_window_get_xid.argtypes = [C.c_void_p]
    gdk.gdk_x11_window_get_xid.restype = C.c_ulong
    if not gtk.gtk_init_check(None, None):
        raise RuntimeError('real GTK3 cannot open owned X11 display')
    settings = gtk.gtk_settings_get_default()
    assert settings
    notifications, callbacks = [], []
    for name in PROPERTIES:
        def notify(_settings, _spec, _data, name=name):
            if len(notifications) < 256:
                notifications.append(dict(property=name, time=time.monotonic()))
        callback = callback_type(notify)
        callbacks.append(callback)  # GObject retains only the C function pointer.
        assert obj.g_signal_connect_data(settings, ('notify::' + name).encode(),
            callback, None, None, 0)
    window = gtk.gtk_window_new(0)
    gtk.gtk_window_set_title(window, b'GTK3 native preferences font sample')
    label = gtk.gtk_label_new(b'Application font sample: Aa Bb 0123456789')
    gtk.gtk_container_add(window, label)
    gtk.gtk_widget_show_all(window)

    def snapshot():
        values = {}
        for name, kind in PROPERTIES.items():
            if kind == 'string':
                value = C.c_void_p()
                obj.g_object_get(C.c_void_p(settings), name.encode(),
                    C.byref(value), C.c_void_p())
                try:
                    values[name] = C.string_at(value).decode('utf-8') if value.value else None
                finally:
                    if value.value:
                        glib.g_free(value)
            else:
                value = C.c_int()
                obj.g_object_get(C.c_void_p(settings), name.encode(),
                    C.byref(value), C.c_void_p())
                values[name] = bool(value.value) if kind == 'boolean' else value.value
        minimum, natural = Requisition(), Requisition()
        gtk.gtk_widget_get_preferred_size(label, C.byref(minimum), C.byref(natural))
        native = gtk.gtk_widget_get_window(window)
        return dict(pid=os.getpid(), values=values,
            notifications=list(notifications), mapped=bool(gtk.gtk_widget_get_mapped(window)),
            xid=int(gdk.gdk_x11_window_get_xid(native)) if native else 0,
            font_sample_natural=[natural.width, natural.height],
            gtk=[getattr(gtk, n)() for n in ('gtk_get_major_version',
                'gtk_get_minor_version', 'gtk_get_micro_version')],
            loaded_libraries=sorted({line.rsplit(None, 1)[-1]
                for line in Path('/proc/self/maps').read_text().splitlines()
                if '/' in line and any(s in line for s in
                    ('libgtk-3', 'libgdk-3', 'libgobject-2.0', 'libglib-2.0'))}))

    pending = bytearray()
    try:
        while True:
            for _ in range(128):
                if not glib.g_main_context_iteration(None, 0):
                    break
            if not select.select([sys.stdin.buffer], [], [], .01)[0]:
                continue
            data = os.read(sys.stdin.fileno(), 4096)
            if not data:
                assert not pending, 'partial toolkit control EOF'
                break
            pending.extend(data)
            assert len(pending) <= 8192, 'toolkit control bound'
            while b'\n' in pending:
                raw, _, rest = pending.partition(b'\n')
                pending[:] = rest
                request = json.loads(raw)
                assert set(request) == {'verb'}
                if request['verb'] == 'quit':
                    print(json.dumps(dict(ok=True)), flush=True)
                    return 0
                assert request['verb'] == 'state'
                print(json.dumps(snapshot()), flush=True)
    finally:
        gtk.gtk_widget_destroy(window)
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
