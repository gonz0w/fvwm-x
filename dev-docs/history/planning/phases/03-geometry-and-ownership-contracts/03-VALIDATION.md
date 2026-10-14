---
phase: 03
slug: geometry-and-ownership-contracts
# status lifecycle: draft (seeded by plan-phase) -> validated (set by validate-phase)
status: draft
nyquist_compliant: false
wave_0_complete: false
created: 2026-08-30
---

# Phase 03 - Validation Strategy

> Per-phase validation contract for feedback sampling during execution.

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | Meson test runner; plain C assertion executables |
| **Config file** | `tests/meson.build` |
| **Quick run command** | `python3 tests/run_in_linux.py -- meson test -C builddir monitor-ownership icccm-size-hints monitor-scale monitor-topology --print-errorlogs` |
| **Full suite command** | `python3 tests/run_in_linux.py -- meson test -C builddir --print-errorlogs` |
| **Full live-X command** | `python3 tests/run_in_linux.py --live-x -- meson test -C builddir --suite live-x --print-errorlogs` |
| **Estimated runtime** | <10 seconds for focused native ownership and ICCCM tests; live-X and full-suite timing is environment-dependent |

---

## Sampling Rate

- **After every task commit:** Run the named C target through `tests/run_in_linux.py`.
- **After every plan wave:** Run `python3 tests/run_in_linux.py -- meson test -C builddir --print-errorlogs`.
- **Before `/gsd-verify-work`:** Full native suite must be green; existing live-X suite is non-regression only (no new live-X cases required for TEST-01).
- **Max feedback latency:** 10 seconds for focused native tests.

---

## Per-Task Verification Map

| Task ID | Plan | Wave | Requirement | Threat Ref | Secure Behavior | Test Type | Automated Command | File Exists | Status |
|---------|------|------|-------------|------------|-----------------|-----------|-------------------|-------------|--------|
| W0-own | TBD | 0 | TOP-02 | T-03-02 | Negative origin, gap, clone, partial overlap, rotation, and output reorder all yield the same owner | native unit | `python3 tests/run_in_linux.py -- meson test -C builddir monitor-ownership --print-errorlogs` | No - Wave 0 | pending |
| W0-hyst | TBD | 0 | GEO-01 | T-03-02 | Incumbent kept on seam; challenger wins only beyond hysteresis; identity ties | native unit | `python3 tests/run_in_linux.py -- meson test -C builddir monitor-ownership --print-errorlogs` | No - Wave 0 | pending |
| W0-icccm | TBD | 0 | GEO-03 | T-03-01 | min/max/base/inc/aspect/fixed/gravity on a scaled target; overflow initialized | native unit | `python3 tests/run_in_linux.py -- meson test -C builddir icccm-size-hints --print-errorlogs` | No - Wave 0 | pending |
| W0-work | TBD | 0 | GEO-05 | T-03-02 | Work-area/strut intersection and monitor-rect placement math in root pixels including negative origins | native unit | `python3 tests/run_in_linux.py -- meson test -C builddir monitor-ownership --print-errorlogs` | No - Wave 0 | pending |
| W0-px | TBD | 0 | GEO-07 | T-03-04 | No logical conversion in packet/EWMH sources | source audit | `git diff` of `status_json.c`, `Module.h`, `vpacket.h`, `CONFIGARGS`, `EWMH_SetFrameStrut` | Yes | pending |
| W0-scale | TBD | 0 | TEST-01 | T-03-02 | Scale arithmetic, signed rounding, invalid DPI, 1,000 round trips | native unit | `python3 tests/run_in_linux.py -- meson test -C builddir monitor-scale --print-errorlogs` | Yes - `tests/test_monitor_scale.c` | pending |
| W0-snap | TBD | 0 | TEST-01 | — | Snapshot diffs remain green | native unit | `python3 tests/run_in_linux.py -- meson test -C builddir monitor-topology --print-errorlogs` | Yes - `tests/test_monitor_topology.c` | pending |

*Status: pending, green, red, or flaky.*

---

## Wave 0 Requirements

- [ ] `tests/test_monitor_ownership.c` — TOP-02 / GEO-01 / GEO-05 rectangle fixtures
- [ ] `tests/test_icccm_size_hints.c` — GEO-03 matrix including scaled targets
- [ ] `monitor-ownership` and `icccm-size-hints` registrations in `tests/meson.build`
- [ ] Optional: extract work-area intersection tests if not folded into ownership helpers

Existing coverage that must stay green: `monitor-scale`, `monitor-scale-config`, `monitor-topology`, `fscreen-topology-adapter`.

No new test framework or fixture service is required.

---

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| Protocol fields remain actual X11 pixels | GEO-07 | Source-path audit of packet and EWMH writers, not a runtime oracle | Confirm no logical conversion in `status_json.c`, `libs/Module.h`, `libs/vpacket.h`, `CONFIGARGS`, and `EWMH_SetFrameStrut` |

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
