# INPUT-01: bounded button-release servicing

## Current compatibility authority

The quoted broad approval rule below records the policy at the time of this
historical decision. It is superseded by the user’s
[configuration-only compatibility scope](CONFIG-COMPATIBILITY-SCOPE-2026-10-07.md).
It creates no present approval hold for internal/runtime behavior alone.

Status: approved by the user on 2026-10-06; implemented; focused checks passed, combined local qualification accepted. Owner: INPUT-01,
with existing LOOP-01 readiness and CLEAN-01 policy helpers.

The image activation checkpoint has a separate frozen source and qualification.
This proposal changes the next input batch only.

## Reproduced behavior

On the qualified Linux ARM64 GCC image-activation binary, start a Move through
Mouse 1, press Escape and keep the mouse button down. The final
`WaitForButtonsUp` loop repeatedly queries the pointer and sleeps one microsecond.
A previously scheduled safe timer and an independent module Echo do not run
until the button is physically released. The enclosing function also waits.

In one controlled 0.603-second observation, timer, module Echo and function
continuation were all absent during the hold; all resumed after release.
The WM used 0.08 CPU seconds in that interval. This is a local functional
reproducer and CPU sample, not a general performance benchmark.

Reproducer and logs:
`builddir/codex-2026-10-06/svg-core-activation/button_release_baseline.py` and
`button-release-baseline-evidence/`. The baseline uses normal XTest input,
Escape and existing FVWM commands. No production code is modified.

## Proposed contract

- Reuse the existing bounded interaction transport/command/timer pump for
  button-release waits, including the common menu, focus and paging callers.
  Preserve the current audited direct builtin policy, producer heads, active
  command leases, synchronous-receiver restrictions and unsafe timer barriers.
- Keep the enclosing function and its producer's queued continuation behind the
  physical release barrier. Arbitrary window/style/colorset mutations, topology
  refresh and ordinary X lifecycle dispatch stay at their existing outer boundary.
- Service asynchronous image transport/deadlines without committing image owners
  or starting another nested interaction inside the wait.
- Replace spinning with a finite readiness wait and a pointer-state fallback at
  most once per 20 milliseconds. Preserve selected release/key/expose handling,
  grab ownership and cancellation; do not introduce a deadline that ends a
  legitimately held button.
- Show the existing wait cursor after 20 milliseconds, using monotonic elapsed
  time instead of 20 loop iterations. Preserve configured cursor/BusyCursor policy.
- Refuse malformed button numbers before shift arithmetic. This defensive
  correction does not require a compatibility exception for valid input.

This extends the approved held-drag ordering exception into the common final
button-release wait. Safe independent commands/timers can become visible before
release in contexts that previously blocked them; wait-cursor timing also becomes
explicit. [AGENTS.md](../../AGENTS.md) requires this decision: “A specific
compatibility break requires an explicit user decision.” It is not a new approval
request for the already accepted Move/Resize servicing contract.

## Acceptance batch

Implement release-state policy, shared readiness, cancellation and final-wait
service as one related INPUT-01 batch. Cover invalid/valid button state,
Escape with a held button, ordinary release, unsafe/per-producer ordering,
continuous input, exposes, termination and pending image work. Use focused
native policy/nested-X and scoped sanitizer checks during implementation; run the
combined Linux/BSD/default-server gates once for the batch. No new dependency,
thread, dispatcher, privileged service or arbitrary reentrant command path.

Local acceptance: eight profiles/924 entries, 120 code identities, all four
BSD default X servers (20 driver invocations), optional 120/102 entries and
five scoped sanitizer checks. [Report](../verification/VERIFICATION-2026-10-06-BUTTON-RELEASE.md). Native release CI remains unrun.
