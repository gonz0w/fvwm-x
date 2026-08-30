---
phase: 1
slug: topology-snapshot-and-live-x-harness
status: draft
nyquist_compliant: false
wave_0_complete: false
created: 2026-08-29
---

# Phase 1 - Validation Strategy

> Per-phase validation contract for feedback sampling during execution.

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | Meson test runner; plain C assertion executables; Python 3 standard-library live-X harness |
| **Config file** | `tests/meson.build`; Wave 0 adds a `live-x-tests` option to `meson.options` |
| **Quick run command** | `meson test -C builddir monitor-topology topology-refresh-policy --print-errorlogs` |
| **Full suite command** | `meson test -C builddir --print-errorlogs` |
| **Live-X command** | `meson test -C builddir --suite live-x --print-errorlogs` |
| **Estimated runtime** | Quick unit suite under 10 seconds; live-X suite under 90 seconds |

---

## Sampling Rate

- **After every task commit:** Run the narrowest applicable named test, with
  `meson test -C builddir monitor-topology topology-refresh-policy --print-errorlogs`
  as the default once both unit executables exist.
- **After every plan wave:** Run `meson test -C builddir --print-errorlogs`.
- **After live-X harness or integration changes:** Run
  `meson test -C builddir --suite live-x --print-errorlogs` in the native-Linux
  environment.
- **Before `/gsd-verify-work`:** The native full suite, sanitizer CI suite,
  Xvfb smoke scenario, and authoritative Xorg-dummy suite must be green.
- **Max feedback latency:** 10 seconds for pure policy tasks and 90 seconds for
  live-X integration tasks.

---

## Per-Task Verification Map

| Task ID | Plan | Wave | Requirement | Threat Ref | Secure Behavior | Test Type | Automated Command | File Exists | Status |
|---------|------|------|-------------|------------|-----------------|-----------|-------------------|-------------|--------|
| 01-01-01 | 01 | 0 | TOP-01, TOP-03 | T-01, T-02, T-03 | Reject malformed candidates and retain the last committed snapshot | unit | `meson test -C builddir monitor-topology --print-errorlogs` | No - Wave 0 | pending |
| 01-01-02 | 01 | 0 | TOP-03 | T-02 | Bound event-storm work without sleeping or starving commits | unit | `meson test -C builddir topology-refresh-policy --print-errorlogs` | No - Wave 0 | pending |
| 01-02-01 | 02 | 1 | TOP-01, TOP-03 | T-01, T-03 | Publish only complete validated topology data and serialize valid JSON | unit + integration | `meson test -C builddir monitor-topology topology-refresh-policy --print-errorlogs` | No - Wave 0 | pending |
| 01-03-01 | 03 | 1 | TOP-06 | T-04, T-05, T-06 | Isolate nested servers, avoid shell injection, and clean up bounded child processes | integration | `meson test -C builddir --suite live-x --print-errorlogs` | No - Wave 0 | pending |
| 01-03-02 | 03 | 2 | TOP-01, TOP-03, TOP-06 | T-01, T-02, T-07 | Verify authoritative snapshots, epochs, coalescing, and test-only failed-query recovery | integration | `meson test -C builddir topology-xorg-dummy --print-errorlogs` | No - Wave 0 | pending |

Threat references:

- **T-01:** Malformed or hostile X reply counts, dimensions, identities, or
  output arrays cause unsafe arithmetic or partial publication.
- **T-02:** A continuous topology event storm causes denial of service or
  indefinitely postpones a committed refresh.
- **T-03:** Invalid Atom-name bytes produce invalid JSON or unsafe diagnostics.
- **T-04:** Harness command construction permits shell or argument injection.
- **T-05:** A nested X server is exposed over TCP.
- **T-06:** Timed-out child processes survive test teardown.
- **T-07:** Failed-query fault injection becomes reachable in production.

---

## Wave 0 Requirements

- [ ] `tests/test_monitor_topology.c` covers candidate ownership, structural
  validation, canonical equality, epoch publication, allocation failures, and
  deterministic serialization for TOP-01 and TOP-03.
- [ ] `tests/test_topology_refresh_policy.c` covers quiet and maximum deadlines,
  timeout rounding, reset, and re-arming for TOP-03.
- [ ] `tests/live_x/live_x_harness.py` provides dependency-free process
  lifecycle, readiness predicates, scenario execution, state comparison,
  diagnostics, and bounded cleanup for TOP-06.
- [ ] `tests/live_x/xorg-dummy.conf` defines deterministic outputs and modes.
- [ ] `tests/live_x/xrr_query_fault.c` supplies Linux-only, test-process-only
  failure injection without a production runtime switch.
- [ ] `tests/meson.build` and `meson.options` register unit tests and an opt-in
  nonparallel `live-x` suite.
- [ ] `.github/workflows/ccpp.yml` adds one native-Linux live-X job with required
  X server packages and failure artifact upload.

---

## Required Scenario Coverage

### Native Unit Scenarios

- Candidate deep copies remain valid after source destruction; cleanup is
  idempotent and every recoverable allocation failure leaves no partial state.
- Failed query, empty active topology, invalid or duplicate identity, invalid
  UTF-8, unsafe dimensions/counts, and incomplete outputs are rejected.
- Zero or implausible physical dimensions remain unchanged observable facts.
- Monitor and output permutations compare equal; changing each semantic field
  compares unequal; a rename is remove/add.
- First publication is epoch 1; equal, reordered, and failed candidates do not
  advance; each semantic change advances exactly once.
- Refresh policy exercises quiet extension, exact boundaries, hard cap,
  positive timeout ceiling, due reset, equal refresh, and re-arming.
- Status v4 serialization has exact keys/types, canonical order, and strict JSON
  parser acceptance.

### Live-X Scenarios

- Xvfb must prove startup, RandR 1.5+, FVWM epoch 1, inspection, and clean
  teardown. It must attempt create/mutate/remove operations, capability-gate
  mutation assertions from queried state, and never treat command exit status
  alone as evidence of a topology change.
- Xorg-dummy must authoritatively prove output/mode/position/mm changes, primary
  changes, Monitor create/remove, automatic Monitor mutation, multi-output
  membership, membership reordering without an epoch, and one epoch for a rapid
  burst.
- The failed-query scenario must start from a good snapshot, fail exactly one
  `XRRGetMonitors` call in the spawned test FVWM process, retain JSON and epoch,
  emit a diagnostic, then recover on the next topology event.
- Failures must retain server logs, FVWM logs, `xrandr` state, and the last
  committed snapshot before bounded child teardown.

---

## Manual-Only Verifications

All phase behaviors have automated verification. Physical multi-monitor
hardware remains outside Phase 1 because this phase records topology facts and
does not define scale policy.

---

## Validation Sign-Off

- [ ] All tasks have automated verification or explicit Wave 0 dependencies.
- [ ] Sampling continuity has no three consecutive tasks without an automated
  check.
- [ ] Wave 0 covers every missing test or harness reference.
- [ ] No watch-mode flags or fixed sleeps are used.
- [ ] Pure-policy feedback latency is under 10 seconds.
- [ ] Live-X feedback latency is under 90 seconds.
- [ ] `nyquist_compliant: true` and `wave_0_complete: true` are set after the
  planned test infrastructure exists and passes.

**Approval:** pending
