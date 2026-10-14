#!/bin/sh
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.

# Preserve the platform-specific public entry point; setup lives in one helper.
set -eu
if ! [ "$(uname -s)" = Linux ]; then
    echo 'This verification entry point requires Linux.' >&2
    exit 1
fi
exec sh "$(dirname "$0")/run_native_gate.sh" "$@"
