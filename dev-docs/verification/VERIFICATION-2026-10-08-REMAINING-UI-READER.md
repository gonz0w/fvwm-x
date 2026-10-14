# IconMan and Ident runtime readers and incomplete display text

Status: the three-story batch is strictly locally qualified. Eight Linux/BSD
compiler profiles pass1648 standard entries;120 installed executable identities
and248 BSD default-X drivers validate. Optional Linux profiles pass211/191 and
scoped checks pass97 (31 C leak-enabled,66 actual leak-disabled). Seven complete
final archives validate exact source hashes/modes and runners. No combined
failure, rerun or source change occurred. Native release CI and physical desktop
coverage remain separate.

Frozen source:1379 files/eight changed paths from c249, SHA256
`d21f013b5effaee94e11049a06920e83f6a3c5003ef7fbea7f764d78d03c2e43`.
Artifacts: builddir/codex-2026-10-08/module-ui-reader-production.
No configuration migration, dependency or native packet-format change applies.

IconMan keeps the shared module_packet_reader on its main-loop stack and passes
it to its existing packet dispatcher. Incomplete reads return to X and tooltip
scheduling. Initial resource acquisition remains blocking. Ident initializes its
reader after initial configuration and target selection; a one-descriptor poll
services window-list acquisition, and the popup retains the same packet state.
Its middle-button target-selection path therefore keeps partial progress across
popup replacement instead of switching back to a completed-packet reader.
All shipped ReadFvwmPipe callers and declarations are updated together.

The IconMan actual control holds five partial start/header/body boundaries while
ordinary configured function-key commands proceed. Packet completion and following
native command order are checked; partial-body EOF/termination exits are separate.
Ident's one-shot popup can close through a key press/release while its body input
is incomplete; partial-body EOF and termination are also checked. The pure reader
unit covers all boundaries/order. Ident's target reselection and an observable
runtime completion acknowledgement are outside these focused controls. Existing
Ident configuration/transport, IconMan transport/tooltip and shared Buttons/Pager
driver cases remain in the affected scope.

An initial ordinary-action control exposed an existing IconMan drawing assertion:
its configured display text was not yet available when STRING_CHANGED attempted
an owned copy. Rendering now copies an empty string until the text is available;
the stored window metadata is preserved. The control exercises the missing initial
text, then provides the later visible-icon name and repeats ordinary input before
holding fragments. This is a failure-handling fix, not a configuration change.

Retained preparation failures: source1c9 compile lacked the explicit diagnostic
header in IconMan/fvwm.c; configure passed, tests/scoped/native were not run.
Source2ed normal two C and seven/eight actual entries passed; IconMan's initial UI
action aborted in draw_button before any held bytes. Complete available logs and
source/runners remain under initial-log-header-build and initial-missing-display-label.
Final source adds the diagnostic header and owned blank-rendering fallback.
Ordinary control deadlines are unchanged. Scoped policy leaks are enabled;
actual X-process leaks are disabled. No whole-process leak claim follows.

The combined three-story checkpoint completed after strict runtime-reader c249
qualification. The OpenBSD AMD64 ordinary200-entry scope includes its separately
bounded existing TCG harness check. Complete final evidence, including the final
Linux ARM64 optional results, is validated by validate.py and recorded in
qualification-summary.json. Early partial collection is not full qualification.
Core/X state stays single-threaded. Startup acquisition, configured blocking
callbacks, target reselection, full layout/Unicode and recursive expansion remain
open. The separate sourcecf44 batch addresses continuous-X fairness; it is not
part of this source identity.
