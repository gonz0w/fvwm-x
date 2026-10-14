# Bounded native module command construction

Status: focused normal/scoped sanitizer and both native BSD ARM64 controls pass.
Combined qualification is pending in the next module write/cleanup batch.

SendText constructs its native frame in checked stack storage using the core's
existing 1000-byte command limit. Typed copies preserve window, length, literal
body and continuation fields. An oversized command is refused before emission;
the core already refused that command. Ordinary construction no longer allocates.
No configuration migration, new dependency or new user limit is introduced.

Eight boundary lengths, NULL/oversized refusal, exact wire order, zero measured
heap allocations and a real owned-pipe command pass. Normal and scoped sanitizer
profiles each pass three pure and three actual-core entries. Leak detection is
enabled for pure controls and disabled for actual-core runs. Both native BSD
ARM64 overlays compile the production object and pass the command unit.

Frozen source: 1352 files, three owned paths from client1351. Archive SHA-256:
`cd8e6fa8462142fdc43a42a1e6187afa79d98455eb88fb6b153b56039332628f`.
Evidence: `builddir/codex-2026-10-07/module-command-production`.
Three complete archives validate compressed EOF and every payload hash/mode:
12 Linux files and 17 files for each native BSD ARM64 overlay.

This snapshot only diagnoses incomplete writes; it does not repair them.
The subsequent write-progress/termination story remains separately frozen.
Source inspection establishes the removed per-command allocation; no physical
latency benchmark or native Linux release sanitizer CI result is claimed.
