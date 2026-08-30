---
phase: 01-topology-snapshot-and-live-x-harness
reviewed: 2026-08-30T16:34:38Z
depth: deep
files_reviewed: 17
files_reviewed_list:
  - .github/workflows/ccpp.yml
  - doc/fvwm3_manpage_source.adoc
  - fvwm/builtins.c
  - fvwm/events.c
  - fvwm/meson.build
  - fvwm/status_json.c
  - fvwm/status_json.h
  - libs/FScreen.c
  - libs/FScreen.h
  - libs/monitor_topology.c
  - libs/monitor_topology.h
  - tests/live_x/live_x_harness.py
  - tests/meson.build
  - tests/run_in_linux.py
  - tests/test_fscreen_topology.c
  - tests/test_monitor_topology.c
  - tests/test_status_json.c
findings:
  critical: 0
  warning: 0
  info: 0
  total: 0
status: clean
---

# Phase 1: Final Remediation Code Re-review Report

**Reviewed:** 2026-08-30T16:34:38Z
**Depth:** deep
**Files Reviewed:** 17
**Status:** clean

## Summary

All original findings CR-01 through CR-04 and WR-01 through WR-03 remain resolved. Final remediation commit `842e7802b` also resolves the re-review's remaining live-X false-green warning. The wrapper now requires the exact four members of the project-qualified `fvwm3:live-x` suite and rejects missing, extra, duplicate, and mislabeled membership. The change introduces no new correctness, resource, command-routing, or CI reliability defect.

All reviewed files meet quality standards. No blocker or warning remains.

## Narrative Findings (AI reviewer)

### Original Finding Resolution

| Finding | Final verdict | Direct evidence |
|---|---|---|
| CR-01 | Resolved | `fvwm/status_json.c:23-80` constructs one screen/desktop object at a time with explicit ownership; `fvwm/status_json.c:112-118` has no fixed-capacity indexing. `tests/test_status_json.c:118-176` covers 1,025 retained monitors. |
| CR-02 | Resolved | `libs/FScreen.c:838-869` preserves legacy disabled/y/x/width/height/name ordering with relational comparisons. `libs/FScreen.c:871-890` uses `int64_t` distance arithmetic, while `libs/FScreen.c:1273-1297` preserves half-open containment and strict-lower-distance/RB-order tie selection. Geometry endpoint representability is enforced at `libs/monitor_topology.c:75-80`; extremes are covered at `tests/test_fscreen_topology.c:450-478`. |
| CR-03 | Resolved | Emissions are cleared once at `libs/FScreen.c:1013-1018`, missing active records retain `MONITOR_DISABLED` through remove/reinsert traversal at `libs/FScreen.c:1055-1070`, and the changed-generation path emits once at `fvwm/events.c:1655-1681` and `fvwm/events.c:1702-1711`. The regression verifies retained then cleared state at `tests/test_fscreen_topology.c:544-561`. |
| CR-04 | Resolved | `libs/monitor_topology.c:435-444` formats the complete `uint64_t` with `PRIu64`; exact `2^53 + 1` and `UINT64_MAX` wire values are tested at `tests/test_monitor_topology.c:395-429`. |
| WR-01 (Status atomicity) | Resolved | All required creation and attachment operations are checked at `fvwm/status_json.c:35-79` and `fvwm/status_json.c:92-123`; exhaustive allocation-failure cleanup is tested at `tests/test_status_json.c:178-232`. |
| WR-02 | Resolved | `tests/run_in_linux.py:49-56` reconfigures with live-X enabled, compiles, and invokes the exact-suite preflight before the caller. |
| WR-03 | Resolved | Default Meson self-tests are registered at `tests/meson.build:83-92`; both ordinary and live-X CI jobs invoke the self-tests and workflow checker at `.github/workflows/ccpp.yml:33-37` and `.github/workflows/ccpp.yml:86-90`. |
| Re-review WR-01 | Resolved by `842e7802b` | `tests/run_in_linux.py:37-43` fixes the expected names and exact project-qualified suite. `validate_live_x_tests()` at `tests/run_in_linux.py:69-82` computes missing and extra members and uses cardinality to reject duplicates. Tests at `tests/run_in_linux.py:135-153` prove valid membership succeeds and mislabeled, missing, and extra records fail. |

### Final Change Regression Check

- The preflight command uses the repository script after source synchronization and compilation (`tests/run_in_linux.py:49-61`), so Linux and Darwin/Podman routes execute the same validator.
- Exact `fvwm3:live-x` qualification matches the Meson project name at `meson.build:1-3` and all four current `suite: 'live-x'` registrations at `tests/meson.build:121-198`.
- Missing and extra set checks plus the exact length check reject omitted tests, unexpected suite members, and duplicate records (`tests/run_in_linux.py:71-82`). An expected test assigned only to another project-qualified suite is absent from the selected suite and fails as missing.
- File I/O, JSON decoding, schema/type failures surfaced by the validator, and membership failures return nonzero at `tests/run_in_linux.py:226-234`; valid preflight returns zero.
- Internal modes do not execute caller commands (`tests/run_in_linux.py:196-234`), while normal Linux and Podman argv routing remains unchanged at `tests/run_in_linux.py:85-127` and `tests/run_in_linux.py:236-253`.

### Verification Performed

- Inspected commit `842e7802b` and its current implementation directly.
- Ran `python3 tests/run_in_linux.py --self-test`; it passed.
- Ran `python3 -m py_compile tests/run_in_linux.py`; it passed.
- Ran `git diff --check 842e7802b^..842e7802b`; it passed.
- Considered the supplied canonical live-X result: 4/4 tests passed.

---

_Reviewed: 2026-08-30T16:34:38Z_
_Reviewer: the agent (gsd-code-reviewer)_
_Depth: deep_
