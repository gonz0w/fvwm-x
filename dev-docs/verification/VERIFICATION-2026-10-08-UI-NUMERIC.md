# Monitor hit testing and UI numeric fields

Status: four new stories strictly focused-qualified; combined with the four
undispatched monitor stories into one eight-story checkpoint. No separate e761
full matrix. Final source30be99f5f16f82dc0d402e9b4bc2610476ba5709886dfd9d36373cb6c7b93461:
1398 files/nine new owning paths, twenty-four paths from canonical UI text3c37.
Artifacts: builddir/codex-2026-10-08/module-ui-numeric-production.

The new stories are disabled/checked monitor hit testing, IconMan numeric fields,
IconMan property window IDs and Pager desk/style/mouse-number handling.
The existing configuration syntax and representable decimal-prefix arguments
remain usable. No configuration migration or required dependency is introduced.

Pager hit testing skips disabled monitors, checks positive global extents before
modulo and uses wide endpoint sums. Dedicated pure controls cover disabled regions,
zero extents and endpoint addition past INT_MAX without signed overflow.

IconMan's startup/runtime modifier and startup logging fields use the existing
shared checked decimal parser. Missing or out-of-range values leave prior state
intact; startup parsing starts at the directive end rather than beyond its NUL.
Property window IDs use checked unsigned conversion and the X11 32-bit resource
range; invalid input becomes None. Existing decimal prefixes and leading plus or
whitespace remain supported. Pure controls cover exact boundaries and clearing
failed outputs; actual packet controls retain input progress after these records.

Pager's five desk/style selectors use checked decimal parsing before selecting a
style. Mouse numbers are checked before decrementing, including INT_MIN. Actual
startup controls send malformed/overflow selectors and valid decimal-prefix and
wildcard styles, then continue ordinary input. This does not qualify remaining
Rows/Columns, border arithmetic, full layout or desk-range resource limits.

These new controls exposed a real existing null-member offset expression in
Pager style setters under UBSan. All twenty-three local call sites now supply
standard C offsetof values to size_t arguments; the matching setters retain their
field types and values. No pointer is fabricated from NULL or converted to an
integer. The failing source3d27 and complete original16/17 actual outcomes plus
all available process logs remain under initial-style-offset-failure. The first
runner also selected an unregistered renamed unit; initial-test-name-failure
retains that attempt. No passing scope is claimed for either failure.

Final normal/scoped Linux8 C +17 actual per profile and native FreeBSD/OpenBSD
ARM648 C +17 actual pass. Complete193/99/114-file payloads strictly validate
full1398-file source hashes/modes, exact runners, all outcomes and eleven native
binary records. Pure scoped leaks are enabled; actual-process leaks are disabled.
Native development trees incrementally update the previously collected monitor
trees; prior monitor evidence is retained. The final source reruns affected
focused controls after the actual offsetof repair and valid-style fixture additions.

The combined eight-story source30be checkpoint is prepared behind canonical
text3c37 qualification. Required1760 standard entries/eight profiles,120 installed
identities,280 default-BSD-X drivers,optional225/205 and scoped111 (37 C,74 actual)
remain requirements until complete final archives validate. Physical sessions,
native release sanitizer CI, startup defaults, nested Pager drag waits and
remaining layout/border/numeric work remain open.

The still-undispatched30be extensive candidate is folded into the related Pager
startup sourcea653, after complete combined-source focused checks pass normal/
scoped Linux and native BSD ARM64 nine C +eighteen actual entries. One twelve-
story owner matrix at1776/120/284 optional227/207 scoped113 follows text3c37.
No separate30be matrix is dispatched. [Combined scope](VERIFICATION-2026-10-08-PAGER-STARTUP.md).
