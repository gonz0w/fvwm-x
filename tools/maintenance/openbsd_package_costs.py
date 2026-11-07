#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License, version 2 or later.
"""Validate a captured OpenBSD installed graph and package size estimates.

The collector supplies +CONTENTS, +REQUIRING and +REQUIRED_BY without executing
packing-list instructions. Dependencies use the installed package database's
resolved names, not a reimplementation of OpenBSD package-pattern matching.
Sizes are the integer estimates returned by pkg_info -q -s for each package.
"""
import re

from package_costs import MAX_DEPENDENCIES, MAX_PACKAGES, NAME, compare

MAX_BYTES = 16 * 1024 * 1024
MAX_PACKAGE_BYTES = 2 * 1024 * 1024
FIELDS = {'contents', 'requiring', 'required_by', 'size'}


def _names(text):
    if text is None:
        return set()
    if (not isinstance(text, str) or text.count('\n') > MAX_PACKAGES):
        raise ValueError('dependency metadata must be text or absent')
    names = text.splitlines()
    if (len(names) > MAX_PACKAGES or len(set(names)) != len(names) or
            any(not NAME.fullmatch(name) for name in names)):
        raise ValueError('invalid or duplicate installed dependency name')
    return set(names)


def parse_snapshot(snapshot):
    """Return an exact-name graph, refusing incomplete or inconsistent records.

    Full installed package identifiers serve as both keys and version identities.
    Missing dependency files mean empty sets, as in OpenBSD::RequirementList;
    missing size or packing-list captures never mean a zero-cost package.
    Forward and reverse edges must agree across the entire captured snapshot.
    """
    if (not isinstance(snapshot, dict) or not snapshot or
            len(snapshot) > MAX_PACKAGES):
        raise ValueError('invalid package snapshot')
    total = edges = 0
    packages, forward, reverse = {}, {}, {}
    for name, row in snapshot.items():
        if (not isinstance(name, str) or not NAME.fullmatch(name) or
                not isinstance(row, dict) or set(row) != FIELDS):
            raise ValueError('invalid package identifier or metadata fields')
        for key, value in row.items():
            if value is None and key in {'requiring', 'required_by'}:
                continue
            if (not isinstance(value, str) or len(value) > MAX_PACKAGE_BYTES or
                    '\x00' in value or '\r' in value or
                    len(value.encode('utf-8')) > MAX_PACKAGE_BYTES):
                raise ValueError('invalid package metadata type or byte limit')
            total += len(value.encode('utf-8'))
            if total > MAX_BYTES:
                raise ValueError('snapshot byte limit exceeded')
        contents = row['contents'].splitlines()
        if [line for line in contents if line.startswith('@name ')] != [
                '@name ' + name]:
            raise ValueError('packing-list package identity mismatch')
        if not re.fullmatch(r'[0-9]{1,20}\n', row['size']):
            raise ValueError('missing or malformed package size estimate')
        forward[name] = _names(row['requiring'])
        reverse[name] = _names(row['required_by'])
        if (any(line.startswith('@depend ') for line in contents) and
                not forward[name]):
            raise ValueError('declared dependency without installed resolution')
        edges += len(forward[name])
        if edges > MAX_DEPENDENCIES:
            raise ValueError('dependency edge limit exceeded')
        packages[name] = dict(version=name, flat_bytes=int(row['size']),
            dependencies={dep: dep for dep in sorted(forward[name])})
    for name in packages:
        for dependency in forward[name]:
            if dependency not in packages or name not in reverse[dependency]:
                raise ValueError('missing package or inconsistent forward edge')
        for dependent in reverse[name]:
            if dependent not in packages or name not in forward[dependent]:
                raise ValueError('missing package or inconsistent reverse edge')
    # Apply the shared token/size/whole-graph contract before returning a result.
    compare(packages, [], [])
    return packages
