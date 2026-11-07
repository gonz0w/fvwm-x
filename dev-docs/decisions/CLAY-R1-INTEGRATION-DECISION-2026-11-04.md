# R1 Clay layout integration decision

Status: **selected R1 production decision; remaining prototype research deferred to R2**. LAYOUT-01 [#212](https://github.com/gonz0w/fvwm-x/issues/212).
The selected G6 outcome is to reject production integration of the evaluated
private Clay row prototype for R1. Retain the existing
[native responsive panel and collection layout](NATIVE-LAYOUT-2026-10-26.md),
including its shipped panel and optional browser consumers, alongside supported
configured legacy geometry.

## Reason and boundary

The [Linux comparison](../verification/VERIFICATION-2026-11-04-DRAWING-LAYOUT-CHECKPOINT.md)
covered selected actions, geometry, updates, native fallback and three controlled
theme profiles. Three fixed-order runs per variant included twelve actual resize
samples each. Median-of-run-medians reflow was 55.639 ms for the native layout
control and 59.631 ms for Clay. These timings include IPC, server scheduling,
queries and polling; they are not solver-only measurements and do not establish
a causal speed gain.

The shared checkpoint records 55 successful GUI invocations covering 54
selected cases and one affected Cairo positive, nine screenshots, five native
C/sanitizer executions and nine focused Python methods. Previously completed
cases retain their original source and executable bindings, as the report explains.

For this Linux ARM64 profile, the Clay experiment requested a 414400-byte arena
and added 102584 text, 456 data and 1256 BSS bytes to the Buttons module relative
to the native layout control. Arena allocation, ELF sections and RSS measure
different costs. The pinned source, adapter and upgrade burden add maintenance;
the experiment removes no production code. Together with the measured workload,
these costs do not justify R1 integration of this candidate. This does not claim
that native layout is universally faster or that other Clay uses would have the
same results.

The [comparison contract](CLAY-LAYOUT-COMPARISON-2026-11-04.md) limits the
candidate to uncompressed rows with equal positive weights. Its GROW distribution
intentionally differs from native weighted surplus. Unsupported weights,
compression, refusal and invalid inputs or output retain the native fallback.
Keep this private experiment and its evidence while evaluation acceptance remains
open: `layout-eval` stays default-disabled and its comparison targets remain
non-installed. This decision adds no mandatory dependency or toolkit, and does
not authorize broad deletion or claim that development and hypothetical adoption
costs are zero.

## Owner-approved R1 cutoff — 2026-11-07

Cam approved the [R1 cutoff](REJECTED-PROTOTYPE-R1-CUTOFF-2026-11-07.md).
Remaining operation-level implicit-wait measurements, OpenBSD comparison package
closures and frozen prototype-specific comparisons move to
[R2 #366](https://github.com/gonz0w/fvwm-x/issues/366). These unavailable results
are not passing or zero. The existing reject decision and available source-bound
evidence complete the revised R1 evaluation outcome; no production integration
follows. Production native consumer requirements and all R1 safety/platform/
release checks retain their existing owners and acceptance.

The following original acceptance description is historical for those transferred
prototype-research requirements. It does not reinstate them as R1 blockers.

## Remaining acceptance and reconsideration

The [platform cost ledger](../verification/VERIFICATION-2026-11-04-DRAWING-LAYOUT-COSTS.md)
records each target's actual build/linkage/package/install evidence and gaps.
Linux runtime observations do not qualify pending BSD process measurements.
Evaluation measurement gaps, including unqualified implicit-roundtrip collection
for the shared drawing comparison, remain explicit. This non-adoption decision
can be recorded while the frozen criteria remain pending; it does not close #212,
complete R1 or waive release and platform gates.

Reconsider integration only through a separately selected, finite proposal that
names a concrete user benefit or removable ownership, demonstrates it against
the retained native consumers, and pairs the result with comparable costs and
profile coverage. A new proposal must address applicable geometry, fallback and
measurement evidence; this evaluation does not automatically select future work.
