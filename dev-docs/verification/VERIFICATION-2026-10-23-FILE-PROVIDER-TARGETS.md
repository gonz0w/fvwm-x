# Live target and observation controls — focused checkpoint

Five related FILE-ACTION/DATA/VIEW-01 consumer capabilities are ready for review:
live reveal, observed target properties, explicit hidden/watch controls,
configured action metadata, and versioned native focus for target visibility,
ready in [PR #178](https://github.com/gonz0w/fvwm-x/pull/178).
Supported configuration syntax remains usable. The browser remains in-progress
development. [Contract](../decisions/FILE-PROVIDER-TARGETS-2026-10-23.md),
[machine receipt](VERIFICATION-2026-10-23-FILE-PROVIDER-TARGETS.json),
[manual schemas](../../doc/FvwmMFL.adoc#_isolated_navigation_and_application_requests).

## Candidate and focused checks

Base: merged PR177 `761f0dcc5a0e1cdc4ae60a25f64b20e6d00d8d97`.
The 2339-file frozen source archive SHA256 is
`42410e3b688c0ec55ca32812bd651b8c9a472b0e584d5ef1f4400b89c62c3c3e`.
Product, policy, fixture and manual sources remain exact after freeze. Subsequent
changes are documentation/evidence only. Guest hash/mode guards and complete
host gzip EOF/CRC/file manifest validation bind the canonical archives. No
runtime, fixture or manual correction follows the frozen checkpoint.

| Gate | Linux ARM64 GCC | FreeBSD ARM64 |
| --- | --- | --- |
| Affected Form/MFL builds and 12 registered policy/native-X entries | Pass | Pass |
| Five Go policy packages | 80 top cases | 79 top cases |
| Go race | 80 cases / five packages | Not repeated |
| Vet and browser/CLI builds | Pass | Pass |
| Browser native-X | Slate dark, Dune light, classic-large | Controlled fixture |
| Accepted screenshots | 39 per theme | 39 |
| Scoped guest leak-enabled ASan/UBSan | Three policy entries, MFL lifetime and sanitized-Form browser native-X pass | Not repeated |
| Meson helper identities and three changed manuals | Pass | Not repeated |

Twenty-four native entries cover `form-list`, `form-input`, `form-command`,
`form-transport-xvfb`, `form-input-xvfb`, `form-button-xvfb`, `form-fields-xvfb`,
`mfl-request`, `mfl-stream`, `mfl-request-xvfb`, `mfl-settings-xvfb` and
`mfl-operations-xvfb` on both targets. Go policy plus Linux race compose
**239 top-case executions / 15 package outcomes**. Both targets pass vet/build;
Meson browser/CLI bytes match the tested standalone helpers. Three changed
manuals (Form, MFL, browser) render from the frozen sources.

New unit checks cover 1–8 target admission and ninth/duplicate/malformed refusal,
bounded canonical byte paths, metadata-only configured action discovery,
complete candidate refusal, exact target selection and filter/focus ownership,
missing/mixed-parent resolution, hidden planning and watch/result ownership.
Pure native focus checks cover stale/zero generations, out-of-range/empty/refused
state and preservation of selection while scrolling to entry 4096.

Five composed browser native-X runs pass: the three themes, FreeBSD fixture and
a Linux sanitized-Form fixture. Each includes the prior selection/grid/refresh/
watch/worker/cancellation suite, then byte-preserving live reveal with exact
selection and first-item focus, explicit hidden targets and multi-properties,
no target execution, failed reveal preserving prior state, hidden/watch setters
and configured labels/availability without executables. Standalone native focus
messages also prove stale/range/pending refusal, unchanged staging/selection,
preserved input drafts and unchanged X windows. Native acknowledgement follows
the requested focus before the browser adopts its current mapping.

Three scoped guest sanitizer policy entries (`form-list`, `mfl-request`,
`mfl-stream`), MFL lifetime native-X and the sanitized Form's whole browser
fixture pass with leak detection. These are five scoped exercises, not a new
native-CPU Linux release qualification. Core and unaffected build profiles are
retained; no full rebuild/install/default-server matrix is repeated for a PR.
`make check lint` passes. Prior-cycle portable `make verify` remains retained
because portable harness sources are unchanged.

## Captures and storage

![Slate observed target properties](../../doc/screenshots/targets-slate-dark.png)

![Dune target focus and reveal](../../doc/screenshots/targets-dune-light.png)

![Classic typed watch configuration](../../doc/screenshots/targets-classic-large.png)

The [gallery](../../doc/development-preview.md#live-reveal-and-target-properties)
and [image manifest](../../doc/screenshots/targets-manifest.json) preserve
unmodified screenshots with exact source/configuration/archive identities.
Four new captures extend each browser run from 35 to 39. The inspected Dune
capture shows first-target scrolling; Slate shows observed properties for a
hidden file and final symlink; classic shows native watch state. These are
representative configurations, not arbitrary-theme/accessibility acceptance.

The owned FreeBSD VM shuts down normally; the shared Linux container remains
available. After canonical host preservation and complete hash/mode/CRC checks,
227 individually verified Linux guest duplicates retire (24,761,020 bytes):
this batch's capture tree/archive and compressed source archive. Working source,
incremental builds, SDK caches, VM disks and unrelated evidence stay intact.
FreeBSD capture copies remain in its stopped VM; no cleanup-only boot is made.

## Remaining coverage and next checkpoint

PR175 remains the [integrated reference](VERIFICATION-2026-10-23-BROWSER-PROVIDER-CYCLE.md).
Merged PR176–177 are **2/3; this batch becomes 3/3 on merge**. Close the
accumulated affected six-target checkpoint before another feature batch. Full
compiler/optional/install/default-X/consumer/owner/milestone/release acceptance
stays at its required boundary. Physical hardware/toolkit/compositor/assistive,
native-CPU Linux and protected-content checks remain separate.

The [PR174 performance comparison](PERFORMANCE-2026-10-23-PR174.md) is still the
reference, **3/5 after PR177; 4/5 on this merge**. No comparison is due. The new
Form/browser/MFL binaries carry no new performance claim. File mutation/jobs,
default folder roles/FileManager1, protected clipboard transfer/storage and full
browser handoff remain open under their owners; SQLite is not introduced.
