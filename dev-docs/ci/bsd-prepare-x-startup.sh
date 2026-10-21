#!/bin/sh
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or any later version.
# Signed updates and startup evidence in an owned disposable OpenBSD image.
set -eu
export PATH="/usr/local/bin:/usr/X11R6/bin:/usr/bin:/bin:/usr/sbin:/sbin"
test "$(uname -s)" = OpenBSD
case $(uname -m) in
    amd64) driver=vesa ;;
    arm64) driver=wsfb ;;
    *) echo 'Unsupported OpenBSD startup profile.' >&2; exit 1 ;;
esac
unset TRUSTED_PKG_PATH
mkdir -p builddir-bsd
uname -a > builddir-bsd/before-system.txt
syspatch -l > builddir-bsd/before-syspatch.txt
sha256 /usr/X11R6/bin/Xorg "/usr/X11R6/lib/modules/drivers/${driver}_drv.so" \
    > builddir-bsd/before-server.txt
if ! command -v python3 >/dev/null; then
    FETCH_CMD='/usr/bin/ftp -4 -w 30' pkg_add -I python3
fi
python3 --version
# syspatch refuses concurrent kernel reordering. Preserve that protection;
# wait at most four minutes, and refuse a failed process inventory.
for attempt in $(jot 121 0); do
    if pgrep -f '/usr/libexec/reorder_kernel' >/dev/null; then
        test "$attempt" -lt 120 || exit 1
        sleep 2
    else
        test "$?" -eq 1 || exit 1
        break
    fi
done
# Exit 2 means the signed patch check found nothing new to install. Fresh
# release images may already contain every erratum; other failures remain fatal.
status=0
syspatch || status=$?
case $status in
    0|2) ;;
    *) echo "OpenBSD signed update failed (status $status)." >&2
        exit "$status" ;;
esac
# Status 2 also covers a syspatch self-update. Never certify that as fully
# patched while the newly installed tool still reports pending signed errata.
syspatch -c > builddir-bsd/pending-syspatch.txt
if test -s builddir-bsd/pending-syspatch.txt; then
    echo 'OpenBSD still has pending signed updates; rerun preparation.' >&2
    exit 1
fi
syspatch -l > builddir-bsd/after-syspatch.txt
sha256 /usr/X11R6/bin/Xorg "/usr/X11R6/lib/modules/drivers/${driver}_drv.so" \
    > builddir-bsd/after-server.txt
# Pair with qemu_bsd.py --reboot-after-prepare before running the checks.
