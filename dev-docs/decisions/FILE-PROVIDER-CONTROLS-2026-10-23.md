# FILE-ACTION/DATA/VIEW-01: conditional browser controls

Status: implementation for focused review, after the accumulated PR173–175
platform cycle is closed. Supported configuration syntax is unchanged.

Extend the existing browser provider with five typed controls: atomic selection
edits, literal filtering, observed sorting, list/grid presentation and observed
properties. Every control requires the current provider/state generation and a
strict values schema. The browser captures current native selection/focus before
checking that precondition. Unknown IDs/fields/types and stale state cannot
partially change the admitted model.

Reusable collection policy owns replace/add/remove/clear selection admission,
including hidden entries and the 256-item total cap. Native Select visible and
external provider controls reuse that policy. The wire accepts at most eight
IDs per edit; repeated conditional edits can compose a larger selection. Values
fit 384 serialized bytes and the existing 1000-byte whole-request cap. MFL reuses the JSON UTF-8 policy to refuse malformed text before its printer
could normalize it, then forwards the admitted typed object. The consumer
validates bounded values again before native work.
No labels, filenames or values become commands or shell fragments.

MFL keeps its one active response per producer and normal client waits. Query and
control use the same ownership/framing/timeout/cancel/disconnect machinery.
Cancellation is wait-only; a submitted control may still publish. The browser
reports applied only after the existing native publication acknowledgement. On
publication loss it retires the view/provider instead of reporting split state
as success. State tokens also include view/inspected IDs. Accepted controls renew
the ordinary idle timer; queries and refused controls do not.

Selection/filter/sort/properties retain the native frame and drafts. Explicit
view layout switches can recreate it, retaining provider identity and collection
ownership. There are no new widgets, filesystem operations, external application
launches, clipboard payloads, SQLite or GUI dependencies. These controls complete
another part of the typed collection/action boundary; external file actions,
FileManager1, protected content and full owner/install/release acceptance remain
open. See [the MFL manual](../../doc/FvwmMFL.adoc) and
[browser manual](../../doc/fvwm-file-browser.adoc).
