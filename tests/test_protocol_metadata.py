#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or any later version.
"""Check deterministic installed metadata and decode native C packets in Perl."""
import argparse
import json
from pathlib import Path
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools/maintenance'))
import generate


def run(probe, perl, module_library):
    with tempfile.TemporaryDirectory(prefix='fvwm-metadata-') as temporary:
        output = Path(temporary) / 'generated'
        generate.metadata(ROOT, output, 0)
        for name in ('Constants.pm', 'Commands.pm'):
            assert (output / name).read_bytes() == (ROOT / 'perllib/FVWM' / name).read_bytes(), name
            subprocess.run([perl, '-c', str(output / name)], check=True)
        # Export parity includes dynamic native long size and documented names.
        subprocess.run([perl, '-I' + str(ROOT / 'perllib'), '-MFVWM::Constants',
                        '-e', 'for (@FVWM::Constants::EXPORT) { die $_ unless defined &{"FVWM::Constants::$_"}; }'], check=True)
        packets = subprocess.check_output([str(probe)])
        result = subprocess.check_output(
            [perl, '-I' + str(ROOT / 'perllib'), str(ROOT / 'tests/protocol_metadata_decode.pl'),
             str(module_library.resolve())],
            input=packets)
        assert len(json.loads(result)) == 11
        print(result.decode().strip())


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--probe', type=Path, required=True)
    parser.add_argument('--perl', default='perl')
    parser.add_argument('--module-library', type=Path, required=True)
    args = parser.parse_args()
    run(args.probe, args.perl, args.module_library)
