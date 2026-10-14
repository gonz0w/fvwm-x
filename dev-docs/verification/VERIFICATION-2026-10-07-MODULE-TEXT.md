# Module configuration views and command text

Status: three related stories are implemented; focused normal/scoped sanitizer
and native FreeBSD/OpenBSD ARM64 checks pass. Full combined qualification is
prepared separately. Existing configuration files need no migration; no library
or new command limit is added.

`GetConfigLine` clears its output before reading, requires the three context
words plus text, and finds a NUL inside the declared payload before exposing the
borrowed view. Leading whitespace remains trimmed; empty text, literal quotes,
commas, comments and native bytes remain supported. The next packet read still
invalidates this static view. This does not validate every module packet type.

`SendFvwmPipe` sends byte views directly through the checked frame writer instead
of allocating a copy for each comma-separated segment. Literal comma splitting,
including empty fields and commas inside quotes, remains unchanged. The existing
1000-byte command cap is retained, and failed transport stops following frames.

`InitGetConfigLine` builds its existing request in bounded stack storage instead
of allocating a formatted string. Admitted requests have identical bytes. Missing
or over-limit requests terminate the module transport instead of waiting for an
unsent request; these exceed the already enforced command contract.

Frozen source: 1357 files, four owned paths from producer1356, one new pure test.
SHA-256: `4d07345da3fcfe108bd7965698a8b0486cdc5c7fefbad1ed6eec9e33c19ca4fd`.
Evidence: `builddir/codex-2026-10-07/module-text-production`.

Normal and scoped sanitizer profiles each pass three pure and three actual
process/core checks. Pure sanitizer checks enable leak detection; process/core
checks disable it. Both native BSD ARM64 overlays pass configuration-view and
command-write units. Three archives validate complete payload hashes, modes and
compressed EOF: 12 Linux and 24 files per BSD overlay.

Controls cover fragmented configuration replies, exact-end terminators, empty
and whitespace-only lines, missing/unterminated text, unrelated/end packets,
cleared old outputs, EOF/termination and a real owned pipe. Producer checks cover
six literal-comma fields, 256 empty frames with zero allocation, oversized-segment
refusal followed by a valid command, partial-write failure, four admitted request
lengths through the existing limit, and missing/oversized request refusal.

The initial focused runner misspelled a test selector and stopped before tests;
its script/logs are retained under `initial-selector-error`. Earlier config-only
sources and checks remain in `module-config-production`: functional source
2a0aeccf passed normal/scoped 3+3; test-only line wrapping e28c8f6d had compile-only
Linux checks and passing native BSD units. Those snapshots are not relabelled as
the final three-story source.

Ten combined runners pass preflight. Required future totals are 1384 standard
entries/eight compiler profiles, 120 staged identities, 140 default BSD X drivers,
optional 178/158 and 61 scoped checks (25 pure/36 process/core). These are required
counts, not passing results. The validator has no inherited recovery assumptions.

Blocking module transport, broader per-packet consumer validation and resource
teardown remain separate. Native Linux release sanitizer CI and physical desktop
checks remain unrun.

This three-story source was not full-qualified. The later combined gate exposed
its mistaken null-match refusal through the shipped colorset consumer. That case
is restored and tested in the [corrected action checkpoint](VERIFICATION-2026-10-07-MODULE-ACTIONS.md#null-match-correction-and-retained-first-matrix).
Earlier focused assertions that expected null refusal remain historical evidence,
not the accepted null-match contract.
