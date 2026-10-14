#!/bin/sh
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or any later version.
set -eu
. builddir-bsd/png-bootstrap/environment.sh
sh tests/run_on_bsd.sh builddir-bsd
python3 tools/maintenance/ci_png.py verify \
    --output builddir-bsd/png-bootstrap --build-directory builddir-bsd
