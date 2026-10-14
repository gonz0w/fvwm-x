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
| **Platform router** | `tests/run_in_linux.py`: native argv execution on Linux; Podman `--platform linux/amd64` on Darwin; `--live-x` installs the four audited X packages in the container |
| **Config file** | `tests/meson.build`; default-false `live-x-tests` option in `meson.options` |
| **Quick run command** | `python3 tests/run_in_linux.py -- meson test -C builddir monitor-topology fscreen-topology-adapter topology-refresh-policy topology-event-adapter --print-errorlogs` |
| **Full suite command** | `python3 tests/run_in_linux.py -- meson test -C builddir --print-errorlogs` |
| **Live-X command** | `python3 tests/run_in_linux.py --live-x -- meson test -C builddir --suite live-x --print-errorlogs` |
| **Native Linux/CI equivalents** | Direct `meson test` commands are retained in the dedicated Linux workflow after package installation |
| **Workflow syntax gate** | Ruby Psych `YAML.parse_file` runs before harness semantic assertions; it is verification-only and not a project runtime dependency |
| **Estimated runtime** | Quick unit suite under 10 seconds; live-X suite under 90 seconds |

---

## Sampling Rate

- **After every task commit:** Run the narrowest named target through
  `tests/run_in_linux.py`; code-producing tasks marked `tdd="true"` create their
  focused assertion first, observe RED, then implement to GREEN.
- **After every plan wave:** Run the wrapped full native suite.
- **After live-X harness or integration changes:** Run
  the wrapped named scenario and then the wrapped `live-x` suite. The wrapper
  supplies Podman x86_64 plus X packages on macOS and executes directly on Linux.
- **Before `/gsd-verify-work`:** The native full suite, sanitizer CI suite,
  Xvfb smoke scenario, and authoritative Xorg-dummy suite must be green.
- **Max feedback latency:** 10 seconds for pure policy tasks and 90 seconds for
  live-X integration tasks.

---

## Per-Task Verification Map

| Task ID | Plan | Wave | Requirement | Threat Ref | Behavior Under Test | Test Type | Automated Command | Test Creation |
|---------|------|------|-------------|------------|---------------------|-----------|-------------------|---------------|
| 01-01-01 | 01 | 1 | TOP-01, TOP-03 | T-01-01..03 | Wrapper routing plus detached epoch-1 tracer | unit/tracer | wrapped `monitor-topology` | TDD: wrapper self-test and model RED first |
| 01-01-02 | 01 | 1 | TOP-01, TOP-03 | T-01-01..03 | Malformed facts, canonical equality, exhaustive model allocation failure | unit | wrapped `monitor-topology` | TDD expansion in `test_monitor_topology.c` |
| 01-01-03 | 01 | 1 | TOP-01, TOP-03 | T-01-03 | Production/test linkage exactly once | build + unit | wrapped configure/compile/`monitor-topology` | Existing tests become registered |
| 01-02-01 | 02 | 2 | TOP-01, TOP-03 | T-01-04..06 | One-query X adapter, ownership, changed/equal/failed retention | focused unit | wrapped `fscreen-topology-adapter` | TDD in new adapter test |
| 01-02-02 | 02 | 2 | TOP-01, TOP-03 | T-01-05 | Every legacy node/name/screen staging allocation fails before publication without state change | focused unit | wrapped `fscreen-topology-adapter` + full suite | TDD fail-index loop before seam implementation |
| 01-03-01 | 03 | 3 | TOP-03 | T-01-07..09 | Quiet/hard-cap boundaries, ceiling, reset/rearm | unit | wrapped `topology-refresh-policy` | TDD in new policy test |
| 01-03-02 | 03 | 3 | TOP-03 | T-01-07..09 | Production/test policy linkage | build + unit | wrapped quick native sample | Registration after RED/GREEN unit |
| 01-04-01 | 04 | 4 | TOP-03 | T-01-10..12 | Exact accepted/rejected event classes, timeout choice, refresh-result dispatch | focused unit | wrapped `topology-event-adapter` | TDD in new event adapter test |
| 01-04-02 | 04 | 4 | TOP-03 | T-01-10 | Actual `fvwm_topology_event_dispatch()` wiring orders screen update before dirty mark; rejected events and all refresh/downstream counters remain zero | focused unit + production link | wrapped exact adapter source-count check + explicit `fvwm3` target link + quick native sample + clang-format dry-run | TDD callback counters/order trace before wiring |
| 01-04-03 | 04 | 4 | TOP-03 | T-01-11..12 | Top-of-loop due ordering and changed-only once side effects | focused unit + full | wrapped event adapter + full suite | TDD ordering trace/counters before integration |
| 01-05-01 | 05 | 3 | TOP-01 | T-01-13..15 | Exact Status v4 schema, strict UTF-8 JSON, common snapshot source, C/header formatting | unit + build | wrapped `monitor-topology` + clang-format dry-run | TDD serializer assertions before implementation |
| 01-05-02 | 05 | 3 | TOP-01 | T-01-15 | Manual schema/epoch/PrintInfo contract matches fixture | docs + build | wrapped mandoc compile + `monitor-topology` | Existing serializer fixture anchors docs |
| 01-06-01 | 06 | 5 | TOP-03, TOP-06 | T-01-16..19 | argv-only process lifecycle, deadlines, evidence, bounded teardown, and shim syntax in preload/wrap modes | harness self-test + C syntax | wrapped Python compile/`--self-test` plus two `${CC:-cc} -fsyntax-only` invocations | TDD standard-library self-tests and compile-time mode rejection |
| 01-06-02 | 06 | 5 | TOP-03, TOP-06 | T-01-19, T-01-SC | Per-invocation preload probe and wrap fallback; no production seam | live-X | wrapped `topology-fault-injection-probe` | TDD scenario assertions before target wiring |
| 01-07-01 | 07 | 6 | TOP-01, TOP-06 | T-01-20..22 | Actual Xvfb startup, Status/PrintInfo epoch 1, evidence, cleanup | live-X | wrapped `topology-xvfb` | TDD baseline predicates before scenario implementation |
| 01-07-02 | 07 | 6 | TOP-01, TOP-03, TOP-06 | T-01-20..22 | Observed-state capability branch and no false epoch | live-X | wrapped `topology-xvfb` + live suite | TDD transition/unsupported assertions |
| 01-08-01 | 08 | 7 | TOP-01, TOP-03, TOP-06 | T-01-23,25,26 | Exact Xorg-dummy mutations, reorder/duplicates, burst and sustained cap | live-X | wrapped `topology-xorg-dummy` | TDD predicates/scenario metadata before implementation |
| 01-08-02 | 08 | 7 | TOP-03, TOP-06 | T-01-24,28 | Standalone probe/select, last-good retention, one later recovery | live-X | wrapped `topology-xorg-dummy-failed-query` | TDD independent invocation; no prior-test state |
| 01-08-03 | 08 | 7 | TOP-01, TOP-03, TOP-06 | T-01-27,28 | Parsed YAML workflow contract, one native lane, retained evidence | YAML syntax + semantic source + full/live | wrapped Ruby Psych parse, `--check-workflow`, full and live suites | Syntax parser and semantic checker added before CI reliance |

---

## Test-Creation and Execution Ordering

- Wave 1's leading tracer creates `tests/run_in_linux.py` before any Meson/X
  verification relies on it, then creates `test_monitor_topology.c` RED first.
- Every production behavior task marked `tdd="true"` names the focused test
  file and behavior written before implementation; there is no obsolete Wave 0.
- Plan 02 creates call-scoped FScreen query/allocation seams and exhaustive
  fail-index tests before claiming runtime/staging retention.
- Plan 04 creates a focused event adapter test before `events.c` wiring, so real
  event classes, timeout selection, and changed-only side effects have native
  evidence independent of live-X timing.
- Each live-X Meson test owns setup/probe/teardown within its invocation.
  `topology-xorg-dummy-failed-query` probes preload and chooses link-wrap itself;
  it has no test-order or durable-state dependency.

---

## Required Scenario Coverage

### Native Unit Scenarios

- Candidate deep copies remain valid after source destruction; cleanup is
  idempotent and every recoverable allocation failure leaves no partial state.
- Failed query, empty active topology, invalid or duplicate identity, invalid
  UTF-8, unsafe dimensions/counts, and incomplete outputs are rejected.
- FScreen's call-scoped RandR adapter distinguishes query failure/empty/equal/
  changed outcomes and releases all X-owned data exactly once.
- Every recoverable legacy node, name, and screen-record allocation failure
  preserves the committed pointer, epoch, and complete `monitor_q` digest.
- Zero or implausible physical dimensions remain unchanged observable facts.
- Monitor and output permutations compare equal; changing each semantic field
  compares unequal; a rename is remove/add.
- First publication is epoch 1; equal, reordered, and failed candidates do not
  advance; each semantic change advances exactly once.
- Refresh policy exercises quiet extension, exact boundaries, hard cap,
  positive timeout ceiling, due reset, equal refresh, and re-arming.
- Event adapter exercises root/RandR accepted and rejected classes, minimum
  timeout selection, exact update-before-mark dispatch, no immediate query, no
  refresh/downstream callback path, and changed-only once side effects.
- Status v4 serialization has exact keys/types, canonical order, and strict JSON
  parser acceptance.

### Live-X Scenarios

- Xvfb must prove startup, RandR 1.5+, FVWM epoch 1, inspection, and clean
  teardown. It must attempt create/mutate/remove operations, capability-gate
  mutation assertions from queried state, and never treat command exit status
  alone as evidence of a topology change.
- Xorg-dummy must authoritatively prove the exact `846x286` to `900x300`
  physical-mm mutation, output/mode/position changes, primary
  changes, Monitor create/remove, automatic Monitor mutation, multi-output
  membership, membership reordering without an epoch, and one epoch for a rapid
  burst. User Monitor reordering uses explicit delete/recreate with the same
  name and reversed membership because same-name mutation returns `BadValue`.
- Sustained relevant delete/recreate pressure must maintain sub-50-ms command
  gaps for at least 400 ms and must not postpone the distinct semantic commit
  beyond the 250 ms policy cap plus 100 ms scheduler/Status tolerance.
- The failed-query scenario must start from a good snapshot, fail exactly one
  `XRRGetMonitors` call in the spawned test FVWM process, retain JSON and epoch,
  emit a diagnostic, then recover on the next topology event. The invocation
  itself probes preload and falls back to link-wrap; it reads no prior result.
- Failures must retain server logs, FVWM logs, `xrandr` state, and the last
  committed snapshot before bounded child teardown.

---

## Manual-Only Verifications

All phase behaviors have automated verification. Physical multi-monitor
hardware remains outside Phase 1 because this phase records topology facts and
does not define scale policy.

---

## Validation Sign-Off

- [ ] All 19 tasks have independently runnable automated verification after implementation.
- [ ] Sampling continuity has no three consecutive tasks without an automated
  check.
- [ ] Every missing test/harness is created TDD-style in the task that first
  relies on it; the platform wrapper is first in the tracer.
- [ ] No watch-mode flags or fixed sleeps are used.
- [ ] Pure-policy target feedback latency is under 10 seconds.
- [ ] Live-X target feedback latency is under 90 seconds.
- [ ] `nyquist_compliant` remains false and `wave_0_complete` remains false
  until implementation creates every planned test and all commands pass.

**Approval:** draft; implementation evidence pending
