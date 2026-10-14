# STACK-01: parent continuation and storage-failure adoption

Status: locally qualified from the exact 1319-file snapshot. The subsequent
initialized-list batch has its separate pending source and report.
The user approved the invoked-window failure contract on 2026-10-07. The accepted
1314-file traversal checkpoint and [private parent-depth review](VERIFICATION-2026-10-07-PARENT-DEPTH.md)
remain separate evidence for their source.

## Production changes

`fvwm/stack_parent.c/h` prepares parent policy with no X work or callbacks. It
uses the authoritative stack ring and first-ring-match selection, retaining
icon, layer, parent-style and branch-weight rules. The first 32 continuations
use inline storage; deeper paths have one checked allocation for indexed keys
and continuation frames. No persistent registry or client/depth limit is added.
Existing constant-space cycle policy remains shared. Sorted keys are unused
once leaf X work begins; frame borrows have the former call stack's synchronous
operation scope. Pop clears consumed inline/continuation slots; finish releases
and clears the operation owner, including failure and early cancellation.

The stack wrapper now executes the leaf and unwinds children iteratively, checking
the same current parent transient-inclusion style before each child operation.
The former recursive parent helper and misleading one-level recursion comment
are removed. Cycles and approved storage refusal discard temporary branch ranks
and use the original target with existing inclusion rules. Existing new-window
placement/follow-up, focus, module broadcasts, X ordering and exposed redraw
service remain in their owners. The AsciiDoc manual records the failure rule.

## Focused evidence

The pure unit covers 1–4096-record chains across all three modes, the inline
boundary, caller-supplied branch weights, flag/link preservation, indexed and
inline duplicate selection, stack-only membership, an invoked target outside
the ring, missing/self/icon/layer/style stop rules, short/deep/prefixed cycles,
forced allocation refusal, direct fallback preparation, retry and early owner
cleanup. An independent visited-vector/linear-lookup oracle checks policy.

The actual-core driver has 13 subruns: 90 cycle/style/operation parity cases,
4096-parent Raise/Lower/Restack with default and constrained 128-KiB stacks, and
normal ancestry/forced storage refusal for each operation. Synthetic records
have real unmapped X frames; deep controls register X identities, preserve
parent hints and verify authoritative ring/backlinks against real server order.
Refusal cases use independent numeric expected orders. These controls leave
focus-button policy to existing real-client regressions.

Eleven focused Linux tests pass. Three pure leak-enabled and eight actual-core
leak-disabled Clang ASan/UBSan checks pass. Both native FreeBSD/OpenBSD ARM64
focused groups pass the pure unit and all 13 actual-core controls. Initial
compilation exposed a removed include still needed by the independent transient
query; the include was restored before these successful builds. The initial
failure is retained. Neighboring tests cover client identity, layer propagation,
parent hints/retirement, transient closure and rank/sort behavior.

Exact source: `builddir/codex-2026-10-07/stack-parent-production/source-stack-parent.tgz`,
1319 files. Hashes, modes, scripts and logs are in that directory. The snapshot
contains only nine owned changes from the qualified traversal source; unrelated
shared manual/build/runtime changes are preserved in the working tree and
excluded from this checkpoint. Full compiler/install/default-BSD-X/optional and
scoped sanitizer gates pass. Native Linux release CI and physical desktop
coverage remain separate from local emulated verification.

## Combined qualification

Source archive SHA-256 `2fee76484576e06636d739e6753db1e34ced585736561ca57e8019ba97ade22d`.
The combined checkpoint passes 1208 registered test entries across eight
Linux/BSD compiler profiles, 120 staged code identities with verified RPATH
transformations, 88 default BSD X driver invocations, optional 156/136 entries
and 35 scoped sanitizer checks (13 pure leak-enabled; 22 actual-core with leaks
disabled). Seven retained archives validate all 1319 source hashes/modes,
registered test identities, installed bytes and archive payloads through EOF.

OpenBSD AMD64's unchanged fake-command verification-profile test exceeded its
30-second outer test allowance twice. The affected-only check passed in 30.93
seconds with a finite 60-second outer allowance; internal 15-second subprocess
deadlines stayed unchanged. Both timeouts, the bounded rerun and exact combined
145-entry record are retained. No production fix or runtime deadline adjustment
was made in the combined gate. This accommodation is local TCG test timing,
not passing native release CI.

The established Asciidoctor renderer includes the approved fallback in the
command manual. Native OpenBSD mandoc at 60 columns renders the paragraph
correctly. Its inherited two renderer macro errors remain unchanged; one added
long-input-line STYLE diagnostic is corrected by source reflow in the following
initialized-list checkpoint. This is a scoped content/render check, not a clean
lint verdict for the inherited complete manual or a completed DOCS-01 rewrite.
