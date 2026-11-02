# Ordered selection source observation

Owner Cam; #22/#63/#71. Status: implemented four-story source batch; separate final review pending, base merged
PR338 `627ec305f7fb8e23d7b4fed1584725d3179dd0d5`. The
[tracker](../PROJECT-TRACKER.md) owns execution. Reuse the current
[borrowed sink](CONTENT-SELECTION-SINK-2026-11-01.md) without installed capture,
new defaults/service or mandatory dependency. No copy-policy waiver.

## Finite outcome

1. Add pure caller-owned lifetime/generation policy, with nonzero unique observer
   epochs, nonwrapping generations, explicit unavailable/unsynchronized/ready/
   closed/exhausted states and safe token outputs. Every admitted ownership
   notification invalidates, even for the same owner or equal/wrapped timestamps.
2. Add an optional module-only XFixes observer on one private InputOnly window,
   subscribing to set-owner, window-destroy and client-close notifications.
   Missing build/server support is explicit refusal. No dependency is added to
   existing FSelection callers; optional linking follows the tray pattern.
3. Add one bounded queued-observer collector and current-token callback. Consume
   only exact authentic observer events; preserve unrelated/requestor FIFO and
   cached input state. Incomplete scans invalidate and refuse. No general event
   dispatcher, registry, idle polling or blocking stabilization loop.
4. Author pure and independent public-X cases for window ABA/reacquisition,
   equal/wrapped timestamps, exhaustion/restart, initial/query races, client loss,
   forged events, flood refusal, reentry, close and unrelated FIFO. Join the
   already named shared transfer checkpoint; no per-helper runtime/matrix.

The private native `FSelectionSource` gains an observer epoch, and current()
receives the actual query display/selection. Reject mismatched binding before
touching the observer queue or policy; a query on another connection cannot
order this connection's queued notifications. Update all shipped
consumers together; this C-only change affects no FVWM configuration, portable
packet, Perl metadata or installed capture protocol. Tokens remain native
continuations, not authenticated external claims. Caller storage and display
remain alive through every outstanding callback; epochs are unique across
observer lifetimes, including recreated storage. Native storage starts zeroed;
live/busy/incompletely closed opens refuse without losing resources. Clean
closed storage can restart only under a new caller-unique epoch.

## Ordering and refusal

Create a private observer window, subscribe, then query XGetSelectionOwner on
the same connection. The synchronous reply orders the preceding subscription
and earlier server notifications. Drain only already-queued events with a fixed
budget. If any authentic ownership event is found, or scanning is incomplete,
invalidate and return retry/unsynchronized; issue no token. A later explicit
refresh repeats query then bounded drain. Only a complete scan without ownership
events publishes that queried owner under a fresh generation. Empty selection
has no capturable token. Do not assume subscription supplies an initial snapshot.

Existing FSelection and native-producer publication perform the owner query
before current(). Preserve that barrier. Current drains only queued observer
notifications, compares the exact epoch/generation/owner and never issues X
requests or mints a replacement. It refuses an unsynchronized/closed observer
without repeatedly rescanning an unchanged unrelated prefix. Recovery happens
outside transfer after the module services general queue progress.

Match display, extension event type, private window, selection, recognized
subtype and send_event=False. Do not match the notification owner against the
old owner: successor and loss notifications must invalidate too. Timestamp
fields are metadata, never a token ordering/identity proof. Content changes
without an ownership operation remain outside this observation contract.

Raw queue collection follows FEvent's bounded snapshot/restore pattern without
changing its cached timestamps/input state. Restore unrelated events in original
FIFO order. Close invalidates before destroying only this observer's window;
keep identity/counters, do not reassign selection ownership or free metadata
retained by callbacks. Required cleanup/error and responsive-server limits remain
explicit. No core-loop use or entered-call preemption is promised.

## Evidence boundary

The installed xorgproto 2025.1 fixesproto selection-tracking specification defines
owner, three causes and two timestamps. It does not establish actual same-window
server behavior; independent real-X observations remain required. Existing tray
subscriptions are an optional-dependency example, not a qualified generation
observer. The same-window, initialization and query-barrier tests join one early shared
Linux primitive-transfer gate across PR338 plus this eight-story transport/
observer handoff. Bring that finite gate forward because actual same-window
notification semantics are essential to the chosen approach; do not build
capture on a source-only assumption. Combine pure normal/scoped-sanitizer and
public private-X transport/observer/adapter cases, without WM UI/install/all-OS
or unchanged owner Q1 repetition. Installed capture/retention/consumer gates
remain separate and wait for their combined feature handoff.

The owner-copy and lock-time capture/access questions remain separate. This
public-byte source batch cannot authorize installed sensitive copying or turn
retained history during lock into capture/read access. No new UI: screenshots
and theme checks are not applicable here.


## Observed checkpoint disposition

[Combined evidence](../verification/VERIFICATION-2026-11-02-SELECTION-OBSERVER.md)
qualifies the Linux ARM64 positive primitive cases, scoped sanitizer and build-
disabled refusal. It also retains an adapter pre-poll failure and its targeted
correction. Both bounded server-absence configurations abort external Xvfb;
that runtime refusal remains unavailable and queued for acceptance/release.
No source-only, synthetic-event or compiled-stub result substitutes for it.
The finite implementation may integrate under the standing cadence; broader
issue closure still requires its frozen criteria. No Xserver rewrite follows.
