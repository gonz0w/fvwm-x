#!/bin/sh
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or any later version.
set -eu
test "$(uname -s)" = Linux
python3 tools/maintenance/ci_assets.py --profile png-source \
    --cache builddir/ci-png/cache --output builddir/ci-png/inputs
python3 tools/maintenance/ci_png.py prepare \
    --source builddir/ci-png/inputs/libpng-1.6.59.tar.xz \
    --output builddir/ci-png/source.tar
python3 tools/maintenance/ci_png.py build \
    --source builddir/ci-png/source.tar --output builddir/ci-png/native
