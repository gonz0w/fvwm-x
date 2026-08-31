---
phase: 05
slug: logical-size-transition-coordinator
# status lifecycle: draft (seeded by plan-phase) -> validated (set by validate-phase)
status: draft
nyquist_compliant: false
wave_0_complete: false
created: 2026-08-31
---

# Phase 05 - Validation Strategy

> Per-phase validation contract for feedback sampling during execution.

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | Meson test runner; plain C assertion executables (`-UNDEBUG`) |
| **Config file** | `tests/meson.build` |
| **Quick run command** | `python3 tests/run_in_linux.py -- meson test -C builddir geometry-transition icccm-size-hints monitor-scale monitor-ownership --print-errorlogs` |
| **Full suite command** | `python3 tests/run_in_linux.py -- meson test -C builddir --print-errorlogs` |
| **Estimated runtime** | <10 seconds for focused native geometry-transition tests; full-suite timing is environment-dependent |

---

## Sampling Rate

- **After every task commit:** Run `python3 tests/run_in_linux.py -- meson test -C builddir geometry-transition --print-errorlogs`
- **After every plan wave:** Run `python3 tests/run_in_linux.py -- meson test -C builddir --print-errorlogs`
- **Before `/gsd-verify-work`:** Full native suite must be green; existing live-X suite is non-regression only.
- **Max feedback latency:** 10 seconds for focused native tests.

---

## Per-Task Verification Map

| Task ID | Plan | Wave | Requirement | Threat Ref | Secure Behavior | Test Type | Automated Command | File Exists | Status |
|---------|------|------|-------------|------------|-----------------|-----------|-------------------|-------------|--------|
| W0-trips | TBD | 0 | GEO-02 | T-05-01 | Stored logical 7 stays 7 after 1,000 trips through 5/6/7/8 quarters; dest pixels match one `apply` | native unit | `python3 tests/run_in_linux.py -- meson test -C builddir geometry-transition --print-errorlogs` | No - Wave 0 | pending |
| W0-inc | TBD | 0 | GEO-02 | T-05-01 | Increment snap does not rewrite logical source | native unit | same | No - Wave 0 | pending |
| W0-ovf | TBD | 0 | GEO-02 | T-05-02 | Overflow/invalid dest outputs stay 0; last-good policy documented | native unit | same | No - Wave 0 | pending |
| W0-freeze | TBD | 0 | GEO-04 | T-05-03 | Pointer 1x1 dest freeze: frame resize that would flip overlap does not change dest | native unit | same | No - Wave 0 | pending |
| W0-grab | TBD | 0 | GEO-04 | T-05-03 | Grab offset converts with apply-from-source; origin = pointer + dest_offset | native unit | same | No - Wave 0 | pending |
| W0-max | TBD | 0 | GEO-06 | T-05-01 | Maximize dest = dest work rect, not scaled `g.max` | native unit | same | No - Wave 0 | pending |
| W0-fs | TBD | 0 | GEO-06 | T-05-01 | Fullscreen dest = dest monitor rect; restore uses stored logical | native unit | same | No - Wave 0 | pending |
| W0-fixed | TBD | 0 | GEO-06 | T-05-02 | Fixed-size client pixels unchanged; chrome totals follow dest | native unit | same | No - Wave 0 | pending |
| W0-px | TBD | 0 | GEO-07 | T-05-04 | No logical fields in `vpacket.h` / Status | source audit | `git diff` of `libs/vpacket.h`, `fvwm/module_interface.c`, `fvwm/status_json.c` | Yes | pending |

*Status: pending, green, red, or flaky.*

---

## Wave 0 Requirements

- [ ] `tests/test_geometry_transition.c` — covers GEO-02, GEO-04, GEO-06
- [ ] `geometry-transition` registration in `tests/meson.build`
- [ ] Link `libs/geometry_transition.c` via `libs/meson.build` `libfvwm3_sources`

Existing coverage that must stay green: `monitor-scale`, `icccm-size-hints`, `monitor-ownership`, `wm-ui-metrics`.

None of the existing tests cover logical-size crossings. No new test framework or fixture service is required.

---

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| Protocol fields remain actual X11 pixels | GEO-07 | Source-path audit of packet writers, not a runtime oracle | Confirm no logical conversion in `libs/vpacket.h`, `fvwm/module_interface.c`, `fvwm/status_json.c` |

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
