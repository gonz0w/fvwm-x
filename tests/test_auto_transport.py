#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or any later version.
"""Check actual Auto timer/readiness behavior through owned native pipes."""
import argparse
import os
from pathlib import Path
import resource
import signal
import tempfile
import time
from test_event_transport import Module, packet


def run(binary):
    with tempfile.TemporaryDirectory(prefix='fvwm-auto-transport-') as directory:
        root = Path(directory)
        old_limit = resource.getrlimit(resource.RLIMIT_NOFILE)
        can_high = old_limit[1] == resource.RLIM_INFINITY or old_limit[1] > 4096
        if can_high and old_limit[0] <= 4096:
            resource.setrlimit(resource.RLIMIT_NOFILE, (4097, old_limit[1]))
        try:
            cases = [('zero', '0'), ('delay', '5'), ('decimal-prefix', '5tail'),
                     ('legacy-no-digit', 'none'), ('short-body', '5'),
                     ('fragmented', '5'), ('idle-term', '5'),
                     ('pending-term', '2147483647'), ('negative', '-1'),
                     ('out-of-range', '2147483648'),
                     ('decimal-overflow', '999999999999999999999999999')]
            if can_high:
                cases.append(('high-descriptor', '5'))
            else:
                print('high descriptor unavailable under inherited hard limit')
            for label, delay in cases:
                case_root = root / label
                case_root.mkdir()
                module = Module(binary, case_root,
                    [delay, 'Silent Echo auto-proof'],
                    4096 if label == 'high-descriptor' else 0)
                try:
                    if label in ['negative', 'out-of-range', 'decimal-overflow']:
                        assert module.process.wait(timeout=2) == 1, label
                    else:
                        module.receive(lambda: any(text == 'NOP FINISHED STARTUP'
                            for _, text in module.commands))
                        if label == 'idle-term':
                            module.process.send_signal(signal.SIGTERM)
                        else:
                            if label == 'short-body':
                                os.write(module.producer, packet(1 << 6))
                            data = packet(1 << 6, (23, 0, 0))
                            if label == 'fragmented':
                                for offset in range(0, len(data), 3):
                                    os.write(module.producer, data[offset:offset + 3])
                                    time.sleep(0.001)
                            else:
                                os.write(module.producer, data)
                            if label == 'pending-term':
                                module.receive(lambda: any(text == 'NOP UNLOCK'
                                    for _, text in module.commands))
                                assert not any('auto-proof' in text
                                    for _, text in module.commands)
                                module.process.send_signal(signal.SIGTERM)
                            else:
                                module.receive(lambda: any('auto-proof' in text
                                    for _, text in module.commands))
                                actions = [(window, text) for window, text
                                           in module.commands if 'auto-proof' in text]
                                assert len(actions) == 1 and actions[0][0] == 23
                                module.close_input()
                        assert module.process.wait(timeout=2) == 0, label
                    module.log.flush()
                    text = (case_root / 'event.log').read_text()
                    assert not any(marker in text for marker in
                        ['AddressSanitizer', 'runtime error:', 'Assertion']), text
                    print(label, 'passed')
                finally:
                    module.close()
        finally:
            resource.setrlimit(resource.RLIMIT_NOFILE, old_limit)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(allow_abbrev=False)
    parser.add_argument('--module', type=Path, required=True)
    run(parser.parse_args().module)
