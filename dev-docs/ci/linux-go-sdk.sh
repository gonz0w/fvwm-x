#!/bin/sh
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or any later version.
# Explicit disposable-job SDK bootstrap, never a normal configure/build fetch.
set -eu
test "$#" -le 1 || exit 1
system=$(uname -s)
arch=$(uname -m)
case "$system/$arch" in
Linux/aarch64)
    platform=linux-arm64; size=67076180
    digest=94f3e30b8e374bc285e7dadc11e0865726b9bc6e85b841ccceaabc0214c6b7c8 ;;
Linux/x86_64)
    platform=linux-amd64; size=70590635
    digest=ecbadb99091a3f46e31f5f934b068b1864eafa7995211b39eaddf76996045fe5 ;;
*) echo 'The Go SDK recipe requires Linux AMD64/ARM64.' >&2; exit 1 ;;
esac
url=https://dl.google.com/go/go1.27.2.$platform.tar.gz
# Keep SDK bytes outside the provider's collected build trees. Preserve its
# identity/license with the evidence; the compiler archive is independently pinned.
sdk=builddir/go-sdk
if test -e "$sdk" || test -L "$sdk"; then
    echo 'Refuse to overwrite an existing job SDK.' >&2
    exit 1
fi
umask 077
mkdir -p builddir
mkdir "$sdk"
if test "$#" -eq 1; then
    archive=$1
    test -f "$archive" && test ! -L "$archive"
else
    archive=$sdk/download.tgz
    for attempt in 1 2 3; do
        rm -f "$archive"
        if (ulimit -f 262144
            case "$system" in
                Linux) curl --fail --location --connect-timeout 30 \
                    --max-time 180 --output "$archive" "$url" ;;
            esac) && test "$(wc -c < "$archive" | tr -d ' ')" = "$size" &&
            test "$(sha256sum "$archive" | cut -d " " -f 1)" = "$digest"; then
            break
        fi
        test "$attempt" -lt 3 || exit 1
        sleep "$attempt"
    done
fi
test "$(wc -c < "$archive" | tr -d ' ')" = "$size"
test "$(sha256sum "$archive" | cut -d " " -f 1)" = "$digest"
gzip -t "$archive"
# Extraction is only from the exact authenticated upstream SDK archive.
tar xzf "$archive" -C "$sdk"
test "$(head -n 1 "$sdk/go/VERSION")" = go1.27.2
mkdir -p builddir
cat > builddir/go-sdk-environment.sh <<'ENV'
export GOROOT="$PWD/builddir/go-sdk/go"
export PATH="$GOROOT/bin:$PATH"
export GOTOOLCHAIN=local GOPROXY=off GOSUMDB=off
ENV
. builddir/go-sdk-environment.sh
go version > builddir/provider-go.txt
{
    printf 'source=%s\nsha256=%s\nbytes=%s\n' "$url" "$digest" "$size"
    sha256sum "$sdk/go/bin/go" "$sdk/go/LICENSE"
} > builddir/provider-go-sdk.txt
cp "$sdk/go/LICENSE" builddir/provider-go-LICENSE
