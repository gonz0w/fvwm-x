# Project workflow and code navigation

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

The user requested this cadence on 2026-10-06: complete more related stories
before extensive multi-platform testing. Use focused checks during development
and one full qualification pass for the combined checkpoint. Required platform
coverage stays in place; it is not required after every story or small edit.

Prefer a batch of roughly three to five related stories, or the natural set
needed to complete one ownership/behavior contract. Keep individual changes
reviewable and track each story separately. Do not expand a batch into unrelated
rewrites, or wait for the entire backlog before qualification.

Use one PR for that coherent checkpoint, rather than one PR per story. Keep
related code, tests, docs, tracker updates and evidence in the same PR; add
related corrections to it while it is open. Split only for an independent
contract, different ownership/dependencies, a configuration decision or a diff
that cannot reasonably be reviewed together. A PR count is not a progress goal.
Prepare docs and evidence before freezing the final candidate so routine upkeep
does not create a separate follow-up PR. Record the final commit/tree in the PR
and tested input identities in the report; reports need not embed their own
commit hash. An already merged PR gets a new batch when further work is needed.

| Stage | Checks and status |
| --- | --- |
| Each story | Compile affected targets in the existing Linux development environment; run affected policy tests and the relevant reproducer. Run focused nested-X checks for changed X behavior and scoped sanitizers when memory/lifetime changes warrant them. Record implementation and focused evidence; keep full qualification pending. |
| Platform-sensitive change | Check the affected BSD/architecture early when changing OS APIs, signals, descriptors, resource limits, native ABI or build detection. Use the relevant focused gate, not the whole matrix by default. |
| Combined checkpoint | Freeze the source and run required full Linux/BSD compiler, integration, install, default-server and relevant optional-feature/sanitizer gates once for the batch. Record exact source/profile scope and required remaining release checks. |
| Failure or later change | Fix the concrete failure and rerun affected checks. Preserve valid earlier evidence; repeat broader checks only when changed shared behavior, build inputs or unresolved concerns justify them. Never carry a stale result forward as qualification for different code. |

During implementation, use statuses such as **implemented / focused checks
passed / full qualification pending**. Close the qualified checkpoint only when
its required gates pass. Existing accepted reports remain historical evidence;
this cadence does not retroactively change their scope.

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

Source synchronization must give changed files fresh modification times. Do
not restore old archive timestamps over a reused source tree and expect Ninja
to infer changes from file hashes. Preserve another owner's tree, logs and
artifacts. The wrapper compiles default targets, and tests retain Meson's
prerequisite build for targets excluded from that default (such as Go test
binaries). Install uses `--no-rebuild` after compilation/testing, and failures
still prevent installation. Full portable
checks run once at the combined checkpoint. Docs/status-only corrections use
repository/link/branch-diff checks and retain unchanged runtime evidence.

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
