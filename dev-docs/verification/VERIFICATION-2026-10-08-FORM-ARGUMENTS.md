# Form button arguments and owned text copies

Status: three related stories are focused-qualified outside the running five-story
Form/harness checkpoint. Final component source1374:
`6a57aca4304d2d0ad1098d60d39825fd4e2877031b195046cf6080513d40fc0b`.
Six owned code/test paths; no separate full component matrix is dispatched.

1. Button preparation borrows mode, quoted label and key views before publishing
   an item. The key follows the original quoted source boundary rather than the
   length of translated/unescaped label output. Empty labels, mode-only and legacy
   key-only forms remain supported. Incomplete quoted labels are refused before
   creating the button; later valid configuration remains usable.
2. One terminator-aware argument cursor replaces duplicated Position/Input/Choice
   token walks and is reused by Button preparation. Terminal fields stop at NUL;
   omitted Input initial text and Choice label retain their existing empty defaults.
3. Owned quoted/token copies size allocations from the consumed field, preserving
   escape and trailing-backslash treatment. The remainder of the configuration
   line is not copied or included in allocation size. A test allocator observes
   the actual production helper request without platform allocator-size APIs:
   a five-byte field in a 2048-byte input requests six bytes, and escaped fields
   retain the expected output and bounded capacity.

An ordinary `Button continue "Say \"hello\"" F12` configuration confirms the
previous Form build does not emit its button command. The complete baseline
output/driver and passing intermediate button-only sourcec44/component controls
are retained under button-only-controls. The subsequent two-story source93d
passes normal/scoped and FreeBSD focus; its source/logs are separately retained
under argument-cursor-initial-focus. No earlier unsafe field walk is executed.

Final Linux normal/scoped each pass seven C policy entries, two Python harness
entries and twenty-one actual process/core entries. Both BSD ARM64 overlays pass
Form policy, timer/input, escaped-label/F12 and terminal-field controls plus
harness ownership checks. The terminal-field control reads an empty Input and
selected Choice value through the existing command expansion engine, emits the
expected command, continues timer/input servicing and exits normally on EOF.
These controls use the actual Form module and privately owned X servers.

Complete Linux/FreeBSD/OpenBSD payload archives contain 167/65/67 files. Strict
validation checks every payload hash/mode/EOF, all 1374 developer source identities,
eight consumed BSD source identities, exact runners, policy/actual results and
recorded timer/command/cleanup outcomes. Leak checks cover C policy; actual module
checks disable leaks and Python is not sanitizer-instrumented. Evidence is under
builddir/codex-2026-10-08/form-button-production. Wider qualification, native release
sanitizer CI and physical hardware/toolkit/compositor checks remain separate.

No supported configuration migration is required. Input width/layout arithmetic,
selection growth, command-expansion bounds, shell callbacks and partial packet
read ownership remain open. The new cursor does not validate geometry or establish
a complete parser rewrite. Manual paragraph upkeep joins the later combined gate.

The button manual paragraph is prepared after the code-focused snapshot. It
will be the sole additional source difference when this component joins the
next wider checkpoint. Existing Linux Asciidoctor/FreeBSD mandoc checks verify
the new text; baseline/candidate retain the same eighteen existing diagnostics
and status 3. Complete source/roff/text/log identities are under manual-check.
No compiled source changes after the final focused gate.

The combined argument/data source0375 is now [locally qualified](VERIFICATION-2026-10-08-FORM-CHECKPOINT.md) at1544/120/204, optional198/178 and scoped84; seven final archives strictly validate with no combined failure/rerun. The focused component sources above retain their distinct scope.
