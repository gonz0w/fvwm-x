#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or any later version.
"""Test bounded source/boot policy and actual subprocess cleanup without a VM."""
import argparse
import io
import json
import os
from pathlib import Path
import subprocess
import sys
import tarfile
import tempfile
import time
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools/maintenance'))
import ci_assets
import qemu_bsd as qemu


class QemuTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='fvwm-qemu-policy-')
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.bin = self.root / 'bin'
        self.bin.mkdir()
        self.archive = self.root / 'source.tgz'
        self.env = patch.dict(os.environ,
            PATH=str(self.bin) + os.pathsep + os.environ['PATH'])
        self.env.start()
        self.addCleanup(self.env.stop)

    def source(self, members=None):
        with tarfile.open(self.archive, 'w:gz') as archive:
            for name, kind, mode in members or [('README', tarfile.REGTYPE, 0o644)]:
                item = tarfile.TarInfo(name)
                item.type = kind
                item.mode = mode
                item.linkname = '/outside'
                body = b'owned source\n' if kind == tarfile.REGTYPE else b''
                item.size = len(body)
                archive.addfile(item, io.BytesIO(body))
        return self.archive

    def stub(self, name, body):
        path = self.bin / name
        path.write_text('#!' + sys.executable + '\n' + body + '\n')
        path.chmod(0o755)
        return str(path)

    def test_source_identity(self):
        result = qemu.validate_source(self.source())
        self.assertEqual(result['members'], 1)
        self.assertEqual(result['expanded_bytes'], len(b'owned source\n'))
        self.assertEqual(result['sha256'], qemu.sha256(self.archive))

    def test_source_refuses_paths_links_and_special_files(self):
        cases = [('../escape', tarfile.REGTYPE, 0o644),
            ('/absolute', tarfile.REGTYPE, 0o644),
            ('.git/config', tarfile.REGTYPE, 0o644),
            ('builddir/data', tarfile.REGTYPE, 0o644),
            ('bad\nname', tarfile.REGTYPE, 0o644),
            ('link', tarfile.SYMTYPE, 0o777),
            ('link', tarfile.LNKTYPE, 0o644),
            ('fifo', tarfile.FIFOTYPE, 0o600),
            ('setuid', tarfile.REGTYPE, 0o4755)]
        for case in cases:
            with self.subTest(case=case), self.assertRaises(ValueError):
                qemu.validate_source(self.source([case]))

    def test_source_refuses_duplicate_members(self):
        item = ('README', tarfile.REGTYPE, 0o644)
        with self.assertRaises(ValueError):
            qemu.validate_source(self.source([item, item]))

    def test_source_refuses_large_archive_before_parsing(self):
        with self.archive.open('wb') as out:
            out.truncate(256 * 1024**2 + 1)
        with self.assertRaises(ValueError):
            qemu.validate_source(self.archive)

    def test_qemu_profiles_have_local_forwarding_and_owned_disk(self):
        for profile in ci_assets.PROFILES:
            with self.subTest(profile=profile):
                argv = qemu.qemu_arguments(profile, self.root, 23232, 'tcg')
                self.assertIn('user,id=net0,ipv6=off,hostfwd=tcp:127.0.0.1:23232-:22', argv)
                self.assertNotIn('-snapshot', argv)
                self.assertEqual(argv[argv.index('-monitor') + 1], 'none')
                self.assertEqual(argv[argv.index('-display') + 1], 'none')
                if profile.endswith('arm64'):
                    self.assertIn('ramfb', argv)
                    self.assertIn('-bios', argv)
                else:
                    self.assertEqual(argv[argv.index('-vga') + 1], 'std')
                self.assertFalse(any('anyvm.py' in item or '.qemu' in item
                                     for item in argv))

    def test_qemu_escapes_commas_in_disk_path(self):
        argv = qemu.qemu_arguments('freebsd-amd64', self.root / 'with,comma', 2222, 'tcg')
        self.assertIn('with,,comma', argv[argv.index('-drive') + 1])

    def test_qemu_refuses_unknown_profile_acceleration_or_port(self):
        for profile, port, accel in [('unknown', 123, 'tcg'),
                ('freebsd-amd64', 0, 'tcg'), ('freebsd-amd64', 65536, 'tcg'),
                ('freebsd-amd64', 123, 'unknown')]:
            with self.subTest(profile=profile, port=port, accel=accel):
                with self.assertRaises(ValueError):
                    qemu.qemu_arguments(profile, self.root, port, accel)

    def test_ssh_uses_job_authentication_and_host_record(self):
        argv = qemu.ssh_arguments(self.root, 2222, self.root / 'key')
        for value in ['BatchMode=yes', 'IdentitiesOnly=yes', 'IdentityAgent=none',
                'ControlMaster=no', 'StrictHostKeyChecking=yes',
                'UserKnownHostsFile=' + str(self.root / 'known_hosts')]:
            self.assertIn(value, argv)
        self.assertIn('/dev/null', argv)
        self.assertNotIn('StrictHostKeyChecking=no', argv)
        first = qemu.ssh_arguments(self.root, 2222, self.root / 'key', first=True)
        self.assertIn('StrictHostKeyChecking=accept-new', first)

    def test_published_boot_selection(self):
        for profile in ci_assets.PROFILES:
            rows, digest = ci_assets.load_boot_assets(ci_assets.BOOT_PINS, profile)
            self.assertEqual(len(rows), 2 if profile.endswith('arm64') else 1)
            self.assertTrue(rows[0]['name'].endswith('-host.id_rsa'))
            self.assertEqual(len(digest), 64)

    def test_image_refuses_backing_external_data_corruption_and_bad_geometry(self):
        original = {'format': 'qcow2', 'virtual-size': 200 * 1024**3}
        cases = [dict(original, format='raw'), dict(original, **{'backing-filename':'outside'}),
            dict(original, **{'full-backing-filename':'outside'}),
            dict(original, **{'virtual-size':True}), dict(original, **{'virtual-size':0}),
            dict(original, **{'virtual-size':qemu.MAX_VIRTUAL_DISK + 1}),
            dict(original, **{'format-specific':{'data':{'data-file':'outside'}}}),
            dict(original, **{'format-specific':{'data':{'corrupt':True}}})]
        for data in cases:
            with self.subTest(data=data):
                with patch.object(qemu, 'command', side_effect=lambda argv, out, timeout:
                        out.write_text(json.dumps(data))):
                    with self.assertRaises(ValueError):
                        qemu.image_info(self.root / 'disk', self.root / 'info')

    def test_image_accepts_bounded_sparse_virtual_geometry(self):
        data = {'format':'qcow2', 'virtual-size':200 * 1024**3, 'actual-size':3 * 1024**3}
        with patch.object(qemu, 'command', side_effect=lambda argv, out, timeout:
                out.write_text(json.dumps(data))):
            self.assertEqual(qemu.image_info(self.root / 'disk', self.root / 'info'), data)

    def test_actual_command_preserves_binary_output_and_separate_errors(self):
        exe = self.stub('zstd', 'import os\nos.write(1,b"QFI\\xfb")\nos.write(2,b"diagnostic\\n")')
        out = self.root / 'disk'
        qemu.command([exe], out, 10, binary=True)
        self.assertEqual(out.read_bytes(), b'QFI\xfb')
        self.assertEqual((self.root / 'disk.stderr').read_bytes(), b'diagnostic\n')

    def test_actual_command_failure_is_retained(self):
        exe = self.stub('ssh', 'import sys\nprint("failure evidence")\nsys.exit(23)')
        out = self.root / 'log'
        with self.assertRaises(RuntimeError):
            qemu.command([exe], out, 10)
        self.assertEqual(out.read_text(), 'failure evidence\n')

    def test_actual_command_deadline_reaps_child(self):
        exe = self.stub('ssh', 'import time\ntime.sleep(30)')
        processes = []
        launch = subprocess.Popen
        def record(*args, **kwargs):
            process = launch(*args, **kwargs)
            processes.append(process)
            return process
        start = time.monotonic()
        # Capture the owned PID at launch, before interpreter/exec startup.
        # The deadline also covers startup on a slow emulated guest.
        with patch.object(qemu.subprocess, 'Popen', side_effect=record):
            with self.assertRaises(TimeoutError):
                qemu.command([exe], self.root / 'log', 2)
        self.assertLess(time.monotonic() - start, 8)
        self.assertEqual(len(processes), 1)
        self.assertIsNotNone(processes[0].returncode)
        with self.assertRaises(ProcessLookupError):
            os.kill(processes[0].pid, 0)

    def test_actual_command_hard_file_limit(self):
        exe = self.stub('ssh', 'import os\nos.write(1,b"x"*65536)')
        out = self.root / 'log'
        with self.assertRaises(RuntimeError):
            qemu.command([exe], out, 10, limit=1024)
        self.assertLessEqual(out.stat().st_size, 1024)

    def test_actual_console_limit_stops_and_reaps_vm(self):
        exe = self.stub('qemu-system-aarch64', 'import os,time\nos.write(1,b"x"*131072)\ntime.sleep(30)')
        out = self.root / 'console'
        with patch.object(qemu, 'MAX_LOG', 1024):
            vm = qemu.LoggedVM([exe], out)
            try:
                deadline = time.monotonic() + 10
                while vm.error is None and time.monotonic() < deadline:
                    time.sleep(.05)
                with self.assertRaises(RuntimeError):
                    vm.check()
            finally:
                vm.close()
        self.assertEqual(out.stat().st_size, 1024)
        self.assertIsNotNone(vm.process.returncode)
        self.assertFalse(vm.thread.is_alive())

    def test_collector_start_failure_reaps_launched_process(self):
        launch = subprocess.Popen
        for vm_case in (False, True):
            with self.subTest(vm=vm_case):
                processes = []
                def record(*args, **kwargs):
                    process = launch(*args, **kwargs)
                    processes.append(process)
                    return process
                exe = self.stub('qemu-system-aarch64' if vm_case else 'zstd',
                                'import time\ntime.sleep(30)')
                with patch.object(qemu.subprocess, 'Popen', side_effect=record), \
                        patch.object(qemu.threading.Thread, 'start',
                                     side_effect=RuntimeError('fixture refusal')):
                    with self.assertRaises(RuntimeError):
                        if vm_case:
                            qemu.LoggedVM([exe], self.root / 'vm-console')
                        else:
                            qemu.command([exe], self.root / 'binary-output',
                                         10, binary=True)
                self.assertEqual(len(processes), 1)
                self.assertIsNotNone(processes[0].returncode)
                with self.assertRaises(ProcessLookupError):
                    os.kill(processes[0].pid, 0)

    def test_actual_workers_disable_core_dumps(self):
        exe = self.stub('ssh', 'import resource\nprint(resource.getrlimit(resource.RLIMIT_CORE)[0])')
        out = self.root / 'log'
        qemu.command([exe], out, 10)
        self.assertEqual(out.read_text(), '0\n')

    def test_actual_quiet_vm_cleanup(self):
        exe = self.stub('qemu-system-aarch64', 'import time\ntime.sleep(30)')
        vm = qemu.LoggedVM([exe], self.root / 'console')
        vm.check()
        vm.close()
        self.assertIsNotNone(vm.process.returncode)
        self.assertFalse(vm.thread.is_alive())

    def test_command_refuses_nonfinite_deadline_before_launch(self):
        for timeout in (0, -1, float('nan'), float('inf'), 21601):
            with self.subTest(timeout=timeout), self.assertRaises(ValueError):
                qemu.command(['ssh'], self.root / 'log', timeout)

    def test_actual_binary_error_output_has_separate_hard_cap(self):
        exe = self.stub('zstd', 'import os,time\nos.write(2,b"x"*131072)\ntime.sleep(30)')
        out = self.root / 'disk'
        with patch.object(qemu, 'MAX_LOG', 1024):
            with self.assertRaises(RuntimeError):
                qemu.command([exe], out, 10, limit=4096, binary=True)
        self.assertEqual((self.root / 'disk.stderr').stat().st_size, 1024)
        self.assertEqual(out.stat().st_size, 0)

    def test_cleanup_failure_still_removes_auth_and_preserves_status(self):
        key = self.root / 'job-key'
        key.write_text('private test fixture')
        key.with_suffix('.pub').write_text('public test fixture')
        class FailedVM:
            def close(self):
                raise RuntimeError('fixture collector failure')
        result = {'status': 'passed'}
        with self.assertRaises(RuntimeError):
            qemu.finish_job(FailedVM(), key, self.root, result)
        self.assertFalse(key.exists())
        self.assertFalse(key.with_suffix('.pub').exists())
        status = json.loads((self.root / 'status.json').read_text())
        self.assertEqual(status['status'], 'failed')
        self.assertEqual(status['cleanup_error'], 'fixture collector failure')

    def test_image_refuses_invalid_metadata_shapes(self):
        for data in ([], {'format-specific': []},
                     {'format-specific': {'data': []}}):
            with self.subTest(data=data):
                with patch.object(qemu, 'command',
                        side_effect=lambda argv, out, timeout:
                        out.write_text(json.dumps(data))):
                    with self.assertRaises(ValueError):
                        qemu.image_info(self.root / 'disk', self.root / 'info')

    def test_command_refuses_invalid_file_limits_before_launch(self):
        for limit in (0, -1, True, qemu.MAX_IMAGE + 1):
            with self.subTest(limit=limit), self.assertRaises(ValueError):
                qemu.command(['ssh'], self.root / 'log', 10, limit=limit)

    def test_legacy_vga_opt_in_refuses_other_profiles_before_work(self):
        args = argparse.Namespace(boot_timeout=1, run_timeout=1,
            openbsd_vga_test=True, profile='openbsd-arm64')
        with self.assertRaises(ValueError):
            qemu.run(args)

    def test_existing_job_directory_is_preserved(self):
        self.source()
        script = self.root / 'run.sh'
        script.write_text('true\n')
        job = self.root / 'job'
        job.mkdir()
        (job / 'keep').write_text('existing evidence')
        args = argparse.Namespace(source_archive=self.archive,
            prepare_script=None, run_script=script, output=job,
            boot_timeout=1, run_timeout=1)
        with self.assertRaises(ValueError):
            qemu.run(args)
        self.assertEqual((job / 'keep').read_text(), 'existing evidence')


if __name__ == '__main__':
    unittest.main()
