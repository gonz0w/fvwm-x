#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
"""Exercise an installed browser's real role workers without starting an X server.

This Linux fixture needs Python 3 and the existing optional file(1) classifier.
It supplies filesystem inputs and private worker requests, never worker replies.
Permission-denied acquisition is covered by the separate unprivileged unit probe.
"""
import argparse
import base64
import ctypes
import hashlib
import json
import os
from pathlib import Path
import signal
import stat
import subprocess
import sys
import tempfile
import time
import traceback


PREFIX = 'fvwm-file-browser: XDG role '
TEXT = b'An ordinary plain text document for actual MIME observation.\n'


def encoded(value):
    return base64.b64encode(os.fsencode(value)).decode('ascii')


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def desktop_string(value):
    return value.replace('\\', '\\\\').replace('\n', '\\n').replace('\t', '\\t')


def exec_word(value):
    value = str(value).replace('%', '%%')
    for char in ('\\', '"', '$', '`'):
        value = value.replace(char, '\\' + char)
    return '"' + value + '"'


def write(path, value):
    path.parent.mkdir(mode=0o700, parents=True, exist_ok=True)
    path.write_text(value, encoding='utf-8')


def preference_state(roots):
    result = {}
    for root in roots:
        for path in sorted(root.rglob('*mimeapps.list')):
            info = path.stat()
            result[str(path)] = dict(sha256=digest(path),
                mode=stat.S_IMODE(info.st_mode), mtime_ns=info.st_mtime_ns)
    return result


class Fixture:
    def __init__(self, browser, root, evidence):
        self.browser, self.root, self.evidence = browser, root, evidence
        self.config, self.lower_config = root / 'config', root / 'lower-config'
        self.user, self.system = root / 'user-data\nquoted', root / 'system-data'
        self.folder = root / '$(touch SHELL-WAS-NOT-RUN); literal folder'
        for path in (self.config, self.lower_config,
                     self.user / 'applications', self.system / 'applications',
                     self.folder):
            path.mkdir(mode=0o700, parents=True)
        self.target = os.fsencode(self.folder) + b'/raw-\xff ; $(literal).txt'
        with open(self.target, 'wb') as stream:
            stream.write(TEXT)
        self.record = root / 'application-record.json'
        self.recorder = root / 'record application.py'
        write(self.recorder, '''import base64,json,os,signal,sys
from pathlib import Path
signal.alarm(4)
enc=lambda value:base64.b64encode(os.fsencode(value)).decode('ascii')
dest=Path(sys.argv[1]); temporary=dest.with_suffix('.tmp')
temporary.write_text(json.dumps(dict(pid=os.getpid(),argv=[enc(x) for x in sys.argv[2:]],cwd=enc(os.getcwd()))))
temporary.replace(dest)
''')
        self.env = dict(os.environ, HOME=str(root), XDG_CONFIG_HOME=str(self.config),
            XDG_CONFIG_DIRS=str(self.lower_config), XDG_DATA_HOME=str(self.user),
            XDG_DATA_DIRS=str(self.system), XDG_CURRENT_DESKTOP='FVWM',
            LANG='C', LC_ALL='C', LANGUAGE='C', DESKTOP_STARTUP_ID='old-fixture-id')
        self.preferences = (self.config, self.lower_config, self.user, self.system)
        self.serial = 0
        scanned, _ = self.rpc('scan', dict(operation='scan',
            path_bytes=encoded(self.folder)))
        assert scanned['status'] == 'loaded', scanned
        self.snapshot = scanned['snapshot']
        matches = [item['id'] for item in self.snapshot['items']
            if base64.b64decode(item['name_bytes']) == os.path.basename(self.target)]
        assert len(matches) == 1, matches
        self.file_id = matches[0]

    def rpc(self, label, request):
        before = preference_state(self.preferences)
        process = subprocess.Popen([str(self.browser), '--worker'], env=self.env,
            stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
            start_new_session=True)
        try:
            stdout, stderr = process.communicate(json.dumps(request).encode(), timeout=8)
        except BaseException:
            if process.poll() is None:
                os.killpg(process.pid, signal.SIGKILL)
                process.wait(timeout=2)
            raise
        self.serial += 1
        record = dict(label=label, returncode=process.returncode,
            stdout_sha256=hashlib.sha256(stdout).hexdigest(),
            stderr=stderr.decode('utf-8', errors='replace'),
            preferences_unchanged=before == preference_state(self.preferences))
        self.evidence['workers'].append(record)
        assert record['preferences_unchanged'], record
        assert process.returncode == 0, record
        assert len(stdout) <= 4 * 1024 * 1024 and len(stderr) <= 64 * 1024, record
        result = json.loads(stdout)
        assert result['schema_version'] == 1, result
        record['status'] = result['status']
        record['result'] = result
        if 'error' in result:
            record['error'] = result['error']
        return result, stderr.decode('utf-8')

    def request(self, kind):
        result = dict(operation=kind + '-roles', snapshot=self.snapshot)
        if kind == 'file':
            result['id'] = self.file_id
        return result

    def roles(self, label, kind, expected, default, diagnostic=None):
        result, stderr = self.rpc(label + '-' + kind, self.request(kind))
        assert result['status'] == 'roles', result
        roles = result['file_roles' if kind == 'file' else 'directory_roles']
        assert [item['id'] for item in roles['choices']] == expected, roles
        assert roles['default_id'] == default, roles
        if diagnostic is None:
            assert not stderr, stderr
        else:
            lines = stderr.splitlines()
            assert lines and all(line.startswith(PREFIX) for line in lines), stderr
            assert any(PREFIX + diagnostic in line for line in lines), stderr
        return roles

    def entry(self, root, name, fields='', mime='inode/directory;text/plain'):
        command = ' '.join(exec_word(x) for x in
            (sys.executable, self.recorder, self.record, 'literal;$()')) + ' %f'
        write(root / 'applications' / name,
            '[Desktop Entry]\nType=Application\nName=' + name + '\nExec=' +
            desktop_string(command) + '\nMimeType=' + mime + ';\n' + fields)

    def defaults(self, root, value, mime='inode/directory;text/plain'):
        write(root / 'mimeapps.list', '[Default Applications]\n' + ''.join(
            part + '=' + value + '\n' for part in mime.split(';')))

    def launch(self, kind, roles):
        assert not self.record.exists()
        choice = next(row for row in roles['choices'] if row['id'] == roles['default_id'])
        request = dict(self.request(kind), operation=kind + '-role-launch',
            role_id=choice['id'], role_fingerprint=choice['fingerprint'])
        if kind == 'file':
            request.update(role_mime=roles['mime'], observed_mime=roles['observed_mime'])
        result, _ = self.rpc('literal-launch-' + kind, request)
        assert result['status'] == 'started' and result['action_count'] == 1, result
        deadline = time.monotonic() + 4
        while not self.record.exists() and time.monotonic() < deadline:
            time.sleep(.01)
        assert self.record.is_file(), 'submitted application did not record its own execution'
        observed = json.loads(self.record.read_text())
        target = self.target if kind == 'file' else self.folder
        assert observed['argv'] == [encoded('literal;$()'), encoded(target)], observed
        assert observed['cwd'] == encoded(self.folder), observed
        while time.monotonic() < deadline:
            waited, status = os.waitpid(observed['pid'], os.WNOHANG)
            if waited:
                assert os.waitstatus_to_exitcode(status) == 0, (waited, status)
                break
            time.sleep(.01)
        else:
            raise AssertionError('owned application did not exit within its deadline')
        self.evidence['applications'].append(dict(kind=kind, recorded=observed,
            waited_pid=waited, wait_status=status))
        self.record.unlink()


def reap_owned(evidence):
    """Only this isolated fixture's adopted children are eligible for cleanup."""
    deadline = time.monotonic() + 5
    while time.monotonic() < deadline:
        try:
            pid, status = os.waitpid(-1, os.WNOHANG)
        except ChildProcessError:
            return
        if pid:
            evidence['cleanup'].append(dict(pid=pid, status=status, forced=False))
        else:
            time.sleep(.01)
    children = set()
    for task in Path('/proc/self/task').iterdir():
        children.update(int(value) for value in (task / 'children').read_text().split())
    for pid in children:
        waited, status = os.waitpid(pid, os.WNOHANG)
        if not waited:
            os.kill(pid, signal.SIGKILL)
            waited, status = os.waitpid(pid, 0)
            evidence['cleanup'].append(dict(pid=waited, status=status, forced=True))
        else:
            evidence['cleanup'].append(dict(pid=waited, status=status, forced=False))
    assert not any(row['forced'] for row in evidence['cleanup']), evidence['cleanup']


def run(browser, root, evidence):
    fixture = Fixture(browser, root, evidence)
    for kind in ('directory', 'file'):
        fixture.roles('absent-providers', kind, [], '')
    evidence['cases'].append('absent optional providers yield an empty role result')

    fixture.entry(fixture.user, 'good.desktop')
    fixture.entry(fixture.system, 'broken.desktop')
    fixture.entry(fixture.user, 'unrelated.desktop', 'Terminal=maybe\n',
        mime='application/x-unrelated')
    fixture.defaults(fixture.config, 'broken.desktop;good.desktop;')
    broken = fixture.user / 'applications' / 'broken.desktop'
    for name, value in (
            ('Exec', '[Desktop Entry]\nType=Application\nName=Broken\nExec=app %Z\n'),
            ('boolean', '[Desktop Entry]\nType=Application\nName=Broken\nExec=app\nTerminal=maybe\n'),
            ('parse', '[Desktop Entry]\nType=Application\nName=Broken\nName=Again\nExec=app\n')):
        write(broken, value)
        for kind in ('directory', 'file'):
            fixture.roles('invalid-' + name, kind, ['good.desktop'], 'good.desktop',
                diagnostic='invalid-desktop-entry')
    evidence['cases'].append('three malformed preferred owners mask their lower ID without blocking a valid different handler')
    evidence['cases'].append('an unrelated malformed owner cannot abort directory or file roles')

    broken.unlink()
    (fixture.system / 'applications' / 'broken.desktop').unlink()
    (fixture.user / 'applications' / 'unrelated.desktop').unlink()
    fixture.entry(fixture.user, 'missing.desktop', 'TryExec=fvwm-fixture-program-missing\n')
    fixture.defaults(fixture.config, 'missing.desktop;good.desktop;')
    for kind in ('directory', 'file'):
        fixture.roles('unavailable-program', kind, ['good.desktop'], 'good.desktop')
    evidence['cases'].append('missing executables are quiet ineligible candidates')

    fixture.entry(fixture.system, 'parent.desktop', mime='application/octet-stream')
    fixture.entry(fixture.system, 'alt.desktop')
    write(fixture.lower_config / 'mimeapps.list', '[Default Applications]\n'
        'inode/directory=alt.desktop;\ntext/plain=alt.desktop;\n'
        'application/octet-stream=parent.desktop;\n')
    fixture.defaults(fixture.config, 'good.desktop;')
    fixture.entry(fixture.system, 'good.desktop', 'Hidden=true\n')
    directory = fixture.roles('config-and-data-precedence', 'directory',
        ['good.desktop', 'alt.desktop'], 'good.desktop')
    file_roles = fixture.roles('specificity-before-parent-default', 'file',
        ['good.desktop', 'alt.desktop', 'parent.desktop'], 'good.desktop')
    assert file_roles['observed_mime'] == 'text/plain', file_roles
    evidence['cases'].append('user data/config precedence and exact-type association beat lower roots and parent defaults')
    fixture.launch('directory', directory)
    fixture.launch('file', file_roles)
    assert not list(root.rglob('SHELL-WAS-NOT-RUN'))
    evidence['cases'].append('two actual applications receive literal arguments, raw file bytes and exact cwd, then exit and are reaped')

    for index in range(24):
        write(fixture.user / 'applications' / ('bad-%02d.desktop' % index),
            '[Desktop Entry]\nType=Application\nName=Bad\nExec=app %Z\n')
    result, stderr = fixture.rpc('bounded-diagnostics', fixture.request('directory'))
    assert result['status'] == 'roles', result
    lines = stderr.splitlines()
    assert len(lines) == 17 and all(line.startswith(PREFIX) for line in lines), stderr
    assert sum(PREFIX + 'invalid-desktop-entry' in line for line in lines) == 16, stderr
    assert sum(PREFIX + 'diagnostics-suppressed' in line for line in lines) == 1, stderr
    assert 'count=8' in lines[-1], stderr
    evidence['cases'].append('24 rejected entries produce 16 bounded details and one eight-item suppression summary')

    write(fixture.config / 'mimeapps.list', '[Default Applications]\ninode/directory=good.desktop;\ninode/directory=again.desktop;\n')
    result, _ = fixture.rpc('malformed-preferences-still-fatal', fixture.request('directory'))
    assert result['status'] == 'failed' and 'duplicate' in result.get('error', '').lower(), result
    evidence['cases'].append('malformed preference syntax remains a failed lookup')
    evidence['cases'].append('every actual worker preserves preference content, mode and mtime')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--browser', type=Path, required=True)
    parser.add_argument('--artifacts-dir', type=Path, required=True)
    args = parser.parse_args()
    browser = args.browser.resolve(strict=True)
    out = args.artifacts_dir.resolve()
    out.mkdir(mode=0o700, parents=True, exist_ok=False)
    evidence = dict(status='running', browser=str(browser), browser_sha256=digest(browser),
        fixture_sha256=digest(Path(__file__).resolve()), kernel=os.uname()._asdict()
        if hasattr(os.uname(), '_asdict') else list(os.uname()), cases=[], workers=[],
        applications=[], cleanup=[], limits=dict(real_x_or_ui=False,
            full_platform_release=False, permission_denied='separate unprivileged unit probe',
            launch_submission_is_not_application_success=True))
    code = 1
    try:
        assert sys.platform.startswith('linux'), 'exact detached-child Wait needs the Linux subreaper profile'
        libc = ctypes.CDLL(None, use_errno=True)
        assert libc.prctl(36, 1, 0, 0, 0) == 0, ctypes.get_errno()
        with tempfile.TemporaryDirectory(prefix='xdg-roles-', dir=out) as temporary:
            try:
                run(browser, Path(temporary), evidence)
                evidence['status'] = 'passed'
                code = 0
            finally:
                reap_owned(evidence)
    except BaseException as error:
        code = 1
        evidence['status'] = 'failed'
        evidence['failure'] = repr(error)
        evidence['traceback'] = traceback.format_exc()
    finally:
        (out / 'evidence.json').write_text(json.dumps(evidence, indent=2) + '\n')
    print(json.dumps(dict(status=evidence['status'], cases=len(evidence['cases']),
        workers=len(evidence['workers']), evidence=str(out / 'evidence.json'))))
    return code


if __name__ == '__main__':
    raise SystemExit(main())
