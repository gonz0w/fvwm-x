# M2.1 desktop acquisition and action workers

Selected by the user's M2 maintenance request after merged PR189. Parent owner:
RUNTIME-GO-01 #23, with FD-01 #7 and PROC-01 #15 for this finite process-boundary
slice. This checkpoint does not close those broad owners or all of M2.

The remaining installed Python runtime is chiefly `default-config/desktop.py`,
configured as FvwmDesktop. Its screenshot function owns ctypes/Xlib and PNG
encoding; settings, preview/editor/checker and native module dispatch share the
resident controller. Artwork generators are build/developer tools, not runtime
controllers. FvwmPerl/perllib retain M6; the already accepted M1 utilities are not
ported again. No new GUI toolkit, shell policy or desktop feature is selected.

This related batch moves application discovery, Linux/BSD CPU/memory/network/
battery acquisition, audio controls and session power requests into one private
nonresident Go executable. It introduces one bounded data-only protocol and
supervising bridge, reuses Desktop Entry syntax parsing, and removes the old
Python acquisition/backend implementations. The lexer preserves the launcher's
existing POSIX word quoting, single quotes and unselected field-code removal;
it intentionally differs from the full XDG menu's strict Exec grammar. Neither
path evaluates shell expansions. Parser rejection of malformed/duplicate keys,
NUL and invalid UTF-8 is explicit; normal user-hidden masking, current-desktop
filters, literal argv, working directory and terminal flag behavior remain.
Application order is deterministic Unicode lowercase order, with stable ties;
this is not a new localization or menu-layout feature.

One job admits 64 KiB input, 1 MiB encoded results, 64 KiB backend output, 64 app
roots, 8192 enumerated entries, depth 16, 2048 candidates per root and 512 apps.
Go enforces a four-second context and bounded command waits; the bridge applies
six-second partial request/output supervision. All reads and subprocess work
remain outside the native core event loop. Kernel-blocked filesystem calls
retain OS cancellation limitations. Typed results are validated before replacing
a published application/status snapshot. The existing resident owner still
admits at most four jobs onto two threads and alone writes FVWM module commands.

The executable owns its process group, including direct CLI invocation, so a
backend deadline, output overflow or inherited pipe can retire the group safely.
The controller retains an unreaped leader until outer failure cleanup signals
that group; it never signals a group after reaping the leader. Selector creation
precedes child construction, with rollback on fork/setup failure. Healthy jobs
exit normally. No automatic sudo/doas, arbitrary worker command execution or
new session service is introduced. The existing explicit Run command and
configuration shell behavior remain unchanged.

Install the worker beside FvwmDesktop. Build-tree callers resolve the sibling
Go output; tests may set `FVWM_DESKTOP_WORKER`. This is private transport, not a
supported user configuration syntax. The remaining Python adapter is temporary
until the resident controller moves; Go adoption alone is not a code-size,
startup, memory or performance improvement. Record job overhead separately.

Acceptance uses one combined incremental Linux policy/race/CLI/install/native-X
checkpoint and a targeted FreeBSD ARM64 process/file check, plus offline builds
for all six target pairs. Review corrections cover selector setup ownership and literal backslashes
inside launcher double quotes. Repeat only affected bridge/worker/installed
operations checks and offline cross-builds. All other accepted sources,
native C binaries, settings/capture owners and earlier theme evidence stay
source-bound. Full six-target/default-X/session measurements belong to the
later complete controller/capture handoff, not each internal port. See the
[report](../verification/VERIFICATION-2026-10-23-M2-DESKTOP-WORKERS.md).

Next M2 slices: resident settings/schema/editor/preview/checker and launcher
coordination, then the separately reviewed screenshot/native-X capture boundary.
Before #23 closes, qualify a Python-free installed desktop and complete
full-session startup, RSS/peak memory, idle wakeups/CPU and input-latency
comparisons. No full-session or interpreter-free acceptance is claimed here.
