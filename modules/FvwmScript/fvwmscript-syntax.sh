#!/bin/sh
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or any later version.

# Generate a complete candidate outside runtime sources for review.
script_dir=$(CDPATH= cd -P "$(dirname "$0")" && pwd) || exit 1
exec python3 "$script_dir/../../tools/maintenance/generate.py" parsers "$@"
