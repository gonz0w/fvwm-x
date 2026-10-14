#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or any later version.
"""Build the approved PNG input inside an owned Linux/BSD CI job."""
import argparse
import hashlib
import io
import json
import os
from pathlib import Path, PurePosixPath
import platform
import re
import resource
import shlex
import shutil
import signal
import subprocess
import sys
import tarfile
import time

import ci_assets

TAR_SIZE = 6880256
TAR_SHA256 = 'de225c673e1148bfa30f3fd16fcb4adf36507026ab60941ca9c9d7f95374f3d3'
MAX_LOG = 64 * 1024**2


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def prepare_source(source, destination):
    """Only the host needs xz support; transfer the exact verified tar payload."""
    with source.open('rb') as stream:
        raw = stream.read(ci_assets.PNG_SOURCE['size'] + 1)
    if (len(raw) != ci_assets.PNG_SOURCE['size'] or
            hashlib.sha256(raw).hexdigest() != ci_assets.PNG_SOURCE['sha256']):
        raise ValueError('PNG input differs from the approved release')
    import lzma
    with lzma.open(io.BytesIO(raw)) as stream:
        payload = stream.read(TAR_SIZE + 1)
    if (len(payload) != TAR_SIZE or
            hashlib.sha256(payload).hexdigest() != TAR_SHA256):
        raise ValueError('PNG tar payload differs from the approved release')
    with destination.open('xb') as stream:
        os.chmod(destination, 0o600)
        stream.write(payload)
    return {'source': ci_assets.PNG_SOURCE, 'tar_bytes': TAR_SIZE,
            'tar_sha256': TAR_SHA256}


def extract_source(source, output):
    if source.stat().st_size != TAR_SIZE or digest(source) != TAR_SHA256:
        raise ValueError('PNG guest input differs from verified tar payload')
    names = set()
    with tarfile.open(source, 'r:') as archive:
        for member in archive:
            name = PurePosixPath(member.name)
            if (name.is_absolute() or '..' in name.parts or
                    not name.parts or name.parts[0] != 'libpng-1.6.59' or
                    member.name in names or len(names) >= 1000 or
                    not (member.isfile() or member.isdir()) or
                    not 0 <= member.size <= TAR_SIZE):
                raise ValueError('unexpected PNG source archive member')
            names.add(member.name)
            path = output.joinpath(*name.parts)
            path.parent.mkdir(parents=True, exist_ok=True)
            if member.isdir():
                path.mkdir(exist_ok=True)
            else:
                with archive.extractfile(member) as src, path.open('xb') as dst:
                    shutil.copyfileobj(src, dst)
                path.chmod(member.mode & 0o777)


def command(argv, log, cwd=None, env=None, timeout=1200):
    """Bound and reap the whole owned build process group, retaining failures."""
    with log.open('xb') as stream:
        worker = [sys.executable, str(Path(__file__).resolve()),
                  '--exec-worker', *argv]
        process = subprocess.Popen(worker, cwd=cwd, env=env, stdout=stream,
            stderr=subprocess.STDOUT, start_new_session=True)
        deadline = time.monotonic() + timeout
        try:
            while process.poll() is None:
                if time.monotonic() >= deadline or stream.tell() >= MAX_LOG:
                    raise TimeoutError('PNG build deadline/output limit: ' + log.name)
                time.sleep(0.1)
            if process.returncode or log.stat().st_size >= MAX_LOG:
                raise RuntimeError('PNG command failed: ' + log.name)
        finally:
            # Descendants are owned even when make or a shell exits first.
            try:
                os.killpg(process.pid, signal.SIGTERM)
            except ProcessLookupError:
                pass
            try:
                process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                os.killpg(process.pid, signal.SIGKILL)
                process.wait()
            try:
                os.killpg(process.pid, signal.SIGKILL)
            except ProcessLookupError:
                pass


def png_paths(text):
    return set(re.findall(r'(/[^\s()]*libpng[^\s()]*)', text))


def check_loader(text, prefix):
    paths = png_paths(text)
    if not paths:
        raise ValueError('loader observation contains no PNG library')
    for name in paths:
        path = Path(name).resolve(strict=True)
        if prefix.resolve() not in path.parents or not path.is_file():
            raise ValueError('loader selected PNG outside the owned prefix: ' + name)
    return {name: digest(Path(name).resolve()) for name in sorted(paths)}


def symbols(path, log):
    command(['nm', '-D', '-g', str(path)], log, timeout=30)
    return {line.split()[-1] for line in log.read_text().splitlines()
            if len(line.split()) >= 3 and line.split()[-2] != 'U' and
            line.split()[-1].startswith('png_')}


def build(source, output):
    system = platform.system()
    if system not in ('Linux', 'FreeBSD', 'OpenBSD'):
        raise ValueError('PNG bootstrap requires an owned Linux/BSD CI job')
    output = output.absolute()
    output.mkdir(mode=0o700)
    work = output / 'source'
    work.mkdir()
    extract_source(source, work)
    work /= 'libpng-1.6.59'
    prefix = output / 'prefix'
    compiler = shutil.which(os.environ.get('CC', 'cc'))
    if compiler is None:
        raise ValueError('PNG bootstrap requires one installed CC executable')
    build_env = dict(os.environ, CC=compiler)
    configure = ['./configure', '--prefix=' + str(prefix), '--disable-static']
    if system == 'FreeBSD':
        configure += ['ac_cv_lib_m_feenableexcept=no', 'ac_cv_func_feenableexcept=no']
    command(configure, output / 'configure.log', work, env=build_env)
    make = shutil.which('gmake') or shutil.which('make')
    flags = []
    if system == 'OpenBSD':
        # The base package names the same libpng 1.6 API as libpng.so.18.2.
        # Keep this private alias version explicit; unknown package ABIs refuse.
        vendor = Path('/usr/local/lib/libpng.so.18.2')
        if not vendor.is_file():
            raise ValueError('unexpected OpenBSD PNG package ABI')
        flags = ['libpng16_la_LDFLAGS=-no-undefined -export-dynamic -version-number 18:2:0']
    for phase, args in [('compile', ['-j2']), ('check', ['check']),
                        ('install', ['install'])]:
        command([make, *flags, *args], output / (phase + '.log'), work, env=build_env)
    suite = (work / 'test-suite.log').read_text()
    if not all(row in suite for row in ('# TOTAL: 36', '# PASS:  36', '# FAIL:  0')):
        raise ValueError('PNG upstream suite did not pass all 36 tests')
    pkg_path = (str(prefix / 'lib/pkgconfig') +
                ':/usr/local/libdata/pkgconfig:/usr/local/lib/pkgconfig:/usr/X11R6/lib/pkgconfig')
    env = dict(os.environ, PKG_CONFIG_PATH=pkg_path,
               LD_LIBRARY_PATH=str(prefix / 'lib'))
    env.pop('LD_BIND_NOW', None)
    if system == 'OpenBSD':
        native = prefix / 'lib/libpng16.so.18.2'
        original = symbols(vendor, output / 'vendor-symbols.log')
        patched = symbols(native, output / 'patched-symbols.log')
        if not original or original - patched:
            raise ValueError('patched PNG omits vendor API symbols')
        (prefix / 'lib/libpng.so.18.2').symlink_to(native.name)
    command(['pkg-config', '--modversion', 'libpng'], output / 'version.log', env=env)
    if (output / 'version.log').read_text().strip() != '1.6.59':
        raise ValueError('installed PNG metadata differs from approved release')
    probe = output / 'probe.c'
    probe.write_text('/* SPDX-License-Identifier: GPL-2.0-or-later */\n'
        '#include <stdio.h>\n#include <string.h>\n#include <png.h>\n'
        '#include <librsvg/rsvg.h>\nint main(void) {\n'
        'RsvgHandle *h = rsvg_handle_new();\n'
        'const char *v = png_get_libpng_ver(NULL);\n'
        'printf("%s\\n", v); g_object_unref(h);\n'
        'return strcmp(v, "1.6.59") != 0; }\n')
    command(['pkg-config', '--cflags', '--libs', 'libpng', 'librsvg-2.0'],
            output / 'probe-flags.log', env=env)
    command([compiler, str(probe), '-o', str(output / 'probe'),
             *shlex.split((output / 'probe-flags.log').read_text())],
            output / 'probe-build.log', env=env)
    command([str(output / 'probe')], output / 'probe-run.log',
            env=dict(env, LD_BIND_NOW='1'), timeout=30)
    command(['ldd', str(output / 'probe')], output / 'probe-loader.log', env=env, timeout=30)
    loaded = check_loader((output / 'probe-loader.log').read_text(), prefix)
    command([compiler, '--version'], output / 'compiler.log', timeout=30)
    metadata = {'platform': platform.platform(), 'source': ci_assets.PNG_SOURCE,
        'tar_sha256': TAR_SHA256, 'configure': configure, 'make_flags': flags,
        'compiler': compiler, 'compiler_sha256': digest(Path(compiler).resolve()),
        'loaded_png': loaded, 'installed': {str(p.relative_to(prefix)): digest(p)
            for p in prefix.rglob('*') if p.is_file() and not p.is_symlink()}}
    (output / 'receipt.json').write_text(json.dumps(metadata, indent=2) + '\n')
    (output / 'environment.sh').write_text(
        'export PKG_CONFIG_PATH=' + shlex.quote(pkg_path) + '\n'
        'export LD_LIBRARY_PATH=' + shlex.quote(str(prefix / 'lib')) + '\n')


def verify(output, directory, build_only=False):
    prefix = output.absolute() / 'prefix'
    receipt = json.loads((output / 'receipt.json').read_text())
    expected = {value for name, value in receipt['installed'].items()
                if '/libpng' in name and '.so' in name}
    if not expected:
        raise ValueError('PNG build receipt contains no shared library identity')
    identities = {}
    env = dict(os.environ)
    env.pop('LD_BIND_NOW', None)
    paths = [directory / 'fvwm3', directory / 'bin/fvwm-image-svg-worker']
    stage = directory / 'install-stage'
    if not build_only:
        paths += [p for p in stage.rglob('*') if p.is_file() and
                  p.name in ('fvwm3', 'fvwm-image-svg-worker')]
    if len(paths) != (2 if build_only else 4):
        raise ValueError('expected build and staged core/SVG worker consumers')
    for index, path in enumerate(paths):
        log = directory / ('png-loader-%d.log' % index)
        command(['ldd', str(path)], log, env=env, timeout=30)
        loaded = check_loader(log.read_text(), prefix)
        if not set(loaded.values()) <= expected:
            raise ValueError('PNG consumer differs from the qualified build')
        identities[str(path)] = {'sha256': digest(path), 'loaded_png': loaded}
    (directory / 'png-consumers.json').write_text(json.dumps(identities, indent=2) + '\n')


def main():
    if len(sys.argv) > 1 and sys.argv[1] == '--exec-worker':
        # Exec in a fresh interpreter: finite files without threading hazards.
        resource.setrlimit(resource.RLIMIT_FSIZE, (MAX_LOG, MAX_LOG))
        resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
        os.execvpe(sys.argv[2], sys.argv[2:], os.environ)
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('operation', choices=['prepare', 'build', 'verify'])
    parser.add_argument('--source', type=Path)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--build-directory', type=Path)
    parser.add_argument('--build-only', action='store_true',
                        help='Go profile stages clients; core profile checks installed consumers')
    args = parser.parse_args()
    if args.operation == 'prepare':
        if args.source is None:
            parser.error('prepare requires --source')
        prepare_source(args.source, args.output)
    elif args.operation == 'build':
        if args.source is None:
            parser.error('build requires --source')
        build(args.source, args.output)
    else:
        if args.build_directory is None:
            parser.error('verify requires --build-directory')
        verify(args.output, args.build_directory, args.build_only)


if __name__ == '__main__':
    try:
        main()
    except (OSError, ValueError, RuntimeError, TimeoutError) as error:
        print('CI PNG bootstrap failed: ' + str(error), file=sys.stderr)
        sys.exit(1)
