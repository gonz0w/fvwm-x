# OUTPUT-02: one deadline for a synchronous module handshake

## Current compatibility authority

The quoted broad approval rule below records the policy at the time of this
historical decision. It is superseded by the user’s
[configuration-only compatibility scope](CONFIG-COMPATIBILITY-SCOPE-2026-10-07.md).
It creates no present approval hold for internal/runtime behavior alone.

Status: approved by the user and locally qualified on 2026-10-07. See the
[combined report](../verification/VERIFICATION-2026-10-07-MODULE-SYNC-TREE.md). This is separate from OUTPUT-01's approved asynchronous
service bounds and frozen qualification.

## Concrete observation

A bounded private-X fixture calls the real core `PositiveWrite` with a synchronous
mask and `moduleTimeout = 1`. An owned child sends eight harmless, correctly
framed NOP commands at 250-ms intervals, then the existing unlock response.
The core returns after 2.049 seconds, exceeding the configured one-second value.
The existing read loop resets its timeout after every command or incomplete
receive, and after EINTR. The synchronous writable wait also resets its timeout
after readiness or interruption. Activity can therefore extend the operation
without a total deadline. This reproduces timing behavior, not a compromise or
a measured desktop-latency improvement.

Inputs, runner and log are under
`builddir/codex-2026-10-06/module-sync-deadline/`; guest artifacts are
`/tmp/fvwm-button-release-source/module-sync-baseline-evidence/`. The write shim
accepts only one owned output endpoint so the observation isolates unlock timing;
ordinary writes and actual module framing/command execution retain their owner.
The child is explicitly reaped, and the private core/server have finite harness
cleanup. No production change was needed for this observation.

## Recommended contract

Keep the existing `ModuleTimeout` command, positive configurable override and
30-second default. Interpret its value as one monotonic deadline, captured when a
synchronous packet begins its output-completion/unlock barrier. Carry that deadline
through writable waits, partial writes, EINTR, partial input and commands while
waiting for UNLOCK. Do not refresh it because of progress or new commands.
A nested synchronous operation gets its own deadline; its caller's original
deadline continues to elapse. Retain packet ordering, unlock semantics, module
pinning/death-row ownership and termination handling. Clock/deadline failures
must fail closed with a useful diagnostic, rather than create an unbounded wait.

On expiry, retire the affected module using the existing lifecycle and return
through normal cleanup. Do not create a second scheduler or dispatch arbitrary
X events through this legacy barrier. Check the deadline before further work and
when commands return. This cannot preempt a single command that itself blocks;
Read/PipeRead and other blocking command owners remain separate audits. The
contract is a deadline on further handshake work, not a hard preemption guarantee.

## Compatibility and qualification

Modules that previously kept a handshake alive by sending commands or making
slow progress may now time out at the configured total duration. They can select
a larger positive `ModuleTimeout` when needed. This is a timing change beyond
OUTPUT-01's approval. [AGENTS.md](../../AGENTS.md) requires: “A specific
compatibility break requires an explicit user decision.” The user approved this timing change on 2026-10-07.

Implemented coverage includes pure monotonic boundary/overflow/clock checks and
actual-core checks for idle timeout, activity without unlock, partial input/output,
EINTR, nested synchronization, timely unlock, disconnect and termination. Keep
ordinary asynchronous budgets unchanged. Use focused Linux/BSD checks during the
batch, then one combined checkpoint with the independent tree-query ownership
repair and other compatible work. Native Linux release CI remains separate.
