# Directory metadata comparison — private source checkpoint

Owner Cam; FILE-DATA-01 #66; base merged PR339
`7edc6342603eb25778b53e3324f070d715c19e68`.
[Finite decision](../decisions/DIRECTORY-BACKEND-EVALUATION-2026-11-02.md) and
[exact source/build receipts](VERIFICATION-2026-11-02-DIRECTORY-BACKEND-SOURCE.json).

Three related stories author public corpus/oracles, actual Go/snapshot baselines
and an explicitly optional direct-cgo SQLite/FTS5 probe. The command is manually
selected by build tags and is not a Meson/install target. Production browser,
module dependencies, source identities, configuration and desktop defaults do
not change. There is no live filesystem scan, file-content read, sensitive index
or installed cache/schema. The snapshot persistence wrapper is evaluation-only.

## Observed compilation

Using the existing approved Go1.27.2 darwin/arm64 SDK, both CGO-disabled and
optional SQLite test binaries compile/link on the early four-file snapshot.
After completing the driver, controls and independent oracles, both test and
command variants compile/link on each of two later seven-file snapshots. These
are incremental affected builds using one owner-controlled cache, not full WM
builds. **Zero authored test or comparison executables run.** System sqlite3's
version-only discovery is separately identified; it is not an experiment result.

An initial guard compared the retained SDK archive digest with the executable
digest and refused before any compiler call. The archive verifies against its
recorded approved checksum, and its exact go/bin/go member matches the existing
SDK executable. The corrected distinct identities are retained; no SDK mismatch,
new download, package change or toolchain substitution is claimed.

Static Mach-O/file/link observations retain each final binary's full hash and
bytes. The baseline has libSystem/libresolv linkage; the optional variant adds
libsqlite3. This is a host static observation, not target runtime loading/RSS,
a complete dependency/install cost or a SQLite feature pass. Per-target header,
engine source/version/options and SDK/build/runtime identities must freeze before
comparison. The Apple provider identity is not a portable adoption pin.

## Authored contracts and review corrections

The corpus has 0/12/256/4096 records and genuine 4097-record refusal coverage.
Domain IDs, raw name bytes, escaped labels, unsigned device/inode values and
signed size/time metadata remain distinct. Independent small goldens cover four
sort keys in both directions, literal Unicode/punctuation filtering, equal labels
with distinct IDs, metadata updates/replacements and stale/no-op generations.
Larger differential checks reuse the accepted real Go model as the reference.
Complete SQLite readback checks original fields/types, normalized labels and the
admitted snapshot digest, including extreme signed/unsigned values in the fixture.
No oracle is reported as executed by compilation.

One serialized SQLite connection binds values and uses native transactions;
ordinal row keys never replace domain IDs/generations. Same-generation no-op and
stale update checks precede replacement publication. Memory reconciliation stages
new model state until a selected snapshot write succeeds. Native handles,
statements and transient C copies have explicit cleanup ownership. Optional
build/module absence remains distinct from a pass.

Early separate analyst review found four material measurement/coverage problems,
all corrected before the final source freeze: controls could overwrite the file
being measured; the full test did not reach partial replacement; FTS construction
and queries lacked a matched scan phase; and independent sort/readback coverage
was too narrow. Controls now use separate invocations/private subdirectories.
Storage is observed before FTS, with its incremental file bytes separate. A
bounded page ceiling must produce DELETE plus partial INSERT before refusal;
the old complete metadata/generation survives and restoring the ceiling permits
recovery. This remains authored, unexecuted behavior.

FTS index construction and repeated ASCII token/prefix queries are separate,
with matched timed Go scans and independent token goldens. That workload does
not replace the browser's Unicode-lowercased literal substring semantics.
Verify mode contains reference work; measure mode omits reference queries and
fault controls, emits a deterministic result digest and explicitly requires a
matching verified source/profile/count/repetition result. Outer acceptance must
enforce that match before interpreting cost samples.

## Queue and accepted evidence

Run native oracles, failure controls, repeated costs and the comparable six-target
table once at the named substantial evaluation handoff. Use the paired manual
[build/run contract](../../bin/FvwmPrompt/cmd/fvwm-metadata-eval/README.md).
External supervision supplies finite process deadlines, startup costs, cleanup
receipts and native raw-RSS units. Go/SQLite counters are separate reported
allocator observations, not total memory; SQLite may disable its counters.
Final data-file bytes do not establish peak journal/storage cost, and committed
connection/process reopen is not power-loss recovery. No daemon/idle-work,
WM speed, production adoption, supported-target or release result is inferred.

The accepted [directory foundation](VERIFICATION-2026-10-23-FILE-BROWSER-FOUNDATIONS.md),
[watch contract](VERIFICATION-2026-10-23-BROWSER-DIRECTORY-WATCH.md) and
[accumulated six-target provider cycle](VERIFICATION-2026-10-23-BROWSER-PROVIDER-CYCLE.md)
provide the first two #66 component criteria's evidence with their exact source,
filesystem and profile limits. Existing installed manuals publish the browser
and provider behavior; do not rerun those unchanged campaigns for this evaluation.
Backend comparison, disposition and six-target cost criteria remain open.
Final current release integration is separately owned.

No new UI exists: screenshots and theme checks are not applicable. Installed
clipboard copy/lock decisions and the prior observer's external-server absence
gap remain separate; #257/#258 stay parked. Separate integrated and signed-delivery
review are still required for integration.
