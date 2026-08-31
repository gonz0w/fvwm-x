---
phase: 04
slug: wm-owned-ui-metrics-and-resources
# status lifecycle: draft (seeded by plan-phase) -> validated (set by validate-phase)
status: draft
nyquist_compliant: false
wave_0_complete: false
created: 2026-08-31
---

# Phase 04 - Validation Strategy

> Per-phase validation contract for feedback sampling during execution.

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | Meson test runner; plain C assertion executables |
| **Config file** | `tests/meson.build` |
| **Quick run command** | `python3 tests/run_in_linux.py -- meson test -C builddir wm-ui-metrics supported-features monitor-scale --print-errorlogs` |
| **Full suite command** | `python3 tests/run_in_linux.py -- meson test -C builddir --print-errorlogs` |
| **Full live-X command** | `python3 tests/run_in_linux.py --live-x -- meson test -C builddir --suite live-x --print-errorlogs` |
| **Estimated runtime** | <10 seconds for focused native UI-metrics and feature tests; live-X and full-suite timing is environment-dependent |

---

## Sampling Rate

- **After every task commit:** Run the named C target through `tests/run_in_linux.py`.
- **After every plan wave:** Run `python3 tests/run_in_linux.py -- meson test -C builddir --print-errorlogs`.
- **Before `/gsd-verify-work`:** Full native suite must be green; existing live-X suite is non-regression only (no new live-X chrome cases required).
- **Max feedback latency:** 10 seconds for focused native tests.

---

## Per-Task Verification Map

| Task ID | Plan | Wave | Requirement | Threat Ref | Secure Behavior | Test Type | Automated Command | File Exists | Status |
|---------|------|------|-------------|------------|-----------------|-----------|-------------------|-------------|--------|
| W0-chrome | TBD | 0 | UI-01 | T-04-02 | Title/border/handle/font px at 1x, 1.25x, 1.5x, 2x, 4x from source 1/7/13 match `monitor_scale_apply` | native unit | `python3 tests/run_in_linux.py -- meson test -C builddir wm-ui-metrics --print-errorlogs` | No - Wave 0 | pending |
| W0-cache | TBD | 0 | UI-02 | T-04-01 | Same source name at two scales yields two cache entries; generation mismatch misses | native unit | `python3 tests/run_in_linux.py -- meson test -C builddir wm-ui-metrics --print-errorlogs` | No - Wave 0 | pending |
| W0-active | TBD | 0 | UI-03 | T-04-02 | Menu border/triangle/icon wanted sizes use the active-monitor factor | native unit | `python3 tests/run_in_linux.py -- meson test -C builddir wm-ui-metrics --print-errorlogs` | No - Wave 0 | pending |
| W0-hit | TBD | 0 | UI-04 | T-04-03 | Hot spot scaled and clamped; handle width scaled | native unit | `python3 tests/run_in_linux.py -- meson test -C builddir wm-ui-metrics --print-errorlogs` | No - Wave 0 | pending |
| W0-edge | TBD | 0 | UI-05 | T-04-02 | edge 1→2 at 2x; MoveThreshold 3→5 at 1.5x; snap 8→16 at 2x; edge 0 stays 0 | native unit | `python3 tests/run_in_linux.py -- meson test -C builddir wm-ui-metrics --print-errorlogs` | No - Wave 0 | pending |
| W0-frac | TBD | 0 | UI-06 | T-04-02 | Explicit title height below scaled font is raised; overflow initializes output 0 | native unit | `python3 tests/run_in_linux.py -- meson test -C builddir wm-ui-metrics --print-errorlogs` | No - Wave 0 | pending |
| W0-feat | TBD | 0 | TEST-05 | — | `HAVE_XCURSOR`, `HAVE_XRENDER`, `HAVE_PNG`, `HAVE_RSVG` are 1 in the supported profile | native unit | `python3 tests/run_in_linux.py -- meson test -C builddir supported-features --print-errorlogs` | No - Wave 0 | pending |
| W0-px | TBD | 0 | GEO-07 | T-04-04 | No logical fields in `vpacket.h` / `CONFIGARGS` | source audit | `git diff` of `libs/vpacket.h` and `CONFIGARGS` | Yes | pending |
| W0-scale | TBD | 0 | TEST-01 | T-04-02 | Existing scale arithmetic stays green | native unit | `python3 tests/run_in_linux.py -- meson test -C builddir monitor-scale --print-errorlogs` | Yes - `tests/test_monitor_scale.c` | pending |

*Status: pending, green, red, or flaky.*

---

## Wave 0 Requirements

- [ ] `tests/test_wm_ui_metrics.c` — UI-01..UI-06 integer/cache-key fixtures
- [ ] `tests/test_supported_features.c` — TEST-05 `HAVE_*` assertions
- [ ] `wm-ui-metrics` and `supported-features` registrations in `tests/meson.build`
- [ ] CI `meson setup` lines gain `-Dxcursor=enabled -Dxrender=enabled -Dpng=enabled -Dsvg=enabled -Dcairo=enabled`

Existing coverage that must stay green: `monitor-scale`, `monitor-scale-config`, `monitor-topology`, `monitor-ownership`, `icccm-size-hints`, `image-safety`.

No new test framework or fixture service is required.

---

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| Protocol fields remain actual X11 pixels | GEO-07 | Source-path audit of packet writers, not a runtime oracle | Confirm no logical conversion in `libs/vpacket.h` and `CONFIGARGS` |

All other phase behaviors have automated native verification. Existing live-X is non-regression only.

---

## Validation Sign-Off

- [ ] All tasks have automated verification or Wave 0 dependencies.
- [ ] Sampling continuity: no three consecutive tasks without automated verification.
- [ ] Wave 0 covers all missing references.
- [ ] No watch-mode flags.
- [ ] Focused native feedback latency is under 10 seconds.
- [ ] `nyquist_compliant: true` is set in frontmatter after implementation validation.

**Approval:** pending
