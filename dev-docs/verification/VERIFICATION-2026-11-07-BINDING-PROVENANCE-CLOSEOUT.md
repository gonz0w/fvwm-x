# Binding provenance qualification closeout

PR370, INSPECT-01 #218. Base: signed draft
`f536ccfd0e86031d9c150eaa4291e37a383665f7`. Cam requested this bounded V2
continuation: repair the private metadata guard, reuse compilation, finish the
remaining checks and capture the new UI. No product source changed after that
draft. [V1's stopped results](VERIFICATION-2026-11-07-BINDING-PROVENANCE.md)
remain historical failures; they are not relabelled as passes.

## Result

The focused native Linux ARM64 checkpoint passes. **All 18 remaining commands
finish in 22.996seconds**, including admission and source verification. The prior
125-step C build, 9.043seconds, is reused without Ninja, reconfiguration, source
copying or another platform/install matrix. No packages or SDKs change.

| Check | Observed result |
| --- | --- |
| Binding/source, query JSON, Script binding-owner C units | Three executable passes |
| Shared source decoder, controller, inspector, worker | Four Go race package passes |
| Desktop and desktop worker | Two exact configured helper builds |
| Binding/source and query JSON scoped ASan/UBSan | Two leak-enabled unit passes; not whole-core instrumentation |
| Affected library closure | All resolved libraries belong to admitted or explicitly measured sanitizer runtimes |
| Desktop/MFL manuals | Four native candidate/base Asciidoctor renders; eight host 60/80-column renders |
| Native binding/inspector fixture | Three case groups pass; genuine queries, worker/Form projection, XTest actions and one capture |

The immutable origins survive nested Read, continued physical lines, keycode
expansion and file deletion after parsing. Global/menu sources and unavailable
builtins, enclosing function/generated contexts, replacement, generation rejection
and unbind are checked. Actual binding actions execute before/after replacement;
read-only inspection preserves binding/settings snapshots. Filtering, F2 paging
and Escape use the real native Form and worker path. No observer X errors or
cleanup failures occur. Pure units additionally cover retention accounting,
list movement, release and the explicit exhausted-budget result.

All 18 command groups retire. The owned container and machine are confirmed
stopped. Current source, metadata, SDK, tools and unrelated binary/library controls
verify again after qualification. The four C consumer outputs remain exactly the
post-build admission bytes; only the two selected Go outputs are rebuilt.

## Metadata guard repair

The actual exported browser target contains **752 entries / 732 unique inputs**.
Its old inventory contains732 / 722. Exactly ten new unique paths appear twice:
four new provenance files and six previously accepted reload/palette files.
The existing finder emits absolute names while explicit-list membership checks
use relative names; both registrations normalize to the same target path.

The corrected private observer requires the exact old multiset plus two copies
of each allowed path, preserves old relative order and checks every other target
field unchanged. It exports the full target inventory before assertions. Seven
bounded offline cases exercise valid, missing/extra-copy, unknown-input, order,
filename and compiler controls. Their first setup referenced a relocated plan;
using the preserved plan resolved that offline preparation failure. No native
attempt was consumed by those helper checks.

This repairs an incorrect observer expectation. It does not alter the project
build graph or infer that an arbitrary target change is acceptable. All three
retained version/build commands and the original source receipt remain separate
from this run's 18 fresh commands. Their elapsed time stays explicit.

## UI and manuals

The unmodified [Slate dark capture](../../doc/development-preview.md#binding-source-inspection-in-progress)
shows the `f12` filter, page3/6 and complete `user:leaf.config:2-3` definition and
`user:entry.config:2-2` include locations. Lead and separate product review inspect
it. This bounded inventory is visibly partial; current status prose wraps
awkwardly and its end is cut off. The new source rows and navigation controls
remain legible. The small black window is the synthetic focus probe. No image
has been retouched, and the capture is not assistive or hardware qualification.

Prior Slate, Dune and enlarged Classic presentation/lifecycle evidence from
[PR266](VERIFICATION-2026-10-26-NATIVE-INSPECTOR.md),
[PR268](VERIFICATION-2026-10-26-CONFIGURATION-DIAGNOSTICS.md),
[PR368](VERIFICATION-2026-11-07-THEME-UI-CLOSEOUT.md) and
[PR369](VERIFICATION-2026-11-07-AUTHORING-TAIL.md) retains its original scope;
those profiles are not rerun. This new source presentation has one fresh capture.

Both manuals render with native Asciidoctor and empty generator diagnostics.
Candidate and merged-c2d1ebec baseline host renders at60/80columns are inspected.
Mandoc lint is **not clean**: Desktop returns3 for baseline/candidate, MFL returns4
for both. Diagnostic families and counts are identical within each pair:
Desktop has one `mso`, one `LINKSTYLE`,120 long-input and19 paragraph diagnostics;
MFL has one each of `mso`/`LINKSTYLE`,274 long-input,130 unsupported-table and29
paragraph diagnostics. The first host wrapper wrongly allowed only lint0/3 and
stopped on the MFL baseline; corrected recording preserves actual codes and
stdout diagnostics. No new formatter family or count is introduced. Generator
cleanup and whole-manual parity remain with DOCS-01.

## Costs and limits

Same-profile executable file-byte deltas: core+7,808; MFL0; IconMan+1,920;
Desktop−1,564; worker+2,745. Script has no previous output, so its delta is
unavailable. These are debug ELF file sizes, not resident memory. Retained binding
source storage is conservatively bounded at1MiB per process; no speed or RSS
improvement is claimed. The due performance comparison remains due and unrun.

#218 remains open for theme field/group, placement and modifier origins and its
broader frozen acceptance. Source facts describe current definitions/list order,
not a universal event winner, arbitrary dynamic definition or override history.
Full OS/install/default-server, native hardware, accessibility and release gates
remain with their owners. This focused pass supports PR370's ready state only
after separate integrated/signed-head review and canonical delivery verification.

[Receipts](VERIFICATION-2026-11-07-BINDING-PROVENANCE-CLOSEOUT.json) include the
22 executed source hashes, 18 fresh and three reused commands, full raw-result
hash, actual metadata, artifact inventory, binary costs, manual diagnostics and
shutdown proof. Archive SHA256:
`78b6610237b22b237c845b37ad165d3b5aaf5d67b7b3d79f02839166c0b76056`.
