# Phase 05 follow-up: shaded transitions and native BSD verification

Date: 2026-10-03
Status: shaded conversion gaps repaired; phase remains under verification.

## Changes

- Unified shaded and unshaded transition planning. Read source client pixels
  before rebinding destination chrome and preserve the moved strip's origin.
- Save converted full geometry in the slot actually used by unshade (`g.max`
  or `g.normal`), then resize only the collapsed strip. This prevents
  `update_absolute_geometry` from restoring the old uncollapsed dimension.
- Initialize maximize/fullscreen destination rectangles from the destination
  monitor before intersecting maximize work areas. The previous maximize
  branch passed an uninitialized rectangle to the intersection function.
- Arm the configure echo gate before frame setup, and commit the transition
  tuple only after a valid plan result.
- Added real-X geometry tests, native NOOP/fixed-client policy cases, BSD
  test registration, a native build/install runner, and BSD CI VM jobs.
- Fixed BSD iconv detection: FreeBSD selected libc while using ports headers;
  OpenBSD missed the installed ports library. Configuration now checks that
  the selected header and library link together.

## Evidence

- Linux GCC build: all 23 registered tests passed after the iconv fix.
- Expanded Xorg dummy regression: 96 shaded round trips passed across all
  eight directions for ordinary, fixed-size, maximized, and reserved-work-area
  clients at 1.25x, 1.5x, and 2x.
- Six additional maximize/fullscreen transition/restore round trips passed.
- Original HEAD fails the new shaded regression on its first case: expected
  250x150 client pixels at 1.25x; observed 198x150. Fixed code passes.
- FreeBSD 15.1 ARM64 in local QEMU: 18/18 tests and staged install passed.
- OpenBSD 7.9 ARM64 in local QEMU: 18/18 tests and staged install passed.
- Both native X session tests exercised 24 real-client shade/unshade cases.
- Python tooling checks, shell syntax, workflow YAML, and diff checks passed.

Raw Linux logs are in `builddir/codex-2026-10-03/`. Native BSD logs, package
versions, checksums, and compressed Meson/X session evidence are under
`builddir/qemu-bsd/{freebsd,openbsd}/`. These paths are ignored local artifacts.
See `dev-docs/BSD.md` for repeatable native verification.

## Remaining decisions and verification

- Decide the release support matrix: BSD release lines and CPU architectures.
  ARM64 local results do not establish AMD64 compatibility. CI jobs are ready
  but have not run; this checkout has no Git remote.
- Actual opaque pointer-drag and toolkit feedback scenarios remain uncovered
  by this new command-driven matrix. Do not claim the entire release matrix.
- Phase 4's two live resource-cache lookup checks remain outstanding.
- Physical GPU/hotplug/compositor checks and native Linux ASan remain pending.
- Fullscreen windows cannot be shaded (`CMD_WindowShade` rejects them); real
  fullscreen transition and restore behavior is covered separately.

The historical 05-REVIEW and 05-VERIFICATION reports are preserved as the
record of the defects found before this repair.
