# OUTPUT-03: bounded synchronous module startup

## Current compatibility authority

The quoted broad approval rule below records the policy at the time of this
historical decision. It is superseded by the user’s
[configuration-only compatibility scope](CONFIG-COMPATIBILITY-SCOPE-2026-10-07.md).
It creates no present approval hold for internal/runtime behavior alone.

Status: approved by the user on 2026-10-07; implementation and combined local qualification
complete. [Verification](../verification/VERIFICATION-2026-10-07-STARTUP-IDENTITY.md). This is a separate approval from OUTPUT-02 packet delivery/UNLOCK.

## Concrete observation

`CMD_ModuleSynchronous` initializes `sec = 0`. An omitted `Timeout` therefore
waits without a time limit for its expected startup response. `ModuleTimeout`
does not currently govern this command. A finite actual-core probe sets
`ModuleTimeout` to one second, launches an owned module that sends eight harmless
NOPs at 250-ms intervals and then its normal startup response, and measures
2.030 seconds before command completion. This is a timing observation, not a
compromise or a desktop-latency benchmark.

The explicit positive `Timeout` uses total elapsed wall-clock seconds, despite
the manual describing an inactivity timeout. The loop polls with a one-microsecond
select timeout and a one-millisecond sleep. Its retry loop repeats any select
error until termination, and input commands can run cleanup while it still holds
the producer pointer. These implementation/lifetime issues need focused checks
independently of a default-limit choice.

Probe sources, source fingerprints and private-core/server logs are under
`builddir/codex-2026-10-07/module-startup-deadline/`; the producer has a finite
run and no unbounded input stream. Guest evidence is under
`/tmp/fvwm-startup-deadline-code/evidence/`.

## Approved behavior

When `Timeout` is omitted, capture the current positive `ModuleTimeout` as the
startup barrier's total monotonic duration: 30 seconds by default. Preserve an
explicit positive `Timeout` override, `Expect` matching and rejection of zero or
negative explicit timeouts. Capture one operation-local deadline; do not reset it
on partial I/O, input commands, readiness or EINTR. Each nested operation owns its
deadline while its parent's time continues. Check before further work and after
commands return; one executing command cannot be preempted.

Use finite readiness waits up to 100 ms and the remaining deadline, with X
connection readiness for the existing Escape check. Preserve inline producer
command execution and startup/function ordering; other X events stay queued.
Do not add general modal command/timer dispatch in this change. Reuse the checked
nonblocking input reader and existing asynchronous output bounds. Stop retrying
terminal select errors. Pin the producer for the entire startup frame; stop using
it if input retires it, while preserving execution of its final continuation=0
command. Use existing death-row cleanup after unwinding.

On a startup timeout, leave a still-live module running asynchronously, as the
current explicit `Timeout` does. Returning from the startup barrier must not
introduce a new module-kill policy. Clock failure or regression ends the barrier
with a diagnostic; normal module transport and teardown retain their owners.
Success still waits for the requested response. Keep configured Escape handling.

## Compatibility decision and verification

An existing startup without an explicit timeout may take longer than the chosen
limit. It will now release the command barrier at that limit, so later startup
commands can run before that module finishes. A larger explicit positive
`Timeout` preserves slow startup ordering. This timing change requires the user's
explicit decision under [AGENTS.md](../../AGENTS.md): “A specific compatibility
break requires an explicit user decision.”

Cover omitted/explicit limits, matching responses, partial input,
activity, EINTR and terminal errors, Escape, termination, nested waits, producer
retirement/final-command lifetime and asynchronous continuation after timeout.
Check native BSD signals/descriptors early and use focused Linux/policy/sanitizer
checks during development. Qualify in the next coherent combined checkpoint.
