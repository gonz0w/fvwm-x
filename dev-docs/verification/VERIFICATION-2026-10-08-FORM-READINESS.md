# Form input, timer and timeout drawing

Status: three related stories are focused-qualified. One successor matrix combines
these with the four focused-qualified Script packet/widget stories; separate Form
and Script-only full matrices were not dispatched.

| Story | Result |
| --- | --- |
| Input readiness | Common cancellation-aware poll replaces Form's fd_set loop. Queued X work remains first; ready pipe work precedes newly ready X work. EOF exits normally and termination is observed during X draining and readiness. |
| Timer integration | The existing libevent countdown is serviced without entering a separate blocking dispatch loop during setup. An active countdown uses finite 100-ms input waits; timer event/base ownership is explicit and setup failures are checked. Configured tenth-second countdown and timeout command remain supported. |
| Timeout draw ownership | The shared draw record borrows the completed timeout buffer only during drawing, then clears the pointer. It no longer frees another item's text or allocates a duplicate. Length comes from the completed output, avoiding the old unrelated-pointer subtraction. |

A safe ordinary-command baseline on the old Form binary confirms that UnMap is
not processed within 0.5 seconds while the 20-tick timer runs; timeout and EOF
complete. The initial raw colon configuration fixture was noncanonical for the
native module pipe and failed before the timer ran. Both fixture/log history and
the corrected baseline with exact old source/binary identities are retained.

Initial repaired source cd251bb3 passes normal 6+18 and FreeBSD ARM64 controls.
OpenBSD ARM64 records a double free while drawing the timed form. The complete
13-file failed payload, 27-file FreeBSD payload, initial normal logs, source and
runner are retained under initial-timeout-text-ownership before correction.
No timeout was extended and no libevent backend cause is claimed.

Final source: 1370 files, three Form-owned paths from focused Script packet1369.
SHA-256: 9216120db69c64e2cd4a07d01abb6cadd4575a0ff63644ab75c1cc85f9cc17dc.
Evidence: builddir/codex-2026-10-08/form-readiness-production.
Normal and scoped profiles each pass six pure and eighteen actual process/core
entries. Both native BSD ARM64 overlays compile/link and pass timed command,
Map/UnMap, idle EOF and termination controls. Input descriptor 4096 passes on
Linux/FreeBSD; OpenBSD's inherited hard-limit exclusion remains explicit.

Complete final payload archives contain 127 Linux, 27 FreeBSD and 26 OpenBSD files.
Strict validation checks payload hashes/modes/EOF, all 1370 developer source
hashes, all three consumed native overlay source/header hashes, exact runners and
actual timer/input/EOF/termination records. Timed input completes before timeout;
the timeout command still fires after the configured interval. These checks do
not establish pixel cadence under continuous X traffic or preempt blocking shell
commands/partial packet reads. Broader Form text/parser ownership remains open.

The [combined UI/module checkpoint](VERIFICATION-2026-10-08-MODULE-UI.md) carries
full qualification. Native release sanitizer CI, physical hardware, toolkit and
compositor checks remain separate. No configuration migration, core X-state
ownership change or new library is required.
