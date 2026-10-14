# Native module producer and termination checkpoint

Status: the five-story checkpoint is locally qualified. All seven complete
evidence archives pass strict source, test, install and default-X validation.
Existing FVWM configurations
remain usable without migration, and no library is added.

The batch combines checked stack command construction, interrupted/partial write
progress, FvwmRearrange normal-flow cleanup, removal of its unused descriptor
query and FvwmBacker normal-flow termination. See the focused
[write/cleanup evidence](VERIFICATION-2026-10-07-MODULE-WRITES.md),
[construction evidence](VERIFICATION-2026-10-07-MODULE-COMMAND.md) and
[Backer evidence](VERIFICATION-2026-10-07-BACKER-SIGNALS.md).
Functional checks and the final dead-code compile-only step retain their
separate snapshots; full qualification will use this one combined source.

Frozen source: 1356 files, twelve owned paths from client1351, including five
new regression files. SHA-256:
`7af0f639903f0d142eb34e2501acd2be368333cde60977c372eadd56af89bb0b`.
Evidence: `builddir/codex-2026-10-07/module-producer-production`.
All ten combined runners pass Python preflight. Sync checks preceding source
fingerprints and preserves evidence. Four-story write1354 is retained; it did
not run a separate full matrix before this fifth story joined the checkpoint.

Passing totals: 1376 standard entries/eight compiler
profiles, 120 staged code identities, 140 default BSD X driver invocations,
optional 177/157 and 60 scoped sanitizer checks (24 pure/36 actual process/core).
The native OpenBSD AMD64 verification-profile-only TCG allowance remains finite;
ordinary deadlines are unchanged. The fresh validator has no inherited recovery
assumptions. No affected rerun or source change was needed in this combined
matrix. Earlier focused fixture corrections retain their own source/logs.
`qualification-summary.json` records final archive identities and counts.

Native Linux release sanitizer CI and physical/toolkit/compositor checks remain
separate. Broader write/backpressure/readiness owners, other module handlers and
full resource teardown are still open.
