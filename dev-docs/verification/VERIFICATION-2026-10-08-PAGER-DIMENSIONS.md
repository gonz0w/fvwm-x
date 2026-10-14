# Pager derived dimensions — 2026-10-08

Status: implemented; focused checks passed; full qualification pending.
Owner: module construction/numeric UI follow-up in PROJECT-TRACKER.

Four related stories follow parser/grid preparation:

1. Prepare automatic/aspect window sizes with wide arithmetic. Compare before
   a third multiplication in scaled extents. Keep ordinary aspect/scale rules;
   bound derived sizes at the X11 request boundary.
2. Prepare desk cells and label space without signed overflow or zero-sized
   child windows. Use typed XGetGeometry outputs before publishing dimensions.
3. Round icon sizes with a wide page step, and prepare negative-offset window
   coordinates without overflow. Preserve ordinary rounding and placement.
4. Prepare viewport endpoints with wide arithmetic, positive extents and safe
   absent-monitor fallback. Bound desk/shape X request sizes and coordinates.

The immutable source archive SHA256 is
`ce4045cc1f8a357d5ce325d4a1422293bad92adfac80b4a1494c50280566d9b2`.
It contains 1407 files and changes ten owning paths from grid source6e01.

The retained real pre-change scoped Pager binary reports signed overflow in
set_desk_size: INT_MAX rows multiplied by a 15-pixel label. Exact previous source
hashes, binary identity, unchanged regression driver and diagnostic are retained
under `builddir/codex-2026-10-08/pager-dimensions-production/baseline-evidence.tgz`.
The control adds a one-desk INT_MAX × INT_MAX empty grid with explicit geometry
and ShapeLabels. Final checks verify the mapped Pager, one small shape rectangle,
subsequent extreme desk title updates and EOF without the diagnostic.

Normal/scoped Linux ARM64 passes eleven C and twenty-three actual checks per
profile. FreeBSD/OpenBSD ARM64 each passes eleven C and twenty-one actual checks.
Complete291/125/144 payloads validate full source hashes/modes, exact runners and
outcomes; native evidence records fourteen compiled executable identities. Pure
controls retain ordinary expected dimensions and cover aspect, triple products,
cell divisions, icon page-step overflow, viewport endpoints and X request bounds.
Scoped C leaks are enabled; actual X-process leaks are disabled. No new focused
failure or deadline change occurred in the final dimensions source.

Parser9f17, grid6e01 and these four undispatched dimension stories share one
natural ten-story numeric/configuration/rendering checkpoint: fourteen owning
paths from sourcea653. Earlier focus evidence remains retained. One combined
matrix requires1800 standard entries/eight profiles,120 installed identities,
288 BSD default-X invocations,optional230/210 and scoped116 (40 C leak-enabled,
76 actual leak-disabled). It follows strict sourcea653 qualification; no separate
parser, grid or dimensions-only full matrix is dispatched.

Remaining window projection/minimum-size arithmetic, nested drag servicing,
configured callbacks and resource-area bounds remain open. This is neither a
whole-project security closure nor evidence of physical/toolkit/compositor or
native Linux release sanitizer coverage.

Four related window projection/metric stories are folded into this undispatched
checkpoint. Final source3074 is strictly focused-qualified and shares one
14-story1800/120/288 optional230/210 scoped116 matrix behind sourcea653. No
separatece40 matrix. [Projection](VERIFICATION-2026-10-08-PAGER-PROJECTION.md).
