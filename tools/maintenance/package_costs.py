#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License, version 2 or later.
"""Compare complete dependency closures using nominal package flat sizes.

The caller supplies an already parsed package snapshot with exact versions.
The returned bytes describe package metadata for that snapshot. The caller
retains the provenance and accounts for files outside the package database.
"""
import re


MAX_PACKAGES = 4096
MAX_DEPENDENCIES = 32768
NAME = re.compile(r'[A-Za-z0-9][A-Za-z0-9+_.:-]{0,255}')
VERSION = re.compile(r'[A-Za-z0-9][A-Za-z0-9+_.:,~\-]{0,255}')


def _token(value, pattern):
    return isinstance(value, str) and pattern.fullmatch(value) is not None


def _validate(packages):
    if not isinstance(packages, dict) or len(packages) > MAX_PACKAGES:
        raise ValueError('package snapshot must be a bounded dictionary')
    edges = 0
    for name, row in packages.items():
        if (not _token(name, NAME) or not isinstance(row, dict) or
                set(row) != {'version', 'flat_bytes', 'dependencies'}):
            raise ValueError('invalid package name or metadata fields')
        if (not _token(row['version'], VERSION) or
                type(row['flat_bytes']) is not int or row['flat_bytes'] < 0 or
                not isinstance(row['dependencies'], dict)):
            raise ValueError('invalid package version, size or dependencies')
        edges += len(row['dependencies'])
        if edges > MAX_DEPENDENCIES:
            raise ValueError('package dependency edge limit exceeded')
    # Validate the entire snapshot before permitting a zero difference result.
    for row in packages.values():
        for dependency, version in row['dependencies'].items():
            if (not _token(dependency, NAME) or not _token(version, VERSION) or
                    dependency not in packages):
                raise ValueError('missing or invalid package dependency')
            if packages[dependency]['version'] != version:
                raise ValueError('package dependency version mismatch')


def _closure(packages, roots):
    if (not isinstance(roots, (list, tuple, set, frozenset)) or
            len(roots) > MAX_PACKAGES):
        raise ValueError('package roots must be a bounded collection')
    if any(not _token(name, NAME) or name not in packages for name in roots):
        raise ValueError('missing or invalid package root')
    visited = set()
    pending = list(roots)
    while pending:
        name = pending.pop()
        if name in visited:
            continue
        visited.add(name)
        pending.extend(packages[name]['dependencies'])
    return visited


def compare(packages, baseline_roots, candidate_roots):
    """Return deduplicated closures and package additions/removals in bytes.

    Each package row has version, flat_bytes and dependencies fields. Dependency
    keys name packages and their values give the exact required versions.
    Missing or inconsistent metadata raises ValueError, including for packages
    outside the selected roots. Cycles and repeated roots are allowed.
    """
    _validate(packages)
    baseline = _closure(packages, baseline_roots)
    candidate = _closure(packages, candidate_roots)
    added = candidate - baseline
    removed = baseline - candidate
    baseline_bytes = sum(packages[name]['flat_bytes'] for name in baseline)
    candidate_bytes = sum(packages[name]['flat_bytes'] for name in candidate)
    return dict(size_scope='nominal package flat sizes',
        baseline_packages=sorted(baseline), candidate_packages=sorted(candidate),
        added_packages=sorted(added), removed_packages=sorted(removed),
        baseline_flat_bytes=baseline_bytes, candidate_flat_bytes=candidate_bytes,
        added_flat_bytes=sum(packages[name]['flat_bytes'] for name in added),
        removed_flat_bytes=sum(packages[name]['flat_bytes'] for name in removed),
        net_flat_bytes=candidate_bytes - baseline_bytes)
