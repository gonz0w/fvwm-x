#!/bin/sh
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or any later version.
set -eu
export PATH="/usr/local/bin:/usr/X11R6/bin:$PATH"
case $(uname -s) in
FreeBSD)
    pkg install -y xorg-server xf86-video-scfb xf86-video-vesa
    ;;
OpenBSD)
    unset TRUSTED_PKG_PATH
    ;;
*) exit 1 ;;
esac
# Select the pinned patched SDK before the expensive PNG build. The provider
# installs no system Go package and keeps earlier compilers intact.
sh dev-docs/ci/bsd-go-sdk.sh
. builddir-bsd/go-sdk-environment.sh
sh dev-docs/ci/bsd-prepare-core.sh
go version > builddir-bsd/provider-go.txt
test -x "$(command -v Xorg)"
