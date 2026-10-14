# FvwmBacker normal-flow termination

Status: the affected actual-module test passes normal/scoped sanitizer and both
native BSD ARM64 overlays. Combined background/theme qualification is pending.

FvwmBacker replaces its SIGPIPE exit handler with the common termination handler,
also used for orderly SIGTERM/SIGINT. Configuration cancellation returns from
main; EOF requests termination and leaves the packet loop instead of exiting
from the reader. Ordinary return now reports success. Background commands,
configuration arguments and retained-root-pixmap policy are unchanged. The
obsolete private DeadPipe function/prototype is removed. No unsafe predecessor
handler was executed; this finding came from source review.

The real module source and its existing root_bits color implementation are linked
with test-only observations. The probe verifies the three common handlers and a
normal-flow exit callback. Eight owned-pipe controls cover configuration/active
reads, SIGTERM/SIGINT/SIGPIPE, EOF and full outgoing pipes. Both normal and scoped
sanitizer tests pass all eight; actual-module leak detection is disabled. Both
native BSD ARM64 overlays pass the same eight controls. Three complete archives
validate compressed EOF and every payload hash/mode: 8 Linux, 35 FreeBSD and
36 OpenBSD files. The initial probe link omitted root_bits.c; its failure and
source are retained. Correcting that test target did not change production.

Frozen source: 1356 files, five owned paths from write1354. SHA-256:
`7af0f639903f0d142eb34e2501acd2be368333cde60977c372eadd56af89bb0b`.
Evidence: `builddir/codex-2026-10-07/backer-signal-production`.
This story joins the producer batch before its first combined matrix dispatch.
It does not establish complete Backer resource teardown or cancellation inside
arbitrary Xlib/rendering calls; those remain separate ownership work.
