# Native live collection updates

Status: **focused Linux / early FreeBSD checks pass; ready for review**.
Owner: FILE-VIEW-01, shared file/clipboard metadata collection support.
Base: merged PR171 `fb8cc1a2c2cfdf85a83f12d1b3d513eb7c7e80b4`.
[Contract](../decisions/LIVE-COLLECTION-UPDATES-2026-10-23.md),
[source-bound receipt](VERIFICATION-2026-10-23-LIVE-COLLECTION-UPDATES.json).

## Delivered batch

- Opt-in native list/grid snapshot transactions: Begin/Entry/Commit/Abort stage
  up to 4096 entries separately and publish the whole result in the existing
  X child. Source ordinals map stable domain identities; duplicate/unknown/
  over-limit sources poison the candidate. Empty snapshots clear atomically.
- Current selection, focus, range anchor and viewport remap at commit, retaining
  edits made while staging. Removed focus uses surviving old neighbors; new
  entries start unselected. Other native input drafts/windows stay intact.
- Generation-prefixed callbacks and a native publication/status handshake.
  Aborted transaction identifiers never recycle; stale records cannot affect a
  newer candidate. Legacy callbacks stay byte-identical without ListVersioned.
  Pending/old label ownership and abort/exit cleanup are explicit.

The Form manual includes syntax, limits and a working native example. There is
no new module packet ABI, transport, dependency or existing configuration
migration. Ordinary labels/status are public metadata, never protected clipboard
payloads. SQLite, content protection and private delivery remain separate.
This supplies the native primitive: the current browser still rebuilds its form;
refresh/filter/sort and dynamic status/inspector adoption remain the next owner
work, alongside external providers, watching and mutation jobs.

## Focused verification

Both Linux ARM64 GCC and FreeBSD ARM64 Clang pass **seven selected registered
entries**: list/input/command policy plus four existing Form actual-X regressions.
Linux adds **three scoped ASan/UBSan policy/parser passes with leak detection**.
Five composed browser/native-update drivers pass: Slate dark, Dune light,
classic 9x15, a separate instrumented Form run, and FreeBSD native-X. Each
captures **20 screenshots**, including five new staging/commit/removal/empty
views. All three theme commit views are inspected for fit, contrast and retained
input. Exact theme config/font/scale/server inputs are captured in each driver.

The native fixture verifies unchanged X window/child identities and draft bytes,
unchanged callbacks before commit, new generation and remapped selection after
commit, user selection changes during staging, duplicate atomic refusal,
abort/empty/remove, stale traffic and a pending allocation at teardown. Policy
also covers 4096 entries, over-limit atomic refusal, grid viewport alignment,
removed-focus neighbors and generation exhaustion. Existing browser list/grid,
filter/range, stale-action, raw filename, refresh and worker-cancel checks remain
in the same runs. The unchanged Go sources/helper retain PR171 qualification;
no Go policy/race/build or full install matrix is repeated for this C-only work.
Linux core/MFL binaries keep their exact qualified identities.

Initial checks pass. Review then adds unique list-name resolution and a native
high-water mark so an aborted generation cannot be reused; all affected native
checks, themes and scoped sanitizers pass again. All four capture archives are
preserved and verify gzip EOF/CRC, exact file inventories, every hash/mode and
source guards: initial Linux/FreeBSD **138/40 files**, final **138/40 files**.
Final frozen source: **2271 files**, archive
`ad11394e81af2e4de410127ab677371f736327d131500104aedc0a996dfeb18d`.
The private freeze helper's inherited base label is corrected in the receipt
using its captured HEAD/tree; guarded file inputs are unchanged. Final post-freeze amendments correct one ownership comment and normalize new
private handler names to the C convention. Exact replacements and source hashes
are recorded. Affected Linux/default, Linux/instrumented and FreeBSD builds
pass; every allocated runtime section except the build-ID note is identical
to its tested executable. These exact runtime identities retain UI/policy
qualification without redundant reruns.
The updated Form manual builds and renders with host mandoc. Repository/link
and whitespace checks pass. Portable maintenance results from PR170 remain the
cycle reference, not a fresh native-feature result.

Canonical evidence/screenshots:
`/Users/cam/DEV/fvwm3/builddir/codex-2026-10-23/live-collection-updates`.
Completed guest capture/archive duplicates retire only after complete host proof;
sources, build trees, SDK/caches, helper binaries and VM disks remain preserved.
This batch retires 280 Linux and 84 FreeBSD duplicate files; the owned FreeBSD
VM is shut down.

## Remaining gates

PR169 remains the integrated/performance reference. PR170 and PR171 are merged:
**2/3 platform-cycle and 2/5 performance batches**. This becomes **3/3 and 3/5
on merge**. Close the accumulated affected six-target cycle before a fourth
implementation batch. No periodic performance comparison is due; no speedup is
claimed. Full install/compiler-feature/default-X and owner/milestone/release
coverage remain at their required boundary. All local Linux/BSD runs are
emulated; native-CPU/hardware/toolkit/compositor/assistive coverage is distinct.
Instrumented UI leak detection remains off for the inherited font-cache exit
gap; pure/parser sanitizer checks enable it. No browser or clipboard owner is
closed by this supporting checkpoint.
