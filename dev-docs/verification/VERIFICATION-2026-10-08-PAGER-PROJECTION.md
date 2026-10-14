# Pager window projection and metrics — 2026-10-08

Status: implemented; focused checks passed; full qualification pending.
Owner: module construction/numeric UI follow-up in PROJECT-TRACKER.

Four stories complete the current arithmetic/configuration batch:

1. Project window endpoints with wide arithmetic and bounded X coordinates and
   extents. Preserve representable endpoint rounding. Inconsistent tracked
   monitor dimensions produce safe absent geometry.
2. Restore Pager coordinates with quotient/remainder decomposition and checked
   product-plus-correction evaluation. Preserve ordinary tracked-monitor offset
   rounding and saturate unrepresentable core coordinates to the integer range.
3. Parse window border/content minimum into correctly typed non-negative values.
   Refuse malformed/out-of-range input without replacing state. Keep the content
   minimum separately from bounded rendering geometry so border changes cannot
   underflow a previously reduced derived value. Document rendering bounds.
4. Remove the unused SuffixToPercentValue declaration/implementation after a
   repository-wide caller search. This static-library API has no shipped callers
   or configuration-facing syntax. Also move geometry guidance out of AUTHOR.

Frozen archive SHA256:
`3074b1925e9f4bc7a8c7a2e2e13ff1ce8a1e0c0879bfc793240fa7976cd4d266`.
There are1407 files and ten owning paths changed from sourcece40.

The exact preceding scoped Pager and new real-window regression driver reproduce
an endpoint arithmetic failure. The real fixture supplies ordinary320×240 frame
sizes and extreme representable coordinates, followed by ordinary geometry.
The retained source/binary identity, driver and diagnostic are in the owning
builddir/pager-projection-production baseline archive. The original attempt
exited through the test client's default X error handler when the module/window
had already disappeared. Exact surviving harness logs were retained, and its
specifically identified owned Xvfb was stopped. The driver now handles expected
BadWindow query races while recording/refusing other client X errors. The
corrected pre-change attempt retains the intended production diagnostic.

Normal/scoped Linux ARM64 passes11 C +23 actual checks per profile. FreeBSD and
OpenBSD ARM64 each passes11 C +21 actual checks. Complete305/127/146 payloads
validate all source hashes/modes, exact executed runners and outcomes; native
archives identify14 compiled executables. Final scope adds real frame updates,
invalid/zero window metrics, following UI and EOF. Pure controls compare ordinary
forward and inverse results against the prior calculation, including negative
positions and tracked monitors; boundary controls cover both saturation signs,
invalid geometry/state and metric bounds. Scoped C leaks are enabled; actual
process leaks are disabled. No final-source focused failure/deadline change.

All four stories are folded into the undispatched parser/grid/dimensions batch.
The combined14-story numeric/configuration/rendering checkpoint changes15 owning
paths from sourcea653. One matrix requires1800 standard entries/eight profiles,
120 installed identities,288 BSD default-X invocations,optional230/210 and116
scoped checks (40 C leak-enabled,76 actual leak-disabled). Dispatchers require
strict sourcea653 qualification. No separate parser/grid/dimensions/projection
full matrix runs. Earlier focused source and results remain retained.

Nested Pager drag servicing, target/monitor lifetime during callbacks, remaining
pointer/layout arithmetic, configured callbacks and resource-area bounds remain
open. Native Linux release sanitizer CI and physical/toolkit/compositor coverage
remain separate requirements.
