# Native collection publication

Status: implemented supporting contract; focused qualification is recorded in the
[report](../verification/VERIFICATION-2026-10-23-LIVE-COLLECTION-UPDATES.md).
Owners: FILE-VIEW-01, shared file/clipboard metadata collection foundation.
Base: merged PR171 `fb8cc1a2c2cfdf85a83f12d1b3d513eb7c7e80b4`.

The native list/grid can publish a bounded replacement without replacing its
Form or X child. ListVersioned opts in; existing configurations retain their
exact callback bytes and immutable lifecycle. Native module packets, core/MFL
transport, installed Perl metadata and mandatory dependencies do not change.
The [Form manual](../../doc/FvwmForm.adoc) owns syntax and the working example.

One pending snapshot per list owns at most 4096 labels of at most 1024 source
bytes each, an ordinal mapping and a bounded duplicate bitmap. Begin checks the
published generation; admitted transaction identifiers increase through aborts,
with no wrap beyond 2147483647. Entry validation precedes allocation. Duplicate,
unknown or over-limit source ordinals and malformed current-generation entries
poison the candidate; stale-generation records cannot affect a newer candidate.
Commit or Abort owns cleanup, with module-exit cleanup for unfinished work.
No timer or new queue is introduced; the owner must bound its upload/status wait.

At commit, pure policy maps the *current* native selection/focus/anchor/viewport
through unique old indices. A zero ordinal is a new unselected entry. Identity
comes from the domain owner's stable-ID mapping, never labels. Removed entries
retire selection and use surviving old neighbors for focus/viewport fallback.
Input edits while staging survive publication; changing content does not resize
or rebuild any X resource. Restart uses the latest published reset point.

Opt-in Base64 callbacks prefix the legacy focus/bitmap packet with a four-byte
big-endian generation (maximum 518 raw/691 encoded bytes). ListStatus emits an
explicit native published/pending/refusal observation with a bounded literal
correlation token. Core acknowledgement only bounds transport; it cannot prove
another module published an update. Consumers retain both old and proposed
identity mappings until the native reply and refuse callbacks from stale forms
or generations. Same-user/X11 access is not an authentication boundary.

Labels and generic Echo responses are ordinary public metadata. Protected
clipboard content, handles/private delivery, selection ownership and storage
remain with CONTENT/SELECTION/RETENTION/STORE; no sensitive payload enters this
channel. SQLite is deferred. This batch supplies a working native primitive and
checks the existing file browser unchanged. Browser refresh/filter/sort adoption,
fixed-layout status/inspector updates, external providers and watching remain
open; no complete browser/clipboard handoff or speedup is claimed.
