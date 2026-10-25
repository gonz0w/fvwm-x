# Project workflow and code navigation

Apply the [pinned portable method](../.agent-temper/METHOD.md) and the
[project's composition and decisions](PROJECT.md#agent-temper-composition-and-project-decisions).
The local tracker and existing risk-based cadence below remain authoritative;
GitHub is a mirror, not a newly selected Temper profile.

Use one versioned project system:

| Question | Authority |
| --- | --- |
| What is this product and what must remain compatible? | [PROJECT.md](PROJECT.md), [DEVELOPERS.md](DEVELOPERS.md), root AGENTS.md |
| What should be built next? | [PROJECT-TRACKER.md](PROJECT-TRACKER.md#execution-order) |
| Where is the relevant code and who owns state? | [ARCHITECTURE.md](ARCHITECTURE.md), source and Meson |
| Why was a contract chosen? | [decisions](decisions/README.md) and tracker decisions |
| What actually passed? | [verification](verification/README.md), exact logs/artifacts and support matrix |
| How does a user operate the desktop? | AsciiDoc in `doc/`, generated man pages |
| What preceded the current system? | [history](history/README.md) |

## Bounded work and upkeep

Read the relevant tracker items, source and tests. Choose a coherent batch of
reviewable stories and one combined acceptance checkpoint. Preserve another thread's dirty files/builds and reuse
the owning helper rather than inventing a competing transport, cache or policy.
After verification, update status with source/profile scope and evidence.

Before changing a shared helper's accepted inputs, inspect its shipped callers,
including test probes and installed consumers. Check NULL/empty conventions,
boundary lengths and ownership before treating them as invalid. Freeze only the
owning build-file blocks; a shared checkout can contain another chat's newer
targets whose sources belong outside the checkpoint.

Update architecture rows when entry points or ownership change. Add decisions
when a cross-cutting policy is approved; keep proposed decisions visibly proposed.
Never promote a historical checkbox or graph edge into passing evidence.

Keep the tracker path and current guide entry points stable. New dated evidence
belongs under `verification/`; proposals/accepted contract records belong under
`decisions/`. Link the owning tracker ID. Fix references when moving documents,
retain failed/replaced reports and distinguish observations from hypotheses.

Keep a short current checkpoint summary near the top of the tracker. Separate
accepted, running and prepared-but-undispatched checkpoints, and link their
reports. Dated progress entries are history: retain them, but do not rely on
their old "pending" wording to describe current status. Other guides should
link the current summary instead of maintaining competing queues. Record user
changes to immediate work selection there without marking paused or unfinished
work complete.

## Implementation batches and test cadence

User direction, 2026-10-23: new feature pickup is paused while source cleanup,
legacy retirement, dependency reduction and maintenance are organized and
implemented. Follow the [maintenance-first batches](decisions/MAINTENANCE-FIRST-BATCHES-2026-10-23.md)
and the tracker queue. This supersedes the earlier feature-first handoff and
future-release-only placement of general cleanup. Preserve accepted component
work. The [October 25 modernization order](decisions/MODERNIZATION-ORDER-2026-10-25.md)
restores bounded measured optimization alongside cleanup; #209 records the finite
accepted handoff before new feature pickup. Existing-behavior parity for a named
removal remains maintenance.

Group related ownership/error/unwind, dependency, predecessor and tool changes.
Choose finite source/behavior acceptance at pickup rather than reopening a whole
audit. No age-based deletion, broad mechanical rename or inferred service/library
replacement. Serious reproduced failures still advance with their concrete
regression and owner. No new mandatory GUI toolkit or core-state authority.

User direction, 2026-10-23: use [larger related cleanup batches and risk-based
platform checks](decisions/BATCHED-CLEANUP-CADENCE-2026-10-23.md). The automatic
three-PR/ten-story platform trigger is superseded. Aim for six to ten related
substantive improvements where useful, or a complete reviewable owner slice;
counts are not a target for invented stories or unrelated rewrites. Keep related
cleanup in the open owner PR when it improves the same contract. The user has
reopened bounded cleanup; the old full hardening backlog remains deferred.

Implement a coherent group before its combined focused test gate. Use cheap
affected compilation when useful during editing; test early for a reproduced
failure or a dependent contract that needs proof. Do not repeat suites, theme
sets, sanitizers, installs or platform checks after each small edit.

### Feature PR gate

Use one existing incremental Linux development profile and compile affected
targets. Run the affected policy/regression tests, plus nested-X integration
for changed X-visible behavior. Add scoped sanitizers for changed memory/lifetime ownership and race checks
for changed concurrency; moving an unchanged caller is not such a change. Run repository/link/diff checks and inspect the change. When OS APIs,
native ABI/architecture, signals/descriptors, compiler or build behavior actually
change, check the affected target early. An unchanged portable socket/process
API is not itself a reason for another BSD boot. Run the focused gate once at
the combined checkpoint; repeat only affected checks after a correction.

An ordinary PR does not rerun every compiler, optional profile, staged install
or BSD default-server session. Test the built affected module/helper directly.
Run an early installed-path test when packaging, install rules, resource lookup,
runtime linking or installed entry points change. Run the affected default-X
gate early for server-dependent pointer/grab/RandR/rendering behavior. Broaden
for shared dispatch/ordering, native packet changes or a concrete regression
when the selected focused checks cannot establish the changed contract. Record
the reason and relevant targets rather than automatically running everything.

For visible native UI work, capture and inspect screenshots from the affected
actual-X run and include a small review set in the user handoff. Preserve them
with canonical source-bound evidence. Reuse that integration run; screenshots
do not require another matrix or a separate desktop setup.

### Representative theme compatibility

User direction, 2026-10-23: exercise new native primitives with two or three
representative FVWM theme configurations as development progresses. Use the
[theme smoke set](THEME-STANDARDS.md#primitive-theme-compatibility-smoke-set):
Slate dark, Dune light, and a controlled classic/minimal customization with
larger fonts and explicit legacy module colors. Keep actual configuration
snapshots and screenshots with the owning checkpoint; coverage remains pending
until those configurations execute.

Run this small set once on the reused Linux native-X build at shared UI
checkpoints, and earlier when a change directly affects font, palette, focus,
layout or rendering contracts. A local geometry/focus iteration may use one
profile; run the other profiles together before the checkpoint. Add affected
BSD/font/backend checks where needed, without multiplying every compiler,
install or OS run by all themes. Reuse built binaries and the existing session
harness. Fix a regression and repeat affected checks only. The wider default
theme/accessibility and release matrices retain their separate scope.

### Shared integration cycle and release gate

Review accumulated risk after about five merged implementation batches or a
substantial subsystem handoff. This is a review point, not an automatic matrix
or a blocker on starting the next ordinary batch. Maintain the integrated
reference, affected subsystems and missing coverage in the tracker. Select
additional target/compiler/optional checks when accumulated changes or a
concrete failure justify them; document the reason. Portable policy/refactor
batches normally use one incremental Linux profile. Do not boot all six targets
because a PR counter reached a number. Install/default-X checks follow changed
installed or server-dependent contracts, not a new PR or moved helper.

At milestone closure and before a release candidate, complete the full required
support matrix: Linux/FreeBSD/OpenBSD on AMD64/ARM64, required compiler/feature
profiles, native policy and real-X integration, staged installs, default BSD-X
and applicable sanitizer/acceptance coverage. Reuse exact valid cycle evidence
and run missing coverage; changing a PR/milestone label is not a reason to
repeat an identical accepted gate. Native-CPU/hardware/toolkit/compositor gaps
remain separate. No release qualification is inferred from focused PR passes.

Maintain a short cycle ledger in the tracker: integrated reference, merged
feature PRs since it, affected subsystems, deferred gates and closure trigger.
Feature PRs may merge as **focused checks passed / broader coverage pending**.
Do not close a broader owner or acceptance checkpoint until its criteria pass,
or count deferred checks as successful execution. Preserve valid earlier
evidence with its actual source/profile scope. A later correction reruns
affected checks; repeat broader coverage only where inputs or unresolved
concerns invalidate it.

Preserve known unrelated failures with their owning issue and exact source/
profile. Do not repeatedly retry them for a clean matrix or expand the current
batch to repair them. A newly observed failure gets triage and, when useful,
one unchanged isolated control. Unresolved failures remain explicit and can
prevent that profile's install/qualification; a suspected or actual regression
in the batch must be resolved before its acceptance.

Reuse existing builds, guests and qualified unchanged artifacts. Fresh image,
package/bootstrap and dependency qualification belongs to changes in those
inputs, not every feature PR. Use valid owner-controlled development guests
where supported; do not improvise an unsafe resume or mutate another owner.
Bound concurrent
VM jobs to available resources rather than starting every expensive guest at
once. While a frozen candidate is testing, use the wait for review, next-batch
design or independent implementation in a separate checkout; never mutate the
source/build being tested. Consolidate reports and tracker/issue upkeep once
per checkpoint, generate receipts from existing logs, and use documentation/
link/whitespace checks for documentation-only edits. Creating or updating a PR
never triggers another matrix by itself.

Use one PR for a coherent feature batch, rather than one PR per story. Keep
related code, tests, docs, tracker updates and evidence in the same PR; add
related corrections to it while it is open. Split only for an independent
contract, different ownership/dependencies, a configuration decision or a diff
that cannot reasonably be reviewed together. A PR count is not a progress goal.
Prepare a concise source-bound PR record with selected commands/results, review
and deferred checks. Consolidate broad reports/canonical receipts at cycle
closure instead of duplicating a release dossier for each feature PR. Routine
upkeep stays with the open PR. Record final commit/tree and tested inputs;
reports need not embed their own commit hash. An already merged PR gets a new
batch when further work is needed.

| Stage | Checks and status |
| --- | --- |
| Feature PR | One incremental Linux profile; affected compile, policy/regression and nested-X checks, scoped sanitizers when warranted, screenshots for visible UI, repository/diff review. Explicitly record integration/release coverage pending. |
| Platform/install/server-sensitive change | Add the affected target or installed/default-X gate early. Broaden only for the changed contract or a concrete unresolved regression. |
| Accumulated risk review | About five merged implementation batches or a substantial subsystem handoff; inspect coverage debt and select justified affected targets. No automatic all-OS run or next-batch blocker. |
| Milestone / release candidate | Complete required full matrix, compiler/features, staged installs, default-X, applicable sanitizers and acceptance; reuse exact valid cycle evidence and execute missing coverage. |
| Failure or later change | Fix the concrete failure and rerun affected checks. Preserve valid earlier evidence; repeat broader checks only when changed shared behavior, build inputs or unresolved concerns justify them. Never carry a stale result forward as qualification for different code. |

Existing accepted reports remain historical evidence; the new cadence does
not retroactively change their scope or relabel a failed/unrun check.

A reproduced serious defect gets its focused regression check immediately.
Only a deliberate configuration-file/format break or required configuration
migration needs a compatibility decision. Do not stop for internal APIs/ABIs,
module protocols, rendering, incidental timing or error/failure behavior alone.
Other explicit product constraints and actual blockers still apply. Build once per needed
profile and reuse incremental builds; do not clear caches, rebuild all modules,
boot all guests or repeat the full matrix solely for a small local change.
Serialize source sync, configure, build and test within each guest/build directory;
parallel platform gates must use independent environments.

Keep a stable build directory per owner, target, compiler and feature profile
during development. Ninja tracks changed inputs; repeated identical scalar
`./build -D` options now reuse Meson's configured options. Changed or unknown
options, arrays and unavailable option metadata use ordinary Meson setup.
`--reconfigure` and `-f` remain explicit refresh/wipe requests. Use fresh builds
when changing compiler/ABI or when invalidation cannot be established; refresh
dependency detection when dependency inputs change. The native qualification
gate deliberately retains its dependency-cache refresh contract.

The Linux linker-wrap WM fixture shares production core objects; its fault
macro applies only to the probe. Actual-core fixtures retain their separate
assertion-enabled core build. Keep operation-only APIs and private derived
indexes out of broadly included record headers where their layout is not needed.
Measure actual Ninja dependencies and compiler work when changing these boundaries.

Source synchronization must give changed files fresh modification times. Do
not restore old archive timestamps over a reused source tree and expect Ninja
to infer changes from file hashes. Preserve another owner's tree, logs and
artifacts. The wrapper compiles default targets, and tests retain Meson's
prerequisite build for targets excluded from that default (such as Go test
binaries). Install uses `--no-rebuild` after compilation/testing, and failures
still prevent installation. Full portable
checks run once at the shared integration checkpoint, or earlier when their
own harness changes; use affected harness tests during development.
Docs/status-only corrections use
repository/link/branch-diff checks and retain unchanged runtime evidence.

User direction, 2026-10-20: do not accumulate completed guest test-data
copies. After a canonical host capture passes full archive integrity and
payload/identity validation, retire its owner-controlled guest capture and
duplicate archive with exact file/hash and path guards. Keep the canonical
host evidence referenced by reports, source trees, reusable incremental builds,
install stages (including performance references), VM disks and other owners'
work. Failed
attempts must remain represented in canonical evidence before guest retirement.
Record what was removed and the preserved canonical identity; a new PR does not
require keeping another guest copy or repeating tests.

## Periodic performance comparisons

During safety closeout, retain the existing five-checkpoint observations as
lightweight regression telemetry. They do not select tuning work or delay a
safety batch to obtain new benchmark builds or a quieter timing result. Use
existing binaries and record unavailable observations. Investigate a concrete
regression in the owning change; ordinary timing noise does not reopen tuning.

User request, 2026-10-20: track responsiveness, CPU, memory and executable size
without adding a benchmark run to every batch. Establish the first paired
observation at the visual-startup checkpoint after PR #131, then compare every
five merged implementation checkpoints against the last recorded reference.
Record the reference revision, completed-checkpoint count and next comparison
in the tracker. A change expected to affect hot-path cost or retained memory
can bring the comparison forward and reset the count. Documentation-only PRs
do not advance it. This is checkpoint upkeep, not a scheduled background job.

Use `tests/compare_performance.py` on one quiet Linux development profile with
already built reference/current binaries. It alternates three paired runs of
the existing idle/title/resize profiler and keeps raw samples, achieved load,
command/timer median and p95, CPU, Linux peak RSS and `size` output. The same
current profiler drives both binaries; retain their source/build identities,
compiler/options, dependencies and VM/server settings. Different environments
need a new reference. Peak RSS is the process high-water mark at each case,
not isolated per-case allocation. File bytes include debug information;
`size` text/data/bss totals describe a different, narrower measure.

When changing the comparison wrapper, run the opt-in process controls with
`python3 tests/test_performance_comparison.py`; they verify normal completion
and timeout cleanup without starting a WM or measuring performance.

Keep reports under `dev-docs/verification/` and link their machine-readable
receipts and canonical logs. Record source C/header deltas separately from
tests/docs and binary/runtime memory. Report noise and unavailable metrics;
timing ratios are observations, not pass thresholds or universal desktop-speed
claims. A concrete regression warrants focused investigation. Reuse existing
micro-work measurements when relevant; do not rebuild both references, run a
six-target performance matrix, or repeat correctness qualification merely to
refresh the comparison. See [SUPPORT.md](SUPPORT.md#performance-observations).

## Replacing the graph

The old graph mostly indexed documentation, including retired planning files.
The maintained architecture map is a starting point, not a claim of a complete
semantic call graph. Search actual references with `rg`; inspect callers,
dispatch tables, callbacks and compile configuration before claiming code is dead.
The existing source-inventory tool records evidence, not full C semantics.

Meson compilation databases and compiler-aware navigation can supplement this
when needed. Any generated index should remain disposable, tied to source and
configuration, and separate from authoritative project decisions. Add no opaque
background hook or new runtime dependency merely to maintain project context.

GSD phase automation is retired. `.planning` and the `.gsd` sentinel were moved
to history. The ignored `.gsd-graph` cache remains available locally with
`enabled=false` and `auto_update=false`; its hook scripts are not the workflow.
Root AGENTS.md now holds concise current instructions, with the prior generated
version preserved in history.

## Optional Codex integrations

Plugin discovery on 2026-10-05 found GitHub, Codex Security and Superpowers.
GitHub is useful for PR/issues/CI after this project has a remote and connection;
Codex Security can supplement the project's security investigations. Neither
was installed by this migration. Superpowers is a development-methodology
framework, not an established replacement for a C repository graph; adding
another project framework is unnecessary for this workflow.

Local file/search/build tools already cover repository work. Context7 tools are
available in this session for library documentation; prefer primary X.Org/API
documentation when investigating X11 behavior. Plugin discovery is not exhaustive;
other options may exist in the [plugin directory](https://chatgpt.com/plugins).

Skills package repeatable workflows; plugins can add skills, MCP services and
hooks. A small project skill could later automate a demonstrated repetitive
task, but context management alone does not require installing a framework.
See [official skills documentation](https://developers.openai.com/plugins/concepts/skills)
and [plugin architecture](https://developers.openai.com/plugins/concepts/plugins).

For this task's owned OpenBSD QEMU checkpoint evidence, create each new current
verification tree on /home and retain its conventional source-tree path with a
symlink. Keep qualified predecessor evidence and guest build trees. If an
existing current tree must move after a capacity failure, compare every regular
file hash and mode before removing that original tree; preserve the failed
attempt and resume only unfinished checks. This storage setup does not change
source scope, test registration, deadlines or release coverage.

## GitHub and local review

The [approved hosting policy](decisions/LOCAL-REVIEW-GITHUB-2026-10-13.md) uses
`gonz0w/fvwm-x` for code, issues and PR review with Actions disabled. All review,
tests, lint and validation run locally. Keep one manual issue/Project mirror of
the tracker, preserving its execution order and separate acceptance gates.
Update README status when milestones or scope change; retain detail in linked
guides. Native CPU/platform and physical evidence remain explicit, regardless
of where the command ran. Follow [CONTRIBUTING.md](../CONTRIBUTING.md).

Use the [GitHub workflow](GITHUB-WORKFLOW.md) and [stable issue index](GITHUB-ROADMAP.md)
for manual triage; native dependencies express actual prerequisites, not every
reference or completion of an entire broad optional owner.
