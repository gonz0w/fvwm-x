# INPUT-01: bounded active viewport interaction

## Current compatibility authority

The quoted broad approval rule below records the policy at the time of this
historical decision. It is superseded by the user’s
[configuration-only compatibility scope](CONFIG-COMPATIBILITY-SCOPE-2026-10-07.md).
It creates no present approval hold for internal/runtime behavior alone.

Status: approved by the user on 2026-10-06; implemented with eight focused Linux
entries passed; combined qualification pending. [Report](../verification/VERIFICATION-2026-10-06-VIEWPORT-SERVICE.md). This is a subsequent batch;
it is not part of the frozen common button-release qualification.

## Reproduced behavior

Run the approved common-release implementation and bind Mouse 1 on the root to
an immediate function with Echo, interactive Scroll, then a continuation Echo.
Install a safe 400-ms timer before starting Scroll and send an independent module
Echo after the pointer grab. During a 600-ms active hold, timer, module Echo and
function continuation are absent. Press Escape while keeping Mouse 1 down: the
new common release wait now services timer/module commands, and the continuation
remains behind physical release. After release the function continues.

The controlled baseline is in
`builddir/codex-2026-10-06/button-release-service/viewport_baseline.py`; logs are
in the isolated Linux development source's `viewport-baseline-evidence/`.
An initial marker-name collision was corrected before accepting the observation.
This uses ordinary XTest input and existing commands; no production code changes.
The final-wait 0.605-second CPU sample rounded to 0.00 CPU seconds at the available
process-counter resolution. It is functional evidence, not a benchmark.

## Proposed contract

Reuse the existing `fvwm_interaction` wait/cancellation owner and audited
`events_interaction_poll` service for `_drag_viewport`. The monitor remains
borrowed until outer unwind; no topology refresh, arbitrary mutating command,
image-owner publication or X lifecycle dispatch is added inside the interaction.

- Independent audited direct commands and safe timer heads progress during active
  interactive Scroll, with the existing producer/active-command/synchronous
  receiver barriers. Unsafe commands keep their order until the command returns.
- Use finite existing interaction readiness (maximum 100-ms quiet wait, shortened
  by safe timers and image deadlines). Termination or readiness failure ends the
  interaction and releases pointer/keyboard grabs through one cleanup path.
- Bound motion/enter/leave coalescing to the existing 32-event budget and retain
  physical release/key barriers. Correct the current `ButtonRelease` event-number
  typo to `ButtonReleaseMask` in the coalescing mask. Refuse zero/out-of-range
  button numbers before mask shifts.
- Preserve root-monitor selection, scrolling direction, viewport update arithmetic,
  keyboard shortcuts, required expose handling and the surrounding function's
  physical-release barrier. Keep the approved common final release wait.

This makes safe commands/timers visible during active viewport motion where they
previously waited for interaction completion. [AGENTS.md](../../AGENTS.md) requires
an explicit decision: “A specific compatibility break requires an explicit user
decision.” The user explicitly approved this active viewport contract after reviewing the
separate baseline.

## Acceptance batch

Implement readiness/cancellation, coalescing and audited service together. Add
native policy coverage where needed, then real-root Scroll/Reverse, keyboard and
physical release, continuous motion, independent/deferred producer order, timer
barriers, monitor events held until unwind, pending image transport and SIGTERM
checks. Use focused development gates and scoped sanitizers; one combined
Linux/BSD/default-X checkpoint qualifies the related batch. No new dependency,
thread, dispatcher or public command syntax.
