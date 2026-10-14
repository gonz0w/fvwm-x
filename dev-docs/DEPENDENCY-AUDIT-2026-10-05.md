# Native dependency and package audit — 2026-10-05

Status: PNG baseline and reviewed XPM vendor fixes qualified in explicit profiles; broader package security
qualification remains open. This is LIB-02/LIB-07 evidence, not a certification.
The independent [PNG recovery repair](verification/VERIFICATION-2026-10-05-PNG-RECOVERY.md)
passes its runtime gates with the observed packages.

## Observed qualification environments

| Target | Installed PNG | Installed XPM | X server / server-font library |
| --- | --- | --- | --- |
| Ubuntu 24.04 ARM64 | 1.6.43-5ubuntu0.6 | 1:3.5.17-1ubuntu0.24.04.2 | xserver-xorg-core 2:21.1.12-1ubuntu1.8; libXfont2 1:2.0.6-1+deb13u1build0.24.04.2 |
| Debian trixie AMD64 | 1.6.48-1 | 1:3.5.17-1+b3 | xserver-xorg-core 2:21.1.16-1.3+deb13u4; libXfont2 1:2.0.6-1+deb13u1 |
| FreeBSD 15.1-p4 ARM64 | 1.6.58 | absent in this profile | Xorg/Xvfb 21.1.24,1; libXfont2 2.0.9 |
| FreeBSD 15.1-p4 AMD64 | 1.6.58 | absent in this profile | Xorg/Xvfb 21.1.24,1; libXfont2 2.0.9 |
| OpenBSD 7.9 ARM64 | 1.6.58 | base pkg-config 3.5.18 | Xenocara server 21.1.21; initially no installed syspatches |
| OpenBSD 7.9 AMD64 | 1.6.59 | base pkg-config 3.5.18 | Xenocara server 21.1.21; initially no installed syspatches |

Inventories include all declared pkg-config dependencies, package-manager
versions/origins, zlib/Expat/server-font observations, optional feature scope,
server binaries and relevant installed changelogs. Some Debian slim-image
changelogs are absent; missing evidence is recorded. OpenBSD base-library
versions and ports packages are distinct. FreeBSD has no enabled XPM decoder in
this tested profile, so its tests do not qualify XPM decoding.

Vendor Xorg shell launchers were resolved to their actual implementation paths
and hashes. Ubuntu's server reports 21.1.11 while its package is 21.1.12-1ubuntu1.8;
the launcher and implementation belong to the expected vendor installation.
That discrepancy is retained and does not establish its latest patch status.
A version string alone is not used as proof of a security fix.

## Relevant PNG baseline and loader paths

[Upstream libpng](https://www.libpng.org/pub/png/libpng.html) publishes 1.6.59,
released 2026-09-30, with the CVE-2026-46675 compressed ancillary-chunk fix.
[Debian's tracker](https://security-tracker.debian.org/tracker/CVE-2026-46675)
identifies trixie 1.6.48-1+deb13u6 as fixed. The observed Debian 1.6.48-1 is older;
its configured repository offers the fixed package. Ubuntu's corresponding
[libpng1.6 entry](https://ubuntu.com/security/CVE-2026-46675) still needs evaluation;
no matching fix is established for the observed Noble package. Its cached
candidate equals its installed version.

The refreshed FreeBSD repository offers png 1.6.58 on both architectures, and
OpenBSD ARM64's repository also offers 1.6.58. No matching security backport proof
was found for those exact PNG packages. OpenBSD AMD64 has upstream 1.6.59.
FreeBSD pkg audit does not list this PNG advisory in the observed database;
absence from that database does not override upstream evidence.

The FVWM loader calls png_read_info, transforms palette/gray/transparency data,
then png_read_image and png_read_end. It does not consume text/profile metadata.
The lower-level compressed ancillary path may therefore be relevant; that is a
source-based inference, not a reproduced advisory-specific failure. Simplified
API or unrelated setter/getter advisories are not automatically attributed to
this loader. SVG and other library users require their own reachability review.
The recovery fix does not repair these library internals.

A concrete [secure PNG configure proposal](decisions/PNG-SECURITY-BASELINE-PROPOSAL.md)
was explicitly approved on 2026-10-05; six-target patched profiles are accepted in the [baseline report](verification/VERIFICATION-2026-10-05-PNG-BASELINE.md). It accepts verified vendor
backports through an explicit builder assertion; it does not reject fixed
Debian packages merely because their upstream version is lower. Production
Meson now refuses older unverified PNG libraries by default.

## XPM, X server and font packages

[X.org security notices](https://www.x.org/Development/Security/) identify
libXpm 3.5.19 for CVE-2026-4367 and the July server/font fixes at Xorg 21.1.24
and libXfont2 2.0.8. The observed FreeBSD server/font versions exceed those
upstream fix versions; this is a scoped baseline, not a verdict on all advisories.

[Ubuntu's XPM tracker](https://ubuntu.com/security/CVE-2026-4367) gives the Noble
fixed package as 1:3.5.17-1ubuntu0.24.04.1. The installed .2 package and its
changelog record the backport. [Debian](https://security-tracker.debian.org/tracker/CVE-2026-4367)
fixes it in 1:3.5.17-1+deb13u1; the observed +b3 package predates that fix and the
configured repository offers the replacement. A private extracted, authenticated
fixed PNG/XPM package profile now passes the full Debian suite and staged image
checks; its system packages remain unchanged. XPM remains enabled on these
Linux targets. OpenBSD's base 3.5.18 now has vendor proof: the signed 7.9 Xenocara source
archive contains both fix hunks from the signed [7.8 advisory](https://www.openbsd.org/errata78.html). Reverse dry-run comparison succeeds.
Installed XPM library hashes on both snapshots match their architecture's signed
7.9 X base release set. This establishes vendor release/source provenance; it
is not a locally reproduced build. The older version string alone was not a
vulnerability verdict. Evidence is in `xpm-vendor-review/` under the dated build
evidence directory.

The Debian server matches its published July fixed package. Ubuntu's server
[tracker entry](https://ubuntu.com/security/CVE-2026-55999) still needs evaluation;
its exact July server fixes are unqualified here. The installed Ubuntu
libXfont2 changelog records CVE-2026-56001/56002/56003 backports.

Both OpenBSD snapshots initially had no syspatches. The [7.9 errata](https://www.openbsd.org/errata79.html)
include X-server fixes 001 and server/font fixes 020. Vendor syspatch verifies
and applies cumulative signed updates; it cannot install an arbitrary subset.
All 30 available updates were applied to each task-owned snapshot, and both
were rebooted into GENERIC.MP#15. Both updated guests pass 66/66 full tests and all 47 installed default-X
outcomes each, with matching frozen sources and staged install bytes.
The original guests, disk images and host packages have not been updated.
Snapshot changes are session-scoped; the next fresh session must reproduce the
signed update step or use a separately prepared patched verification image.

## Transitive scope and next work

The inventory also records FreeType, Fontconfig, zlib, librsvg and libevent.
FreeBSD pkg audit reports Expat 2.8.2 and Python 3.12.14 findings. Expat's relevant
linked paths need review; Python poplib/imaplib notices are not a demonstrated
WM path. The complete transitive advisory review is open. Native Linux ASan/UBSan
CI is also unrun, and no release or security certification follows from these
runtime passes.

The approved PNG baseline now has qualified upstream/private or vendor-fixed
profiles, preserved optional formats and recorded actual loaded library paths.
Retain signed OpenBSD refresh and patched profiles in reproducible release gates.
Unused PNG ancillary processing, decoder work/ownership bounds and isolation
remain IMAGE-01 work. Remaining vendor/transitive advisory mapping and native
sanitizer qualification remain LIB-07/LIB-02 work.

Evidence folders: `builddir/codex-2026-10-05/script-text-limit/` (initial full
inventories/provenance), `image-loader/` (expanded inventories, repository/advisory
queries and qualified recovery archive), `package-refresh/` (signed-update logs,
post-reboot metadata and new qualification), and `png-security-policy/`
(reviewed candidate, six isolated real-Meson cases and private dependency provenance). No host update, package
publication, remote push or original-image modification was performed.
