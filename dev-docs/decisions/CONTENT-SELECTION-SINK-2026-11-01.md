# Bounded selection sink handoff

Owner Cam; #63/#71/#74 with SELECTION-01. Status: source implementation; native compile/link checking,
following the [shared native owner Q1](../verification/VERIFICATION-2026-11-01-CONTENT-DEDUP-PINNING.md).
The [tracker](../PROJECT-TRACKER.md) owns execution. No installed clipboard
capture, numeric history default, new service/dependency or copy-policy waiver.

## Finite outcome

Deliver four related stories using the current module-only requestor:

1. Add format-8 property delivery through borrowed slices of at most 4 KiB,
   validating the complete bounded Xlib reply first. Avoid an additional
   project-owned allocation; preserve existing packed format-32 wrappers.
2. Add an exact selection sink for direct/INCR data, retaining private requestors,
   the 32-KiB total/4096-chunk/32-event caps and one absolute deadline. Preserve
   original NUL bytes; never normalize text or trust an INCR size hint as an
   allocation authority. Acknowledge an INCR chunk only after consumption.
3. Add an explicit source-token validation seam and native producer adapter.
   Recheck the caller's generation before/after delivery and completion; hold
   one exact reserved producer, append bounded bytes and publish actual length
   only on terminal success. Refusal, cancellation or expiry retires partial
   work without releasing charges before cleanup.
4. Author independent property/selection and owner-adapter cases for direct/
   INCR, embedded NUL, empty/maximum/over-cap replies, malformed later chunks,
   stalled/dead/replaced owners, cancellation and stale callbacks. Execute once
   at the next named real-X transfer checkpoint; reuse Q1's unchanged results.

Keep aggregate/text wrappers usable and their existing command/configuration
contract unchanged. Module-only synchronous Xlib calls and entered callbacks
still need a responsive server/caller; never call this path from the core loop.
Use one complete bounded property reply instead of splitting it across mutable
X requests. A borrowed callback must not retain data, pump events, reenter the
reader or block. Partial successful consumption is not complete publication.

## Implemented source contract

`FPropertyConsume8` validates one complete format-8 reply, calls a synchronous
borrowed consumer in slices at most 4 KiB and wipes its bounded accessible Xlib
reply bytes before release, including refusal paths. Empty input invokes
NULL/offset-zero/length-zero once. Output length remains zero unless every
callback succeeds; partial consumption cannot be rolled back. No additional
project-owned payload allocation or format-32 wrapper change is introduced.

`FSelectionRequestExactTo` snapshots a source/sink and its pre-admitted byte cap
(0..32 KiB; zero permits empty only). Direct and INCR use the same requestor,
header-drain ordering, total deadline and event/chunk limits as the aggregate
path. No new aggregate is allocated. Source/deadline validation surrounds
consumption, service and completion. Current-owner window checks supplement the
caller's ordered generation observer; they do not detect owner-window ABA alone.
At the chunk-count cap only an empty terminal property can be accepted, so the
4097th nonempty chunk cannot reach the consumer. ACK follows successful delivery
and closed native producer access. Failure destroys the private requestor.

`content_selection_receive` accepts one caller-prepared, empty, encoded-only
representation. A guarded zero-write and read-only metadata inspection validate
that producer before taking completion; its capacity bounds the transport.
`producer_taken` makes the handoff explicit. Hint mismatch retires before
requesting payload bytes. Exact offsets append through the existing owner;
only complete transport with current source, fresh post-observer clock and exact
initialized prefix can publish. Failure retires only this ref and leaves resource
charges for scheduled cleanup. Primary diagnostics remain separate from
retirement diagnostics; the primary cleanup-pending bit includes retirement.
A LIVE or otherwise invalid producer cannot be taken or retired by preflight.

Clocks and observers are bounded native callbacks. Source-current may advance
only its own qualified observer, never general/requestor events. No callback
can retain payload pointers or reenter the reader; no concurrent receive or
independent writer may share the producer. A time budget cannot preempt one
blocking call. Publication is not an atomic X-server/native-memory transaction.
These private C APIs add no portable packet, installed caller or public option.

## Copy and source-observation boundary

Today `FPropertyRead` receives an Xlib allocation, copies into ordinary project
heap storage and calls `XFree`; INCR adds another aggregate allocation/copy.
Borrowed delivery removes those project-owned copies. It does not establish
protection for Xlib's original reply or internal buffers. The native owner's
borrowed-source boundary remains binding, and absent secret hints do not prove
content is public. Source-only fixtures use explicit public bytes.

Before installed capture, the owner must freeze one of these copy contracts:

- Permit one bounded, transient ordinary Xlib reply in the isolated native
  process; copy directly to prepared owner storage, wipe accessible reply bytes
  before `XFree` and document remaining library/server/source-client copies.
  This minimizes copies using the current stack but cannot establish that the
  reply was excluded from swap before arrival. Process nondumpability does not
  supply that missing guarantee.
- Require protection before any reply arrives. The current Xlib interface cannot
  attest that property; a separately evaluated process-wide protection approach
  and budgets are required before installed adoption. Locking a returned reply
  afterward cannot prove protection before receipt.

Neither alternative is adopted by this source batch. Do not infer a policy
change from the owner excluding recognized sensitive items or retaining history
while locked. The separate lock-time capture/access question also remains open.

A caller token is not authoritative X-server evidence. `XGetSelectionOwner`
checks cannot detect reacquisition by the same owner window. Installed capture
must use qualified ordered owner-generation observation, or refuse unavailable
profiles; the source callback seam alone does not establish it. Keep the existing
optional XFixes/platform decision with that integration rather than adding a new
mandatory dependency by inference.
