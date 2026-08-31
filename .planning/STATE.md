---
gsd_state_version: 1.0
milestone: v0.1 Monitor and HiDPI Foundations
current_phase: 03
current_phase_name: Geometry and Ownership Contracts
status: executing
stopped_at: Completed 03-05-PLAN.md
last_updated: "2026-08-31T11:23:33.779Z"
last_activity: 2026-08-31
last_activity_desc: Phase 03 execution started
state_head: 0afd3f97cec208a1a9d5b3116d24334cfff1972e
progress:
  total_phases: 8
  completed_phases: 2
  total_plans: 18
  completed_plans: 18
---

# Project State

## Project Reference

See: .planning/PROJECT.md (updated 2026-08-30)

**Core value:** Users can depend on a responsive X11 desktop whose windows remain usable and visually consistent across changing monitor layouts and scale factors.
**Current milestone:** v0.1 Monitor and HiDPI Foundations
**Current focus:** Phase 03 — Geometry and Ownership Contracts

## Current Position

Phase: 03 (Geometry and Ownership Contracts) — EXECUTING
Plan: 5 of 5
Status: Phase 03 plans complete
Last activity: 2026-08-31 — Completed 03-05-PLAN.md

Progress: [██░░░░░░░░] 25%

## Performance Metrics

**Velocity:**

- Total plans completed: 13
- Average duration: 24 min
- Total execution time: 3.65 hours

**By Phase:**

| Phase | Plans | Total | Avg/Plan |
|-------|-------|-------|----------|
| 01 | 8 | - | - |
| 02 | 5 | - | - |

**Recent Trend:**

- Last 5 plans: 8 min, 17 min, 21 min, 46 min, 59 min
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
| Phase 02 P01 | 59min | 3 tasks | 8 files |
| Phase 02 P02 | 15min | 3 tasks | 8 files |
| Phase 02 P03 | 14min | 3 tasks | 9 files |
| Phase 02 P04 | 9min | 2 tasks | 5 files |
| Phase 02 P05 | 21 min | 2 tasks | 3 files |
| Phase 03 P01 | 13 min | 3 tasks | 6 files |
| Phase 03 P02 | 8 min | 2 tasks | 3 files |
| Phase 03 P03 | 14 min | 3 tasks | 6 files |
| Phase 03 P04 | 5 min | 3 tasks | 6 files |
| Phase 03 P05 | 6 min | 3 tasks | 7 files |

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
- [Phase 02]: Represent scale as quarter units from 4 through 16 so policy and conversion paths never require floating-point arithmetic.
- [Phase 02]: Resolve automatic DPI only when both axes are 50..500 DPI inclusive and differ by at most 10% relative to their arithmetic mean.
- [Phase 02]: Initialize every staged monitor to an explicit safe 1x result before publication so failure paths retain a total value.
- [Phase 02]: Treat RandR Monitor names as exact case-sensitive bytes; numeric and g/c/p tokens receive no legacy alias interpretation.
- [Phase 02]: Stage complete replacement entries before mutating the authoritative store so invalid input and allocation failure retain prior policy.
- [Phase 02]: Encode control and backslash bytes before diagnostics cross the logging seam, while retaining the original exact name in policy storage.
- [Phase 02]: Associate topology facts with runtime monitors by exact name and RandR Atom when available, rejecting missing or duplicate associations.
- [Phase 02]: Finalize the sole scale configuration after startup directives and reject later direct declarations until reload support exists.
- [Phase 02]: Use one changed-refresh callback coordinator to guarantee scale recomputation precedes EWMH, broadcast, pan-frame, and Status effects.
- [Phase 02]: PrintInfo Scale reads the core-owned monitor_scale_config state and attached effective_scale values rather than creating a parallel policy path.
- [Phase 02]: Status v4 and module packets remain actual-pixel interfaces without scale metadata; PrintInfo Scale is the inspection boundary.
- [Phase 02]: Scenario-owned startup directives execute as separate argv -c command pairs in exact order, including names with spaces and quotes. — Quotes and spaces must survive as one argv element with no shell interpretation.
- [Phase 02]: add_post_finalize_scale_inspection() appends a separate -c, Schedule 0 PrintInfo Scale pair so evidence is emitted after initial finalization. — Assertions must wait for the scheduled post-finalization record rather than earlier unrelated log output.
- [Phase 02]: Status v4 and module packets remain actual-pixel interfaces; live-X scale proof reads PrintInfo Scale and queried Xorg-dummy facts only. — Phase 2 must not expand Status or module protocol while proving initial scale policy.
- [Phase 03]: Ownership compares literal snapshot x,y,width,height and never folds through DisplayWidth.
- [Phase 03]: Equal overlap and clone ties compare name_atom then strcmp(name), independent of array order.
- [Phase 03]: Empty input returns an initialized FALLBACK result without aborting; overlap area that does not fit in int returns MONITOR_OWNERSHIP_OVERFLOW.
- [Phase 03]: FindScreenOfXY is a 1x1 point wrapper with incumbent None and hysteresis 0, then a logged primary or enabled non-global fallback.
- [Phase 03]: Keep an overlapping incumbent when extra_overlap is at most MONITOR_OWNERSHIP_HYSTERESIS_PX; steal only when extra_overlap is strictly greater. — GEO-01 seam stability: extra_overlap 8 keeps, extra_overlap 9 steals.
- [Phase 03]: An overlapping incumbent wins before name_atom/name identity on equal overlap. — Identity tie-break applies only when there is no overlapping incumbent.
- [Phase 03]: Mapped windows resolve over fw->g.frame; unmapped windows keep the maximized-or-normal rectangle. — PA-05 reference geometry for update_fvwm_monitor.
- [Phase 03]: Unresolved ownership names fall back to existing fw->m or monitor_by_primary without aborting. — T-03-07: NULL snapshot or unresolved name must not abort.
- [Phase 03]: Increment snap without CS_ROUND_UP uses nearest multiple of inc with halfway rounding away from zero; CS_ROUND_UP ceiling snap stays in constrain_size. — PA-08 plus interactive MotionNotify edge checks must not move into libs.
- [Phase 03]: Aspect uses checked __int128 cross-products; PBaseSize is subtracted only when that flag is set and base is not greater than min. — GEO-03 forbids double aspect and using min as the aspect base.
- [Phase 03]: On ICCCM_CONSTRAIN_OVERFLOW or INVALID_ARGUMENT, constrain_size logs and restores the pre-constrain client size rather than inventing a plausible size. — T-03-01: untrusted hint arithmetic must not produce a plausible size after overflow.
- [Phase 03]: Work rect is the snapshot monitor rectangle inset by that monitor's strut integers in root pixels, including negative si->x.
- [Phase 03]: Negative remaining width or height, or invalid insets, initialize the output rectangle to zeros and return MONITOR_OWNERSHIP_INVALID_ARGUMENT.
- [Phase 03]: Stored ewmh_working_area is already in root pixels, so EWMH_GetWorkAreaIntersection intersects without adding m->si->x again.
- [Phase 03]: Strut strips use monitor si extents; they do not wrap through DisplayWidth or DisplayHeight.
- [Phase 03]: When StartsOnScreen is set but not honored, screen_g comes from FScreenGetScrRect CURRENT rather than 0,0,DisplayWidth,DisplayHeight.
- [Phase 03]: EWMH, ICCCM, configure, frame extents, work area, and module packets stay actual X11 pixels; Status and module packets were not expanded.
- [Phase 03]: Work-area insets are work origin minus si origin; consumers must not add si origin onto stored ewmh_working_area. — CR-01: stored ewmh_working_area is already root pixels after 03-04.
- [Phase 03]: Snap copies the stored root-pixel work rect plus page offset only. — Adding si origin onto an already-absolute work origin double-counts (e.g. -1920 + -1856 = -3776).
- [Phase 03]: $[wa.x]/$[wa.y] remain the stored root-pixel origin and are documented as such, not strut insets. — PA-12: prefer the coherent FVWM-X root-pixel work-area rectangle over the pre-03-04 inset meaning.

### Pending Todos

None yet.

### Blockers/Concerns

- Automatic DPI thresholds, ownership hysteresis, and toolkit transition feedback require empirical calibration during their owning phases.
- The event loop must retain bounded work and nonblocking module behavior throughout topology reconciliation.

## Deferred Items

| Category | Item | Status | Deferred At | Milestone |
|----------|------|--------|-------------|-----------|
| Compatibility | Pixel-sensitive application opt-out policy | Future | Initialization | v0.1 |
| Scale policy | Arbitrary scale percentages outside quarter steps | Future | Initialization | v0.1 |

## Session Continuity

Last session: 2026-08-31T11:23:33.209Z
Stopped at: Completed 03-05-PLAN.md
Resume file: None
