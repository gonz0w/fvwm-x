#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or any later version.
"""Validate the required Go compiler without fetching a toolchain."""
import argparse
import re

REQUIREMENT = 'stable Go 1.26.9 in the 1.26 series, or Go 1.27.2 or newer'
VERSION = re.compile(r'go([1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\Z')


def supported_version(value):
    """Return a normalized stable version, refusing unpatched/prerelease SDKs."""
    match = VERSION.fullmatch(value.strip())
    if match is None:
        raise ValueError('compiler did not report a stable Go version')
    version = tuple(int(part) for part in match.groups())
    if not ((1, 26, 9) <= version < (1, 27, 0) or version >= (1, 27, 2)):
        raise ValueError('compiler is older than the patched baseline')
    return '.'.join(str(part) for part in version)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('version', help='the selected compiler env GOVERSION')
    args = parser.parse_args()
    try:
        print(supported_version(args.version))
    except ValueError as error:
        parser.exit(1, 'FVWM-X builds require ' + REQUIREMENT + '.\n'
                    + str(error) + ': ' + repr(args.version) + '\n'
                    'Install or select a supported compiler; automatic toolchain '
                    'downloads are disabled.\n')


if __name__ == '__main__':
    main()
