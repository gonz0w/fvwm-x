# STACK-01 / DATA-01: relative Layer arithmetic

## Current compatibility authority

The quoted broad approval rule below records the policy at the time of this
historical decision. It is superseded by the user’s
[configuration-only compatibility scope](CONFIG-COMPATIBILITY-SCOPE-2026-10-07.md).
It creates no present approval hold for internal/runtime behavior alone.

Status: approved 2026-10-07; refuse overflow and preserve the previous state.
The approved refusal is locally qualified in the exact 1306-file arithmetic
checkpoint, with valid maximum-layer placement and checked transient rank sums.
See [qualification](../verification/VERIFICATION-2026-10-07-STACK-ARITHMETIC.md).

## Concrete controls

Two real-window Clang ASan/UBSan controls use the unchanged, locally qualified
1301-file parent/query source, an owned Xvfb server, MFL and one ordinary client.
`Layer 0 2147483647` overflows the internal insertion threshold `layer + 1`.
`Layer 0 2147483646`, followed by `Layer 2`, overflows relative command addition.
The second control stops at the addition before any resulting layer is applied.
This is verified undefined arithmetic; no compromised desktop is claimed.
Logs and controls are under `builddir/codex-2026-10-07/stack-rank/`.

## Approved contract

Calculate relative offsets in a wider integer. If the result exceeds INT_MAX,
refuse that command, issue the normal runtime diagnostic and retain the previous
layer and stack order. Keep existing negative-result behavior (layer zero),
zero/default/absolute syntax, all representable offsets and ordinary parent
redirection. Absolute layer INT_MAX remains valid; its insertion uses direct
comparisons instead of an overflowing increment.

The alternative is to clamp overflowing positive offsets to INT_MAX. It changes
position even when the requested offset cannot be represented. Preserving the
previous state is consistent with other approved failed-runtime-change contracts.

AGENTS.md says: “A specific compatibility break requires an explicit user
decision.” The user approved preserving the previous state for overflowing relative
commands. It does not authorize a new arbitrary layer limit or parser rewrite.

## Acceptance

A private candidate now passes real-window Clang ASan/UBSan checks for
overflow refusal/unchanged order, maximum-layer
placement among equal-layer siblings, representable relative moves, negative
clamping, default/absolute commands, transient ancestry and follow-up commands.
The reviewed candidate is now activated. The compatible rank/maximum-layer
source passes six focused Linux checks and the same six under scoped Clang
ASan/UBSan; full Linux/BSD qualification is pending. The next
combined Linux/BSD/default-server checkpoint covers the approved implementation;
the separately qualified parent/query source does not cover this change.

Private candidate: `builddir/codex-2026-10-07/stack-rank/relative_candidate.c`.
It computes `(int64_t)layer + offset`, logs and returns before any stack mutation
on a result above INT_MAX. The actual driver also checks a large positive
offset, equal-layer sibling order, default and negative clamping, and subsequent
commands. The production command now uses this reviewed overflow check.
