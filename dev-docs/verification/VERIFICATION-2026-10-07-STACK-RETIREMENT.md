# STACK-01: surviving-window continuation after callbacks

Status: implemented; focused normal/scoped sanitizer Linux and native BSD ARM64
checks pass. This 1337-file source is outside the
preceding frozen 1335-file combined qualification. No configuration migration
or compatibility decision is needed.

## Ownership repair

The actual AddWindow/destroy_window lifecycle control removes the invoked window
inside a stacking publication callback. The old normal build returns, but the
scoped sanitizer identifies a stale record access at parent preparation after
Raise's initial message. This is a bounded regression control on this project's
owned X display, not an exploit or an external-system test. It wraps the callback
boundary; existing synchronous module transport tests cover the unlock wait.

Raise/Lower capture the client and opaque record identity before their initial
message, then resolve authoritative managed membership before continuing.
Raise/Lower/Restack also resolve before post-operation focus work. Local stacking
and the new-window parent follow-up resolve again after callbacks before using
the record. A retired, unlisted or replacement record ends that record's work.
Existing X contexts and the listed-client construction fallback remain the only
identity authority; there is no extra registry, timer, round trip or library.

Parent continuation now captures each frame's client ID during preparation,
including the inline-to-allocated storage transition. Pop returns that captured
ID without reading the borrowed record. The caller resolves both parent and
child before checking transient state or continuing. Missing frames are skipped;
surviving children can still complete their work. Storage ownership, leaf-first
order, cycle/failure fallback and ordinary ancestry remain intact.

## Focused scope

The new pure controls preserve captured IDs after all live IDs/links change,
cover inline and allocated continuations, clear empty outputs and verify complete
teardown. The permanent actual-window fixture has seven cases: target retirement
at the initial Raise/Lower messages, target retirement during Restack and
transient Raise publication, parent/child continuation retirement, a retired
middle frame in a 70-window allocated path, and an ancestor removed before parent
preparation. Every case checks command completion, retired identity exclusion and
exact survivor count.

The final normal development profile passes eight pure and nineteen actual-core
tests, including module output/synchronization/backpressure and prior stacking
controls. Both native FreeBSD/OpenBSD ARM64 overlays pass the strengthened parent
unit and all seven actual-window cases. The private candidate's seven cases also
pass scoped ASan/UBSan after rebuilding the repaired source. The final permanent profile passes the same eight pure and nineteen actual-core
checks under scoped Clang ASan/UBSan (pure leaks enabled, core leaks disabled).
Complete focused/native/private archives validate payload hashes, modes and EOF.

Private setup initially omitted a context header and then used another X
connection during startup's server grab, causing a compile error and a timeout.
The corrected setup uses the already-owned display and explicit placement hints.
An initial candidate case waited for a restack packet on a path that intentionally
emits only Raise; the fixture now exercises actual transient or continuation
publication. Candidate failure artifacts and the baseline sanitizer evidence are
retained separately. No production change was needed for these setup corrections.

Exact focused source: `builddir/codex-2026-10-07/stack-retirement-production`, seven
owned additions/changes from the frozen preceding source. Private lifecycle
controls are in `builddir/codex-2026-10-07/stack-retirement-review`. Combined
six-target/compiler/install/default-X/optional qualification waits for the next
coherent batch; native Linux release CI and physical/toolkit/compositor results
remain separate.

## Combined acceptance update

This change is locally qualified in the exact 1339-file
[stacking/module lifecycle checkpoint](VERIFICATION-2026-10-07-MODULE-LIFECYCLE.md):
1288 standard entries/eight profiles, 120 staged identities, 116 default BSD X
driver invocations, optional 166/146 and 45 scoped sanitizer checks. Seven complete
archives validate source, modes, registered identities, installed bytes and EOF.
Later iterator/packet construction changes remain outside this checkpoint.
Native release CI and physical/toolkit/compositor checks remain unrun.
