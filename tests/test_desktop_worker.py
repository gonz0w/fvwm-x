#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or any later version.
"""Independent native module-pipe and installed CLI peers for the Go owner."""
import json
import os
from pathlib import Path
import select
import signal
import struct
import subprocess
import tempfile
import time
import unittest

ROOT = Path(__file__).resolve().parents[1]
CONTROLLER = os.environ.get('FVWM_DESKTOP_CONTROLLER')
WORD = struct.Struct('@L')
HEADER = struct.Struct('@LLLL')


class ResidentController(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='fvwm-resident-')
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.env = dict(os.environ, FVWM_USERDIR=str(self.root),
                        FVWM_DESKTOP_ROOT=str(ROOT / 'default-config'))
        self.process = None
        self.commands, self.buffer = [], bytearray()

    def start(self, ignore_children=False):
        out_read, out_write = os.pipe()
        in_read, in_write = os.pipe()
        self.input, self.output = in_write, out_read
        self.process = subprocess.Popen([CONTROLLER, str(out_write), str(in_read),
            'config', '0', '0'], pass_fds=(out_write, in_read), env=self.env,
            stdout=subprocess.DEVNULL, stderr=subprocess.PIPE,
            preexec_fn=(lambda: signal.signal(signal.SIGCHLD, signal.SIG_IGN))
                if ignore_children else None)
        os.close(out_write)
        os.close(in_read)
        os.set_blocking(out_read, False)
        self.addCleanup(self.close)

    def close(self):
        if self.process is None:
            return
        os.close(self.input)
        self.process.wait(timeout=8)
        self.process.stderr.close()
        os.close(self.output)
        self.process = None

    def packet(self, kind, text=None, fragmented=False):
        body = b'' if text is None else WORD.pack(0) * 3 + text.encode() + b'\0'
        body += b'\0' * (-len(body) % WORD.size)
        data = HEADER.pack(0xffffffff, kind, 4 + len(body) // WORD.size, 0) + body
        if fragmented:
            for offset in range(0, len(data), 3):
                os.write(self.input, data[offset:offset + 3])
        else:
            os.write(self.input, data)

    def receive(self, predicate):
        deadline = time.monotonic() + 5
        while not predicate(self.commands):
            if time.monotonic() >= deadline:
                self.fail('Module peer deadline; commands: ' + repr(self.commands[-8:]))
            if select.select([self.output], [], [], .05)[0]:
                data = os.read(self.output, 8192)
                if not data:
                    self.fail('controller exited: ' + self.process.stderr.read().decode())
                self.buffer.extend(data)
            while len(self.buffer) >= 2 * WORD.size:
                context, size = struct.unpack_from('@LL', self.buffer)
                length = 3 * WORD.size + size
                if len(self.buffer) < length:
                    break
                self.assertEqual(context, 0)
                self.assertLessEqual(size, 4096)
                self.assertEqual(WORD.unpack_from(self.buffer, length - WORD.size)[0], 1)
                self.commands.append(bytes(self.buffer[2 * WORD.size:length - WORD.size]).decode())
                del self.buffer[:length]

    def ready(self):
        self.packet(1 << 19, fragmented=True)
        self.receive(lambda commands: 'NOP FINISHED STARTUP' in commands)

    def request(self, request=None, raw=None):
        encoded = (raw or json.dumps(request, separators=(',', ':'))).encode().hex()
        self.packet(1 << 22, 'ipc-request abc 0000000000000001 ' + encoded, True)
        prefix = 'SendToModule FvwmMFL MFL_SETTINGS_1 abc 0000000000000001 '
        first = len(self.commands)
        self.receive(lambda commands: any(c.startswith(prefix + 'end ') for c in commands[first:]))
        pieces = [c[len(prefix):].split() for c in self.commands[first:] if c.startswith(prefix)]
        parts = [x for x in pieces if x[0] == 'part']
        self.assertEqual([int(x[1]) for x in parts], list(range(len(parts))))
        return json.loads(bytes.fromhex(''.join(x[2] for x in parts)))

    def test_partial_packets_startup_and_private_mutation(self):
        self.start()
        self.packet(1 << 18, '*FvwmDesktop: palette grove', True)
        self.ready()
        provider = self.request(dict(operation='provider'))['result']
        result = self.request(dict(operation='set', provider=provider['provider'],
            generation=provider['generation'], values=dict(palette='ink', motion=True)))['result']
        self.assertEqual(result['state'], 'committed')
        self.assertEqual(result['settings']['palette']['saved'], 'ink')
        self.assertEqual(result['settings']['palette']['effective'], 'grove')
        state = self.root / 'desktop-state.json'
        self.assertEqual(state.stat().st_mode & 0o777, 0o600)
        self.assertEqual(json.loads(state.read_text())['motion'], True)

    def test_preconfiguration_invalid_and_duplicate_requests_do_not_write(self):
        self.start()
        result = self.request(dict(operation='query', scope='all'))
        self.assertEqual(result['error']['code'], 'provider_not_ready')
        self.ready()
        result = self.request(raw='{"operation":"provider","operation":"reload"}')
        self.assertEqual(result['error']['code'], 'invalid_request')
        self.assertFalse((self.root / 'desktop-state.json').exists())

    def test_inherited_ignored_sigchld_preserves_worker_wait_ownership(self):
        self.env.update(XDG_DATA_HOME=str(self.root), XDG_DATA_DIRS=str(self.root))
        self.start(ignore_children=True)
        self.ready()
        self.receive(lambda commands: 'DestroyMenu recreate SlateLauncherMenu' in commands)

    def test_termination_retires_worker_group_before_late_output(self):
        peer = self.root / 'worker-peer'
        peer.write_text('#!' + os.sys.executable + '\n'
            'import json,os,sys,time\nfrom pathlib import Path\n'
            'json.load(sys.stdin)\n'
            'child=os.fork()\n'
            'if child==0:\n time.sleep(.8)\n'
            f' Path({str(self.root / "late")!r}).touch()\n os._exit(0)\n'
            f'Path({str(self.root)!r}, "started-"+str(os.getpid())).touch()\n'
            'time.sleep(1.3)\n')
        peer.chmod(0o700)
        self.env['FVWM_DESKTOP_WORKER'] = str(peer)
        self.start()
        self.ready()
        deadline = time.monotonic() + 3
        while not list(self.root.glob('started-*')):
            self.assertLess(time.monotonic(), deadline)
            time.sleep(.01)
        self.process.terminate()
        status = self.process.wait(timeout=4)
        # A bounded peer exits naturally after 1.3 s even for the pre-fix
        # control. Late output proves whether its descendant survived teardown.
        time.sleep(1.5)
        self.assertFalse((self.root / 'late').exists(), 'worker survived module termination')
        self.assertEqual(status, 0)

    def test_invalid_native_header_fails_explicitly(self):
        self.start()
        os.write(self.input, HEADER.pack(0xffffffff, 0, 4097, 0))
        self.assertNotEqual(self.process.wait(timeout=5), 0)
        self.assertIn('Invalid FVWM packet', self.process.stderr.read().decode())

    def test_installed_cli_exit_codes_without_display_and_safe_paths(self):
        env = dict(self.env)
        env.pop('DISPLAY', None)
        path = self.root / 'escape\x1b[31m'
        for text, expected in [('*FvwmDesktop: palette ink\n', 0),
                               ('*FvwmDesktop: motion invalid\n', 1),
                               ('Exec false\n', 2)]:
            path.write_text(text)
            result = subprocess.run([CONTROLLER, '--check-config', str(path), '--json'],
                                    env=env, capture_output=True, text=True, timeout=8)
            self.assertEqual(result.returncode, expected, result.stderr)
            self.assertEqual(json.loads(result.stdout)['schema_version'], 1)
        result = subprocess.run([CONTROLLER, '--check-config', str(path)],
                                env=env, capture_output=True, text=True, timeout=8)
        self.assertNotIn('\x1b', result.stdout)
        self.assertIn('\\u001b', result.stdout)


if __name__ == '__main__':
    if not CONTROLLER:
        raise SystemExit('Set FVWM_DESKTOP_CONTROLLER to the built/installed owner')
    unittest.main()
