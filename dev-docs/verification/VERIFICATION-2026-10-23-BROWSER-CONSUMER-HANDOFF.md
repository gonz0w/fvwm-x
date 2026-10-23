# Portable browser consumer handoff

Status: **focused checks passed**; [PR #186](https://github.com/gonz0w/fvwm-x/pull/186) is open for review. The accumulated
[PR176–178 platform cycle](VERIFICATION-2026-10-23-BROWSER-ACTIONS-CYCLE.md)
is closed before implementation. PR178
`e2f61e65b0e5277dedcf28fe6c602efa8b2d1762` remains the integrated reference,
0/3 merged and 1/3 on this feature merge. [Source-bound receipt](VERIFICATION-2026-10-23-BROWSER-CONSUMER-HANDOFF.json).

Scope note: this report records the original feature candidate. The subsequent
[combined provider cleanup](VERIFICATION-2026-10-23-BROWSER-PROVIDER-CLEANUP.md)
changes ten Go files and qualifies the final browser on Linux. Earlier native,
BSD, three-theme, install and sanitizer receipts keep their exact original
source/binary scope. The [new cadence](../decisions/BATCHED-CLEANUP-CADENCE-2026-10-23.md)
supersedes the three-PR trigger and counters below; PR186 is the first pending
batch since integrated PR178.

## Behavior and scope

Four related capabilities support portable file-manager consumers:

- A shared bounded correlated request client preserves FvwmCommand response
  formatting, with cancellation and close/join ownership on every exit.
- Opt-in `fvwm-file-browser --reuse` hands folder refresh, reveal or observed
  properties to an idle matching browser. No match/busy/older capability or
  larger-than-wire target sets retain the independent launch path. An ambiguous
  submitted outcome returns an error without launching a duplicate.
- A generation-checked `present` action publishes native state and uses the
  shared core command engine to deiconify, focus and raise the owned window.
  No filename becomes command syntax. Admission and worker completion remain
  separate; presentation failure cannot retract admitted work.
- Go-enabled builds install a development folder desktop entry with a literal
  local URL/positional-folder CLI route. `--` separates the URL from options.
  Installing the entry changes no default association.

The [contract](../decisions/BROWSER-CONSUMER-HANDOFF-2026-10-23.md) and
[manual](../../doc/fvwm-file-browser.adoc) document bounds, fallback, refusal
and explicit-customization policy. Ordinary metadata/control transport is not a
protected clipboard channel. File jobs/mutation, default-role/FileManager1,
private memory/encrypted persistence/SQLite and broader consumer acceptance
stay open under their existing owners.

## Focused verification

| Gate | Result |
| --- | --- |
| Linux ARM64 GCC / FreeBSD ARM64 MFL policy and X entries | 5 each, 10 total |
| Five Go packages on Linux / FreeBSD | 84 / 83 top cases, vet/build pass |
| Composed Linux race | 84 top cases, 5 package outcomes |
| Browser actual-X | 3 Linux themes and 1 FreeBSD run pass |
| Scoped leak-enabled Linux guest ASan/UBSan | 2 policy entries and 1 MFL lifetime X exercise pass |
| Optional Go staged install | Exact tested browser and desktop entry; no defaults modified |
| Manuals | FvwmMFL, FvwmCommand and browser render |

The five packages are collection, filemodel, browser, command client and
transport: **251 top-case executions / 15 package outcomes** including race.
Cancellation tests prove peer EOF and reader joining; fake servers cover matching,
busy, old capability, stale retries, lost replies and presentation failures.
Actual-X deiconifies the existing browser, hands off raw-byte properties and a
folder URI, checks one provider/unchanged X children and exact selection, and
asserts no item execution. Existing target/work/list behavior runs in each driver.
Each accepted driver captures **41 PNGs**. Qualified core/Form identities are
retained; the affected MFL and Go consumers build incrementally.

The first Linux race browser link fails with `no space left on device`; four
race packages already pass. After canonical-verified completed capture retirement,
only the browser race and previously unrun vet/build/theme/sanitizer/manual/stage
commands run. They pass. This is a storage/link failure, not a runtime race
failure; both original and completion archives remain canonical. No product,
policy or UI fixture change follows the 2354-file freeze. A FvwmCommand manual
paragraph is moved from AUTHOR into request behavior; only that manual rerenders,
with exact before/after hashes recorded. The other manuals match the freeze.

Staged field-code policy and actual URI/CLI tests pass. `desktop-file-validate`
is unavailable in the guest; it is not reported as passing. The optional-Go
profile installs exactly the intended two tagged files. Full Go-disabled,
compiler/feature/install/default-X and toolkit/default-role acceptance remains
at its shared/milestone boundary. Guest sanitizer execution is not a new
native-CPU Linux qualification.

## Captures and preservation

![Slate existing-browser properties](../../doc/screenshots/handoff-slate-dark.png)

![Dune existing-browser folder refresh](../../doc/screenshots/handoff-dune-light.png)

![Classic existing-browser properties](../../doc/screenshots/handoff-classic-large.png)

The [gallery](../../doc/development-preview.md#portable-consumer-handoff) and
[image manifest](../../doc/screenshots/handoff-manifest.json) preserve unmodified
Slate dark, Dune light and classic 9x15-font captures at 1440×1000, scale 1.
Properties, selection, controls and status are inspected in all three. Long
escaped labels stay clipped to the list viewport; broader scale/accessibility
acceptance remains separate.

Canonical host archives pass full gzip EOF/CRC, file/hash/mode and source identity
checks. Source archive SHA256:
`88afcac8aaf70e16fdb5a1c254b7687fbb0fe6cacd422f5ea908e9a79bc6954d`.
Original Linux, FreeBSD and completion archive identities are in the receipt.
The owned FreeBSD guest shuts down normally; no cleanup-only boot is made.
On Linux, 1251 older completed duplicate files (239,908,719 bytes) are retired
against individually verified host archives to clear the link failure. This
batch then retires 191 verified capture duplicates (17,734,523 bytes), retaining
all staged installs, source, builds, SDK caches, VM disks and unrelated evidence.
Canonical host data and preserved performance reference binaries remain.

The [due performance observation](PERFORMANCE-2026-10-23-BROWSER-HANDOFF.md)
uses existing qualified binaries without a benchmark build. It resets the
independent comparison reference on this merge; the new feature platform cycle
remains 1/3 on merge. No broad suite repeats for PR publication.
