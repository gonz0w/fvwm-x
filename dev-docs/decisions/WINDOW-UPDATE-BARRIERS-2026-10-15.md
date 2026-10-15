# Window update ownership and ordering barriers

Owner: DRAW-01 [#25](https://github.com/gonz0w/fvwm-x/issues/25).
This records the existing core contract for subsequent STYLE-01/STATE-01 work.
It does not introduce deferred work or change configuration syntax.
[Boundary verification](../verification/VERIFICATION-2026-10-15-WINDOW-UPDATES.md)
records passing affected checks and the two unrelated full-suite timing gaps.

## Owners and visibility

The core thread owns `Scr.FvwmRoot`, the stack ring, style/decor lists and their
change flags. `Scr.flags.do_need_window_update` schedules a global style pass;
it is not an expose queue, an X transport flush flag or a per-window dirty map.
Style masks, decor flags, default-font, mouse-binding and button-count flags
retain the reasons for the pass. `check_window_style_change` derives an
operation-local `update_win` plan; `apply_window_updates` consumes it before
`flush_window_updates` resets the shared change flags.

Style commands can change rules while existing managed windows retain their
previous applied state. Subsequent commands in the same function must not be
assumed to see those new styles without an explicit update command. New-window
style lookup and direct window operations have their own synchronous owners;
this contract does not postpone them behind a new queue.

| Boundary | Existing owner and required order |
| --- | --- |
| Startup defaults | `fvwm3.c` applies defaults with `flush_window_updates`, then simplifies the style list before user configuration. This unconditional initialization pass is intentional. |
| Ordinary event-loop turn | `HandleEvents` processes deferred signals/topology, scheduled destruction, then asynchronous image publication. Pending global window updates commit before module configuration continuations, stack publication, module output polling and the next ordinary event dispatch. Style-list simplification follows that turn's dispatch. This boundary does not require the input queues to become empty. |
| `UpdateStyles` | If global updates are pending, apply them before the command returns. A clean repeat does not invoke a global pass. This is an application barrier, not proof that all resulting Expose events have been painted. |
| `Refresh` / `RefreshWindow` | `refresh_window(..., True)` maps its temporary cover, commits **all** pending global style changes, destroys the cover, retains `XSync` and services a bounded expose batch. `RefreshWindow` selects the redraw surface, not the scope of the pending style pass. |
| Direct decor/window changes | `apply_decor_change` and direct geometry/stack/property owners retain their existing immediate effects and publications. A future optimization must audit each caller rather than silently redirecting it to the global pass. |
| Interaction and synchronous module waits | Readiness servicing can occur while commands borrow resources. These waits are not ordinary outer-loop commit points. Preserve their audited command set, inline ordering and existing recursive/death-row lifetime rules. |

The global pass keeps its pointer/server grabs, focus deletion/restoration,
default resource preparation, per-window style application and final flag reset
in their current order. The server grab does not make this a rollback transaction
or eliminate synchronous module callbacks. Any future precomputed plan must
revalidate its window/resource identity after callbacks before using borrowed
state; it must not clear invalidation created by recursive work accidentally.

## Retirement and resources

`destroy_window` removes list/stack membership and client identity before delayed
record reclamation. Complex/menu functions can retain the scheduled record until
their borrowed command state unwinds. `destroy_scheduled_windows` refuses to free
it during those functions; the next ordinary turn reclaims scheduled records
before applying pending styles to the surviving managed list. New deferred plans
must not retain a raw pointer beyond the owner's lifetime or independently free
a death-row record.

Fonts, pictures and asynchronous image commits retain their existing owner,
scale/cache generation and prepare-before-release contracts. A window update
must not substitute an old generation merely because its geometry is unchanged.
Expose service resolves current X contexts, merges bounded same-surface damage
and preserves foreign-event order. Retiring one surface must not consume another
surface's pending damage. See the [expose service contract](EXPOSE-SERVICE-PROPOSAL.md).

## Queries, publications and future coalescing

`XFlush` sends buffered X requests; it does not apply pending FVWM styles.
`XSync` additionally waits for server processing; neither drains the core's
bounded redraw backlog. Keep existing synchronization/error barriers until an
owning measurement and regression test justify their removal.

EWMH client/stacking lists, work areas and module packets describe actual current
pixel geometry/state at their existing publication points. `EWMH_GetWorkAreaIntersection`
refreshes its work-area owner before using it. No generic style flag may suppress
these publications or turn a current-state query into a stale-cache read.
STACK-01's queued restack publication retains its own pinned identity and flush
contract; it is not permission to coalesce unrelated EWMH/module state.

STYLE-01/STATE-01 should first measure the owning traversal, matching, grab and
publication costs. A proposed dirty domain must identify its invalidators,
authoritative owner, bounded lifetime, commit point and observable reads.
Coalescing is allowed only within a proven unobserved interval: commands,
module callbacks/packets, X property publication and queries between changes can
require intermediate state. Repeated setters alone do not prove redundancy.
Keep wildcard/rule precedence, command ordering, final geometry, struts,
monitor state and teardown. Do not create a second authoritative window model.

## Executable boundary coverage

`window-update-xvfb` links the actual core objects. It creates two managed X
clients, changes rules through the command engine and checks both applied
`boundary_width` and server-side frame/client geometry plus `_NET_FRAME_EXTENTS`.
It covers deferred application, explicit/clean update commands, Refresh and
RefreshWindow's global style pass, delayed record reclamation and final ordered
style application at the real outer loop's pre-module continuation boundary.
Only startup capture and that observation point are wrapped; style, destruction,
geometry and X property operations execute normally.

This fixture does not qualify every dirty domain, reentrant callback path,
mixed-DPI resource transition or external observer interleaving. Existing expose,
image, interaction, module and stack regressions retain those separate scopes.
The contract enables measured proposals; it does not close the broader DRAW-01,
STYLE-01 or STATE-01 audits or claim a performance improvement.
