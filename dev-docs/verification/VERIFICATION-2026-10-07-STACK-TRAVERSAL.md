# STACK-01 / DEAD-01: transient traversal and inactive functions

Status: the three-story checkpoint is locally qualified from the exact 1314-file
source. Eight standard compiler profiles pass 1192 entries; 120 installed code
identities match; all four BSD default servers pass 84 driver invocations.
Optional profiles pass 154/134 entries; scoped sanitizers pass 33 checks
(twelve pure leak-enabled, twenty-one core leak-disabled). This source extends
the qualified 1306-file arithmetic checkpoint through sixteen owned paths;
other chats are preserved. Native release CI and physical coverage remain open.

## Changes and ownership

Stable ring sorting is described in the [sort report](VERIFICATION-2026-10-07-STACK-SORT.md).
The parent-only closure keeps its first two ordered, allocation-free scans. If
more scans are needed, it builds a typed, operation-local parent/child worklist
from existing cached parent pointers. One parent bucket is queued once; each
remaining candidate is processed once. Existing marked seeds and retained rank
values remain authoritative. Unique parents make final marks/ranks independent
of worklist traversal order; parent style and icon exclusion use the shared
first-mark policy. The helper does no X work or callbacks, changes no links and
retains no records after return. Checked allocation failure occurs before
mutation and resumes the original scan with a diagnostic. Window-group closure
retains its original ordered scan because first-mark order can affect ranks.

The 2048-record reverse-chain actual-core control previously used 4,194,304
membership tests over 2048 passes. The candidate uses two scans/4096 tests plus
2045 remaining-edge visits and 33,727 parent sorting/search comparisons. Forward
and star controls keep their two scans and use no worklist. All ranks and marks
match. These are local operation counts, not end-to-end desktop latency claims;
private measurement counters do not appear in production headers/APIs.

Seven inactive functions were removed: the six listed in DEAD-01 plus the
stacking subsystem's unused ResyncFvwmStackRing declaration/body. All were
unconditionally inside #if 0. Reference review found no source/header/manual/
generated/dispatch dependency. GCC preprocessing before/after gives identical
active bytes in all six affected source files, with only source line diagnostics
normalized. 234 inactive/comment lines were removed. The template and asprintf
reviews remain separate; no active fallback/platform code was deleted.

## Focused evidence

Frozen source SHA-256 `4b4c0e8148072123c7ef42d62343a1a2a15bd12d5a77949f0e203d39ee25cafd`;
`builddir/codex-2026-10-07/stack-traversal/source-stack-traversal.tgz`.
Nine normal checks pass: subtree and sort policies; actual-core subtree, rank and
redirection; real-X stack-layer, raise-unmanaged, parent-layer and layer-boundary.
Scoped Clang ASan/UBSan passes two leak-enabled pure checks and seven leak-disabled
actual-core/real-X checks. Full affected targets compile with GCC and Clang.

The subtree unit covers 320 combinations (5 shapes × 4 modes × 2 icon policies ×
8 sizes), plus allocation refusal/retry. The independent ordered-scan oracle
checks membership, ranks and untouched links for forward/reverse chains, branches,
cycles/preexisting seeds, icons/style gates and signed endpoints. Earlier commentary
and native runner labels incorrectly stated 640; those original logs are retained.
The actual source loops and this report establish the corrected 320 scope.

The permanent actual-core fixture checks real Xlib identity/cache boundaries,
forward/star/reverse ranks, retained layer seeds, unchanged window-group ranks,
and forced worklist allocation failure through the original scan fallback.
Both FreeBSD/OpenBSD ARM64 pass exact-source pure policy checks against their
native compiler/header environment; the final full native core/default-server gates now also pass in the combined
checkpoint.

Focused archive SHA-256 `fbdace0363482a60803a4d6b82e1b1714bfa6f06d90299ac49bcf3da42a2e6ce`;
`builddir/codex-2026-10-07/stack-subtree/focused-evidence.tgz`.
It retains exact 1314 source fingerprints, compiled identities, all nine normal
and two-plus-seven sanitizer results, build provenance, private measurements and
before/after preprocessed cleanup evidence. Native policy archives have exact
source identities. Every retained file payload was read through EOF; validation
is recorded in focused-validation.json. Earlier 1312/1314 pre-cleanup checks and
partial measurement counters remain historical, separately labelled logs.

## Combined qualification

Seven evidence archives validate all 1314 source hashes/modes, complete file
payloads through EOF, registered test identities and installed bytes after the
verified RPATH transformation. qualification-summary.json records exact archive
hashes, compiler/options and per-profile scope. Linux GCC/Clang on AMD64/ARM64
pass 155 entries each; FreeBSD/OpenBSD AMD64/ARM64 pass 143 each. All four native
BSD default-server runs pass twenty-one drivers each. Optional Xcursor-without-
XRender and SVG-disabled profiles pass 154 and 134 entries. The scoped Clang
sanitizer gate passes twelve leak-enabled pure and twenty-one leak-disabled
core checks. No production change or affected rerun was needed in these gates.

Exact frozen archive SHA-256
`4b4c0e8148072123c7ef42d62343a1a2a15bd12d5a77949f0e203d39ee25cafd`.
Default BSD installation identities are those tested by the staged drivers;
synthetic actual-core fixtures remain separately labelled fixture executables.
The original native runner's incorrect 640-case label is retained and corrected
above; the permanent unit's true 320-case scope is unchanged.

The next private acyclic-parent candidate is outside this qualified source;
[review](VERIFICATION-2026-10-07-PARENT-DEPTH.md) and
[storage decision](../decisions/PARENT-WALK-STORAGE-PROPOSAL.md).
Native Linux release CI, physical toolkit/compositor and desktop coverage remain
separately unrun. This local checkpoint does not close the entire STACK-01 audit.
