# XDG provider input policy foundation — 2026-10-12

Owners: RUNTIME-GO-01 / UTIL-01, coordinated with XDG-01 / COMPAT-02.
Status: **three-story pure policy checkpoint locally qualified** at `d33f`.
The full provider/frontend port is still in progress. This package has no
production caller, installed executable or configuration syntax change.

## Scope and ownership

The new standard-library-only
[xdgmenu package](../../bin/FvwmPrompt/internal/xdgmenu) provides:

1. [Desktop entry parsing](../../bin/FvwmPrompt/internal/xdgmenu/desktop.go):
   separate groups/action groups, bounded UTF-8 text, duplicate/key validation,
   string/list escape decoding, explicit boolean errors, locale fallback and
   menu visibility. TryExec availability is a caller-supplied predicate. The
   package neither searches PATH nor executes the application.
2. [Exec argument policy](../../bin/FvwmPrompt/internal/xdgmenu/exec.go):
   two escape passes, whole-argument quoting, exact empty/spaced argv, literal
   percent and metadata/file/URL field codes, one file-code constraint and
   deprecated-code removal. Replacements are inserted once as data. Undefined
   quoted field codes, unsupported codes and malformed source are refused.
   It returns literal argv and does not implement a shell, URI downloader,
   terminal process, startup notification or desktop action dispatcher.
3. [XML and rule policy](../../bin/FvwmPrompt/internal/xdgmenu/xml.go):
   ordered owned trees retain layout/merge attributes and extension elements.
   A conventional DOCTYPE is inert; decoding does not fetch DTDs or resolve
   declared external entities. Unknown entities, malformed roots, duplicate
   attributes, namespaces and excessive input are refused. Rule evaluation
   covers filename/category/all/and/or/not and Include/Exclude predicates.
   Ordered application of those predicates and cross-menu allocation belong
   to the next resolver story, not this decoder.

No process, file, environment or X state belongs to this package. Callers own
returned maps/trees/argv and must not mutate them concurrently. A TryExec
predicate can perform caller-owned work; the policy does not impose a deadline
on that callback. Merge-file acquisition, filesystem metadata/read stalls,
total compilation work and process deadlines remain integration requirements.
The existing Python provider and Perl frontend are still installed unchanged.

Internal limits are 8 MiB per supplied source, 64 KiB per desktop line/value or
XML value/attribute, 100000 physical desktop lines/XML nodes/argument entries,
128 XML/rule levels and 8 MiB total expanded argument bytes. Rule evaluation
shares a 100000-step allowance across node visits and category comparisons in
one evaluation. These are pure external-data boundaries; they have not become
new limits on shipped FVWM configuration files or CLI options. Later provider
adoption must document activated limits and preserve the configuration contract.

Desktop parsing walks lines without allocating a source-wide split array. XML
frames accumulate text in builders, avoiding repeated copies of growing values.
Malformed input returns explicit errors without exposing a partial parse/argv.
No regex substitutes for Exec tokenization, and no label/field is evaluated as
a command by these policies.

## Frozen source and validation

Source archive SHA-256:
`d33f5e72a6d69712b66afa82cb1d64d5bf0e61d6c1acb594788080dcc0542596`.

The archive freezes the Go module's 486 files, including six new implementation/
test files. All 480 predecessor files, including module/vendor metadata and
existing executable sources/tests, are byte-identical to the qualified Script
checkpoint `6fcc`. The new package has no dependency on other first-party or
vendored packages. Native `go list -deps -json` confirms that its own path is
the only non-standard-library dependency in each target's production graph.
The Go module floor remains 1.21; this checkpoint executes 1.22.2 on Linux ARM64
and 1.27.0 on the other guests, not a fresh direct 1.21 compiler check.

Evidence: `builddir/codex-2026-10-12/xdg-policy/` contains `source.tgz`,
`scope.json`, `freeze.py`, `local-test.jsonl`, `remote.py`, `dispatch.py`,
`validate.py`, `results.json`, `validation.json`, six `*-evidence.tgz` proofs
and retained initial dispatch logs. Exact executed runner SHA-256:
`e8c7d6206c0c31a93f1a8a9fedcd2f071dfe99f113824f66e012eaee5d22b03d`.
Each proof contains the frozen archive, scope, executed runner, command/status
records, native identity, unit JSON, vet/dependency/benchmark output and relevant
race output. Strict validation compares source/runner/log composition and the
exact test/package name sets; current scoped source hashes still match.

| Native target | Toolchain | Go package regressions | Policy top tests / named subtests | Vet / dependency graph | Policy race |
| --- | --- | --- | --- | --- | --- |
| Linux ARM64 | 1.22.2 | 8 pass | 11 / 121 pass | pass | pass |
| Linux AMD64 | 1.27.0 | 8 pass | 11 / 121 pass | pass | pass |
| FreeBSD ARM64 | 1.27.0 | 8 pass | 11 / 121 pass | pass | not run |
| FreeBSD AMD64 | 1.27.0 | 8 pass | 11 / 121 pass | pass | not run |
| OpenBSD ARM64 | 1.27.0 | 8 pass | 11 / 121 pass | pass | not run |
| OpenBSD AMD64 | 1.27.0 | 8 pass | 11 / 121 pass | pass | not run |

This is 48 ordinary package outcomes plus two Linux race package outcomes.
The 121 distinct named subtests execute on six ordinary and two race runs:
**968 subtest executions**, not 968 different cases. Top-level parent results
and their unnamed assertions are tracked separately. Whole-module tests include
the seven unchanged prior Go packages. Existing Meson `prompt-transport` runs
`go test ./...` and discovers this package automatically; no new test/build/
install option or competing runtime target was added.

Independent tests use
[Desktop Entry 1.5 Exec](https://specifications.freedesktop.org/desktop-entry/latest/exec-variables.html),
[value types](https://specifications.freedesktop.org/desktop-entry/latest/value-types.html),
[localized values](https://specifications.freedesktop.org/desktop-entry/latest/localized-keys.html)
and the [Menu 1.1 format](https://specifications.freedesktop.org/menu/latest/menu-file-format.html).
The legacy Python regex and a shell parser are not the Exec oracle. Tests cover
escape/list ambiguities, blank/spaced arguments, replacement-with-percent data,
localized non-ASCII values, exact desktop filters, malformed/oversized sources,
depth/node/value/argument/work boundaries, inert DOCTYPEs, refused entities,
rule cycles and retained extension/order metadata. These tests qualify adopted
input-policy functions, not the whole Desktop Entry/Menu standards.

## Measurements and failure history

Two small allocation/time benchmarks run on every native target. The fixed
desktop sample reports 696 bytes and six allocations per parse. Exec expansion
reports 768 bytes/twenty allocations on Go 1.27 and 792 bytes/twenty-one on
Go 1.22.2. ARM64 samples are roughly 0.8–1.6 microseconds; emulated AMD64 samples
are roughly 23–31 microseconds. Exact lines are in `validation.json` and each
native benchmark log. Different VM modes/toolchains prevent interpreting these
as an architecture comparison or a performance gain over Python. No full-session
startup, RSS/PSS, idle CPU, GC or input-latency claim is made.

The first local Exec vector over-escaped a literal quote at the general-string
layer. Its expected argv stayed unchanged; the raw fixture was corrected before
freezing, and local tests/vet passed. Review added non-ASCII whitespace retention,
explicit empty-boolean failure, replacement bounds and a category-comparison
work budget before the native checkpoint.

The first native dispatch omitted a space before the guest marker. Linux ARM64
ran and passed; the other five extracted source into separate `*20261012guest`
directories and stopped before testing. Initial driver, results and logs remain
under `initial-dispatch/`, and those guest directories were preserved. Only the
host dispatch spacing was fixed. The same source/guest runner then passed on
the five intended guest paths; the valid ARM64 proof was retained after exact
identity checks. Extraction's future-default-filter warnings are retained; the
archive member checks had already refused traversal and non-regular entries.

## Next integration checkpoint

Continue the full provider/frontend port with ordered menu discovery/merging,
pool precedence/allocation, layouts/localization/icon resolution, legacy CLI/
saved-default loading and native menu output. Use these policies rather than a
second Exec/parser implementation. Apply a job-wide work/deadline budget across
all entries; a per-rule allowance is not a complete compilation budget.

The paired Form frontend also needs a direct owned settings-data path: existing
save/export commands construct shell text from field values. Preserve existing
field/default names and support old saved preferences while qualifying private
complete writes, actual native Form operations and all emitted/install/manual
references together. No new GUI library or shell-evaluated data path is implied.
Its engineering implementation does not itself require compatibility approval.

The full provider remains unported and Perl retirement remains open. No C core,
module behavior, runtime install or X-visible behavior changed in this batch;
those extensive matrices were not repeated. Go-disabled profiles continue to
use the existing provider. Required future native-X, installed/default/minimal,
consumer/rendering and release coverage remain open. Actual provider CI and
native C sanitizer release evidence are separate from these local Go race runs.
See the [tracker](../PROJECT-TRACKER.md#execution-order) and
[preceding full-provider fixture checkpoint](VERIFICATION-2026-10-12-XDG-FIXTURES.md).
