# Synchronous module readiness — 2026-10-15

Owner: LOOP-01 [#36](https://github.com/gonz0w/fvwm-x/issues/36).
Status: implemented; focused Linux/BSD ARM64 checks pass. Combined qualification
is running. PR #96's ordinary/interactive checkpoint is merged; OpenBSD/VESA
cause investigation #93 remains deferred.

## Problem and behavior

The three remaining synchronous waits in `module_list.c` built descriptor sets
for an unlock response, a blocked output pipe and module startup. They now use
the existing cancellation-aware `fvwmPoll` wrapper and native descriptor arrays.
A shared pure deadline adapter rounds a positive remainder upward to milliseconds
and clamps it at `INT_MAX`; expiration and invalid/regressing clocks initialize
the result to zero. A clamped poll interval ending before the absolute deadline
continues the wait. Every retry keeps its original deadline.

Lock-on-send retains its output/unlock completion barrier, inline command
execution, recursive frame lifetime, death-row pins and retire-on-timeout policy.
Input readiness includes EOF/error notifications so buffered complete frames can
be consumed before EOF. Invalid input, invalid output and output hangup fail
through the existing retirement path. Startup retains read-before-write order,
finite 100 ms X/Escape checks, asynchronous output budgets, custom Expect,
partial-frame continuation and the live module after startup timeout or terminal
readiness failure. The syscall errno is saved before clock/remaining helpers.
No session callbacks or ordinary event dispatch are added to these barriers.

No configuration syntax, packet ABI, installed option or dependency changes.
High-descriptor controls cover these waits where the inherited process limit
permits real endpoints; module constructor and ICE/session admission limits
elsewhere remain unchanged. This is a readiness consolidation, with no measured
desktop-latency or whole-transport high-descriptor claim.

## Regression coverage

The native `event-loop-policy` unit covers positive rounding, submillisecond
remainders, expiration, maximum clamp, and invalid/regressing clocks. Existing
actual-core synchronous and startup fixtures now intercept native poll arrays,
retaining their deadline, partial read/write, interruption, activity, recursion,
command ordering, retirement, buffered Escape and asynchronous handoff controls.
They add real high endpoints where available, invalid endpoints, output hangup,
and complete buffered responses followed by real pipe EOF. Real child producers
still exercise elapsed deadlines and startup continuation on actual clocks.

The fixtures run actual core objects against X and pipes, with controlled
transport/clock callbacks for deterministic cases. Their controlled `_exit`
completion does not qualify full shutdown leaks. Drivers support borrowed
private-authority default displays as well as their own nested X server.

## Qualification

Four target archives are strictly validated: Linux/FreeBSD/OpenBSD ARM64 and
FreeBSD AMD64. Linux ARM64 passes all four full profiles, all final affected
checks and three scoped sanitizer entries. Both FreeBSD targets and OpenBSD
ARM64 pass full/final affected/install/default-X gates. Linux AMD64 GCC passes
its full suite and install; Clang compilation and OpenBSD AMD64's full suite are
still running. The branch remains a draft until those remaining receipts pass.

Full-suite source: `8cf67ca691c1c55c551de0403ffa15a1cc9059db057aee56b5af9544235cebaf`.
Final source: `9ed48f0c34d9d1bf9747b5a3589927b8e706b748059e63e676507672ae02864e`.
The final three-file timeout refinement has its own affected checks; unaffected
full-suite evidence remains bound to its original source. Full results,
installed identities and default-server outcomes will be finalized after all
artifact validation passes.
Focused final logic (`focus04`) passes the three registered policy/core entries
on Linux ARM64, FreeBSD ARM64 and OpenBSD ARM64. The combined candidate includes
subsequent whitespace/comment line wrapping and will be independently checked
as the exact final source. All failed development attempts remain local.

The first generated focus command used a stale target name. The second build
caught a duplicated adapter block returning a value from a void test function;
the test edit was corrected before runtime checks passed. The existing Linux
Podman machine was unavailable until restarted inside a persistent control
session. These are retained setup/test-adapter failures, not passing evidence.
No production source or test deadline was relaxed to recover from them.

Native-CPU release sanitizers, physical input/GPU/toolkit/compositor acceptance,
external session-manager behavior and broader descriptor/runtime audits remain
separate. No #93 cause investigation, hosted CI, remote status publisher, release
or upstream notification is added.

Local source manifests, exact argv, logs and archived receipts are under
`builddir/codex-2026-10-15/module-wait-readiness/` (ignored).
