#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License, version 2 or later.
"""Generate one private negative control without modifying product source."""
from pathlib import Path
import sys


def generate(source, destination):
    text = source.read_text(encoding='utf-8')
    predicate = ('\t\t\t    state->icon_batch[request].acquisition !=\n'
                 '\t\t\t    state->icon_acquisition ||')
    if text.count(predicate) != 1:
        raise ValueError('acquisition predicate changed; review the control')
    changed = text.replace(predicate,
        '\t\t\t    false || /* Private acquisition negative control. */', 1)
    banner = '/* Generated private test control; not installed. */\n'
    destination.write_text(banner + changed, encoding='utf-8')


if __name__ == '__main__':
    if len(sys.argv) != 3:
        raise SystemExit('usage: tray_icon_control_source.py SOURCE OUTPUT')
    generate(Path(sys.argv[1]), Path(sys.argv[2]))
