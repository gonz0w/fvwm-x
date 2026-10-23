# Browser live collection adoption

Status: implemented; focused qualification is recorded in the
[report](../verification/VERIFICATION-2026-10-23-BROWSER-LIVE-ADOPTION.md).
Owners: FILE-VIEW-01 / FILE-ACTION-01, supporting file/clipboard metadata views.
Base: merged PR172 `a5c27ff1ee140c99d91abbb1edb04bf281a6d0e4`; the accumulated
[shared platform checkpoint](../verification/VERIFICATION-2026-10-23-LIVE-COLLECTION-CYCLE.md)
closes before this implementation begins.

Same-folder refresh/filter/sort, selection controls, observed properties and
worker status update the existing native Form. The domain model owns stable
IDs and hidden selection; C owns native input, viewport, labels and X resources.
An isolated worker still owns filesystem calls/process dispatch. One active
child plus one latest navigation and reap/cancellation ordering are unchanged.

Native transactions add optional keep/on/off selection choices, bounded by the
existing 256-entry cap. Source identity preserves live input; new visible entries
can restore a domain selection held outside the old projection. Before commit,
the helper reconciles current native selection through its owned earlier view,
retaining late input even when filtering/removal changed that view. A post-commit
native generation/packet reply establishes the new mapping. Exact unchanged
entries need no new native generation. Refused state can recreate the frame;
missing/invalid transport/publication fails within the rendering deadline.

Direct SendToModule messages carry runtime updates without appending snapshots
to core configuration. Existing runtime configuration syntax stays accepted.
Initial upload is core-acknowledged; live batches await a native reply every
32 commands, bounding both producer and target queues. One native reply slot
and ten seconds per rendering operation are finite; core acknowledgements alone
cannot establish another module's readiness or consumption. ListNotify supplies
actual initialized native state; ListStatus optionally includes the current
versioned selection packet. No module packet ABI or installed Perl schema changes.

Live updates explicitly renew the existing native five-minute orphan timeout;
an already dispatched timeout cannot be revived. Helper idle closure remains.
Label/SetLabel own bounded fixed-size clipped text without another X child.
The browser preallocates status/count/filter/sort/observed-properties areas, so
updates clear/paint text without discarding drafts or moving controls. Existing
Text syntax/drawing stays usable. A frame identity, projection generation and
per-invocation native ticket gate callback version 3; a stable window does not
make an old action replayable. Same-user/X11 access is not authentication.

Navigation and list/grid layout changes intentionally create a new frame;
those boundaries reset input drafts and reveal focus. Apply/Clear filter retain
the editable draft separately from the applied projection. The manuals own
syntax, examples, bounds and failure behavior. Default theme ownership,
mandatory dependencies and existing FVWM configuration contracts do not change.
Ordinary labels/Echo metadata never carry private clipboard content; protected
payload/lifetime/selection/storage and optional SQLite remain separate. External
providers, watching and mutation jobs remain open. No speedup is claimed.
