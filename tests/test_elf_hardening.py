#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
"""Inspect linked C executables, independently of compiler/linker flags."""
import argparse
import json
from pathlib import Path
import re
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--targets', type=Path, required=True)
    parser.add_argument('--readelf', required=True)
    args = parser.parse_args()
    count = 0
    for target in json.loads(args.targets.read_text()):
        if target['type'] != 'executable':
            continue
        for filename in target['filename']:
            binary = Path(filename)
            def inspect(option):
                return subprocess.check_output(
                    [args.readelf, '-W', option, str(binary)], text=True)
            header, program, dynamic = inspect('-h'), inspect('-l'), inspect('-d')
            checks = {
                'PIE': bool(re.search(r'Type:\s+DYN', header)),
                'nonexecutable_stack': any(
                    'GNU_STACK' in line and not re.search(r'\bRW?E\b', line)
                    for line in program.splitlines()),
                'RELRO': 'GNU_RELRO' in program,
                'immediate_binding': bool(re.search(r'BIND_NOW|FLAGS.*NOW', dynamic)),
            }
            assert all(checks.values()), f'{binary}: {checks}'
            print(f'{target["name"]}: PIE, NX stack, RELRO and immediate binding')
            count += 1
    assert count, 'no executable targets were inspected'
    print(f'ELF hardening passed for {count} linked C executables')


if __name__ == '__main__':
    main()
