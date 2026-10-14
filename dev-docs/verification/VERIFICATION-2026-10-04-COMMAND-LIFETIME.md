# Command ownership and module backpressure — 2026-10-04

Cloned command contexts now own a copy of their triggering X event. Previously,
the clone's event pointer still referred into its parent, even though the clone
contained storage for that event. The native regression checks independent
snapshots, nested clones, explicit overrides and use after parent destruction.
Restoring the old implementation fails the pointer-ownership assertion with
SIGABRT. This establishes an ownership defect; it is not evidence of a previously
reproduced desktop crash.

Core module input now admits at most 256 queued commands per producer. A full
producer is omitted from normal read polling until one of its commands starts
execution. Unread bytes remain in its pipe; commands are neither dropped nor
reordered by the queue. Other producers, output writes and synchronous direct
reads retain their existing paths. The limit is documented in the Module manual.

Queue capacity and module lifetime are separate: execution releases the queue
slot before calling the command engine, while the active input retains its
existing module reference until disposal. Discarding an unexecuted input also
releases its slot. This keeps nested waits and synchronous handshakes from
depending on capacity held by the command currently executing.

## Verification matrix

| Environment | Full gate | Final affected checks | Default X server |
| --- | --- | --- | --- |
| Linux ARM64 GCC, Podman VM | 48/48 before adding the stress gate | 5/5 wrapper consumers, then strengthened stress 1/1 | Nested Xvfb/Xorg dummy |
| Linux ARM64 Clang/lld, Podman VM | 49/49 | Strengthened stress 1/1 | Nested Xvfb/Xorg dummy |
| Linux AMD64 GCC, Podman emulation | 49/49 | Final stress included in full gate | Nested Xvfb/Xorg dummy |
| FreeBSD 15.1 ARM64, QEMU/HVF | 39/39 | Compatibility and scheduler passed | Xorg scfb |
| FreeBSD 15.1 AMD64, QEMU/TCG | 39/39 | Compatibility and scheduler passed | Xorg scfb |
| OpenBSD 7.9 ARM64, QEMU/HVF | 39/39 | Compatibility and scheduler passed | Xenocara wsfb, zero-monitor fallback |
| OpenBSD 7.9 AMD64, QEMU/TCG | 39/39 | Compatibility and scheduler passed | Default Xenocara |

All builds and staged installs succeed. The native real-pipe regression exercises
the production 256-command limit, retained unread input, another producer,
idempotent dequeue/discard, direct
unlock reads at capacity, and module retirement while queued inputs still pin
their owner. The context ownership test is native and needs no X server.

The Linux-only `module-backpressure-xvfb` test uses a verification binary with
linker wrappers reducing admission to 16 commands and dispatch to eight. The
production limits remain 256 and 64. Smaller test limits let 17 real module
processes establish saturation without hundreds of processes. These modules
submit 8,704 commands and produce 544 ordered progress markers. The test waits
for observed read suspension before sending independent input, verifies it
executes before the burst finishes, and checks synchronous startup, unlock and
healthy recovery. Both final ARM64 runs observe independent input after 26
markers and AMD64 after 34; these are observations, not promised scheduling
ratios.

A Linux ARM64 control linked with the old event-reading implementation fails
the strengthened test with `producer limit never paused reads`. This restores
the old polling/admission path in the verification binary; it is not a complete
historical build. The existing root-fallback/fault-injection consumers of the
wrapped binary pass their affected follow-up checks.

BSD default-server checks preserve Read/PipeRead ordering, theme reload,
decorations, complex functions, real menu/key/mouse bindings, inherited
descriptors, scheduled nested Wait, timer cancellation and periodic stop,
256-timer ordering, and independent module servicing after 64 timers.

## Evidence and reproduction

Evidence is in ignored `builddir/codex-2026-10-04/command-lifetime/`: platform
archives, full and affected-test output, default-server logs, negative-control
sources/scripts and `matrix-summary.json`. `source-final.tgz` freezes the
previous scheduler snapshot plus this batch's changed files. It includes the
earlier Slate/graphics work but excludes subsequent appearance/Picom changes
being made separately in the shared checkout. The AMD64 archive records one
omission: an inaccessible empty X-server log left by an October 3 run, outside
this batch's evidence. Current test output and artifacts are retained.

Fifteen changed source files are fingerprinted independently against the host
in each guest workspace. ARM64 GCC and Clang share one source workspace. The
final three Linux-only fixture files were also copied to BSD after its gates;
they are not compiled or run there. Evidence is saved before the disposable
QEMU guests shut down. Builds use staged installs, not host desktop installs.

With Linux live-X tests enabled:

```sh
meson test -C builddir execcontext-ownership module-runtime \
    module-backpressure-xvfb --print-errorlogs
```

The full Linux and BSD harnesses remain `tests/run_on_linux.sh` and
`tests/run_on_bsd.sh`. Local verification uses no sanitizers; native Linux
sanitizer CI is configured but remains unrun.

## Remaining scope

This is a per-producer backlog limit, not a global module/process limit. A single
blocking command can still delay the core. Borrowed window/module pointers and
the live last-event pointer in a context retain their existing lifetime rules;
the event copy does not make arbitrary asynchronous context retention safe.

Move/Resize still defer timers and independent module commands during a held
drag. INPUT-01 still needs window/resource lifetime, continuation routing,
Recapture/style mutation and nested-interaction handling. Allowing safe commands
to pass another module's deferred command requires a specific decision about
ordering between modules; this batch retains the shared FIFO.
