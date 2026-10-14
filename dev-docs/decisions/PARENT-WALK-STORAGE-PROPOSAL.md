# STACK-01: iterative parent redirection and storage failure

## Current compatibility authority

The quoted broad approval rule below records the policy at the time of this
historical decision. It is superseded by the user’s
[configuration-only compatibility scope](CONFIG-COMPATIBILITY-SCOPE-2026-10-07.md).
It creates no present approval hold for internal/runtime behavior alone.

Status: approved by the user on 2026-10-07; production adoption is locally qualified from the 1319-file checkpoint. The 1314-file traversal checkpoint remains accepted evidence
for its prior source. This decision concerns a new
resource-failure case, separate from the approved cyclic-parent fallback.

## Concrete control and candidate

The existing actual-core Raise finishes a finite 4096-parent chain with the
default process stack. Under a deliberately constrained 128-KiB stack, it fails;
Clang ASan reports stack-overflow in _raise_lower_recursion with repeated parent
frames. Linux normal/scoped sanitizer and native FreeBSD/OpenBSD ARM64 controls
retain both default-stack and constrained-stack outcomes. Synthetic managed
records use real unmapped X frame windows; this is not evidence that ordinary
desktops fail or that an attacker compromised a desktop.

The private candidate replaces the ancestry call stack with operation-local
frames, retaining parent style/layer/icon gates, branch bonuses and leaf-first
unwind behavior. Up to 32 ancestors use fixed local storage; deeper paths use
one checked allocation containing a sorted snapshot of the authoritative stack
ring and continuation frames (24 bytes per ring record on the tested 64-bit
platforms). Parent lookup remains tied to stack membership, with first-ring-match
selection; no new registry/cache or managed-list membership assumption is added.
No X work/callback occurs during descent. Index keys are not consulted after
leaf work begins; remaining frame borrows match the former C call stack.

There is no new client/depth cap, async completion, event dispatch or mandatory
library. Existing cycle fallback remains the originally invoked window. The
candidate finishes the 4096-parent constrained-stack control in both Linux
profiles and both native BSD ARM64 environments. Baseline/candidate short/deep
cycle, prefix, style and Raise/Lower/Restack controls preserve direct-target policy
and exact ring/server order. Forced storage refusal passes exact ancestry-versus-
invoked-window order for all three operations, normal and scoped sanitizers.

## Approved failure contract

If the deeper parent-walk storage cannot be allocated, log the failure, discard
only temporary branch bonuses and execute the ordinary direct-target operation
on the originally invoked window, with existing transient inclusion rules. Keep
client hints unchanged. Shallow paths remain allocation-free. This matches the
approved cyclic-parent target choice, but applies to a different trigger.

The alternative is to refuse the operation, preserve the current stack and
publish the corresponding corrective restack information. Falling back to the
old unbounded C recursion would leave the constrained-stack defect reachable.

[AGENTS.md](../../AGENTS.md) requires: “A specific compatibility break requires
an explicit user decision.” The new failure contract can move only the invoked
subtree where the previous successful ancestry walk would move a parent tree;
the earlier cycle approval does not establish this resource-failure choice.

## Review and adoption gates

Concrete candidate: `builddir/codex-2026-10-07/stack-parent-depth/candidate.c`,
SHA-256 `f7c4e8bcda4df01f8a175a0daa491afdc4c62b4fbfcaeac0b1ead422bc566201`; candidate.patch shows the exact difference from qualified source.
The [review report](../verification/VERIFICATION-2026-10-07-PARENT-DEPTH.md) records
compiled identities, controls and retained archives. Approval precedes production
activation, pure policy separation/coverage, actual lifetime/group/layer/retirement
regressions and one coherent six-target/install/default-server qualification.
Private checks are not production acceptance or native release CI.

Production policy, ownership and real-frame adoption gates now pass in [the parent-continuation checkpoint](../verification/VERIFICATION-2026-10-07-PARENT-CONTINUATION.md). Further initialized-list work has its separate source; native release CI remains open.
