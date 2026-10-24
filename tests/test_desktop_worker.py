#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or any later version.
"""Exercise the private worker/controller boundary with independent peers."""
import importlib.util
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import time
import unittest
from unittest.mock import patch
ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('desktop', ROOT/'default-config/desktop.py')
desktop = importlib.util.module_from_spec(spec)
spec.loader.exec_module(desktop)
WORKER = os.environ.get('FVWM_DESKTOP_WORKER')


class DesktopWorkerTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='fvwm-worker-')
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)

    def peer(self, body):
        helper = self.root/'peer'
        helper.write_text('#!'+sys.executable+'\nimport os,sys,time,json\n'+body)
        helper.chmod(0o700)
        return patch.dict(os.environ, FVWM_DESKTOP_WORKER=str(helper))

    def test_selector_failure_never_starts_worker(self):
        with patch.object(desktop.selectors,'DefaultSelector',side_effect=OSError('fd limit')):
            with patch.object(desktop.subprocess,'Popen') as spawn:
                with self.assertRaises(OSError):desktop.discover_apps([])
                spawn.assert_not_called()

    def test_spawn_failure_closes_selector(self):
        with patch.object(desktop.selectors,'DefaultSelector') as factory:
            with patch.object(desktop.subprocess,'Popen',side_effect=OSError('fork failure')):
                with self.assertRaises(OSError):desktop.discover_apps([])
            factory.return_value.close.assert_called_once_with()

    def test_launcher_masks_and_literal_argv(self):
        user, system = self.root/'user', self.root/'system'
        user.mkdir(); system.mkdir()
        (system/'app.desktop').write_text('[Desktop Entry]\nType=Application\nName=System\nExec=/bin/sh\n')
        (user/'app.desktop').write_text('[Desktop Entry]\nHidden=true\n')
        (system/'safe.desktop').write_text('[Desktop Entry]\nType=Application\nName=Quoted;Name\nExec=/bin/sh "literal; argument"\n')
        (system/'bad.desktop').write_text('[Desktop Entry]\nType=Application\nName=Bad\nExec=/bin/sh %Z\n')
        result = desktop.discover_apps([user,system])
        self.assertEqual(result,[dict(name='Quoted;Name',argv=['/bin/sh','literal; argument'],cwd=None,terminal=False)])

    def test_launcher_retains_original_quoted_backslashes(self):
        cases = [(r'/bin/sh "\$HOME"',r'\$HOME'),
                 (r'/bin/sh "\`literal\`"',r'\`literal\`'),
                 ('/bin/sh a\u00a0b','a\u00a0b')]
        for index,(source,_) in enumerate(cases):
            (self.root/(str(index)+'.desktop')).write_text(
                '[Desktop Entry]\nType=Application\nName=Case'+str(index)+'\nExec='+source+'\n')
        result=desktop.discover_apps([self.root])
        self.assertEqual([app['argv'] for app in result],
                         [['/bin/sh',argument] for _,argument in cases])

    def test_actual_worker_rejects_invalid_actions_without_backend_execution(self):
        marker = self.root/'executed'
        for name in ('shutdown','systemctl','amixer','sndioctl','mixer'):
            path = self.root/name
            path.write_text('#!/bin/sh\necho invoked > '+str(marker)+'\n');path.chmod(0o700)
        for request in [dict(version=1,operation='power',system='Linux',action='logout'),
                        dict(version=1,operation='volume',system='Linux',action='up; id')]:
            result = subprocess.run([WORKER],input=json.dumps(request).encode(),capture_output=True,
                                    env=dict(os.environ,PATH=str(self.root)),timeout=8)
            self.assertNotEqual(result.returncode,0)
            self.assertIn('error',json.loads(result.stdout))
        self.assertFalse(marker.exists())

    def test_literal_power_backend_arguments_are_not_shell(self):
        marker = self.root/'arguments'
        path = self.root/'systemctl'
        path.write_text('#!'+sys.executable+'\nimport json,sys\nopen('+repr(str(marker))+',"w").write(json.dumps(sys.argv[1:]))\n')
        path.chmod(0o700)
        with patch.dict(os.environ,PATH=str(self.root)):
            self.assertEqual(desktop.power_action('Linux','reboot'),'Power request sent.')
        self.assertEqual(json.loads(marker.read_text()),['reboot'])

    def test_backend_descendant_pipe_is_retired_by_actual_worker(self):
        marker = self.root/'escaped'
        path = self.root/'amixer'
        path.write_text('#!'+sys.executable+'\nimport os,time\n'
            'if os.fork()==0:\n time.sleep(.8)\n open('+repr(str(marker))+',"w").write("escaped")\n time.sleep(30)\n')
        path.chmod(0o700)
        with patch.dict(os.environ,PATH=str(self.root)):
            with self.assertRaises(RuntimeError):desktop.volume('Linux','up')
        time.sleep(1)
        self.assertFalse(marker.exists())

    def test_backend_output_limit_is_a_failed_job(self):
        path=self.root/'amixer'
        path.write_text('#!'+sys.executable+'\nimport sys,time\nsys.stdout.write("x"*70000)\nsys.stdout.flush()\ntime.sleep(30)\n')
        path.chmod(0o700)
        began=time.monotonic()
        with patch.dict(os.environ,PATH=str(self.root)):
            with self.assertRaises(RuntimeError):desktop.volume('Linux','down')
        self.assertLess(time.monotonic()-began,3)

    def test_partial_request_and_bounded_results(self):
        with self.peer('request=json.load(sys.stdin)\nassert request=={"version":1,"operation":"apps","directories":[]}\nprint(json.dumps({"version":1,"result":[]}))\n'):
            self.assertEqual(desktop.discover_apps([]),[])
        for body in ['sys.stdout.write("x"*(1024*1024+1))\n',
                     'print("{\\\"version\\\":1,\\\"version\\\":1,\\\"result\\\":[]}")\n',
                     'print(json.dumps({"version":True,"result":[]}))\n',
                     'print(json.dumps({"version":1,"result":[],"other":1}))\n',
                     'print(json.dumps({"version":1,"result":None}))\n',
                     'sys.exit(23)\n']:
            with self.subTest(body=body), self.peer(body):
                with self.assertRaises((ValueError,RuntimeError,BrokenPipeError)):
                    desktop.discover_apps([])

    def test_bad_records_never_reach_publication(self):
        values = [dict(cpu=[True,0],memory=0,network='up',battery=None),
                  dict(cpu=[1,2],memory=0,network='up',battery=None),
                  dict(cpu=None,memory=-1,network='up',battery=None),
                  dict(cpu=None,memory=0,network='up',battery=101)]
        for value in values:
            with self.peer('json.load(sys.stdin)\nprint('+repr(json.dumps(dict(version=1,result=value)))+')\n'):
                with self.assertRaises(ValueError):desktop.collect_status('Linux')

    def test_blocked_request_stdin_is_subject_to_deadline(self):
        began=time.monotonic()
        with self.peer('time.sleep(30)\n'), patch.object(desktop,'WORKER_SECONDS',.2):
            with self.assertRaises(RuntimeError):desktop.desktop_worker('apps',directories=['x'*4000]*12)
        self.assertLess(time.monotonic()-began,2)

    def test_deadline_reaps_group_leader_and_descendant(self):
        marker=self.root/'pids'
        body='pid=os.fork()\n'
        body+='if pid==0:\n time.sleep(1)\n open('+repr(str(self.root/'late'))+',"w").write("escaped")\n time.sleep(30)\n'
        body+='open('+repr(str(marker))+',"w").write(json.dumps([os.getpid(),pid]))\ntime.sleep(30)\n'
        with self.peer(body), patch.object(desktop,'WORKER_SECONDS',.3):
            with self.assertRaises(RuntimeError):desktop.discover_apps([])
        leader,_=json.loads(marker.read_text())
        with self.assertRaises(ProcessLookupError):os.kill(leader,0)
        time.sleep(1.1)
        self.assertFalse((self.root/'late').exists())


if __name__ == '__main__':
    if not WORKER:
        raise SystemExit('Set FVWM_DESKTOP_WORKER to the built helper')
    unittest.main()
