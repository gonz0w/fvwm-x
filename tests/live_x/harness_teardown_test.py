#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or any later version.
"""Observe outer termination cleanup with an owned, stubborn child session."""
import argparse
import json
import os
from pathlib import Path
import signal
import subprocess
import sys
import tempfile
import time
from live_x_harness import HarnessError, HarnessRuntime, wait_for_predicate


def actor(root):
    child = None
    try:
        with HarnessRuntime(prefix='fvwm-teardown-actor-') as runtime:
            script = ('import os,pathlib,signal,sys;'
                'signal.signal(signal.SIGTERM,signal.SIG_IGN);'
                'pathlib.Path(sys.argv[1]).write_text(str(os.getpid())+","+str(os.getpgrp()));'
                'signal.pause()')
            child = runtime.start('owned-child', [sys.executable, '-c', script,
                str(root / 'child.pid')], 'child.log')
            wait_for_predicate('owned child ready', (root / 'child.pid').exists,
                time.monotonic() + 5, process=child)
            (root / 'ready').write_text('ready')
            signal.pause()
    except HarnessError:
        (root / 'result.json').write_text(json.dumps({
            'outer-termination': 'handled', 'owned-child-status': child.poll(),
            'signal-handler-restored': signal.getsignal(signal.SIGTERM) == signal.SIG_DFL,
        }))


def run(args):
    args.artifacts_dir.mkdir(parents=True, exist_ok=True)
    (args.artifacts_dir / 'evidence.json').unlink(missing_ok=True)
    with tempfile.TemporaryDirectory(prefix='fvwm-teardown-observer-') as temp:
        root = Path(temp)
        child_pid = None
        with (args.artifacts_dir / 'actor.log').open('wb') as log:
            process = subprocess.Popen([sys.executable, str(Path(__file__).resolve()),
                '--actor', str(root)], stdout=log, stderr=subprocess.STDOUT,
                start_new_session=True)
            try:
                deadline = time.monotonic() + 5
                while not (root / 'ready').exists():
                    assert process.poll() is None
                    assert time.monotonic() < deadline
                    time.sleep(.01)
                child_pid, group_id = map(int, (root / 'child.pid').read_text().split(','))
                assert group_id == child_pid
                process.send_signal(signal.SIGTERM)
                status = process.wait(timeout=5)
                record = json.loads((root / 'result.json').read_text()) if (root / 'result.json').exists() else {}
                (args.artifacts_dir / 'observation.json').write_text(json.dumps({
                    'actor-status': status, 'actor-record': record,
                }, indent=2) + '\n')
                if record.get('owned-child-status') is not None:
                    child_pid = None  # Actor already reaped its child.
                assert status == 0 and record == {
                    'outer-termination': 'handled',
                    'owned-child-status': -signal.SIGKILL,
                    'signal-handler-restored': True,
                }, (status, record)
                child_pid = None  # Actor reaped this exact child.
            finally:
                if process.poll() is None:
                    process.kill()
                process.wait(timeout=5)
                if child_pid is not None:
                    try:
                        os.kill(child_pid, signal.SIGKILL)
                    except ProcessLookupError:
                        pass
    prior = signal.getsignal(signal.SIGTERM)
    outer = HarnessRuntime(prefix='fvwm-teardown-outer-')
    inner = HarnessRuntime(prefix='fvwm-teardown-inner-')
    try:
        outer.close()
        assert signal.getsignal(signal.SIGTERM) != prior
    finally:
        inner.close()
        outer.close()
    assert signal.getsignal(signal.SIGTERM) == prior
    (args.artifacts_dir / 'evidence.json').write_text(json.dumps({
        'outer-termination-reaps-owned-child': 'passed',
        'nested-out-of-order-cleanup-restores-handler': 'passed',
    }, indent=2) + '\n')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(allow_abbrev=False)
    parser.add_argument('--actor', type=Path)
    parser.add_argument('--artifacts-dir', type=Path)
    args = parser.parse_args()
    if args.actor is not None:
        actor(args.actor)
    else:
        assert args.artifacts_dir is not None
        run(args)
