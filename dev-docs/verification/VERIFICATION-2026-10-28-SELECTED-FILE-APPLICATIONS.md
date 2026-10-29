# Selected-file application handoff — 2026-10-28

Alt-O now offers applications for exactly one selected regular file. The chooser
shows its observed MIME type, preferred application and associated alternatives.
Enter launches, F5 refreshes metadata for the same captured file and Escape closes.
Changed file identity requires the main browser's Refresh and explicit reselection.
F12 still offers applications for the displayed folder; ordinary Open is unchanged.

This coherent #56/#67 batch adds general read-only MIME roles with inherited
fallback, optional existing file(1) identification, native/typed selected-file
adoption and explicit classifier/worker shutdown ownership. It introduces no
mandatory dependency, resident, default association write or configuration migration.
See the [contract](../decisions/SELECTED-FILE-APPLICATIONS-2026-10-28.md).

## Source and checks

Base is merged PR295 `142fe12803082c647d59278a13df24da29a917ba`.
The [receipt](VERIFICATION-2026-10-28-SELECTED-FILE-APPLICATIONS.json) identifies
35 reviewed source/test paths: eight policy paths, including two unchanged tests,
and 27 browser/C paths. Source02's initial normal pass precedes review corrections.
Source03 binds the corrected browser, completed Go/C builds and minimum-SDK checks;
source05 binds final native execution. Their exact closures and per-package
composition are in the receipt. Source03→05 changes only the independent Python
fixture; all 613 Go inputs and native implementation bytes stay unchanged.
Manuals and the public gallery have separate exact source/capture identities.

| Check | Observed result |
| --- | --- |
| Behavior before implementation | The merged old worker refuses file-roles. Expected pure/worker/provider/group tests and fixture scenarios were frozen before implementation. Absent new APIs are not presented as compiler-error behavior RED. |
| Normal Go | Final composition is 747 cases/subtests in five packages. Four unchanged source02 package passes are retained; the affected browser reruns on corrected source03. No final failures/skips. |
| Race | Final composition is 747 cases in five packages: three original passing packages, serial policy retry and corrected browser retry. Retries set atexit_sleep_ms=0 while retaining race diagnostics. |
| Minimum SDK | Go 1.26.9 passes 680 cases in three packages. Two pure-package passes are retained; only the storage-blocked browser retries with test-only -ldflags=-w. This omits DWARF, not code or race/logic assertions. Production flags are unchanged. Vet passes on all five packages. |
| Incremental build and C | Nine affected Ninja steps pass: four Go programs, MFL request object/module link and two request-test objects/unit link. Native request unit and all 18 independent desktop-menu CLI cases pass. No unrelated native rebuild or C sanitizer repeat for an allowlist-only change. |
| Installed actual X | Twenty recorded feature groups pass in each of Slate, Dune and enlarged Classic, including literal raw file/URI/cwd, actual classification, default-only edits, stale/changed/removed handlers, preserved-identity content changes, replaced/deleted files, empty/multiple/symlink selection, busy/close and unchanged F12. |
| Shutdown ownership | Separate Slate held-classifier native Close and actual MFL transport-loss cases pass: browser Wait, worker identity absence, classifier retirement/adopted Wait, heartbeat cessation and actual socket/module EOF are checked. The trigger precedes the classifier's independent timeout. |
| Targeted BSD | Fresh owned FreeBSD 15.1/OpenBSD 8.0 ARM64 VMs each pass the real base file provider probe and 20 targeted classifier/role/process-group test records. Host Go 1.27.2 cross-compiled immutable qualifiers execute under each actual target kernel. These are not full native build/install/X qualifications. |
| Screenshots | All nine original 1440×1200 captures inspected; three unmodified [public examples](../../doc/development-preview.md#choose-an-application-for-a-selected-file). Captions distinguish initial choices, handler refusal and recovery. Long path/error labels shorten; browser status retains guidance. |
| Manuals and repository | Linux Asciidoctor generation and base/final macOS mandoc renders at 60/80 columns pass. Lint remains exit 4 with the same five existing diagnostic families: MFL 349→359, browser 153→157 lines. make verify passes; 20 native-tool maintenance cases remain explicitly skipped on the host. Final link/whitespace checks follow documentation freeze. |

The retained Linux lab is virtualized ARM64, kernel 6.18.10-200.fc43.aarch64,
using patched Go 1.27.2 and the retained floor 1.26.9 profile. Scoped Meson install
refreshes desktop-controller/desktop-menu/file-browser tags in the existing stage.
Four installed Go programs match build hashes; the capture helper retains its
separate RPATH-adjusted identity. Native MFL is the current built module; unchanged
core/Form are reused. No complete native install or native-CPU sanitizer claim.

## MIME, freshness and ownership

The generic resolver canonicalizes requested types, relevant association keys and
desktop declarations through bounded installed aliases/subclasses. First alias
owner wins; subclass edges combine. Implicit text/plain and octet-stream ancestry
is included. Topological order keeps each child before all ancestors; lexical
ordering of incomparable types is local policy. The complete per-type default/
association algorithm runs before considering a parent: a more-specific eligible
association beats a parent's explicit default. Alternatives are deduplicated.
DirectoryRoles retains its exact inode/directory path, fingerprint and result
shape, without reading unrelated graph files.

Existing root/settings/inventory/result bounds remain. Graph reads cap at 1 MiB
before allocation within the same 16 MiB aggregate budget; aliases 4096, explicit
edges 8192, reachable types 128 and depth 32 are bounded. Association references are
bounded across the whole chain. Missing metadata is empty; malformed, unreadable,
cyclic and excessive data fails visibly. This is application-role interoperability
using available installed metadata, not a new complete MIME content database.

Identification invokes an optional file program with fixed literal -b --mime-type
and the captured absolute path. Two-second lifetime, 256-byte stdout and 4 KiB
diagnostic bounds reject malformed, nonzero, excessive and timed-out results.
The worker revalidates file identity, reclassifies and rereads the selected handler
before literal argv expansion. Default-only edits cannot redirect an explicit
choice. %f/%F retain raw bytes; %u/%U use an encoded local URI. Entry Path wins;
otherwise cwd is the containing folder. Terminal keeps literal program/-e policy.
Start is handoff, not application/window completion; no atomic pathname/executable
lease or interruption of one uninterruptible kernel call is claimed.

Native and typed requests share current selection/snapshot, observed/canonical
MIME and handler fingerprints. Native Launch/Refresh resnapshot current browser
selection because chooser and selection notifications have separate mailboxes.
Read-only diagnostics do not themselves retire choices. Changed selection/target,
non-role work or parent retirement invalidates file capture; retained role failures
cannot repair unrelated failed work. File roles use the existing compact provider/
choice tokens, bounded results and native allowlists; packet layout is unchanged.

One owner retires the worker/classifier group before reaping its leader. All three
application Start paths establish an independent group first. Browser shutdown
cancels and joins the actual worker owner, including blocked result delivery,
before returning. Applications that already escaped handoff cannot be undone;
ambiguous cancellation never automatically retries. Linux unit execution used an
owned subreaper, preserving actual adopted fixture-child Wait records separately
from heartbeat observations.

## Failures, review and evidence

The initial Linux baseline assumed file existed and stopped on absence. That real
missing-provider result is retained. Positive Linux checks use a private extracted
Ubuntu file 5.45/libmagic/database prefix with exact configured-package checksums,
without a system package install. BSD probes use their actual base providers.
Private test-provider provenance is not a package vulnerability assessment.

The nearly full Linux filesystem caused concurrent race and minimum-SDK linker
failures. Valid package passes remain; only failed/affected work repeats serially.
Two attempted host-backed temporary containers failed during OCI startup and ran
no candidate checks. Unused preparation is not qualification. Moving only the
private magic data into RAM and omitting DWARF from one test binary allowed the
remaining floor check; normal production builds still use their original flags.

Copying an immutable Git archive changed non-executable metadata 0664→0644 and
input mtimes. For 770 unchanged C/header paths, exact prior-qualified archive bytes
and executable semantics were checked before restoring original input timestamps.
Current permissions remain. Changed/new/generated/build/dependency inputs and all
outputs are excluded. Per-path before/after receipts are preserved; intended
affected Ninja targets then compile/link. This is cache reuse, not fresh
qualification of unchanged native code.

Separate source review found a test fixture's bare numeric-PID cleanup and missing
browser shutdown join. Cooperative token cleanup and actual owner completion
resolve both; affected browser checks rerun. The first native run passes Slate's
20 feature groups and native Close, then fails its transport oracle expecting1
when clean EOF returns0. Session.Read/run source and existing transport tests
establish exact0; malformed/truncated EOF remains an error. The fixture-only
correction retains ownership/socket/module assertions. Only transport loss and
the remaining Dune/Classic workflows run afterward. The original failed enclosing
Slate record is preserved and never relabelled as an overall pass.

Separate final integrated review accepts staged tree
`d1adea130dfb09424a9e63ff762805ffc92dc75c` with no actionable findings. It covers source, actual raw results,
archives, captures, manuals, row reconciliation, cadence and cleanup. Only the
review-status metadata delta follows that examined tree; runtime, fixtures and
verification payloads remain unchanged.
Requested Temper roles are available; backend model/effort identity is unattested.
Agent review is not independent human assurance or merge authorization.

The host archive has 810 regular members, exact names/modes/bytes/hashes and full
gzip CRC verified; SHA256
`cd3605a0c117859fa1a0e62ce3555132cbcdc4791e59546e2eff5e9444525144`.
Owning host records are in
`builddir/codex-2026-10-28/r1-application-launch-protocol`, with separately verified
BSD archives, source freezes, renders and tool receipts.

An initial deletion guard refused an omitted packaged directory symlink before
mutation. Normal container stop discarded the verified RAM capture duplicates;
a bounded cleanup-only continuation checks the supplemental package-header link
and retires the remaining 18 private fixture files/seven links. No tests repeat.
Sources, builds, stages, caches, SDKs and VM disks remain. All owned labs stop
normally and temporary runner keys are absent. A retained FreeBSD-disk boot was
rejected by automatic approval review as unowned and never executed; the approved
fresh immutable-cache instances provide the targeted coverage instead.

## Acceptance remaining

Earlier merged evidence now checks #56's inherited icon-resolver handoff row and
#67's portable installed command/directory-metadata row, alongside their already
completed Desktop Entry/reuse rows. These are functional-row reconciliations,
not new full-platform results. Neither whole issue closes. Startup notification,
FileManager1/shared optional bus, wider installed/lifecycle/service support and
release gates remain open; #215's separate asset/profile work is unchanged.
#257/#258 stay parked. Performance reference m5-performance-xdg-20261028 is 2/5
after PR295 and 3/5 on the next runtime merge; no comparison is due.
