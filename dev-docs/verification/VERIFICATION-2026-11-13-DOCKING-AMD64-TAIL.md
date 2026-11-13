# Ubuntu and OpenBSD AMD64 docking continuation

The [#222](https://github.com/gonz0w/fvwm-x/issues/222) continuation used
`b7cdb93baa0a716fb0c4aa8b09024154fd1ef3ba`. The previous checkpoint
changed only documentation. The
[FreeBSD, Debian and earlier OpenBSD receipts](VERIFICATION-2026-11-13-DOCKING-AMD64.md)
and the earlier ARM64 and theme evidence remain applicable.

## Ubuntu 24.04 AMD64

The official Ubuntu image manifest digest was
`sha256:2198e4c65076e9cb0b3d695219df667c0e3b0072da64ca3c2648aca17dcecaf5`.
The disposable Docker container ran AMD64 userspace under CPU emulation
on ARM64. It checked its release and architecture, installed the documented
Linux build dependencies, and used Meson 1.7.0, the pinned Go SDK and the
private PNG source. The first execution stopped before FVWM-X configuration
because the PNG bootstrap output parent was missing. The reviewed correction
created that parent before bootstrap.

The corrected run built 36 install targets, staged them privately with
Meson, and verified the staged core version and controller and worker
identity. All nine docking cases passed on isolated Xorg dummy using
the installed binaries and data. The fixture took no screenshots.

| Receipt | SHA-256 |
| --- | --- |
| First execution log | `e484c0d4dde1d6607de06c973d5407ff39457a13227497a673ae00f13317a9b3` |
| Passing execution log | `6478c378b3098192aa5591db50b72b51feebc8e990e3dd2db34f0bdcb203fda2` |
| Staged inputs | `ce09ba5c3f81b14dab3554d5b8b16496e7918266d2ff33971580547f51366b2e` |
| Docking evidence | `652abb16f81c53f055378f416ea165f17eff1c406e2c7f0ea818521d786d001e` |

## OpenBSD 8.0 AMD64

The fresh stock-image TCG run used reviewed plan
`b52f99442cc54b5402ea042e67f76d461ec57e450226952835d458e79d5de410`.
The original timeout returned no phase evidence. On this continuation the
host stderr file was empty during live sampling and flushed when the
runner exited. Read-only snapshots from the owned guest tracked progress. Upstream PNG testing
was still running at 19 minutes after boot. All 36 PNG checks then passed
and Meson configured FVWM-X. The final stderr records Ninja reaching step 224 of 371. The 1,500-second run deadline expired before the install or X
check, and the runner stopped QEMU with status 0.

The observed bottleneck was fresh PNG preparation under CPU emulation,
which left under four minutes for the product build and X check. The
original run did not return enough evidence to assign its exact stopping
point. The next OpenBSD plan can reuse the previously qualified AMD64 PNG
prefix after validating its compiler, package ABI, and runtime loader
against the guest. Track the stock-image and documented default-X fixture
requirements separately.

| Receipt | SHA-256 |
| --- | --- |
| PNG completion snapshot | `6e25f2277b48a20fd97f6eb3ed8234f483e394025b13e44f9032d5144cbfadf1` |
| Core compilation snapshot | `77cc91baa8b1154eabfcbbe249df54876062fd227c98e32de0c718db642f4e04` |
| Final stderr | `fd889fb81b70175de2e0db5e63505fe3ced0011d92947c0892dced3796b6f52e` |
| Final runner status | `c7f3c5a7cc50e24dd4b439c990cc5a9778ecf5fe07d87877052b869c03d65213` |

## Physical qualification

Use a real dock and two outputs at different scales to verify unplug and
replug during a prepared restore, connector re-identification after
restart, and panel, window and pointer reachability on physical displays.
The earlier theme and simulated hotplug results remain in force.
