# Event transport and short configuration progression

Status: five related cleanup stories pass focused checks. The source remains
outside the preceding text/action combined gate. Full qualification is pending.

FvwmEvent reuses `ReadFvwmPacket` instead of maintaining separate header/body
reads. This preserves partial progress, validates declared lengths and ends on
EOF/error. It uses the shared termination flag/wrapper, so SIGTERM/SIGPIPE cancel
the actual blocked module read. The source findings were inspected without
executing the predecessor's unchecked body operations.

Event dispatch checks any required action argument against declared body words,
preserves unsigned window identifiers, and exposes extra text only when its NUL
is inside the declared body. Startup/shutdown have an explicit empty tail rather
than deriving a pointer from a missing packet. Ordinary configured actions and
terminated tail bytes are preserved; no configuration rewrite is required.

Empty Event configuration lines now advance rather than ending collection.
Ident advances before skipping a zero/one-byte line, ending its repeated-current-
line loop. Obsolete, unreferenced DeadPipe functions in Auto/Event are removed;
source/reference searches and actual module linkage verify their ownership.
Ident's raw select wait remains a separate audit; resource teardown is not closed.

Frozen source: 1360 files, six owned paths from corrected action1358; two new
drivers. SHA-256:
`ad70e5cdf0ac180bbdb6e98e00d457b01d2fe6a5279e07632e694e33a7584348`.
Evidence: `builddir/codex-2026-10-07/module-event-production`.

Normal/scoped profiles each pass four pure and four actual process/module checks.
Pure sanitizer checks enable leak detection; actual process/module checks disable
it. Both native BSD ARM64 overlays compile/link actual Event, Ident and Auto,
then pass ten Event and five Ident cases. Three archives validate complete hashes,
modes and compressed EOF: 12 Linux, 34 FreeBSD and 35 OpenBSD payloads.

Event controls use owned pipes for fragmented configuration/header/body, PassID,
short arguments, unterminated tails, upper-range window IDs, invalid declared counts, truncated EOF,
configuration/active SIGTERM and normal EOF/shutdown. Ident uses an owned X
server and root target to verify empty, one-byte, whitespace, repeated short
lines and configuration termination reaching normal module exit.

The first Event fixture included a colon, then a space before PassID, unlike the
core's canonical concatenated packet prefix. Both failed expected-action checks;
paired source/logs remain under `initial-canonical-config-fixture-failure` and
`initial-passid-canonical-prefix-failure`. Corrected fixture text matches the
existing core conversion; production parsing did not change for these failures.

Native Linux release sanitizer CI, full/default-BSD-X combined qualification
and physical desktop checks remain separate and unrun.

The final high-window-ID fixture passes normal/scoped and both native BSD ARM64
controls. The preceding 4ebe68af source/results remain in
`before-high-window-regression`; production module bytes are unchanged. Ten
combined runners pass preflight for required future totals of 1408 standard
entries/eight profiles, 120 installed identities, 148 default BSD drivers, optional
181/161 and 64 scoped checks (26 pure/38 process/core). These are required counts,
not passing results; dispatch waits for corrected action qualification.

The prepared Event-only matrix was not dispatched. Auto readiness joins it in
one [combined consumer checkpoint](VERIFICATION-2026-10-08-MODULE-CONSUMERS.md)
to avoid repeating the extensive matrix for related module owners. The source,
component focused evidence and original prepared runners remain retained.

## Combined qualification — 2026-10-08

These stories are locally qualified in consumer1361: 1416 standard entries/eight
profiles, 120 identities, 152 BSD drivers, optional 182/162 and 65 scoped checks.
Seven final archives strictly validate. Earlier focused failures and descriptor
coverage gaps remain retained. See the [combined report](VERIFICATION-2026-10-08-MODULE-CONSUMERS.md).
