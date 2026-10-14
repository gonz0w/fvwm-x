# Bound the number of desks displayed by each Pager

Status: approved by the user on 2026-10-08: 4096 displayed desks by default,
with a finite build-time override through 65536. Production integration and focused verification are complete; the combined
checkpoint remains pending.
Owner: module ownership/construction follow-up in PROJECT-TRACKER.md.

Pager currently accepts an unbounded inclusive range of desk arguments. The
signed desk count can overflow, and each selected desk adds allocations, layout
work and X windows. A valid numeric range can request far more work than a usable
pager needs. The module runs in its own process, but its X resource requests also
use the shared desktop server.

Propose a default maximum of4096 displayed desks per Pager, with a finite
build-time override from1 through65536. The limit counts displayed desks; it does
not cap desk IDs. A pair of high-numbered desks remains usable. Preserve reversed
endpoint ordering, negative-to-zero correction, decimal prefixes, one-argument
selection and the special current-desk mode. Refuse unrepresentable or over-limit
ranges before sending masks, opening X or allocating per-desk state.

This changes the accepted configuration contract: `FvwmPager 0 4096` requests4097
desks and would be refused by the default build. A finite larger build-time limit
would allow that existing configuration without rewriting its command. Normal
ranges and high desk IDs remain unchanged. Root AGENTS.md says: "Stop for a
compatibility decision only when a change breaks that configuration contract or
requires a configuration migration." This proposal meets that condition.

The reviewable private candidate is in
builddir/codex-2026-10-08/pager-desk-range-proposal. Its pure controls cover the
exact4096/4097 and65536/65537 boundaries, high IDs, reversed/negative endpoints,
invalid integers and count arithmetic. Normal and scoped actual Pager candidates
pass five controls each: default-cap refusal, unrepresentable count refusal,
integer-overflow refusal, ordinary startup and high-ID startup. Exact executed
runner and binary identities validate. Pure scoped leaks are enabled; actual
process leaks are disabled. Production and frozen checkpoints are unchanged.

The first private fixture omitted the normal DesktopSize record and exposed
zero page geometry during startup. Its logs and exact original driver remain
under initial-incomplete-configuration-evidence. The corrected fixture supplies
normal startup configuration; candidate source/deadlines are unchanged. A reliable
initial geometry default is separate authorized follow-up work, not covered by
these passing proposal controls.

The checked range, finite build option and Pager manual are integrated. Normal/
scoped Linux and native BSD ARM64 focus pass with the related startup stories;
the65536 override and default restoration also pass. Combined qualification is
prepared behind the preceding frozen batches. See
[final focused report](../verification/VERIFICATION-2026-10-08-PAGER-STARTUP.md).
Native release CI and physical sessions remain open.
