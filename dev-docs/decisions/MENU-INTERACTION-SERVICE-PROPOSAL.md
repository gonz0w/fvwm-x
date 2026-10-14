# INPUT-01: bounded active menu servicing

## Current compatibility authority

The quoted broad approval rule below records the policy at the time of this
historical decision. It is superseded by the user’s
[configuration-only compatibility scope](CONFIG-COMPATIBILITY-SCOPE-2026-10-07.md).
It creates no present approval hold for internal/runtime behavior alone.

Status: approved by the user on 2026-10-06; implemented and locally qualified; see the [menu report](../verification/VERIFICATION-2026-10-06-MENU-SERVICE.md). No menu production
change is included in the separately frozen viewport checkpoint.

## Reproduced behavior

On the viewport implementation, bind Mouse 1 on the root to an immediate
function: Echo, Popup BaselineMenu, then a continuation Echo. Install a safe
400-ms timer before opening the menu and send an independent module Echo after
the pointer grab. During a 600-ms active hold neither runs. Press Escape while
keeping Mouse 1 down: the qualified common final release wait services them,
but function continuation still waits for physical release.

The read-only baseline is
`builddir/codex-2026-10-06/viewport-service/menu_baseline.py`; logs are in the
isolated development source's `menu-baseline-evidence/`. It uses ordinary
commands and XTest input. The final-wait CPU sample rounds to zero at the
available counter resolution and is functional evidence, not a benchmark.

Source review finds three related paths: `_mloop_get_event` blocks in
`FMaskEvent`; `_mloop_get_event_timeout_loop` sleeps in 10-ms increments and
counts loop iterations for submenu deadlines; motion coalescing is unlimited.
Adding descriptor readiness alone would let module wakeups accelerate those
counters. The whole timing/servicing contract must therefore be handled together.

## Proposed contract

Reuse the existing audited interaction service and cancellation owner for active
menus, including submenu waits. Safe independent module commands and safe timer
heads can run while a menu remains open. Active producer continuations, unsafe
heads, synchronous receiver replies and physical-release continuation preserve
the existing qualified barriers. No arbitrary style/menu/resource mutation,
monitor refresh, general lifecycle dispatch or image publication is permitted
inside this service.

Use maximum 100-ms quiet readiness, shortened by existing safe timer/image
transport deadlines and submenu timing. Bound motion coalescing to 32 events,
retaining release/key barriers and existing menu-specific exposure/tear-off close
handling. Termination/readiness failure cancels through existing menu cleanup;
physical release retains the common final wait, with termination cancellation.

Keep configured popup/popdown delays and their current 10-ms policy, but count
monotonic elapsed ticks rather than descriptor wakeups or CPU loop iterations.
Independent commands must not accelerate opening/closing a submenu. Preserve
selection, dynamic menu actions, keyboard/mouse navigation, tear-off semantics
and configured cursor behavior. Introduce no timeout for a legitimately open menu.

This changes when safe commands/timers become visible during an active menu and
how submenu timing resists early wakeups. [AGENTS.md](../../AGENTS.md) requires
an explicit decision: “A specific compatibility break requires an explicit user
decision.” Prior common-release and viewport approvals cover different active
owners and do not supply this menu timing exception.

## Acceptance batch

Implement readiness/cancellation, motion fairness, monotonic submenu ticks and
audited servicing together. Add native timer-policy boundary tests plus real
root/client menus, submenus and configured delay checks under descriptor wakeups,
held/released Escape, selection/Return, dynamic actions, tear-offs, continuous
motion, safe/unsafe producer order, image transport/ownership and termination.
Use focused checks, scoped sanitizers and one combined Linux/BSD/default-server
checkpoint. Borrowed menu/window resource lifetimes remain explicit; broaden
mutation permission only through a separate ownership contract.
