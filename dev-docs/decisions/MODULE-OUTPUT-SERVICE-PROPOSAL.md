# OUTPUT-01: bounded asynchronous module output

## Current compatibility authority

The quoted broad approval rule below records the policy at the time of this
historical decision. It is superseded by the user’s
[configuration-only compatibility scope](CONFIG-COMPATIBILITY-SCOPE-2026-10-07.md).
It creates no present approval hold for internal/runtime behavior alone.

Status: approved by the user on 2026-10-06; implemented and locally qualified in
the [combined FIFO/stack/output checkpoint](../verification/VERIFICATION-2026-10-06-FIFO-STACK-OUTPUT.md).
Native Linux release CI remains separate; synchronous deadline adoption has its
own pending decision under OUTPUT-02.

## Concrete baseline

The independent development core now uses constant-time FIFO removal. A private
fixture relinks its existing objects, wraps only startup capture and writes to
one owned module endpoint, and invokes the real `FlushMessageQueue`. An always-
ready verification write shim accepts every packet. One ordinary asynchronous
flush consumes 1,000 packets, 2,048,000 bytes and 1,000 write calls before returning.
This proves absence of a service bound; it is not a wall-clock benchmark or an
observation that real pipe writes never block. Inputs fit the existing 8-MiB
queue cap. A repeated EINTR also has no finite write-attempt budget.

Evidence: `builddir/codex-2026-10-06/output-service/output_baseline.c` and
`run_baseline.py`; guest `output-baseline-evidence/result.txt`. No transport or
production write behavior was changed for the baseline. Full frozen active-menu
qualification excludes this independent development batch.

## Proposed contract

For each ordinary asynchronous module flush, stop after the first reached bound:
64 KiB successfully written, 32 completed packets, or 128 write attempts.
Cap each requested write by the remaining byte budget. Count attempts including
EINTR; retain the current offset on interruption or EAGAIN. A partial packet stays
at the head, preserving packet order. Release queue accounting only after a
complete packet is detached; queue memory limits and death-row ownership stay
unchanged. Zero writes and terminal errors retain existing disconnect handling.

Pending output already requests writable readiness in the normal and audited
interaction owners. Return to those owners and continue on their next pass.
Introduce no thread, new scheduler, delivery timeout or mandatory dependency.
The existing synchronous lock-on-send flush retains its completion/wait contract
and module timeout; extending fairness through that legacy synchronous barrier
requires a separate ownership/timing review. This proposal bounds each ready
module's asynchronous pass, not total work for an arbitrary number of modules.

Define limits together in the existing event-loop policy, with pure coverage for
partial writes, exhausted bytes/packets/attempts and non-progress attempts. Add
actual-core write-counter checks, fast/slow real module readers, input/timer
progress, disconnect, held interaction and unchanged synchronous handshake.
Complete these stories with the FIFO/stack correctness batch, then run one
combined checkpoint. Do not claim latency gains before real profile evidence.

## Compatibility decision

A ready module can receive a queued burst over several service passes instead of
one. Command and packet order remain unchanged, but delivery timing changes.
[AGENTS.md](../../AGENTS.md) says: “A specific compatibility break requires an
explicit user decision.” The approval requested is for these finite asynchronous
service bounds; synchronous ordering is retained.
