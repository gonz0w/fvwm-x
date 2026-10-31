#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the GNU General Public License as published by the Free Software
# Foundation; either version 2, or any later version.
"""Shared independent X witnesses for complete-bundle preview fixtures.

This module reads the actual server. It never publishes a root, answers a
provider request, decodes a candidate source or manufactures worker readiness.
"""
import ctypes as C
import ctypes.util
import hashlib
from pathlib import Path
import re
import struct
import zlib


class XImagePrefix(C.Structure):
    # Public Xlib XImage fields through bits_per_pixel; no function table.
    _fields_ = [('width', C.c_int), ('height', C.c_int), ('xoffset', C.c_int),
        ('format', C.c_int), ('data', C.c_void_p), ('byte_order', C.c_int),
        ('bitmap_unit', C.c_int), ('bitmap_bit_order', C.c_int),
        ('bitmap_pad', C.c_int), ('depth', C.c_int),
        ('bytes_per_line', C.c_int), ('bits_per_pixel', C.c_int)]


class PreviewWitness:
    def __init__(self, client):
        self.client = client
        self.x, self.d, self.root = client.x, client.display, client.root
        signatures = {
            'XInternAtom': ([C.c_void_p, C.c_char_p, C.c_int], C.c_ulong),
            'XGetWindowProperty': ([C.c_void_p, C.c_ulong, C.c_ulong,
                C.c_long, C.c_long, C.c_int, C.c_ulong,
                C.POINTER(C.c_ulong), C.POINTER(C.c_int),
                C.POINTER(C.c_ulong), C.POINTER(C.c_ulong),
                C.POINTER(C.POINTER(C.c_ubyte))], C.c_int),
            'XGetGeometry': ([C.c_void_p, C.c_ulong, C.POINTER(C.c_ulong),
                C.POINTER(C.c_int), C.POINTER(C.c_int), C.POINTER(C.c_uint),
                C.POINTER(C.c_uint), C.POINTER(C.c_uint),
                C.POINTER(C.c_uint)], C.c_int),
            'XGetImage': ([C.c_void_p, C.c_ulong, C.c_int, C.c_int,
                C.c_uint, C.c_uint, C.c_ulong, C.c_int], C.c_void_p),
            'XGetPixel': ([C.c_void_p, C.c_int, C.c_int], C.c_ulong),
            'XDestroyImage': ([C.c_void_p], C.c_int),
            'XGrabServer': ([C.c_void_p], C.c_int),
            'XUngrabServer': ([C.c_void_p], C.c_int),
            'XFlush': ([C.c_void_p], C.c_int),
        }
        for name, (arguments, result) in signatures.items():
            function = getattr(self.x, name)
            function.argtypes, function.restype = arguments, result

    def property(self, name):
        actual, count, after = C.c_ulong(), C.c_ulong(), C.c_ulong()
        form, data = C.c_int(), C.POINTER(C.c_ubyte)()
        atom = self.x.XInternAtom(self.d, name.encode(), 0)
        try:
            assert self.x.XGetWindowProperty(self.d, self.root, atom, 0, 1,
                0, 0, C.byref(actual), C.byref(form), C.byref(count),
                C.byref(after), C.byref(data)) == 0
            if actual.value == 0:
                return None
            assert form.value == 32 and count.value == 1 and not after.value
            assert actual.value == self.x.XInternAtom(self.d, b'PIXMAP', 0)
            return int(C.cast(data, C.POINTER(C.c_ulong))[0])
        finally:
            if data:
                self.x.XFree(data)

    def drawable(self, drawable):
        assert drawable and drawable > 1
        root, px, py = C.c_ulong(), C.c_int(), C.c_int()
        width, height, border, depth = (C.c_uint() for _ in range(4))
        assert self.x.XGetGeometry(self.d, drawable, C.byref(root),
            C.byref(px), C.byref(py), C.byref(width), C.byref(height),
            C.byref(border), C.byref(depth))
        assert 0 < width.value <= 8192 and 0 < height.value <= 8192
        image = self.x.XGetImage(self.d, drawable, 0, 0, width, height,
            C.c_ulong(-1).value, 2)
        assert image
        try:
            prefix = C.cast(image, C.POINTER(XImagePrefix)).contents
            assert prefix.data and prefix.width == width.value
            assert prefix.height == height.value and prefix.bytes_per_line > 0
            size = prefix.bytes_per_line * prefix.height
            assert 0 < size <= 256 * 1024 * 1024
            assert prefix.bits_per_pixel in (8, 16, 24, 32)
            row_bytes = prefix.width * (prefix.bits_per_pixel // 8)
            assert row_bytes <= prefix.bytes_per_line
            checksum = hashlib.sha256()
            # Row padding is not a pixel and need not be initialized by Xlib.
            for row in range(prefix.height):
                checksum.update(C.string_at(prefix.data +
                    row * prefix.bytes_per_line, row_bytes))
            return dict(pixmap=drawable, width=width.value, height=height.value,
                depth=depth.value, bytes_per_line=prefix.bytes_per_line,
                bits_per_pixel=prefix.bits_per_pixel,
                byte_order=prefix.byte_order,
                pixels_sha256=checksum.hexdigest(),
                corner_pixels=[int(self.x.XGetPixel(image, x, y))
                    for x, y in ((0, 0), (width.value - 1, 0),
                        (0, height.value - 1),
                        (width.value - 1, height.value - 1))])
        finally:
            self.x.XDestroyImage(image)

    def root_state(self, owned=True):
        # Keep the published XID alive across property, geometry and pixel
        # reads on this observer connection. Ordinary publication may replace
        # and free it between requests otherwise. No RPC/module query, paint
        # request or producer readiness is performed inside this read barrier.
        self.x.XGrabServer(self.d)
        try:
            properties = {name: self.property(name) for name in (
                '_XROOTPMAP_ID', 'ESETROOT_PMAP_ID', '_XSETROOT_ID')}
            assert properties['_XROOTPMAP_ID']
            if owned:
                assert properties['ESETROOT_PMAP_ID'] is None, properties
                assert properties['_XSETROOT_ID'] is None, properties
            return dict(properties=properties,
                image=self.drawable(properties['_XROOTPMAP_ID']))
        finally:
            self.x.XUngrabServer(self.d)
            self.x.XFlush(self.d)


class RenderDirectFormat(C.Structure):
    _fields_ = [(name, C.c_short) for name in ('red', 'red_mask', 'green',
        'green_mask', 'blue', 'blue_mask', 'alpha', 'alpha_mask')]


class RenderFormat(C.Structure):
    _fields_ = [('id', C.c_ulong), ('type', C.c_int), ('depth', C.c_int),
        ('direct', RenderDirectFormat), ('colormap', C.c_ulong)]


class BufferedTintReference:
    """Independent one-pixel server reference, never the candidate backing.

    The production Render path quantizes its premultiplied tint to ARGB8888
    before compositing; the software path blends RGB16 before quantization.
    Keep those two contracts distinct and record which build/server selected.
    """
    def __init__(self, witness, config_header):
        self.witness = witness
        self.x, self.d = witness.x, witness.d
        header = Path(config_header).read_text()
        macro = re.search(r'^\s*#\s*define\s+HAVE_XRENDER\s+(\d+)\b',
            header, re.MULTILINE)
        enabled = bool(macro and int(macro[1]))
        opcode, event, error = (C.c_int() for _ in range(3))
        self.x.XQueryExtension.argtypes = [C.c_void_p, C.c_char_p,
            C.POINTER(C.c_int), C.POINTER(C.c_int), C.POINTER(C.c_int)]
        self.x.XQueryExtension.restype = C.c_int
        extension = bool(self.x.XQueryExtension(self.d, b'RENDER',
            C.byref(opcode), C.byref(event), C.byref(error)))
        self.receipt = dict(build_have_xrender=enabled,
            server_render=extension, opcode=opcode.value,
            formats={}, model='software-rgb16')
        self.render = None
        if enabled and extension:
            library = C.util.find_library('Xrender')
            assert library, 'actual Render profile requires libXrender'
            self.render = C.CDLL(library)
            signatures = {
                'XRenderFindStandardFormat': ([C.c_void_p, C.c_int],
                    C.POINTER(RenderFormat)),
                'XRenderCreatePicture': ([C.c_void_p, C.c_ulong,
                    C.POINTER(RenderFormat), C.c_ulong, C.c_void_p],
                    C.c_ulong),
                'XRenderFreePicture': ([C.c_void_p, C.c_ulong], None),
                'XRenderComposite': ([C.c_void_p, C.c_int, C.c_ulong,
                    C.c_ulong, C.c_ulong] + [C.c_int] * 6 +
                    [C.c_uint] * 2, None),
            }
            for name, (arguments, result) in signatures.items():
                function = getattr(self.render, name)
                function.argtypes, function.restype = arguments, result
            self.rgb = self.render.XRenderFindStandardFormat(self.d, 1)
            self.argb = self.render.XRenderFindStandardFormat(self.d, 0)
            for name, value in (('rgb24', self.rgb), ('argb32', self.argb)):
                self.receipt['formats'][name] = (dict(id=value.contents.id,
                    depth=value.contents.depth) if value else None)
            if self.rgb and self.argb:
                assert self.rgb.contents.depth == 24
                assert self.argb.contents.depth == 32
                self.receipt['model'] = 'raw-xrender-over-argb8888'
        self.x.XCreatePixmap.argtypes = [C.c_void_p, C.c_ulong,
            C.c_uint, C.c_uint, C.c_uint]
        self.x.XCreatePixmap.restype = C.c_ulong
        self.x.XCreateGC.argtypes = [C.c_void_p, C.c_ulong,
            C.c_ulong, C.c_void_p]
        self.x.XCreateGC.restype = C.c_void_p
        self.x.XSetForeground.argtypes = [C.c_void_p, C.c_void_p, C.c_ulong]
        self.x.XFillRectangle.argtypes = [C.c_void_p, C.c_ulong,
            C.c_void_p, C.c_int, C.c_int, C.c_uint, C.c_uint]
        self.x.XFreeGC.argtypes = [C.c_void_p, C.c_void_p]
        self.x.XFreePixmap.argtypes = [C.c_void_p, C.c_ulong]

    def expected(self, source_pixel, source_rgb, tint_rgb):
        receipt = dict(self.receipt, source_pixel=source_pixel,
            source_rgb16=source_rgb, tint_rgb16=tint_rgb)
        if self.receipt['model'] == 'software-rgb16':
            pixel = 0
            for source, tint in zip(source_rgb, tint_rgb):
                pixel = (pixel << 8) | ((12 * source + 88 * tint) // 100 >> 8)
            return pixel, dict(receipt, expected_pixel=pixel)

        # Match only the documented RGB16 -> premultiplied ARGB8888 input
        # quantization. Let the real server implement Over, including its
        # rounding; never guess a pixman formula or read candidate pixels.
        factor = C.c_float(88 / 100).value
        premultiplied = [int(C.c_float(value * factor).value) >> 8
            for value in (*tint_rgb, 65535)]
        red, green, blue, alpha = premultiplied
        packed = (alpha << 24) | (red << 16) | (green << 8) | blue
        pixmaps, pictures, gcs = [], [], []
        try:
            for depth, color in ((24, source_pixel), (32, packed)):
                pixmap = self.x.XCreatePixmap(self.d, self.witness.root,
                    1, 1, depth)
                assert pixmap
                pixmaps.append(pixmap)
                gc = self.x.XCreateGC(self.d, pixmap, 0, None)
                assert gc
                gcs.append(gc)
                self.x.XSetForeground(self.d, gc, color)
                self.x.XFillRectangle(self.d, pixmap, gc, 0, 0, 1, 1)
            self.x.XSync(self.d, 0)
            for pixmap, color in zip(pixmaps, (source_pixel, packed)):
                assert self.witness.drawable(pixmap)['corner_pixels'] == (
                    [color] * 4)
            for pixmap, form in zip(pixmaps, (self.rgb, self.argb)):
                picture = self.render.XRenderCreatePicture(self.d, pixmap,
                    form, 0, None)
                assert picture
                pictures.append(picture)
            self.render.XRenderComposite(self.d, 3, pictures[1], 0,
                pictures[0], 0, 0, 0, 0, 0, 0, 1, 1)
            self.x.XSync(self.d, 0)
            pixel = self.witness.drawable(pixmaps[0])['corner_pixels'][0]
            return pixel, dict(receipt, premultiplied_argb8888=packed,
                expected_pixel=pixel, reference_size=[1, 1])
        finally:
            for picture in reversed(pictures):
                self.render.XRenderFreePicture(self.d, picture)
            for gc in reversed(gcs):
                self.x.XFreeGC(self.d, gc)
            for pixmap in reversed(pixmaps):
                self.x.XFreePixmap(self.d, pixmap)
            self.x.XSync(self.d, 0)


def hashes(source, inputs):
    return {str(path): hashlib.sha256((source / path).read_bytes()).hexdigest()
        for path in inputs}


def binary_hash(path):
    path = Path(path).resolve()
    return dict(path=str(path),
        sha256=hashlib.sha256(path.read_bytes()).hexdigest())


def png_rgb(red, green, blue):
    """Small literal PNG input; only the actual isolated worker decodes it."""
    def chunk(kind, data):
        return struct.pack('>I', len(data)) + kind + data + struct.pack('>I',
            zlib.crc32(kind + data) & 0xffffffff)
    scanline = b'\0' + bytes((red, green, blue)) * 2
    return (b'\x89PNG\r\n\x1a\n' +
        chunk(b'IHDR', struct.pack('>IIBBBBB', 2, 2, 8, 2, 0, 0, 0)) +
        chunk(b'IDAT', zlib.compress(scanline * 2)) + chunk(b'IEND', b''))
