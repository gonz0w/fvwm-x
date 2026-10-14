# GitHub foundation and local review — 2026-10-13

Status: repository administration is configured; the versioned foundation awaits
maintainer [PR #83](https://github.com/gonz0w/fvwm-x/pull/83) review. No release, deployment or broader native/hardware acceptance
is claimed. The [hosting decision](../decisions/LOCAL-REVIEW-GITHUB-2026-10-13.md)
retires remote-CI/workflow-adoption requirements without changing test evidence.

## Repository and ownership

Created private [gonz0w/fvwm-x](https://github.com/gonz0w/fvwm-x). The requested
`~/DEV/DNS` path is absent; its GitHub redirect to DNS/Nomvium supplied the
reference governance/local-review conventions. No DNS product requirements or
credentials were imported. The dedicated private
[FVWM-X Project](https://github.com/users/gonz0w/projects/6) is separate from DNS.

GitHub Actions was disabled **before the first push**. Inherited workflow,
automatic label/dependency and upstream-funding metadata is archived under
`dev-docs/history/github-actions`. There are no active workflow YAML files.
No hosted/self-hosted runner, status publisher or automatic tracker is installed.

Only squash merges are enabled; auto-merge, merge commits, rebase merges and wiki
are disabled. Issues and Projects are enabled; merged topic branches are deleted.
Main ruleset `25040365` is active: PRs, signed commits, linear history, resolved
conversations, no force push/deletion, no bypass actors and zero mandatory
independent approvals during solo maintenance. Release ruleset `25040366`
prevents updating/deleting `v*` tags. Versioned inputs are in `.github/` and do
not apply themselves; the local setup receipts record observed enforcement.
No release tags were created. CODEOWNERS routes ownership without requiring an
author's impossible self-approval.

## Published scope and history

The seed commit is `b651c6139ed4188f6fa92187c51dd2b74f428d06`, descending from the
original checkout's history rather than replacing it with an orphan export.
GitHub reports its signature verified/valid. Contributor commits use the existing
registered SSH signing identity and matching author DCO; no new private key or
global Git configuration is created.

Source is the qualified
`cec11a43f9691f0ec92c05039ac6014b8b1da789e403fb6fa987bfe3c90a8092`
PNG/provider snapshot. All 1,082 C/header/Go files remain byte-identical. The
management PR changes metadata, documentation and the portable developer check;
it does not import another chat's newer runtime changes. The original dirty
checkout, builds, disks and VM evidence are preserved. Its origin now points to
the new repository; no reset, rebase or checkout discards working changes.

The publication checkout is isolated at
`builddir/codex-2026-10-13/github-setup/publish`. The source snapshots, seed
preservation/signature records, remote settings and command logs are retained
under that task directory. Final PR head/tree/base and local signature/DCO
observations belong to the PR and its local candidate receipt, avoiding a
self-referential commit hash inside this report.

## Roadmap organization

All 81 authoritative tracker IDs have matching issues, priority/status/stage
labels, milestones and Project fields. Twelve backfill already accepted local
components; they do not close broader audits or release gates. The bounded
[foundation issue #82](https://github.com/gonz0w/fvwm-x/issues/82) belongs under
CI-01 and closes only when its reviewed foundation PR merges.

Ten milestones represent stages1–9 and Later, without invented deadlines. The
roadmap table is ordered by remaining stage, priority and tracker row order; an
open-work board is also configured. Remaining monitor/protocol work and runtime
controller ports belong to stage5, preserving accepted stage3 checkpoints.
Twenty-seven native blocked-by links express explicit prerequisites. Optional
sub-contract references do not gate consumers on an entire broad owner or
introduce dependency cycles. The [stable index](../GITHUB-ROADMAP.md) and
[workflow](../GITHUB-WORKFLOW.md) specify manual reconciliation, not an automated
second backlog.

The first Project-ordering request returned HTTP502. Its original failure is
retained. Idempotent metadata assignments/positions were reconciled with bounded
retries; no duplicate issues or new product acceptance was inferred.

## Local checks and remaining evidence

`make verify` checks foundation file/link hygiene, README brevity, absence of
active Actions files, changed whitespace, Python/shell syntax, maintenance tools
and native-gate/asset/QEMU/PNG policy. It does not compile the WM or establish the
complete native/X11/Go/race/sanitizer/install/hardware matrix. Local hooks are
explicitly opt-in and bypassable; remote rules do not enforce local evidence.

The initial macOS run passes 96 cases, with five native-Meson catalog checks
explicitly skipped. Those same five pass in an isolated source tree in the
existing local Linux ARM64 lab; its source hash and actual log are retained.
The affected test/Meson/catalog sources are unchanged in the final metadata
candidate, so the native result is retained separately, not relabeled as a
fresh full matrix. Final quick verification, local signature/DCO and exact
head/base/tree records accompany the PR.

The initial import's old/generated whitespace fails `git diff --cached --check`
against its upstream base. Those findings remain in `seed-whitespace.log`, with
unchanged qualified sources; they are not a passing lint result or a reason to
restyle generated runtime code here. The subsequent management PR's diff must
pass its own whitespace check. CLEAN-01/GEN-01 own any later source/generator
repair. Observed paths include `libs/image_svg.c`, `image_svg_protocol.c`,
`module_x_service.c`, generated Script scanner/parser/header files, two native-X
fixtures and the Constants POD template.

Existing PNG/provider/native reports retain their exact scope and failed trials.
Native CPU/sanitizer, physical GPU/input/toolkit/compositor and final release
requirements remain explicit local qualification work. GitHub setup is neither
a production release nor evidence that those unrun checks passed.
