# Auto readiness, delay and packet arguments

Status: three related stories pass focused checks. Combined qualification now
joins Event/configuration in one [consumer checkpoint](VERIFICATION-2026-10-08-MODULE-CONSUMERS.md),
waiting for corrected action qualification; no configuration migration or
new library is required.

FvwmAuto uses the existing termination-aware poll wrapper for its single incoming
pipe. It removes fd_set storage and descriptor-width arithmetic, handles EOF and
descriptor errors, and retries nonterminal interruption. Positive configured
millisecond delays remain unchanged; zero retains the existing one-millisecond
convention. The wait remains indefinite when no window action is pending and is
cancelled atomically on termination.

Delay conversion uses checked strtol before narrowing to int. The legacy decimal
prefix and no-digit/zero conventions remain supported. Negative/out-of-range
values refuse startup instead of feeding invalid readiness arithmetic. A missing
incoming endpoint also refuses startup. The packet consumer verifies a window
word before accessing it, unlocks a short packet, and continues to following
valid input. Existing focus/enter/leave ordering and synchronization remain intact.

Frozen source: 1361 files, four owned paths from Event1360; one new driver.
SHA-256: `4ae424a94004145801b7464f4eb272fd2411a3e86d0e326e074d50f6a0e89c94`.
Evidence: `builddir/codex-2026-10-08/module-auto-production`.
The Python owned-pipe helper is reused from the Event regression; its default
Event constructor behavior remains unchanged.

Normal/scoped profiles each pass four pure and five actual process/module tests,
including prior Event/Ident/Backer/Rearrange controls. Pure sanitizer checks enable
leaks; actual process/module checks disable them. Auto's actual module passes
twelve cases on Linux and FreeBSD ARM64: zero/nonzero delays, decimal-prefix and
no-digit input, short/fragmented packets, idle/pending termination, negative,
out-of-range/overflow delay refusal and a real incoming descriptor 4096.
OpenBSD ARM64 passes eleven cases; its inherited hard descriptor limit prevents
the 4096 case. The hard limit is preserved, and this limitation is reported by
the driver. These controls do not execute the predecessor's out-of-range fd_set.

Three archives validate complete payload hashes, modes and compressed EOF:
12 Linux and 11 files per native BSD overlay. Initial compilation stopped on a
missing log declaration header; source/logs are retained under
`initial-log-header-compile-failure`. Adding the header required no runtime change.

This step keeps module transport blocking with cancellation; it does not change
core authoritative X state or claim full module resource teardown. Ident's raw
readiness path, full combined qualification, native Linux release sanitizer CI
and physical desktop checks remain separate.

## Combined qualification — 2026-10-08

These stories are locally qualified in consumer1361: 1416 standard entries/eight
profiles, 120 identities, 152 BSD drivers, optional 182/162 and 65 scoped checks.
Seven final archives strictly validate. Earlier focused failures and descriptor
coverage gaps remain retained. See the [combined report](VERIFICATION-2026-10-08-MODULE-CONSUMERS.md).
