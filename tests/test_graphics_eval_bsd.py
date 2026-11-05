#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License, version 2 or later.
"""Offline checks of the BSD observer with controlled ps replies."""
from dataclasses import replace
from pathlib import Path
import sys
import unittest
from unittest.mock import Mock, patch

sys.path.insert(0, str(Path(__file__).parent / 'live_x'))
import graphics_eval_bsd as bsd


MODULE = '/tmp/owned/modules/FvwmButtons'
ALIAS = 'DrawingRow'
COMMAND = MODULE + ' 5 6 none 0 0 ' + ALIAS
START = 'Thu Nov  5 12:00:00 2026'


def row(pid=201, parent=200, group=200, uid=65534, state='S', start=START,
        comm='FvwmButtons-ca', command=COMMAND, counters=''):
    return f' {pid} {parent} {group} {uid} {state} {start} {comm} {counters}{command}\n'


class Observer(unittest.TestCase):
    def setUp(self):
        self.core = Mock(pid=200)
        self.core.poll.return_value = None
        with patch.object(bsd.os, 'geteuid', return_value=65534):
            self.observer = bsd.BsdModuleObserver(self.core)

    def test_only_direct_candidates_and_one_shared_deadline(self):
        discovery = '0 0\n200 1\n201 200\n202 200\n203 202\n'
        unrelated = row(pid=202, command='/tmp/owned/modules/FvwmMFL 7 8 none 0 0')
        matched = row(command=COMMAND + ' (FvwmButtons-ca)')
        sample = row(counters='  12:34.56   16384 ',
            command=COMMAND + ' (FvwmButtons-ca)')
        with patch.object(bsd, '_ps', side_effect=[discovery, matched, unrelated, sample]) as ps:
            identity = self.observer.bind(Path(MODULE), ALIAS)
            self.assertEqual(identity.pid, 201)
            self.assertEqual(identity.path, MODULE)
            self.assertEqual(identity.alias, ALIAS)
            self.assertEqual(self.observer.sample(identity)['cpu_centiseconds'], 75456)
        calls = ps.call_args_list
        self.assertEqual([call.args[0][:3] for call in calls], [
            ['-ax', '-o', 'pid='], ['-ww', '-p', '201'],
            ['-ww', '-p', '202'], ['-ww', '-p', '201']])
        self.assertEqual(len({call.args[1] for call in calls[:3]}), 1)
        self.assertNotIn('time=', calls[1].args[0])

    def test_exact_command_and_parent_ownership(self):
        invalid = [row(command=COMMAND.replace('/tmp/owned/', '/tmp/other/')),
            row(command=COMMAND.replace(ALIAS, 'Other')),
            row(command=COMMAND.replace('none', ALIAS).rsplit(' ', 1)[0]),
            row(command=COMMAND + ' extra'), row(command=COMMAND[:-1]),
            row(command=COMMAND + ' (other)'), row(command=COMMAND.replace(' 5 ', ' 5x ')),
            row(uid=1000), row(parent=199), row(group=199), row(pid=202),
            row(state='Z'), row()[:-1], row() + row()]
        for reply in invalid:
            with self.subTest(reply=reply), patch.object(bsd, '_ps',
                    side_effect=['201 200\n', reply]):
                self.assertIsNone(self.observer.bind(MODULE, ALIAS))

    def test_ambiguous_binding_and_unbounded_discovery(self):
        with patch.object(bsd, '_ps', side_effect=['201 200\n202 200\n',
                row(), row(pid=202)]):
            self.assertIsNone(self.observer.bind(MODULE, ALIAS))
        for discovery in ('201 200\n201 200\n', '201 200', '201 200 junk\n',
                ''.join(f'{pid} 200\n' for pid in range(201, 225))):
            with self.subTest(discovery=discovery), patch.object(bsd, '_ps',
                    return_value=discovery) as ps:
                self.assertIsNone(self.observer.bind(MODULE, ALIAS))
                ps.assert_called_once()

    def test_reuse_reparent_exec_and_zombie_samples_unavailable(self):
        with patch.object(bsd, '_ps', side_effect=['201 200\n', row()]):
            identity = self.observer.bind(MODULE, ALIAS)
        self.assertIsNotNone(identity)
        for change in ({'start': START.replace('12:00:00', '12:00:01')},
                {'parent': 1}, {'group': 201}, {'uid': 0}, {'state': 'Z'},
                {'command': COMMAND.replace(' 5 ', ' 9 ')}, {'comm': 'another'}):
            with self.subTest(change=change), patch.object(bsd, '_ps',
                    return_value=row(counters='0:00.01 16384 ', **change)):
                sample = self.observer.sample(identity)
                self.assertEqual(sample['status'], 'unavailable')
                self.assertIsNone(sample['cpu_centiseconds'])
                self.assertIsNone(sample['rss_kib'])
        with patch.object(bsd, '_ps') as ps:
            self.assertEqual(self.observer.sample(replace(identity, pid=202))['status'],
                'unavailable')
            self.core.poll.return_value = 0
            self.assertEqual(self.observer.sample(identity)['status'], 'unavailable')
            ps.assert_not_called()

    def test_precision_and_missing_metrics(self):
        with patch.object(bsd, '_ps', side_effect=['201 200\n', row()]):
            identity = self.observer.bind(MODULE, ALIAS)
        with patch.object(bsd, '_ps', return_value=row(counters='0:00.00 4096 ')):
            sample = self.observer.sample(identity)
        self.assertEqual(sample['cpu_centiseconds'], 0)
        self.assertEqual(sample['cpu_text'], '0:00.00')
        self.assertEqual(sample['cpu_resolution_centiseconds'], 1)
        self.assertEqual(sample['rss_kib'], 4096)
        self.assertIsNone(sample['high_water_kib'])
        self.assertIsNone(sample['wakeups'])
        for counters in ('0:00 4096 ', '0:60.00 4096 ', '0:00.00 - ', '0:00.00 -1 '):
            with self.subTest(counters=counters), patch.object(bsd, '_ps',
                    return_value=row(counters=counters)):
                self.assertEqual(self.observer.sample(identity)['status'], 'unavailable')


if __name__ == '__main__':
    unittest.main()
