# Maintenance backlog organization

Status: **verified**, [PR #188](https://github.com/gonz0w/fvwm-x/pull/188) open
for review. Planning/queue changes only.
[Inventory and readback receipt](MAINTENANCE-PLANNING-2026-10-23.json),
[accepted batch register](../decisions/MAINTENANCE-FIRST-BATCHES-2026-10-23.md).

All **73 open issues** have exactly one primary placement:

| Disposition | Issues | Result |
| --- | ---: | --- |
| Maintenance owners/children | 34 | M1–M8 plus three bounded decision lanes |
| Conditional qualification owners | 12 | Existing status/milestone retained; consult when affected |
| Performance-only work | 5 | Backlog / Later |
| Paused feature/polish work | 22 | Backlog; existing stage/milestone retained |

Nineteen selected maintenance issues move from Later to milestone 05. Other
maintenance owners keep their original milestone. #179/#181 are Ready at the
top of the board; #180 remains Backlog and natively blocked by #179. Other
maintenance work is Backlog until pickup. The Work batch field has fourteen
choices and matches the complete register. It supplies the full logical queue;
other cards retain their prior relative order. No issue is closed or duplicated.
#41's misleading geometry title now matches its existing helper-sandbox scope.

Readback verifies all issue bodies, single status/priority/stage labels,
milestones and matching Project fields. Every native parent/blocked-by relation,
priority and stage remains exact. The remaining acceptance/evidence text is
preserved below a current scheduling note. Merged PR187 is Done; planning PR188
is Review / P2 / stage 05. The batch dependency graph is acyclic.

A bulk metadata request returns HTTP 502 after partially applying setters.
Readback identifies eighteen completed assignments; 152 remaining assignments
finish in smaller idempotent groups. A bulk card-position request also returns
HTTP 502. Read back its outcome, restore attempted moves, then promote only
the two Ready M1 cards with individual setters. Final readback confirms every
expected value and the original relative order of all other cards; neither
unknown response is treated as proof of a failed or successful write. Original
payloads, snapshots and recovery records remain in the canonical host directory
recorded in the receipt. No authorization or scope block remains.

Repository/link/whitespace checks (`make check lint`) pass. Changes contain only
documentation/planning JSON. Runtime/build/test sources, native dependencies,
installed options, VM data and accepted component evidence remain unchanged.
No source deletion, compiler/install switch, package adoption, release, VM boot
or product test matrix occurs. This planning work adds no implementation-batch
or performance-comparison count: the last affected platform reference remains
PR178, with merged PR186/187 since it; the preserved PR186 performance reference
remains 1/5 after PR187. Actual removals start with the defined M1 development
checkpoint, including the one combined compiler/install/platform acceptance
required by its changed contract.
