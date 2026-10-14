---
phase: 02
slug: scale-policy-and-configuration
# status lifecycle: draft (seeded by plan-phase) -> validated (set by validate-phase)
status: draft
nyquist_compliant: false
wave_0_complete: false
created: 2026-08-30
---

# Phase 02 - Validation Strategy

> Per-phase validation contract for feedback sampling during execution.

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | Meson test runner, plain C assertion executables, and the Python standard-library live-X harness |
| **Config file** | `tests/meson.build`; `live-x-tests` remains opt-in |
| **Quick run command** | `python3 tests/run_in_linux.py -- meson test -C builddir monitor-scale monitor-scale-config --print-errorlogs` |
| **Full suite command** | `python3 tests/run_in_linux.py -- meson test -C builddir --print-errorlogs` |
| **Focused live-X command** | `python3 tests/run_in_linux.py --live-x -- meson test -C builddir scale-policy-xorg-dummy --print-errorlogs` |
| **Full live-X command** | `python3 tests/run_in_linux.py --live-x -- meson test -C builddir --suite live-x --print-errorlogs` |
| **Phase format command (local macOS)** | `xcrun clang-format --dry-run --Werror libs/FScreen.c libs/FScreen.h libs/monitor_scale.c libs/monitor_scale.h fvwm/builtins.c fvwm/commands.h fvwm/events.c fvwm/functable.c fvwm/fvwm3.c fvwm/monitor_scale_config.c fvwm/monitor_scale_config.h fvwm/topology_event_adapter.c fvwm/topology_event_adapter.h tests/test_fscreen_topology.c tests/test_monitor_scale.c tests/test_monitor_scale_config.c tests/test_topology_event_adapter.c` |
| **Estimated runtime** | <10 seconds for focused native policy tests; live-X and full-suite timing is environment-dependent |

---

## Sampling Rate

- **After every task commit:** Run the narrow named test target affected by the task through `tests/run_in_linux.py`.
- **After every plan wave:** Run `python3 tests/run_in_linux.py -- meson test -C builddir --print-errorlogs`.
- **After every live-X task:** Run only the focused Xorg-dummy scale test; run the complete `live-x` suite at plan/phase completion.
- **Before `/gsd-verify-work`:** Full native and live-X suites and the documentation build must be green; run `xcrun clang-format --dry-run --Werror libs/FScreen.c libs/FScreen.h libs/monitor_scale.c libs/monitor_scale.h fvwm/builtins.c fvwm/commands.h fvwm/events.c fvwm/functable.c fvwm/fvwm3.c fvwm/monitor_scale_config.c fvwm/monitor_scale_config.h fvwm/topology_event_adapter.c fvwm/topology_event_adapter.h tests/test_fscreen_topology.c tests/test_monitor_scale.c tests/test_monitor_scale_config.c tests/test_topology_event_adapter.c` for the phase format gate on local macOS. ASan remains native Linux CI responsibility.
- **Max feedback latency:** 10 seconds for focused pure-policy tests.

---

## Per-Task Verification Map

| Task ID | Plan | Wave | Requirement | Threat Ref | Secure Behavior | Test Type | Automated Command | File Exists | Status |
|---------|------|------|-------------|------------|-----------------|-----------|-------------------|-------------|--------|
| 02-02-01 | 02-02 | 2 | SCALE-01 | T-02-05, T-02-06, T-02-07 | Strict factor and exact-name input validation leaves prior valid configuration unchanged | native unit | `python3 tests/run_in_linux.py -- meson test -C builddir monitor-scale-config --print-errorlogs` | No - created test-first by task | pending |
| 02-01-01 | 02-01 | 1 | SCALE-02 | T-02-02 | Exact wide arithmetic rejects invalid or overflowing physical facts | native unit | `python3 tests/run_in_linux.py -- meson test -C builddir monitor-scale --print-errorlogs` | No - created test-first by tracer | pending |
| 02-03-01/02-03-03 | 02-03 | 3 | SCALE-03 | T-02-09, T-02-10 | Invalid higher-priority sources never suppress a valid lower-priority result; changed topology recomputes once before EWMH/broadcast/pan/status while unchanged/failed invokes no downstream callback | native unit + injected coordinator | `python3 tests/run_in_linux.py -- meson test -C builddir monitor-scale-config topology-event-adapter --print-errorlogs` | `tests/test_topology_event_adapter.c` exists and is extended by task | pending |
| 02-04-01/02-05-02 | 02-04, 02-05 | 4, 5 | SCALE-04 | T-02-13, T-02-17 | Diagnostics escape monitor names and explain the selected fallback without format-string use | native + live-X | `python3 tests/run_in_linux.py --live-x -- meson test -C builddir scale-policy-xorg-dummy --print-errorlogs` | No - created test-first by tasks | pending |
| 02-01-02 | 02-01 | 1 | SCALE-05 | T-02-02 | Checked conversion reports overflow and never wraps, clamps, or reuses rounded output | exhaustive native unit | `python3 tests/run_in_linux.py -- meson test -C builddir monitor-scale --print-errorlogs` | No - created test-first by task | pending |

*Status: pending, green, red, or flaky.*

---

## Wave 0 Requirements

- [ ] Plan 02-01 Tasks 1-2 create `tests/test_monitor_scale.c` test-first for parser, exact DPI, precedence, rounding, drift, and overflow coverage.
- [ ] Plan 02-02 Task 1 creates `tests/test_monitor_scale_config.c` test-first for sequential storage, exact matching, invalid replacement retention, and explanation coverage.
- [ ] Plan 02-03 Task 3 extends `tests/test_topology_event_adapter.c` with `fvwm_topology_dispatch_refresh_result()` callback-order coverage for scale recompute before all existing downstream effects.
- [ ] Plans 02-01, 02-02, and 02-05 register `monitor-scale`, `monitor-scale-config`, and `scale-policy-xorg-dummy` in `tests/meson.build` as their tests become executable.
- [ ] Plan 02-05 creates self-tested scenario-owned startup commands and the Xorg-dummy scale-policy scenario in `tests/live_x/live_x_harness.py` without introducing a second harness.

No new test framework or fixture service is required.

---

## Manual-Only Verifications

All phase behaviors have automated verification. Human review is limited to confirming documentation wording and does not replace test evidence.

---

## Validation Sign-Off

- [ ] All tasks have automated verification or Wave 0 dependencies.
- [ ] Sampling continuity: no three consecutive tasks without automated verification.
- [ ] Wave 0 covers all missing references.
- [ ] No watch-mode flags.
- [ ] Focused pure-policy feedback latency is under 10 seconds.
- [ ] `nyquist_compliant: true` is set in frontmatter after implementation validation.

**Approval:** pending
