---
gsd_state_version: 1.0
milestone: v0.1 Monitor and HiDPI Foundations
current_phase: 02
current_phase_name: Scale Policy and Configuration
status: executing
stopped_at: Phase 2 context gathered
last_updated: "2026-08-30T18:22:53.291Z"
last_activity: 2026-08-30
last_activity_desc: Phase 01 complete, transitioned to Phase 2
state_head: 201c039367b5ea8863f812a148124e0db3b728a5
progress:
  total_phases: 8
  completed_phases: 1
  total_plans: 13
  completed_plans: 8
---

# Project State

## Project Reference

See: .planning/PROJECT.md (updated 2026-08-29)

**Core value:** Users can depend on a responsive X11 desktop whose windows remain usable and visually consistent across changing monitor layouts and scale factors.
**Current milestone:** v0.1 Monitor and HiDPI Foundations
**Current focus:** Phase 01 — Topology Snapshot and Live-X Harness

## Current Position

Phase: 02 (Scale Policy and Configuration) — READY TO EXECUTE
Plan: Not started
Status: Ready to execute
Last activity: 2026-08-30 — Phase 01 complete, transitioned to Phase 2

Progress: [██████████] 100%

## Performance Metrics

**Velocity:**

- Total plans completed: 8
- Average duration: 20 min
- Total execution time: 2.67 hours

**By Phase:**

| Phase | Plans | Total | Avg/Plan |
|-------|-------|-------|----------|
| 01 | 8 | - | - |

**Recent Trend:**

- Last 5 plans: 12 min, 8 min, 17 min, 21 min, 46 min
- Trend: Slower

**Per-Plan Metrics:**

| Plan | Duration | Tasks | Files |
|------|----------|-------|-------|
| Phase 01 P01 | 32min | 3 tasks | 7 files |
| Phase 01 P02 | 18min | 2 tasks | 4 files |
| Phase 01 P03 | 6min | 2 tasks | 5 files |
| Phase 01 P05 | 8min | 2 tasks | 6 files |
| Phase 01 P04 | 12min | 3 tasks | 8 files |
| Phase 01 P06 | 17min | 2 tasks | 4 files |
| Phase 01 P07 | 21min | 2 tasks | 2 files |
| Phase 01 P08 | 46min | 3 tasks | 4 files |

## Accumulated Context

### Decisions

Decisions are logged in PROJECT.md Key Decisions table.

- v0.1 keeps all X11, ICCCM, EWMH, and module geometry in actual pixels.
- Per-monitor effective scale follows override, validated automatic DPI, configured global fallback, then safe 1x.
- Geometry and ownership contracts precede UI resources; X-visible transitions follow destination UI metrics.
- Hotplug reconciliation and module integration follow the topology, scale, geometry, UI, and transition foundations.
- [Phase 01]: Canonical snapshots retain RandR Atom/name identity, root-pixel facts, and sorted deduplicated output membership.
- [Phase 01]: Reject overflowing pixel coordinate spans while preserving raw physical millimetre facts without DPI plausibility policy.
- [Phase 01]: Darwin native verification stages read-only source into a clean case-sensitive Podman volume.
- [Phase 01]: FScreen performs exactly one active XRRGetMonitors query per refresh and publishes only a fully copied and validated candidate.
- [Phase 01]: Complete all new legacy monitor, screen, and name allocations before changing the committed snapshot pointer.
- [Phase 01]: Keep raw primary and complete output facts authoritative in the snapshot; legacy synthetic primary and first-output fields are compatibility-only derivations.
- [Phase 01]: Topology refresh due time is the earlier of a trailing 50 ms quiet deadline and a first-event 250 ms hard cap. — Bounds burst coalescing latency while preserving one deferred generation.
- [Phase 01]: Clean refresh policy state returns -1, due state returns 0, and every positive fractional millisecond rounds up. — Distinguishes no deadline from due work and prevents zero-time busy looping.
- [Phase 01]: Refresh completion resets the entire generation regardless of changed, unchanged, or failed refresh outcome. — Keeps the pure policy independent of publication semantics while allowing later events to rearm cleanly.
- [Phase 01]: Build the complete topology cJSON tree detached and transfer it to the Status parent only after every allocation succeeds.
- [Phase 01]: Status v4 and PrintInfo Topology both read FScreenGetTopologySnapshot(), while existing screen fields and FVWM_STATUS_PIPE transport remain unchanged.
- [Phase 01]: PrintInfo quotes monitor names through cJSON escaping so valid UTF-8 control and quote bytes cannot forge additional log lines.
- [Phase 01]: Only root ConfigureNotify plus RandR screen, CRTC, output, output-property, and resource events arm topology refreshes; provider and lease events remain unrelated. — Keeps event marking complete but narrow and constant-time.
- [Phase 01]: Service due topology generations at the top of HandleEvents before consuming another queued X event. — Preserves the 250 ms hard cap under sustained event pressure.
- [Phase 01]: Only changed FScreen refreshes run EWMH, broadcast, pan-frame, and Status effects. — Unchanged and failed generations must preserve epoch and avoid duplicate downstream work.
- [Phase 01]: Only the test shim reads and atomically unlinks FVWM_TEST_XRR_FAIL_FILE; production source and ordinary FVWM environments expose no fault switch.
- [Phase 01]: Each fault-bearing invocation probes LD_PRELOAD against actual production FVWM first and runs the non-installed linker-wrap executable only when preload is not proven.
- [Phase 01]: Nested X processes use display-fd allocation, disabled TCP, isolated sessions, monotonic deadlines, and bounded reverse-order cleanup.
- [Phase 01]: Xvfb Monitor mutation support is established only when repeated xrandr --listmonitors queries expose the requested Monitor; command exit status is retained solely as evidence.
- [Phase 01]: The literal delete/recreate mutation is presented to FVWM as one settled server transition by briefly stopping event consumption until both argv operations complete.
- [Phase 01]: An unsupported Xvfb still receives every literal create/delete/recreate/delete command and must retain the exact baseline Status record and epoch.
- [Phase 01]: Xorg-dummy physical-size authority combines root --fbmm mutation with per-output WIDTH_MM and HEIGHT_MM properties because the dummy DDX does not propagate root millimeters into automatic Monitor facts.
- [Phase 01]: Rapid multi-command XRandR transitions briefly suspend FVWM event consumption so each operation is observed as one settled semantic generation.
- [Phase 01]: Failed-query verification selects preload first and uses a fresh wrapped-FVWM run only when that invocation proves preload cannot intercept the real query.
- [Phase 01]: Live-X CI remains a separate non-matrix Debian-container job with failure-only immutable evidence upload.

### Pending Todos

None yet.

### Blockers/Concerns

- Phase 1 requires native-Linux validation of Xvfb/Xorg-dummy RandR Monitor behavior.
- Automatic DPI thresholds, ownership hysteresis, and toolkit transition feedback require empirical calibration during their owning phases.
- The event loop must retain bounded work and nonblocking module behavior throughout topology reconciliation.

## Deferred Items

| Category | Item | Status | Deferred At | Milestone |
|----------|------|--------|-------------|-----------|
| Compatibility | Pixel-sensitive application opt-out policy | Future | Initialization | v0.1 |
| Scale policy | Arbitrary scale percentages outside quarter steps | Future | Initialization | v0.1 |

## Session Continuity

Last session: 2026-08-30T17:11:46.899Z
Stopped at: Phase 2 context gathered
Resume file: /Users/cam/DEV/fvwm3/.planning/phases/02-scale-policy-and-configuration/02-CONTEXT.md
