# Scheduler ownership and fairness verification — 2026-10-04

A scheduled command used to remain in the schedule queue until its callback
returned. A callback entering `Wait` could reenter the scheduler and execute its
own occurrence again. The saved pre-fix Linux ARM64 binary exits with SIGSEGV in
the new scheduled-function regression (`wm_exit_status: -11`).

The scheduler now dequeues an occurrence before calling the command engine.
That callback owns its job until destruction or periodic rescheduling. Nested
waits and repeated scheduler calls share a 64-command allowance, replenished
only after a successful descriptor poll. An overdue remainder causes a zero
timeout poll rather than another unrestricted drain. Termination stops further
scheduled dispatch.

Execution checks the head of the existing sorted queue instead of scanning all
future jobs. Equal-deadline insertion order is unchanged; periodic deadlines
still start after execution finishes. XFlush moves to the batch boundary.
Schedule syntax, group cancellation and context lookup remain supported. This
does not replace the sorted queue with a heap.

## Verification matrix

| Environment | Full gate | Final affected checks | Default X server |
| --- | --- | --- | --- |
| Linux ARM64 GCC, Podman VM | 47/47 | 2/2 | Nested Xvfb/Xorg dummy |
| Linux ARM64 Clang/lld, Podman VM | 47/47 | 2/2 | Nested Xvfb/Xorg dummy |
| Linux AMD64 GCC, Podman emulation | 47/47 | 2/2 | Nested Xvfb/Xorg dummy |
| FreeBSD 15.1 ARM64, QEMU/HVF | 38/38 after harness correction | 2/2 | Xorg scfb passed |
| FreeBSD 15.1 AMD64, QEMU/TCG | 38/38 | 2/2 | Xorg scfb passed |
| OpenBSD 7.9 ARM64, QEMU/HVF | 38/38 | 2/2 | Xenocara wsfb passed |
| OpenBSD 7.9 AMD64, QEMU/TCG | 38/38 | 2/2 | Default Xenocara passed |

All gates include the current desktop, graphics fixes, compatibility,
interaction cancellation and earlier hardening work together. Builds and staged
installations succeed. Final affected checks rebuild the warning-free queue test
and run the corrected configure harness. The ARM64 GCC full gate preceded the
stronger backlog fixture; that final scheduler fixture passed separately there
and is included in the other full gates.

Native queue tests verify dequeue-before-callback, recursive budget sharing,
no refill between polls, future-head rejection, callback cancellation and
insertion. The new `scheduler-dispatch-xvfb` gate verifies:

- A scheduled complex function enters `Wait` exactly once; a newly scheduled
  timer executes during that wait, and the function resumes once its client maps.
- Group Deschedule removes both queued commands. Periodic execution stops after
  an external Deschedule and retains a delay after each occurrence.
- A Read fixture queues 256 timers, then stops the WM from inside PipeRead.
  Deadlines expire before the multiplexer resumes. All timers retain their
  order, and independent module input executes after 64 jobs. This result holds
  on both Linux architectures and all four default BSD servers.

## Configure harness correction

The initial FreeBSD ARM64 full run passed 37 checks and timed out in the existing
configure-merge test. It left the WM stopped. Stopping FVWM immediately after a
frame appears can freeze it while management still owns the X server grab,
blocking the test client's XSync.

The test client now acquires and synchronizes its own server grab before stopping
the WM and submitting the burst. It resumes the WM before releasing that grab.
This preserves the queued-request test while eliminating that ownership race.
The full FreeBSD ARM64 rerun passes 38/38, and final configure checks pass on all
six platforms and both ARM64 Linux compilers. Initial timeout evidence is kept.

## Evidence and reproduction

Raw evidence is under ignored `builddir/codex-2026-10-04/scheduler/`:
six platform archives, the Linux pre-fix crash record, default-server logs,
full-suite output, final affected-check logs, and `matrix-summary.json`.
`source-final.tgz` contains the final production/test snapshot. Fifteen source
fingerprints, including scheduler, event loop, tests, graphics and desktop files,
match the host independently in all six guest workspaces. GCC and Clang share
the verified ARM64 source workspace. Disposable QEMU snapshots preserve saved
disks; evidence was copied to the host before shutdown.

With live-X tests enabled:

```sh
meson test -C builddir event-loop-policy scheduler-dispatch-xvfb \
    configure-merge-xvfb --print-errorlogs
```

The scheduler script also accepts `--display` for an existing default server.
Linux and BSD full gates remain `tests/run_on_linux.sh` and
`tests/run_on_bsd.sh`.

## Remaining scope

This is a count budget, not an elapsed-time budget. One blocking command can
still delay other work. Virtualized gates establish behavior and ordering, not
physical desktop latency or GPU performance. Native Linux sanitizer CI remains
configured but unrun; no local sanitizer claim is made.

Move/Resize still defer timers and independent module commands during a held
drag. The scheduler fix is a prerequisite for INPUT-01, not its completion.
Window/resource lifetime, Recapture, command continuations and nested
interactions still need a coherent dispatch model before arbitrary commands
can run during those waits.
