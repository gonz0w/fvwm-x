# FILE-ACTION/DATA/VIEW-01: isolated browser action requests

Status: focused implementation after merged PR176; configuration syntax stays
unchanged. This batch supplies four related capabilities: conditional navigation
and refresh, revalidated application/configured-selection handoff, retained
correlated outcomes, and owned cancellation/supersession recovery.

Reuse the existing files provider and browser worker, not another transport or
job service. Native selection is captured before generation admission. Only one
worker request can be accepted while idle; a busy or malformed/stale request
cannot supersede it. Navigation accepts Home/Up/Back, an observed directory ID,
or canonical base64 absolute path bytes (192-byte decoded wire bound). Refresh
reconciles the existing model. Open/terminal/custom actions use only locally
configured programs and worker-side target/whole-selection revalidation.
Directory-only navigation never falls through into an application launch.

The immediate native-acknowledged reply reports admission, never completion.
One latest accepted work identity (MFL serial plus provider incarnation) and
outcome stay in the browser: accepted/cancelling/superseding until terminal
loaded/started/failed/cancelled/superseded. Polling state recovers a lost reply;
no extra queue, history service, persistence or cross-domain clipboard data
is introduced. State generations include occupancy and outcome. A newer
accepted request replaces the one retained result, while view controls/refusals
preserve it. Failure detail is clipped presentation text.

Explicit work cancellation retains the slot until child Wait. Native navigation
can supersede work through existing bounded replacement ownership. Cancelled or
superseded directory output cannot publish. Process handoff may already have
occurred and cannot be undone; started does not mean application completion.
IPC waiter cancellation/disconnect only discards that response. Browser closure
retires the provider and outcomes. Existing worker deadlines remain unchanged.

No new UI widget, blocking filesystem call in the event loop, dependency,
mutation, FileManager1, SQLite or sensitive clipboard payload transport is added.
Reuse the current three-theme native-X checks and capture the visible navigation,
cancelled, started and stale-target status examples. Full owner/handoff/release
acceptance and the accumulated platform cycle remain separately tracked.
