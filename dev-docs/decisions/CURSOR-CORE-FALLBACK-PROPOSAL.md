# Deferred image-cursor fallback — approved 2026-10-06

## Current compatibility authority

The quoted broad approval rule below records the policy at the time of this
historical decision. It is superseded by the user’s
[configuration-only compatibility scope](CONFIG-COMPATIBILITY-SCOPE-2026-10-07.md).
It creates no present approval hold for internal/runtime behavior alone.

Tracker: IMAGE-01 / INPUT-01. Status: approved by the user on 2026-10-06; production activation stays off.

## Concrete decision

Use FVWM's existing bitmap cursor conversion for deferred image cursors when the
build/server cannot use the prepared ARGB32 path, instead of reproducing
Xcursor's threshold/median/ordered/diffusion conversion algorithms in a new
incremental subsystem. The user approved the simpler conversion on 2026-10-06. This keeps the
codebase smaller while accepting the documented cursor-pixel differences.

The affected inputs are deferred SVG, compressed XPM and ambiguous/misnamed image
cursors. Ordinary recognized PNG/XPM/bitmap cursor paths and native Xcursor
files/animations retain their current loader. The default color-capable path
preserves premultiplied alpha, scaled/explicit/XPM hotspots and cursor dimensions.

The fallback applies when Xcursor is disabled, XRender is disabled/unavailable,
or Xcursor reports that the display should use core cursors (including
XCURSOR_CORE / Xcursor.core). Builds without Xcursor already use FVWM's bitmap
conversion. With Xcursor present, deferred inputs will use FVWM's existing
black/white threshold and alpha cutoff, and will no longer use XCURSOR_DITHER's
median/ordered/diffusion choices. A build with Xcursor enabled but WM XRender
disabled also uses this bitmap fallback, even if the separate Xcursor library
could make a color cursor. Optional recoloring remains supported.

## Independent observation

The normal Linux probe runs a four-by-four cursor with alpha 128. With
XCURSOR_CORE=1, the actual existing PImageLoadCursorFromFile/Xcursor path shows an
opaque white pixel. The prepared FVWM bitmap path shows a transparent pixel.
Both report hotspot (2,2). This is an output difference, not merely a timing
change covered by the earlier asynchronous approval.

The comparison uses XFixesGetCursorImage on the actual server cursor. The matching
upstream source was read from the official X.Org libXcursor 1.2.1 source archive:
[upstream source](https://www.x.org/releases/individual/lib/libXcursor-1.2.1.tar.xz).
Its display capability policy handles XCURSOR_CORE; its cursor implementation
contains the four conversion choices. The current proposal reuses FVWM's existing
bounded bitmap converter instead of copying those algorithms.

## Prepared implementation

All deferred cursor jobs use the existing scheduler and its one active worker.
ARGB32 pixels are premultiplied/uploaded in bounded steps, with no palette or
XGetImage copy. Cursor creation runs inside the existing owned X request range
and is acknowledged before publication. Refused or superseded creation retains
the old cursor. The ready uncached lease transfers one cursor handle to its slot;
its temporary image resources are released. Native cursor animation remains on
its existing path. No new mandatory library is introduced.

Focused normal, sanitizer and optional-profile results are recorded separately;
they verify implementation mechanics. The user approval supplies the separate
compatibility decision; it does not qualify production activation.
Production activation, broader owner acknowledgement and the combined Linux/BSD
checkpoint remain pending.

## Alternative

Preserve Xcursor's exact core-cursor appearance and its dither preferences by
implementing their bounded conversion in the same scheduling system. This adds
algorithm/configuration coverage and maintenance work. Returning to synchronous
image decoding is not recommended for the responsiveness goal.

Root [AGENTS.md](../../AGENTS.md) requires: “A specific compatibility break
requires an explicit user decision.” That requirement applies to these changed
core-cursor pixels.
