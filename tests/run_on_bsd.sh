#!/bin/sh
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.

# Preserve the platform-specific public entry point; setup lives in one helper.
set -eu
if ! case $(uname -s) in FreeBSD|OpenBSD) true ;; *) false ;; esac; then
    echo 'This verification entry point requires BSD.' >&2
    exit 1
fi
exec sh "$(dirname "$0")/run_native_gate.sh" "$@"
