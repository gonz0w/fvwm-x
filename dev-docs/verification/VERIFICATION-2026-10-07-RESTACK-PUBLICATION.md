# STACK-01: owned restack publication across callbacks

Status: locally qualified from the immutable 1335-file combined source. No configuration format/name/argument change or approval hold applies.

## Finding and repair

A finite actual-core 170-window control models a command executed inside
PositiveWrite's synchronous unlock wait. Lower moves record 83 during the first
restack part. The original live-ring packet continuation then follows the changed
links: its next part includes the root XID and 87 record slots disagree with the
operation's original stream. Both normal and scoped ASan/UBSan controls reproduce
this result. The control wraps the callback boundary, rather than running a real
module process handshake; the existing module-sync suite remains included.
This is an ordering/ownership finding, not a demonstrated memory corruption or
security compromise.

`stack_restack.c/h` owns numeric client/frame/opaque-handle records before any
module callback, including the initial Raise message. Packet parts preserve the
existing size limit and repeated last-record anchor. They never dereference live
window records after preparation. Checked counting/byte arithmetic and fallible
allocation leave an empty owner on refusal; teardown frees once. Ordinary
synchronous delivery and module masks remain intact.

A nested restack also marks final publication pending. Completing the old owned
snapshot alone would leave its tail after a newer nested broadcast, so the outer
event loop publishes the current ring after command resources unwind. This extra
repair uses the existing copied output queue without a synchronous wait. Queue
masks, byte cap, order, retirement and writable readiness remain shared with
PositiveWrite; the ordinary broadcast still honors its synchronous mask. Two
small static counters/flags track nesting/pending work. No window pointer,
persistent snapshot, registry, arbitrary timer, library or thread is retained.
Snapshot refusal logs and skips that publication while keeping the WM running.

## Focused coverage and source

The pure unit covers empty/single/adjacent boundaries, allocation refusal/retry,
broken endpoints, 2/3/83/84/85/166/167/170/513-record streams, every repeated
anchor and packet bound. Clearing live IDs and links after preparation leaves
every copied packet unchanged; teardown is idempotent.

The real-X fixture covers 21 cases: 1/2/3/83/84/85/170 windows, ordinary delivery,
Lower during the first restack part, and Lower during the initial Raise boundary.
Original packet records remain exact; the final repair matches the current ring
and a second flush adds nothing. A separate call through the actual queue helper
uses a synchronous mask with no unlock input: it returns with one copied packet,
retains the producer, survives mutation of the input buffer and drains identical
bytes through normal output servicing.

Eight pure and eighteen actual-core tests pass in normal Linux and scoped Clang
ASan/UBSan profiles (pure leaks enabled, core leaks disabled). Existing stacking,
module-sync, output and backpressure controls are included. Both native BSD ARM64
overlays pass the new pure unit and all 21 actual-core cases. The first native
runner omitted the group predicate header from its overlay; its compile failures
are retained and the complete-overlay rerun passes without production changes.

Frozen source and complete focused/private evidence:
`builddir/codex-2026-10-07/restack-publication`. This combined source extends the
qualified 1332-file storage source through 18 owned additions/changes/deletions,
including [group-free closure and unbuilt-source cleanup](VERIFICATION-2026-10-07-STACK-GROUP-CLEANUP.md).
The final combined gate passes 1272 accepted entries across eight compiler
profiles on Linux/FreeBSD/OpenBSD AMD64 and ARM64, 120 exact staged C identities,
and 108 installed default-BSD-X driver invocations. Optional profiles pass
164/144 entries; scoped sanitizer coverage passes 43 checks (16 pure with leaks
and 27 actual-core with leaks disabled). Seven final archives validate full
compressed/payload EOF, exact source hashes/modes, registered identities and
installed RPATH-adjusted bytes. Archive SHA-256:
`6740e6f7398bf05536c04079da3811d8bde1744b04aec2971f1db719d03807de`.

OpenBSD AMD64's first ordinary run stalls while a Meson pre-exec child remains
asleep. Process state, initial logs and exact source identities are preserved.
Only that verified owned child is ended; the run resumes and records image-input
as TIMEOUT (exit 0) and image-decompress as FAIL (signal 9), both at about 929
seconds. The underlying spawn-stall cause is not established. Each affected test
passes three reruns with source and outer/internal limits unchanged. Validation
checks the exact two-record replacement, retaining the other 150 ordinary
results. The existing verification-profile-only outer 60-second/internal
15-second TCG allowance is preserved; the ordinary tests retain their original
limits. No production or fixture edit is made for recovery.

Native Linux release CI and physical/toolkit/compositor checks remain separate.
Later stacking retirement and module allocation changes are outside this source.
