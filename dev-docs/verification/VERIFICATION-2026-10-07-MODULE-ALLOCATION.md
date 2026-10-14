# Module output: recoverable storage admission

Status: implemented; focused normal/scoped sanitizer Linux controls pass.
Native FreeBSD/OpenBSD ARM64 queue units and all six actual-core cases also pass. Combined six-target, install, default-X
and optional qualification remains pending. No configuration migration is needed.

## Change

Output packet copies, queue records and deferred configure snapshots now use
fallible admission. If the requested record cannot be stored, the newly reserved
bytes are released, any unowned packet is freed, and only the affected producer
is retired through existing death-row cleanup. Previously owned output and
configure snapshots stay retained while callbacks pin the producer. The core
continues serving other producers. Ordinary packet order and synchronous
completion remain unchanged.

The shared queue has a separate fallible append API. Its existing allocation
policy remains unchanged for other consumers. Both append paths use one linking
helper, including visibility rules while the queue is locked.

## Evidence

A bounded actual-core baseline refused one allocation at each of the three
admission sites. All three normal and all three scoped sanitizer runs exited
with status 1. The repaired private controls return status 0; no sanitizer error
is reported. These are local allocation-failure regressions, not external-system
tests. They demonstrate these admission paths, not recovery from every process
allocation failure.

The permanent unit controls cover empty/nonempty failure, FIFO retry, caller
ownership and locked append visibility. Six actual-core cases cover each of the
three admission sites with empty and populated output/configure owners. They
verify reservation rollback, pinned retention, eventual descriptor closure,
unaffected producer output and successful subsequent admission.

Final focused normal and Clang ASan/UBSan profiles each pass eleven pure and
twenty actual-core tests. Pure controls use leak detection; startup/core controls
retain the existing leak-disabled scope. Existing output budgets, synchronization,
backpressure and stacking/retirement regressions are included.

The isolated native unit link initially omitted the new queue object. Both linker
errors are preserved; adding that object to the overlay link fixes the harness,
with no production change.

Exact focused source: 1339 files, eight owned additions/changes from the separately
frozen 1337-file stacking retirement source. Archive SHA-256: `ebcf73a6436d212ad2d69114840370a4a38d61f00307e501341784a2d0e255d4`.
Source/evidence directory: `builddir/codex-2026-10-07/module-allocation-production`.
Baseline/candidate controls: `builddir/codex-2026-10-07/module-allocation-review`.
Native Linux release CI and physical/toolkit/compositor checks remain separate.

## Combined acceptance update

This change is locally qualified in the exact 1339-file
[stacking/module lifecycle checkpoint](VERIFICATION-2026-10-07-MODULE-LIFECYCLE.md):
1288 standard entries/eight profiles, 120 staged identities, 116 default BSD X
driver invocations, optional 166/146 and 45 scoped sanitizer checks. Seven complete
archives validate source, modes, registered identities, installed bytes and EOF.
Later iterator/packet construction changes remain outside this checkpoint.
Native release CI and physical/toolkit/compositor checks remain unrun.
