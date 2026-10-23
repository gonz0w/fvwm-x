# Runtime menu-opacity preview

Status: implemented contract; qualification belongs to the current checkpoint.
Owners: IPC-02 / CFG-01. User direction: continue related features toward safe
settings preview/recovery, with actual-X screenshots and batched testing.

## Bounded first owner

One fixed core-owned lease snapshots the exact prior opacity of an existing
named MenuStyle. Start checks the shared appearance generation and applies the
candidate atomically, on the authoritative thread. Only the scalar survives;
no borrowed menu/decor/image pointer or configuration command is retained.
Opacity is sampled at presentation; existing pixels may need menu reopening.

Typed MFL start/query/confirm/revert use the existing directed reply transport.
One native producer plus its incarnation cookie and connection/view identifies
the actor. A terminal lease is one slot, replaced by the next start; there is
no history/worker/service. Read-only refuses mutation, not core-owned cleanup.
Pending-wait cancellation never retracts a submitted start.

A monotonic deadline (1–30 seconds, default 15) restores on matching appearance
freshness without a consumer poll. Disconnect queues same-producer abandon;
module retirement recovers before freeing its record. A full queue still leaves
the core deadline responsible. Eligible ordinary/interaction service points
recover only the scalar; no required producer order, synchronous reply barrier,
borrowed face or lifetime boundary is bypassed. One blocking call/stalled server
cannot be preempted by this deadline.

Intervening tracked appearance activity conservatively refuses restore. It can
be unrelated activity; target deletion is never repaired by recreating it.
Callers inspect current policy and start again. No generic blind fallback or
rollback file is permitted. This is availability/conflict control, not an X11
security boundary.

## Native consumer and persistence limit

Desktop settings offers launcher-opacity choices and a native Form with Keep /
Revert. One bounded reply wait (8 KiB / 5 seconds) and one queued control keep
Revert precedence. Helper incarnation/view tokens retire stale callbacks. One
stale-start retry obtains a fresh generation, then still applies conditionally;
all further refusal remains explicit. Active status polls at most once a second.
Missing/invalid replies abandon and report uncertainty. Form controls continue
until the helper observes the result and retires its own dialog. Closing the
window leaves the core deadline responsible.

Keep changes runtime policy only. Saved preferences remain byte-for-byte
unchanged; a later theme update or restart can replace the value. General
saved-preference confirmation, owned image/resource snapshots and complete
appearance transactions remain open. Existing registry `preview: false` and
general appearance transaction flags stay truthful; MFL advertises the separate
`menu-opacity` runtime capability.

Configuration syntax, arguments, themes and bindings retain their contract.
This adds an internal module command and its generated installed Perl command
catalog entry. Native packet types/constants, dependencies, install paths and
release defaults are unchanged.
