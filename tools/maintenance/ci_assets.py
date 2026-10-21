#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or any later version.
"""Materialize pinned CI inputs; never execute or decompress downloaded files."""
import argparse
import contextlib
import hashlib
import json
import os
from pathlib import Path
import re
import stat
import subprocess
import sys
import tempfile
from urllib.parse import urlsplit
from urllib.request import HTTPRedirectHandler, build_opener

from common import ROOT, fresh_output

PINS = ROOT / 'dev-docs/ci/ASSET-PINS-2026-10-20.json'
BOOT_PINS = ROOT / 'dev-docs/ci/BOOT-PINS-2026-10-20.json'
# Approved upstream release, independently checked against libpng.org.
PNG_SOURCE = {
    'name': 'libpng-1.6.59.tar.xz',
    'url': 'https://download.sourceforge.net/libpng/libpng-1.6.59.tar.xz',
    'size': 1074540,
    'sha256': 'd80dd2a38a37f803cb9b6ac7b14bd6e74ddc3b654780a8380bdf93523fdb4389',
}
PROFILES = {
    'freebsd-amd64': ('freebsd-builder', '2.2.8', 'freebsd-15.1'),
    'freebsd-arm64': ('freebsd-builder', '2.2.8', 'freebsd-15.1-aarch64'),
    'openbsd-amd64': ('openbsd-builder', '2.1.1', 'openbsd-8.0'),
    'openbsd-arm64': ('openbsd-builder', '2.1.1', 'openbsd-8.0-aarch64'),
}
CHUNK = 1024 * 1024
MAX_ASSET = 2 * 1024 * 1024 * 1024


class HTTPSRedirects(HTTPRedirectHandler):
    def redirect_request(self, request, response, code, message, headers, url):
        if urlsplit(url).scheme != 'https':
            raise ValueError('download redirected outside HTTPS')
        return super().redirect_request(request, response, code, message,
                                        headers, url)


def load_lock(path, expected):
    with open(path, 'rb') as stream:
        raw = stream.read(CHUNK + 1)
    if len(raw) > CHUNK:
        raise ValueError('asset lock exceeds 1 MiB')
    lock = json.loads(raw)
    assets = {}
    for asset in lock['assets']:
        name = asset['name']
        if name not in expected or name in assets:
            raise ValueError('unexpected or duplicate locked asset')
        repository, release = expected[name]
        url = ('https://github.com/anyvm-org/' + repository +
               '/releases/download/v' + release + '/' + name)
        digest = asset['download_sha256']
        size = asset['size']
        if (asset['repository'] != 'anyvm-org/' + repository or
                asset['release'] != release or asset['url'] != url or
                asset['verified_download'] is not True or
                not isinstance(digest, str) or
                re.fullmatch('[0-9a-f]{64}', digest) is None or
                asset['publisher_digest'] != 'sha256:' + digest or
                type(size) is not int or not 0 < size <= MAX_ASSET):
            raise ValueError('inconsistent locked asset: ' + name)
        assets[name] = {'name': name, 'url': url, 'size': size,
                        'sha256': digest}
    if assets.keys() != expected.keys():
        raise ValueError('incomplete asset lock')
    return assets, hashlib.sha256(raw).hexdigest()


def load_assets(path, profile):
    if profile == 'png-source':
        raw = json.dumps(PNG_SOURCE, sort_keys=True).encode()
        return [dict(PNG_SOURCE)], hashlib.sha256(raw).hexdigest()
    expected = {'anyvm.py': ('anyvm', '0.7.2')}
    for repository, release, stem in PROFILES.values():
        for suffix in ('.profile.json', '.qcow2.zst', '.qemu'):
            expected[stem + suffix] = (repository, release)
    assets, digest = load_lock(path, expected)
    if profile == 'all':
        selected = list(assets.values())
    else:
        stem = PROFILES[profile][2]
        selected = [assets['anyvm.py']] + [assets[stem + suffix] for
                    suffix in ('.profile.json', '.qcow2.zst', '.qemu')]
    return selected, digest


def load_boot_assets(path, profile):
    expected = {stem + '-host.id_rsa': (repository, release) for
                repository, release, stem in PROFILES.values()}
    firmware = 'QEMU_EFI-2024.02-2ubuntu0.9.fd'
    expected[firmware] = ('firmware', '0.0.2')
    assets, digest = load_lock(path, expected)
    if profile == 'all':
        return list(assets.values()), digest
    selected = [assets[PROFILES[profile][2] + '-host.id_rsa']]
    if profile.endswith('arm64'):
        selected.append(assets[firmware])
    return selected, digest


def checked_copy(source, destination, asset):
    """Hash the same descriptor whose bytes reach the private output file."""
    digest = hashlib.sha256()
    size = 0
    with destination.open('xb') as out:
        os.chmod(destination, 0o600)
        while True:
            data = source.read(min(CHUNK, asset['size'] - size + 1))
            if not data:
                break
            size += len(data)
            if size > asset['size']:
                raise ValueError('asset exceeds locked size: ' + asset['name'])
            digest.update(data)
            out.write(data)
    if size != asset['size'] or digest.hexdigest() != asset['sha256']:
        raise ValueError('asset size/hash mismatch: ' + asset['name'])


@contextlib.contextmanager
def private_cache(path):
    # Parent paths are caller-owned workspace paths. Pin the final directory;
    # cache entries are opened relative to this FD, never through a symlink.
    path.mkdir(parents=True, mode=0o700, exist_ok=True)
    fd = os.open(path, os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW)
    try:
        info = os.fstat(fd)
        if info.st_uid != os.getuid() or stat.S_IMODE(info.st_mode) != 0o700:
            raise ValueError('cache must be owned by this user with mode 0700')
        yield fd
    finally:
        os.close(fd)


def open_cached(fd, asset):
    try:
        entry = os.open(asset['sha256'], os.O_RDONLY | os.O_NOFOLLOW |
                        os.O_NONBLOCK, dir_fd=fd)
    except FileNotFoundError:
        return None
    try:
        info = os.fstat(entry)
        if (not stat.S_ISREG(info.st_mode) or info.st_uid != os.getuid() or
                info.st_nlink != 1 or info.st_mode & 0o077 or
                info.st_size != asset['size']):
            raise ValueError('unsafe or wrong-sized cache entry: ' + asset['name'])
        stream = os.fdopen(entry, 'rb')
        entry = -1
        return stream
    finally:
        if entry != -1:
            os.close(entry)


def download(asset, destination, timeout):
    # DNS/connect/slow-body stalls are bounded by the parent process deadline,
    # not just urllib's per-operation timeout. The worker spawns no children.
    result = subprocess.run([sys.executable, str(Path(__file__).resolve()),
        '--download-worker', asset['url'], str(asset['size']), asset['sha256'],
        str(destination)], timeout=timeout, capture_output=True)
    if result.returncode != 0:
        raise ValueError('download verification failed: ' + asset['name'])


def materialize(assets, lock_hash, profile, cache, destination,
                offline=False, timeout=300):
    if type(timeout) is not int or not 1 <= timeout <= 300:
        raise ValueError('download timeout must be between 1 and 300 seconds')
    rows = []
    with private_cache(cache) as cache_fd:
        with fresh_output(destination, ROOT) as stage:
            for asset in assets:
                out = stage / asset['name']
                stream = open_cached(cache_fd, asset)
                if stream is not None:
                    with stream:
                        checked_copy(stream, out, asset)
                    origin = 'verified-cache'
                else:
                    if offline:
                        raise ValueError('missing offline asset: ' + asset['name'])
                    download(asset, out, timeout)
                    # Publish immutable cache content after verification. A
                    # concurrent cache entry is never replaced or trusted here.
                    # Prepare an independent cache inode before publication;
                    # the consumer's output can never mutate that input.
                    private = stage / (asset['name'] + '.copy')
                    with out.open('rb') as source:
                        checked_copy(source, private, asset)
                    try:
                        os.link(private, asset['sha256'], dst_dir_fd=cache_fd)
                    except FileExistsError:
                        with open_cached(cache_fd, asset) as source:
                            check = stage / (asset['name'] + '.cache-check')
                            checked_copy(source, check, asset)
                        check.unlink()
                    finally:
                        private.unlink()
                    origin = 'verified-download'
                rows.append(dict(asset, origin=origin, mode='0600'))
            (stage / 'manifest.json').write_text(json.dumps({
                'profile': profile, 'lock_sha256': lock_hash,
                'status': 'verified inputs; no extraction, execution or boot',
                'assets': rows}, indent=2) + '\n')
            os.chmod(stage / 'manifest.json', 0o600)
    return rows


def main():
    if len(sys.argv) > 1 and sys.argv[1] == '--download-worker':
        url, size, digest, destination = sys.argv[2:]
        asset = {'name': Path(destination).name, 'size': int(size),
                 'sha256': digest}
        allowed = (load_assets(PINS, 'all')[0] +
                   load_boot_assets(BOOT_PINS, 'all')[0] + [PNG_SOURCE])
        if (url not in {item['url'] for item in allowed} or
                not 0 < asset['size'] <= MAX_ASSET or
                re.fullmatch('[0-9a-f]{64}', digest) is None):
            raise ValueError('invalid acquisition worker input')
        if url == PNG_SOURCE['url'] and (asset['size'], digest) != (
                PNG_SOURCE['size'], PNG_SOURCE['sha256']):
            raise ValueError('PNG worker input differs from reviewed release')
        with build_opener(HTTPSRedirects()).open(url, timeout=15) as source:
            if not source.geturl().startswith('https://'):
                raise ValueError('download redirected outside HTTPS')
            checked_copy(source, Path(destination), asset)
        return
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--profile', choices=[*PROFILES, 'all', 'png-source'], required=True)
    parser.add_argument('--cache', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--offline', action='store_true')
    parser.add_argument('--timeout', type=int, default=300)
    args = parser.parse_args()
    assets, lock_hash = load_assets(PINS, args.profile)
    rows = materialize(assets, lock_hash, args.profile, args.cache,
                       args.output, args.offline, args.timeout)
    print('Verified %d pinned inputs; no files executed.' % len(rows))


if __name__ == '__main__':
    try:
        main()
    except (OSError, ValueError, KeyError, TypeError,
            subprocess.TimeoutExpired) as error:
        print('CI asset preflight failed: ' + str(error), file=sys.stderr)
        sys.exit(1)
