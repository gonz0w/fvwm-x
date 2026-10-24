#!@PYTHON@
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
"""Isolated legacy Xlib/PNG capture; native replacement remains a separate M2 owner."""

import ctypes as C
import ctypes.util
import datetime as dt
import os
from pathlib import Path
import struct
import sys
import tempfile
import time
import zlib

def png_bytes(width, height, rows):
    def chunk(kind, data):
        crc = zlib.crc32(kind + data) & 0xffffffff
        return struct.pack('!I', len(data)) + kind + data + struct.pack('!I', crc)
    raw = b''.join(b'\0' + row for row in rows)
    return (b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('!IIBBBBB', width, height, 8, 2, 0, 0, 0))
            + chunk(b'IDAT', zlib.compress(raw, 6)) + chunk(b'IEND', b''))


def screenshot(directory):
    """Use the existing Xlib connection ABI; encode PNG with stdlib zlib.

    XGetPixel handles visual masks, depth and endianness (including 16-bit X).
    This intentionally happens in a worker with its own display connection.
    """
    class Image(C.Structure):
        _fields_ = [('width', C.c_int), ('height', C.c_int), ('xoffset', C.c_int),
                    ('format', C.c_int), ('data', C.c_void_p), ('byte_order', C.c_int),
                    ('bitmap_unit', C.c_int), ('bitmap_bit_order', C.c_int),
                    ('bitmap_pad', C.c_int), ('depth', C.c_int),
                    ('bytes_per_line', C.c_int), ('bits_per_pixel', C.c_int),
                    ('red_mask', C.c_ulong), ('green_mask', C.c_ulong),
                    ('blue_mask', C.c_ulong)]
    x = C.CDLL(ctypes.util.find_library('X11') or 'libX11.so')
    for name, args, result in (
        ('XOpenDisplay', [C.c_char_p], C.c_void_p),
        ('XDefaultScreen', [C.c_void_p], C.c_int),
        ('XRootWindow', [C.c_void_p, C.c_int], C.c_ulong),
        ('XDisplayWidth', [C.c_void_p, C.c_int], C.c_int),
        ('XDisplayHeight', [C.c_void_p, C.c_int], C.c_int),
        ('XGetImage', [C.c_void_p, C.c_ulong, C.c_int, C.c_int, C.c_uint, C.c_uint, C.c_ulong, C.c_int], C.POINTER(Image)),
        ('XGetPixel', [C.POINTER(Image), C.c_int, C.c_int], C.c_ulong),
        ('XDestroyImage', [C.POINTER(Image)], C.c_int),
        ('XCloseDisplay', [C.c_void_p], C.c_int),
        ('XSync', [C.c_void_p, C.c_int], C.c_int),
        ('XSetErrorHandler', [C.c_void_p], C.c_void_p),
    ):
        function = getattr(x, name)
        function.argtypes, function.restype = args, result
    display = None
    # Connection retries are bounded and stay in the capture worker. A brief
    # local-server connection failure should not immediately lose the request.
    for attempt in range(3):
        display = x.XOpenDisplay(None)
        if display:
            break
        if attempt < 2:
            time.sleep(0.1)
    if not display:
        raise RuntimeError('Cannot open the X display')
    # A concurrent RandR resize may invalidate the capture rectangle. Contain
    # BadMatch here instead of letting Xlib terminate the desktop helper.
    handler_type = C.CFUNCTYPE(C.c_int, C.c_void_p, C.c_void_p)
    handler = handler_type(lambda _display, _event: 0)
    previous_handler = x.XSetErrorHandler(C.cast(handler, C.c_void_p))
    image = None
    try:
        screen = x.XDefaultScreen(display)
        width, height = x.XDisplayWidth(display, screen), x.XDisplayHeight(display, screen)
        if width <= 0 or height <= 0 or width * height > 16777216:
            raise RuntimeError('Screenshot exceeds the 16 megapixel limit')
        image = x.XGetImage(display, x.XRootWindow(display, screen), 0, 0,
                            width, height, C.c_ulong(-1).value, 2)
        if not image:
            raise RuntimeError('X server cannot capture this display')
        channels = []
        for mask in (image.contents.red_mask, image.contents.green_mask, image.contents.blue_mask):
            if not mask:
                raise RuntimeError('Screenshot needs a TrueColor visual')
            shift = (mask & -mask).bit_length() - 1
            channels.append((mask, shift, mask >> shift))
        rows = []
        info = image.contents
        for y in range(height):
            row = bytearray()
            # Decode common ZPixmap formats directly. Respect server byte order
            # and padded scanlines; XGetPixel handles less common pixel layouts.
            if (info.bits_per_pixel in (24, 32) and info.xoffset == 0 and
                    (info.red_mask, info.green_mask, info.blue_mask) == (0xff0000, 0xff00, 0xff)):
                size = info.bits_per_pixel // 8
                data = C.string_at(info.data + y * info.bytes_per_line, width * size)
                offsets = (2, 1, 0) if info.byte_order == 0 else tuple(range(size - 3, size))
                row = bytearray(width * 3)
                for channel, offset in enumerate(offsets):
                    row[channel::3] = data[offset::size]
                rows.append(row)
                continue
            if info.bits_per_pixel in (16, 32) and info.xoffset == 0:
                size = info.bits_per_pixel // 8
                data = C.string_at(info.data + y * info.bytes_per_line, width * size)
                fmt = ('<' if info.byte_order == 0 else '>') + ('H' if size == 2 else 'I')
                pixels = (item[0] for item in struct.iter_unpack(fmt, data))
            else:
                pixels = (x.XGetPixel(image, column, y) for column in range(width))
            for pixel in pixels:
                row.extend(((pixel & mask) >> shift) * 255 // maximum
                           for mask, shift, maximum in channels)
            rows.append(row)
        directory.mkdir(parents=True, exist_ok=True)
        fd, filename = tempfile.mkstemp(prefix='FVWM-X-' + dt.datetime.now().strftime('%Y%m%d-%H%M%S-'),
                                        suffix='.png', dir=directory)
        with os.fdopen(fd, 'wb') as stream:
            stream.write(png_bytes(width, height, rows))
        return 'Screenshot saved: ' + filename
    finally:
        if image:
            x.XDestroyImage(image)
        x.XSync(display, 0)
        x.XCloseDisplay(display)
        x.XSetErrorHandler(previous_handler)


if __name__ == '__main__':
    try:
        if len(sys.argv) != 2 or len(os.fsencode(sys.argv[1])) > 4096:
            raise ValueError('Capture requires one bounded output directory')
        print(screenshot(Path(sys.argv[1])))
    except (OSError, ValueError, RuntimeError) as error:
        print('fvwm-desktop-capture: ' + str(error), file=sys.stderr)
        raise SystemExit(1)
