# Isolated browser action requests — focused checkpoint

Four related FILE-ACTION/DATA/VIEW-01 capabilities are ready for review: typed
navigation/refresh, revalidated application/configured-selection handoff, retained
correlated outcomes, and owned cancellation/supersession recovery. Supported
configuration syntax is unchanged. The browser stays in-progress development.
[Contract](../decisions/FILE-PROVIDER-WORK-2026-10-23.md),
[machine receipt](VERIFICATION-2026-10-23-FILE-PROVIDER-WORK.json),
[manual schemas](../../doc/FvwmMFL.adoc#_isolated_navigation_and_application_requests).

## Candidate and focused checks

Base: merged PR176 `fda9dc705e89cfa31b210d477e43055553ddf3cb`.
The 2331-file frozen source archive SHA256 is
`8df0a0c8bb4273fb903a7d556ee1eeaa955c205e61c6f5aa5e14f8758b929e01`.
Product and policy sources remain exact after freeze. One explicit UI fixture
naming derivation and documentation/evidence amendments follow it. Guest
source/hash/mode and completion binary guards, host gzip EOF/CRC and complete
capture manifest checks bind the evidence; canonical archives remain preserved.

| Gate | Linux ARM64 GCC | FreeBSD ARM64 |
| --- | --- | --- |
| Affected MFL build and five registered policy/native-X entries | Pass | Pass |
| Five Go policy packages | 76 top cases | 75 top cases |
| Go race | 76 cases / five packages | Not repeated |
| Vet and browser/CLI builds | Pass | Pass |
| Browser native-X | Slate dark, Dune light, classic-large | Controlled fixture |
| Accepted screenshots | 35 per theme | 35 |
| Scoped guest leak-enabled ASan/UBSan | Two policy and one MFL lifetime native-X exercise pass | Not repeated |
| Meson helper identities and two changed manuals | Pass | Not repeated |

Ten registered entries cover `mfl-request`, `mfl-stream`, `mfl-request-xvfb`,
`mfl-settings-xvfb` and `mfl-operations-xvfb` on both targets. Go policy and
Linux race compose **227 top-case executions / 15 package outcomes**, with vet
and matching Meson/manual helper identities. New unit checks cover strict values,
canonical bounded byte paths, current directory IDs, worker-side replacement
refusal, configured selection capture, busy admission, cancellation retaining the
slot until reap, serial-correlated terminal outcomes and supersession.

Four accepted browser native-X runs compose prior selection/filter/sort/grid/
watch behavior with typed navigation/back/refresh, configured literal handoff,
retained outcomes, busy refusal, responsive unrelated core input during a blocked
FIFO request, explicit cancellation/reaping and stale target refusal before open.
All use the existing isolated worker and native publication; no new UI widget,
filesystem mutation, application-completion guarantee or clipboard payload path
is added. The CLI/manual build and product binaries are guarded through the
completion. Reviewed screenshots are unmodified; arbitrary themes/accessibility
are not claimed qualified by these three configurations.

## Retained failure and targeted completion

The first UI driver on both targets reaches typed navigation/refresh/cancellation,
then fails with `TypeError: 'int' object is not callable`: an older native widget
variable named `control` shadows the new driver helper. Rename the helper to
`apply_control`. No product, policy, timeout or native ordering change follows
freeze. Its old/new hashes are recorded in the machine receipt.

Retain the passing native, Go/race/vet/build results. Repeat only the four UI
runs with the corrected fixture and unchanged product binaries, then complete
the sanitizer/manual gates skipped by the original driver's early stop. All
pass. Original archives and failing logs remain preserved. `make check lint`
passes; the earlier shared cycle's `make verify` is retained because portable
harness sources are unchanged. The owned FreeBSD VM shuts down normally; the
shared Linux container remains available. The Linux guest retires 208 individually hash-verified capture/source archive
duplicates (26,047,555 bytes) after canonical host preservation. Source,
incremental builds, SDK caches and VM disks remain owned working inputs.
FreeBSD capture copies remain in its stopped VM; no cleanup-only boot is made.

## Themes and remaining coverage

![Slate configured-action handoff](../../doc/screenshots/work-slate-dark.png)

![Dune cancelled work](../../doc/screenshots/work-dune-light.png)

![Classic stale-target refusal](../../doc/screenshots/work-classic-large.png)

The [gallery and examples](../../doc/development-preview.md#typed-navigation-outcomes-and-cancellation)
and [image manifest](../../doc/screenshots/work-manifest.json) retain exact
configuration/source/archive identities. Four new status/navigation captures per
profile extend the earlier 31 to 35.

PR173–175's [shared platform checkpoint](VERIFICATION-2026-10-23-BROWSER-PROVIDER-CYCLE.md)
remains closed at PR175. Merged PR176 advances the new integration cycle to
**1/3; this batch becomes 2/3 on merge**. Accumulated six-target/compiler/optional/
install/default-X/owner/milestone/release acceptance remains pending at its
required boundary. Guest sanitizers are not a new native-CPU Linux release
qualification. Physical hardware/toolkits/compositor/assistive checks and
protected-content delivery/storage remain separate.

The [PR174 performance comparison](PERFORMANCE-2026-10-23-PR174.md) remains the
reference, **2/5 after PR176; 3/5 on this merge**. No new comparison is due;
no performance claim is derived from the new binaries. File mutation/jobs,
FileManager1 and the complete browser consumer handoff remain open under their
broader owners; SQLite and protected clipboard content are not introduced.
