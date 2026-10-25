# Modernization planning and issue reconciliation

Date: 2026-10-25. Owner: Cam (@gonz0w). Planning owner: MODERNIZE-01 #208.
Scope: issue/Project organization, existing-owner handoffs and authoritative
documentation. No C/Go/Meson/runtime behavior changes.

## Recorded changes

- Created twenty scoped issues #208–#227 with outcomes, acceptance, owner,
  scheduling, dependencies and local verification boundaries.
- Updated 44 existing issue bodies, preserving accepted checklists/evidence.
  Restored #29/#33/#37/#38/#39 to milestone 04; #29/#34 are P1. Queued work
  remains Backlog rather than being falsely marked active or complete.
- Added nineteen native parent links and thirty-three native blockers. Existing
  parents/blockers are retained. Feature roots reach the bounded #209 scheduling
  gate; no current maintenance owner waits for new feature implementation.
- Preserved all fourteen existing Work batch option identities while renaming
  deferred performance and paused features; added F1–F4. Mirrored twenty new
  cards and updated only the affected priority/batch fields and issue positions.
  Existing status, unrelated PR cards and closed-component state are retained.
- Updated tracker/scope, stable Markdown/JSON mapping, workflow, decision and
  brief README. Current instructions supersede older performance deferral while
  preserving maintenance-first scope and accepted runtime/dependency work.

## Verification

Remote readback passes across 116 issues and 173 Project items: all twenty new
issues match their labels/milestones/fields; prior bodies and accepted evidence,
existing native parents/blockers, old card statuses and fourteen Work batch
option identities are preserved. All future feature paths reach #209 and the
complete graph has zero cycles. The [machine receipt](MODERNIZATION-PLANNING-2026-10-25.json)
records the applied relationships and snapshot hashes.

`make check` and working-tree `git diff --check` pass. The first documentation
check caught the README word limit; the status was tightened to 241 words and
the check passes. Staged `git diff --cached --check` also passes; the final
planning candidate contains documentation/metadata only. No runtime,
benchmark, platform matrix, physical compositor/docking or release acceptance
is claimed by planning. All checks remain local; Actions is disabled.

## Source and review limits

Prepared in an isolated worktree from current origin/main, preserving the dirty
shared checkout. GitHub has accepted newer runtime/settings/IPC/linkage work
than that shared checkout; this plan reuses it and does not overwrite history.
A separate bounded read-only reviewer (requested route `gpt-6.1-sol`, high)
found one live PERF-01 tuning-deferral contradiction. The tracker row, execution selection and
workflow now consistently put measured optimization alongside cleanup; accepted
results and separately scoped experiments remain intact. Follow-up review
confirms the finding resolved with no new material contradiction. The reviewer
checked all six saved snapshot hashes and preservation/cycle/gate assertions.
The reviewed decision SHA-256 is
`72e6f40a6811e690f4233fd21c5e4867371b461d3e388bb2c9d9eee6bcb0f304`;
the machine receipt is
`3d4ae87d9cf79894325da9522c428559070994bc915c45743a0b34696981aaf8`.
Dispatch accepted the explicit route; independent runtime model metadata was
not returned. This is agent review, not human/GitHub approval or native execution.

Remote mutations cover authorized issue/Project organization and the planning
documentation review branch/PR.
Maintainer merge and later feature/operational acceptance remain separate.
