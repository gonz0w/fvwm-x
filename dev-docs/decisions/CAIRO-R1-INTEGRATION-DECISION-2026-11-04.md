# R1 Cairo drawing integration decision

Status: **selected R1 production decision; remaining prototype research deferred to R2**. GRAPHICS-02 [#211](https://github.com/gonz0w/fvwm-x/issues/211).
The selected G6 outcome is to reject production integration of the evaluated
private Cairo drawing prototype for R1 and retain the existing native Form and
Buttons drawing paths. The decision is limited to that prototype; it does not
change the existing isolated SVG renderer, adopt a text engine or toolkit, or
select a general renderer replacement.

## Reason and boundary

The [Linux comparison](../verification/VERIFICATION-2026-11-04-DRAWING-LAYOUT-CHECKPOINT.md)
covered selected native, Cairo and ordinary controls, including clipping,
fallback and lifetime cases and three controlled theme profiles. Fifteen
performance runs compared five variants in three fixed orders. The observed
action-latency ranges overlap and do not establish a causal speed gain. In this
profile, Buttons RSS after the workload was 7856 KiB for Cairo versus
6536–6548 KiB for native graphics, approximately 1.3 MiB more. Those are scoped
process observations, not package cost or a whole-desktop measurement.

The shared checkpoint records 55 successful GUI invocations covering 54
selected cases and one affected Cairo positive, nine screenshots, five native
C/sanitizer executions and nine focused Python methods. Previously completed
cases retain their original source and executable bindings, as the report explains.

The prototype adds scratch-resource and adapter ownership while removing no
production code. G5 fixed an observed scratch-resource failure after destination
destruction and qualified the corrected private path within the Linux checkpoint.
The retained native drawing path therefore meets the current integration choice
without taking on the evaluated Cairo path's added runtime and maintenance cost.
This is a release decision based on available evidence, not a claim that native
drawing is universally faster or that other Cairo uses have the same cost.

Keep the [comparison prototype](DRAWING-COMPARISON-PROTOTYPE-2026-11-03.md)
and its evidence available while evaluation acceptance remains open. Its
`graphics-eval` feature stays default-disabled and its comparison targets remain
non-installed. This decision does not authorize broad source removal, change
ordinary build dependencies, or assert that development and hypothetical
adoption costs are zero.

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
Complete implicit synchronous-roundtrip measurement also remains
unqualified; issued X requests cannot stand in for it. This non-adoption decision
can be recorded while those frozen evaluation criteria remain pending. It does
not close #211, complete R1 or waive release and platform gates.

Reconsider integration only through a separately selected, finite proposal that
names a concrete user benefit or removable ownership, demonstrates it against
the retained native baseline, and pairs the result with comparable costs and
profile coverage. That proposal must resolve applicable lifetime, fallback and
measurement evidence rather than treating this evaluation as automatic approval.
