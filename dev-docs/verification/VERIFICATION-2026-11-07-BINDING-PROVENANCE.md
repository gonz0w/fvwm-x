# Binding provenance: draft checkpoint

Base: merged PR369, `c2d1ebecd3bdbc2d7d8808d6f0b0f4750116ef54`.
Owner: Cam; INSPECT-01 #218. Branch: `codex/r1-binding-provenance`.
This candidate is **not ready to merge**. Affected C compilation passes, but
runtime, Go, manual-render and new screenshot qualification remain pending.
The native campaign is stopped at its recorded attempt limit.

## Implemented scope

Four related stories retain current Key/PointerKey/Mouse definition origins,
cover menu/fallback bindings, expose replacement/selection facts in existing
paged queries, and display source/span/include details in the native inspector.
Bindings own immutable source references; a conservative 1MiB source-retention
budget preserves binding behavior when diagnostic storage is exhausted.

The additive version1 `provenance` object carries source, replacement flag,
absolute list position, selection order and `requires-event-context`. It does
not claim a universal winner or retain an override history. Function/generated
locations identify enclosing invocation context. The controller and inspector
share one strict source-schema decoder. Source details survive key filtering;
paths, actions and window patterns retain the existing redaction boundary.

No configuration syntax, native packet type, mandatory dependency or resident
service changes. #218 stays open for theme field/group, placement and modifier
origins and its broader acceptance. Async Colorset/preview ownership is excluded.

## Observed results

One existing native Linux ARM64 GCC profile is used; no installation or package
change occurs. Ninja and configured Meson version observations pass. The
125-step incremental C build passes in **9.043seconds**, compiling core/library,
MFL, FvwmScript, FvwmIconMan and three affected unit executables. Existing legacy
compiler diagnostics remain in the retained build log; this is not a clean-warning
claim. No unit executable has run in this checkpoint.

The first qualification stops after compilation at a browser target-metadata
guard. Source comparison establishes six already-admitted reload/palette Go files
missing from the old configured target, in addition to four new files. The
continuation verifies the applied source, exact post-build metadata, tool/SDK and
unaffected binary/library controls and preserves the passing build. It fails the expected list-length assertion before running any fresh command.
The later path-set and remaining target checks do not execute. Source analysis
identifies ten expected unique additions; this is not an observed guard pass.
Actual list multiplicities were not exported; no broader metadata equality or
runtime pass is inferred.

Separate source review identifies duplicate registration: the finder emits
absolute paths, while the explicit-list membership test uses relative names and
appends the same files again. Old metadata already contains ten duplicates. The
current source construction predicts twenty added entries for ten unique files;
the actual new count/list remains unobserved. This supports a task-runner count
defect without claiming that all subsequent target fields match.

The next bounded action belongs in the tracker. Do not
weaken the control, rerun compilation or renew the campaign by changing its name.
Retain the source and incremental build for a separately selected continuation.

## Preparation and stopping

A transient other-owner VM declines quiet-host admission before any owned start.
It later ends without our control. Performance observation is disabled and stays
due. Two preparation failures precede source mutation/tests: an absent previously
unbuilt Script output, then a guessed Meson path. The driver records absent prior
outputs and derives the actual regeneration tool from pinned Ninja metadata.
These preparation failures remain failures, not native test attempts or passes.

The actual qualification and its one diagnosed continuation are retained. All
executed command groups are absent; owned container and machine shutdown are
confirmed after both jobs. The final continuation verifies 2,518 current files
from the 2,513-file admission plus explicit overlays. Original source/hash and
new post-C output measurements remain distinct. No further native run is selected.

## Reused and pending evidence

[PR266 inspector](VERIFICATION-2026-10-26-NATIVE-INSPECTOR.md),
[PR268 diagnostics](VERIFICATION-2026-10-26-CONFIGURATION-DIAGNOSTICS.md),
[PR368 themes](VERIFICATION-2026-11-07-THEME-UI-CLOSEOUT.md) and
[PR369 lifecycle](VERIFICATION-2026-11-07-AUTHORING-TAIL.md) retain their original
source/profile scope. The new C/Go changes do not inherit their runtime passes.
Prior Slate, Dune and enlarged Classic presentation evidence is reused; there
is **no new UI screenshot** from this stopped checkpoint.

Pending: three C unit executions; four affected Go race packages; two configured
Go helper builds; two scoped leak-enabled ASan/UBSan unit builds/executions;
affected dynamic-library closure; candidate/base renders of two manuals with
host formatter review; and the focused native query/inspector fixture/capture.
The full OS/install/default-server matrix remains with the combined release
checkpoint. No timing, RSS, full-issue or release acceptance is claimed.

[Machine-readable receipts](VERIFICATION-2026-11-07-BINDING-PROVENANCE.json)
record source hashes, retained passing commands, stopped jobs, artifact hashes,
new output admission, cleanup and pending gates. Separate source and signed-head
review can clear **draft publication only**.
