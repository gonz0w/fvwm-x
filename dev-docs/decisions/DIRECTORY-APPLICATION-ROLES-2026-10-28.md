# Current-folder application roles

Selected finite implementation under #56/#67, 2026-10-28. Owner Cam. The
[tracker](../PROJECT-TRACKER.md#r1-portable-application-roles--2026-10-28) owns
execution and acceptance; this record fixes the role/launch contract.

The approved scope is read-only associations for known `inode/directory`, then
one explicit native browser chooser and typed-client adoption. Existing file
navigation and personal defaults retain behavior. No MIME type classifier,
database generator, association writer, new process service, bus or dependency.
The simpler direct executable chooser cannot honor configured application roles;
small pure association policy plus existing acquisition is the necessary addition.

## Adopted association clauses

Use MIME Applications 1.0.1 sections 2–4, already owned by STANDARDS/XDG-01:
[locations](https://specifications.freedesktop.org/mime-apps/latest/file.html),
[associations](https://specifications.freedesktop.org/mime-apps/latest/associations.html),
[defaults](https://specifications.freedesktop.org/mime-apps/latest/default.html).
The linked pages identify version 1.0.1 at selection; later versions do not
silently change these rules. Independent normative fixtures qualify this slice,
not the full MIME/shared-database specification.

Search config-home, each config-dir, data-home/applications and each
system-data-dir/applications. At each location, desktop-specific defaults precede
generic defaults. Desktop names are ASCII-lowercase components of
XDG_CURRENT_DESKTOP; preserve the existing empty-environment FVWM fallback.
Desktop-specific files provide defaults, not Added/Removed association changes.
Generic Added/Removed rules retain directory scope and effective desktop-ID
ownership. Lower data-directory removal must not suppress a higher-priority
entry. Preserve explicit Added order and lexical ID order for unspecified ties.

Try default IDs in source order and list order, requiring an installed associated
eligible application; Default alone does not create an association. Otherwise use
the first eligible association. A Hidden entry shadows lower same-ID entries.
NoDisplay/OnlyShowIn/NotShowIn govern menu presentation, not role eligibility.
Use valid Application metadata/Exec and executable/TryExec availability. DBus
activation is not implemented: a valid available Exec fallback may participate;
bus-only entries are unavailable. See [Desktop Entry keys](https://specifications.freedesktop.org/desktop-entry/latest/recognized-keys.html).

Missing files are empty inputs. Malformed/unreadable sources or exceeded
resource bounds fail discovery visibly; do not silently assert another default.
Reuse existing bounded acquisition instead of creating a second directory walker.
The selected bounds are 64 combined config/data roots, 16 desktop components,
1,152 association sources, 64 KiB per source read, 16 MiB total source data,
8,192 acquisition items, walk depth 16, 2,048 desktop candidates and 128 eligible
handlers with at most 1 MiB conservative metadata/wire accounting. Source reads
apply the smaller limit before allocation. Overflow refuses publication, not
silent partial truncation. Handler IDs/metadata crossing JSON must be valid UTF-8;
raw target-folder bytes retain their separate identity/base64 representation.

## Consumer and launch contract

Open folder with… always targets the currently displayed folder. Initial choice
is the effective default; users can choose an associated alternative for one
launch. Native Launch/Refresh/Close and typed provider actions use the same
captured choices and bounded busy/cancel/correlated-result mechanisms. The
existing files provider adds directory_roles paging and three explicit role
actions; only its C request operation/scope whitelist and Go consumers change.
Generation/choice tokens stay compact hexadecimal data in existing Values
framing. No packet layout or installed Perl packet metadata changes. F12 opens
the chooser; Enter/F5/Escape provide launch/refresh/close access.

Capture folder identity, chooser/view generation, application desktop ID and
metadata fingerprint. Worker revalidates Snapshot.RootPath(), rereads role and
entry inputs, and refuses replaced folders or removed/changed selected handlers.
A changed default cannot silently substitute for the explicit chosen application.
Keep raw path bytes in the existing snapshot/base64 wire representation; expand
Exec inside the worker, avoiding corrupt non-UTF-8 argv in JSON text fields.

Reuse exact Desktop Entry expansion with one raw Files operand and one encoded
local-URI URLs operand. Pass no appended operand if Exec has no file code. Use
explicit entry Path or otherwise the captured folder for cwd. Terminal=true
uses the existing browser literal terminal program plus -e and literal argv;
missing terminal is unavailable. No shell evaluation or remote URI acquisition.
Start success is process handoff, not proof of an application/window outcome.

## Verification boundary

Before production changes, freeze expected independent precedence/association/
default/eligibility and launch/freshness examples. New API unit cases are new
policy acceptance, not old-code compilation failures. Capture old worker/consumer
missing-feature behavior where available. One combined affected Linux checkpoint
covers pure/worker/provider checks, installed independent handlers and browser
reuse, native theme screenshots and separate integrated review. Preserve initial
failures, cancellation/cleanup evidence and wider platform/release limits.
