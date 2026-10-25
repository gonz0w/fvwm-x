#!/bin/sh
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.

# Shared Linux/BSD build, test and install gate. No packages are installed.
set -eu

profile=standard
case ${1:-} in
--sanitizers) profile=sanitizers; shift ;;
--go-clients) profile=go-clients; shift ;;
esac
if [ "$#" -gt 1 ]; then
    echo 'Usage: run_native_gate.sh [--sanitizers|--go-clients] [build-directory]' >&2
    exit 1
fi
system=$(uname -s)
if [ "$system" = OpenBSD ]; then
    kernel=$(sysctl -n kern.version)
    case "$(uname -r)/$kernel" in
        '8.0/OpenBSD 8.0 ('*) ;;
        *) echo 'OpenBSD release qualification requires OpenBSD 8.0.' >&2
            exit 1 ;;
    esac
fi
case $system in
Linux) default_dir=builddir/linux-native ;;
FreeBSD|OpenBSD)
    default_dir=builddir-bsd
    # Honor an explicitly selected compiler/Meson environment first.
    PATH="$PATH:/usr/local/bin:/usr/X11R6/bin"
    PKG_CONFIG_PATH="${PKG_CONFIG_PATH:+$PKG_CONFIG_PATH:}/usr/local/libdata/pkgconfig:/usr/local/lib/pkgconfig:/usr/X11R6/lib/pkgconfig"
    export PATH PKG_CONFIG_PATH
    ;;
*) echo 'This verification entry point requires Linux, FreeBSD or OpenBSD.' >&2
    exit 1 ;;
esac
if [ "$profile" = sanitizers ]; then
    if [ "$system" != Linux ]; then
        echo 'The release sanitizer profile requires native Linux.' >&2
        exit 1
    fi
    default_dir=builddir/sanitizers
fi
if [ "$profile" = go-clients ]; then
    default_dir=builddir/go-clients
fi
build_dir=${1:-$default_dir}
# Vendored inputs and the installed SDK are the complete Go build inputs.
# Never fetch a toolchain or a module during this gate.
export GOTOOLCHAIN=local GOPROXY=off GOSUMDB=off
go version
python_bin=$(command -v python3 || true)
if [ -z "$python_bin" ]; then
    for candidate in /usr/local/bin/python3.[0-9] /usr/local/bin/python3.[0-9][0-9]; do
        if [ -x "$candidate" ]; then
            python_bin=$candidate
        fi
    done
fi
if [ -z "$python_bin" ]; then
    echo 'Install Python 3 before running native verification.' >&2
    exit 1
fi
uname -a
"$python_bin" --version
meson --version
if [ "$system" = Linux ]; then
    ${CC:-cc} -dumpmachine
fi
if [ -f "$build_dir/meson-private/coredata.dat" ]; then
    set -- --clearcache --reconfigure
else
    set --
fi
if [ "$profile" = sanitizers ]; then
    set -- "$@" --buildtype=debug -Db_sanitize=address,undefined
else
    set -- "$@" --buildtype=debugoptimized
fi
meson setup "$build_dir" "$@" --prefix=/usr/local \
    -Dpython="$python_bin" -Dlive-x-tests=true \
    -Dxcursor=enabled -Dxrender=enabled -Dpng=enabled \
    -Dpkg_config_path="${PKG_CONFIG_PATH:-}" \
    -Dpng-security-backport="${FVWM_PNG_SECURITY_BACKPORT:-false}" \
    -Dsvg=enabled
if [ "$profile" = go-clients ]; then
    exec sh tests/run_go_clients.sh "$build_dir" "$python_bin"
fi
meson compile -C "$build_dir" -j 2
case $system in
Linux) readelf -h "$build_dir/fvwm3" ;;
*) file "$build_dir/fvwm3" ;;
esac
meson test -C "$build_dir" --print-errorlogs
# Sanitizers qualify instrumented test binaries, not the installed profile.
if [ "$profile" = sanitizers ]; then
    echo "Native Linux sanitizer tests passed: $build_dir"
    exit 0
fi
stage_dir=$(cd "$build_dir" && pwd)/install-stage
meson install -C "$build_dir" --destdir "$stage_dir"
"$stage_dir/usr/local/bin/fvwm3" --version
test -f "$stage_dir/usr/local/share/xsessions/fvwm3.desktop"
echo "$system build, tests, and staged installation passed: $stage_dir"
