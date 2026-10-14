# Shared runtime packet reader, Form and Script

Status: three stories focused-qualified. Normal/scoped Linux seven C + eighteen
actual checks per profile and native BSD ARM64 controls pass. Strict complete
333/206/218-file archives validate source, runner and outcome identities. Full
qualification remains pending; the running editing3159 gate is unchanged.

A shared caller-owned packet reader retains partial start/header/body progress
without waiting for more pipe bytes. Initialization preserves descriptor flags
and adds O_NONBLOCK after blocking configuration startup has finished. The caller
retains descriptor ownership. Runtime reads use the existing cancellation-aware
read wrapper, with at most 64 read attempts per pass and the established native
packet size limit. Word counts are checked before body arithmetic. No read-ahead
crosses a packet; returned storage remains valid until the next read.

Form and Script use this owner in their runtime loops. Startup configuration
still uses its existing completed-packet reader. Incomplete input lets X, timers
and Script session readiness proceed. EOF/cancellation ends the module; framing
and hard errors are reported through the existing debug diagnostic. Script packet
dispatch is now a separate helper; partial input does not skip periodic/session
work. Other module readers and synchronous configured callbacks remain separate.
No library dependency, config syntax or native packet format is added or changed.

The pure owned-pipe controls check byte fragmentation, minimum/maximum packets,
ordering, bounded start-marker synchronization, completion on the final allowed
read with no further readiness, invalid sizes, every partial EOF boundary,
preexisting cancellation and descriptor failure. Form controls hold header/body
fragments while function-key commands and the configured timer proceed, then
complete the packet and verify the following command. Script controls hold a
partial header/body while periodic tasks proceed, verify handler continuation/
order, and close or terminate with partial input. Existing session, widget,
storage, execution, packet-admission and all Form controls remain in focused scope.

Frozen source: 1378 files, nine changed paths including three new files, SHA256
`49a52c37cd900d887bf24ac73ce2713d23edc7429e5443b2e47a490cf136216b`.
Artifacts: builddir/codex-2026-10-08/module-reader-production.
Native overlays compile the reader, new unit and complete changed Form/Script
owners, link known donor libraries and check sixteen consumed source files.
Linux policy leak checking is enabled; actual process leak checking is disabled.
No Python instrumentation or whole-process leak qualification follows.

The first archive validator filtered only modules/tests and omitted the two
consumed libs files from its expected count. The initial validator is retained;
including libs validates all sixteen without source, runner or test changes.
Broader module fairness, other consumers, startup acquisition, synchronous
callbacks, whole Unicode/layout and recursive expansion remain open. Native
release CI and physical/toolkit/compositor verification remain separate.
