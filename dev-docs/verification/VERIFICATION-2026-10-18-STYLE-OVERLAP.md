# Style selector disjointness — 2026-10-18

STYLE-01 #38 continues from merged PR #108,
`b011bb7effe2757cb1338a55ab3ba44ac7d84192`, as one three-story checkpoint.
Implementation and focused checks pass; one combined qualification checkpoint
is pending. This report does not yet claim its completion.

## Three related improvements

- Exact/minimum lengths: an unescaped fixed-width pattern or literal cannot
  overlap a pattern whose prepared minimum byte length is larger. This also
  rejects unequal fixed widths. Escaped patterns have no assumed minimum.
- Fixed positions: two unescaped fixed-width patterns cannot overlap if their
  lengths differ or two literal bytes conflict at the same position. Question
  marks retain their one-byte wildcard meaning. Existing literal prefix/suffix
  rejection remains the cheaper first check.
- Literal membership: a literal specifies one possible field value. Against an
  unescaped star pattern, reuse the prepared matcher with a synchronous borrowed
  subject and its already-known literal length. A miss proves disjointness;
  the legacy empty-tail semantics still apply. No recursive escaped proof runs.

False remains a proof that one window cannot satisfy both selectors. Only the
same explicit field or conflicting numeric IDs can establish disjointness;
legacy compatibility ORs and different identity fields remain conservative.
Rule precedence and all original update/publication/grab/lifetime owners remain.
No configuration name, argument, format or wildcard byte meaning changes.
`libs/wild.c` is unchanged. No per-rule metadata, allocation or retained result
is added. Fixed-position checking scans pattern width; literal matching can
revisit spans. Lower record counts are not a universal elapsed-time claim.

## Review refinement: bounded optional work

The broad checkpoint remains bound to signed source
`97c728358660675353de35f866c833c926b4f203`. Review adds a conservative work
policy to the final candidate: at most 4096 fixed positions, or a 4096 product
of `(pattern bytes + 1) * (literal bytes + 1)`, per optional field proof.
The division/addition guards avoid overflow. A larger proof returns unknown and
retains the existing records; ordinary pattern parsing and matching are not
limited. Existing prefix/suffix and exact/minimum checks retain their semantics.
This bounds added proof work, not total simplification or wall-clock latency.

Focused Linux policy/core/update and twenty real-client cases pass the correction.
New policy checks cover fixed-width budget boundaries and unchanged long-pattern
matching; a fourth actual-core control retains three records for an over-budget
224-byte literal while the original three cases still merge to two. Each corrected
driver emits sixteen rows, including the four budget-control rows. Borrowed title,
reference, collision, repetition and removal checks remain.

The running broad results are retained with their original source. The corrected
candidate receives a separate affected six-target/ten-profile/scoped/install/X
completion rather than another full suite. Its exact source and results remain
pending; this report does not claim final qualification yet. Proof helpers add no
heap storage or retained metadata; eligible merges keep existing copy/free paths.

## Measured baseline and candidate

Before production edits, the unchanged merged matcher retains three records in
each selected actual-core case: an older selector's border/title, a disjoint
middle selector that conflicts with both masks, and the older selector's later
handle rule. All selectors share their literal ends, defeating the existing
prefix/suffix-only proof. The source-bound baseline records six observations
(normal and conservative reference) with three lookup visits each.

| Case | Baseline records / lookup visits | Candidate normal / hash-collision | Conservative reference |
| --- | --- | --- | --- |
| Fixed width below another pattern's minimum | 3 | 2 | 3 |
| Different literal at an internal fixed position | 3 | 2 | 3 |
| Literal excluded by a general star pattern | 3 | 2 | 3 |

Each candidate driver supplies twelve new rows across normal/reference and
forced hash-collision streams. Before/after simplification, all three witness
names retain expected border/handle/title results; complete initialized lookups
are compared with the independent original predicate. Borrowed strings are
consumed before merging/retirement. Repeated simplification and selector removal
retain the original ownership and restore defaults. These deliberately small
workloads measure three-to-two records; they do not establish session-wide speed.

## Focused evidence and pending checkpoint

- Incremental Linux policy tests pass after each coherent proof. Existing
  NULL/empty/escape/high-byte/all-selector, matcher, overlap and index checks
  remain. A new independent language oracle compares 28,900 pattern pairs over
  four explicit fields using 127 legacy-match witnesses per pattern. Fixed
  pairs and singleton literals have complete witnesses in that finite domain;
  no whole-language equivalence is claimed for arbitrary star pairs.
- Linux and early OpenBSD ARM64 each pass four policy/filter/core/update entries
  and twenty supervised real-client cases: eleven wildcard, six simplification
  and three indexed cases. Three new simplification cases observe actual frame
  extents through the existing command/update/removal path.
- No registration or semantic deadline is weakened. New core work is collected
  inside the two existing repeated-ID children; no extra child is needed.
- One frozen six-target/ten-profile build/test/install/default-X and scoped
  sanitizer checkpoint follows. Reuse warm profiles and preserve original
  failures instead of rerunning known unrelated failures for a clean result.

The first affected Linux build caught a test-only undeclared reverse-result
variable introduced during formatting cleanup. Its log/source/archive remain;
the corrected checkpoint changes that declaration, then passes the same affected
checks. No production matcher change was needed for that failure. A pre-dispatch
column audit also caught formatting; no native run consumed that unused snapshot.

Raw snapshots, baseline/candidate rows, failed/corrected focused archives and
scripts remain under `builddir/codex-2026-10-18/style-overlap-proofs/`.
Physical native-CPU sanitizer, hardware/toolkit/compositor and release gates
remain separate. VESA #93 cause work stays deferred. Current selection and
acceptance status are authoritative in the tracker.
