# XDG Applications consumer — 2026-10-28

Four related #56 outcomes adopt existing shared policy in FvwmDesktop:
exact Desktop Entry Exec grammar, absolute XDG data roots, typed/localized
metadata and declared Desktop Actions. #67 benefits from reliable application
metadata; neither whole issue closes here. No dependency, service, worker schema,
configuration command or callback format changes.

## Candidate and observations

Base is merged PR292 `c9b5aa7b3f60ee6b383daf9212f0215636f008d0`.
The [receipt](VERIFICATION-2026-10-28-XDG-APPLICATIONS.json) records all six Go
file identities, the independent native fixture, binaries, captures and limits.
Source02 contains 1,855 selected inputs / 588 Go files, closure
`0a73d9f6bc8ae50bd5a47b016df9277ba2a72d1c11620844f0a7301c7e083ee3`.
Runtime and fixture hashes remain unchanged after qualification; final manual
and public gallery edits are documentation-only and separately checked.

The existing virtualized Linux ARM64 lab runs kernel 6.18.10-200.fc43.aarch64
and Go 1.27.2. Reuse of the retained incremental build avoids an all-module or
six-target rebuild. These are actual guest executions, not native-CPU sanitizer,
physical hardware, BSD or full release qualification.

| Check | Observed result |
| --- | --- |
| New regression tests on old production | Expected behavior failures in Exec, relative roots, localization/actions, typed refusals and action bounds; existing local argv caps pass. |
| Affected Go tests | 638 case/subtest passes across 4 packages; race repeat 638 / 4; retained qualified floor SDK 488 / 3. No skipped/failing cases in these final runs. |
| Go vet and incremental build | Worker, shared XDG policy, menu CLI and controller checks pass. Desktop/worker/menu targets build; no C source changed. |
| Existing independent menu CLI integration | `tests/test_xdg_desktop.py --provider …` passes; extracted locale helper preserves provider behavior. |
| Scoped install | Meson desktop-controller/desktop-menu tags pass; three Go executables match build hashes. The installed C capture helper has its separate hash and ELF dynamic-table receipt after installation adjusts RPATH. All native captures use that installed helper. |
| Installed actual-X consumer | Slate dark, Dune light and enlarged Classic pass all seven recorded case groups each, including actual worker policy and native launch effects. |
| Screenshots | 15 real captures inspected; three unmodified [public examples](../../doc/development-preview.md#localized-applications-actions). No obvious clipped labels/notices or missing French accent/em-dash glyphs in these examples. |
| Manual | Final AsciiDoc generates roff in Linux; macOS mandoc renders at 60/80 columns. Lint exits 3 on both base/final with the same four diagnostic families; long-line style reports 64→68, other counts unchanged. This is not clean lint or native BSD rendering. |

## Behavioral controls

Application discovery ignores relative environment roots, deduplicates absolute
roots, honors user precedence and keeps high-priority Hidden/invalid ID tombstones.
Explicit relative request roots remain literal. The existing bounded walker is
unchanged: 64 roots, 8,192 entries, depth 16 and 2,048 candidates/root, without following
directory symlinks. Results remain 512 records and the existing encoded-byte budget.
Exec uses the shared two-pass Desktop Entry parser, retaining 4,096 arguments / 64 KiB
expanded output locally. Independent expected argv includes empty arguments,
spaces, percent codes and shell-like characters as literal bytes; no marker runs.

Locale preferences match the existing provider, including LANGUAGE before the
selected non-C locale and C/POSIX suppression. Name/Icon/Path/TryExec, visibility
and Terminal use typed/decoded helpers. The first eight declared action IDs are
considered; malformed, duplicate, missing or unlisted actions never add a launch
record. A valid primary survives bad action data. Actions inherit parent cwd,
Terminal and application field-code context, with optional localized icon override.
Unit cases cover combined record/output limits and parent-context boundaries.

Three native configurations use real installed Desktop/worker processes, real
native menus, positive provider readiness and record-and-exit owned applications.
They prove exact argv/cwd, main/action launches, refresh/new generation, refusal
of the retired action, current-action success and a notice when an executable
is removed after menu commitment. Terminal=true is checked as worker metadata;
no external terminal is launched. Process-parent/start identities and reaping
are observed before core retirement; all fixture cleanup and observer X-error
lists are empty. Still images alone do not establish these behavioral results.

## Retained failures, review and cleanup

The first test fixture used T.Chdir, rejected by vet against the module's declared
Go version; it was corrected before the actual behavior RED. A source-receipt
directory name collided before the first focused command. Final Go checks pass
once in a fresh directory. The missing Linux mandoc tool stops only formatting;
existing host rendering completes that check without installing packages.
An initial install hash assumption incorrectly required the C helper's build and
installed images to be identical despite RPATH adjustment. Both hashes/ELF tables
are retained; the actual installed helper passes all captures. Podman cp returned
without retrieving tmpfs files; the existing command stream retrieved the archive
and manual, with explicit size/hash/member/mode/gzip-CRC verification.
No product failure or weakened behavior oracle is hidden by these setup repairs.

The canonical archive has 211 regular members, SHA256
`76c2dd5fd1c909a0345e229ad712ee5b72cf963203e22544a8fba7f5dcff14a6`.
Host evidence lives in the owning `builddir/codex-2026-10-28/r1-desktop-integration`
record. Guest duplicates retire only after verification; source/build/install
stage/cache/SDK/disks remain. The owned container and machine stop normally.
Separate source review accepts the runtime/fixture with no blocker. Final separate integrated review accepts staged tree
`3053a648145e2823e990b641838dd4971120ec58` with no actionable finding,
including archive integrity, actual results, manual and public captures. Repository
and staged whitespace checks pass. A final review-status metadata delta follows
that examined tree; runtime and fixture identities remain unchanged.
Roles are available; backend model/effort selection remains unattested.

## Remaining acceptance

MIME application roles, startup notification, wider installed-platform checks,
FileManager1 and broader #56/#67 release criteria remain open. Existing icon
resolver and prior component evidence remain accepted. No whole-issue closure,
all-OS acceptance, performance improvement, memory budget or accessibility claim
is inferred. PR292 advances the periodic reference to 4/5; the next runtime merge
reaches 5/5. #257/#258 stay parked.
