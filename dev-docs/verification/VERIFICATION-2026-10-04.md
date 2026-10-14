# WM review implementation verification

This batch implements IPC-01 and GEO-01 from
[the active tracker](../PROJECT-TRACKER.md). It preserves FVWM configuration and
theme syntax. MFL's newline protocol now has explicit resource limits described
in [its manual](../../doc/FvwmMFL.adoc).

## Changes verified

- MFL retains incomplete socket commands and forwards only complete lines;
  rejects NULs and commands exceeding the existing 1000-byte core limit.
- Module packets are assembled incrementally; command output uses queued
  nonblocking writes instead of helpers that lose partial transfers/EAGAIN.
- Socket input is capped at 16 KiB; client and core-command output queues at
  256 KiB each. The connection cap is 32. Callbacks yield after 32 commands or
  64 core packets. Slow readers and command producers are isolated from healthy
  clients. Partial lines have an absolute 30-second deadline, and pending writes
  have a 30-second inactivity timeout. Idle subscriptions remain valid.
- Event JSON is serialized once per packet and copied to interested clients.
  Removing one extended subscription preserves the others. Bare `set`/`unset`
  strings no longer let the subscription parser step beyond the command buffer.
- Configure-request merging retains the newest supplied fields and stops at
  relevant WM_NORMAL_HINTS changes. Sibling/stack fields retain their order.
- Valid size-hint coordinates remain unchanged; only values outside the signed
  16-bit range are clamped.

## Results

| Target | Completed full suite | Follow-up affected tests | Staged install |
| --- | --- | --- | --- |
| Linux ARM64, GCC | 35/35 | 4/4 | Passed |
| Linux ARM64, Clang/lld | 35/35 | 4/4 | Passed |
| Linux AMD64, GCC | 35/35 | 4/4 | Passed |
| FreeBSD 15.1 ARM64 | 26/26 | 4/4 | Passed |
| FreeBSD 15.1 AMD64 | 26/26 | 4/4 | Passed |
| OpenBSD 7.9 ARM64 | 26/26 | 4/4 | Passed |
| OpenBSD 7.9 AMD64 | 26/26 | 4/4 | Passed |

Full suites include three new native tests and two new live-X tests. The final
extended-subscription fix was found after the full suites; affected unit and
live-X tests were rerun on all targets. Linux runs use the existing Podman
case-sensitive workspaces; ARM64 runs natively, AMD64 is emulated locally.
BSD runs use native guest toolchains under local QEMU.

Both real-X regressions fail against unchanged pre-fix Linux AMD64 binaries:
MFL executes an unterminated line, and the configure burst fails to reach its
latest requested size. These are retained negative controls, not passing gates.

The live MFL test checks fragmented/coalesced commands, exact/over-limit and
embedded-NUL input, partial EOF, 192 commands spanning callback budgets,
stalled subscribers, connection limits, and a stopped core that fills the
command queue. A healthy client continues after each offending client is closed.
The extended-subscription case removes enter-window interest while retaining
Echo interest. A separate native ARM64 deadline test keeps a healthy subscriber
idle while trickling an unfinished command; the partial client is closed at the
original deadline.

The configure test freezes only the private WM while an X client queues 121
resizes, then checks final 340x180 client geometry under both UseGravity and
IgnoreGravity. After a size-hint change, client geometry reaches 350x190.
Native tests cover all coordinate boundaries, latest/disjoint configure fields,
stacking preservation, and the exact hint-barrier predicate.

Default-server runs pass both regressions on all four BSD targets: FreeBSD scfb,
OpenBSD AMD64's default server, and OpenBSD ARM64 wsfb with the root-sized
single-monitor fallback. Final MFL subscription coverage was also repeated on
these servers. No physical GPU or compositor was involved.

## Responsiveness sample

Native Linux ARM64 GCC, private Xvfb, 64 immediate and 64 scheduled commands per
case, 10,000 requested load updates per second:

| Load | Median command reply | Median 10 ms scheduled reply | WM CPU |
| --- | --- | --- | --- |
| Idle | 0.162 ms | 11.704 ms | 1.23% |
| Title changes | 0.156 ms | 11.767 ms | 2.61% |
| Resize requests | 1.685 ms | 13.422 ms | 30.08% |

These are descriptive local samples, not release thresholds or comparisons
against other WMs. The earlier 16-event-budget resize sample was 2.58/14.51 ms
at 30.61% CPU; host/guest conditions differ, so this does not isolate a causal
speedup. It establishes that this batch remains responsive in the same workload.

## Retained evidence and limits

- Linux archives: `builddir/codex-2026-10-04/review/{arm64,amd64}-evidence.tar.gz`.
- BSD archives: `builddir/qemu-bsd/{freebsd,openbsd,freebsd-amd64,openbsd-amd64}/review-evidence-2026-10-04.tar.gz`.
- Archives retain full-suite logs, affected-test follow-up, real-X evidence,
  relevant startup/server logs, and native ARM64 latency/deadline samples.

Native Linux ASan CI remains configured but unrun; no Git remote is configured.
No arbitrary FVWM theme collection, physical input/GPU, mixed-monitor hardware,
external compositor or actual GTK/Qt toolkit session was verified in this batch.
Further security/property/descriptor, interaction, rendering, polling and
sandbox work remains tracked rather than implied complete.
