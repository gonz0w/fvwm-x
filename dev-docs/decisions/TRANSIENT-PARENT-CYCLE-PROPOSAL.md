# STACK-01: finite transient-parent traversal for Layer

## Current compatibility authority

The quoted broad approval rule below records the policy at the time of this
historical decision. It is superseded by the user’s
[configuration-only compatibility scope](CONFIG-COMPATIBILITY-SCOPE-2026-10-07.md).
It creates no present approval hold for internal/runtime behavior alone.

Status: approved 2026-10-07. The user chose the originally invoked window.
The guard is activated; focused Layer acceptance passes; the combined checkpoint is locally qualified. This is
separate from the frozen atom/expose checkpoint.

## Concrete control

`get_transientfor_top_fvwmwindow` follows StackTransientParent links before Layer
selects its effective target. The existing lookup refuses self/root/missing
parents, but a two-window cycle repeats indefinitely. A finite actual-core
control observes 16 alternating lookups, then changes only its private fixture's
transient flags so the control can finish. It does not stall a live user session.
The unchanged source is the frozen atom/expose checkpoint; its stack.c is the
same as the accepted JSON checkpoint.

The baseline and complete archive are under
`builddir/codex-2026-10-07/atom-expose/transient-cycle-control-evidence.tgz`.
Its SHA-256 is
`3ad8ac94ffb555c9d90134f154114959ddb3761b0174e4658f31ca0cf92e53da`.
An initial fixture-only absent setter macro was corrected to the existing field.

## Recommended fallback and candidate

Detect repeated parent records with Brent's constant-space cycle check. Keep
one parent lookup per step for acyclic chains, retain current indexed identity
lookup, and impose no arbitrary depth limit. On a detected cycle, log the reason
and use the originally invoked window as Layer's target. Existing layer/group/
transient processing then follows its current rules. Do not edit the client's
hints or retain pointers past the command.

The alternative is to refuse Layer for that cyclic chain, leaving window layers
unchanged. Both options prevent the repeated traversal; the recommendation keeps
the invoked window directly usable. Ordinary acyclic ancestor selection remains
unchanged under either option.

A reviewable private candidate is in
`builddir/codex-2026-10-07/transient-parent-cycle/candidate/`: the exact stack
function change, a pure policy header, unit test and bounded actual-core control.
That reviewed guard is now activated in production; its initial review evidence
remains separate from acceptance of the installed code. The core candidate detects the
two-window cycle after three lookups, preserves its transient flags and returns
the original target. Its acyclic control retains the ancestor with one lookup.
The unit checks 10000 distinct acyclic nodes, 992 cycle/prefix cases and checked
counter boundaries; GCC and leak-enabled ASan/UBSan pass locally. An initial
private unit include path and GCC identity-marker initialization warning are
retained. The identical policy also passes on native Linux AMD64 and FreeBSD/OpenBSD
AMD64/ARM64. All six platform source identities match; real-window Layer
acceptance follows adoption. The private candidate archive SHA-256 is
`f589f883472e8ae5eebcdc156c6f42b4528a13defe1ddd867b0eee99e3b6faa6`;
its validated evidence and native result records are in the same review directory.

The fallback defines new Layer behavior for cyclic client/configuration hints.
[AGENTS.md](../../AGENTS.md) says: “A specific compatibility break requires an
explicit user decision.” I am treating this fallback choice as that compatibility
decision; it is distinct from the already approved redraw timing contract.

## Acceptance after approval

Keep the finite baseline; add pure native policy coverage and actual Layer
checks for two/three-node cycles, prefixed cycles, acyclic/deep/missing/self/root
parents, different layers, stack/focus order, grouping, window retirement and
subsequent commands. Keep cycle detection operation-local, preserve ordinary
ancestor lookup counts, and reuse existing window lifetime and command owners.
Qualify with the next coherent stacking/query batch, without changing the
already frozen atom/expose source or carrying its results forward.

The final four-story source and acceptance scope are recorded in the [stacking/query report](../verification/VERIFICATION-2026-10-07-STACK-PARENT-QUERY.md). Normal public client hints reject cycles; the defensive cyclic tests use private internal records.
