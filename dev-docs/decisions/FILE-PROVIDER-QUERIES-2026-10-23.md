# FILE-DATA/VIEW-01: read-only browser provider checkpoint

Status: implemented for focused review; broader owners and integration acceptance
remain open. Configuration syntax is unchanged.

The existing optional Go browser exposes its admitted metadata/current native
selection through the existing private MFL socket. This advances four related
capabilities: bounded instance discovery, generation-bound metadata/selection
pages, typed observed selection summaries and a correlated `FvwmCommand
--request` entry point. The native Form is reused; no toolkit, second filesystem
worker, database, clipboard store or core module packet is introduced.

MFL owns eight connection-scoped producer identities and one active response per
producer. Queries use normal per-client waits and absolute deadlines. Producer
lookup never retains a client pointer across callbacks. A closing producer keeps
its identity until actual retirement; peer failure marks retirement through the
existing continuation instead of freeing queue members during iteration.
Cancellation/disconnect discards the wait, keeping the producer busy until its
terminal response or disconnect. Stalled work is therefore finite; it cannot
accumulate hidden queued queries. Registered producer connections cannot change
to read-only mode; reconnect to retire the role.

Browser state stays on its existing main goroutine. Its sole transport reader
admits one immutable query to a bounded channel; the existing native status
handshake reconciles current focus/selection without a user callback. Results
read only the admitted snapshot. Opaque tokens bind metadata/projection and
current native selection/focus, and are scoped to the producer incarnation.
Names/paths use base64; 64-bit sizes/timestamps use decimal strings. Display
labels never identify paths. Eight entries per page and 8192 assembled bytes
bound responses; too-large results are explicit failures.

This is metadata observation, not path freshness or permission to act. File
mutation, external typed actions/FileManager1 and clipboard content protection
remain with their owning stories. SQLite is still unnecessary for this layer.
The complete wire/user contract is in [FvwmMFL](../../doc/FvwmMFL.adoc),
[browser](../../doc/fvwm-file-browser.adoc) and
[FvwmCommand](../../doc/FvwmCommand.adoc) manuals. These shipped components must
be updated together; internal protocol evolution needs no configuration migration.

Focused acceptance: native C schema/reservation/framing tests; actual-X fake
producer routing/cancel/deadline/disconnect/replacement checks; Go pure policy,
CLI socket regressions, race/vet/build; current native selection and stale-page
queries in three representative theme runs; scoped MFL lifetime sanitizers and
early FreeBSD affected builds. The accumulated platform cycle remains separate.
