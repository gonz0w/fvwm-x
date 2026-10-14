# FVWM-X

## What This Is

FVWM-X is an internal, modernized X11 window manager derived from FVWM3. It
keeps FVWM's efficient event-driven core and process-isolated module model while
making multi-monitor, mixed-DPI, input, configuration, rendering, and
accessibility behavior reliable for current desktop hardware.

The first milestone establishes monitor and HiDPI foundations for users with
mixed-density XRandR layouts.

## Core Value

Users can depend on a responsive X11 desktop whose windows remain usable and
visually consistent across changing monitor layouts and scale factors.

## Requirements

### Validated

- ✓ Manages X11 clients through ICCCM/EWMH-compatible framing, focus, stacking,
  placement, virtual desktops, and command dispatch — existing FVWM3 core
- ✓ Discovers XRandR 1.5 monitors and tracks monitor geometry, primary state,
  per-monitor desks, viewports, and topology changes — existing monitor model
- ✓ Extends the core with isolated module processes over bounded, nonblocking
  IPC — hardened before v0.1
- ✓ Supports Xft/fontconfig text, PNG/SVG/XPM images, XRender, Xcursor, and
  external compositors — existing optional rendering stack
- ✓ Builds with Meson and exercises focused native tests under GCC, Clang,
  ASan, and UBSan CI — hardened before v0.1
- ✓ Derive a safe automatic scale for each active monitor and permit an
  explicit per-monitor configuration override — Phase 2
- ✓ Verify scale and topology policy with deterministic unit tests and live
  X11 integration scenarios — Phase 1–2
- ✓ Resolve window ownership, ICCCM size constraints, and work-area, strut,
  and placement geometry in literal root pixels — Phase 3
- ✓ Apply monitor scale consistently to decorations, fonts, icons, cursors,
  and interactive hit targets — Phase 4

### Active

- [ ] Keep placement, maximize, fullscreen, edge, and work-area geometry
  correct in mixed-scale layouts
- [ ] Preserve a window's logical size when it crosses monitors with different
  scales
- [ ] Reconcile monitor ownership and scaled geometry safely after hotplug,
  removal, primary changes, negative coordinates, and overlapping layouts

### Out of Scope

- Native Wayland compositor or backend — FVWM-X remains an X11 window manager
- Integrated compositing — external compositors remain the supported model
- A replacement configuration language or theme format — preserve existing
  FVWM configuration and theme behavior during internal modernization
- Fractional client-content scaling — X11 clients control their own rendering;
  v0.1 scales window-manager-owned UI and window geometry
- Configuration-language redesign — deferred until monitor scaling establishes
  the runtime model configuration must express

## Context

The codebase is a single-threaded event-driven C11 window-manager core. X
events, module IPC, timers, scheduled commands, and deferred signal work share
the loop in `fvwm/events.c`. Authoritative window state lives in `FvwmWindow`
and process-wide `ScreenInfo Scr`; XRandR monitor state is maintained by
`libs/FScreen.c` and consumed throughout placement, movement, borders, EWMH,
and virtual desktop code.

The pre-v0.1 hardening tranche added bounded image handling, incremental module
framing, output backpressure, event-loop fairness, deferred signal work,
sanitizer CI, release safety, and focused native tests. The formal repository
map is in `.planning/codebase/`.

Phase 2 added a unified exact-quarter scale domain in `libs/monitor_scale.c`
and a core-owned transactional store in `fvwm/monitor_scale_config.c`. Phase 3
extracted monitor ownership, ICCCM size constraints, and root-pixel work-area
contracts into `libs/monitor_ownership.c` and `libs/icccm_size_hints.c`.
Drawing and chrome policy remain in `fvwm/move_resize.c`, `fvwm/borders.c`,
and `fvwm/placement.c`. Phase 4 added `wm_ui_scale_px` plus scale-aware
Picture/Flocale caches so FVWM-owned chrome, fonts, icons, cursors, and hit
targets convert once into actual X11 pixels.

## Constraints

- **Platform**: X11/XRandR 1.5+ only — native Wayland is explicitly excluded
- **Target systems**: Linux and FreeBSD with Xorg, and OpenBSD with its default
  Xenocara X11 stack, on both AMD64 and ARM64. The initial BSD release baseline
  is FreeBSD 15.1 and OpenBSD 7.9. Build and X session checks are release gates
  for all six OS/architecture combinations; local BSD verification uses QEMU.
- **Architecture**: Keep authoritative X state single-threaded — avoid races
  and preserve existing event-loop ordering
- **Responsiveness**: No monitor or scale operation may introduce blocking work
  into the main event loop
- **Rendering**: Scale FVWM-owned UI and geometry, not arbitrary client pixels
- **Compositing**: Integrate cleanly with external compositors; do not add one
- **Compatibility**: Preserve FVWM themes, configuration syntax, and command
  behavior; specific breaks require an explicit user decision
- **Verification**: Pure policy requires native unit coverage; X-visible
  behavior requires nested-X integration coverage on native Linux CI
- **Environment**: Local macOS verification uses Podman x86_64 emulation;
  AddressSanitizer remains a native Linux CI responsibility

## Key Decisions

| Decision | Rationale | Outcome |
|----------|-----------|---------|
| Name the internal product FVWM-X and start milestones at v0.1 | Treat modernization as a new product line rather than an upstream patch release | — Pending |
| Start with monitor and HiDPI foundations | Later rendering, input, and configuration work needs one authoritative scale model | — Pending |
| Use explicit per-monitor scale with automatic DPI fallback | Overrides provide deterministic control while automatic behavior gives a usable default | Shipped in Phase 2 |
| Keep EWMH, ICCCM, configure, frame-extents, work-area, and module geometry in actual X11 pixels; derive work-area insets from stored root-pixel rectangles | Prevents double-counting monitor origin on negative-origin layouts | Shipped in Phase 3 |
| Scale FVWM-owned chrome once through wm_ui_scale_px; cache fonts and pictures by scale_quarters, target size, and generation; do not mutate display-wide font DPI or cursor default size | Keeps mixed-DPI chrome internally consistent without retargeting client pixels | Shipped in Phase 4 |
| Preserve logical window size across scale changes | Perceived size should remain stable when users move windows between monitors | — Pending |
| Remain X11-only with external compositing | Limits scope to improving the current architecture instead of building a compositor platform | — Pending |
| Preserve FVWM themes and configuration files | User priority: simplify internals without requiring users to rewrite their desktop | Approved 2026-10-04 |

## Current tracking

GSD has been retired. This file preserves milestone context; current work and
verification are tracked in [the project tracker](../../PROJECT-TRACKER.md).

Last updated: 2026-10-04 — configuration/theme compatibility is a requirement.
