#!/bin/sh
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or any later version.
set -eu
export PATH="/usr/local/bin:/usr/X11R6/bin:/usr/bin:/bin:/usr/sbin:/sbin"
test "$(uname -s)" = OpenBSD
mkdir -p builddir-bsd
uname -a > builddir-bsd/after-boot.txt
python3 tests/test_bsd_default_x.py
python3 tests/test_qemu_bsd.py
python3 tests/live_x/harness_teardown_test.py \
    --artifacts-dir builddir-bsd/teardown
python3 tests/run_bsd_default_x.py --server-only --repeat 16 \
    --disposable-vm --artifacts-dir builddir-bsd/startup
