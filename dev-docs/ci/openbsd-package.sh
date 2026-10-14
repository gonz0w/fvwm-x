#!/bin/sh
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or any later version.
# Complete the Meson bootstrap package before normal signed install.
set -eu
test "$(uname -s)" = OpenBSD
case ${1:-} in
    meson) package=meson-1.10.2v0; file_blocks=32768 ;;
    *) echo 'Expected the Meson CI package.' >&2; exit 1 ;;
esac
case $(uname -m) in
    amd64) package_arch=amd64 ;;
    arm64) package_arch=aarch64 ;;
    *) echo 'Unsupported OpenBSD package architecture.' >&2; exit 1 ;;
esac
mkdir -p builddir-bsd
archive=builddir-bsd/$package.tgz
for attempt in 1 2 3; do
    rm -f "$archive"
    if (ulimit -f "$file_blocks"
        ftp -4 -w 30 -o "$archive" \
            "https://cdn.openbsd.org/pub/OpenBSD/7.9/packages/$package_arch/$package.tgz") &&
        test -s "$archive" && gzip -t "$archive"; then
        break
    fi
    test "$attempt" -lt 3 || exit 1
    sleep "$attempt"
done
tar tzf "$archive" > "builddir-bsd/provider-$1-members.txt"
sha256 -q "$archive" > "builddir-bsd/provider-$1-sha256.txt"
unset TRUSTED_PKG_PATH
FETCH_CMD='/usr/bin/ftp -4 -w 30' pkg_add -I "$(pwd)/$archive"
