# Legacy strut and root work-area safety — 2026-10-21

Owners: EWMH-01 #18 / DATA-01 #6 / INPUT-01 #9; R11/W03/W06 in the
[requirement register](STANDARDS-2026-10-21-PROTOCOL-REQUIREMENTS.md).
Status: implemented; focused development checks pass; combined qualification
and maintainer review pending.

Three related repairs validate CARDINAL strut values before signed storage,
release both reservation records when the property is absent, and publish
current dynamic reservations in the bounded per-desktop root snapshot. The
existing all-zero static auto-hide behavior remains. Malformed replacements
retain the previous accepted records. Publication uses one window walk and
bounded desktop storage; it adds no XSync or event dispatch.

Acceptance is independent root readback for mutation, desktop movement,
viewport movement, base changes, ignore-style refresh and destruction; typed
shape/range refusal, recovery and static/dynamic ownership use pure and actual
core controls. The previous installed core is a source-bound negative control.
One combined six-target checkpoint reuses owner-controlled builds, stages ten
profiles and exercises authenticated default BSD servers. Detailed counts and
canonical evidence will be added after completion.

The first focused runner used an unregistered selector; no tests ran in that
attempt. Its copied previous testlog is not evidence of execution. The corrected
five-entry focus passed. A later builder edit initially passed the anonymous
BaseStrut record to an ewmh_strut helper; both compilers rejected the mismatch,
and no tests ran. Corrected explicit field initialization passed affected focus.
Original logs and affected completions remain preserved.

This checkpoint adopts the legacy full-edge clauses of EWMH 1.5 sections 3.9
and 5.9. Partial struts, nonsticky-page panels, broader topology and independent
multi-monitor root authority remain open. Existing per-monitor internal
work-area policy is retained. Opposing reservations from different clients may
overlap, producing a contained empty root work area. Native-CPU Linux sanitizer,
physical hardware/toolkit/compositor and final release gates remain separate.
PR144 remains the performance reference at 2/5 merged implementation checkpoints;
no comparison is due.
