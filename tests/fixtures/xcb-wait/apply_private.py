#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License, version 2 or later.
"""Patch only a fresh, separately owned libxcb 1.15 source extraction."""
import argparse
import hashlib
from pathlib import Path

PINS = {
    'src/xcb_in.c': '9606018edb026079812bdff9da4cde942514aa33a78834bf46438e7542fc75fd',
    'src/Makefile.in': 'bcef9374d971b696eae3f64075b5db19cb420b472f6c9aa341a31ceac8d03a04',
}


def replace_once(text, before, after):
    if text.count(before) != 1:
        raise ValueError('private XCB patch anchor count mismatch')
    return text.replace(before, after, 1)


def patch(source):
    source = Path(source)
    if source.resolve(strict=True) != source or not source.is_dir():
        raise ValueError('private XCB root must be an absolute real directory')
    content = {}
    for name, expected in PINS.items():
        path = source / name
        if (path.is_symlink() or path.resolve(strict=True) != path or
                not path.is_file() or path.stat().st_size > 262144):
            raise ValueError('private XCB source type or byte limit')
        raw = path.read_bytes()
        if hashlib.sha256(raw).hexdigest() != expected:
            raise ValueError('private XCB source hash mismatch: ' + name)
        content[name] = raw.decode('ascii')
    fixture = Path(__file__).resolve().parent
    additions = {}
    for name in ('fvwm_xcb_wait.c', 'fvwm_xcb_wait.h'):
        target = source / 'src' / name
        if target.exists() or target.is_symlink():
            raise ValueError('private XCB observer target already exists')
        additions[target] = (fixture / name).read_bytes()

    text = content['src/xcb_in.c']
    text = replace_once(text, '#include "xcbint.h"\n',
        '#include "xcbint.h"\n#include "fvwm_xcb_wait.h"\n')
    text = replace_once(text,
        'static int read_block(const int fd, void *buf, const intptr_t len)\n{',
        'static int read_block(xcb_connection_t *c, void *buf, const intptr_t len)\n{\n'
        '    const int fd = c->fd;')
    text = replace_once(text,
        'int ret = read_block(c->fd, (char *) buf + done, len - done);',
        'int ret = read_block(c, (char *) buf + done, len - done);')
    text = replace_once(text,
        '#endif /* !_Win32 */\n        {\n#if USE_POLL',
        '#endif /* !_Win32 */\n        {\n'
        '            fvwm_xcb_wait_partial(c);\n#if USE_POLL')
    text = replace_once(text,
        '        insert_reader(&c->in.readers, &reader, request, &cond);\n',
        '        fvwm_xcb_wait_reply_begin(c, request);\n'
        '        insert_reader(&c->in.readers, &reader, request, &cond);\n')
    text = replace_once(text,
        '        while(!poll_for_reply(c, request, &ret, e))\n'
        '            if(!_xcb_conn_wait(c, &cond, 0, 0))\n'
        '                break;\n',
        '        while(!poll_for_reply(c, request, &ret, e))\n'
        '        {\n'
        '            fvwm_xcb_wait_reply_wait(c, request);\n'
        '            if(!_xcb_conn_wait(c, &cond, 0, 0))\n'
        '                break;\n'
        '        }\n')
    text = replace_once(text,
        '        remove_reader(&c->in.readers, &reader);\n'
        '        pthread_cond_destroy(&cond);\n',
        '        remove_reader(&c->in.readers, &reader);\n'
        '        fvwm_xcb_wait_reply_end(c, request, ret != 0);\n'
        '        pthread_cond_destroy(&cond);\n')
    content['src/xcb_in.c'] = text
    text = content['src/Makefile.in']
    text = replace_once(text, 'am_libxcb_la_OBJECTS = xcb_conn.lo xcb_out.lo xcb_in.lo',
        'am_libxcb_la_OBJECTS = fvwm_xcb_wait.lo xcb_conn.lo xcb_out.lo xcb_in.lo')
    text = replace_once(text, '\t\txcb_conn.c xcb_out.c xcb_in.c xcb_ext.c xcb_xid.c',
        '\t\tfvwm_xcb_wait.c xcb_conn.c xcb_out.c xcb_in.c xcb_ext.c xcb_xid.c')
    content['src/Makefile.in'] = text

    # Prepare every input before changing this disposable extraction. Refuse
    # reapplication instead of modifying another build or replacing controls.
    for name, text in content.items():
        (source / name).write_text(text, encoding='ascii')
    for target, raw in additions.items():
        with target.open('xb') as stream:
            stream.write(raw)
    return {name: hashlib.sha256((source / name).read_bytes()).hexdigest()
        for name in (*PINS, 'src/fvwm_xcb_wait.c', 'src/fvwm_xcb_wait.h')}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__, allow_abbrev=False)
    parser.add_argument('source', type=Path)
    args = parser.parse_args()
    import json
    print(json.dumps(patch(args.source), sort_keys=True))
