# Developing FVWM-X

Use the [project tracker](PROJECT-TRACKER.md) for the next bounded change and
current evidence. [PROJECT.md](PROJECT.md) records scope;
[ARCHITECTURE.md](ARCHITECTURE.md) identifies subsystem owners. Archived
upstream branch/release/parser instructions are history, not current policy.

## Change contract

Compatibility is limited to existing FVWM configuration files and their format.
Keep supported names, arguments, bindings, functions and theme directives usable
without requiring a configuration rewrite. Obtain a user decision only when a
change breaks that contract or requires a configuration migration. Internal
APIs/ABIs, module protocols, exact legacy rendering, incidental timing and error
or allocation-failure handling do not need separate compatibility approval.
Update shipped consumers, document changed behavior and run affected checks.
This does not waive other explicit product, safety or verification requirements.
See the [clarified scope](decisions/CONFIG-COMPATIBILITY-SCOPE-2026-10-07.md).
Keep authoritative X/window state single-threaded and preserve event ordering.
Bound external data, queues, callbacks and service work before use/allocation.
Do not add a compositor, Wayland backend or mandatory desktop packages.

The user-approved [runtime language migration](decisions/RUNTIME-GO-LUA-2026-10-06.md)
selects Go for first-party Python/Perl runtime helper replacements and optional
Go-hosted Lua for primitive policies/workflows. C retains native core/UI/resource
ownership. Explicit retirement of FvwmPerl/perllib/Perl-specific APIs is the
approved configuration exception; preserve unrelated configured FVWM/FvwmScript
behavior and document exact migration impacts. Qualify ports before default/install
switches and keep current hardening first. Python build/test tooling remains
until its separate checkpoint; no interpreter has been removed by scoping.

Follow [STANDARDS.md](STANDARDS.md) for adopted protocol/UI requirements. Record
the specification section/revision, advertised behavior, owner and independent
acceptance evidence for affected changes. Preserve required clauses and truthful
capability reporting. [THEME-STANDARDS.md](THEME-STANDARDS.md) applies to generated
assets and default native UI; web exports have their own HTML/accessibility gate.
An existing atom, dependency or palette ratio is not complete conformance proof.

Read the affected source and tests before choosing an abstraction. Reuse local
X contexts, existing policy/transport helpers and cache generations. Keep
ownership explicit across callbacks, recursive commands, X window destruction
and module death-row cleanup. A count/time budget cannot preempt a blocking
handler; identify that limitation when changing scheduling.

## Coding conventions

- C is C11. Include generated `config.h` as the first non-comment line of each
  implementation file. Use source-root-relative quoted headers where appropriate.
- Follow `.clang-format` and `.editorconfig`: tabs in C/headers, 80-column
  limit, opening braces on their own lines, LF and a final newline. Match
  surrounding legacy code; avoid unrelated formatting churn.
- New C helpers/files use descriptive lower snake case; file-private helpers
  are `static`. Preserve compatibility-facing exported names and subsystem
  prefixes. Reserve `F` filenames in `libs/` for wrappers/FVWM abstractions.
- Use `void` for no-argument functions, `const` for read-only inputs, domain
  result types and initialized output parameters. Keep state explicit rather
  than introducing unprefixed globals. Preserve local Xlib Bool/Window conventions
  and standard-C bool conventions in pure helpers.
- Check arithmetic before allocation and indexing. Use project allocation and
  diagnostic helpers where appropriate; clean up owned resources exactly once.
  Recoverable errors return explicit status; do not silently swallow malformed
  external input or add unconditional debug prints.
- Follow four-space Meson indentation, gofmt for project Go, and established
  Perl/Python style. New Perl uses strict/warnings. Treat vendored dependencies
  as third-party source; preserve their licenses and formatting.
- Preserve the GPL notice in new C implementations and scripts, not headers.
  Comments explain invariants, ownership and non-obvious policy.
- Define module packet changes with all producers/consumers together, including
  installed Perl metadata. Never infer native packet layout is a portable
  network format or bypass the shared command engine.

## Build and verification

Follow [INSTALL.md](../INSTALL.md) for dependencies and supported options.
The compatible `./build` entry point delegates to Python stdlib tooling;
direct Meson use remains supported. Use an isolated build directory, especially
while another thread owns a runtime batch. Do not wipe its build or VM state.

```sh
./build -b builddir-local --test
./build -b builddir-man -m --test
```

These commands use the documented wrapper; they do not install unless `-i`
is supplied. PNG-enabled builds must follow the audited dependency/backport
policy in INSTALL.md; an option is not proof of a vendor patch.

Pure policy needs native unit tests; X-visible changes need nested-X integration
on native Linux and the relevant [six-target matrix](SUPPORT.md). Local macOS
uses emulated Linux/BSD verification where appropriate. Native Linux ASan/UBSan
CI remains separate; configured workflows or emulated passes are not that result.
Use [BSD.md](BSD.md) for the BSD harness. Run appropriate tests once, then broaden
when failures, new changes or unresolved concerns justify it.
Use the [batch cadence](WORKFLOW.md#implementation-batches-and-test-cadence):
focused checks per related story, affected-platform checks when needed, then one
extensive qualification pass for the combined checkpoint. A story may be
implemented and locally checked while full matrix qualification remains pending.

Record source/profile scope, actual checks, retained failures and remaining gaps
in a dated [verification report](verification/README.md). Do not mark audit,
candidate tooling, native CI, physical hardware or third-party themes complete
without the corresponding evidence.

## Documentation and handoff

Document every user-visible feature and option in the relevant AsciiDoc manual
under `doc/`. [DOCUMENTATION.md](DOCUMENTATION.md) defines man-page quality and
optional HTML output. Update the architecture map only when its ownership or
entry points change; add a decision record for an accepted cross-cutting contract.

Use `codex/` for newly requested development branches unless the user specifies
otherwise. Keep commits/review changes bounded and descriptions focused on the
problem, final behavior and validation. Publishing/release destinations and
credentials are distinct from build/test work; inherited upstream workflows
do not establish FVWM-X publication ownership.

The [workflow](WORKFLOW.md) replaces GSD phase automation and the stale graph.
It does not require a plugin, generated graph, or reading every document before
a small edit.

## Local review and GitHub hosting

GitHub hosts `gonz0w/fvwm-x`, its roadmap issues and PR review. All checks run
locally; Actions and remote status publishers are disabled. Follow
[CONTRIBUTING.md](../CONTRIBUTING.md) for the quick foundation check and affected
native gates. `make verify` is not full runtime/platform acceptance. Keep the
brief README status current and preserve failed/skipped evidence. The tracker
remains authoritative for scope and execution; mirror issue/Project state
manually. See the [hosting decision](decisions/LOCAL-REVIEW-GITHUB-2026-10-13.md).
