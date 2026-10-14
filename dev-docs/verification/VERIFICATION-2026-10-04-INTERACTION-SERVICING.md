# Held-drag servicing — 2026-10-04

Move and Resize now service bounded module transport, audited direct commands
and eligible timers while the pointer is held, including during continuous
motion and wireframe server grabs. The user approved the specific ordering
exception: a safe command from an independent module may pass another module's
deferred command. Each producer's order, including its active command's
complex-function continuation, remains intact. Normal dispatch retains the
shared FIFO.

## Implementation and compatibility

The core input queue uses intrusive global and per-producer links, removing its
separate queue-node allocation. A drag can take only a producer's first queued
command, and cannot take from a producer with an active command. Inputs leave
the queue before execution; their existing references retain module lifetime.
The production 256-command admission limit and pipe wire format are unchanged.

Each interaction pass executes at most eight module commands and inspects at
most 64 records. A saved scan cursor advances across blocked producers, with a
nonblocking descriptor poll between unfinished scans. A completed all-deferred
scan returns to a finite wait. Normal recursive dispatch retains its shared
64-command budget. The transport pump uses poll for X/module readiness, pauses
reads at admission capacity, and handles output and retirement without running
X lifecycle handlers. It does not replace the core's normal select loop.

The command engine's existing prefix parser identifies literal direct builtins.
Nop, Beep, Echo, Schedule and Deschedule are audited; module replies and
SendToModule are admitted only when relevant receivers are asynchronous. Echo
also waits when an echo receiver is synchronous. Subscription updates wait
when the sender has synchronous masks; sync/nograb-mask changes stay deferred.
Silent/KeepRc and ordinary argument expansion retain their existing execution
path. Dynamic command names, conditions and complex functions are deferred as
whole commands rather than partly executed. Style, colorset, window/resource
mutation and nested interactions remain deferred.

Due safe timer heads receive an eight-command interaction budget while sharing
the normal scheduler budget. An unsafe head holds later timers in deadline
order. Lifecycle events, topology changes, session callbacks and arbitrary
commands remain for normal dispatch after the interaction unwinds. The parent
function continues after release or cancellation.

Temporary module/timer execution now saves and restores both current and
previous cached X events. Restoring only the current event broke the previous
event used by FPutBackEvent. The last real X timestamp remains monotonic.

## Verification matrix

| Environment | Full gate | Final event-cache follow-up | X coverage |
| --- | --- | --- | --- |
| Linux ARM64 GCC, Podman VM | 51/51 | 5/5, staged install refreshed | Xvfb and Xorg dummy |
| Linux ARM64 Clang/lld, Podman VM | 51/51 | 5/5, staged install refreshed | Xvfb and Xorg dummy |
| Linux AMD64 GCC, Podman emulation | 51/51 with final cache fix | Included in full gate | Xvfb and Xorg dummy |
| FreeBSD 15.1 ARM64, QEMU/HVF | 41/41 | 5/5 and default-server checks | Xorg scfb |
| FreeBSD 15.1 AMD64, QEMU/TCG | 41/41 | 5/5 and default-server checks | Xorg scfb |
| OpenBSD 7.9 ARM64, QEMU/HVF | 41/41 | 5/5 and default-server checks | Xenocara wsfb, root-sized zero-monitor fallback |
| OpenBSD 7.9 AMD64, QEMU/TCG | 41/41 | 5/5 and default-server checks | Default Xenocara |

All builds and staged installs succeed. Every BSD final default-server run
repeats compatibility, held-drag servicing and cancellation scripts after the
cache fix. These are local gates; native Linux sanitizer CI remains unrun.

The native queue regression exercises a 256-record blocked producer, bounded
scan progress to an independent producer, same-producer head barriers, active
command barriers, cursor repair after normal removals, FIFO recovery and nested
shared budgets. Existing real-pipe admission and Linux saturation/unlock tests
remain passing; their verification-only 16/8 limits do not change production.

The new real-X regression holds actual Move and Resize interactions and checks:

- Independent prefixed input, safe timers and asynchronous module callbacks
  execute before physical release, also during continuous motion.
- Style/UpdateStyles and same-producer tails wait, then execute in order. The
  test inspects frame/client geometry to verify the actual title-style change.
- A module's active complex function resumes before its own queued tail;
  independent producers can execute while that function holds the drag.
- Dynamic names and complex functions stay deferred as whole commands. An
  unsafe timer head holds later timer commands until release.
- A synchronous message receiver cannot reenter the drag through SendToModule.
  After release, its real Style/UpdateStyles callback, unlock and source tail
  complete successfully.

The cancellation regression retains twelve Move/Resize destruction,
withdrawal, titlebar and termination cases. Wireframe termination cases now
also prove timer and independent module servicing while the WM owns the server
grab, using an already connected probe. Native X tests check cached-event
restoration, bounded motion and release/key/lifecycle ordering.

Two negative controls fail as expected. The saved pre-service ARM64 WM times
out waiting for independent input during a held drag. Linking the actual old
FEvent implementation into the new native X probe exits with SIGABRT because
the restored previous event has the wrong coordinate. Scripts, sources and
outputs are retained; these are not sanitizer results.

An initial GCC full run passed 50/51: adding timer setup exceeded FVWM's ten
startup-command argument slots, silently omitting the fixture's titlebar
binding. Moving that binding to the module setup before its ready barrier
fixes the harness; subsequent GCC and Clang full gates pass 51/51. Failed logs
and diagnostics remain saved. A separate fixture correction uses callable
Style/UpdateStyles: Recapture is not a registered command in this tree, so a
literal Recapture string would not establish mutation coverage.

## Evidence and reproduction

Ignored `builddir/codex-2026-10-04/interaction-service/` contains seven build
archives across six OS/architecture targets, full stdout, follow-up/default-X
logs, negative controls, diagnostic scripts and `matrix-summary.json`.
Thirty changed files are independently fingerprinted in each guest workspace
against `source-final.tgz`; GCC and Clang ARM64 share one workspace. The frozen
source extends the previous command-lifetime snapshot and excludes later
appearance/Picom runtime changes from the shared checkout. A later desktop-test
dependency addition to tests/meson.build is also outside this frozen gate; it
is preserved in the working tree. The AMD64 archive records one inaccessible
empty X-server log left by an October 3 run; current test evidence is retained.
Disposable BSD guests shut down after saving
evidence. Installs target staging directories, not the host desktop.

With live-X enabled:

```sh
meson test -C builddir module-queue-order interaction-servicing-xvfb \
    interaction-cancellation-xvfb --print-errorlogs
```

The BSD servicing script also accepts `--display` to exercise the default
server; the full harnesses remain tests/run_on_linux.sh and tests/run_on_bsd.sh.

## Remaining decision and scope

A controlled module requesting synchronous M_CONFIGURE_WINDOW packets
reproduces a remaining legacy stall. While it withholds unlock, independent
input does not execute and the pointer remains grabbed after physical button
release. Releasing its reply restores progress and the parent continuation.
The diagnostic records all five observations in frame-sync-diagnostic/evidence.json.
This is existing PositiveWrite behavior, retained by this pass.

The next compatibility decision is whether to deliver final geometry after a
drag to synchronous frame receivers instead of waiting for every intermediate
reply. It has been asked separately; the current implementation preserves
those synchronous waits. This is not covered by the earlier cross-module
ordering approval.

INPUT-01 remains partial: arbitrary mutating commands, target/resource lifetime,
hints/topology and nested interactions require further work. Count budgets do
not preempt one blocking handler or guarantee an elapsed-time limit. These
checks establish servicing and ordering under controlled X loads; they do not
measure physical desktop/GPU latency or arbitrary third-party themes.
