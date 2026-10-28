# Current-folder application roles — 2026-10-28

Four related outcomes under #56/#67 now share existing browser/worker primitives:
read-only `inode/directory` association/default discovery, a native current-folder
chooser, freshly validated literal application handoff, and typed-client access.
F12 opens the chooser; Enter launches, F5 refreshes and Escape closes it. Existing
navigation and saved defaults retain their behavior. No dependency, MIME sniffing,
association writer, D-Bus service or configuration migration is introduced.

## Source and observed checks

Base is merged PR294 `32e0727aab28615a73910d67ab6e8d39e7651b99`.
The [receipt](VERIFICATION-2026-10-28-APPLICATION-ROLES.json) binds 24 runtime
source/test files and the independent native fixture. Focused source02 closure is
`a5499143229583128e1500b7b6d643f27cc6126116254c741fae7337ad03b54b`;
final native source06 is
`1029b600a1942b1b71642721a8381cc245a681f04283e467fceedafbf281c7a6`.
Only the native fixture differs between these closures; all 601 Go inputs and
native implementation inputs remain unchanged. Final manuals are separately
hashed, generated and rendered; gallery/status changes are documentation only.

The retained virtualized Linux ARM64 GCC lab uses kernel
6.18.10-200.fc43.aarch64 and Go 1.27.2. The focused checkpoint reuses its existing
compiler, feature profile, build cache and scoped install stage.

| Check | Observed result |
| --- | --- |
| Old feature absence | Merged old worker rejects directory-roles as unsupported. Three prewritten Go consumer cases fail behavior assertions; the expanded C admission test reaches its expected assertion failure with core dumps disabled. New pure APIs have test-first source snapshots, not invented old-code compiler RED. |
| Go focused checkpoint | 706 cases/subtests in five packages pass, race repeat 706/5; retained floor SDK 639/3. No final Go failures/skips; vet passes. |
| Incremental build | Desktop, worker, menu CLI, file browser, MFL and request unit build. No whole-WM rebuild or full OS matrix. |
| C admission | Native request unit passes. Scoped ASan/UBSan request unit passes with leak detection and halt-on-error; this is emulated-guest execution, not native-CPU sanitizer qualification. |
| Existing CLI consumer | All 18 independent desktop-menu cases pass. |
| Scoped install | Desktop-controller, desktop-menu and file-browser tags install. Four Go executables match build hashes; capture helper has a separately recorded RPATH-adjusted hash. Desktop role entry matches source. Native MFL is the current built module; unchanged core/Form are reused. This is not a complete native install qualification. |
| Installed actual X | Slate dark, Dune light and enlarged Classic pass 14 recorded case groups each. Default/alternative launch, literal raw path/URI/cwd, default-only changes, stale/changed/removed handlers, busy/close, empty discovery, refresh recovery and actual installed-browser reuse are exercised. Cleanup failures are empty. |
| UI captures | All nine unmodified 1440×1200 captures inspected; three [public examples](../../doc/development-preview.md#choose-an-application-for-the-current-folder). Controls and French labels are visible. Long path/failure labels shorten; browser status retains refresh guidance. This is visual inspection, not physical or assistive-technology acceptance. |
| Manuals | Both base/final roff pages generate in Linux; macOS mandoc renders at 60/80 columns. Lint exits 4 for existing request/unknown-macro/table/paragraph/long-line families. MFL diagnostic lines 337→349; browser 149→153. Detailed family counts are retained; no clean-lint claim. |
| Repository | One local make verify cycle passes, with 20 maintenance cases explicitly skipped because native Meson/compiler tools are unavailable on the host. Affected Linux runtime checks above remain separate. Final source/link/whitespace checks follow the documentation freeze. |

## Behavior and limits

The [contract](../decisions/DIRECTORY-APPLICATION-ROLES-2026-10-28.md) adopts MIME
Applications 1.0.1 sections 2–4 for the known directory type. Config/data and
current-desktop precedence, Added/Removed scope and effective desktop-ID ownership
are explicit. Default entries must be associated and eligible; unavailable IDs
fall through deterministically. Hidden shadows lower same-ID entries.
NoDisplay/OnlyShowIn/NotShowIn do not disqualify folder roles. Valid Exec fallback
can participate without a bus; bus-only entries remain unavailable.

Acquisition remains in the bounded isolated worker: 64 roots, 16 desktop
components, 1,152 settings sources, 64 KiB/read, 16 MiB total, 8,192 items/depth16,
2,048 desktop candidates, 128 handlers and conservative 1 MiB metadata accounting.
Missing sources are empty; malformed, unreadable and over-budget sources fail
visibly. A malformed nonempty Exec fails discovery; a valid missing executable
is ordinary ineligibility. Pure precedence/eligibility and worker/provider cases
cover these distinctions and bounded refusals.

The chooser captures the displayed folder, generation, selected desktop ID and
metadata fingerprint. A fresh worker revalidates the folder and metadata before
expanding literal argv. `%f/%F` preserve raw folder bytes; `%u/%U` use the existing
encoded local URI. Entry Path overrides folder cwd. Terminal uses the existing
literal terminal program plus -e; its exact argv is covered by worker tests,
without launching a real terminal in the X fixture. Start means started, not
application/window completion, and no descriptor lease across executable lookup
and Start is claimed. Default-only edits never redirect an explicit selection.

Typed `directory_roles` queries and role actions use existing provider tokens,
compact choice identities and bounded work/results. Only the existing C operation
and scope allowlists change; packet ABI and installed Perl metadata stay exact.
Retained chooser recovery may preserve proven pre-role collection authority; it
cannot repair unrelated failed work. Other file-work intents, including navigation and scheduled watch refreshes,
and chooser/parent retirement invalidate that proof. No implicit retries or file-content classification.

## Failures, review and evidence preservation

Pre-execution review corrected malformed Exec handling before candidate tests;
policy freeze01 is retained as unexecuted history. Initial manually assembled
sanitizer linkage omitted an existing parser symbol. Deriving the actual cached
Ninja link closure fixed the harness; the resulting instrumented unit passes.

Four native attempts are retained as failures: an escaped public-label expectation;
selection of a retiring/loading Form; a recovery helper requiring ready for an
expected failed Form; and a reuse observer competing with the real client's
queries and receiving provider_busy. Fixture-only corrections preserve native
localized text, exact mapped alias/focus/ListStatus, explicit failed recovery and
actual child PID/Wait0. Only post-reuse readiness treats documented busy as
pending. Deadlines and other error assertions remain. Final freeze05 passes all
three profiles. No production source changed to accommodate those fixture errors.
Final review also aligned chooser-retirement prose to non-role file work and
scheduled watch refreshes. Only the two final manuals were regenerated and
rendered; their five hash-verified payloads are retained in the manual-revision
record. The original archive retains the superseded manual rendering. No runtime
tests/builds were repeated for this documentation correction.
A briefly requested redundant empty-label wording/test delta was cancelled and
restored exactly to the already tested browser freeze01 before final execution.

Separate final integrated review accepts staged tree
`af9c219cc94eb5c922dbb6ece23be85c2135fe44` with no remaining actionable finding. It covers policy02/browser01,
fixture05, actual evidence/source bindings, archive integrity, manuals, captures,
status/cadence and cleanup. Both documentation findings are resolved; only the
review-status metadata delta follows that examined tree. Runtime, fixtures and
captures remain unchanged.
Requested Temper roles are available; backend model/effort identity is unattested.
An agent review is not independent human assurance or merge authorization.

All 721 archive members/names/modes/bytes/hashes and gzip CRC are verified on the
host. Archive SHA256:
`9854bd7f13fd2ff15176aa093bd5330bbee8dcda73a1c02071292b3e662cdb03`.
Host evidence is retained under the owning
`builddir/codex-2026-10-28/r1-application-roles` checkpoint. Only verified guest
capture/receipt duplicates and their duplicate archive are retired. Sources,
builds, stages, SDK/cache and VM disks remain. The owned container/machine stopped
normally before delivery.

## Acceptance remaining

#56's first Desktop Entry row is complete through merged PR293/294; this batch
advances its MIME/default-role row and #67's portable folder interoperability.
Neither whole issue closes: arbitrary-file MIME/database integration, startup
notification, wider icon/platform support and #67 adapter/support acceptance
remain separate. Six-target/default-X, native-CPU sanitizer, physical/assistive
and full R1 release qualification remain open. #257/#258 stay parked.

Performance reference `m5-performance-xdg-20261028` is 1/5 after PR294; the next
runtime merge reaches 2/5. No comparison is due and no new speed claim is made.
