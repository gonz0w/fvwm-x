---
phase: 01-topology-snapshot-and-live-x-harness
verified: 2026-08-30T16:40:42Z
status: passed
score: 8/8 must-haves verified
behavior_unverified: 0
overrides_applied: 0
requirements:
  TOP-01: satisfied
  TOP-03: satisfied
  TOP-06: satisfied
decision_coverage:
  honored: 16
  total: 16
  not_honored: []
human_verification: []
residual_risks:
  - "The supplied security-audit result reports three accepted low-severity documentation gaps; no high or critical open threat was reported. No security-audit artifact is present in this checkout, so that result is corroborative rather than primary verification evidence."
  - "The dedicated native-Linux GitHub Actions lane is source- and checker-verified, but this checkout has no git remote, so its hosted run status could not be queried. Independent Linux/amd64 Podman execution passed all 14 configured tests with zero skips."
  - "Ancient TODO/FIXME/XXX comments remain in legacy portions of FScreen.c, events.c, and builtins.c. Git blame dates them to 2002-2025; none was introduced by Phase 01 or lies in its topology implementation paths."
---

# Phase 1: Topology Snapshot and Live-X Harness Verification Report

**Phase Goal:** Users and maintainers can rely on one complete, atomic, reproducible view of the live RandR Monitor topology.
**Verified:** 2026-08-30T16:40:42Z
**Status:** passed
**Re-verification:** No — initial goal-backward verification after all eight plans and remediation through `842e7802b`.

## Goal Achievement

### Observable Truths

| # | Truth | Status | Code and behavioral evidence |
|---|---|---|---|
| 1 | One committed snapshot owns stable identity, actual root-pixel geometry, raw physical dimensions, raw primary state, complete output membership, and an epoch. | ✓ VERIFIED | `libs/monitor_topology.h:21-42` defines every fact. `libs/monitor_topology.c:208-285` deep-copies and canonicalizes detached candidates; `:292-361` validates and compares every semantic field. `monitor-topology` passed in the independent 14/14 run. |
| 2 | Publication is atomic: first commit is epoch 1, semantic change advances once, equality/order noise does not advance, and failures retain the last good pointer, epoch, and legacy view. | ✓ VERIFIED | The one-swap publication boundary is `libs/monitor_topology.c:364-400`. FScreen stages all fallible compatibility allocations before publication at `libs/FScreen.c:730-754` and applies allocation-free reconciliation at `:1004-1078`. Pointer/epoch/view retention and fail-index tests are in `tests/test_fscreen_topology.c:358-447,481-568`; canonical and allocation tests are in `tests/test_monitor_topology.c:577-934`. Both targets passed. |
| 3 | Topology notifications perform constant-time dirty marking and one deferred, single-threaded refresh bounded by a 50 ms quiet deadline and 250 ms first-event cap. | ✓ VERIFIED | Event classification/marking is wired at `fvwm/events.c:3618-3643`; the due refresh runs before the next queued event at `:3705-3728`; timeout merging is at `:3803-3838`. The pure policy at `fvwm/topology_refresh_policy.c:24-112` preserves the first deadline, extends only the quiet deadline, uses saturation and ceil-to-ms, and resets after an attempt. Native boundary tests and the authoritative sustained-pressure scenario passed. |
| 4 | Status v4 and `PrintInfo Topology` expose the same committed snapshot with deterministic, strict inspection semantics. | ✓ VERIFIED | `fvwm/status_json.c:83-124` builds complete Status v4 atomically from the committed topology. `fvwm/builtins.c:122-155` obtains `FScreenGetTopologySnapshot()` for Status; `:2157-2229` does the same for PrintInfo. `libs/monitor_topology.c:429-557` emits canonical JSON and exact `uint64_t` epochs. Exact schema, allocation-failure, 1,025-monitor, and uint64-boundary tests passed; Xvfb and Xorg live tests parsed real Status lines and observed PrintInfo output. The user contract is documented at `doc/fvwm3_manpage_source.adoc:3453-3458,8092-8172`. |
| 5 | Xvfb provides a mandatory capability-aware baseline and never treats command exit status as proof of Monitor mutation. | ✓ VERIFIED | `tests/live_x/live_x_harness.py:2410-2557` requires RandR 1.5+, exact queried baseline, Status v4 epoch 1, PrintInfo, stability, and teardown. Mutation support is decided only by repeated `--listmonitors` state at `:2579-2714`; unsupported mutation still executes all operations and preserves the baseline. `topology-xvfb` passed and was not skipped. |
| 6 | Xorg-dummy is the authoritative, non-skippable source for create/change/remove, physical/output/primary transitions, canonical equality, rapid-burst coalescing, and bounded pressure progress. | ✓ VERIFIED | The deterministic fixture is `tests/live_x/xorg-dummy.conf`. `run_xorg_dummy_topology()` at `tests/live_x/live_x_harness.py:1445-2038` compares independently queried X state to Status after each transition, verifies reorder/duplicate zero-epoch behavior, one rapid-burst epoch, and a semantic commit by the production hard cap plus tolerance under >400 ms pressure. `tests/meson.build:138-156` registers it without skip semantics and with a hard timeout. The test passed. |
| 7 | A failed real `XRRGetMonitors` query retains the exact last-good topology, emits no failed-generation Status, and a later event recovers with one commit. | ✓ VERIFIED | Runtime failure retention is implemented at `libs/FScreen.c:638-781`. The isolated shim exists only in `tests/live_x/xrr_query_fault.c`; production source contains no fault control. `run_xorg_dummy_failed_query()` at `tests/live_x/live_x_harness.py:2086-2374` independently probes preload then wrap, requires one consumed sentinel and one diagnostic, compares retained topology, rejects downstream emission, and verifies epoch-2 recovery plus post-recovery equality. The standalone failed-query test passed. |
| 8 | The harness guarantees bounded teardown/evidence, and native Linux CI runs exactly the four authoritative live-X tests once. | ✓ VERIFIED | `ManagedProcess`/`HarnessRuntime` at `tests/live_x/live_x_harness.py:175-318` use argv-only `shell=False`, private TMPDIR, new sessions, reverse terminate/wait/kill teardown, and hard deadlines. `capture_failure()` at `:2761-2792` preserves server/FVWM logs, xrandr state, Status, and metadata before cleanup. `.github/workflows/ccpp.yml:74-115` defines one non-matrix Linux job, installs exactly four audited packages, runs the exact suite, and uploads failure evidence. `tests/run_in_linux.py:37-82` fails closed unless the qualified suite contains exactly four audited names. Router, harness, and workflow checks passed. |

**Score:** 8/8 truths verified (0 present-but-behavior-unverified).

## Requirements Coverage

| Requirement | Status | Evidence |
|---|---|---|
| TOP-01 | ✓ SATISFIED | Truths 1, 2, and 4 prove complete committed facts, canonical identity/membership, exact epochs, Status v4, PrintInfo, raw units, and strict JSON. Live Xvfb/Xorg comparisons use queried server state as an independent oracle. |
| TOP-03 | ✓ SATISFIED | Truths 2, 3, 6, and 7 prove detached atomic publication, one settled semantic commit, no epoch for equality/failure, bounded storm progress, exact last-good retention, and later recovery. |
| TOP-06 | ✓ SATISFIED | Truths 5, 6, and 8 prove reproducible Xvfb and authoritative Xorg-dummy creation/change/removal, capability-aware Xvfb handling, non-skippable authority, teardown/evidence, and dedicated native-Linux CI wiring. |

No additional Phase 01 requirement is mapped in `.planning/REQUIREMENTS.md`; there are no orphaned requirements.

## Required Artifacts and Wiring

| Artifact | Exists/Substantive | Wired | Verdict |
|---|---|---|---|
| `libs/monitor_topology.[ch]` | Owned immutable model, validation, equality, publication, JSON; no stubs | Linked by `libs/meson.build`; consumed by FScreen, Status, and tests | ✓ VERIFIED |
| `libs/FScreen.[ch]` | Real X adapter, committed getter, staged compatibility view | `FScreenInit()` requires epoch 1; event loop calls `FScreenRefreshTopology()` | ✓ VERIFIED |
| `fvwm/topology_refresh_policy.[ch]` | Pure two-deadline policy | Linked into fvwm3 and called by event marking, due check, and select timeout | ✓ VERIFIED |
| `fvwm/topology_event_adapter.[ch]` / `fvwm/events.c` | Real event classes and deferred coordinator | Production source registration plus live-X event paths | ✓ VERIFIED |
| `fvwm/status_json.[ch]` / `fvwm/builtins.c` | Allocation-safe Status v4 and human inspection | Both read `FScreenGetTopologySnapshot()`; live tests consume output | ✓ VERIFIED |
| `tests/live_x/live_x_harness.py` / fixtures/shim | Substantive 3,076-line lifecycle and scenario harness | Four exact Meson registrations; wrapper and CI execute suite | ✓ VERIFIED |
| `.github/workflows/ccpp.yml` | Dedicated native Linux lane and evidence upload | Workflow checker validates exact commands/packages/membership | ✓ VERIFIED |

### Key Link and Data-Flow Verification

`XRRGetMonitors` → detached source facts → canonical candidate → staged legacy allocations → one committed pointer swap → allocation-free reconciliation → changed-only downstream work → Status/PrintInfo. No link terminates in a static fixture or mock in production. Live-X expected topology is derived from independent XRandR/Xlib queries, not generated by FVWM itself.

## Behavioral Spot-Checks

| Check | Command | Result | Status |
|---|---|---|---|
| Router, harness, workflow contracts | `python3 tests/run_in_linux.py --self-test && python3 tests/live_x/live_x_harness.py --self-test && python3 tests/live_x/live_x_harness.py --check-workflow .github/workflows/ccpp.yml` | Exit 0 | ✓ PASS |
| Aggregate native plus live-X behavior | `python3 tests/run_in_linux.py --live-x -- meson test -C builddir --print-errorlogs` | 14/14 OK; 0 failed, 0 skipped, 0 timeout | ✓ PASS |

This independently confirms the supplied aggregate 14/14 result at `842e7802b` rather than relying on SUMMARY or REVIEW narration.

## Test Quality Audit

| Test area | Linked requirements | Disabled/skipped | Oracle/provenance | Strongest assertion | Verdict |
|---|---|---:|---|---|---|
| Snapshot/FScreen/status native tests | TOP-01, TOP-03 | 0 | Independent fixtures plus exact pointer/value/allocation accounting | Behavioral/value | ✓ STRONG |
| Refresh/event policy native tests | TOP-03 | 0 | Injected time and real protocol constants | Boundary/behavioral | ✓ STRONG |
| Xvfb/Xorg-dummy live tests | TOP-01, TOP-03, TOP-06 | 0 | External XRandR/Xlib queried state, compared to FVWM output | End-to-end behavioral | ✓ STRONG |
| Harness/workflow self-tests | TOP-06 | 0 | Synthetic malformed inputs and exact source contracts | Behavioral/value | ✓ STRONG |

Disabled requirement tests: 0. Circular expected-value generation: 0. The live scenarios use the X server as the external oracle; result/evidence files are outputs, not generated expectations.

## Prohibition Checks

| Prohibition | Evidence | Verdict |
|---|---|---|
| No logical/scaled topology units or physical-mm coercion | Topology source stores only raw integer pixel/mm facts; unit and live tests preserve zero/extreme/queried mm values. | ✓ VERIFIED |
| No second scale/ownership/recovery/module implementation in snapshot model | Model API is limited to ownership, validation, equality, epoch publication, and JSON. | ✓ VERIFIED |
| No new inspection service/socket/module protocol | Inspection remains on existing Status FIFO and PrintInfo paths. | ✓ VERIFIED |
| No production failed-query switch | Fault symbols/environment reads occur only in `tests/live_x` and test-gated, non-installed Meson targets; ordinary binary is checked for wrapper symbols. | ✓ VERIFIED |
| No false Xvfb support or skipped baseline | Support depends on queried Monitor state; baseline remains mandatory in both branches. | ✓ VERIFIED |
| No optional skip for authoritative Xorg scenarios | Four exact suite members are fail-closed by wrapper introspection; no skip/exit-77 path exists; aggregate run reports zero skips. | ✓ VERIFIED |

## Anti-Patterns and Review History

- No phase-owned placeholder, empty implementation, disabled requirement test, or unreferenced new debt marker was found.
- Historical markers in the large legacy files were inspected with `git blame`; they predate Phase 01 and are unrelated to the topology paths, so they are recorded as residual legacy debt rather than evidence that this phase is incomplete.
- Git history contains all implementation and remediation commits through `842e7802b`; the final remediation adds exact qualified live-X suite membership validation.
- The final code review is clean (0 critical, 0 warning). Review claims were not used as proof; each remediated path was checked in current source and covered by the independent 14/14 run.

## Security and Residual Risk

The supplied security-audit conclusion—no open high/critical threats and three accepted low documentation gaps—is consistent with direct inspection: nested X servers disable TCP, child environments remove inherited fault controls, commands are argv-only, test fault targets are non-installed, and teardown is bounded. The three low documentation gaps remain accepted residual risk; no security audit document is present in this checkout to enumerate them further.

The local verification route is Linux/amd64 under Podman because the host is macOS. The repository also contains and source-validates a dedicated native-Linux CI job, but hosted CI status cannot be queried because this checkout has no git remote.

## Decision Coverage

All 16 trackable `01-CONTEXT.md` decisions are honored by shipped artifacts (`check.decision-coverage-verify`: 16/16).

## Human Verification

N/A — infrastructure/foundation phase with no visual user-facing behavior. All acceptance criteria, including runtime state transitions and cleanup/ordering invariants, have automated native or nested-X behavioral coverage.

## Gaps Summary

No blocking or uncertain must-have remains. No item is deferred to a later phase to make the Phase 01 goal true.

---

_Verified: 2026-08-30T16:40:42Z_
_Verifier: the agent (gsd-verifier)_
