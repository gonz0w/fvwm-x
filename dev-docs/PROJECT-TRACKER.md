# FVWM-X project tracker

Updated: 2026-10-04. This is the active tracker; GSD planning files and the
upstream TODO are historical context. Update status with evidence, not estimates.

## Goals and constraints

- A simpler codebase that runs existing FVWM themes and configuration files.
  Preserve syntax, command ordering, bindings, functions, and module behavior;
  obtain a specific user decision before introducing a compatibility break.
- A responsive desktop: keep X state single-threaded, bound work and queues,
  and measure input/geometry responsiveness under load before and after changes.
- Security and availability: validate external data before allocation or use,
  contain abusive or stalled clients, and make descriptor ownership explicit.
- Linux, FreeBSD default Xorg, and OpenBSD default Xenocara, on AMD64 and ARM64.
  Local BSD verification uses QEMU. Keep the root-sized fallback for servers
  reporting no RandR monitors. No native Wayland backend or built-in compositor.

## Baseline

Monitor/HiDPI, shaded/interactive resize, MFL event flags, monotonic scheduling,
and a 16-event X dispatch budget are implemented. Latest completed gates:
Linux AMD64 GCC and ARM64 GCC/Clang each 30 tests; four BSD targets each 21 tests,
with default-X-server geometry and responsiveness checks. These counts describe
the previous baseline, not changes begun below. Native Linux ASan CI is
configured but has not run. No remote is configured.

This implementation batch passes 35-test Linux suites and 26-test BSD suites,
plus affected follow-up tests and default-X BSD checks. See the
[2026-10-04 report](VERIFICATION-2026-10-04.md) for exact coverage and limits.

The subsequent property/descriptor/compatibility batch passes 40-test Linux
suites and 31-test BSD suites, with final affected-test follow-up and default-X
checks on all four BSD targets. See the
[hardening report](VERIFICATION-2026-10-04-HARDENING.md). These are local gates;
native Linux sanitizer CI remains unrun.

The legacy-property/session follow-up passes 41-test Linux suites and 32-test
BSD suites, with final native-policy/startup follow-ups and checks on all four
default BSD X servers. See the [session report](VERIFICATION-2026-10-04-SESSION.md).
These results use an isolated hardening snapshot: concurrently added desktop
configuration/tests are preserved in the checkout and excluded from this batch.

The first interaction pass adds shared cancellation waits and bounded motion
coalescing. Linux gates pass 43 tests and BSD gates pass 34 tests, with affected
follow-ups and all four BSD default servers checked. See the
[interaction report](VERIFICATION-2026-10-04-INTERACTION.md) for scope and harness
follow-ups. Timers/module commands still wait during a held drag.

Evidence: [platform support](SUPPORT.md), [BSD harness](BSD.md),
[verification](VERIFICATION-2026-10-03.md), and
[comparative WM review](WM-REVIEW-2026-10-03.md).

## Ordered work

| ID | Priority / status | Work and acceptance |
| --- | --- | --- |
| IPC-01 | P0 / complete | MFL newline framing and limits. Preserve split lines; execute only complete commands; reject oversized/NUL input per client. Incremental nonblocking pipe transport, bounded input/output, connection cap, callback budgets and slow-reader timeout. Verify fragmented/coalesced traffic, boundary lengths, healthy-client isolation and backpressure with native unit and real-X tests. |
| GEO-01 | P0 / complete | Fix latest-value configure merging and WM_NORMAL_HINTS barrier; fix valid coordinates being clamped to -32768. Native boundary/ordering tests and real-X bursts with explicit UseGravity/IgnoreGravity policies. |
| DATA-01 | P1 / in progress | Typed, capped EWMH acquisition and complete fixed records implemented. Motif/Open Look records and ICCCM MULTIPLE pairs validated before indexing, with native and real-X coverage. Legacy names/class/command/session strings, protocols and colormap lists are now capped; startup WM_STATE and root restart markers are validated. Standard text encodings, empty command arguments and Corel protocol types remain supported. Remaining: module-side generic getters, tree-walk bounds, icon acquisition/decoder stress. |
| FD-01 | P1 / partially implemented | Core module pipes get close-on-exec before fork; child keeps only protocol endpoints; failed exec uses _exit. Read/PipeRead streams fail closed on flag setup. MFL locks reject unsafe files, retain one inode across restart, and direct signals exit libevent. Native lifecycle and default-X restart tests pass. ICE listeners, accepted connections, authentication streams and core session connections now get checked close-on-exec before use. Literal iceauth argv removes shell interpretation of save-directory paths. Disconnect callbacks no longer leave freed watch/pending nodes; authenticated registration, early disconnect, shutdown and cleanup pass real-X tests. Remaining: injected setup failures, helper connection/high-descriptor bounds and broader helper descriptor audit. Shared module logging descriptor remains intentional. |
| INPUT-01 | P1 / partially implemented | Shared Move/Resize wait and cancellation state now holds IDs, retains lifecycle events for normal dispatch after return, and ends on client destruction/withdrawal or termination. Motion coalescing has a 32-event budget and release/key barriers. Client/titlebar and click-to-focus cancellation, real wireframe server-grab release, unrelated destruction and queue ordering pass native/real-X gates across all six platforms and four BSD default servers. The enclosing function resumes after a normal release or cancellation. Timers and independent module commands still stall during a held drag. Remaining: target lifetime pinning, safe command/continuation routing, Recapture/style/resource mutation, hints/topology and nested interactions. |
| DRAW-01 | P1 / queued | Defer redundant decoration/geometry work using explicit dirty state; skip no-op updates. Preserve ordering and final geometry. Compare latency, CPU and X request counts under drag/title/resize floods. |
| XIO-01 | P2 / queued | Remove individually proven unnecessary XSync round trips; batch atom initialization. Keep required error/order boundaries. Profile before/after and test startup, frame and error recovery. A whole-core XCB migration is not approved. |
| LOOP-01 | P2 / queued | Replace duplicated select/fd_set bookkeeping with portable poll readiness. Preserve dispatch order and budgets; test high descriptors, EINTR, timeout and teardown on all six targets. |
| BOX-01 | P2 / queued | Sandbox narrow helpers/modules first with OpenBSD pledge/unveil and FreeBSD Capsicum where appropriate; evaluate Linux helper restrictions. Preserve reload/spawn/font/theme access. Whole-core sandbox policy requires an explicit compatibility review. |

P0 addresses reproduced correctness/availability failures. P1 addresses exposure
and the most consequential responsiveness work. P2 follows measurement and
platform evidence; do not trade working configuration semantics for broad churn.

## Supporting work

- PERF-01: count plus elapsed-time budgets; bound overdue timer execution.
  Consider a timer heap only if measured scans justify it. A budget cannot
  preempt a single blocking handler.
- CLEAN-01: extract pure protocol/geometry/ownership policies and consolidate
  duplicated cleanup. Avoid mechanical API renames and style-only rewrites.
- VERIFY-01: add protocol/parser fuzz entry points and native sanitizer coverage;
  inspect produced ELF hardening, rather than assuming flags guarantee it.
  Linux ARM64 GCC binary inspection confirms PIE, a non-executable stack,
  GNU_RELRO plus BIND_NOW, and stack-protector references. Broader binary
  inspection and native sanitizer CI remain open.
- IMAGE-01: evaluate an isolated, resource-bounded image decoder helper while
  keeping authoritative X operations in the core.
- COMPAT-01: representative baseline complete on all six OS/architecture
  targets. Fixtures cover decoration recreation, vector buttons, colorsets,
  styles, menus, real key/mouse bindings, complex-function geometry, quoted
  nested Read and PipeRead ordering, and inherited stream descriptors.
  Extend this baseline for synchronous handshakes and interactive continuations
  before changing them. Real XSMP registration is now covered. Arbitrary third-party theme collections remain untested.

## Decisions and progress

- 2026-10-04: user confirmed simple internals, FVWM theme/config compatibility,
  responsiveness and security; requested all review items be tracked and begun.
- 2026-10-04: IPC-01 and GEO-01 complete with native and real-X regression
  coverage. Pre-fix binaries fail both new real-X regressions. A follow-up fix
  preserves other subscriptions when an extended event is removed.
- 2026-10-04: CLEAN-01 has protocol and configure-policy extractions with native
  unit tests. Remaining cleanup is still open.
- 2026-10-04: bounded EWMH reads fix the short-strut indexing hazard; legacy
  decoration and MULTIPLE validation fixes additional malformed-record paths.
  Valid four-field Motif and three/five-field Open Look records remain supported.
- 2026-10-04: MFL lock lifetime and direct-signal exit bugs reproduced and fixed;
  duplicate-instance, KillModule restart and direct SIGTERM restart checks pass.
  Pre-fix negative controls fail the new legacy-property and signal regressions.
- 2026-10-04: INPUT-01 has an executable held-pointer baseline and a deliberately
  failing --require-responsive mode. It is diagnostic work, not a passing CI gate.
- 2026-10-04: shared ICCCM/session getters remove duplicate parsing and rejected
  reply leaks. Startup scalar checks, legacy protocol compatibility and ICE
  callback lifetime fixes pass Linux and all BSD default-server regressions.
  Restoring pre-fix ICE lifetimes makes the new disconnect regression fail.
- 2026-10-04: native Linux sanitizer CI now enables the live-X tests as well as
  native tests; it remains configured but unrun. FreeBSD harness dependencies
  explicitly include iceauth. Optional session-disabled build checked separately.
- 2026-10-04: INPUT-01 cancellation foundation implemented. Held-button client
  destruction/withdrawal no longer traps the WM, including titlebar and focus
  cleanup. Termination releases opaque and wireframe grabs. A pre-fix binary
  fails the new cancellation regression. Motion coalescing is shared and bounded;
  release and key barriers remain intact. Default-server checks include OpenBSD
  ARM64 wsfb with the root-sized fallback. No compatibility decision required.
- 2026-10-04: DESKTOP-01 baseline complete: one commented Slate layout with
  four light/dark palettes, a centered launcher, primary top bar, persisted
  appearance/schedule/opacity controls, and stdlib/base-tool desktop services.
  Old shipped default assets and scripts were removed; personal configs and
  module fallback compatibility remain. Native graphics error-handler and
  root-pixmap cleanup bugs discovered by palette switching are fixed with a
  deterministic real-X regression and a failing pre-fix control. Linux AMD64
  build/install, 25 native tests and desktop/compatibility checks pass; native
  desktop and graphics checks pass on both BSD architectures. GTK/Qt, a full
  notification server, bundled authentication locker, and graphical monitor
  editor remain separate work. See [desktop verification](DESKTOP-VERIFICATION-2026-10-04.md).
- Next: finish INPUT-01's command/continuation and lifetime model. A timer or
  module command may mutate or Recapture the active window, so it cannot simply
  run inside the existing loops. Keep command ordering and enclosing function
  continuation semantics. The current wait intentionally retains lifecycle
  events and does not run the normal dispatcher or arbitrary commands. Other
  items remain queued with their acceptance criteria above.
