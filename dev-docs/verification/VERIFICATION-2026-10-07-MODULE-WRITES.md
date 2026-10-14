# Native module write progress and normal-flow cleanup

Status: three functional stories pass focused normal/scoped sanitizer and both
native BSD ARM64 controls. A fourth dead-code cleanup compiles on those profiles
without repeating functional tests. Combined qualification remains pending after
client1351.
Existing FVWM configuration files remain usable without migration.

The batch combines checked stack command construction, interrupted/partial
write progress and FvwmRearrange signal cleanup. SendText retains its void API
and native wire layout. Its existing 1000-byte core limit is checked before
emission. Positive writes advance only the remaining frame; nonterminal EINTR
retries preserve those bytes. A zero/error/incomplete write requests termination,
preventing later frames from following a partial frame. The module borrows its
pipe endpoint and never closes it in this helper.

fvwmWrite reuses the existing atomic termination mechanism, arming its jump only
across the write syscall. It does not introduce a deadline or change blocking
module-pipe behavior into asynchronous core work. Ordinary module writes can
still wait for a reader; termination now cancels that wait. SendText has no core
callers. This batch does not claim a finite normal-write deadline or complete
backpressure policy for all producers.

FvwmRearrange uses the common termination handler for SIGPIPE/SIGALRM and orderly
SIGTERM/SIGINT. It leaves configuration/packet reads and performs memory/console
cleanup in normal flow. The active ten-second alarm is preserved and cancelled
on return. Layout arithmetic and sorting are unchanged. Full core desktop/layout
qualification remains part of the combined checkpoint.

Affected normal and scoped sanitizer profiles each pass three pure and three
actual-process/core entries. Pure checks use leak detection; actual process/core
runs disable it. The command unit covers eight lengths, exact native wire bytes,
zero construction allocations, two interruption offsets, fragmented progress,
partial failure, EAGAIN, zero write, suppression of subsequent output and actual
owned full-pipe termination with the borrowed descriptor still open.

The actual FvwmRearrange source is linked with test-only ownership observations.
Ten controls cover configuration/active reads, SIGTERM/SIGINT/SIGPIPE/SIGALRM,
ordinary EOF and full outgoing pipes. The probe checks the installed handlers,
original alarm, exactly one owned-console cleanup and zero exit status. The driver
owns all pipes, module children and nested servers. Both native BSD ARM64 overlays
pass the command unit and all ten controls. Three archives validate all payload
hashes/modes and compressed EOF: 12 Linux, 43 FreeBSD and 44 OpenBSD files.

Frozen source: 1354 files, eight owned paths from client1351, including three
new regression files. SHA-256:
`a25a102ceeeb0467565904dfa2ea5354436dc2bee7822434ed34e770504a1d79`.
Evidence: `builddir/codex-2026-10-07/module-write-production`.
The earlier construction-only [snapshot](VERIFICATION-2026-10-07-MODULE-COMMAND.md)
and cleanup-only snapshot remain retained. Initial cleanup-runner import and
Linux fopen64 instrumentation failures are preserved under
`builddir/codex-2026-10-07/rearrange-signal-production`; both were test corrections,
not production failures. The final cleanup-only normal/scoped 3+3 passes precede
the separately frozen write dependency. No unsafe prior signal handler was run.

No native release sanitizer CI, physical desktop, toolkit or compositor result
is claimed. Remaining owners include other module signal handlers, wider
readiness/backpressure policy and complete module resource teardown.

Final four-story snapshot: 1354 files/eight paths, SHA-256
`2bbe7ddc3b0ca2bb6ddf151b8b1a06b743e34cdb17cad67a520d48d955eaf6de`.
It removes only Rearrange's unused fd_width, GetFdWidth call and associated
includes from the preceding focused-passing a25a snapshot. Normal/scoped and
both native BSD ARM64 builds compile/link; functional tests were not repeated.
The initial native compile-only runner printed a generic functional-pass label;
its --compile-only branch did not execute those tests. The runner label is now
corrected and original logs retained. Earlier functional archives remain paired
with their original source under before-unused-width-cleanup.

Ten final combined runners pass Python preflight. Required future totals are
1368 standard entries/eight profiles, 120 staged identities, 136 default BSD X
driver invocations, optional 176/156 and 59 scoped checks (24 pure/35 actual
process/core). These are requirements, not results. Sync requires the preceding
client source fingerprints; full dispatch waits for that checkpoint's review.
