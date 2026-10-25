# FVWM-X development instructions

Read the pinned [Agent Temper method](.agent-temper/METHOD.md),
[local project policy](dev-docs/PROJECT.md) and the selected owning work item first.
Only the Codex profile is selected in local project policy. Product/platform rules below are local
extensions; the method does not choose them. Follow the existing work authority
and required gates. Recorded owner decisions persist across sessions.

FVWM-X is an X11 window manager derived from FVWM3. These are current repository
instructions; retired GSD plans and graph data are historical references.

## Project authority

- Track priorities, decisions, status and verification in
  `dev-docs/PROJECT-TRACKER.md`. Its execution order is authoritative.
  Current user direction is cleanup and measured optimization first; new feature
  pickup stays queued behind the finite handoff in
  `dev-docs/decisions/MODERNIZATION-ORDER-2026-10-25.md`;
  use `dev-docs/decisions/MAINTENANCE-FIRST-BATCHES-2026-10-23.md` for the
  cleanup/dependency queue and approved retirement boundaries.
- Use `dev-docs/PROJECT.md` for scope, `dev-docs/ARCHITECTURE.md` for source
  navigation, and `dev-docs/DEVELOPERS.md` for conventions and change contracts.
- `dev-docs/WORKFLOW.md` defines upkeep. GSD commands, phases, plugins and graph
  hooks are not required. Do not re-enable retired automation by reading archive
  configuration. Read only documents relevant to the task.

## Architecture and compatibility

- Target X11/XRandR 1.5+ on Linux, FreeBSD Xorg and OpenBSD Xenocara, AMD64/ARM64.
  No native Wayland backend or built-in compositor.
- Keep authoritative X/window state single-threaded in the core; preserve
  dispatch, configuration and command ordering. Do not introduce blocking work
  into the main loop or remove a required ordering/error barrier casually.
- Compatibility is limited to existing FVWM 3.x configuration files and their
  format: keep supported command/directive names, arguments, bindings, functions
  and theme configuration usable without requiring users to rewrite them.
  Stop for a compatibility decision only when a change breaks that configuration
  contract or requires a configuration migration. Internal APIs/ABIs, module
  protocols, exact legacy rendering, incidental timing and error/failure behavior
  do not require compatibility approval. Update shipped consumers and document
  and verify changed behavior. Other explicit product constraints still apply.
  See `dev-docs/decisions/CONFIG-COMPATIBILITY-SCOPE-2026-10-07.md` and
  `dev-docs/decisions/DEPENDENCY-SIMPLIFICATION-2026-10-23.md` for the approved
  Perl-specific and pre-FVWM3 retirement exceptions.
- Scale FVWM-owned UI and geometry, not arbitrary client pixels. X11/EWMH/ICCCM
  and module geometry remain actual pixels. Preserve scale/cache generations.
- Extend UI/services with existing isolated modules and base-system capabilities.
  No new mandatory GUI toolkit or extra desktop packages are implied by cleanup.
- Route managed-window actions through the shared command engine. Change native
  module producers/consumers and installed Perl metadata together; packet types
  are native ABI, not a portable network format.

## Ownership, limits and errors

- Validate external lengths/types and arithmetic before allocation/indexing.
  Bound queues, callbacks and service work. Preserve partial packets and
  per-producer continuations/order; a time budget cannot preempt one blocking call.
- Make ownership explicit across recursive commands, X window destruction,
  callbacks and module teardown. Use death-row cleanup and existing lifetime
  mechanisms; do not free records retained by pending work.
- Reuse existing X contexts, policy, transport, property and cache helpers.
  Prefer typed operation-local state and pure testable policy over new globals.
- Initialize output parameters; return explicit failure status and clean up once.
  Use project allocation/diagnostic helpers and expected asynchronous-X error
  policy. Avoid silent malformed-input failures and unconditional debug prints.

## Code conventions

- C11; generated config.h is first non-comment include in implementation files.
  Follow .clang-format/.editorconfig, tabs in C/headers, 80 columns, LF/final newline
  and local legacy declaration conventions. Avoid unrelated formatting churn.
- New C files/helpers use lower snake case; private helpers are static; preserve
  exported names/prefixes. Reserve initial F in libs filenames for wrappers.
  Use const, void parameter lists and domain types/results consistently.
- New state is explicit or static, not unprefixed global. Preserve local Xlib
  Bool/Window conventions and standard-C bool conventions in pure helpers.
- Four-space Meson indentation; gofmt for project Go; surrounding Perl/Python
  conventions. New Perl enables strict/warnings. Do not restyle vendored code.
- Preserve GPL notices in new C implementations/scripts, not headers. Comments
  explain invariants, ownership and compatibility. No cross-layer .c inclusion.

## Verification and shared-workspace workflow

- Follow `dev-docs/SUPPORT.md` and `dev-docs/BSD.md`. Pure policy needs native unit
  coverage; X-visible behavior needs nested-X Linux integration. Apply relevant
  platform coverage when affected APIs/ABI/build/server behavior or accumulated
  risk requires it, and full acceptance at milestone/release boundaries.
  Physical hardware/toolkits/compositors retain their own checks; emulated
  passes do not establish native-CPU qualification.
- Follow the tiered cadence in `dev-docs/WORKFLOW.md`: normal feature PRs use one
  incremental Linux profile, affected policy/X tests and scoped sanitizers when
  warranted. Check platform-sensitive changes early on the affected target.
  Implement larger related batches before one combined focused gate. Review
  coverage debt after roughly five merged batches; this is not an automatic
  matrix or a blocker on the next portable cleanup batch. Use affected BSD/
  architecture checks for changed contracts or concrete failures. Full compiler/
  feature/install/default-X qualification belongs at milestone/release boundaries
  unless the changed contract requires it earlier. Track deferred gates in the
  tracker; focused-checks-passed PRs may merge while
  integration is pending. Do not report deferred checks as passes.
- Default to one PR per coherent batch: usually six to ten related improvements
  where useful, or a complete reviewable owner slice. Keep related implementation,
  tests, docs, evidence and review fixes together. Reuse owner-controlled
  incremental builds per compiler/feature profile; do not rebuild or run a full
  matrix solely because a PR is created.
- Local macOS uses emulated Linux/BSD where appropriate. Native Linux ASan/UBSan
  execution is a separate qualification; emulated passes are not evidence of
  native execution. Run review, tests, lint and validation locally; do not add
  GitHub Actions, hosted/self-hosted runners or success-status publishers.
  Record failures and remaining gaps honestly.
- Keep work bounded to the owning tracker item and acceptance checkpoint. Preserve
  another thread's edits, builds, VM disks and evidence. Use isolated build dirs.
  Do not implement a broad rewrite, API rename or dependency replacement by inference.
- Native dependency/features/install targets are authoritative in meson.build and
  meson.options. ./build delegates to Python stdlib tooling. Follow INSTALL.md's
  package/backport requirements; a backport assertion is not a vulnerability check.
- Document user-visible features/options in doc/ AsciiDoc. Man pages are primary;
  HTML is optional and derived from the same source. Follow
  `dev-docs/DOCUMENTATION.md`; leave build/release default changes with their owner.
- New verification reports go under dev-docs/verification; proposals/decisions under
  dev-docs/decisions. Keep tracker/source scope and architecture links current.
- Maintain the brief README status when scope or milestones change. GitHub issues
  and Project fields mirror the authoritative tracker manually; use signed,
  author-DCO commits and PR review. See CONTRIBUTING.md and GOVERNANCE.md.
- Newly requested branches default to codex/. Release publishing, credentials and
  upstream notifications have separate ownership from build/test work.

Apply the Codex routing profile selected in local project policy. When useful,
delegate bounded inspection to `temper_workflow`, accepted implementation to
`temper_implementer`, difficult analysis to `temper_analyst`, and separate final
review to `temper_reviewer`. Supply the work item/comments, policy, relevant
files, acceptance, checks and edit boundaries. Follow local escalation, fallback,
resource and execution limits; the lead owns integration and verifies evidence.
