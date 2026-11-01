# Bounded core manager selection completion

Selected under SELECTION-01 #22 by the lead, within the accepted R1 transfer
contract. The analyst and separate reviewer accepted the finite design before
implementation. This changes internal completion and error behavior, preserving
FVWM configuration files. Runtime qualification is queued at the next shared
major-feature checkpoint; source and compilation alone are not acceptance.

The core manager supports TARGETS, MULTIPLE, TIMESTAMP and VERSION for its
screen's WM_Sn. Startup confirms acquisition as before. Only a genuine server
SelectionClear naming that active connection, selection and owner window ends
the session. Clear admission does not use the request timestamp's half-wrap
policy and adds no ownership query. Core clipboard/history or SAVE_TARGETS is
not selected by this decision.

The previous conversion helper returned success after submitting XChangeProperty.
It could not mark a MULTIPLE pair failed when the server rejected the write.
A synchronous XSync after every conversion would block the core loop. Instead,
a manager-only continuation records exact native X request serials and receives
an ordered server marker on a private InputOnly window. This follows the existing
picture completion mechanism without sharing image jobs or adding a generic
transport framework. The private window avoids several NoFocusWin event-mask
restoration paths that do not retain PropertyChangeMask.

## Fixed behavior and bounds

- One active conversion and sixteen queued request headers; admission fixes the
  one-second total deadline. The existing 128-pair/1,024-byte packed control cap
  remains. Only the fixed-size manager metadata is produced.
- Record display, serial and expected opcode before each write, including before
  an Xlib flush can deliver an error. The error handler only marks owned records;
  unrelated errors retain their previous handling.
- A direct conversion reports success after its terminal marker. MULTIPLE first
  settles constituent writes in order, replaces failed destinations with None,
  then confirms the corrected control rewrite with a second terminal marker.
  Unsupported targets and control-property overwrite are refused.
- A marker must be PropertyNewValue on the exact display/window/atom/serial and
  must not be synthetic. A terminal marker error also settles that phase.
  Individual data-write or window-creation errors do not settle later writes.
- Expiry sends one refusal. Keep the active serial map until its terminal marker
  or error; expire queued headers even while that tombstone waits. Loss/Close
  prevents new success publication and preserves serial ownership through
  XCloseDisplay. Forked children do not control the parent's marker window.
- The existing bounded MULTIPLE snapshot read remains synchronous. Observe fresh
  time after it and before output; no deadline can preempt an entered Xlib call.

## Core integration and interactions

Manager error routing precedes generic suppression. Ordinary dispatch consumes
private markers before property coalescing. Readiness servicing runs in both
ordinary and interaction waits, including under queued-event floods. The timeout
query is pure and returns zero for work made ready by error processing, preserving
the final FPending-to-poll invariant and existing startup timeout semantics.

Some resize paths dispatch property notifications directly; other interactions
leave them queued. A manager-only scan visits at most 32 already-queued events
using raw queue operations, preserving FEvent's input/time cache and the order
of retained records. It consumes only private markers. A genuine manager clear
closes metadata admission/publication but remains queued for ordinary lifecycle
dispatch; the scan never invokes Done from an interaction.

A marker extracted by a modal event mask is deferred until a complete bounded
scan establishes that no queued loss precedes it. Under a longer unconsumed
prefix the request may expire. This conservative availability limit preserves
ownership and input order without adding an unbounded scan or deferral queue.

## Authored acceptance

Link production manager code to deterministic X transport for identity predicates,
write errors during submission, marker/control phases, native serial wrap,
creation failure followed by later errors, FIFO capacity/expiry, timeout plus
late completion, close/fork guards and modal queue ordering. This verifies routing;
injected BadAlloc is not server allocation-exhaustion evidence.

Independent actual-X peers add rejected destination writes with recovery and
mixed MULTIPLE failures. An invalid atom is first confirmed on the peer; a
synthetic SelectionRequest is necessary because XConvertSelection would reject
it before core dispatch. Two actual screen managers cover synthetic exact-clear
survival, queued work during genuine loss, successor preservation and continued
service on the other screen. A peer assigning another selection to the core's
window owns it itself; that cannot establish a genuine foreign-role core clear.

Retain earlier manager/Form/file-only transfer results with exact source/profile
limits. New sources require their named shared qualification. Script's separate
Fontconfig clean-exit failure and Q67's two unrun disabled-service checks remain
open; neither is silently absorbed or called passed by this implementation.
