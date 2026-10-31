#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or any later version.
"""Verify the default-X driver's ownership and authentication preconditions."""
import argparse
import json
import os
from pathlib import Path
import struct
import tempfile
import subprocess
import unittest
from unittest.mock import patch

import run_bsd_default_x as driver
import desktop_live
from run_bsd_default_x import authority_record, preflight, query_ready
from live_x_harness import HarnessCancelled, HarnessError
from mfl_stream_live import Connection


class DefaultXTests(unittest.TestCase):
    def test_readiness_query_timeout_and_status(self):
        with patch('run_bsd_default_x.subprocess.run', side_effect=
                   subprocess.TimeoutExpired('xrandr', 2)) as query:
            self.assertIsNone(query_ready(':197', {'DISPLAY': ':197'}))
            self.assertEqual(query.call_args.kwargs['timeout'], 2)
        for status, expected in [(1, None), (0, 'ready')]:
            with patch('run_bsd_default_x.subprocess.run', return_value=
                       subprocess.CompletedProcess([], status, stdout='ready')):
                self.assertEqual(query_ready(':197', {}), expected)

    def test_complete_preview_uses_installed_paths_and_real_native_fixture(self):
        args = argparse.Namespace(server_only=False, complete_preview_only=True,
            fvwm=Path('/owned/core'), mfl=Path('/owned/mfl'),
            backer=Path('/owned/backer'), image_worker=Path('/owned/image-worker'),
            preview_probe=Path('/owned/probe'), preview_config_header=None,
            execution_profile='emulated-cpu')
        self.assertEqual(driver.fixture_commands(args, Path('/build'),
            Path('/source')), [('complete-preview', [
                '--complete-bundle-only', '--execution-profile', 'emulated-cpu',
                '--fvwm', '/owned/core', '--mfl', '/owned/mfl',
                '--backer', '/owned/backer', '--probe', '/owned/probe',
                '--worker', '/owned/image-worker',
                '--config-header', '/build/config.h'])])

    def test_complete_preview_tail_forwards_only_tail_selector(self):
        args = argparse.Namespace(server_only=False,
            complete_preview_only=False, complete_preview_tail_only=True,
            fvwm=None, mfl=None, backer=None, image_worker=None,
            preview_probe=None, preview_config_header=None,
            execution_profile='emulated-cpu')
        self.assertEqual(driver.fixture_commands(args, Path('/build'),
            Path('/source')), [('complete-preview', [
                '--complete-bundle-only', '--execution-profile', 'emulated-cpu',
                '--complete-bundle-tail-only', '--fvwm', '/build/fvwm3',
                '--mfl', '/build/modules/FvwmMFL/FvwmMFL',
                '--backer', '/build/modules/FvwmBacker/FvwmBacker',
                '--probe', '/build/tests/image-owner-probe',
                '--worker', '/build/bin/fvwm-image-svg-worker',
                '--config-header', '/build/config.h'])])

    def test_authority_wire_record(self):
        cookie = bytes(range(16))
        record = authority_record(197, cookie)
        self.assertEqual(record[:2], struct.pack('!H', 65535))
        fields = []
        offset = 2
        while offset < len(record):
            length, = struct.unpack('!H', record[offset:offset+2])
            offset += 2
            fields.append(record[offset:offset+length])
            offset += length
        self.assertEqual(fields, [b'', b'197', b'MIT-MAGIC-COOKIE-1', cookie])

    def test_listener_readiness_failure_closes_socket(self):
        with patch('mfl_stream_live.socket.socket') as factory:
            factory.return_value.connect.side_effect = ConnectionRefusedError()
            with self.assertRaises(ConnectionRefusedError):
                Connection('/private/unready-test-endpoint')
            factory.return_value.close.assert_called_once()

    def test_wrong_platform_or_missing_vm_ack_never_inventories(self):
        with patch('run_bsd_default_x.subprocess.run') as inventory:
            for system, disposable in [('Linux', True), ('FreeBSD', False),
                                       ('OpenBSD', False)]:
                with self.assertRaises(RuntimeError):
                    preflight(system, disposable, 197)
            inventory.assert_not_called()

    def test_invalid_display_never_inventories(self):
        with patch('run_bsd_default_x.subprocess.run') as inventory:
            for number in (0, -1, 65536):
                with self.assertRaises(RuntimeError):
                    preflight('FreeBSD', True, number)
            inventory.assert_not_called()

    def test_running_server_or_inventory_failure_is_preserved(self):
        for status in (0, 2):
            with patch('run_bsd_default_x.subprocess.run', return_value=
                       subprocess.CompletedProcess([], status)):
                with self.assertRaises(RuntimeError):
                    preflight('FreeBSD', True, 197)

    def test_existing_socket_or_lock_is_preserved(self):
        with patch('run_bsd_default_x.subprocess.run', return_value=
                   subprocess.CompletedProcess([], 1)):
            for occupied in ((True,), (False, True)):
                with patch('run_bsd_default_x.Path.exists',
                           side_effect=occupied):
                    with self.assertRaises(RuntimeError):
                        preflight('OpenBSD', True, 197)

    def test_empty_vm_is_ready(self):
        with patch('run_bsd_default_x.subprocess.run', return_value=
                   subprocess.CompletedProcess([], 1)), patch(
                       'run_bsd_default_x.Path.exists', return_value=False):
            preflight('FreeBSD', True, 197)
            preflight('OpenBSD', True, 197)


class StartupEvidenceTests(unittest.TestCase):
    def args(self, root, **overrides):
        values = dict(build_directory=None, artifacts_dir=root / 'artifacts',
                      server_only=True, repeat=1, disposable_vm=True,
                      display_number=197, xlock_only=False, directory_only=False,
                      script_com_only=False, desktop_menu_only=False,
                      desktop_controller_only=False, notify_only=False,
                      fvwm=None, notify=None, mfl=None, notify_client=None,
                      capture=None, execution_profile='unspecified')
        values.update(overrides)
        return argparse.Namespace(**values)

    def test_server_only_requires_no_build_or_fixture_paths(self):
        args = self.args(Path('/unused'))
        self.assertEqual(driver.fixture_commands(args, None, None), [])
        args.server_only = False
        with patch.object(driver, 'run_once') as launch:
            with self.assertRaisesRegex(ValueError, 'build-directory'):
                driver.run(args)
            launch.assert_not_called()

    def test_preview_tail_accepts_preview_paths_without_full_mode(self):
        args = self.args(Path('/unused'), server_only=False,
            build_directory=Path('/build'), complete_preview_tail_only=True,
            backer=Path('/owned/backer'), fvwm=Path('/owned/core'),
            image_worker=Path('/owned/worker'), preview_probe=Path('/owned/probe'),
            preview_config_header=Path('/owned/config.h'))
        with patch.object(driver, 'run_once') as launch:
            driver.run(args)
            launch.assert_called_once_with(args)

    def test_repeat_bounds_and_client_mode_are_checked_before_launch(self):
        for count, server in [(0, True), (33, True), (2, False)]:
            with patch.object(driver, 'run_once') as launch:
                with self.assertRaises(ValueError):
                    driver.run(self.args(Path('/unused'), repeat=count,
                                         server_only=server))
                launch.assert_not_called()

    def test_notification_paths_require_the_notification_mode(self):
        args = self.args(Path('/unused'), fvwm=Path('/explicit/fvwm3'))
        with patch.object(driver, 'run_once') as launch:
            with self.assertRaisesRegex(ValueError, 'require --notify-only'):
                driver.run(args)
            launch.assert_not_called()

    def test_notification_reuses_authenticated_display_with_explicit_paths(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory).resolve()
            build = root / 'build'
            args = self.args(root, build_directory=build, server_only=False,
                notify_only=True, fvwm=root / 'prefix/bin/fvwm3',
                notify=root / 'prefix/libexec/FvwmNotify',
                mfl=root / 'prefix/libexec/FvwmMFL',
                capture=root / 'prefix/bin/fvwm-desktop-capture',
                execution_profile='emulated-cpu')
            launches = []
            class Child:
                def poll(self):
                    return None
                def close(self, deadline):
                    pass
                def wait_until_exit(self, deadline):
                    return 0
            def launch(runtime, name, argv, log_name, env):
                self.assertEqual(env['DISPLAY'], ':197')
                self.assertTrue(Path(env['XAUTHORITY']).is_file())
                launches.append((name, argv))
                child = Child()
                runtime.processes.append(child)
                return child
            with patch.object(driver, 'preflight'), patch.object(
                    driver.os, 'uname', return_value=os.uname_result(
                        ('FreeBSD', 'fixture', '15.1', 'test', 'amd64'))), patch.object(
                    driver, 'dependency_identity', return_value={}), patch.object(
                    driver.HarnessRuntime, 'start', new=launch), patch.object(
                    driver, 'wait_for_predicate', return_value='ready'), patch.object(
                    driver.subprocess, 'run', return_value=subprocess.CompletedProcess([], 1)):
                result = driver.run_once(args)
            self.assertEqual([name for name, argv in launches], ['default-xorg', 'notify'])
            argv = launches[1][1]
            self.assertEqual(Path(argv[1]).name, 'notify_live.py')
            self.assertEqual(argv[2:], ['--display', ':197', '--artifacts-dir',
                str(args.artifacts_dir / 'notify'), '--build', str(build),
                '--execution-profile', 'emulated-cpu', '--fvwm', str(args.fvwm),
                '--notify', str(args.notify), '--mfl', str(args.mfl),
                '--client', str(build / 'tests/notify-client'),
                '--capture', str(args.capture)])
            self.assertEqual(result['status'], 'passed')
            self.assertEqual(result['fixtures']['notify']['status'], 0)

    def stage_desktop(self, build, version='fixture-version'):
        prefix = build / 'install-stage/usr/local'
        controller = prefix / 'libexec/fvwm3' / version / 'FvwmDesktop'
        controller.parent.mkdir(parents=True, exist_ok=True)
        controller.write_text('staged fixture')
        data = prefix / 'share/fvwm3'
        (data / 'default-config').mkdir(parents=True, exist_ok=True)
        (data / 'default-config/config').write_text(
            '# staged desktop sentinel\nRead $[FVWM_DATADIR]/default-config/theme\n')
        return controller, data

    def test_desktop_controller_rejects_missing_or_ambiguous_stage(self):
        with tempfile.TemporaryDirectory() as directory:
            build = Path(directory) / 'build'
            args = self.args(Path(directory), server_only=False,
                             desktop_controller_only=True)
            with self.assertRaisesRegex(RuntimeError, 'exactly one staged FvwmDesktop'):
                driver.fixture_commands(args, build, Path('/source'))
            self.stage_desktop(build, 'version-one')
            self.stage_desktop(build, 'version-two')
            with self.assertRaisesRegex(RuntimeError, 'exactly one staged FvwmDesktop'):
                driver.fixture_commands(args, build, Path('/source'))

    def test_desktop_controller_rejects_incomplete_or_escaped_stage(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            build = root / 'build'
            controller, data = self.stage_desktop(build)
            args = self.args(root, server_only=False,
                             desktop_controller_only=True)
            (data / 'default-config/config').unlink()
            with self.assertRaisesRegex(RuntimeError,
                                        'Missing staged desktop configuration'):
                driver.fixture_commands(args, build, Path('/source'))
            (data / 'default-config/config').write_text('staged configuration')
            outside = root / 'unstaged-controller'
            outside.write_text('outside the stage')
            controller.unlink()
            controller.symlink_to(outside)
            with self.assertRaisesRegex(RuntimeError, 'inside the install stage'):
                driver.fixture_commands(args, build, Path('/source'))

    def test_desktop_controller_uses_authenticated_display_and_staged_data(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory).resolve()
            build = root / 'build with spaces'
            controller, data = self.stage_desktop(build)
            args = self.args(root, build_directory=build, server_only=False,
                             desktop_controller_only=True)
            launches = []
            class Child:
                def poll(self):
                    return None
                def close(self, deadline):
                    pass
                def wait_until_exit(self, deadline):
                    return 0
            def launch(runtime, name, argv, log_name, env):
                self.assertEqual(env['DISPLAY'], ':197')
                authority = Path(env['XAUTHORITY'])
                self.assertTrue(authority.is_file())
                self.assertEqual(authority.stat().st_mode & 0o777, 0o600)
                launches.append((name, argv))
                child = Child()
                runtime.processes.append(child)
                return child
            with patch.object(driver, 'preflight'), patch.object(
                    driver.os, 'uname', return_value=os.uname_result(
                        ('FreeBSD', 'fixture', '15.1', 'test', 'amd64'))), patch.object(
                    driver, 'dependency_identity', return_value={}), patch.object(
                    driver.HarnessRuntime, 'start', new=launch), patch.object(
                    driver, 'wait_for_predicate', return_value='ready'), patch.object(
                    driver.subprocess, 'run', return_value=subprocess.CompletedProcess([], 1)):
                result = driver.run_once(args)
            self.assertEqual([name for name, argv in launches],
                             ['default-xorg', 'desktop-controller'])
            self.assertEqual(launches[0][1][1:5],
                             [':197', '-nolisten', 'tcp', '-noreset'])
            argv = launches[1][1]
            self.assertEqual(Path(argv[1]).name, 'desktop_live.py')
            self.assertEqual(argv[2:], [
                '--display', ':197', '--artifacts-dir',
                str(args.artifacts_dir / 'desktop-controller'),
                '--build', str(build), '--installed-desktop', str(controller),
                '--installed-data', str(data), '--settings-only'])
            self.assertEqual(result['status'], 'passed')
            self.assertEqual(result['fixtures']['desktop-controller']['status'], 0)

    def test_desktop_configuration_uses_staged_or_source_defaults(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory).resolve()
            build = root / 'build'
            (build / 'modules').mkdir(parents=True)
            controller, data = self.stage_desktop(build)
            source = Path(desktop_live.__file__).resolve().parents[2]
            for installed in (data, None):
                options = argparse.Namespace(build=build,
                    artifacts_dir=root / 'artifacts', display=':197',
                    installed_desktop=controller, installed_core=None,
                    installed_modules=None, installed_data=installed)
                expected = (data if installed else source) / 'default-config'
                def launch(runtime, name, argv, log_name, env):
                    self.assertEqual(name, 'fvwm')
                    self.assertEqual(env['DISPLAY'], ':197')
                    self.assertEqual(env['FVWM_DESKTOP_ROOT'], str(expected))
                    config = Path(argv[argv.index('-f') + 1]).read_text()
                    self.assertIn(str(expected), config)
                    self.assertEqual('# staged desktop sentinel' in config,
                                     installed is not None)
                    raise HarnessError('stop before native launch')
                with patch.object(desktop_live.HarnessRuntime, 'start', new=launch):
                    with self.assertRaisesRegex(HarnessError, 'stop before native launch'):
                        desktop_live.run(options)

    def test_successful_repetition_never_hides_an_earlier_failure(self):
        with tempfile.TemporaryDirectory() as directory:
            args = self.args(Path(directory), repeat=3)
            with patch.object(driver, 'run_once', side_effect=[
                    HarnessError('server exited before readiness'), {}, {}]) as launch:
                with self.assertRaisesRegex(RuntimeError, 'At least one'):
                    driver.run(args)
            summary = json.loads((args.artifacts_dir / 'results.json').read_text())
            self.assertEqual(summary['status'], 'failed')
            self.assertEqual([x['status'] for x in summary['attempts']],
                             ['failed', 'passed', 'passed'])
            self.assertEqual(len(set(call.args[0].artifacts_dir
                                     for call in launch.call_args_list)), 3)
            with self.assertRaises(FileExistsError):
                driver.run(args)

    def test_cancellation_does_not_start_the_next_attempt(self):
        for error in (HarnessCancelled('termination'), KeyboardInterrupt()):
            with tempfile.TemporaryDirectory() as directory:
                args = self.args(Path(directory), repeat=3)
                with patch.object(driver, 'run_once', side_effect=error) as launch:
                    with self.assertRaises(type(error)):
                        driver.run(args)
                    self.assertEqual(launch.call_count, 1)
                summary = json.loads((args.artifacts_dir / 'results.json').read_text())
                self.assertEqual(summary['status'], 'cancelled')
                self.assertEqual(summary['requested'], 3)

    def test_old_vendor_log_is_not_attributed_to_a_new_failure(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            old = root / 'vendor.log'
            old.write_text('previous invocation')
            artifacts = root / 'artifacts'
            artifacts.mkdir()
            runtime = argparse.Namespace(root=root / 'runtime')
            runtime.root.mkdir()
            for started in (True, False):
                evidence = {}
                with patch.object(driver, 'dependency_identity', return_value={}):
                    driver.save_evidence(runtime, evidence, artifacts, old,
                                         driver.log_identity(old), started)
                self.assertFalse(evidence['vendor_log_copied'])
                self.assertFalse((artifacts / 'Xorg-server.log').exists())
            before = driver.log_identity(old)
            old.write_text('this invocation with a different length')
            with patch.object(driver, 'dependency_identity', return_value={}):
                driver.save_evidence(runtime, evidence, artifacts, old, before, True)
            self.assertTrue(evidence['vendor_log_copied'])
            self.assertEqual((artifacts / 'Xorg-server.log').read_text(), old.read_text())

    def test_failed_readiness_keeps_phase_status_and_final_teardown_log(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            args = self.args(root)
            processes = []
            class Server:
                closed = False
                def poll(self):
                    return -6
                def close(self, deadline):
                    if not self.closed:
                        self.log.write_text('final server teardown')
                        self.closed = True
            def launch(runtime, name, argv, log_name, env):
                self.assertEqual(name, 'default-xorg')
                self.assertIn('-nolisten', argv)
                self.assertIn('tcp', argv)
                self.assertTrue(Path(env['XAUTHORITY']).is_file())
                child = Server()
                child.log = runtime.root / log_name
                child.log.write_text('initial server log')
                processes.append(child)
                runtime.processes.append(child)
                return child
            with patch.object(driver, 'preflight'), patch.object(
                    driver.os, 'uname', return_value=os.uname_result(
                        ('FreeBSD', 'fixture', '15.1', 'test', 'amd64'))), patch.object(
                    driver, 'dependency_identity', return_value={}), patch.object(
                    driver.HarnessRuntime, 'start', new=launch), patch.object(
                    driver, 'wait_for_predicate', side_effect=HarnessError('server exited')):
                with self.assertRaises(HarnessError):
                    driver.run(args)
            result = json.loads((args.artifacts_dir / 'results.json').read_text())
            self.assertEqual(result['status'], 'failed')
            self.assertEqual(result['phase'], 'server-readiness')
            self.assertEqual(result['server_status_before_cleanup'], -6)
            self.assertEqual(result['fixtures'], {})
            self.assertEqual((args.artifacts_dir / 'xorg.log').read_text(),
                             'final server teardown')
            self.assertTrue(processes[0].closed)
            self.assertFalse((args.artifacts_dir / 'Xauthority').exists())

    def test_freebsd_identity_includes_launcher_wrapper_and_real_server(self):
        with patch.object(driver.shutil, 'which', return_value='/usr/local/bin/Xorg'), patch.object(
                driver.Path, 'is_file', side_effect=lambda: True), patch.object(
                driver.Path, 'read_text', return_value=''), patch.object(
                driver, 'file_identity', side_effect=lambda p: {'path': str(p)}):
            result = driver.dependency_identity('FreeBSD', Path('/fixture/log'))
        self.assertEqual({v['path'] for v in result['files']}, {
            '/usr/local/bin/Xorg', '/usr/local/libexec/Xorg',
            '/usr/local/libexec/Xorg.wrap'})

    def test_identity_uses_loaded_vendor_paths_and_records_patch_failure(self):
        with tempfile.TemporaryDirectory() as directory:
            log = Path(directory) / 'Xorg.log'
            log.write_text('Loading /usr/X11R6/lib/modules/drivers/vesa_drv.so\n'
                           'Loading /tmp/unrelated.so\n')
            with patch.object(driver.shutil, 'which', return_value='/usr/X11R6/bin/Xorg'), patch.object(
                    driver, 'file_identity', side_effect=lambda p: {'path': str(p)}), patch.object(
                    driver.subprocess, 'run', return_value=subprocess.CompletedProcess(
                        [], 1, stdout='', stderr='inventory refused')) as inventory:
                result = driver.dependency_identity('OpenBSD', log)
            self.assertEqual({v['path'] for v in result['files']}, {
                '/usr/X11R6/bin/Xorg', '/usr/X11R6/lib/modules/drivers/vesa_drv.so'})
            self.assertEqual(result['syspatch']['status'], 1)
            self.assertEqual(inventory.call_args.args[0], ['syspatch', '-l'])
            self.assertEqual(inventory.call_args.kwargs['timeout'], 5)


if __name__ == '__main__':
    unittest.main()
