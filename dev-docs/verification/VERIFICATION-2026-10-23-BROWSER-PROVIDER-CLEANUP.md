# Combined browser provider cleanup

Status: **focused Linux checks passed**, following merged [PR #186](https://github.com/gonz0w/fvwm-x/pull/186).
[Source-bound receipt](VERIFICATION-2026-10-23-BROWSER-PROVIDER-CLEANUP.json).
The user requests larger sets of related cleanup before a combined test gate;
the [revised cadence](../decisions/BATCHED-CLEANUP-CADENCE-2026-10-23.md) removes
the automatic three-PR all-OS checkpoint. Full milestone/release qualification
and affected-target checks remain required.

## Related maintainability improvements

- One operation catalog drives advertised capabilities, frame admission and
  worker routing. Consumer metadata receives a fresh names slice.
- Strict bounded object/duplicate checks and typed string/boolean/array decoding
  are shared by the operation schemas, retaining their literal data limits.
- Read-only queries, value admission and state application have separate source
  boundaries. The state-application function moves byte-for-byte; generation,
  cancellation, mutation and worker ownership remain intact.
- Query results have a concrete map return type. Shipped callers no longer need
  interface assertions to publish action state or inspect query results.

The provider transport file drops from 307 to 169 lines, and the schema file
from 225 to 132 lines, with the moved responsibilities explicitly named in the
architecture guide. This is a readability/ownership change, not a performance
claim or a replacement service. Existing configuration, fields, schemas, wire
operation order and native publication/reply sequencing remain unchanged.

## One combined checkpoint

Implement all four improvements, then run one existing Linux ARM64 profile:

| Gate | Result | Command time |
| --- | --- | ---: |
| Five affected Go packages | 85 top cases / 5 package outcomes pass | 1.577 s |
| Browser race | 45 top cases / 1 package outcome pass | 8.977 s |
| Affected vet | Pass | 0.115 s |
| Incremental browser build | Pass | 0.167 s |
| Native provider/handoff integration | Pass, Slate dark | 11.563 s |

Combined command time: **22.399 seconds**, excluding transfer, collection and
local review/docs. No failed product gate or product correction/retry follows this freeze.
A contract test ties all sixteen advertised operations to admitted example
schemas and worker ownership, refuses unknown operations and proves metadata
cannot modify the catalog. Existing malformed values, stale/atomic refusal,
selection/target/work, bounds, chunking and provider/CLI tests also run.
The real-X driver exercises native selection/publication, generation changes,
workers/cancellation, target controls, presentation and CLI reuse. It retains
one provider/window and executes no revealed target. Its 41 screenshots are
canonical; the handoff properties image is inspected. No new UI is introduced.

Only ten Go files change against the earlier PR186 candidate. Host and guest
guards bind 1504 runtime/test/build inputs; native core/MFL/Form, command client,
Go compiler, OS-specific observer, C/ABI/build/install/theme/configuration inputs
remain exact. The existing three-theme gallery keeps its original source-bound
identity; it is not relabeled as a new cleanup three-theme run. Earlier BSD,
scoped C sanitizer, stage/manual and platform receipts remain under their
original sources, not new final-browser qualification. No BSD boot, C rebuild,
install, default-X, new sanitizer matrix or performance rerun is needed for
this portable boundary cleanup. Broader platform/release coverage remains open.

Canonical host preservation validates full gzip EOF/CRC, all file hashes/modes,
source manifest, original native/client/compiler identities and final inputs.
The archive contains 62 manifest members, SHA256
`9677d5b0332ed459da10bb1823b79b87b22e387313f49c1403d552d15c025aef`.
After preservation, retire 62 individually verified guest duplicates
(6,321,041 bytes), retaining source, incremental builds, installs, caches and
disks. The source delta is only 11,452 bytes; no complete source/build snapshot
is duplicated. PR178 remains the last integrated reference; PR186 is the first
merged implementation batch since it, and this cleanup is the next pending
batch. Review accumulated risk after about five merged batches or a substantial
handoff rather than starting an automatic platform job.

The cleanup branch follows merged PR186 and the dependency-planning docs from
PR185. Rebase conflicts affect only two documentation indexes; both entries are
retained. All 1504 guarded runtime/test/build inputs remain exact against the
tested source, so no product test repeats for the documentation-only baseline
change. The inherited README exceeds the 250-word hygiene limit (257 words);
shorten screenshot alt captions to 248 words total, preserving the dependency
planning link and product status. Repository checks then pass. The source-bound
receipt records this derivation.

The earlier [periodic comparison](PERFORMANCE-2026-10-23-BROWSER-HANDOFF.md)
retains its preserved pre-cleanup PR186 binary identities. It does not measure
this final browser executable, and cleanup makes no new performance claim.
