# Standalone XDG menu parity — 2026-10-28

This complete existing-consumer slice under #56 adds declared Desktop Actions,
localized application/action/directory icons and current-desktop visibility to
`fvwm-menu-desktop`. Shared pure metadata helpers remove the Desktop worker's
duplicate action planning. No new configuration option, dependency, service,
MIME engine, bus or startup-notification implementation is selected.

## Source and results

Base is merged PR293 `d6e49462baa00100ea90a0285429fd6724d304f5`.
The [receipt](VERIFICATION-2026-10-28-XDG-MENU-PARITY.json) identifies all eight
Go source/test files and both independent fixtures. Source02 closure
`1b85978f3418c62401e7cb030d52711d956fb3f71aa6965792c68fbfae4145be`
binds the focused Go/build/CLI checkpoint. Source03 closure
`30d9122f8ed7081e37386cc5eeaf658299fe565e786dd2ec54d53b008b272a2d`
binds installed native execution: only the final fixture differs, with all Go
inputs unchanged. It contains 1,860 selected inputs, including 592 Go files.
Final manual/gallery edits are documentation-only; the exact final manuals are
separately generated/rendered and hashed in the receipt.

The retained virtualized Linux ARM64 normal GCC lab uses kernel
6.18.10-200.fc43.aarch64 and Go 1.27.2. No C/core/native-module source changes.

| Check | Observed result |
| --- | --- |
| New cases against merged old code | Five actual Go regression failures and three independent CLI failures establish missing actions, grouping/inline limits, localized icons and current-desktop visibility. No compiler failure is treated as behavior RED. |
| Affected Go | 688 cases/subtests in five packages pass; race repeat 688/5; retained floor SDK 501/3. No final failures/skips. Vet passes. |
| Incremental build | Desktop, worker and menu CLI build using the retained compiler/feature cache. No complete WM/module rebuild. |
| Independent CLI | All 18 cases pass, including exact private catalog argv/cwd/Terminal, first-eight limit, current-desktop visibility, --desktop selection and old record replay. |
| Scoped install | Existing desktop-controller/desktop-menu Meson tags pass. Three installed Go executables match build hashes; installed capture helper has its separate RPATH-adjusted hash/ELF receipt. |
| Installed actual-X | Slate dark, Dune light and enlarged Classic pass seven recorded case groups each; all cleanup and observer-X-error lists are empty. |
| Screenshots | Nine actual captures inspected; three unmodified [public examples](../../doc/development-preview.md#localized-xdg-menu-actions). French accents/em dashes and synthetic colored fixture icons are readable, with no obvious clipping in these examples. |
| Manuals | Both final manuals generate roff in Linux; macOS mandoc renders base/final at 60/80 columns. Lint exits 3 with the same four existing families. Long-line style counts Desktop 68→67, standalone menu 9→13; other counts unchanged. No clean-lint/BSD-native claim. |

## Contract controls

Each desktop ID stays one allocation/layout unit. Its primary and declared-action
rows travel together at explicit Filename or Merge placement, preserving reservation,
parent-label sorting and duplicate-reference rules. Inline limits count actual
visible rows; a parent with actions cannot be aliased as a single-row submenu.
No synthetic desktop IDs or second menu framework are introduced.

Both existing consumers consider the first eight declared IDs. Invalid, duplicate,
missing and unlisted actions never replace a valid primary. Optional icons use
shared locale ordering and require a base Icon; actions fall back to the localized
parent. Field codes retain base application name/source context, while action
icons override %i. Terminal and cwd come from the parent. Each caller expands one
action before admitting the next; row/argument/byte budgets are charged and full
provider overflow refuses the whole publication. Unit controls exercise the
boundary where the primary fits but its action exceeds the count or byte budget.

The worker retains 512 rows, 4,096 arguments and 64 KiB of expanded argv per
launch, plus the 1 MiB aggregate result limit with conservative encoding accounting
and its existing cwd/executable checks. The standalone provider retains its larger global presentation/render/
catalog budgets, invalid-primary error semantics and launch-time executable lookup.
Its executable callback remains TryExec visibility policy, not an extra filter on
primary/action argv. Shared XDG_CURRENT_DESKTOP parsing honors colon lists and the
empty FVWM fallback. --desktop remains the existing menu filename/prefix selector.

Actual installed-provider execution produces private launch records and native
commands. Positive provider enrollment/theme queries precede native selection.
Main/action/current-regenerated/old-exported-reference launches produce exact
literal argv/cwd in owned record-and-exit programs; exit is observed before core
retirement. A shell-like marker never runs. Unlike controller snapshots, standalone
exported menu references intentionally remain valid after regeneration. Distinct
known PNG colors independently verify native localized app/action/directory icon
resolution. These synthetic icons are test data, not new shipped artwork.

## Review, failures and cleanup

The first CLI regression helper raised StopIteration when an action was absent;
it was replaced by an explicit one-row assertion, producing proper behavior RED.
A pre-gate source review found an added standalone action-existence check using
the TryExec callback. Freeze02 removed it and added a direct regression before
candidate execution. Freeze01 and the finding delta remain preserved; no passing
result is attributed to that unexecuted candidate. Final focused and native checks
pass without a product/fixture correction after their source freezes.

Separate final integrated review accepts staged tree
`43cd26452eb34518b5bfe20570388428542cc64f` with no remaining finding.
It verifies source/consumer contracts, actual evidence, archive integrity, manuals,
public captures, periodic observations and cleanup. Review corrected gallery
fragments and distinguished per-launch argv from aggregate result limits.
Repository and staged whitespace checks pass. The final review-status metadata
delta follows that examined tree; runtime, fixtures and captures are unchanged. Requested roles are available; backend model/effort remain unattested.
Agent review is not independent human assurance or merge authorization.

All 190 archive members/names/modes/bytes/hashes and gzip CRC are verified on the
host, archive SHA256
`b04dcc71d3a4648e7092e28934de1d2b2db269e41a7749270ce3db602dcc9404`.
Host records are in the owning `builddir/codex-2026-10-28/r1-xdg-next` checkpoint.
Verified guest duplicates are retired; source/build/stage/cache/SDK/VM disks and
measured performance binaries remain. The owned container/machine stop normally.

## Remaining work

This candidate completes the existing desktop-entry consumer parity needed for
#56's first acceptance row on merge; it does not close the issue. MIME/default-role,
startup notification, wider installed-platform/icon integration and broader
#56/#67 acceptance remain open. No six-target, native-CPU sanitizer, physical,
assistive-technology or full R1 release result is inferred from this focused gate.
#257/#258 remain parked.

The due [PR288→retained PR293 observation](PERFORMANCE-2026-10-28-XDG-MENUS.md)
is complete before this menu-parity implementation. Timing ranges overlap;
no speed gain or leak cause is established. It does not measure this new menu's
frontend latency. Counter resets 0/5 after PR293; this candidate becomes 1/5 on
merge. No extra benchmark build, correctness repeat or all-OS matrix was run.
