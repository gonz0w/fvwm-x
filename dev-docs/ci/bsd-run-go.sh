#!/bin/sh
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or any later version.
set -eu
export PATH="/usr/local/bin:/usr/X11R6/bin:$PATH"
. builddir-bsd/go-sdk-environment.sh
. builddir-bsd/png-bootstrap/environment.sh
sh tests/run_native_gate.sh --go-clients builddir-bsd-go
meson compile -C builddir-bsd-go fvwm-image-svg-worker -j 2
python3 tests/run_bsd_default_x.py --disposable-vm \
    --build-directory builddir-bsd-go \
    --artifacts-dir builddir-bsd-go/default-x
for fixture in xlock directory script-com desktop-menu; do
    python3 tests/run_bsd_default_x.py --disposable-vm \
        --build-directory builddir-bsd-go --"$fixture"-only \
        --artifacts-dir "builddir-bsd-go/default-x-$fixture"
done
python3 tools/maintenance/ci_png.py verify \
    --output builddir-bsd/png-bootstrap --build-directory builddir-bsd-go \
    --build-only
