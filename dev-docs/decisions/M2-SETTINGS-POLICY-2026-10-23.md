# M2.2 settings policy and native UI planning

After merged [PR190](https://github.com/gonz0w/fvwm-x/pull/190), batch the
remaining settings/UI policy under RUNTIME-GO-01 #23, with FD-01 #7 and PROC-01
#15 retaining their broader scopes. New features remain paused.

One reusable Go package owns the ordered preference registry, strict value and
scope validation, unversioned/schema-1/schema-2 decoding, schedule policy,
saved/effective candidate preparation, query metadata and native editor,
settings/appearance menu, schedule and preview-dialog construction. The existing
private worker exposes bounded pure requests for that package. It receives
snapshots, never preference paths, file descriptors, commands to execute or
preview leases. Its settings operation cannot call backend capabilities.

The resident Python controller remains the sole mutable authority: direct
private-file reads/locks/fsync/replacement, saved/effective snapshots, revision
checks, generation/incarnation identifiers, editor/draft ownership, preview
core confirmation/recovery, module replies and command publication. Keep file
replacement in that process so a lost worker reply cannot make a committed file
look like an uncommitted action. Pure candidates and plans complete before
writes or consumer replacement. Old Python policy/builders are removed.

The one-shot JSON protocol remains private. Known operation fields are exact;
duplicate object keys, non-integer integer tokens, deep JSON and oversized
input fail explicitly. Stored preferences retain the 8192-byte limit and strict
schema rules. Query/candidate/editor results do not publish or commit by
construction. UI plans have finite records/lines and are validated completely
before publication; malformed later records cannot publish an earlier prefix.
A failed editor plan keeps the current editable draft. Only immutable scalar
validation results use a 128-entry cache. No settings snapshot, revision,
generation, plan or preview state is cached.

Calls are synchronous in the transitional resident controller. They add bounded
process/JSON overhead; this is a migration/ownership checkpoint, not a speed or
source-size improvement. The final resident Go controller will import this
package directly. Do not add a second long-lived settings service or move
preference commits into one-shot workers. Native UI and all FVWM3 configuration
names/arguments remain supported, including module override precedence and
literal saved-preference formats. No new toolkit or desktop feature.

Use one combined reused Linux profile: pure policy/race/vet, retained controller
and process controls, fresh adjacent installed worker, native desktop/settings/
operations, three representative settings theme profiles and captured examples.
The policy adds no OS-specific I/O; retain PR190's affected FreeBSD evidence for
unchanged transport/commits and six-target compile results. Offline cross-builds
can check new standard-library sources without another VM matrix. Full native
six-target/default-X and Python-free desktop/session qualification remain at
M2/owner closure. Review accumulated coverage after this fifth post-PR178 merge;
a counter alone does not cause a matrix. See the
[report](../verification/VERIFICATION-2026-10-23-M2-SETTINGS-POLICY.md).

Next: move the resident configuration/dispatch/generation/editor/preview owners
together, then separately qualify native X screenshot capture. Configuration
source walking still uses Python; its literal preference validation now uses
the same Go registry. #23/#7/#15 and M2 remain open.
