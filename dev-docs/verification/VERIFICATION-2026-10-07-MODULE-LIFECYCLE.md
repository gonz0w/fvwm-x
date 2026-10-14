# Stacking and module lifecycle ownership checkpoint

Status: locally qualified from the immutable combined six-target checkpoint.
No configuration migration is needed. Later module iterator/packet changes have
a separate source and qualification checkpoint.

## Related changes

This checkpoint combines [stacking retirement continuation](VERIFICATION-2026-10-07-STACK-RETIREMENT.md),
[module output allocation admission](VERIFICATION-2026-10-07-MODULE-ALLOCATION.md),
and allocation-free producer retirement with fallible incoming command storage.

Retirement now detaches and relinks the already-owned producer list record into
death row. It does not allocate or free that list record until cleanup. Pinned
producer lifetime, iteration order, idempotent retirement and cleanup ownership
remain intact. If incoming command storage is unavailable, the receiver clears
its output and uses the existing error/retirement path. Previously queued input
keeps its producer pin and exact text until discarded; other producers continue.
Module creation/registration and unrelated allocations retain their existing
policy. These changes address the audited admission/retirement paths, not every
possible allocation failure in the process.

## Focused evidence

Prior-source unit controls reproduce exit status 1 during both retirement and
incoming command admission in normal/scoped sanitizer profiles. The repaired
unit verifies zero allocation attempts during repeated pinned retirement. Input
controls refuse storage with zero or one previously queued command, check clear
error output and unchanged reference/queue counts, then check cleanup and a new
producer's successful admission.

Each final normal/scoped Linux profile passes eleven pure and twenty actual-core
tests, including output/sync/backpressure, seven real-window retirement cases
and six populated/empty module storage cases. Pure controls enable leak detection;
core startup controls retain the existing leak-disabled scope. Native FreeBSD
and OpenBSD ARM64 overlays pass queue and runtime units plus all six actual-core
storage cases. The initial native overlay runner added a fourth compile argument
by mistake; both runner errors are preserved and the corrected invocation passes
with no production edit.

Frozen source: 1339 files, fifteen owned additions/changes from the locally
qualified 1335-file publication source; three further paths from the focused
allocation source. Archive SHA-256: `a03862f4d6873b8b74fedd6a349fb424d43d19f47927665378334201d85a431c`.
Source and acceptance evidence: `builddir/codex-2026-10-07/module-lifecycle-production`.
## Combined qualification

All eight compiler profiles pass: Linux ARM64/AMD64 GCC and Clang each pass 167
entries; FreeBSD/OpenBSD ARM64/AMD64 each pass 155. The total is 1288 accepted
standard entries. All 120 staged code identities match tested output after the
expected install RPATH transformation. Default BSD X servers pass 116 driver
invocations, including stacking retirement and populated/empty module storage.
Optional mixed cursor/software and SVG-disabled profiles pass 166 and 146
entries. Scoped Clang ASan/UBSan passes 45 checks: sixteen pure with leak checking
and twenty-nine startup/core checks with the existing leak-disabled scope.

Seven complete archives validate all 1339 source hashes/modes, registered test
identities, installed bytes and compressed EOF/CRC. No combined-gate source
change or affected rerun was required. Earlier checkpoints' failures and focused
runner corrections remain preserved in their own evidence; those results are
not presented as failures from this checkpoint. The existing OpenBSD AMD64
verification-profile-only finite TCG allowance remains documented; ordinary
suite deadlines are unchanged.

Native Linux release sanitizer CI and physical/toolkit/compositor evidence remain
unrun. Local QEMU/emulated passes do not establish those results.
