# SVG completion in the core event loop — approved

## Current compatibility authority

The quoted broad approval rule below records the policy at the time of this
historical decision. It is superseded by the user’s
[configuration-only compatibility scope](CONFIG-COMPATIBILITY-SCOPE-2026-10-07.md).
It creates no present approval hold for internal/runtime behavior alone.

Owner: IMAGE-01 / INPUT-01. Status: approved on 2026-10-06; implementation and
qualification in progress. The user approved asynchronous core completion with
previous-image retention and existing absent-image first-load behavior. The
accepted isolated decoder and its finite limits remain in production.

## Current behavior and constraint

`PImageLoadSvg` calls `image_svg_load`, which calls `image_process_run` until the
worker exits, fails, is cancelled or reaches its five-second total deadline.
This owns a finite worker and validates typed output, but its caller waits
synchronously. When the caller is the core, ordinary window-manager input and
command service cannot continue through that wait. The accepted SVG report
explicitly leaves this responsiveness work open; isolation alone does not make
the parent asynchronous.

The shared loader returns actual pixels and dimensions to callers immediately.
Changing the core to return while an SVG is pending therefore changes observable
completion and first-load geometry. Dispatching arbitrary X handlers or commands
inside the current wait would re-enter mutable resource/command owners and does
not satisfy the single-threaded ownership contract.

## Approved observable contract

- Permit core SVG requests to finish asynchronously. Configuration syntax,
  logical command order and existing decoder size, deadline and memory limits
  stay unchanged. A command can finish before its SVG pixels are available.
- Keep the previous usable picture until the matching replacement succeeds.
  When there is no previous picture, use the component's existing absent-image
  rendering until completion. Do not introduce a new branded placeholder.
- Commit validated results and recalculate affected geometry through their
  existing owners on the main thread. Coalesce redraw/update work. Never apply
  a result from a superseded configuration, destroyed owner or old monitor/scale
  generation; cancel or discard it with owned cleanup.
- Keep an open menu or active interaction's layout/resource snapshot stable.
  Defer its replacement/geometry commit until that owner releases the snapshot.
  A failed replacement keeps the previous usable picture and produces the
  existing diagnostic. A failed first load keeps absent-image behavior.
- Keep standalone utilities' bounded synchronous behavior. Module adoption can
  follow each module's readiness/lifetime gate; this first batch is the core.

This changes themes or command sequences that assume a newly requested SVG is
already painted, or has established its final dimensions, when the command
returns. AGENTS.md says, “A specific compatibility break requires an explicit
user decision.” The approved five-second decoder limit does not authorize this
completion change. The user subsequently approved this contract on 2026-10-06.

## Concrete implementation boundaries and gates

Split the existing literal process transport into an owned start/step/cancel
state machine. Register its descriptors and monotonic deadline with the core's
normal readiness loop. Keep authoritative X state single-threaded and avoid
recursive dispatch, nested decoder waits and new mandatory libraries.

Start with one active decoder worker in the core. Acquire source/output buffers
only for the active request, coalesce equivalent pending cache requests, and
bound scheduler work per turn. Do not allocate a full image buffer for every
pending theme item. The existing cache/request population and broader storage
budgets still require measurement and review; this proposal does not silently
approve a new theme-count cap or claim to sandbox filesystem access.

Audit every core SVG resource owner and its current synchronous size/layout
assumptions before adoption. Preserve process cleanup, input snapshots, packet
validation and stricter inherited worker limits. Tests must cover a slow decode
while independent input/commands/timers continue, successful and refused first
loads/replacements, stale generations, owner destruction/XID reuse, shutdown,
open menus and active interactions. Qualify full Linux/BSD suites, exact staged
installs, default BSD X and scoped sanitizers. Native Linux release CI and
physical hardware remain separate gates. Primary acquisition/storage latency,
other image formats and broader font/modal work remain open.
