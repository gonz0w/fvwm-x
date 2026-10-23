# FILE-ACTION/DATA/VIEW-01: live target and observation controls

Status: focused implementation after merged PR177; supported configuration
syntax remains usable. Complete five related consumer capabilities: live reveal,
observed target properties, explicit hidden/watch controls, action capability
metadata, and versioned native focus needed for live target viewport ownership.

Reuse the existing files provider, Locate policy and one isolated worker slot.
Reveal/inspect accept 1–8 canonical base64 absolute byte paths, each at most 192
decoded bytes, in the existing 384-byte values/1000-byte request envelope.
Unknown fields, duplicate paths and malformed encodings are refused before
native work. Worker policy resolves all targets in one lexical parent without
following final links or opening items; missing/mixed-parent/dot/root targets
cannot publish partial results. Explicit hidden targets include hidden entries.

Validate a complete candidate before changing model, directory, history,
selection or inspector. Success clears the admitted filter, replaces selection
and focuses the first requested target. Inspect adopts the whole observed
properties set; reveal clears the inspector. Same-folder publication retains
native windows and drafts, including an unapplied filter draft; changed folders
use normal navigation. Cancellation/supersession/result retention reuse PR177.

A native runtime ListFocus(name, generation, one-based row) reuses pure focus/
scroll policy, requiring current positive generation, valid row, no refusal and
no pending transaction. Rejected/stale focus cannot mutate published/staged
state. Browser target publication commits its selection then directs focus and
observes the native snapshot before adopting the mapping; changed-folder render
uses the existing initial focus directive. No extra X round trip enters the core.

Hidden is an isolated rescan, committing the flag with the resulting snapshot.
Watch sets/retries the existing observer and reports desired versus observed
state honestly. Read-only clients cannot change either. Neither introduces
unbounded work or a new service. Query state returns action index/display labels
and opener/terminal configured availability, never executable details.

Three representative native-X themes cover first-target focus/visibility,
byte-preserving exact selection, multi-target properties, no execution, failed
handoff preservation, hidden/watch controls and unchanged same-folder windows.
Focused Linux/early FreeBSD checks precede review. The new integration cycle is
2/3 after PR177, 3/3 on this merge; close its accumulated platform checkpoint
before the following feature batch. PR174 performance is 3/5, then 4/5 on merge.
No comparison is due. Mutation/jobs, default roles/FileManager1, SQLite,
protected clipboard payload/storage and full owner/release acceptance remain open.
