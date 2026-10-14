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
FreeBSD/arm64)
    platform=freebsd-arm64; size=66755349
    digest=b77d9b8aa9cde90e1045a13f6166e46e370ef7225556e9c2043d351c59557ecb ;;
FreeBSD/amd64)
    platform=freebsd-amd64; size=70290004
    digest=3baf61eec549b0c3f638d82636388e36abf94aa14814a011f011d2648ad1eded ;;
OpenBSD/arm64)
    platform=openbsd-arm64; size=66522918
    digest=befd66f13c7bd83fa3fa794a3deec5a1d6b5bf64817afbedaf258b10257f875c ;;
OpenBSD/amd64)
    platform=openbsd-amd64; size=70013564
    digest=ab74a2b31761a8c34e9a1059b0f9c82a7db42780fde4e35bac270758d45816a1 ;;
*) echo 'The Go SDK recipe requires supported FreeBSD/OpenBSD.' >&2; exit 1 ;;
esac
url=https://dl.google.com/go/go1.27.2.$platform.tar.gz
# Keep SDK bytes outside the provider's collected build trees. Preserve its
# identity/license with the evidence; the compiler archive is independently pinned.
sdk=builddir-bsd-go-sdk
if test -e "$sdk" || test -L "$sdk"; then
    echo 'Refuse to overwrite an existing job SDK.' >&2
    exit 1
fi
umask 077
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
                FreeBSD) fetch -T 30 -o "$archive" "$url" ;;
                OpenBSD) ftp -4 -w 30 -o "$archive" "$url" ;;
            esac) && test "$(wc -c < "$archive" | tr -d ' ')" = "$size" &&
            test "$(sha256 -q "$archive")" = "$digest"; then
            break
        fi
        test "$attempt" -lt 3 || exit 1
        sleep "$attempt"
    done
fi
test "$(wc -c < "$archive" | tr -d ' ')" = "$size"
test "$(sha256 -q "$archive")" = "$digest"
gzip -t "$archive"
# Extraction is only from the exact authenticated upstream SDK archive.
tar xzf "$archive" -C "$sdk"
test "$(head -n 1 "$sdk/go/VERSION")" = go1.27.2
mkdir -p builddir-bsd
cat > builddir-bsd/go-sdk-environment.sh <<'ENV'
export GOROOT="$PWD/builddir-bsd-go-sdk/go"
export PATH="$GOROOT/bin:$PATH"
export GOTOOLCHAIN=local GOPROXY=off GOSUMDB=off
ENV
. builddir-bsd/go-sdk-environment.sh
go version > builddir-bsd/provider-go.txt
{
    printf 'source=%s\nsha256=%s\nbytes=%s\n' "$url" "$digest" "$size"
    sha256 "$sdk/go/bin/go" "$sdk/go/LICENSE"
} > builddir-bsd/provider-go-sdk.txt
cp "$sdk/go/LICENSE" builddir-bsd/provider-go-LICENSE
