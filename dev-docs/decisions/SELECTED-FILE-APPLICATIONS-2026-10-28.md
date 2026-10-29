# Selected-file application handoff

Selected under #56/#67, 2026-10-28; owner Cam. The
[tracker](../PROJECT-TRACKER.md#r1-selected-file-application-handoff--2026-10-28)
owns scope and status. This checkpoint adds one selected regular-file consumer
of application roles, following merged PR295. No configuration migration,
association writer, mandatory dependency or resident is selected.

The existing ordinary Open delegates to an opener; it cannot offer associated
alternatives or retain an explicit captured choice. Reuse the existing native
chooser, isolated worker, Desktop Entry expansion and typed provider. Keep F12
for the current folder; Alt-O opens applications for exactly one selected regular
file. No focused-item fallback, symlink/special-file or multiple-file launch.
Ordinary Open, navigation and configured actions retain their contracts.

## Identification and role policy

Use an existing optional file(1) executable with literal arguments
`-b --mime-type <absolute-path>`. Missing provider is a visible unavailable result;
no package installation, shell, decompression, special-file read or symlink
following is requested. Keep classification in isolated work. One invocation has
a two-second deadline, 256-byte stdout cap and 4-KiB diagnostic cap. Accept one
bounded ASCII type/subtype token with optional final LF/CRLF; reject parameters,
wildcards, extra lines, whitespace/control contamination, empty output, nonzero
status and overflow. A time limit cannot preempt an uninterruptible kernel call.
Actual provider availability/behavior requires platform evidence.

General known-type acquisition reuses the existing bounded association and
Desktop Entry implementation. Adopt MIME Applications 1.0.1
[sections 2–4](https://specifications.freedesktop.org/mime-apps/latest/default.html)
and installed Shared MIME metadata
[alias/subclass rules](https://specifications.freedesktop.org/shared-mime-info/latest/ar01s02.html#subclassing).
Read XDG data roots' `mime/aliases` and `mime/subclasses` once. First-precedence
alias definitions win; valid subclass edges are combined. Canonicalize requested
types, relevant association keys and desktop declarations. Conflicting
alias-equivalent keys refuse, instead of depending on map iteration.

Include implicit text/* → text/plain and non-inode/* → application/octet-stream
inheritance without self-edges. Order children before all ancestors; lexical
ordering of incomparable types is a local deterministic tie policy. Execute the
complete default/association algorithm separately for each type: an eligible
association for a more-specific type wins over an explicit parent default.
First nonempty resolution supplies the initial default; append alternatives in
chain order and deduplicate desktop IDs. Missing installed metadata supplies only
available/implicit inheritance; this is not a new shared-MIME classifier or full
content-database implementation.

Keep the current 64-root, 16-desktop, 1,152 settings-source, 64-KiB settings read,
16-MiB aggregate source-data, 8,192 acquisition-item/depth16, 2,048 desktop-entry,
128-handler and conservative 1-MiB result limits. Alias/subclass reads are capped
at 1 MiB/file before allocation and count against the same aggregate budget.
Bound aliases to 4,096, explicit edges to 8,192, reachable types to 128 and depth
to 32. Retained association references are bounded across the entire chain,
not independently multiplied by its length. Provider refusal update,
2026-10-29: malformed/cyclic MIME settings or graphs, excessive input and other
acquisition errors fail visibly. Optional settings/aliases/subclasses with an
open-stage permission refusal are skipped with bounded warnings. Malformed
effective desktop entries are individually ineligible and retain their ID masks;
valid different handlers remain usable. See the
[provider results and limits](../verification/VERIFICATION-2026-10-29-XDG-PROVIDERS.md). Preserve DirectoryRoles as an exact inode/directory
wrapper with unchanged shapes/fingerprints and no alias/subclass reads; corrupt
unrelated MIME metadata must not break current-folder roles.

## Capture, launch and ownership

Capture selected item ID, snapshot, observed MIME, choice generation, desktop ID
and metadata fingerprint. Refresh rebuilds the same explicit target only while
its identity/selection proof remains current. Revalidate the regular file,
reclassify it and reread application metadata before launch. Refuse changed file,
type, association or handler until refresh; changing defaults never substitutes
for the explicitly chosen handler. Other file work, changed selection/target,
chooser/parent retirement or cancellation invalidate captured authority. Role
failure cannot repair unrelated failed work.

Expand one raw file path for %f/%F and its encoded local URI for %u/%U inside the
worker. Preserve invalid UTF-8 through snapshot/base64 identity fields. Entry Path
wins; otherwise cwd is the containing folder. Terminal keeps the existing literal
program plus -e. Do not append an operand absent a field code. Start success is
handoff, not application/window completion; no pathname/executable descriptor
lease is claimed. Cancellation racing handoff has an uncertain outcome and never
causes an automatic retry.

Parent owns a new process group for worker/classifier work. Classifier inherits
that group; successful application launches acquire their own group before Start.
On normal completion, failure and cancellation, signal the owned worker group
before reaping its leader. Serialize cleanup: no delayed cancellation may signal
a recycled PGID after Wait. Retain the worker slot until reap, including slow
kernel teardown. Test actual classifier cancellation/crash retirement and ordinary
application survival rather than inferring them from a deadline.

Typed clients share the selected-file capture through `file_roles` pages and
`file-roles`, `file-role-launch`, `file-roles-close`. Existing provider/outer
state generation and separate compact choice tokens remain distinct. No new MIME
assertion override or separate command executable is needed for this consumer.
Update native MFL admission and Go consumers together; packet layout is unchanged.

## Finite verification

Freeze independent expected policy/worker/admission cases before implementation.
Capture old missing-feature behavior without calling new-API compiler errors
regressions. One combined incremental Linux policy/race/worker/provider and actual
X checkpoint covers Slate, Dune and enlarged Classic with new UI captures.
Check affected BSD provider/group behavior early. Preserve failed attempts and
source-bound limits; full six-target/install/default-X/native-CPU/physical and
assistive/release acceptance stays separate. Startup notification, FileManager1
and its shared bus, arbitrary-file database generation and default takeover are
outside this checkpoint. #257/#258 stay parked.
