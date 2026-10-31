#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the GNU General Public License as published by the Free Software
# Foundation; either version 2, or any later version.
"""Actual PyQt5 widget observer; never writes application preferences.

Qt consumes the real X server itself. Bounded JSON stdin requests snapshots;
stdout contains only these independent observations. No platform theme, font,
icon theme, cursor theme or animation style is selected by this probe.
"""
import json
import os
import re
import select
import sys
import time


def main():
    from PyQt5 import QtCore, QtGui, QtWidgets

    app = QtWidgets.QApplication([])
    assert app.platformName() == 'xcb', app.platformName()
    events = []

    def changed(name):
        if len(events) < 128:
            events.append(dict(name=name, at=time.monotonic()))

    class Observer(QtCore.QObject):
        def eventFilter(self, obj, event):
            names = ('ApplicationFontChange', 'FontChange',
                'ApplicationPaletteChange', 'PaletteChange', 'ThemeChange',
                'StyleChange')
            for name in names:
                if event.type() == getattr(QtCore.QEvent, name, -1):
                    changed(name)
            return False

    watcher = Observer()
    app.installEventFilter(watcher)
    connected = []
    for name in ('fontChanged', 'paletteChanged'):
        callback = getattr(app, name, None)
        if callback is not None:
            callback.connect(lambda *args, name=name: changed(name))
            connected.append(name)
    window = QtWidgets.QWidget()
    window.setWindowTitle('Qt5 native application preference observation')
    layout = QtWidgets.QVBoxLayout(window)
    label = QtWidgets.QLabel('Application font sample: Aa Bb 0123456789')
    button = QtWidgets.QPushButton('Actual Qt widget')
    layout.addWidget(label)
    layout.addWidget(button)
    window.show()

    def snapshot(capture):
        font, metrics = label.font(), label.fontMetrics()
        hints = {}
        for name in ('SH_Widget_Animate', 'SH_Widget_Animation_Duration'):
            key = getattr(QtWidgets.QStyle, name, None)
            hints[name] = (window.style().styleHint(key, None, window)
                if key is not None else None)
        result = dict(pid=os.getpid(), qt=QtCore.qVersion(),
            pyqt=QtCore.PYQT_VERSION_STR, platform=app.platformName(),
            mapped=window.isVisible() and window.windowHandle().isExposed(),
            xid=int(window.winId()), values=dict(
                application_font=app.font().toString(),
                widget_font=font.toString(), family=font.family(),
                point_size=font.pointSizeF(), pixel_size=font.pixelSize(),
                font_height=metrics.height(),
                sample_size=[label.sizeHint().width(), label.sizeHint().height()],
                icon_theme=QtGui.QIcon.themeName(),
                fallback_icon_theme=QtGui.QIcon.fallbackThemeName(),
                folder_icon_available=not QtGui.QIcon.fromTheme('folder').isNull(),
                style=app.style().objectName(), animation_style_hints=hints,
                cursor_shape=int(window.cursor().shape())),
            signals_connected=connected, events=list(events),
            cursor_limit='Qt cursor shape does not attest cursor theme/size')
        if capture:
            assert re.fullmatch('[a-z0-9-]{1,64}', capture)
            path = os.path.join(os.environ['FVWM_SETTINGS_CAPTURE_DIR'],
                capture + '.png')
            assert window.grab().save(str(path), 'PNG')
            result['screenshot'] = str(path)
        return result

    pending = bytearray()
    try:
        while True:
            app.processEvents(QtCore.QEventLoop.AllEvents, 10)
            if not select.select([sys.stdin.buffer], [], [], .01)[0]:
                continue
            data = os.read(sys.stdin.fileno(), 4096)
            if not data:
                assert not pending, 'partial Qt control EOF'
                return 0
            pending.extend(data)
            assert len(pending) <= 8192
            while b'\n' in pending:
                raw, _, rest = pending.partition(b'\n')
                pending[:] = rest
                request = json.loads(raw)
                assert set(request) == {'verb', 'capture'}
                assert request['verb'] in ('state', 'quit')
                result = (snapshot(request['capture']) if
                    request['verb'] == 'state' else dict(ok=True))
                print(json.dumps(result), flush=True)
                if request['verb'] == 'quit':
                    return 0
    finally:
        window.close()


if __name__ == '__main__':
    raise SystemExit(main())
