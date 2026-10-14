# Buttons and borrowed-session readiness

Status: normal/scoped focused checks and native BSD ARM64 controls pass. Full qualification is pending. This component joins
[IconMan](VERIFICATION-2026-10-08-ICONMAN-READINESS.md) in one subsequent matrix;
neither component has dispatched a separate full gate.

Buttons uses the common cancellation-aware poll wrapper for X and module pipes.
Queued X events retain priority and return one event to the existing outer owner.
Session descriptors remain borrowed and admitted by fsm. A bounded snapshot
adapter writes at most capacity entries, reports the required count, initializes
readiness, and converts selected session readiness back to the existing dispatch
owner. No per-wait allocation is added; session admission remains FD_SETSIZE-bound.
The existing 10-ms pending-session fallback is retained. Terminal readiness errors
end the module loop; EOF keeps ordinary cleanup. The obsolete local descriptor
width query/state is removed. Script's separate select-based path remains open.

Frozen source: 1365 files, six owned paths from focused IconMan1364.
SHA-256: `2523390d6a3fe8a41f47380852ee394a3da549ffc942ca69a5c9d87837b085d9`.
Evidence: `builddir/codex-2026-10-08/module-buttons-production`.
Final normal and scoped profiles each pass four pure and eleven actual-module/core
entries; pure leak checks are enabled, process/core leak checks disabled. Native
BSD ARM64 overlays compile/link the changed owners and pass popup and real-session
controls with matching source hashes. Three complete archives validate payload
hashes/modes and compressed EOF: 66 Linux files, 33 FreeBSD and 34 OpenBSD.
Actual Buttons controls require a mapped window and exercise EOF, termination,
fragmented unknown input and descriptor 4096 where permitted. Session controls
exercise borrowed snapshot count/capacity/order, invalid watched descriptor
metadata, pending early disconnects and real authenticated XSMP registration with
normal disconnect. Ownership and current configuration files remain unchanged.

The first normal profile passed ten of eleven entries; its session fixture could
not launch the core because Path removed a leading ./ from the relative binary.
The fixture now resolves executable paths. The initial source, runner, logs and
complete 23-file archive remain in initial-core-path-fixture. This is a test path
correction, not a production session repair. Required broader counts are not
promoted before their completed evidence validates.

This does not complete readiness consolidation, arbitrary high session admission,
bounded X-event service, whole teardown or nonblocking packet transport. Native
release CI and physical/toolkit/compositor checks remain separate.

The preceding sourcecf focused pass is retained under before-inactive-session-fast-path.
Final source25 adds early returns when session management is inactive, avoiding a
descriptor-slot scan during ordinary Buttons waits. Fresh normal/scoped 4+11 and
both native BSD ARM64 controls pass; all 1365 developer source hashes match the
frozen archive. The full successor will combine these stories with IconMan.
