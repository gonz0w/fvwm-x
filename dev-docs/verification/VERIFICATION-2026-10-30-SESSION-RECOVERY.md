# Portable session recovery — 2026-10-30

Owner: Cam; SESSION-01 #72, with existing #77/#225 lifetime boundaries.
Base: merged PR306 `c0884267f2fda6d54831115731c92cd663f87bbc`.
The [structured record](VERIFICATION-2026-10-30-SESSION-RECOVERY.json)
binds source closures, commands, failures, native observations and retention.

## Delivered checkpoint

Six related lifecycle outcomes share the existing session parent: equivalent
local display keys, a shared core/parent lease, native Restart retention,
positive lease acknowledgment, bounded direct-WM shutdown and coalesced signals
with ownership-aware cleanup. No new mandatory dependency, preference store,
service registry, automatic WM restart or process adoption is introduced.

Only numeric local :D and :D.0 share a lock key; other screens remain distinct
and actual launch arguments/environment remain literal. The same flock file
description reaches the core on private FD4, with readiness on FD3. Closing one
holder never explicitly unlocks the survivor. The core validates and retains the
lease, removes private environment and normally keeps it close-on-exec. Only
native Restart temporarily preserves it for the core's own exec; ordinary
applications, modules and explicit custom replacement do not inherit it.

The parent requires exact RL before acquisition when it passes a lease. Legacy
R, extra/malformed bytes, timeout or EOF suppress autostart without preventing a
usable core. This handoff also runs with --no-autostart. Pipe-creation failure
preserves core launch/lease and disables acquisition, without claiming an
acknowledged handoff. Arbitrary host/transport aliases remain unqualified.

A first explicit stop signal cancels further acquisition/helper work and goes
only to the direct owned WM. Five seconds later, if needed, the parent requests
termination of that same process and observes actual Wait for one second.
Repeated signals cannot reset deadlines or broadcast to a process group.
Observed exit supplies the status; otherwise termination remains unconfirmed
with nonzero return and a reserved diagnostic. Independent applications,
admitted lockers and borrowed buses are never signalled. An owned bus is cleaned
after confirmed WM exit and preserved if that exit remains unconfirmed.

Acquisition has a separate two-second final cancellation barrier; a terminated
worker's actual Wait is observed for up to one second. Ordinary diagnostics use
63 slots and the terminal diagnostic one, preserving the total 64-line bound.
These observation periods cannot preempt a blocked Start, termination syscall
or kernel-delayed exit. Six seconds is not a total wrapper teardown guarantee.
The [manual](../../doc/fvwm-session.adoc) describes recovery and remaining limits.

## Focused results

| Check | Observed result |
| --- | --- |
| Incremental Linux build | Core, wrapper, readiness/lease unit and manual pass; 0.637 s initially and 0.224 s for the affected correction |
| Go normal/race | Two affected packages pass 122 test/subtest records each, with no failures/skips in the final repeat |
| Go vet | Both affected packages pass initially and after the acquisition correction |
| Linux C helper | Normal and ASan/UBSan descriptor, refusal, flock, exec, RL/R and SIGPIPE units pass; leak checking and undefined-behavior halt enabled |
| FreeBSD helper | FreeBSD 15.1 ARM64 HVF guest, Clang 19.1.7: actual descriptor/flock/exec units pass with unchanged C source; writable duplex pipe modes remain covered |
| BSD Go builds | FreeBSD/OpenBSD ARM64 wrapper cross-builds pass at source01; compile-only, before the portable acquisition-barrier correction |
| Install/manual | Actual session-startup Meson tag stages the wrapper/helper/login entry/manual; authoritative installed destinations supply the updated core/wrapper integration prefix; final manual rebuild passes |
| Parent loss/Restart | Real core remains usable after its owned parent exits; same inode remains locked, alias competitors launch no autostart, native Restart keeps PID/birth/lease, ordinary/module execs lack the lease/private environment, final holder exit permits a fresh owner/one new scan |
| Missing acknowledgment | Actual stalled startup exposes usable core with retained lease, zero scans and zero autostart applications |
| Ordinary stop | Wrapper returns observed status 0 in 0.003 s; core exit is observed and the independent application remains alive |
| Stalled/repeated stop | Exact owned core is stopped through checked pidfd; repeated signals yield one grace/termination request, observed status 137 in 5.020 s, and surviving independent X client/admitted fake locker |
| Repository | Maintenance, whitespace and fixture syntax checks pass locally |

Four native groups compose source01 no-ack/ordinary-stop with source02 parent-
loss/stalled-stop. The actual-X fixture checks PPID, process birth and executable
identity before using exact pidfds. It distinguishes observed orphan exit from
reaping. Independent-child release files persist outside disposable runtime
directories until exit observation; failures and cleanup errors are retained.

There is no new UI surface, screenshot set or repeated theme matrix. Accepted
[provider/theme evidence](VERIFICATION-2026-10-29-SESSION-PROVIDERS.md) remains
applicable to unchanged controller/Form/MFL rendering. Existing startup, logout,
optional-service and provider results are reused rather than retested wholesale.
No full OS matrix, full package/default-display login, physical authentication,
secure-lock readiness, live system-provider authorization or whole-desktop/native-
CPU performance qualification is claimed. Kernel-delayed WM Wait remains helper/
source coverage only, not a manufactured native runtime pass.

## Review, failures and composition

Separate C and integrated source reviews accept descriptor retention, native-
only inheritance, exact acknowledgment and direct-process shutdown. Final
source/artifact/evidence review is recorded in the PR handoff. Agent review is
not human approval; actual backend model/effort metadata remains unverified.
GitHub Actions stays disabled.

Source review identified that the new WM-return path had removed the old
acquisition barrier: cancellation alone could lose a race with CLI exit. The
all-return bounded drain now observes cancellation/termination before ordinary
exit. A new real active-scan/WM-exit regression requires actual child ProcessState
and SIGKILL observation before Run returns. This is a source-discovered gap;
no pre-change native orphan reproduction is invented.

The first FreeBSD recipe explicitly selected a POSIX-only namespace, hiding
BSD flock declarations. Its failure is retained. Removing that standalone flag
matches the project's normal BSD namespace; the unchanged helper passes in a
fresh disposable guest. No production source or support baseline changed.

The first native parent-loss group passed lease retention, alias competition,
Restart and nonleak checks, then waited for the wrong fresh-owner MFL filename:
the fixture kept a dot that the actual MFL sanitizer replaces with underscore.
The corrected observer passes. A deep copy also preserves the checked live
window observation before observer cleanup changes it. Only affected Go/wrapper/
manual and two native groups repeat; passing C, sanitizer, FreeBSD and unaffected
native results are composed. Redundant fixture/core MFL startup produces a
retained lock diagnostic; the actual admitted listener and core remain usable.

## Cost, retention and remaining work

Current unstripped core is 5,691,440 bytes (+2,080 from PR306), wrapper 5,244,507
bytes (+6,540). File sizes include debug metadata and are not resident memory.
Performance remains 1/5 from the last comparison at merged PR305; this candidate's
runtime merge becomes 2/5. No benchmark rerun is due.

All 136 raw Linux files verify against the 394,348-byte host archive,
SHA256 `58dfdc79354e5d16d9fd13631d6d3bdb6cc8f2cc76f74b3b5f44ee5d0acc1a88`.
Retired 44,454,451 bytes of owned Linux duplicates/stages/cross-build copies,
both disposable BSD disks (6,453,723,136 bytes) and their verified duplicate
inputs (1,969,461,358 bytes). Current source, reusable normal/sanitizer builds,
useful installed prefix, SDK/caches and current performance references remain.
Owned fixture processes are absent and all owned Linux/BSD guests stopped.

#72/#77/#225 remain open for their complete frozen criteria. Parent-crash cleanup
of an owned optional bus still needs its own guardian/lifecycle work; arbitrary
display-alias equivalence, full installed/support acceptance, qualified secure
locking/idle/suspend/inhibitors and named agent/portal/tray adapters remain with
their owners. This checkpoint does not claim orphan-free optional buses after
parent death. #257/#258 remain parked.
