# STACK-01: acyclic parent depth and private iterative review

Status: private review retained as historical evidence. The user approved the
storage-failure contract on 2026-10-07; production adoption is covered separately
in [the parent-continuation report](VERIFICATION-2026-10-07-PARENT-CONTINUATION.md).
The independently qualified three-story traversal checkpoint remains evidence
for its prior source.

## Observed behavior

A finite actual-core startup fixture creates 4096 unmapped X windows and synthetic
managed records with an acyclic parent chain. Ordinary default-stack Raise exits
normally. A deliberately restricted 128-KiB stack produces a signal failure;
Clang ASan identifies stack-overflow in _raise_lower_recursion. Default-stack
controls pass in normal/scoped sanitizer Linux and native FreeBSD/OpenBSD ARM64.
This stress control does not claim ordinary desktop failure or compromise.
Focus button grabs are wrapped; full client lifecycle/decoration acceptance remains
separate from these synthetic-record real-frame controls.

The private candidate uses 32 inline ancestor frames and one checked operation-
local allocation only for deeper paths. Sorted parent keys come from the actual
stack ring; no managed-list membership shortcut is used. Descent performs no
X/callback work; leaf-first continuations preserve existing styles and branch
weights. Index keys are unused after leaf execution begins; record frame borrows
have the same operation scope as the original recursive frames. No mandatory
library, command timing change or new depth/client limit is proposed.

## Focused results

The candidate completes both default and constrained-stack 4096-parent controls
under normal/scoped Clang ASan/UBSan Linux, plus both native BSD ARM64 environments.
Both old/new normal and scoped sanitizer fixtures pass 90 cycle/prefix/style/
operation cases per executable, matching direct-target fallback and real server
order, plus the following acyclic operation. Deep cases cross the inline-storage
boundary. Forced storage failure and ordinary ancestry each pass Raise, Lower
and Restack with exact numeric expected ring and real-server order in normal and
scoped sanitizers. The proposed failure affects only the invoked window/subtree;
ordinary successful ancestry is preserved.

Candidate SHA-256 `f7c4e8bcda4df01f8a175a0daa491afdc4c62b4fbfcaeac0b1ead422bc566201`; production remains the exact qualified
1314-file source. `builddir/codex-2026-10-07/stack-parent-depth/review-validation.json`
records three retained archive identities. Linux archive SHA-256
`9bdf4ec8b2e7446f7c4fa93fd239964788896127ea6d2953813831b6ac83a0c8`.
Every archived file identity and payload through EOF validates; candidate bytes
match on Linux and both native guests. Source/compile/link controls, binaries,
ASan trace and initial/final logs are retained. The initial fixed-small-stack
fixture and two-operation storage checks are historical; final variants add the
default-stack control and all three operations. No production source was changed
for these controls.

The [storage-failure proposal](../decisions/PARENT-WALK-STORAGE-PROPOSAL.md) recommends
invoked-window fallback on failed deeper storage, separately from approved cycle
fallback. Production refactoring, native pure units, complete group/layer/
retirement checks and full qualification follow approval. Native Linux release
sanitizer CI, AMD64 private depth checks and physical desktop coverage remain open.
