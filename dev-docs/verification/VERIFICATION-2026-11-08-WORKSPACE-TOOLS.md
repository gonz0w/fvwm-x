# Workspace tools implementation checkpoint — #221/#223

2026-11-08. **Draft only: native UI bootstrap failed before the new scene.**
No new screenshot or issue-closure claim. The candidate adds named scratchpads
and a searchable native workspace/window overview as one related feature batch.
[Behavior and wire contract](../decisions/WORKSPACE-TOOLS-2026-11-07.md).
[Source hashes and command receipts](VERIFICATION-2026-11-08-WORKSPACE-TOOLS.json).

## Observed passing checks

- Incremental production core, Desktop and Prompt build; four C executables:
  workspace-tools-policy, workspace-tools-core, palette-policy and
  action-palette-snapshots. The core and C passes are reused unchanged after
  controller-only repairs.
- Five affected Go packages: desktopcontroller, desktopcheck, actionpalette,
  collection and commandmeta. Four sibling passes are retained from the combined
  run; the repaired controller package passes separately.
- Desktop controller race check; two isolated recipe-derived ASan/UBSan policy
  and in-memory-core executables; affected FvwmDesktop manual render.
- Separate static core/Go/wire review, repaired findings and evaluation-input
  preflight. Agent review is a second examination, not human approval.

Execution uses the existing Linux ARM64 AArch64 guest profile on the existing
Apple host, its admitted SDK, library and build inputs. Sanitizer results cover
selected new policy/core objects and their units linked against existing
libraries; they do not establish a whole-program or six-target release pass.
No new dependency, package, install, full-platform or benchmark campaign ran.

## Native failure and stopping point

The shared action-palette harness did not observe its initial application
catalog within its existing deadline. Failure occurred at
`tests/live_x/action_palette_live.py:441`, before `workspace_tools_live.scene`.
Slate therefore has no new feature cases or captures; Dune and enlarged Classic
were not reached. This is an unresolved bootstrap failure, not evidence that
the scratchpad/overview interaction scenarios passed. Retained worker receipts and actual menu publications show discovery succeeded
with two entries. The third was skipped because the old fixture used
`shlex.join` shell quotes in its Desktop Entry Exec field. The shared parser
correctly rejects single-quoted shell syntax. The candidate now reuses the
installed-XDG fixture's two-stage Desktop Entry serializer, preserving literal
argv, percent escaping and the exact readiness expectation. This fixture-only
correction is source-reviewed but **not rerun** after the attempt limit. No
mask/startup regression is established by this failure.

The failed fixture records no cleanup failures or unexpected observer X errors;
all harness children exited. Both the owned container and VM shut down with
confirmed receipts. The foreign VM was neither controlled nor qualified.

## Frozen acceptance status

| Issue | Implemented and checked without live UI | Still needed before closure |
| --- | --- | --- |
| #221 | Literal definitions, core-owned bounded claims/leases, matching/refusal policy, coalesced toggles, uncertainty/retry, fixed command plans, focus guards and manager source; parser/policy/controller units and scoped sanitizers pass. | Actual terminal/application keyboard launch/summon/dismiss/focus, lifecycle/refusal/placement cases and three-theme/enlarged UI evidence. |
| #223 | Committed scalar facts, snapshot/identity/topology guards, exclusive virtualized overview, typed mark/move/native-pointer submission and labels-first decision; core/controller regressions pass. | Actual integrated search/switch/move/focus/drag, stale/teardown/update behavior and three-theme/enlarged captures; reconcile bounded large-list reuse against the new consumer. |

Underlying palette, collection, workarea and theme evidence retains its original
scope. It cannot replace the new feature-specific UI checks. Full R1 release
platform/default-server/install/physical/assistive acceptance remains separate.
Both issues stay open; keep the PR draft until the corrected fixture reaches
and qualifies the new UI scene.

## Attempts and retained evidence

One combined run plus two diagnosed focused repairs exhausted the recorded
attempt limit. The initial compiler failure was a missing palette-policy
include. The next run passed build/C and four Go packages but caught idle Form
teardown and a wrapped-notice test oracle. The final repair passes controller,
race, sanitizer and manual checks, then hits the shared native bootstrap failure.
Passing checks were reused; no full matrix was restarted. Budgets were debited
conservatively between repairs, rather than renewed.

Final frozen plan: `7a9b0707137d869a93ddc7785c8037cdc74a23d5e623e22d47abbdd520054275`.
Final raw archive SHA256: `9bbc21b506c89ff616cedc666783f35f0422cc39cdf5a746078f7cb5e17aaf48`.
Lead-owned raw logs and source/control receipts remain in the current private
workspace-tools checkpoint for diagnosis. No new campaign was started at the
limit. Continue only after a finite bootstrap repair/checkpoint is recorded.
