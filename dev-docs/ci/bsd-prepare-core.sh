#!/bin/sh
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or any later version.
# Required build/test packages in a disposable CI image only.
set -eu
export PATH="/usr/local/bin:/usr/X11R6/bin:$PATH"
mkdir -p builddir-bsd
uname -a > builddir-bsd/provider-system.txt
case $(uname -s) in
FreeBSD)
    export ASSUME_ALWAYS_YES=yes
    pkg install -y meson ninja pkgconf python3 perl5 libevent fontconfig \
        iceauth libICE libX11 libXext libXft libXrandr libXt libXtst \
        xtrans libXcursor libXrender png librsvg2-rust cairo \
        xorg-vfbserver xrandr dejavu
    pkg info > builddir-bsd/provider-packages.txt
    ;;
OpenBSD)
    unset TRUSTED_PKG_PATH
    # The runner deliberately disables IPv6; use the matching base fetch mode.
    export FETCH_CMD='/usr/bin/ftp -4 -w 30'
    # Keep normal package signatures; complete the previously truncated archive.
    sh dev-docs/ci/openbsd-package.sh meson
    meson --version > builddir-bsd/provider-meson.txt
    for attempt in 1 2 3; do
        if pkg_add -I ninja libevent png librsvg cairo &&
            pkg-config --exists libpng librsvg-2.0 cairo &&
            pkg-config --atleast-version=2.52 librsvg-2.0; then
            break
        fi
        # Retry a missing dependency, never continue with a partial package.
        case $(pkg_info) in
            *partial-*) echo 'Partial package installation; refuse this job.' >&2
                exit 1 ;;
        esac
        test "$attempt" -lt 3 || exit 1
        sleep "$attempt"
    done
    pkg_info > builddir-bsd/provider-packages.txt
    ;;
*) echo 'The QEMU BSD preparation requires FreeBSD or OpenBSD.' >&2; exit 1 ;;
esac
pkg-config --modversion libpng > builddir-bsd/provider-png.txt
pkg-config --exists libpng librsvg-2.0 cairo
pkg-config --atleast-version=2.52 librsvg-2.0
python3 tools/maintenance/ci_png.py build \
    --source /root/fvwm-ci/png-source.tar \
    --output builddir-bsd/png-bootstrap
# Configure performs the patched baseline check; preparation never asserts a
# vendor backport merely because a package or operating system is recognized.
test -x "$(command -v Xvfb)"
# Every native build now requires the patched SDK, not just utility gates.
if test ! -f builddir-bsd/go-sdk-environment.sh; then
    sh dev-docs/ci/bsd-go-sdk.sh
fi
. builddir-bsd/go-sdk-environment.sh
python3 tools/maintenance/go_version.py "$(go env GOVERSION)"
