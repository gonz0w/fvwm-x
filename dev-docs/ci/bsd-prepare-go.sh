#!/bin/sh
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or any later version.
set -eu
export PATH="/usr/local/bin:/usr/X11R6/bin:$PATH"
case $(uname -s) in
FreeBSD)
    pkg install -y go xorg-server xf86-video-scfb xf86-video-vesa
    ;;
OpenBSD)
    unset TRUSTED_PKG_PATH
    sh dev-docs/ci/openbsd-package.sh go
    ;;
*) exit 1 ;;
esac
# Install the optional SDK before the expensive PNG build; transport failures
# must not discard completed dependency qualification unnecessarily.
sh dev-docs/ci/bsd-prepare-core.sh
go version > builddir-bsd/provider-go.txt
test -x "$(command -v Xorg)"
