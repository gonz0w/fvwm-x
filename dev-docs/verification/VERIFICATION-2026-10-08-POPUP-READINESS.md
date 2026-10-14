# Combined Ident/Pager popup readiness

Status: locally qualified from the exact source below. Six targets/eight
compiler profiles and seven final archives pass strict validation. No Ident-only
matrix was dispatched.

Ident adopts cancellation-aware two-descriptor poll, validates declared packet
bodies in startup and popup owners, and removes an obsolete quit declaration.
Pager adopts the same wrapper while preserving queued-X priority and packet
ordering, advances empty/whitespace configuration lines, and removes an already
disabled quit body. Configuration names, arguments and theme files remain usable.
No new mandatory library or authoritative core X-state change is introduced.

Frozen source: 1363 files, seven owned paths from qualified consumer1361.
SHA-256: `03f000b923406325de53fca92746b1d40ddb5a5d293c310bb0384fceee374653`.
Evidence: `builddir/codex-2026-10-08/module-popup-production`.
The [Ident component report](VERIFICATION-2026-10-08-IDENT-TRANSPORT.md) retains
its earlier focused snapshot, preflight errors and archive-collection corrections.

Final normal and scoped sanitizer profiles each pass four pure and seven
actual-module checks. Both modules pass popup EOF, termination and their owned
packet/configuration controls. Linux and FreeBSD exercise input descriptor 4096;
OpenBSD's inherited hard limit excludes it explicitly. High X-connection
descriptors are not forced. Native BSD ARM64 overlays compile/link actual modules,
verify final owned source hashes, pass five existing Ident configuration cases
and pass both popup modes. Three complete focused archives validate payload
hashes/modes and compressed EOF: 37 Linux, 48 FreeBSD and 49 OpenBSD files.

The initial source32 snapshot omitted DesktopSize from the private Pager fixture.
Scoped UBSan reported division by zero during viewport initialization. The core
normally sends that startup metadata. Only the fixture was corrected; this is
not a production division repair. Its source, ten runners, logs and initial
33/48/49-file archives remain under initial-incomplete-pager-fixture. Fresh final
checks above use source03 and complete startup metadata.

The combined matrix passes 1432 standard entries/eight compiler profiles,
120 staged identities and 160 BSD installed/default-X drivers (40 per target).
Optional profiles pass 184/164 entries. Scoped sanitizers pass 67 distinct checks
(26 pure leaks enabled; 41 process/core leaks disabled). All seven archives
strictly validate source hashes/modes, complete payloads, compressed EOF and
installed identities. No full-matrix failure, affected rerun or source change was
required. Initial component fixture/preflight failures remain retained. Ordinary
deadlines and the existing finite OpenBSD AMD64 TCG verification profile are
unchanged. See qualification-summary.json in the evidence directory.

Packet transport remains blocking with common termination cancellation. Complete
resource teardown, bounded X-event service, wider Pager payload admission and
Buttons/IconMan readiness remain open. Native release CI and physical/toolkit/
compositor checks remain separate requirements.
