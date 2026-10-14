#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or any later version.
"""Meson wrapper with explicit installation and compatible ./build options."""
import argparse
import getopt
import os
from pathlib import Path
import shlex
import subprocess
import sys
import tempfile

from common import ROOT


def display_command(argv):
    # Keep the wrapper usable with Meson's Python 3.7 floor.
    return ' '.join(shlex.quote(item) for item in argv)


def legacy_arguments(argv):
    """Translate documented ./build options, retaining setup-option order."""
    options, remaining = getopt.getopt(
        argv, 'dfb:D:himp:vw',
        ['dry-run', 'test', 'jobs=', 'log=', 'meson=', 'source=', 'reconfigure'])
    if remaining:
        raise ValueError('unexpected build arguments: ' + ' '.join(remaining))
    translated = ['--builddir', 'compile']
    for option, value in options:
        if option == '-h':
            return None
        if option == '-b':
            translated += ['--builddir', value]
        elif option == '-p':
            translated += ['-D', 'prefix=' + value]
        elif option == '-D':
            translated += ['-D', value]
        elif option in ('-d', '-m', '-w'):
            if option in ('-d', '-m'):
                translated += ['-D', 'mandoc=true']
            if option in ('-d', '-w'):
                translated += ['-D', 'htmldoc=true']
        elif option == '-f':
            translated.append('--wipe')
        elif option == '-i':
            translated.append('--install')
        elif option == '-v':
            translated.append('--verbose')
        else:
            translated.append(option)
            if value:
                translated.append(value)
    return translated


def plan(args):
    source = args.source.resolve()
    builddir = args.builddir.resolve()
    if builddir == source or builddir in source.parents:
        raise ValueError('build directory must not contain the source tree')
    configured = (builddir / 'meson-private/coredata.dat').is_file()
    if builddir.exists() and not configured and any(builddir.iterdir()):
        raise ValueError('build directory is nonempty and is not a Meson build')
    commands = []
    if not configured or args.reconfigure or args.wipe or args.define or args.prefix:
        setup = [args.meson, 'setup']
        if configured:
            setup.append('--reconfigure')
            if args.wipe:
                setup.append('--wipe')
        setup += [str(builddir), str(source)]
        setup += ['-D' + option for option in args.define]
        if args.prefix:
            setup += ['--prefix', args.prefix]
        commands.append(setup)
    commands.append([args.meson, 'compile', '-C', str(builddir),
                     '-j', str(args.jobs)])
    if args.test:
        commands.append([args.meson, 'test', '-C', str(builddir),
                         '--print-errorlogs'])
    if args.install:
        commands.append([args.meson, 'install', '-C', str(builddir)])
    return commands


def run(argv, log, verbose):
    # Drain binary chunks rather than lines: a child without newlines cannot
    # accumulate an unbounded line, and binary compiler diagnostics survive.
    with subprocess.Popen(argv, stdout=subprocess.PIPE,
                          stderr=subprocess.STDOUT) as child:
        while True:
            chunk = child.stdout.read1(65536)
            if not chunk:
                break
            log.write(chunk)
            log.flush()
            if verbose:
                sys.stdout.buffer.write(chunk)
                sys.stdout.buffer.flush()
        status = child.wait()
    return 128 - status if status < 0 else status


def main():
    argv = sys.argv[1:]
    legacy = bool(argv and argv[0] == '--legacy')
    if legacy:
        argv = legacy_arguments(argv[1:])
        if argv is None:
            print('Usage: ./build [-b builddir] [-d] [-f] [-i] [-m] '
                  '[-p prefix] [-v] [-w] [-D option]\n'
                  '  -b directory (default: compile); -d both documentation formats\n'
                  '  -m manuals; -w HTML; -f explicit Meson wipe/reconfigure\n'
                  '  -i install; -p prefix; -v verbose; repeat -D for Meson options\n'
                  '  --dry-run, --test, --jobs N, --log PATH are also supported')
            return 1
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, default=ROOT)
    parser.add_argument('--builddir', type=Path, default=ROOT / 'builddir-maint/native')
    parser.add_argument('--meson', default='meson')
    parser.add_argument('-D', '--define', action='append', default=[])
    parser.add_argument('--prefix')
    parser.add_argument('--jobs', type=int, default=os.cpu_count() or 1)
    parser.add_argument('--reconfigure', action='store_true')
    parser.add_argument('--wipe', action='store_true',
                        help='explicitly wipe/reconfigure the selected Meson build')
    parser.add_argument('--test', action='store_true')
    parser.add_argument('--install', action='store_true')
    parser.add_argument('--verbose', action='store_true')
    parser.add_argument('--dry-run', action='store_true')
    parser.add_argument('--log', type=Path)
    args = parser.parse_args(argv)
    if args.jobs < 1:
        parser.error('--jobs must be positive')
    commands = plan(args)
    if args.dry_run:
        for command in commands:
            print(display_command(command))
        return 0
    if legacy and args.log is None:
        descriptor, name = tempfile.mkstemp(prefix='fvwm-build-', suffix='.log')
        os.close(descriptor)
        logpath = Path(name)
    else:
        logpath = args.log or args.builddir.parent / 'maintenance-build.log'
    logpath.parent.mkdir(parents=True, exist_ok=True)
    # Append keeps preceding evidence; opening happens before any build work.
    with logpath.open('ab') as log:
        print(f'Build output: {logpath.absolute()}', flush=True)
        for argv in commands:
            display = display_command(argv)
            print(display, flush=True)
            log.write(('\n$ ' + display + '\n').encode())
            if argv[1] == 'install':
                # Keep stdin/terminal for Meson's normal privilege handling.
                status = subprocess.call(argv)
                status = 128 - status if status < 0 else status
            else:
                status = run(argv, log, args.verbose)
            if status:
                print(f'Command failed ({status}); see {logpath}', file=sys.stderr)
                return status
    return 0


if __name__ == '__main__':
    try:
        sys.exit(main())
    except (OSError, ValueError, getopt.GetoptError) as error:
        print(str(error), file=sys.stderr)
        sys.exit(1)
    except KeyboardInterrupt:
        sys.exit(130)
