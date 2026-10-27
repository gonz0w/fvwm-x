# Native viewport Form and browser scaling — 2026-10-27

Owner: Cam. Accepted bounded consumer adoption for #51/#69, following merged
[PR275 metadata](MODULE-UI-SCALE-HANDOFF-2026-10-27.md). Both parent issues retain
their frozen release acceptance; this batch does not close all native consumers.

## Outcome and boundary

A dedicated single-viewport Form may opt into `UIScale monitor`; `off` remains
the default for personal Forms. The browser emits this additive directive.
Consume the core-owned committed monitor snapshot and existing monitor ownership
policy. Support the browser's Text/TextHex, fixed Label, Input/InputHex, Button,
List and Timeout controls, including its empty idle-close timer without resets.
Reject unsupported compositions explicitly while retaining their complete 1x
presentation. No general Form rewrite, new toolkit, resident, dependency or scale
policy authority is selected. Parked #257/#258 work remains excluded.

## Observable contract

Prepare bounded owned font references and geometry from retained logical inputs
before changing any live table/metric/widget. Preserve stable DrawTable and X
window identities. Measure the returned font, including bitmap fallback, and
use one published spacing/gutter set for geometry, drawing, clipping and hits.
Refusal releases staging exactly once and retains the whole last-good view.
A successful event-turn publication updates fonts, GCs and geometry together,
then retires old font references. Repeated crossings must not accumulate rounding.

The core already preserves eligible logical client size across monitor scales.
Form must not multiply received/current client dimensions. Reflow inside actual
parent geometry. If the target minimum cannot fit, request an absolute minimum
at most once for that target/minimum tuple and retain old rendering until an
actual suitable ConfigureNotify. New targets supersede pending ones; no polling
source or resize-request loop. Metadata, ConfigureNotify and collection commit/
abort are the retry events. Startup metadata can arrive before the frame exists.

Automatic scaling never advances collection generation or update_serial. Keep
selection/range/focus/top identity, unsent text, character cursor/history and
native child windows. A pending collection publication defers font/layout
adoption until commit/abort. Explicit F7 ListLayout continues to use retained
logical cell units and exact acknowledged collection generations; no permissive
Go generation-drift protocol is introduced. Resource generations remain distinct
from topology epochs and quarter factors.

UIScaleStatus uses a bounded caller token and reports current versus desired
scale, topology identity, committed/requested minima and finite readiness state.
Status observation has no retry side effect. The installed manual records the
final literal field grammar; no synthetic acknowledgment establishes adoption.

## One combined checkpoint

Reuse one incremental Linux profile for affected C/Go compilation and policy;
use scoped sanitizers for new ownership. No Go concurrency change is selected,
so no automatic race repeat. Actual Xorg/RandR checks cover 1/1.25/1.5/2, mixed
monitor movement in both directions, automatic physical-fact change/removal,
replay/restart, initial scaled mapping, minimum-size deferral/supersession,
4096-entry bounds, F7/refresh and pending publication, drafts/cursor/selection,
keyboard actions, drawing/hit geometry, bitmap fallback and unchanged legacy
Forms. Capture Slate, Dune and enlarged Classic from that same run, inspect
pixels and publish a small source-bound set. Report failed/unavailable cases
honestly; do not convert font-size simulation into actual scale evidence.

No full OS/compiler/install matrix is triggered merely by this PR. The broader
#51 six-target consumer and release gates remain open; #69 can close only if all
of its frozen rows are actually established by retained and current evidence.
The PR269 performance reference is 3/5 after PR275; this implementation becomes
4/5 on merge, so a periodic comparison is not due. Lead owns tracker/resources/
integration; selected Temper routes retain separate review. Terra's capacity
failure selects the recorded explicit Sol fallback for bounded Go/docs work.
