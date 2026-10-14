# XDG discovery and preference policy — 2026-10-12

Owners: RUNTIME-GO-01 / UTIL-01, coordinated with XDG-01 / COMPAT-02.
Status: **four-story pure policy checkpoint locally qualified**
at frozen `9337`. The full provider/frontend port remains in progress.
These functions have no production caller or installed executable.

## Four-story scope

1. [Search directories](../../bin/FvwmPrompt/internal/xdgmenu/paths.go) resolve
   explicit HOME/XDG inputs without consulting process state. Empty values use
   the standard defaults, relative path entries are ignored, equivalent paths
   are deduplicated and user locations retain precedence. A custom configuration
   path does not depend on the presence of `/etc/xdg/menus`.
2. The same file resolves supplied menu inventories in search order. The first
   instance of each `.menu` basename wins; names within a directory sort
   deterministically. It retains names containing spaces and Unicode and does
   not sort or alter caller-owned slices. Acquisition must supply file names,
   regular-file checks and diagnostics; this policy performs no filesystem work.
3. [Application pools](../../bin/FvwmPrompt/internal/xdgmenu/pool.go) implement
   relative desktop-file IDs, recursive path flattening and last-AppDir
   precedence. Duplicate directory paths use their last occurrence. A hidden
   high-priority entry remains in the pool so later visibility filtering cannot
   reveal its lower-priority replacement. Within a directory, ambiguous flattened
   IDs use the first lexically sorted relative path; the specification does not
   prescribe an order for that collision. Parsed entries remain caller-owned.
4. [Preference data](../../bin/FvwmPrompt/internal/xdgmenu/preferences.go) reads
   and encodes the existing `*FvwmForm-XDGMenu-ConfigDefault:` record format,
   retaining record order, duplicate records and whole unquoted value tails.
   Unknown records remain available to the reader but do not affect provider
   defaults. Spaced/non-ASCII titles and terminal commands survive, `TitlesOff`
   is honored, toggles apply in file order and menu selections deduplicate with
   the final state. Existing names, built-in defaults and empty-field behavior
   remain usable. Icon sizes are parsed as representable integers; the codec
   does not impose a new rendering range.

Search policy follows the [XDG Base Directory 0.8 specification](https://specifications.freedesktop.org/basedir/latest/).
Pool precedence and IDs follow the
[Menu 1.1 file format](https://specifications.freedesktop.org/menu/latest/menu-file-format.html).
Application pools supplied to a child must still combine inherited and local
AppDirs in the resolver's correct order. Menu-file parent merging, include/
exclude allocation, moves, layouts, directory entries and icon lookup are not
implemented by these inventory functions. This is not whole-standard conformance.

## Ownership and limits

All inputs are snapshots supplied by the caller. The package performs no reads,
writes, process launches, environment mutation, X requests or command execution.
Menus/pools are new maps/slices; entry objects remain shared immutable inputs.
Invalid input returns an explicit error without exposing a partial result.

Inventories bound total supplied names/entries, total path bytes and emitted
path bytes; directory-prefix expansion cannot turn a small name list into
unbounded stored paths. Pool insertions charge output work even when replacing
an existing ID. Limits reuse the input foundation: 100000 items, 64 KiB values/
paths and 8 MiB source/input/output byte allowances. Path inventories validate
UTF-8/NULs and desktop relative components before flattening. These limits are
inactive external-data policy, not newly activated FVWM configuration limits.

Preference data retains literal quotes, dollar signs and backslashes without
evaluating them. Encoding refuses newlines/NULs, invalid keys and values which
cannot round-trip through the legacy single-line format, including leading or
trailing ASCII whitespace. Parsing trims legacy padding and CRLF while retaining
non-ASCII whitespace. Unrelated directives are ignored as data, never run.
Encoding is not a settings writer or authorization to feed arbitrary data back
through native `UseData`/core `Read`: the paired frontend still requires an owned
direct data path and private complete publication. Current shell save/export
actions are unchanged and are not qualified by this codec.

The existing Python provider and Perl frontend remain byte-identical to the
fixture checkpoint: provider SHA-256
`9c26c248cf6500270770df7c09642ab94dd89cabb9a89f99689a2bf681ff4062`,
frontend SHA-256
`b18c81f4f0ba9bc9a0cb310af7c5dbc4d0c5b6d64438264c62261a8c3828f588`.
No CLI option, Form field, user manual, Meson/install selection or runtime
configuration has changed. Existing `prompt-transport` discovers the new tests
through `go test ./...`; Go source enumeration discovers new files automatically.

## Frozen source and focused verification

Source archive SHA-256:
`9337dda3bb1b53f1d9662c7372917de2d8532d913233be999e4a8b3b2f8fc5e9`.
It contains 492 Go module files: six new implementation/test files and all
486 files unchanged from the qualified `d33f` input foundation, including
module/vendor metadata and every executable source. No dependency was added.
The package remains standard-library-only; Go's optional 1.21 floor is unchanged.

Evidence: `builddir/codex-2026-10-12/xdg-discovery/` contains the frozen archive,
scope, freeze/dispatch/validation scripts, local test JSON and native logs/proofs.
Executed guest runner SHA-256:
`00706f4979d5cfbf7e6ace1d9b6086d08f26dd0767909c0327e9109c6e7088f9`.
Private guest roots end in `fvwm-xdg-discovery-20261012`; predecessor source/
build trees and caches are retained. OpenBSD evidence lives under `/home`.

Local offline focused policy tests/vet and all eight Go packages pass. The
policy now contains 20 top-level tests and 230 named subtests: nine new test
functions and 109 additional named subtests. Meaningful regressions cover
custom/no-system XDG paths, ordered duplicate menus, hidden overrides, recursive
IDs, duplicate AppDirs, input immutability, aggregate path expansion, old saved
padding/comments/empty fields, whole-tail titles, ordered toggles, literal value
round-trips, numeric errors and source/line/item/value/output boundaries.

All six native targets pass the affected checkpoint. Strict validation checks
exact test/package sets, source/runner/proof/log composition, native architecture
and the stdlib-only dependency graph. Every current scoped file still matches
its frozen hash. There are 48 ordinary package outcomes and two Linux race
package outcomes. The 230 named subtests run on six ordinary and two race runs:
1840 subtest executions, not 1840 different cases; parent results are separate.
The nine new test functions contain 109 new named subtests. No test or dispatch
failure occurred in this batch. Retained archive-extraction future-filter
warnings are not test failures; member checks refuse traversal/non-regular files.

| Native target | Toolchain | Go package regressions | Policy top tests / named subtests | Vet / stdlib graph | Policy race |
| --- | --- | --- | --- | --- | --- |
| Linux ARM64 | 1.22.2 | 8 pass | 20 / 230 pass | pass | pass |
| Linux AMD64 | 1.27.0 | 8 pass | 20 / 230 pass | pass | pass |
| FreeBSD ARM64 | 1.27.0 | 8 pass | 20 / 230 pass | pass | not run |
| FreeBSD AMD64 | 1.27.0 | 8 pass | 20 / 230 pass | pass | not run |
| OpenBSD ARM64 | 1.27.0 | 8 pass | 20 / 230 pass | pass | not run |
| OpenBSD AMD64 | 1.27.0 | 8 pass | 20 / 230 pass | pass | not run |

A direct fresh Go 1.21 compiler check remains open; these are the actual executed
versions. Two inherited input microbenchmarks retain their allocation counts
and exact outputs in `validation.json`, as observations rather than discovery/
provider speed or footprint evidence. No end-to-end startup, memory, GC or
responsiveness improvement is claimed.

Documentation upkeep validates 794 local links, all owning whitespace checks
and the current frozen source hashes. `upkeep.json` records exact document
identities. Whole shared-checkout `git diff --check` still reports unrelated
generated Script scanner/parser whitespace, retained in
`unrelated-diff-check.txt`; those other-thread files were not changed here.

## Remaining full-port work

Ordered XML file acquisition/merging, inherited pools, include/exclude allocation,
OnlyUnallocated two-pass behavior, moves/layouts/localization/icon resolution,
legacy CLI selection/overrides and native menu output remain. A job-wide work/
cancellation/deadline owner must cover acquisition and the entire compilation;
bounded pure policies do not preempt one stalled filesystem call or callback.

Coordinate a direct Form settings/save/export path with the native consumer,
preserving existing fields and old saved preferences. Then qualify provider/
frontend actions, install/manifests/help/man pages, offline/default/minimal
profiles and real native X behavior before activation. The full provider is
unported; Python/Perl retirement and broader XDG interoperability remain open.
No C/core/X matrix was repeated for these inactive pure Go changes. Actual
provider CI, native C sanitizer release evidence and physical checks remain
separate. See the [tracker](../PROJECT-TRACKER.md#execution-order),
[input foundation](VERIFICATION-2026-10-12-XDG-POLICY.md) and
[full-provider fixtures](VERIFICATION-2026-10-12-XDG-FIXTURES.md).
