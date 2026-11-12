# Docking profile theme and platform qualification

PR379 is merged at `2da0ef133503b8b552a4bbed10bb3cd0212a176d`. The
previous Slate scene passed all nine durable docking cases. The same Linux
ARM64 core and controller binaries passed all nine cases in each of two
additional isolated Xorg scenes. The driver reused 44 passing command
receipts, including the original Slate scene.

Dune light with Sans 10 has a 367×383 editor frame. The enlarged
`classic-large` configuration uses the Slate palette and DejaVu Sans
Mono 18 and has a 666×529 editor frame. The editor and its 13 visible
interactive controls fit on the primary output in both captures. The
fixture window titles on the secondary have clipping or overlapping
decoration during the final placement capture. The screenshots document
the profile editor and docking result.

The desktop controller tests cross-compiled for Linux, FreeBSD and
OpenBSD on both AMD64 and ARM64. The cross-compile covers the Go controller
test package. The native production build and Xorg scene remain qualified
on Linux ARM64.

FreeBSD 15.1-RELEASE-p3 ARM64 and OpenBSD 8.0 ARM64 each compiled
and ran the standalone C docking-policy test and ran all six Go docking
tests natively. Both disposable guests exited cleanly. The first
FreeBSD attempt stopped on the fixture's unpatched release string. The
first OpenBSD attempt passed the C policy and storage tests but lacked
the palette file and package working directory needed by the remaining
controller tests. The corrected batch used the same executable inputs
and supplied the shipped palette fixture.

Full native product builds, install/default-X gates and physical
docking remain open under #222. The BSD passes cover the docking units.

[Dune light](../../doc/screenshots/workspace-docking-profile-dune-light.png) ·
[Enlarged configuration](../../doc/screenshots/workspace-docking-profile-enlarged.png) ·
[Results and hashes](VERIFICATION-2026-11-11-DOCKING-QUALIFICATION.json)
