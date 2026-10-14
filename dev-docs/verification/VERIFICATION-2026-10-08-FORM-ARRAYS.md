# Form counted-array storage

Status: three related owners are focused-qualified outside the frozen argument/data
checkpoint. Final source1374:
`d7c55798bf7915715bd19900fa1b97c1f071be05310d67c7d12a1910f77b2e6e`.

Line items, selection choices and button commands share checked geometric
capacity. Counts stay in their existing int fields, with validation against both
INT_MAX and the available size_t element count. Line storage retains its existing
spare slot. Capacity is published only after allocation succeeds. Unrepresentable
storage retains the module's existing fatal allocation policy; recoverable
configuration publication and broader geometry remain separate.

Each owner passes element count and element size once to fxrealloc. The previous
calls multiplied by pointer size twice, overallocating these arrays by eight on
the tested 64-bit profiles. The deterministic 256-entry, eight-slot policy control
requires six capacity changes, compared with 32 fixed-chunk changes. This is an
allocation-policy result, not measured desktop latency or total memory usage.

An actual Form module crosses line, selection and command capacity boundaries:
40 line items, 18 selected choices across several lines and 21 button commands.
F12 emits all commands and selected values in order; timer/input service, mapping
and EOF remain usable. Pure controls cover spare slots, saturation, signed invalid
counts, zero element size and the size_t element limit without large allocation.

Final normal/scoped Linux runs each pass seven C policy, two Python harness and
23 actual process/core entries. Both native BSD ARM64 overlays pass Form policy
and transport/input/button/field/data/array controls plus harness ownership checks.
Complete 183/77/81-file archives strictly validate payload hashes, modes and EOF,
1374 developer source identities, eight native consumed source identities, exact
runners and recorded outcomes. Leak checks are enabled for C policy; actual
module checks disable leaks and Python is not sanitizer-instrumented.

The initial source32ef passes Linux normal/scoped and FreeBSD focus. OpenBSD's
new array fixture observes a created window before the immediate mapped-state
assertion succeeds. Its complete failure payload and prior source remain under
initial-native-map-observation. The fixture now waits for actual mapping inside
the same five-second deadline, followed by final Linux/BSD focused checks; no
production array code changes during this correction. The initial host runner
archive-name error is retained separately and occurred before guest source sync.

Evidence: builddir/codex-2026-10-08/form-arrays-production.
Input width/pixel preparation joins this storage work before one combined matrix;
no array-only full matrix is dispatched. Full qualification, native release CI and
physical hardware/toolkit/compositor checks remain separate. Layout sums, editing
growth, command expansion, shell callbacks and partial reads remain open.
