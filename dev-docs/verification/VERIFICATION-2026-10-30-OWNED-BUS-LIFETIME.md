# Owned session-bus lifetime — 2026-10-30

Owner: Cam; #72/#77 lifecycle checkpoint, preserving #225's locker boundary.
Base: merged PR307 `7649a50b6f24286989e34da16dd8e288e1cd4e07`.
The [structured record](VERIFICATION-2026-10-30-OWNED-BUS-LIFETIME.json)
contains exact inputs, commands, failures, native observations and retention.

## Delivered checkpoint

Six related outcomes share the existing optional service path: one fixed native
guardian owns the actual daemon; parent/core writers retain its lifetime;
startup confirms address/authentication before applications; daemon receipt and
guardian Wait remain distinct; cleanup has bounded reporting plus passive reap;
terminal loss fences late work without changing borrowed/off or power/locker
ownership. The [decision](../decisions/OWNED-BUS-LIFETIME-2026-10-30.md)
records alternatives, protocol, cost rationale and exclusions.

Creating an owned bus adds one resident native guardian, using the enabled
helper's existing libdbus closure without making D-Bus calls in that branch.
The core holds private writer FD5 across native Restart; ordinary apps/modules,
the daemon and explicit replacement do not inherit it. RLB acknowledges both
retained leases before one-shot acquisition. Parent death leaves a coherent
surviving core's bus usable; after both writers close, only the guardian signals
and waits for its exact child. Independent applications alone do not retain the
bus after session exit, and are never signalled.

The fixed guardian accepts no caller command arguments or PID adoption. Address
startup has five seconds, TERM two and post-KILL Wait observation one. W proves
an actual child waitpid; U reports uncertainty and retains a passive reaper.
The parent releases its writer after existing worker barriers and observes for
four seconds, never killing the guardian on timeout. An explicit pipe keeps
receipt parsing independent of exec.Cmd.Wait; direct null-device stderr avoids
an inherited copier dependency. Guardian death alone never proves daemon reap.
Off/reuse/external/default-disabled paths add no guardian or lifetime writer.
Configuration syntax/defaults, settings schema, five-item inventory and public
IPC ceilings remain unchanged. See the [manual](../../doc/fvwm-session.adoc).

## Focused results

| Check | Observed result |
| --- | --- |
| Incremental Linux build | Core, wrapper, enabled helper, two C units and manual pass; 0.793 s initially, 0.223 s for final wrapper correction |
| Go ordinary/race | Final two-package runs pass 162 test/subtest records each, with no failures/skips |
| Go vet | Both affected packages pass at source01; later fixture changes and direct-null stderr correction add no new vet contract |
| Normal/scoped sanitizers | Readiness/lease and guardian real-child units pass normally and under ASan/UBSan, about 8.2 s per combined policy invocation |
| Early FreeBSD | FreeBSD 15.1 ARM64 HVF guest, Clang 19.1.7: actual descriptor/flock/exec/guardian units pass before the fatal-poll correction; core lease source is unchanged, healthy guardian paths unchanged |
| BSD Go builds | FreeBSD/OpenBSD ARM64 wrapper cross-builds pass at source01, before the portable stderr correction; compile-only |
| Install/manual | Actual session-startup tag stages wrapper/helper/login entry/manual; authoritative installed destinations supply the integration prefix |
| Disabled helper | Actual default-disabled sanitizer-profile target refuses guardian mode with status 2 before launch and has no libdbus linkage; staged at the configured helper path, its native consumer passes, then enabled bytes are restored |
| Actual-X lifecycle | Nine enabled groups plus one disabled-helper consumer pass in composed evidence; no OS power actions, physical user authentication or secure-lock claim |
| Repository | Maintenance, whitespace and fixture syntax checks pass locally |

Native groups cover parent loss plus same-PID Restart and final core exit,
normal/stalled stop, borrowed/off modes, missing acknowledgment, application/
module/custom-exec nonleak, daemon loss and guardian loss. Authentication is
recorded at actual autostart invocation, not inferred from process creation.
Checked birth/PPID/executable/pidfd identities restrict signals to fixture-owned
processes. The stopped-core case observes status 137 in 5.052 s, including bus
retirement, with independent application and admitted fake locker alive.

Source02 supplies normal/stalled/borrowed/off/no-ack/nonleak and disabled results;
source03 supplies parent-loss/current cost and both terminal-loss groups. Final
Go results are source03. Passing source01 C/sanitizer/build/install and scoped
BSD results are reused where unchanged. The disabled consumer is a scoped
installed-helper substitution using the same Go/core, not a full disabled-profile
installation or release-size comparison. All restored-prefix hashes agree.

UI/rendering is unchanged; prior inspected three-theme captures remain
applicable. No new screenshot/theme matrix, full OS/package/default-display
matrix, BSD Go runtime, physical lock or native-CPU performance qualification is
claimed. Real kernel-delayed U and fatal poll failure have source/synthetic-peer
coverage only; the maximum-record test does not guarantee partial writes on
every platform's pipe capacity.

## Findings, failures and composition

Separate source review found a fatal-poll retry spin; it now enters bounded
owned-child cleanup and passive exact wait rather than retrying a failed poll
facility. Review also found a released-writer handoff race after failed bootstrap;
release revokes eligibility synchronously before Close. A passive-guardian
failure regression proves WM launch continues without a closed FD5 or owned
address adoption. These findings are closed in source.

The first Go run's three failures were fixture factories treating existing
provider discovery as autostart acquisition. Exact worker dispatch now preserves
the one-shot/no-autostart assertions; only the failed tests first repeated.
The initial native observer read only the Go parent's main task child list.
It now unions that known parent's tasks, retaining exact identity checks. All
native groups repeated because the same inventory supports negative assertions.

Two loss observers then queried only the controller's cached snapshot. Correlated
completed session-status refreshes now precede those checks; terminal state,
unchanged attempts and two failed post-loss reprobes remain required. That run
also exposed a real stderr ownership issue: io.Discard adds an exec copier whose
pipe can be inherited by the daemon, delaying guardian Wait after guardian loss.
Direct null-device stderr removes that dependency. A real held-descendant test
observes guardian SIGKILL/Wait while its descendant still holds a lifetime
witness. Only affected Go/wrapper and three native groups repeat afterward.
Original failures and cleanup observations remain in the raw archive.

Final separate source/artifact/evidence review is recorded in the PR handoff.
Agent review is not human approval; backend model/effort metadata remains
unverified. GitHub Actions stays disabled.

## Scoped cost and retention

Three 100 ms-spaced snapshots cover only the known parent/guardian/daemon trio.
All three report these instantaneous KiB values:

| Process | RSS | PSS |
| --- | ---: | ---: |
| Parent | 6,264 | 5,111 |
| Guardian | 2,000 | 570 |
| Daemon | 4,204 | 2,082 |

Across the 0.209 s first-to-last interval, observed CPU ticks and context-switch
counts do not change. The guardian is blocked in poll. Context switches are
activity proxies, not scheduler wakeup counts; this is neither a peak, a
whole-desktop total, a long-term idle budget nor an alternative-runtime benchmark.
The guardian exists only for actually owned buses. Current unstripped core is
5,692,424 bytes (+984 from PR307), wrapper 5,317,507 (+73,000), enabled helper
222,168 (+32,344). File sizes include debug metadata, not resident-memory costs.
Periodic WM comparison remains 2/5 from merged PR305; this runtime merge becomes
3/5, so no periodic benchmark was due.

All 563 raw Linux files verify against the 701,462-byte host archive, SHA256
`88aaea005fb71ad3f11fcd161f206bd063a119e7e2610795a4ff9c19ebb10614`.
Retired 47,007,738 Linux duplicate bytes and 4,212,444,215 bytes of disposable
BSD disk/input copies. Current source, normal/sanitizer builds, useful prefix,
SDK/caches and current performance references remain. Owned fixture processes
are absent; Linux and BSD guests stopped normally. Current compact raw evidence
and exact source manifests remain available for review.

#72/#77/#225 stay open for complete frozen acceptance. Guardian failure can still
leave daemon disposition unknown; incompatible cores do not acquire FD5
protection. Full installed/support qualification and remaining service/secure-
lock adapters retain their owners. The actual NOTIFY-01 #76 server is next;
its accepted outbound client is reused. #257/#258 remain parked.
