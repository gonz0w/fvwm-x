# Bounded file-operation jobs — 2026-10-27

Owner: Cam; FILE-JOBS-01 [#68](https://github.com/gonz0w/fvwm-x/issues/68).
Base: merged PR288, `ce1583a1b4e6c459efd404929c645c4b81567bc0`.
This substantial feature batch adds all eight operations and their isolated
worker/native browser consumer. It is focused PR evidence; #68 remains open
for missing required profile coverage. No release qualification is claimed.

[Contract and limits](../decisions/FILE-JOBS-2026-10-27.md),
[exact source, commands and receipts](VERIFICATION-2026-10-27-FILE-JOBS.json),
[three-theme gallery](../../doc/development-preview.md#bounded-file-operation-jobs).
Canonical raw archives remain in the owning local checkpoint
`builddir/codex-2026-10-27/r1-browser-batch`.

## Delivered behavior

- Create folder, rename, recursive copy, move, symbolic link, home trash,
  restore and explicit permanent delete through captured byte-preserving targets.
- One active mutation child, zero queued jobs, independent navigation, bounded
  planning/progress/decisions and ordered per-target outcomes. The owner retains
  the slot until actual reap; lost/invalid results never cause automatic replay.
- No overwrite: Skip, New name or Cancel resolve conflicts. Failed copies retain
  their source. Partial content, metadata and cleanup remain explicit. Directory
  and cross-filesystem moves are non-atomic; fast-move partial names may share an
  inode. Trash capability failure never becomes permanent deletion.
- F9 native editor, F1–F8 operations, separate F8 target preview/F10 confirmation,
  paged literal-name presentation, cancel, open trash and retained result pages.
  Dialog generations reject retired callbacks. Active jobs renew native leases
  and suspend ordinary idle exit; closing the owner cancels rather than detaches.

No new daemon, mandatory toolkit, database, bus, network listener or core X-state
thread is introduced. The [manual](../../doc/fvwm-file-browser.adoc#_file_jobs)
defines metadata, path, trash, race and recovery limits.

## Source and pre-qualification review

All product Go/C/build/manual inputs stayed frozen through qualification.
The Linux initial runtime aggregate is
`ce10fac969050485af8e0a095edbb89d3da13f06d2acc7e314c5b7fccb24e725`;
its native-run aggregate after a test-only remount correction is
`87616255c225f8c14631d04eee73251db8ad07faebbdba5c3645764d0a5997c1`.
The five-target archive is
`53ffbeb0e50598a5f15d3b62c609f8e7eed605224d23e2c6cadd66b8343535ba`.
Later FreeBSD/OpenBSD fixture corrections were uploaded outside those frozen
sources with their own before/after hashes; the JSON classifies these
test-helper and test-source deltas separately from production inputs and final
docs/gallery. The single-file Go overlay changes only the reviewed test;
source/member guards and the runtime browser remain unchanged.

Separate review found a destination-replacement bug before qualification: an
exclusive-created file could be replaced while its old handle was copied, then
metadata/source removal could proceed against the wrong published destination.
The regression failed against the preserved original engine. Created-entry and
ancestor receipts now validate publication, metadata completion and each source
removal; the corrected regression passes. Review also fixed reliable item-event
ordering behind coalesced progress, per-target cancellation results and relative
XDG trash-path policy. These fixes precede the frozen candidate.

Named Temper workflow, implementer, analyst and separate reviewer routes were
requested and accepted. Actual backend model/effort identity is not attested;
no role/model was reported unavailable. Agent review is a separate examination,
not independent human assurance.

## Combined affected checks

| Profile | Observed result |
| --- | --- |
| Linux ARM64 existing development container | Nine phases pass: SDK identity/gate, incremental configure/browser build, four Go packages, race for the same packages, floor SDK gate/backend tests and Asciidoctor manual generation. Go1.27.2; floor1.26.9. Ordinary/race each 203 test passes/5 explicit skips; floor 47/5. |
| Linux AMD64 QEMU | Browser build and four Go packages pass; actual EXDEV, destination replacement across filesystems, ENOSPC and EROFS pass. The mode-permission case passes separately as nobody after its root-suite skip. |
| FreeBSD15.1 ARM64 QEMU | Same affected browser/packages/four native filesystem cases and unprivileged permission pass after the test-only tmpfs option correction. |
| FreeBSD15.1 AMD64 QEMU | Same affected browser/packages/four native filesystem cases and unprivileged permission pass with the corrected fixture. |
| OpenBSD8.0 AMD64 QEMU | Four Go packages, four actual filesystem cases and unprivileged permission pass with the corrected MFS fixture. |
| OpenBSD8.0 ARM64 QEMU | Composed coverage: three original full-package passes and remaining browser cases, then the exact six-subcase launch-test overlay supplement; four actual filesystem cases and unprivileged permission pass. Original failing package exit remains retained. |

The generated manual also renders at 80 columns with macOS mandoc (exit 0).
Its lint exits 4: the Asciidoctor preamble uses unsupported `mso`/`LINKSTYLE`,
paragraph-after-heading warnings and generated source-line style diagnostics.
The file-jobs table and recovery text were inspected in rendered output.
This is not a clean mandoc-lint or installed-path/BSD manual claim; shared
formatter cleanup remains with DOCS-01.

The four packages are `internal/filejobs`, `internal/filemodel`,
`cmd/fvwmfilebrowser` and `transport`. The native opt-in cases are
`TestNativeCrossFilesystemMoveAndTrashRefusal`,
`TestNativeCrossFilesystemMoveDestinationReplacement`,
`TestNativeFullDestinationPreservesSource` and
`TestNativeReadOnlyFilesystemRefusal`; actual error behavior is required,
not a successful package that skipped those cases. The permission supplement
runs only `TestReadOnlyDestinationRefusal` under the existing unprivileged
account; no account/authentication policy is changed.

The Linux ARM64 container's two read-only fixture setup attempts failed before
these native filesystem assertions ran: frontend remount failed, then the
explicit remount syscall returned EACCES. All private mounts were cleaned;
container security settings were unchanged. This exact profile's actual
EXDEV/ENOSPC/EROFS fixture coverage remains unavailable. Other targets' results
do not fill that row. Host Go1.27.0 tests/cross-compiles are diagnostic only,
not the approved native compiler or filesystem qualification.

No full C/core/sanitizer/install/default-X matrix repeats for this pure-Go
consumer and filesystem checkpoint. Go race tests cover changed concurrency;
no C memory ownership changed. Wider native-CPU, physical hardware, toolkit,
assistive and release acceptance retains its existing owners.

## Actual-X and themes

The final driver is
`2c050b487a6d743630ecba123c2b88d79334bf2adeaa5f03349bfb9c81805bfa`.
Slate dark 1.25x, Dune light 1x and Classic enlarged 2x each pass 17 distinct real
jobs: 51 total, with 12 unmodified 2560×2160 captures. All captures were inspected;
three representative images are shipped with their config/source/binary hashes.
The Classic controls use explicit legacy colors over the Slate desktop.

Checks include all eight operations, recursive contents, literal/invalid-UTF8
names, symlinks, conflict Skip/New name/Cancel, no queued mutation, unrelated
browser navigation during a decision, permanent-delete target preview, retired
callback refusal, exact worker loss, dialog close and actual WM restart.
Ordinary browser/core/server waits are zero. The deliberately killed worker
reports interrupted/unknown effects without replay; cancellation and restart
have their own expected outcomes. They are not successful zero-exit mutations.

No live copy-progress state was sampled: these synthetic copies completed
between observations. Pure tests cover progress/unknown totals; the actual
paused conflict independently establishes decision/cancel/input responsiveness.
This limitation remains visible rather than claiming a captured progress view.

Two earlier native fixture failures are retained: a query exceeded the provider's
advertised page size; then CUT_BUFFER0 was overwritten before the Form consumed
a pasted field. Bounded generation-pinned paging and one combined field paste
with exact callback readback fixed those fixtures. No product edits followed.

## Setup failures, cost and cleanup

FreeBSD tmpfs rejected `nodev` before Go tests. Remove that unsupported option
only on FreeBSD, retain the failed capture, and run the pending affected stage.
OpenBSD's restored Meson cache contained only retired `cairo=enabled`; the
finite reviewed correction preserves all other options and immutable caches.
Its MFS mountpoint then exceeded the OS mount-name limit; a short private parent
fixed that setup. One ARM64 QEMU prepare/reboot failed with a Virtqueue message;
an unchanged isolated retry passed reboot. Generic MFS remount treated its
synthetic source as a filename; the reviewed OpenBSD-only fixture uses private
seed population followed by an initial read-only mount, with mount journals and
residue refusal. These are retained setup failures, not passing product tests.
The OpenBSD ARM64 ordinary package run also exposed an existing launch-test
publication race: shell redirection creates an empty report before printf writes
its bytes. The test treated any existing report as finished. Its reviewed test-only
correction waits for nonempty bytes within the unchanged three-second deadline,
preserves the exact once-only assertion and fails unexpected read errors.
An external single-file Go overlay preserves frozen source and production bytes;
the affected test receives its own scoped supplement rather than another full
package/platform matrix. The original failing JSON stream remains retained.

The [OpenBSD8.0 mount_mfs manual](https://man.openbsd.org/OpenBSD-8.0/mount_mfs.8)
documents population; [mount options](https://man.openbsd.org/OpenBSD-8.0/mount.8)
define read-only behavior. Exact guest results remain in the JSON.

The Linux ARM64 browser grows 5720923→6091518 bytes (+370595, about 6.5%).
Core and Form binaries are unchanged. This is feature-size accounting, not a
speed or memory improvement claim. PR288 remains the periodic measured reference
at 0/5; this merge advances 1/5, so no performance comparison is due.

The Linux UI archive verifies 330 files; its policy/manual archive verifies 20.
All archive members, hashes, sizes, modes and gzip CRC verify before this batch's
guest duplicates are retired. The owned Linux container and VM are stopped;
source/build/install/reference caches and other owners' resources are preserved.
Supported-target archive/lifecycle and duplicate-retirement receipts are recorded
individually. Eighteen finished owned disposable VM disks/copied image inputs
were retired after identity, canonical-copy and release checks, removing files
accounted for as 47242067968 allocated bytes (about 47.2 GB); canonical failures/successes and
immutable image/cache/SDK inputs remain. No persistent or other-owner disk is removed. No missing coverage closes #68 or changes its frozen criteria.
