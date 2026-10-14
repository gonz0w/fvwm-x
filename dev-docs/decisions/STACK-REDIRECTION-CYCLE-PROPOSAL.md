# STACK-01: cyclic parent redirection in Raise, Lower and RestackTransients

## Current compatibility authority

The quoted broad approval rule below records the policy at the time of this
historical decision. It is superseded by the user’s
[configuration-only compatibility scope](CONFIG-COMPATIBILITY-SCOPE-2026-10-07.md).
It creates no present approval hold for internal/runtime behavior alone.

Status: approved 2026-10-07; originally invoked window fallback activated. The approved Layer guard and
hint-validation/query batch are already activated. Production regression acceptance passes; the combined checkpoint is locally qualified.

## Finite control and scope

The shared `_raise_lower_recursion` path can repeat a cyclic, same-layer
StackTransientParent chain. An operation-local fixture uses two private records
and real allocated X windows. GCC function-entry instrumentation observes 16
recursive entries, then clears only that fixture's transient flags so it can
finish. A private address accessor exposes the observed function to the fixture;
no production traversal logic is replaced in this baseline.

Ordinary client hints already reject cycles. This is defensive handling of an
internally cyclic chain, not evidence that a normal application's hint reaches
this path or that a user desktop was compromised. The new validation guard also
rejects an already-cyclic parent chain without changing its other members.

## Recommended contract

If parent redirection repeats a record, unwind before stacking any parent.
Discard only the temporary branch-rank bonuses, then perform the operation on
the originally invoked window with parent redirection disabled. Existing
transient/group inclusion, layer, icon, stack/focus and module notification rules
still apply. Retain all client hints. Ordinary acyclic parent redirection,
per-level processing and icon/different-layer stopping remain unchanged.

This covers Raise, Lower and RestackTransients together. It adds no arbitrary
parent-depth limit. Deep acyclic recursion and quadratic closure/rank work remain
separate STACK-01 work; this candidate does not solve them. The alternative is
to refuse stacking for a cyclic redirection, with coherent module notifications.

The private candidate uses the already-tested constant-space parent-chain policy
plus a typed operation-local cycle result passed through the existing recursive
calls. The top operation owns the fallback; no global or retained window pointer
is added. Detection propagates back before restack work starts.

## Review evidence

`builddir/codex-2026-10-07/transient-parent-cycle/raise-cycle-candidate/` contains
the instrumented review source, fixture and `production-stack.c` with the test
accessor removed. `raise-candidate-source-fingerprints.json` records their bytes.
The archive SHA-256 is `a2d7f42f3b22be628a3e01515a6b5c13040c7930f1ab49af5ad4b4abe45293a9`.

The bounded Linux actual-core candidate detects the two-window cycle after three
parent-redirection entries. Raise moves the invoked record to the top; Lower
moves it to the bottom; RestackTransients completes at the existing position.
Both hints and transient flags remain intact. An acyclic control retains normal
parent redirection with one recursive entry. Initial private signature-edit and
missing legacy RestackWindow declaration failures are retained; neither was
activated in production. Wider chain/group/lifetime/real-window and platform
acceptance follows approval, in the next combined stacking/query checkpoint.

The fallback defines new behavior for these three commands on cyclic internal
state. [AGENTS.md](../../AGENTS.md) requires: “A specific compatibility break
requires an explicit user decision.” The existing Layer approval specifies Layer;
this proposal asks for the same choice for the other redirection owner.

The user selected the originally invoked window on 2026-10-07. Production regression coverage now compares two/three-node and prefixed cycles across all three commands with direct-target behavior, both with and without transient inclusion, and checks real server/ring order. See the [stacking/query report](../verification/VERIFICATION-2026-10-07-STACK-PARENT-QUERY.md).
