# Style update work checkpoint

Owner: STYLE-01 [#38](https://github.com/gonz0w/fvwm-x/issues/38).
Base: merged PR #115, `57b3ee0bf3cb10b67c9a595c934aff6169ba3b6e`.
Composed affected checkpoint passes; required CI/SVG/install omissions remain.

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
pass on their nine registered profiles; the minimal profile has neither.
No full matrix, production rebuild or install repetition is required for this
fixture-only change. Original failures, trace/control sources, rejected runner/
object-mapping preparations and corrected affected results remain distinct.

## Source-bound composed checkpoint

Three style-update improvements pass a composed affected checkpoint: one
original broad run (2005/2006 executed standard, 441/441 optional), seventy
production-correction checks and eighteen fixture-correction checks. The
original image-effects timeout is traced to a discarded early acknowledgement
and corrected without changing production deadlines. Six original sanitizer
checks, three corrected actual-core checks, nine final fresh stages and four
final authenticated BSD displays pass. Two known required OpenBSD AMD64 CI/SVG
omissions and its install gap remain without retries.

Original signed/DCO broad source: `1eca95fdd0df2080ea43240ceebcf68c530d8c26`;
archive `120d8b9ceb6cc12970bfb9efe3c8b09cc7ac61dc5e92c33097a97116649bc7a8`. Its full results remain historical
pre-correction evidence, not a full run of the final source.
Final signed/DCO corrected source: `b52e5bea9c21a7349c369d13b86e54c42684784c`; tree `be6adc79c83d1fbbb91e276fe7d14888dd8e75b3`;
archive `046d2315c8e96c1c5fba9764e00eaa6162c400b443affc0e475176f82f2e8784` (2006 regular inputs).
Final fixture-only source: `3d01112be6080a8b0500d92c200b74a3eb0e9f0b`; tree `86178c7128affe04b5131ccb14a0ce77ac3d28bf`;
archive `157dba2afffb7fa40f9be9a5f1c0914d1a30e55962693e0a37a669ae3542bbb5` (2006 regular inputs).
All C/header/build/manual inputs are unchanged from the corrected production
source. Only the image-effects Python driver and this report differ.
[Machine receipt](VERIFICATION-2026-10-19-STYLE-UPDATE-WORK-COMBINED.json) retains all three source scopes, raw archives,
executed script/log/input hashes, profile and install identities, and exact delta.
Final upkeep changes docs/evidence only; all final production/test/manual inputs
stay exact to the final fixture snapshot. The seventy-entry correction checkpoint
retains its production-source identity; its inputs are unaffected by the later
image-effects driver change.

| Scope | Passing result |
| --- | --- |
| Original broad executed / required standard | 2005/2006 executed; 2005/2008 including omissions; original fixture failure retained |
| Original software/minimal | 441/441 |
| Final affected, six targets/ten profiles | 70/70: update-work, style-match/filter/profile, shared wildcard, update barriers and EWMH state |
| Final image-effects fixture correction | 18/18 across nine registered profiles; minimal has neither registration; no rebuild |
| Scoped ASan/UBSan | Original six pass; final three actual-core fixtures pass; unchanged three pure-policy scopes retained |
| Final fresh stages | 9 / 135 code / 88 catalogs |
| Final authenticated default BSD X | 4 displays / 26 fixtures, unauthenticated refusal and clean exit |
| Final staged geometry | 18 invocations / 126 cases |
| Final real-client regression | 410 cases / ten complete invocations |
| Final update work | Fifteen drivers / eleven observation groups each, including implicit zero foreground refresh |
| Portable repository checks | 166 passes / eleven native-Meson skips, once |
| Compiler diagnostics | Eleven original and eleven final logs; no diagnostics in affected C files |

All final drivers retain zero selected unused nonzero-colorset grabs/contexts
and zero ordinary non-sticky contexts. Sticky/icon-only contexts and all pending
style/decor/barrier controls pass. Colorset zero conservatively keeps its legacy
global scheduling and blanket border refresh, including inherited colors.
The original candidate's scheduling assertion fails the added isolated zero
control; corrected objects pass. Preparatory assertion-classifier errors remain
archived and do not represent extra native matrix executions.

Warm owner builds are reused, Ninja two jobs and serial Meson tests. At most six
native target jobs run after an 18-logical-CPU / 128-GiB capacity check; a target's
final job starts only after its original capture completes. Source/build/archive
capacity and source hashes/modes are checked. No tested source/build is mutated
by the isolated probe, no full suite repeats and no VM restart/disk/package/
graphics/deadline change is introduced. Final installs and clients use final
binaries; unchanged module inputs retain the original broader evidence.

OpenBSD AMD64's original broad scope passes 242/243 executed entries. The one
image-effects failure remains failed in that scope; the traced fixture correction
passes all eighteen affected entries separately on unchanged final production
binaries. The deterministic old-acknowledgement-order control fails after the
scheduled echo is delivered; the corrected early-delivery control passes.
It retains two required omissions under CI-01 (two-second asset worker timeout, PR #113 runtime
`2cf1f2f470d65cf1ebfb44dfe69a5edce11e7118`) and IMAGE-01 (SVG/MFL startup refusal,
PR #109 runtime `97c728358660675353de35f866c833c926b4f203`). Final affected checks
and built probes pass; neither omission is retried or counted as an executed pass.
Its final fresh install/two staged geometry probes remain unperformed. Both broad
and final owners retain a nonzero gate exit for inherited required omissions;
successful capture is separate. Causes remain unproved; VESA #93 stays deferred.
Physical native-CPU sanitizer, hardware/toolkit/compositor and release acceptance
remain separate.
