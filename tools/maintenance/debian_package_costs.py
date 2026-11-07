#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License, version 2 or later.
"""Parse an installed, single-architecture Debian package-cost snapshot.

Input is a complete dpkg-query snapshot with nine tab-separated fields:
binary:Package, Version, Installed-Size, Depends, Pre-Depends, Provides, Status,
Architecture and Multi-Arch. Supply the separately captured native architecture.
Foreign architectures and source-package architecture/profile restrictions are
unsupported and refused. Historical seven-field captures lack the needed facts.

For each Depends/Pre-Depends group, select the first satisfiable declared
alternative. Prefer its installed real package, otherwise require one installed
provider. Record that selection: it is a reproducible installed satisfier closure,
not a claim about the package manager's original or unique installation plan.
Recommends, Suggests and package files outside this database are not included.
"""
import re


MAX_BYTES = 16 * 1024 * 1024
MAX_FIELD = 65536
MAX_PACKAGES = 4096
MAX_RELATIONS = 32768
MAX_CHECKS = 262144
MAX_SIZE_KIB = ((1 << 63) - 1) // 1024
PACKAGE = r'[a-z0-9][a-z0-9+.-]{0,254}'
ARCHITECTURE = re.compile(r'[a-z0-9][a-z0-9-]{0,63}')
BINARY_NAME = re.compile(r'(' + PACKAGE + r')(?::([a-z0-9-]+))?')
RELATION = re.compile(r'(' + PACKAGE + r')(?::([a-z0-9-]+))?'
    r'(?:\s*\((<<|<=|=|>=|>>)\s+([^\s()]+)\))?')
CONTROL = re.compile(r'[\x00-\x08\x0b-\x1f\x7f]')
RESOLUTION_POLICY = ('first satisfiable declared alternative; installed real '
    'package before a unique installed provider')


def _version_parts(value):
    if (not isinstance(value, str) or not value or len(value) > 256 or
            not value.isascii()):
        raise ValueError('invalid Debian version')
    epoch, body = '0', value
    if ':' in body:
        epoch, body = body.split(':', 1)
        if not re.fullmatch(r'[0-9]+', epoch):
            raise ValueError('invalid Debian version epoch')
    if '-' in body:
        upstream, revision = body.rsplit('-', 1)
    else:
        upstream, revision = body, '0'
    if (not re.fullmatch(r'[0-9][A-Za-z0-9.+:~\-]*', upstream) or
            not re.fullmatch(r'[A-Za-z0-9+.~]+', revision)):
        raise ValueError('invalid Debian version components')
    return epoch, upstream, revision


def _numeric_compare(left, right):
    left, right = left.lstrip('0'), right.lstrip('0')
    if len(left) != len(right):
        return 1 if len(left) > len(right) else -1
    return (left > right) - (left < right)


def _order(character):
    if character == '~':
        return -1
    if not character or character.isdigit():
        return 0
    if character.isalpha():
        return ord(character)
    return ord(character) + 256


def _fragment_compare(left, right):
    a = b = 0
    while a < len(left) or b < len(right):
        while ((a < len(left) and not left[a].isdigit()) or
                (b < len(right) and not right[b].isdigit())):
            ca = left[a] if a < len(left) else ''
            cb = right[b] if b < len(right) else ''
            if _order(ca) != _order(cb):
                return 1 if _order(ca) > _order(cb) else -1
            a += bool(ca)
            b += bool(cb)
        start_a, start_b = a, b
        while a < len(left) and left[a].isdigit():
            a += 1
        while b < len(right) and right[b].isdigit():
            b += 1
        difference = _numeric_compare(left[start_a:a], right[start_b:b])
        if difference:
            return difference
    return 0


def compare_versions(left, right):
    """Return -1, 0 or 1 under Debian epoch/upstream/revision ordering."""
    ea, ua, ra = _version_parts(left)
    eb, ub, rb = _version_parts(right)
    return (_numeric_compare(ea, eb) or _fragment_compare(ua, ub) or
        _fragment_compare(ra, rb))


def _satisfies(actual, operator, expected):
    if operator is None:
        return True
    if actual is None:
        return False
    difference = compare_versions(actual, expected)
    return {'<<': difference < 0, '<=': difference <= 0,
        '=': difference == 0, '>=': difference >= 0,
        '>>': difference > 0}[operator]


def _relation(text, provides=False):
    match = RELATION.fullmatch(text)
    if match is None:
        raise ValueError('unsupported or malformed Debian relation: ' + text)
    name, qualifier, operator, version = match.groups()
    if provides and (qualifier is not None or operator not in (None, '=')):
        raise ValueError('unsupported Debian Provides relation')
    if version is not None:
        _version_parts(version)
    return name, qualifier, operator, version


def _groups(text, budget, provides=False):
    if not text:
        return []
    groups = []
    for group in text.split(','):
        alternatives = group.split('|')
        if provides and len(alternatives) != 1:
            raise ValueError('alternatives are invalid in Provides')
        budget[0] += len(alternatives)
        if budget[0] > MAX_RELATIONS:
            raise ValueError('Debian relation count limit exceeded')
        groups.append([(part.strip(), _relation(part.strip(), provides))
            for part in alternatives])
    return groups


def _eligible(row, qualifier, native_architecture):
    if qualifier == 'any':
        # The collector supplies this field explicitly. Do not infer permission
        # from an unqualified binary:Package spelling or installed architecture.
        return row['multiarch'] == 'allowed'
    if qualifier not in (None, 'native', native_architecture):
        raise ValueError('unsupported foreign dependency architecture')
    return row['architecture'] in (native_architecture, 'all')


def parse_snapshot(text, native_architecture):
    """Return a validated package_costs graph and its resolution provenance.

    All rows must be installed. Only native-architecture and architecture-all
    packages are supported. :any requires explicit Multi-Arch: allowed; other
    :any combinations are refused rather than guessed. Sizes are KiB
    metadata converted to bytes, not measured disk usage or incremental installs.
    """
    if (not isinstance(text, str) or not text or len(text) > MAX_BYTES or
            not text.isascii() or not text.endswith('\n') or
            CONTROL.search(text) is not None):
        raise ValueError('invalid, truncated or oversized Debian snapshot')
    if (not isinstance(native_architecture, str) or
            ARCHITECTURE.fullmatch(native_architecture) is None or
            native_architecture in ('all', 'any', 'native')):
        raise ValueError('invalid native Debian architecture')
    rows = {}
    by_name = {}
    providers = {}
    relation_budget = [0]
    for line in text.splitlines():
        fields = line.split('\t')
        if len(fields) != 9 or any(len(field) > MAX_FIELD for field in fields):
            raise ValueError('Debian snapshot needs nine bounded fields')
        (binary, version, size, depends, predepends, provides, status,
            architecture, multiarch) = fields
        match = BINARY_NAME.fullmatch(binary)
        if (match is None or status != 'install ok installed' or
                architecture not in (native_architecture, 'all') or
                multiarch not in ('', 'no', 'same', 'foreign', 'allowed')):
            raise ValueError('invalid or unsupported Debian package metadata')
        name, qualifier = match.groups()
        if ((qualifier is not None and qualifier != architecture) or
                (architecture == 'all' and
                    (qualifier is not None or multiarch == 'same'))):
            raise ValueError('binary package architecture mismatch')
        if binary in rows or name in by_name:
            raise ValueError('duplicate installed Debian package')
        if len(rows) >= MAX_PACKAGES:
            raise ValueError('Debian package count limit exceeded')
        _version_parts(version)
        if (not re.fullmatch(r'0|[1-9][0-9]{0,15}', size) or
                int(size) > MAX_SIZE_KIB):
            raise ValueError('invalid Debian Installed-Size')
        row = dict(version=version, flat_bytes=int(size) * 1024,
            architecture=architecture, multiarch=multiarch)
        row['groups'] = [('Depends', group)
            for group in _groups(depends, relation_budget)]
        row['groups'].extend(('Pre-Depends', group)
            for group in _groups(predepends, relation_budget))
        # Validate even the alternatives that a prior satisfier may bypass.
        for _, group in row['groups']:
            for _, (_, qualifier, _, _) in group:
                if qualifier not in (None, 'any', 'native', native_architecture):
                    raise ValueError('unsupported foreign dependency architecture')
        for group in _groups(provides, relation_budget, provides=True):
            _, (provided, _, _, provided_version) = group[0]
            providers.setdefault(provided, []).append((binary, provided_version))
        rows[binary] = row
        by_name[name] = binary

    packages = {binary: dict(version=row['version'],
        flat_bytes=row['flat_bytes'], dependencies={})
        for binary, row in rows.items()}
    resolutions = {}
    checks = 0
    for binary, row in rows.items():
        resolutions[binary] = []
        for field, group in row['groups']:
            selected = None
            for original, (name, qualifier, operator, version) in group:
                if qualifier not in (None, 'any', 'native', native_architecture):
                    raise ValueError('unsupported foreign dependency architecture')
                real = by_name.get(name)
                checks += 1
                if (real is not None and
                        _eligible(rows[real], qualifier, native_architecture) and
                        _satisfies(rows[real]['version'], operator, version)):
                    selected = dict(alternative=original, package=real,
                        via='package', provided_version=None)
                else:
                    candidates = {}
                    for owner, provided_version in providers.get(name, []):
                        checks += 1
                        if checks > MAX_CHECKS:
                            raise ValueError('Debian resolution work limit exceeded')
                        if (_eligible(rows[owner], qualifier, native_architecture) and
                                _satisfies(provided_version, operator, version)):
                            candidates.setdefault(owner, provided_version)
                            if len(candidates) > 1:
                                raise ValueError('ambiguous provider for ' + original)
                    if candidates:
                        owner, provided_version = next(iter(candidates.items()))
                        selected = dict(alternative=original, package=owner,
                            via='Provides', provided_version=provided_version)
                if checks > MAX_CHECKS:
                    raise ValueError('Debian resolution work limit exceeded')
                if selected is not None:
                    break
            if selected is None:
                raise ValueError('unsatisfied dependency for ' + binary + ': ' +
                    ' | '.join(part for part, _ in group))
            owner = selected['package']
            packages[binary]['dependencies'][owner] = rows[owner]['version']
            resolutions[binary].append(dict(field=field,
                group=' | '.join(part for part, _ in group), **selected))
    return dict(packages=packages, resolutions=resolutions,
        native_architecture=native_architecture,
        architecture_scope='native architecture and architecture-all only',
        resolution_policy=RESOLUTION_POLICY,
        size_scope='Installed-Size metadata in KiB converted to bytes')
