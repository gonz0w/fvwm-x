#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
"""Provision small, private native filesystem fixtures inside an owned guest.

Requires root on Linux/FreeBSD/OpenBSD. Never mounts a disk or device. Evidence
records each command; cleanup refuses a changed ownership marker or live mount.
"""
import argparse
import ctypes
import json
import os
from pathlib import Path
import platform
import shutil
import stat
import subprocess
import tempfile


SCOPE = 'FVWM-FILEJOBS-NATIVE-FILESYSTEMS-1'
MARKER = '.fvwm-filejobs-fixture'


def identity(path):
    st = path.lstat()
    if not stat.S_ISDIR(st.st_mode) or st.st_uid != os.geteuid():
        raise RuntimeError('fixture directory ownership/type changed')
    return dict(device=st.st_dev, inode=st.st_ino, uid=st.st_uid)


def run(state, *args):
    result = subprocess.run(list(args), capture_output=True, text=True,
                            timeout=60, check=False)
    state['commands'].append(dict(argv=list(args), exit=result.returncode,
                                  stdout=result.stdout, stderr=result.stderr))
    save(state)
    if result.returncode:
        raise RuntimeError('fixture command failed: ' + repr(list(args)))
    return result.stdout


def save(state):
    (Path(state['root']) / 'ownership.json').write_text(
        json.dumps(state, indent=2) + '\n')


def validate(root):
    root = Path(root)
    if not root.is_absolute() or root.name.startswith('fvwm-filejobs-fs-') is False:
        raise RuntimeError('not an owned fixture directory name')
    actual = identity(root)
    marker = root / 'ownership.json'
    st = marker.lstat()
    if not stat.S_ISREG(st.st_mode) or st.st_uid != os.geteuid() or st.st_size > 65536:
        raise RuntimeError('invalid filesystem ownership receipt')
    state = json.loads(marker.read_text())
    if state.get('scope') != SCOPE or state.get('root') != str(root) or state.get('identity') != actual:
        raise RuntimeError('filesystem fixture ownership does not match')
    if state.get('system') not in ('Linux', 'FreeBSD', 'OpenBSD'):
        raise RuntimeError('unsupported fixture system')
    for name in state['mounted']:
        if name not in ('writable', 'readonly'):
            raise RuntimeError('unowned mount path')
    return state


def prepare(parent):
    system = platform.system()
    if os.geteuid() != 0 or system not in ('Linux', 'FreeBSD', 'OpenBSD'):
        raise RuntimeError('requires root in an owned Linux/BSD test guest')
    parent = Path(parent).resolve(strict=True)
    root = Path(tempfile.mkdtemp(prefix='fvwm-filejobs-fs-', dir=parent))
    state = dict(scope=SCOPE, root=str(root), identity=identity(root),
                 system=system, commands=[], mounted=[], environment={})
    save(state)
    try:
        for name in ('writable', 'readonly'):
            path = root / name
            path.mkdir(mode=0o700)
            if system == 'OpenBSD' and name == 'readonly':
                seed = root / 'seed'
                seed.mkdir(mode=0o700)
                (seed / MARKER).write_text('FVWM-FILEJOBS readonly-fs\n')
                # -P populates before the final read-only mount. Updating an
                # mfs:PID mount through generic mount treats PID as a filename.
                before_mounts = run(state, 'mount').splitlines()
                try:
                    run(state, 'mount_mfs', '-s', '16384', '-P', str(seed),
                        '-o', 'ro', 'swap', str(path))
                finally:
                    after_mounts = run(state, 'mount').splitlines()
                    # mount_mfs -P uses an internal temporary mount. Retain
                    # any unexpected residue rather than claiming cleanup.
                    state['unexpected_mounts'] = [line for line in after_mounts
                        if line not in before_mounts and
                        ' on ' + str(path) + ' ' not in line]
                    save(state)
            elif system == 'OpenBSD':
                run(state, 'mount_mfs', '-s', '16384', 'swap', str(path))
            else:
                options = ('size=8m,nosuid,nodev,noexec' if system == 'Linux'
                           else 'size=8m,nosuid,noexec')
                # FreeBSD tmpfs rejects nodev; these fixtures create no devices.
                run(state, 'mount', '-t', 'tmpfs', '-o', options,
                    'tmpfs', str(path))
            state['mounted'].append(name)
            save(state)
            if path.stat().st_dev == root.stat().st_dev:
                raise RuntimeError('fixture did not create a separate filesystem')
        for name, kind in (('other', 'other-fs'), ('full', 'full-fs')):
            path = root / 'writable' / name
            path.mkdir(mode=0o700)
            (path / MARKER).write_text('FVWM-FILEJOBS ' + kind + '\n')
            key = 'OTHER' if name == 'other' else 'FULL'
            state['environment']['FVWM_FILEJOBS_TEST_' + key + '_FS'] = str(path)
        readonly = root / 'readonly'
        if system != 'OpenBSD':
            (readonly / MARKER).write_text('FVWM-FILEJOBS readonly-fs\n')
        else:
            identity(readonly)
            if (stat.S_IMODE(readonly.stat().st_mode) != 0o700 or
                (readonly / MARKER).read_text() != 'FVWM-FILEJOBS readonly-fs\n' or
                state.get('unexpected_mounts')):
                raise RuntimeError('populated readonly MFS identity differs')
        if system == 'Linux':
            # Use the syscall flags directly: a mount frontend can reapply
            # an inherited SELinux context option that remount does not accept.
            libc = ctypes.CDLL(None, use_errno=True)
            libc.mount.argtypes = [ctypes.c_char_p, ctypes.c_char_p,
                                   ctypes.c_char_p, ctypes.c_ulong,
                                   ctypes.c_void_p]
            libc.mount.restype = ctypes.c_int
            flags = 32 | 4096 | 1 | 2 | 4 | 8
            status = libc.mount(None, os.fsencode(readonly), None, flags, None)
            error = ctypes.get_errno() if status else 0
            state['commands'].append(dict(syscall='mount', target=str(readonly),
                flags=['MS_REMOUNT', 'MS_BIND', 'MS_RDONLY', 'MS_NOSUID',
                       'MS_NODEV', 'MS_NOEXEC'], exit=status, errno=error))
            save(state)
            if status:
                raise OSError(error, os.strerror(error), str(readonly))
        elif system == 'FreeBSD':
            run(state, 'mount', '-u', '-o', 'ro', str(readonly))
        state['environment']['FVWM_FILEJOBS_TEST_READONLY_FS'] = str(readonly)
        state['status'] = 'ready'
        save(state)
        return state
    except BaseException:
        state['status'] = 'failed'
        save(state)
        raise


def cleanup(root):
    state = validate(root)
    root = Path(state['root'])
    if state.get('unexpected_mounts'):
        raise RuntimeError('unexpected MFS population mount retained for guest shutdown')
    for name in list(reversed(state['mounted'])):
        path = root / name
        # Refuse a replacement mountpoint or a path redirected by a symlink.
        if path.is_symlink() or not path.is_dir():
            raise RuntimeError('fixture mountpoint changed')
        run(state, 'umount', str(path))
        state['mounted'].remove(name)
        save(state)
    for name in ('writable', 'readonly'):
        path = root / name
        if path.exists() and (path.is_symlink() or path.stat().st_dev != root.stat().st_dev):
            raise RuntimeError('refusing cleanup of a live or redirected mount')
    # The complete receipt is returned before removing only this exact tree.
    state['status'] = 'cleaned'
    save(state)
    shutil.rmtree(root)
    return state


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest='action', required=True)
    sub.add_parser('prepare').add_argument('--parent', type=Path, required=True)
    sub.add_parser('cleanup').add_argument('--root', type=Path, required=True)
    args = parser.parse_args()
    value = prepare(args.parent) if args.action == 'prepare' else cleanup(args.root)
    print(json.dumps(value))


if __name__ == '__main__':
    main()
