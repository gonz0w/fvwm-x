#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or any later version.
"""Fixture-only bounded gate; forward actual desktop worker bytes unchanged.

A one-shot arm file selects palettes/inspect and input/result phase. Holding a
real completed result permits a deterministic stale-result test without making
up worker success. The controller still owns cancellation and its six-second
deadline. Requests/results have the same bounds as the production protocol.
"""
import hashlib
import json
import os
from pathlib import Path
import select
import subprocess
import sys
import time


def atomic_json(path, value):
    temporary = path.with_suffix('.tmp')
    temporary.write_text(json.dumps(value, indent=2) + '\n')
    temporary.replace(path)


def main():
    worker, root = sys.argv[1], Path(sys.argv[2])
    data = sys.stdin.buffer.read(65537)
    assert len(data) <= 65536
    request = json.loads(data)
    operation = request.get('operation')
    token = str(os.getpid()) + '-' + str(time.monotonic_ns())
    record = dict(pid=os.getpid(), operation=operation,
        request_sha256=hashlib.sha256(data).hexdigest(),
        started=time.monotonic())
    arm = root / ('arm-' + str(operation))
    claimed = root / ('claimed-' + token)
    phase = None
    try:
        arm.rename(claimed)
    except FileNotFoundError:
        pass
    else:
        phase = json.loads(claimed.read_text())['phase']
        assert phase in ('input', 'result')

    def hold(at):
        if phase != at:
            return
        record.update(token=token, phase=phase, held=time.monotonic())
        atomic_json(root / ('held-' + operation + '.json'), record)
        deadline = time.monotonic() + 2
        while not (root / ('release-' + token)).exists():
            if time.monotonic() >= deadline:
                record['gate_timeout'] = True
                atomic_json(root / ('done-' + token + '.json'), record)
                raise TimeoutError('fixture gate exceeded two seconds')
            select.select([], [], [], min(.005,
                max(0, deadline - time.monotonic())))

    hold('input')
    completed = subprocess.run([worker], input=data, capture_output=True,
        timeout=2.8, check=False)
    assert len(completed.stdout) <= 1048576
    record.update(returncode=completed.returncode,
        result_sha256=hashlib.sha256(completed.stdout).hexdigest())
    hold('result')
    if operation in ('palettes', 'inspect'):
        record['finished'] = time.monotonic()
        atomic_json(root / ('done-' + token + '.json'), record)
    sys.stdout.buffer.write(completed.stdout)
    sys.stderr.buffer.write(completed.stderr)
    return completed.returncode


if __name__ == '__main__':
    raise SystemExit(main())
