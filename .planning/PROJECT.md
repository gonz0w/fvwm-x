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

### Active

- [ ] Derive a safe automatic scale for each active monitor and permit an
  explicit per-monitor configuration override
- [ ] Apply monitor scale consistently to decorations, fonts, icons, cursors,
  and interactive hit targets
- [ ] Keep placement, maximize, fullscreen, edge, and work-area geometry
  correct in mixed-scale layouts
- [ ] Preserve a window's logical size when it crosses monitors with different
  scales
- [ ] Reconcile monitor ownership and scaled geometry safely after hotplug,
  removal, primary changes, negative coordinates, and overlapping layouts
- [ ] Verify scale and topology policy with deterministic unit tests and live
  X11 integration scenarios

### Out of Scope

- Native Wayland compositor or backend — FVWM-X remains an X11 window manager
- Integrated compositing — external compositors remain the supported model
- Compatibility with every FVWM2/FVWM3 configuration or module ABI — clean
  breaks are acceptable when they produce a coherent FVWM-X design
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

Mixed-DPI support currently has no unified scale abstraction. Geometry and
drawing policy are spread across large, tightly coupled files including
`fvwm/move_resize.c`, `fvwm/borders.c`, `fvwm/placement.c`, and
`libs/FScreen.c`. v0.1 should extract pure scale and geometry policy where
possible before wiring X11 side effects.

## Constraints

- **Platform**: X11/XRandR 1.5+ only — native Wayland is explicitly excluded
- **Architecture**: Keep authoritative X state single-threaded — avoid races
  and preserve existing event-loop ordering
- **Responsiveness**: No monitor or scale operation may introduce blocking work
  into the main event loop
- **Rendering**: Scale FVWM-owned UI and geometry, not arbitrary client pixels
- **Compositing**: Integrate cleanly with external compositors; do not add one
- **Compatibility**: Prefer a coherent FVWM-X contract over legacy config or
  module compatibility when they conflict
- **Verification**: Pure policy requires native unit coverage; X-visible
  behavior requires nested-X integration coverage on native Linux CI
- **Environment**: Local macOS verification uses Podman x86_64 emulation;
  AddressSanitizer remains a native Linux CI responsibility

## Key Decisions

| Decision | Rationale | Outcome |
|----------|-----------|---------|
| Name the internal product FVWM-X and start milestones at v0.1 | Treat modernization as a new product line rather than an upstream patch release | — Pending |
| Start with monitor and HiDPI foundations | Later rendering, input, and configuration work needs one authoritative scale model | — Pending |
| Use explicit per-monitor scale with automatic DPI fallback | Overrides provide deterministic control while automatic behavior gives a usable default | — Pending |
| Preserve logical window size across scale changes | Perceived size should remain stable when users move windows between monitors | — Pending |
| Remain X11-only with external compositing | Limits scope to improving the current architecture instead of building a compositor platform | — Pending |
| Permit configuration and module compatibility breaks | Legacy constraints must not prevent a coherent modern runtime model | — Pending |

## Evolution

This document evolves at phase transitions and milestone boundaries.

**After each phase transition** (via `/gsd-transition`):
1. Requirements invalidated? Move to Out of Scope with reason
2. Requirements validated? Move to Validated with phase reference
3. New requirements emerged? Add to Active
4. Decisions to log? Add to Key Decisions
5. "What This Is" still accurate? Update if drifted

**After each milestone** (via `/gsd-complete-milestone`):
1. Full review of all sections
2. Core Value check — still the right priority?
3. Audit Out of Scope — reasons still valid?
4. Update Context with current state

---
*Last updated: 2026-08-29 after initialization*
