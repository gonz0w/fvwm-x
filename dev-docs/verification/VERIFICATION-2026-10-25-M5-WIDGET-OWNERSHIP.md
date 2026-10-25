# M5 Script common widget-resource ownership

Status: focused checks pass; broader qualification pending. Owners: SCRIPT-01
#4 / CLEAN-01 #34. This finite widget family does not close either broad issue.

## Integrated foundation

All prior open PRs are merged under explicit maintainer authorization:

- PR242: `5094080af751f99772d720ea5581018a7ecccd3c`, tree
  `496f30fa6ff93e7d399a7c825be8a224d77c008e`.
- PR228: `389a02203f1c6a6fc3764f77d22d566a42ba5066`, tree
  `ae7300ec0b0a8bdee8de123474f7616d8260608b`.
- PR237: `d5187be3b8abe67c2dd62e7d818c6f45884b66fc`, tree
  `085aa76e87ef5530be3ff7398cdce0da7cdc6c00`.

Canonical signatures/DCO and exact reviewed trees are verified; completed
branches are retired and Project/labels Done. A final GitHub readback shows no
open PRs before this batch. PR228/237 integrate the latest maintenance evidence,
remove obsolete pickup prose and fill six JSON issue/five parent mappings.
Separate final reviews and local check/lint/link/whitespace gates pass; those
planning changes preserve runtime/Meson/INSTALL exactly and need no native rerun.
Their historical review hashes retain their original scope. #35/#36 remain
completed, #93 remains superseded, and queued hardening/library scopes stay open.

## Complete widget family

Fourteen ordinary widget destructors now share one private window/GC callback.
Swallow retains its client/placeholder callback and borrowed base GC. Shutdown
still completes every initialized callback before releasing any widget record.
The common second pass unloads retained fonts for all fifteen types, releases
owned named colors and icon resources, frees eight transferred/duplicated strings
and the record, then clears the slot. Repeated empty-slot release is harmless.

The existing completed-initializer count distinguishes constructed resources
from allocated/prepared partial widgets. Partial widgets do not own the base GC
or initialized color references. Colorset pixels remain borrowed; existing
named↔colorset transitions retain the ownership selector. Parser/default strings
are individually owned by each widget; unconstructed parser objects, AST/base
resources, font caches and Form cleanup remain outside this checkpoint.

Six initializers release their temporary cursor reference immediately after
XCreateWindow attaches it: CheckBox, RadioButton, List, HScrollBar, VScrollBar
and TextField. The window retains its server reference. Three other cursor
initializers already follow this pattern. Existing cursor-selection conditions,
configuration syntax, font replacement/refusal, resource revisions, image
loading, QuitFunc/message drain and dead-pipe early return are preserved.

## Before-control and focused acceptance

The new control links actual unchanged production objects. All eighteen saved
baseline production files match the canonical base. The original two-step build
passes; the intended failing control identifies missing font releases for
Rectangle, MiniScroll, SwallowExec, HDipstick and VDipstick, then records six
cursor creates/attachments and zero frees before aborting. The baseline source,
commands, binary identity and failed output remain in the canonical evidence.
These are ownership gaps, not an exploit or demonstrated runtime-growth claim.

The final actual-object control prepares/initializes all fifteen types and runs
real ChangeFont dispatch for shared/replaced fonts, invalid metrics and refused
loads. One sweep verifies 55 font unloads, 14 GCs, 15 windows, 60 named-color
releases, 45 pixmaps, 15 pixel vectors and 180 watched heap frees. All nine cursor
initializers attach before releasing their references. Additional controls cover
empty/repeated slots, four partial stages, borrowed colorsets/base GC, and empty,
adopted and already-cleared Swallow placeholders. Counts verify the actual owner
calls with wrapped external dependencies; unchanged font-cache internals are not
reimplemented. Source review plus actual-X exits establish ShutdownX's caller
ordering; the unit's manual two passes alone are not an execution of ShutdownX.

One reused Linux ARM64 checkpoint passes on its first combined invocation:

- GCC normal and Clang scoped ASan/UBSan each pass four units: widget owner,
  icon owner, geometry and widget math. Leak detection and halt-on-error remain
  enabled; no suppression or expectation/deadline change.
- Nine actual-X all-widget observations pass: fifteen startup windows, font
  changes, named↔colorset transitions, font-option update/recovery, normal exit
  and refusals before preparation and during layout preparation. The font-option
  case proves continuation, not rejection; the owner unit and retained actual
  geometry fixture supply rejection/recovery evidence.
- Actual icon conversion/replacement/refusal/retry/expose/exit, existing geometry
  and modal mutation/termination fixtures pass against the same built module.

The eight recorded build/test commands total 24.47 seconds in these existing
caches; that is this fixture's command time, not general development speed or
WM performance. No successful command repeats, and no additional gate failure
occurs after the intended old-source control. Repository checks/lint/links and
working/staged whitespace checks complete delivery.

## Evidence and qualification limits

Private evidence: `builddir/codex-2026-10-25/m5-widget-ownership`.
Twenty runtime/test inputs plus two checkpoint documents are captured; 278
relevant source/header/build/fixture hashes and modes match host/guest. Thirteen
selected executable identities and both generated configurations are retained.
The canonical `native-evidence.tgz` contains 143 regular payloads including its
manifest. All payloads and gzip EOF/CRC validate. The
[machine receipt](VERIFICATION-2026-10-25-M5-WIDGET-OWNERSHIP.json) records exact
source/commands, original failure, results, output and archive identities.

The existing all-widget fixture captures one native 800x600 screenshot, inspected
for surviving controls after font/color updates. This uses controlled legacy
colors/fonts; no new visual design or primitive is added. It is not a new
Slate/Dune/classic full-theme qualification. Prior theme evidence stays historical.

No fresh BSD, AMD64, installed-path, complete compiler/feature/theme matrix or
native-CPU/physical/toolkit/compositor/release acceptance is claimed. No OS API,
module packet, mandatory dependency or install rule changes; accumulated gates
remain with their owners. Scoped owner leak checks do not claim whole-module,
interpreter/global/font-cache leak cleanliness. Broader #4/#34 remain open.
Periodic PR238 reference is 4/5 after PR242; this merge reaches 5/5, making the
next regular comparison due. No performance improvement is assumed.

Separate final source/evidence review is clear. Exact source, raw outcomes,
sanitizer options, output identities, count assertions and archive payloads are
verified; prior merge API facts remain attributed to the lead. Guest and host
raw duplicates retire after exact archive and active-reference checks, retaining
the canonical archive, screenshot, sources and build caches. Signed author-DCO
delivery remains required. Requested
Astra/high analysis, Sol/high implementation and separate Sol/high review follow
the project routing policy; runtime model metadata remains unverified.
