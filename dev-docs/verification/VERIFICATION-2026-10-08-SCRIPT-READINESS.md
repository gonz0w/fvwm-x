# Script readiness and Swallow ownership

Status: final normal and scoped profiles each pass five pure and sixteen actual
module/core entries. Native FreeBSD/OpenBSD ARM64 actual overlays also pass.
The component joins IconMan/Buttons in one combined checkpoint; no Script-only
full matrix has been dispatched.

Script uses common cancellation-aware poll readiness for its main X/pipe wait and
selection-message shutdown wait. Queued X priority, one ordinary packet dispatch,
configuration grammar and command names remain unchanged. Borrowed session
readiness uses the existing bounded FSM adapter. Pending sessions wake within the
existing ten-millisecond policy even when no PeriodicTasks block is configured.
The one-second periodic interval now has an initialized monotonic clock; unrelated
input does not restart it. Pure policy checks cover rounding, expiry, backward
clock input, distant endpoints and inactive NULL clock arguments.

Source review found that the old loop read uninitialized periodic clock state when
no periodic task was configured. It also computed a pending-session timeout but
passed an infinite timeout without a periodic block. These are source findings;
no exploit claim or deliberately unsafe baseline run is made.

The real idle-session test exposed an independent shutdown ownership error: an
unfilled SwallowExec widget called XKillClient on its own placeholder window,
causing Script's X connection to fail. The widget now records its placeholder
separately, destroys only that owned resource and retains existing client teardown
for a successfully matched external window. Matching releases the placeholder.
Actual controls cover an empty widget, real independent-client reparenting and
client closure, alongside EOF, termination, fragmented ordinary packets, periodic
tasks and input descriptor 4096 where the inherited hard limit permits it.

Frozen source: 1366 files, eight owned paths from focused Buttons1365; combined
scope is seventeen paths from qualified popup1363. SHA-256:
6439c478eb23193aef8b1a68b35d02082bd7218bfc6a7e8a4492242edad74312.
Evidence: builddir/codex-2026-10-08/module-script-production. Three complete
archives validate payload hashes/modes/EOF: 98 Linux, 75 FreeBSD and 74 OpenBSD
files. Strict final validation checks exact executed runners, all 1366 developer
source hashes, fourteen consumed native-overlay source hashes, real session
markers and seven transport outcomes. OpenBSD records the inherited high-input
descriptor limit explicitly. Pure scoped leak checking is enabled; actual
process/core leak checking is disabled.

All initial attempts remain retained: an unregistered test selector; private
fixtures using WindowFont instead of Font and a literal WriteToFile first argument
instead of a script variable; the initial shared-display session failure; the
unchanged failure after isolating the real XSMP client onto a second X server; an
incomplete placeholder-field compile edit; and the real-client fixture's incorrect
M_MAP constant. The last fixture is corrected to the existing native packet type.
The placeholder production repair is demonstrated by the now passing empty-widget
and real-session controls. No ordinary deadline was extended. Session registration
is checked through the real core's authenticated connection and explicit log
marker. Script runs on the tested display; the separate session client uses its
own Xvfb display to avoid managing the fake-piped module's windows.

This does not complete nonblocking packet transport, packet-specific Script
payload admission, bounded X-event draining, arbitrary high session admission or
whole teardown. The existing shutdown selection miss count and final two-second
sleep remain; this is not a total shutdown-message deadline. High X connections
are not separately forced. Native release CI and physical/toolkit/compositor
coverage remain separate requirements.
