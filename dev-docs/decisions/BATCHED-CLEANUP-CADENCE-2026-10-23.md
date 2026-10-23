# Larger cleanup batches and risk-based platform checks

Accepted user direction, 2026-10-23: combine larger sets of similar cleanup
issues, then test the combined result. Repeated all-OS checks for small changes
are slowing delivery. This supersedes the automatic three-PR/ten-story platform
trigger in the [previous cadence](FEATURE-TEST-CADENCE-2026-10-22.md).

## Decision

- Batch by subsystem and shared contract. Aim for roughly six to ten substantive
  related improvements when useful, or a complete reviewable owner slice. Do
  not invent extra stories or mix unrelated rewrites to hit a number. Related
  cleanup can enlarge an existing open PR; independent work gets its own batch.
- Implement the group before running its combined focused gate. Use cheap
  affected compilation while editing when useful; test sooner for a reproduced
  failure or a dependency that needs proving. Do not run a test suite, sanitizer
  run, theme set, install or platform matrix after each helper/refactor.
- For portable cleanup, one incremental Linux build and affected regression
  tests are the normal merge gate. Add one nested-X exercise when changed
  orchestration affects native state. Run race/sanitizer checks for changed
  concurrency or ownership contracts; unchanged transports do not require new
  sanitizer runs simply because a caller moved between files.
- Add BSD/architecture/compiler checks when the change actually affects their
  APIs, ABI, conditionals, build/link/dependency detection or a known target
  failure. Merely using an unchanged portable socket/process API is not a
  reason to boot BSD. Install/default-X checks require an affected contract.
- Keep a pending-coverage ledger. Review accumulated risk after about five
  merged implementation batches or a substantial subsystem handoff; this is
  a review trigger, not an automatic six-target job or a blocker on the next
  ordinary batch. Select missing affected targets and record the reason.
- Full required Linux/FreeBSD/OpenBSD AMD64/ARM64, compiler/feature/install/
  default-X and applicable acceptance checks remain at milestone/release
  qualification, or earlier when focused evidence cannot establish a changed
  contract. Reuse exact valid evidence and execute missing gates once. Never
  describe unrun platform coverage as passing.
- New rendering/layout/theme behavior still gets representative themes and
  real screenshots once per combined UI checkpoint. A refactor with unchanged
  native/rendering/configuration sources reuses the existing gallery; it does
  not create another three-theme run. No new performance comparison is due
  solely for cleanup or PR creation; retain the periodic comparison cadence.

PR186 remains open and is enlarged with four related provider maintainability
improvements: one operation catalog for capability/admission/worker routing,
strict typed value decoding, separate query versus mutation ownership, and
compiler-checked query results. Run the accumulated cleanup's Linux policy/vet/
build and one native-X provider/handoff exercise. Existing C/ABI/build/install/
rendering contracts are unchanged. Preserve the earlier feature/platform receipts
under their original sources; report the cleanup as focused qualification only.

This change adds no test-selection framework, scheduler, hosted CI or report
requirements per small edit. One short combined receipt and tracker update
serves the batch. Broader cleanup owners remain open; performance tuning stays
deferred. The user explicitly reopens bounded related cleanup now, without
making the entire old hardening backlog a prerequisite for new features.
