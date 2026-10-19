# Style update work checkpoint

Owner: STYLE-01 [#38](https://github.com/gonz0w/fvwm-x/issues/38).
Base: merged PR #115, `57b3ee0bf3cb10b67c9a595c934aff6169ba3b6e`.
Implemented; focused qualification in progress. One combined checkpoint follows.

## Three related improvements

1. `update_style_colorset` marks normal/highlight border styles only if one of
   their eight stored colorset references matches; zero retains the legacy
   blanket border refresh for inherited normal/highlight colors. Partial arrays retain their
   implicit zero references; one-number configuration still fills all components.
2. `update_fvwm_colorset` lets existing style/decor owners schedule global window
   updates when referenced. Menu and tear-off updates keep their immediate owner;
   existing pending work is never cleared. An unused nonzero colorset no longer forces
   focus deletion/restoration, lookup/application and server/pointer grabs.
3. `apply_window_updates` creates its operation-local command context only for
   an admitted sticky/icon-sticky handler. Necessary contexts retain their original
   creation/application/release point and borrowed lifetime, inside the same grab.

There is no new dirty map/cache, retained plan or moved update/grab/publication
barrier. Startup's unconditional update owner, UpdateStyles/Refresh/RefreshWindow,
outer-loop ordering and required callbacks remain. Arrays are checked in bounded
8-element walks without retained metadata. Configuration syntax stays unchanged.

## Focused controls

The new actual-core fixture uses two managed X clients. It checks all eight
normal/highlight references, unrelated colorsets, partial-array zero and single
colorset replication, ordinary/unused update costs, sticky and icon-only sticky
contexts, and pending style/decor work. Applied border widths are checked in
server-side client/frame geometry and `_NET_FRAME_EXTENTS`; reference arrays and
change masks are checked against the authoritative style/window records. This
is not a physical pixel/toolkit/compositor qualification.

The unchanged production baseline schedules one unnecessary server grab and two
contexts for the selected unused-colorset pass, plus two contexts for an ordinary
non-sticky pass. Candidate expectations are zero in each. Sticky updates retain
two necessary contexts; icon-only updates retain one for the iconified client.
These are selected operation counts, not a universal latency claim.

The first baseline fixture used the nonexistent `IconStickyAcrossDesks` token;
its assertion failed. The fixture now uses supported `StickyAcrossDesksIcon`.
Original failure/source/core log are retained; production was not changed for
that fixture correction. Existing update-barrier, style, wildcard, EWMH, module
and real-client drivers retain their separate checks.

Ignored owner: `builddir/codex-2026-10-19/style-update-work/`. Changed native files
receive fresh mtimes; archive/hash/mode and exact Git source identity are checked.
Reuse warm builds and one six-target combined checkpoint. Retain the known
OpenBSD AMD64 CI-01 two-second worker/SVG startup omissions and install gap,
without retries for green. VESA #93 remains deferred; native-CPU sanitizer,
physical hardware/toolkit/compositor and release gates remain separate.

## Review correction before publication

Review found that colorset zero's legacy border invalidation also refreshes
implicitly inherited normal/highlight colors. The final source preserves the
zero blanket border refresh and global scheduling. A new control replaces the
previous rules with fresh nonzero border sets, changes colorset zero's foreground
and verifies both authoritative normal/highlight foreground values after update.
An isolated build of the original candidate fails its new scheduling assertion;
corrected objects pass all twelve markers. Warm source/builds remain untouched.
Two diagnostic preparations expected a later assertion name; their actual earlier
failure and all inputs/logs are retained. Only the final private check executes
the correction after reusing the captured failing control.

The original combined checkpoint continues on its frozen `1eca95fdd` source.
Final correction gets affected-only checks, fresh stages/client probes and default
BSD X coverage after each original target finishes. No full matrix repeats; broad
results and final affected results retain distinct source/archive identities.

## Image-effects fixture acknowledgement correction

The original OpenBSD AMD64 broad run records one new image-effects timeout.
An unchanged isolated control reproduces it; the pre-batch core passes while
the corrected production core initially fails. A passive record trace passes,
showing sensitivity to observation timing. A deterministic 100-ms diagnostic
delay before the immediate acknowledgement proves the ordering error: the
scheduled echo arrives first, command() discards it while waiting for the later
immediate acknowledgement, and the following wait times out. Production
scheduling delivers the required echo; this is not evidence of a VESA defect.

The shipped fixture sends Schedule directly and waits for its scheduled echo,
which is its own acknowledgement. The 30-ms schedule, two-second responsiveness
assertion, five-second connection/effect limits and all worker/publication
controls remain. Only this Python fixture changes after the seventy-entry
production correction checkpoint. The two affected image-effects registrations
will run on their nine registered profiles; the minimal profile has neither.
No full matrix, production rebuild or install repetition is required for this
fixture-only change. Original failures, trace/control sources, rejected runner/
object-mapping preparations and corrected affected results remain distinct.
