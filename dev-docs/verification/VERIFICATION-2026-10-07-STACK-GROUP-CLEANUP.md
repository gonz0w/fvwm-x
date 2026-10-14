# STACK-01 / DEAD-01: group-free closure and unbuilt sources

Status: implemented; focused normal/scoped sanitizer and native BSD ARM64
controls pass. Exact 1330-file source is separate from the preceding frozen
1332-file storage checkpoint. Combined qualification passes in the 1335-file callback/publication
batch: 1272 accepted standard entries, 120 staged identities, 108 default BSD X
drivers, optional 164/144 and 43 scoped sanitizers. See the
[combined report](VERIFICATION-2026-10-07-RESTACK-PUBLICATION.md), including retained
OpenBSD spawn-stall failures and unchanged affected reruns. No configuration migration or approval hold applies.

## Group-free Iconify/Deiconify

The group-aware entry path previously disabled the parent worklist even when no
usable group hint existed. A private actual-core comparison reproduces 4,194,304
membership tests for a 2048-record reverse chain in the group-aware path. The
candidate uses 4096 tests through the same parent-only worklist. Forward/star,
retained ranks, cached layers, identity boundaries, existing groups and allocation
fallback controls match in normal/scoped sanitizer profiles. These are operation
counts, not physical desktop latency measurements; counters remain private.

One pure predicate now owns style opt-in, WindowGroupHint and None/self/root
exclusions. The existing ordered mark path uses that predicate. A conservative
preflight over the selected range keeps ordered group scanning when any usable
hint exists; otherwise the existing two-scan parent worklist applies. First-mark
policy, group-enabled behavior, icon exclusion and rank/layer owners stay intact.
No callback, X request, persistent registry or new allocation owner is added.

The extended pure unit covers eighteen hint eligibility combinations in addition
to its existing 320 closure combinations and refusal/retry control. The actual-core
fixture adds twenty group-aware parent-only controls across five sizes and three
shapes, including five allocation refusals through the original scan fallback.
It retains the existing twenty closure controls and ordered-group/rank checks.
Seven pure/fourteen actual-core tests pass in the normal Linux build and scoped
Clang ASan/UBSan profile. Both native BSD ARM64 overlays pass the extended subtree
unit and six actual-core result categories. An initial native runner argument typo
is retained separately; the source did not change for the corrected invocation.

## Unbuilt-source removal

The empty `fvwm/template.c` and obsolete `libs/asprintf.c` are removed. Neither
appears in any supported native compile database. Six operating-system/architecture
environments, including both Linux compiler profiles, pass isolated system
asprintf/vasprintf calls using the project's generated config. Compiled FVWM
support code refers to the system vasprintf symbol; config declarations remain
because they support that active call path. No formatter replacement, library
floor or build-source-list change is introduced. Prior immutable sources retain
both deleted files.

The review's initial OpenBSD thin-archive nm invocation failed, followed by a
name-only nm-output assertion failure. Reading the compiled object directly and
handling the observed `nm -u` format completes those two reviews; original logs
are retained. These were review-tool issues, not window-manager failures.
Build-graph/link evidence is retained in the preceding checkpoint's
`dead-source-review` artifacts. Deletion is based on that evidence and reference
review, rather than lexical absence alone.

Exact current source and focused runners are in
`builddir/codex-2026-10-07/stack-group-cleanup`; private counts/comparison source are
in `builddir/codex-2026-10-07/stack-group-gate`. Source hashes distinguish these
changes from the locally qualified preceding storage checkpoint. Native Linux
release CI and physical/toolkit/compositor checks remain separate.
