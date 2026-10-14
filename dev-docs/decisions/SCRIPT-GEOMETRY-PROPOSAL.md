# Script geometry boundaries — approved and qualified

## Current compatibility authority

The quoted broad approval rule below records the policy at the time of this
historical decision. It is superseded by the user’s
[configuration-only compatibility scope](CONFIG-COMPATIBILITY-SCOPE-2026-10-07.md).
It creates no present approval hold for internal/runtime behavior alone.

Owner: SCRIPT-01. Status: approved, implemented and qualified on 2026-10-06.
[Accepted verification](../verification/VERIFICATION-2026-10-06-SCRIPT-GEOMETRY.md)
records seven full compiler legs, exact installs, default BSD X and scoped
sanitizers. Wider resource budgets and font shadow work remain separate.
The user selected refusal of invalid geometry with previous-state preservation.
This is separate from the approved SVG resource limits and value-rendering
clamps. The decision applies to window and widget geometry, not stored script
values or progress/scrollbar ranges.

## Concrete finding

The checked integer parser permits `ChangeSize 1 65536 30`. The instruction
stores width 65536 and sends it to `XResizeWindow`. Core X11 window extents are
unsigned 16-bit fields; 65536 becomes zero in the request. The module's fatal
X-error path aborts. An owned installed-Linux-ARM64 control records child exit
by SIGABRT, no post-command script marker, and continued core command service.
The control does not capture the X error text; the source identifies the zero
extent request and fatal error handler. This is a module finding, not a claim
that the core was compromised.

Evidence: `builddir/codex-2026-10-05/script-geometry-review/evidence.tgz` and
`control.py`. The scope is one actual runtime size boundary, not a complete
widget or font audit. Reviewed paths include `instructions.c` ChangeSize and
ChangePosition, `script.y` startup geometry, FvwmScript.c GUI creation and
widget font/icon-derived sizes.

## Recommended contract

- Accept window/widget extents from 1 through 65535 pixels and coordinates from
  -32768 through 32767 at X11 window request boundaries.
- Refuse a runtime ChangeSize/ChangePosition request outside these ranges with
  a useful diagnostic. Keep both the previous stored geometry and actual X
  geometry. Validate the complete operation before changing either field.
- Preserve existing zero/omitted widget startup sizing and ordinary font/icon
  minimum expansion. Validate the final derived geometry before creating or
  resizing an X window. Refuse impossible startup geometry with a diagnostic.
- Calculate font/text/icon layout with wide checked arithmetic before narrowing.
  Preserve all representable legacy results and rounding. A failed runtime font
  or layout change retains the previous usable state.
- Do not introduce an arbitrary smaller window-size cap in this decision.
  Resource budgets, visible-screen popup sizing, modal servicing and Unicode
  remain separate work. X11's representability limits are not memory limits.

Existing themes relying on coordinate wrap or extent truncation change
behavior under this contract. AGENTS.md says, “A specific compatibility break
requires an explicit user decision.” The approved drawing clamps for numeric
values do not decide window creation/resize behavior.

## Required implementation gates

Pure boundary/overflow policy units; actual-module refusal, state preservation,
normal resize/reposition, font replacement and recovery checks; full Linux/BSD
AMD64/ARM64 suites; exact staged installs and default BSD X; scoped local
sanitizers with native Linux release CI still reported separately.
