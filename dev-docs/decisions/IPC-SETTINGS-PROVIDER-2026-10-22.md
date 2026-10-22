# Correlated desktop settings provider — IPC-02

This next IPC-02 subset extends the
[versioned MFL request contract](IPC-REQUESTS-2026-10-22.md) with three related
features: registry/value snapshots, generation-checked atomic updates, and
correlated scoped reset/reload. FvwmDesktop remains the single settings owner.
The existing registry, saved/effective precedence, writer lock and atomic file
revision check are reused. No core command, native packet layout, dependency,
listener or new visual UI is added; configured text/Echo clients remain usable.

MFL validates the bounded envelope and forwards only hex-encoded literal data
via existing SendToModule/M_STRING. FvwmDesktop validates setting names/types,
combined schedules, scopes and the observed helper generation before mutation.
Queries return an immutable provider-incarnation token; actions require it and
queries can target it. Other instances ignore targeted requests, preventing a
non-owner's stale refusal from racing a successful commit by the intended owner.
An exited/restarted target reaches the deadline; untargeted query rediscovers
an owner. Neither provider tokens nor transport cookies authenticate X11 peers.
It returns a bounded result/error envelope in ordered chunks. MFL assembles
only a pending response, then destroys it; it does not cache settings or create
a second authority. A module-incarnation cookie and nonreused serial isolate
late responses across MFL restarts. These tags are not peer authentication.

The provider owns preference commit/reload outcomes. Results distinguish
observed, committed and reloaded settings, command submission, submission
failure after commit and unknown asynchronous completion. A deadline cannot
retract a forwarded action. Clients query after an uncertain result, rather
than assume failure or replay automatically. All actions require the observed
32-hex-character provider identity and helper generation; restart retires the
provider identity, while accepted update and reload retire old generations.
This does not provide shared-core generations or preview.

Input remains a 1000-byte MFL line, with at most 384 bytes of normalized
provider data. Each client shares eight pending slots and an absolute
five-second deadline across core/provider work. One assembly allocates at most
8193 bytes, at most 32 chunks of 256 decoded bytes; malformed order, size or NUL
data terminates the response. Teardown/timeout releases partial storage; no
request history, automatic retry or unbounded callback is added. If a future
registry exceeds the response limit, omit its snapshot while retaining a
committed outcome/generation and report settings_unavailable.

This is cycle 1's first feature PR under the accepted
[faster cadence](FEATURE-TEST-CADENCE-2026-10-22.md): one incremental Linux
profile, affected policy/actual-X/legacy checks and scoped leak-enabled native
request assembly/module lifecycle checks. There is no OS API, shared native
layout, installation or server-specific behavior change requiring an early
BSD/install/default-X gate. Six-target/alternate profiles and complete release
acceptance remain explicitly pending in the tracker. No new UI is selected;
later visible consumers retain actual-X screenshots.
