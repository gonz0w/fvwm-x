# Script selection owner and requestor checkpoint

Status: selected for staged implementation under SELECTION-01 #22 by the lead
within the owner-approved R1 scope, 2026-10-31. This is an internal protocol/API
decision; supported configuration commands, arguments and bindings stay usable.
The tracker owns progress. Whole #22, protected clipboard/history/storage and
manager/SAVE_TARGETS retain their separate acceptance.

## Finite outcome and roles

Script PRIMARY ownership holds an immutable text snapshot, confirmed server
time and local epoch, separate from widget text and root cut buffers. Matching
ownership loss retires that epoch; stale events and shutdown cannot replace a
newer foreign owner. Metadata describes the actual supported representations.
Keep legacy STRING bytes; advertise UTF8_STRING only for a valid representation.

The named Script private selection remains a destructive per-destination message
queue, not a clipboard. TARGETS and TIMESTAMP are observational and never take a
message. Advertise the three metadata targets and currently queued destination
atoms only. Direct custom-target replies preserve exact NUL-inclusive bytes.
Ordered MULTIPLE consumes matching messages in FIFO order; unsupported, malformed,
stale, foreign or over-budget requests do not dequeue. Retain the existing
40-message/32 KiB-per-message limits and one total two-second shutdown grace.
No reliable-delivery/requeue guarantee is added for a peer destroyed on delivery.

Requestors distinguish exact custom-target reads from negotiated text. Exact
reads never probe TARGETS. Text reads negotiate usable UTF8_STRING/STRING;
legacy fallback follows only an actual TARGETS refusal. A valid TARGETS list
may be direct or INCR-encoded XA_ATOM/32 within its same 1,024-byte cap; metadata
is not rejected merely for using INCR. Use one absolute monotonic deadline across
negotiation, fallback and INCR. Validate all reply
fields, types, lengths and terminal conditions before returning complete data.
No partial result reaches Script. Keep transfer events ahead of safe servicing;
unrelated input retains order. Individual Xlib calls or entered callbacks cannot
be preempted by a time budget; never call this module helper from the WM loop.

## Bounds adopted for this slice

- Text and exact transfer wire data: 32 KiB. A local C terminator is separate;
  private-message wire NUL remains included in its existing 32 KiB limit.
- Request lifetime: at most one second total, including fallback and callbacks.
- TARGETS reply: at most 256 packed atoms/1,024 bytes.
- INCR: at most 4,096 nonempty chunks and the same aggregate byte/deadline cap;
  validate the header before acknowledgement and require a typed empty terminal.
- Owner MULTIPLE: at most 128 pairs/1,024 packed bytes and 128 KiB aggregate
  property output. Validate the complete control list before side effects;
  refuse nested MULTIPLE, invalid destinations and control-property overwrite.
- Service work: at most 32 event records per pump. If temporary deferral is
  needed, at most 256 records, restored in FIFO order on unwind. A filtered
  implementation that leaves unrelated events queued needs no new deferral array.
- One active Script requestor operation; recursion/mutation/quit/pipe loss cancels
  stale work through the existing execution/lifetime guards.

## Staged implementation and shared qualification

1. Add operation-local typed request/result primitives in `libs/FSelection` for
   bounded exact/negotiated direct and INCR reads. Prefer the existing module-only
   synchronous/service pattern over a new general engine. Preserve the existing
   wrapper bodies/callers until explicit adoption. Author meaningful native-X
   coverage with this change; review and compile the affected target as needed.
2. Add Script-local immutable owner/epoch and metadata/private-queue primitives.
   Freeze the advertised role set before metadata adoption. Reuse FProperty
   packing and expected asynchronous-X policy; no core-source inclusion.
3. Adopt requestors in TextField/private messages, with safe non-input X/module
   servicing, mutation cancellation, ownership loss and shutdown preservation.
   Keep exact custom-target message handling separate from text normalization.
4. At the completed major feature/shared checkpoint, run affected Linux native
   policy/X checks and warranted scoped ASan/UBSan once over these related PRs.
   Capture visible behavior at this checkpoint and reuse the theme session.
   Execute the two queued Q67-OPENBSD-NOBUS checks here, with the empty directory
   default preserved. Do not rerun the six enabled-service or transfer sweeps.

Tests are authored alongside each change; fresh runtime execution is queued to
this combined checkpoint under the owner's 2026-10-31 cadence. A reproduced
serious defect retains its focused regression. Full platform/install/default-X
qualification remains a milestone/release gate; early affected targets require
an actual changed contract or observed failure. Source/static review and needed
incremental builds precede implementation merges. Pending is never passed.

S12 Form cut-buffer evidence is retained. Core S01–S06 and any adopted S15
manager role retain their own unresolved register rows; S16 uses PR322's file-only
participants/evidence. This slice alone does not close whole #22. No generic
clipboard engine extraction, new daemon/toolkit/storage/image feature or mandatory
bus is selected. Details and source observations from the bounded analyst
handoff remain in the owner-controlled Script selection working notes.
