# DRAW-01 / INPUT-01: correct and bound synchronous redraw servicing

## Current compatibility authority

The quoted broad approval rule below records the policy at the time of this
historical decision. It is superseded by the user’s
[configuration-only compatibility scope](CONFIG-COMPATIBILITY-SCOPE-2026-10-07.md).
It creates no present approval hold for internal/runtime behavior alone.

Status: approved by the user on 2026-10-07; implementation and combined local
qualification with atom batching/icon identity pass. See the
[qualification report](../verification/VERIFICATION-2026-10-07-ATOM-EXPOSE.md).

## Concrete observation

The actual frozen JSON-checkpoint core queues 1000 one-pixel Expose records for
its own private NoFocus window, plus one PropertyNotify marker. It calls the
production handle_all_expose with a link observer around the existing library
callback. The helper selects and invalidates all 1000 Expose records, calls the
provided redraw handler zero times, and retains the non-expose marker. This is a
finite queue/control observation, not an exploit or desktop-latency benchmark.

The source explains this result. The only production caller of
FWeedAndHandleIfEvents supplies a predicate returning 1 for Expose. The library
invalidates matching events, but calls its handler only if the predicate stops
with return bit 2. It therefore discards the selected redraws. handle_all_expose
is called after raising windows, before Read executes commands, and after
Refresh's temporary window is destroyed. Normal HandleExpose also scans and
merges matching-window exposures without a service-work cap.

Sources/logs for the bounded actual-core control are under
`builddir/codex-2026-10-07/expose-drain/`. Initial fixture compile errors caused by
using raw Xlib names instead of FVWM's event wrappers are retained. The corrected
control uses the existing wrappers and passes against the exact accepted core.

## Recommended contract

Correct synchronous expose servicing so selected redraw events reach the existing
Expose dispatcher, resolving current window identity for each dispatch. Preserve
non-expose events and their FIFO order. Keep the caller's saved event/timestamp
state across the helper and reuse normal icon/tear-off-menu drawing and lifetime
owners. Do not dispatch arbitrary configuration, module commands or lifecycle
events from the redraw helper.

Bound each synchronous service pass and each matching-window merge to 32 queued
records. Inspect only a finite queued snapshot; do not repeatedly fetch new
socket input while trying to finish the redraw backlog. Leave deferred events
for normal dispatch, preserving damage coverage and other-window order. Use
operation-local state rather than a competing global drawing owner.

A nested merge has its own finite bound: a service pass can invoke merges while
handling its selected events. Do not claim that a count budget preempts a slow
render or a single Xlib call, or that it is an end-to-end latency guarantee.

Existing ordinary small queues complete before the helper returns. For larger
queues, a Read, Raise or Refresh command can return while redraw events remain
pending; normal event dispatch completes them. This is a specific presentation-
timing change, so [AGENTS.md](../../AGENTS.md) requires the user's decision:
“A specific compatibility break requires an explicit user decision.”

The alternative is to correct handler delivery while preserving the full
queued-redraw barrier. That retains the old intended completion contract, but
one helper may do all queued rendering before other safe work gets a turn.

## Acceptance checks

Keep the actual baseline. Check handler delivery, final merged rectangles,
multiple windows, non-expose ordering, finite snapshots, reentrant callbacks,
window destruction, queue continuation and saved event/timestamp restoration.
Use pure policy tests and actual icon/tear-off-menu/Read/Refresh/Raise checks on
nested Linux X, scoped sanitizers and early BSD. Qualify with the atom-batching
work in one next combined checkpoint. Do not infer completion from a private
counter fixture alone. Broader dirty-state/coalescing and barrier removal remain
separate DRAW-01 work.
