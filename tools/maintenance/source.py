#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or any later version.
"""Read-only source inventory and deterministic committed-source archives."""
import argparse
import gzip
import hashlib
import json
import os
import re
from pathlib import Path, PurePosixPath
import stat
import subprocess
import sys
import tarfile
import tempfile

from common import ROOT, fresh_output, source_file

# Release exclusions only: originals and historical evidence remain in Git.
EXCLUDE = {'.git', '.github', '.planning', '.gsd', '.agents', '.codex',
           'dev-docs', 'AGENTS.md', '.disabled-travis.yml', '.DS_Store',
           '.clang-format', '.editorconfig', '.gitignore', '.mailmap'}
DEPENDENCIES = ['x11', 'xrandr', 'xext', 'xft', 'fontconfig', 'xt', 'ice',
                'xtrans', 'libevent', 'libpng', 'librsvg-2.0',
                'cairo', 'freetype2', 'fribidi', 'sm', 'xcursor', 'xfixes',
                'xpm', 'xrender']


def file_identity(path):
    """Hash regular input with fixed-size reads, including large assets."""
    digest = hashlib.sha256()
    size = 0
    with path.open('rb') as stream:
        for chunk in iter(lambda: stream.read(128 * 1024), b''):
            size += len(chunk)
            digest.update(chunk)
    return {'bytes': size, 'sha256': digest.hexdigest()}


def go_metadata(root, name):
    """Observe directive text; leave resolution/grammar checks to Go tooling."""
    path = source_file(root, name)
    result = {'path': name, **file_identity(path), 'directives': [],
              'note': 'Text observation, not a resolved module graph or Go validation.'}
    block = None
    for line, text in enumerate(path.read_text().splitlines(), 1):
        text = text.strip()
        if not text or text.startswith('//'):
            continue
        if block:
            if text.split('//', 1)[0].strip() == ')':
                block = None
            else:
                result['directives'].append({'kind': block, 'text': text,
                                             'line': line})
            continue
        match = re.match(r'^(module|go|toolchain|require|replace|exclude)\s+(.+)$', text)
        if match:
            kind, value = match.groups()
            if value.split('//', 1)[0].strip() == '(':
                block = kind
            else:
                result['directives'].append({'kind': kind, 'text': value,
                                             'line': line})
    if block:
        result['unfinished_block'] = block
    result['companions'] = []
    for suffix in ('go.sum', 'vendor/modules.txt'):
        companion = str(Path(name).parent / suffix)
        if (root / companion).exists() or (root / companion).is_symlink():
            result['companions'].append({'path': companion,
                                         **file_identity(source_file(root, companion))})
    return result


def git(root, *args):
    return subprocess.check_output(['git', '-C', str(root), *args])


def maintenance_metadata(root):
    """Text inventory, not a YAML parser or a vulnerability verdict."""
    result = {'vendored': {}, 'workflow_references': []}
    header = root / 'libs/cJSON.h'
    if header.is_file():
        text = source_file(root, 'libs/cJSON.h').read_text()
        numbers = [re.search(r'^#define CJSON_VERSION_' + part + r'\s+(\d+)', text, re.M)
                   for part in ('MAJOR', 'MINOR', 'PATCH')]
        if all(numbers):
            result['vendored']['cJSON'] = '.'.join(match.group(1) for match in numbers)
    gomod = root / 'bin/FvwmPrompt/go.mod'
    if gomod.is_file():
        result['go_manifest'] = go_metadata(root, 'bin/FvwmPrompt/go.mod')
        result['vendored']['go_modules'] = dict(re.findall(
            r'^(?:\s*require\s+|\s*)([\w./-]+)\s+(v[0-9][^\s]+)', source_file(root, 'bin/FvwmPrompt/go.mod').read_text(), re.M))
    workflows = root / '.github/workflows'
    if workflows.is_dir():
        for path in sorted(workflows.glob('*.y*ml')):
            for line, text in enumerate(source_file(root, path.relative_to(root)).read_text().splitlines(), 1):
                match = re.match(r'^\s*(?:-\s*)?uses:\s*([^\s#]+)', text)
                if match:
                    reference = match.group(1).strip("\"'")
                    pinned = bool(re.search(r'@[0-9a-f]{40}$', reference))
                    result['workflow_references'].append({
                        'path': path.relative_to(root).as_posix(), 'line': line,
                        'uses': reference, 'full_commit_pin': pinned,
                        'note': 'Text reference only; review reusable/local/container actions separately.'})
    return result


def inventory(root, untracked=False, dependencies=False):
    root = Path(root).resolve()
    args = ['ls-files', '-z', '--cached']
    if untracked:
        args += ['--others', '--exclude-standard']
    paths = sorted(set(os.fsdecode(name) for name in git(root, *args).split(b'\0') if name))
    entries = []
    for name in paths:
        path = root / name
        try:
            info = path.lstat()
        except FileNotFoundError:
            entries.append({'path': name, 'state': 'deleted'})
            continue
        if path.is_symlink():
            data = os.fsencode(os.readlink(path))
            kind = 'symlink'
            identity = {'bytes': len(data), 'sha256': hashlib.sha256(data).hexdigest()}
        elif stat.S_ISREG(info.st_mode):
            # Refuse parent-directory links before reading file data.
            if root not in path.resolve().parents:
                raise ValueError(f'source escapes checkout: {name}')
            identity = file_identity(path)
            kind = 'file'
        else:
            raise ValueError(f'unsupported source type: {name}')
        entries.append({'path': name, 'kind': kind, **identity,
                        'mode': oct(stat.S_IMODE(info.st_mode))})
    result = {'schema': 1, 'commit': git(root, 'rev-parse', 'HEAD').decode().strip(),
              'status': git(root, 'status', '--porcelain=v1', '-z').decode('utf-8', 'surrogateescape'),
              'files': entries,
              'note': 'Working-tree observation, not an atomic release snapshot.',
              'maintenance': maintenance_metadata(root)}
    if dependencies:
        result['pkg_config'] = {}
        result['pkg_config_available'] = True
        for name in DEPENDENCIES:
            try:
                check = subprocess.run(['pkg-config', '--modversion', name],
                                       capture_output=True, text=True)
            except FileNotFoundError:
                result['pkg_config_available'] = False
                result['pkg_config'] = {name: None for name in DEPENDENCIES}
                break
            result['pkg_config'][name] = check.stdout.strip() if check.returncode == 0 else None
    return result


def safe_member(member):
    name = PurePosixPath(member.name)
    if name.is_absolute() or '..' in name.parts or not name.parts:
        raise ValueError(f'unsafe archive path: {member.name}')
    if not (member.isfile() or member.isdir() or member.issym()):
        raise ValueError(f'unsupported archive type: {member.name}')
    if member.issym():
        target = PurePosixPath(member.linkname)
        if target.is_absolute() or '..' in target.parts:
            raise ValueError(f'unsafe archive link: {member.name}')
    return name


def archive(root, output, ref='HEAD', epoch=0, allow_dirty=False):
    if epoch < 0:
        raise ValueError('archive epoch must be nonnegative')
    if git(root, 'status', '--porcelain=v1', '-z') and not allow_dirty:
        raise ValueError('dirty checkout: commit a reviewed snapshot first; --allow-dirty is for development archives only')
    commit = git(root, 'rev-parse', '--verify', ref + '^{commit}').decode().strip()
    with fresh_output(output, root) as stage, tempfile.TemporaryFile() as raw:
        # Git writes to disk, rather than retaining the complete tar in RAM.
        # Check completion before opening it or publishing the fresh output.
        subprocess.run(['git', '-C', str(root), 'archive', '--format=tar', commit],
                       stdout=raw, check=True)
        raw.seek(0)
        target = stage / 'source.tar.gz'
        with tarfile.open(fileobj=raw, mode='r:') as source:
            with target.open('wb') as stream, gzip.GzipFile(filename='', fileobj=stream, mode='wb', mtime=0) as zipped:
                with tarfile.open(fileobj=zipped, mode='w', format=tarfile.PAX_FORMAT) as dest:
                    for member in sorted(source.getmembers(), key=lambda item: item.name):
                        name = safe_member(member)
                        if name.parts[0] in EXCLUDE or '.DS_Store' in name.parts:
                            continue
                        # Git archive's default tar.umask can emit group-writable
                        # modes; normalize permissions while retaining executability.
                        member.mode = (0o755 if member.isdir() or
                                       (member.isfile() and member.mode & 0o111)
                                       else 0o644 if member.isfile() else 0o777)
                        member.uid = member.gid = 0
                        member.uname = member.gname = ''
                        member.mtime = epoch
                        member.pax_headers = {}
                        dest.addfile(member, source.extractfile(member) if member.isfile() else None)
        (stage / 'archive.json').write_text(json.dumps(
            {'schema': 1, 'commit': commit, 'epoch': epoch,
             **file_identity(target),
             'excluded_top_level': sorted(EXCLUDE),
             'development_only': allow_dirty}, indent=2) + '\n')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('kind', choices=['inventory', 'archive'])
    parser.add_argument('--source', type=Path, default=ROOT)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--untracked', action='store_true')
    parser.add_argument('--dependencies', action='store_true')
    parser.add_argument('--ref', default='HEAD')
    parser.add_argument('--epoch', type=int, default=0)
    parser.add_argument('--allow-dirty', action='store_true')
    args = parser.parse_args()
    if args.epoch < 0:
        parser.error('--epoch must be nonnegative')
    if args.kind == 'archive':
        archive(args.source, args.output, args.ref, args.epoch, args.allow_dirty)
    else:
        result = inventory(args.source, args.untracked, args.dependencies)
        with fresh_output(args.output, args.source) as stage:
            (stage / 'inventory.json').write_text(json.dumps(result, indent=2, ensure_ascii=True) + '\n')


if __name__ == '__main__':
    try:
        main()
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        print(str(error), file=sys.stderr)
        sys.exit(1)
