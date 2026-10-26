# GitHub work and local acceptance

Repository: [gonz0w/fvwm-x](https://github.com/gonz0w/fvwm-x), initially private.
Project: [FVWM-X roadmap](https://github.com/users/gonz0w/projects/6).
The [stable issue index](GITHUB-ROADMAP.md) links every imported tracker ID.

The tracker owns execution order, scope and accepted evidence. GitHub organizes
live issue/PR review and mirrors that order manually. Pick a bounded unblocked
outcome from the current tracker batch, inspect open PRs and read its decisions/evidence.
Split broad planning into reviewed implementation slices when picked up. Do not
restart accepted work or interpret a local checkpoint as physical/release acceptance.

## Release roadmap

The [2026-10-26 release plan](decisions/RELEASE-PLAN-2026-10-26.md) supersedes
cleanup-first scheduling. Milestones are R1 Configurable Desktop Lite, R2
Maintenance and Runtime Simplification, and R3 Developer Tooling and Deferred
Work. Retired stage milestones retain completed history. Stage labels/field
remain technical ownership; an old Later stage for an R1 feature is replaced by
its actual subsystem stage. Each open issue has exactly one Major/Minor work
size label and matching Work size Project field, independently of priority.

All current feature/evaluation contracts target R1. Later-release audit parents
or children do not block an accepted R1 subset. Keep actual native prerequisites
acyclic and preserve accepted evidence. Coordination references are not blanket
blockers. New features enter later release intake unless the owner explicitly
changes frozen R1 scope. Finish evaluation decisions and accepted integrations;
no candidate is forced merely because it was evaluated.

## Status and priority

Use exactly one `status:` label, one `priority:` label and one owning stage.
Match Project Status/Priority/Stage and the repository milestone. Bug forms start
in Backlog; maintainer triage supplies priority/stage before pickup.

| Label | Project Status | Meaning |
| --- | --- | --- |
| status:backlog | Backlog | Awaiting its stage, scoped design or prerequisite |
| status:ready | Ready | Bounded and unblocked for pickup |
| status:in-progress | In progress | Active tracker-owned work |
| status:review | Review | Reviewable PR awaits maintainer |
| status:validation | Validation | Remaining explicit qualification; accepted evidence stays accepted |
| status:done | Done | Every scoped criterion accepted; issue closed |

P0 is a reproduced critical defect; P1 is important current foundation work;
P2 is normal planned work. Current user-selected batch order precedes ordinary
stage/priority sorting; a new serious defect jumps ahead. Stage remains technical
ownership, not a release-order field. The Project's
Work batch field mirrors the current batch register. Update positions manually when ownership/order changes.
Milestones follow the three scoped releases; calendar dates remain unset.
Major/Minor measures breadth, not estimated days or severity. Each issue has a
concrete end result, testable acceptance, exclusions and accepted-work notes in
the versioned release contract. Do not reopen accepted audit/utility checkpoints.

Use native blocked-by links for actual prerequisites, not every cross-reference.
A prerequisite contract within a broad owner need not wait for that owner's
whole optional feature. Avoid cycles such as making notifications wait for the
entire bus integration that itself includes notifications. Native sub-issues
scope a bounded outcome under one owning tracker issue; foundation #82 belongs
to CI-01. Keep incomplete issues open with exact remaining criteria.

## Review and acceptance

Follow [CONTRIBUTING.md](../CONTRIBUTING.md): signed author-DCO `codex/` commits,
coherent PR batches and local source-bound evidence. GitHub Actions is disabled.

One coherent acceptance checkpoint normally means one PR, even when it closes
several bounded issues. Put related implementation, tests, docs and evidence
together; update that PR for related corrections while it remains open. Do not
create a PR merely because one story is implemented, or repeat platform builds
solely because a PR was created. [Batch cadence](WORKFLOW.md#implementation-batches-and-test-cadence)
defines when to split and how to reuse the owner's incremental build/evidence.

There are no hosted/self-hosted runners or success-status publishers. `make verify`
checks portable foundation/policy only; affected native/runtime gates remain
explicit. Preserve failures, skipped capabilities and teardown observations.

Use Refs for broader/partial work. Closes applies only when merging satisfies
all criteria of that bounded issue. A component merge does not close a phase,
release or physical-hardware gate. Agents leave PRs open; the maintainer reviews
local evidence and merges deliberately. Keep source/tree/base and actual commands
in the PR; refresh evidence when the relevant candidate changes.

Versioned main/tag rules are policy inputs, not enforcement evidence by themselves.
The [setup report](verification/VERIFICATION-2026-10-13-GITHUB-FOUNDATION.md)
records observed remote settings and local checks. Maintain the brief README
when scope or milestone status changes; keep details in tracker and reports.

## Future feature intake

The roadmap issue form records the proposed release, Major/Minor breadth, finite
outcome/acceptance, actual prerequisite contracts and resource/dependency impact.
New feature requests enter R2/R3 intake unless the owner approves an R1 scope
change. Triage applies the real milestone, work label and matching Project fields;
a form selection alone does not assign them. Add accepted release scope to the
versioned manifest and its release gate. Never expand a closed audit implicitly.
