#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or any later version.
"""Run one of four pinned BSD CI images in an owned disposable QEMU job."""
import argparse
import contextlib
import hashlib
import json
import math
import os
import re
from pathlib import Path, PurePosixPath
import resource
import signal
import socket
import subprocess
import sys
import tarfile
import threading
import time

import ci_assets
import ci_png
from common import ROOT, fresh_output

MAX_IMAGE = 64 * 1024**3
MAX_VIRTUAL_DISK = 256 * 1024**3
MAX_SOURCE = 4 * 1024**3
MAX_LOG = 64 * 1024**2
MAX_EVIDENCE = 2 * 1024**3


def limited_argv(argv, limit):
    return [sys.executable, str(Path(__file__).resolve()), '--exec-worker',
            str(limit), *argv]


def sha256(path):
    digest = hashlib.sha256()
    with path.open('rb') as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b''):
            digest.update(chunk)
    return digest.hexdigest()


def validate_source(path):
    if not path.is_file() or path.stat().st_size > 256 * 1024**2:
        raise ValueError('source archive must be a regular file below 256 MiB')
    count = size = 0
    names = set()
    with tarfile.open(path, 'r:gz') as archive:
        for item in archive:
            name = PurePosixPath(item.name)
            count += 1
            size += item.size
            if (count > 100000 or size > MAX_SOURCE or item.size < 0 or
                    item.size > 128 * 1024**2 or name.is_absolute() or
                    '..' in name.parts or not name.parts or
                    name.parts[0] in ('.git', 'builddir') or
                    any(ord(c) < 32 for c in item.name) or
                    not (item.isfile() or item.isdir()) or item.mode & 0o7000):
                raise ValueError('unsupported source archive member')
            if str(name) in names:
                raise ValueError('duplicate source archive member')
            names.add(str(name))
    return {'sha256': sha256(path), 'members': count, 'expanded_bytes': size}


def stop(process):
    if process.poll() is None:
        process.terminate()
        try:
            process.wait(timeout=5)
        except subprocess.TimeoutExpired:
            process.kill()
            process.wait(timeout=5)


def command(argv, output, timeout, stdin=None, limit=MAX_LOG, check=True,
            binary=False, watch=None):
    """Retain bounded output and reap this process on every failure/cancellation."""
    if type(limit) is not int or not 0 < limit <= MAX_IMAGE:
        raise ValueError('command output limit outside finite range')
    if not math.isfinite(timeout) or not 0 < timeout <= 21600:
        raise ValueError('command timeout outside finite range')
    with contextlib.ExitStack() as stack:
        log = stack.enter_context(output.open('xb'))
        errors = subprocess.PIPE if binary else log
        process = subprocess.Popen(limited_argv(argv, limit),
                                   stdin=stdin or subprocess.DEVNULL,
                                   stdout=log, stderr=errors)
        collector = None
        try:
            if binary:
                collector = BoundedOutput(process, process.stderr,
                    output.with_name(output.name + '.stderr'))
            deadline = time.monotonic() + timeout
            while process.poll() is None:
                if watch:
                    watch()
                if collector:
                    collector.check()
                if output.stat().st_size >= limit:
                    raise RuntimeError('command output exceeded its limit')
                if time.monotonic() >= deadline:
                    raise TimeoutError('command deadline expired')
                time.sleep(0.05)
            # RLIMIT_FSIZE can return a short write at exactly the limit.
            # Refuse a full sink even if its producer ignores that short write.
            if output.stat().st_size >= limit:
                raise RuntimeError('command output exceeded its limit')
            if collector:
                collector.close()
                collector.check()
            if check and process.returncode:
                raise RuntimeError('command failed; see ' + output.name)
            return process.returncode
        finally:
            stop(process)
            if collector:
                collector.close()


class BoundedOutput:
    """Drain one pipe with an exact cap and a joined owner."""
    def __init__(self, process, pipe, output):
        self.process = process
        self.pipe = pipe
        self.error = None
        self.thread = threading.Thread(target=self.drain, args=(output,),
                                       daemon=True)
        try:
            self.thread.start()
        except BaseException:
            self.pipe.close()
            raise

    def drain(self, output):
        try:
            with output.open('xb') as log:
                size = 0
                while True:
                    chunk = self.pipe.read1(65536)
                    if not chunk:
                        break
                    available = MAX_LOG - size
                    log.write(chunk[:available])
                    size += len(chunk)
                    if size > MAX_LOG:
                        raise RuntimeError('process output exceeded its limit')
        except BaseException as error:
            self.error = str(error)
            if self.process.poll() is None:
                self.process.terminate()
        finally:
            self.pipe.close()

    def check(self):
        if self.error:
            raise RuntimeError(self.error)

    def close(self):
        self.thread.join(timeout=5)
        if self.thread.is_alive():
            raise RuntimeError('process output collector did not finish')


class LoggedVM:
    """Own QEMU and an exact-size console sink; no unbounded console file."""
    def __init__(self, argv, output):
        self.process = subprocess.Popen(limited_argv(argv, MAX_IMAGE),
            stdin=subprocess.DEVNULL,
            stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        try:
            self.collector = BoundedOutput(self.process, self.process.stdout,
                                           output)
        except BaseException:
            stop(self.process)
            raise

    @property
    def error(self):
        return self.collector.error

    @property
    def thread(self):
        return self.collector.thread

    def check(self):
        self.collector.check()
        if self.process.poll() is not None:
            raise RuntimeError('QEMU exited during the job')

    def close(self):
        stop(self.process)
        self.collector.close()


def image_info(image, output):
    command(['qemu-img', 'info', '--output=json', str(image)], output, 15)
    if output.stat().st_size > 65536:
        raise ValueError('image metadata too large')
    data = json.loads(output.read_text())
    if not isinstance(data, dict):
        raise ValueError('image metadata must be an object')
    specific = data.get('format-specific', {})
    if not isinstance(specific, dict):
        raise ValueError('invalid format metadata')
    details = specific.get('data', {})
    if not isinstance(details, dict):
        raise ValueError('invalid qcow2 metadata')
    if (data.get('format') != 'qcow2' or
            'backing-filename' in data or 'full-backing-filename' in data or
            type(data.get('virtual-size')) is not int or
            not 0 < data.get('virtual-size', 0) <= MAX_VIRTUAL_DISK or
            'data-file' in details or details.get('corrupt', False)):
        raise ValueError('image must be a bounded standalone qcow2')
    return data


def qemu_arguments(profile, job, port, acceleration):
    if profile not in ci_assets.PROFILES or not 1 <= port <= 65535:
        raise ValueError('unsupported profile or forwarding port')
    arm = profile.endswith('arm64')
    system = profile.split('-')[0]
    if acceleration not in ('tcg', 'kvm', 'hvf'):
        raise ValueError('unsupported acceleration')
    machine = 'virt,gic-version=3' if arm else 'pc'
    if arm and system == 'openbsd':
        machine += ',acpi=off'
    cpu = ('neoverse-n1' if system == 'openbsd' else 'max') if arm else 'max'
    if acceleration != 'tcg':
        cpu = 'host'
    argv = ['qemu-system-aarch64' if arm else 'qemu-system-x86_64',
        '-machine', machine, '-accel', acceleration, '-cpu', cpu,
        '-m', '4096', '-smp', '2', '-display', 'none', '-monitor', 'none',
        '-serial', 'stdio',
        '-netdev', 'user,id=net0,ipv6=off,hostfwd=tcp:127.0.0.1:%d-:22' % port,
        '-device', 'virtio-net-pci,netdev=net0',
        '-drive', 'if=none,id=root,format=qcow2,file=' +
            str(job / 'disk.qcow2').replace(',', ',,'),
        '-device', 'virtio-blk-pci,drive=root,bootindex=1',
        '-device', 'qemu-xhci', '-device', 'usb-kbd', '-device', 'usb-tablet']
    if arm:
        argv += ['-bios', str(job / 'inputs/QEMU_EFI-2024.02-2ubuntu0.9.fd'),
                 '-device', 'ramfb']
    else:
        argv += ['-vga', 'std']
    return argv


def check_openbsd_release(profile, identity):
    """Admit only the selected release before modifying a disposable guest."""
    if not profile.startswith('openbsd-'):
        return
    machine = 'arm64' if profile.endswith('arm64') else 'amd64'
    lines = identity.splitlines()
    if (len(lines) != 4 or lines[:3] != ['OpenBSD', '8.0', machine] or
            not lines[3].startswith('OpenBSD 8.0 (')):
        raise ValueError(
            'OpenBSD qualification requires release 8.0 on ' + machine)


def ssh_arguments(job, port, key, first=False):
    return ['ssh', '-F', '/dev/null', '-p', str(port), '-i', str(key),
        '-o', 'BatchMode=yes', '-o', 'IdentitiesOnly=yes',
        '-o', 'IdentityAgent=none', '-o', 'ConnectTimeout=5',
        '-o', 'ConnectionAttempts=1', '-o', 'ServerAliveInterval=5',
        '-o', 'ServerAliveCountMax=2', '-o', 'ControlMaster=no',
        '-o', 'StrictHostKeyChecking=' + ('accept-new' if first else 'yes'),
        '-o', 'UserKnownHostsFile=' + str(job / 'known_hosts'),
        'root@127.0.0.1']


def wait_ssh(vm, argv, job, seconds, prefix='boot'):
    deadline = time.monotonic() + seconds
    attempt = 0
    while time.monotonic() < deadline:
        vm.check()
        attempt += 1
        code = command(argv + ['true'], job / ('%s-%03d.log' % (prefix, attempt)),
                       min(10, max(0.1, deadline - time.monotonic())),
                       check=False, watch=vm.check)
        if code == 0:
            return
        time.sleep(min(1, max(0, deadline - time.monotonic())))
    raise TimeoutError('SSH boot deadline expired')


def boot_identity(path):
    value = path.read_text().strip()
    if re.fullmatch(r'[0-9]+', value):
        seconds, micros = int(value), 0
    else:
        match = re.fullmatch(
            r'\{ sec = ([0-9]+), usec = ([0-9]+) \} [^\r\n]+', value)
        if not match:
            raise RuntimeError('invalid prepared guest boot identity')
        seconds, micros = map(int, match.groups())
    if seconds <= 0 or micros >= 1000000:
        raise RuntimeError('invalid prepared guest boot identity')
    return seconds, micros


def reboot_prepared_guest(vm, ssh, job, seconds):
    """Require a new boot identity before accepting a prepared guest again."""
    identity = ['sysctl -n kern.boottime']
    command(ssh + identity, job / 'prepare-boot-before.log', 15, watch=vm.check)
    before = boot_identity(job / 'prepare-boot-before.log')
    code = command(ssh + ['/sbin/shutdown -r now'],
                   job / 'prepare-reboot.log', 30, check=False, watch=vm.check)
    if code not in (0, 255):
        raise RuntimeError('prepared guest reboot request failed')
    deadline = time.monotonic() + seconds
    attempt = 0
    while time.monotonic() < deadline:
        vm.check()
        attempt += 1
        log = job / ('prepare-reboot-%03d.log' % attempt)
        code = command(ssh + identity, log,
                       min(10, max(0.1, deadline - time.monotonic())),
                       check=False, watch=vm.check)
        current = boot_identity(log) if code == 0 else None
        # SSH can answer once more before shutdown. It is not a completed boot.
        if code == 0 and current and current != before:
            return {'before': before, 'after': current}
        time.sleep(min(1, max(0, deadline - time.monotonic())))
    raise TimeoutError('prepared guest reboot deadline expired')


def run(args):
    if getattr(args, 'reboot_after_prepare', False) and not args.prepare_script:
        raise ValueError('--reboot-after-prepare requires --prepare-script')
    if not 1 <= args.boot_timeout <= 1800 or not 1 <= args.run_timeout <= 21600:
        raise ValueError('boot/run timeout outside finite range')
    vga_test = getattr(args, 'openbsd_vga_test', False)
    if vga_test and args.profile != 'openbsd-amd64':
        raise ValueError('legacy VESA fixture requires OpenBSD AMD64')
    source = validate_source(args.source_archive)
    for script in (args.prepare_script, args.run_script):
        if script and (not script.is_file() or script.stat().st_size > 65536):
            raise ValueError('job scripts must be regular files below 64 KiB')
    # Publish a new private job directory immediately so failed jobs retain
    # evidence. Existing output is refused by the shared publication owner.
    with fresh_output(args.output, ROOT) as stage:
        (stage / 'status.json').write_text('{"status":"prepared"}\n')
    job = args.output.absolute()
    result = {'profile': args.profile, 'source': source, 'status': 'running',
              'phase': 'acquisition', 'acceleration': args.accel}
    vm = None
    key = job / 'job-key'
    try:
        scripts = {}
        for name, path in (('prepare', args.prepare_script),
                           ('run', args.run_script)):
            if path is None:
                continue
            digest = sha256(path)
            size = path.stat().st_size
            scripts[name] = job / (name + '.sh')
            with path.open('rb') as stream:
                ci_assets.checked_copy(stream, scripts[name], {
                    'name': name + '.sh', 'size': size, 'sha256': digest})
            result[name + '_script'] = {'sha256': digest, 'bytes': size}
        # Transfer the checked private snapshot rather than reopen caller data
        # after validation. A concurrent source change refuses publication.
        with args.source_archive.open('rb') as stream:
            ci_assets.checked_copy(stream, job / 'source.tgz', {
                'name': 'source.tgz', 'size': args.source_archive.stat().st_size,
                'sha256': source['sha256']})
        if validate_source(job / 'source.tgz') != source:
            raise ValueError('source snapshot changed during preparation')
        assets, asset_lock = ci_assets.load_assets(ci_assets.PINS, args.profile)
        assets = [item for item in assets if item['name'].endswith(
                  ('.qcow2.zst', '.profile.json'))]
        boot, boot_lock = ci_assets.load_boot_assets(ci_assets.BOOT_PINS,
                                                   args.profile)
        if getattr(args, 'png_bootstrap', False):
            boot += [ci_assets.PNG_SOURCE]
        ci_assets.materialize(assets + boot, asset_lock + ':' + boot_lock,
            args.profile, args.cache, job / 'inputs', args.offline)
        result['input_manifest'] = json.loads(
            (job / 'inputs/manifest.json').read_text())
        if getattr(args, 'png_bootstrap', False):
            result['png_input'] = ci_png.prepare_source(
                job / 'inputs' / ci_assets.PNG_SOURCE['name'], job / 'png-source.tar')
        stem = ci_assets.PROFILES[args.profile][2]
        profile = json.loads((job / 'inputs' / (stem + '.profile.json')).read_text())
        if (profile.get('os') != args.profile.split('-')[0] or
                profile.get('arch') != ('aarch64' if args.profile.endswith('arm64')
                                        else 'x86_64') or
                profile.get('transport') != 'ssh'):
            raise ValueError('unexpected locked guest profile')
        result['published_profile'] = profile
        result['phase'] = 'extraction'
        command(['zstd', '-d', '-c', str(job / 'inputs' / (stem + '.qcow2.zst'))],
                job / 'disk.qcow2', 300, limit=MAX_IMAGE, binary=True)
        result['image'] = image_info(job / 'disk.qcow2', job / 'image-info.json')
        result['expanded_image_sha256'] = sha256(job / 'disk.qcow2')
        command(['ssh-keygen', '-q', '-t', 'ed25519', '-N', '', '-f', str(key)],
                job / 'key-generation.log', 15)
        with socket.socket() as reservation:
            reservation.bind(('127.0.0.1', 0))
            port = reservation.getsockname()[1]
        argv = qemu_arguments(args.profile, job, port, args.accel)
        result['qemu_argv'] = argv
        result['phase'] = 'boot'
        vm = LoggedVM(argv, job / 'qemu.log')
        result['qemu_pid'] = vm.process.pid
        bootstrap = job / 'inputs' / (stem + '-host.id_rsa')
        ssh = ssh_arguments(job, port, bootstrap, first=True)
        wait_ssh(vm, ssh, job, args.boot_timeout)
        result['phase'] = 'authentication'
        # Only this disposable image is changed. Subsequent commands must use
        # the generated job key and the first connection's private host record.
        with key.with_suffix('.pub').open('rb') as public:
            command(ssh + ['umask 077; mkdir -p /root/.ssh; '
                'cat > /root/.ssh/authorized_keys'], job / 'key-rotation.log',
                15, stdin=public, watch=vm.check)
        ssh = ssh_arguments(job, port, key)
        command(ssh + ['true'], job / 'job-authentication.log', 15, watch=vm.check)
        if args.profile.startswith('openbsd-'):
            command(ssh + ['uname -s; uname -r; uname -m; '
                "sysctl -n kern.version | sed -n '1p'"],
                    job / 'guest-release.log', 15, watch=vm.check)
            identity = (job / 'guest-release.log').read_text()
            check_openbsd_release(args.profile, identity)
            result['guest_release'] = identity.splitlines()
        if vga_test:
            result['phase'] = 'graphics-restart'
            # Explicit disposable legacy-VGA fixture opt-in only. VESA's
            # int10 initialization needs aperture access. Ordinary/core jobs
            # retain the pinned image's aperture setting.
            code = command(ssh + ["printf '\\nmachdep.allowaperture=2\\n' "
                ">> /etc/sysctl.conf; /sbin/shutdown -p now"],
                job / 'graphics-shutdown.log', 30, check=False)
            if code not in (0, 255):
                raise RuntimeError('graphics setup/shutdown command failed')
            try:
                vm.process.wait(timeout=120)
            except subprocess.TimeoutExpired:
                raise TimeoutError('graphics shutdown deadline expired')
            initial_vm = vm
            initial_vm.close()
            vm = None
            result['bootstrap_qemu_exit_status'] = initial_vm.process.returncode
            result['bootstrap_qemu_pid'] = initial_vm.process.pid
            if initial_vm.process.returncode != 0:
                raise RuntimeError('graphics bootstrap VM exited abnormally')
            argv = qemu_arguments(args.profile, job, port, args.accel)
            result['graphics_qemu_argv'] = argv
            vm = LoggedVM(argv, job / 'qemu-graphics.log')
            result['qemu_pid'] = vm.process.pid
            wait_ssh(vm, ssh, job, args.boot_timeout, 'graphics-boot')
            command(ssh + ['sysctl -n machdep.allowaperture'],
                    job / 'graphics-aperture.log', 15, watch=vm.check)
            aperture = (job / 'graphics-aperture.log').read_text().strip()
            if aperture != '2':
                raise RuntimeError('legacy VESA fixture aperture was not applied')
            result['disposable_vesa_aperture'] = 2
        result['phase'] = 'source-copy'
        guest_prepare = 'umask 077; mkdir -p /root/fvwm-ci/source && '
        if args.profile.startswith('openbsd'):
            guest_prepare = ('umask 077; mkdir -p /home/fvwm-ci/source && '
                'test ! -e /root/fvwm-ci && '
                'ln -s /home/fvwm-ci /root/fvwm-ci && ')
        with (job / 'source.tgz').open('rb') as archive:
            command(ssh + [guest_prepare +
                'tar xzf - -C /root/fvwm-ci/source'], job / 'source-copy.log',
                120, stdin=archive, watch=vm.check)
        if getattr(args, 'png_bootstrap', False):
            with (job / 'png-source.tar').open('rb') as archive:
                command(ssh + ['umask 077; cat > /root/fvwm-ci/png-source.tar'],
                    job / 'png-copy.log', 30, stdin=archive, watch=vm.check)
        # A script file and process stdin are distinct. Package tools and tests
        # must see EOF, never consume the following commands from sh -s input.
        for name, path in scripts.items():
            with path.open('rb') as script:
                command(ssh + ['umask 077; cat > /root/fvwm-ci/' + name + '.sh'],
                    job / (name + '-copy.log'), 15, stdin=script, watch=vm.check)
        if args.prepare_script:
            result['phase'] = 'prepare'
            command(ssh + ['cd /root/fvwm-ci/source && '
                'exec sh /root/fvwm-ci/prepare.sh'], job / 'prepare.log',
                7200, watch=vm.check)
        if getattr(args, 'reboot_after_prepare', False):
            result['phase'] = 'prepare-reboot'
            result['prepared_boot'] = reboot_prepared_guest(
                vm, ssh, job, args.boot_timeout)
        result['phase'] = 'run'
        code = command(ssh + ['cd /root/fvwm-ci/source && '
            'exec sh /root/fvwm-ci/run.sh'], job / 'run.log', args.run_timeout,
            check=False, watch=vm.check)
        result['run_status'] = code
        result['phase'] = 'collection'
        # Collect only the source job's conventional build artifacts, never
        # guest authentication or unrelated system/user directories.
        command(ssh + ['cd /root/fvwm-ci/source && '
            'set --; for p in builddir-bsd builddir-bsd-go; do '
            'test ! -L "$p" || exit 1; '
            'test ! -d "$p" || set -- "$@" "$p"; done; '
            'test "$#" -gt 0 || exit 1; tar czf - "$@"'],
            job / 'guest-evidence.tgz', 180, limit=MAX_EVIDENCE, binary=True,
            watch=vm.check)
        if code:
            raise RuntimeError('guest gate failed; run and collection logs retained')
        result['status'] = 'passed'
    except BaseException as error:
        result['status'] = 'failed'
        result['error'] = str(error)
        raise
    finally:
        finish_job(vm, key, job, result)


def finish_job(vm, key, job, result):
    """Remove ephemeral auth and retain status even if VM cleanup fails."""
    cleanup_error = None
    if vm is not None:
        try:
            vm.close()
            result['qemu_exit_status'] = vm.process.returncode
        except BaseException as error:
            result['status'] = 'failed'
            result['cleanup_error'] = str(error)
            cleanup_error = error
    for path in (key, key.with_suffix('.pub')):
        path.unlink(missing_ok=True)
    (job / 'status.json').write_text(json.dumps(result, indent=2) + '\n')
    if cleanup_error is not None:
        raise cleanup_error


def main():
    if len(sys.argv) > 1 and sys.argv[1] == '--exec-worker':
        limit = int(sys.argv[2])
        argv = sys.argv[3:]
        allowed = ('qemu-system-aarch64', 'qemu-system-x86_64', 'qemu-img',
                   'zstd', 'ssh', 'ssh-keygen')
        if (not 0 < limit <= MAX_IMAGE or not argv or
                Path(argv[0]).name not in allowed):
            raise ValueError('unsupported execution worker')
        # Apply the hard file cap in a fresh interpreter, avoiding preexec_fn
        # in the console collector's threaded parent. Exec retains ownership.
        resource.setrlimit(resource.RLIMIT_FSIZE, (limit, limit))
        resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
        os.execvp(argv[0], argv)
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--profile', choices=ci_assets.PROFILES, required=True)
    parser.add_argument('--source-archive', type=Path, required=True)
    parser.add_argument('--prepare-script', type=Path)
    parser.add_argument('--run-script', type=Path, required=True)
    parser.add_argument('--reboot-after-prepare', action='store_true',
        help='Reboot the owned guest after preparation; require a new boot identity')
    parser.add_argument('--cache', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--offline', action='store_true')
    parser.add_argument('--png-bootstrap', action='store_true',
                        help='consume the approved fixed upstream PNG input')
    parser.add_argument('--openbsd-vga-test', action='store_true',
        help='Opt into aperture 2 only in a disposable OpenBSD AMD64 VESA fixture')
    parser.add_argument('--accel', choices=['tcg', 'kvm', 'hvf'], default='tcg')
    parser.add_argument('--boot-timeout', type=int, default=600)
    parser.add_argument('--run-timeout', type=int, default=7200)
    args = parser.parse_args()
    def cancel(signum, frame):
        raise RuntimeError('job cancelled by signal %d' % signum)
    signal.signal(signal.SIGTERM, cancel)
    signal.signal(signal.SIGINT, cancel)
    run(args)


if __name__ == '__main__':
    main()
