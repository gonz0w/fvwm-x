# Style selector disjointness — 2026-10-18

STYLE-01 #38 continues from merged PR #108,
`b011bb7effe2757cb1338a55ab3ba44ac7d84192`, as one three-story checkpoint.
The original combined checkpoint and final affected completion are complete.
Owning checks pass; the original SVG startup/install gap remains explicit.

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
`libs/wild.c` is unchanged. The proof helpers add no per-rule metadata or heap
storage; eligible merges retain their existing copy/free allocation paths.
Fixed-position checking scans pattern width; literal matching can revisit spans. Lower record counts are not a universal elapsed-time claim.

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

The broad results retain their original source. The corrected candidate passes
a separate affected six-target/ten-profile/scoped/install/X completion. The
correction repeats no full suite; both exact scopes appear below.

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

Each original driver supplies twelve new rows across normal/reference and
forced hash-collision streams. Before/after simplification, all three witness
names retain expected border/handle/title results; complete initialized lookups
are compared with the independent original predicate. Borrowed strings are
consumed before merging/retirement. Repeated simplification and selector removal
retain the original ownership and restore defaults. These deliberately small
workloads measure three-to-two records; they do not establish session-wide speed.

## Focused evidence and completed checkpoints

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
- One broad six-target/ten-profile matrix runs for the three-story batch. A
  review correction adds bounds and repeats only four affected entries per
  profile, relevant sanitizer/install/default-X and real-client checks.

## Exact source scopes

| Scope | Signed/DCO commit | Tree | Archive SHA256 |
| --- | --- | --- | --- |
| Original broad | `97c728358660675353de35f866c833c926b4f203` | `94ed5a0205c52276daa63b4cd21be004885d218c` | `c5a8dc9783c78e30a16ae790397e304148bf7388318081b77e7ac028e7b0451a` |
| Final bounded implementation | `da60c4f3eeaca9521b2b71973fe661546f374810` | `118742b51b2db1c3006b5ba61b49e6afea65dcbb` | `9cdcaeb286263468781d1f057f214858bcc5fa587e7bff89a5f09c08115f5995` |

Each archive has 1989 regular inputs matching normalized Git bytes and modes.
Guest pre/postchecks bind the executed source. Final documentation/evidence
changes leave every tested non-Markdown input unchanged. The final correction
changes only the optional proof work and its coverage; the broader generic
geometry/EWMH observations retain their original source, rather than claiming
a repeated full matrix for the final source.

| Profile | Original broad passed / entries | Final affected passed / entries | Final fresh stage |
| --- | --- | --- | --- |
| freebsd-amd64/default | 243 / 243 | 4 / 4 | yes |
| freebsd-arm64/default | 243 / 243 | 4 / 4 | yes |
| fvwm-x-linux-arm64-svg/clang | 255 / 255 | 4 / 4 | yes |
| fvwm-x-linux-arm64-svg/gcc | 255 / 255 | 4 / 4 | yes |
| fvwm-x-linux-arm64-svg/minimal | 203 / 203 | 4 / 4 | yes |
| fvwm-x-linux-arm64-svg/software | 234 / 234 | 4 / 4 | yes |
| linux-amd64/clang | 255 / 255 | 4 / 4 | yes |
| linux-amd64/gcc | 255 / 255 | 4 / 4 | yes |
| openbsd-amd64/default | 242 / 243 | 4 / 4 | prevented by original SVG failure |
| openbsd-arm64/default | 243 / 243 | 4 / 4 | yes |

The original source passes **1991/1992 standard and 437/437 optional** entries.
All eight owning/retained style/update/EWMH entries pass all ten profiles.
Its scoped Linux ARM Clang/compiler-rt ASan+UBSan passes four entries, nine fresh
stages bind 135 code/88 catalog identities, and four authenticated BSD default
displays pass 22 fixture invocations with clean teardown. Nine staged clients
pass 180 style cases; 18 generic geometry invocations pass 126 cases at 1x/1.5x.

The final bounded source passes **40/40 affected entries** (32 standard, eight
optional): style-match, style-id-filter, style-profile-xvfb, window-update-xvfb.
Four final scoped ASan+UBSan entries pass; policy retains leak detection while
`_exit` core probes retain leak-disabled scope. Nine fresh stages bind 135 code/
88 catalogs to tested builds and Meson's expected install-rpath transformation.
Four authenticated BSD default displays pass twelve style/update/real-client
fixtures, fresh vendor logs, cookie-less refusal and clean teardown. Ten actual
client invocations pass 200 style cases: **nine staged and one built** (OpenBSD
AMD64). No fresh OpenBSD AMD64 install or staged geometry is claimed.

Final actual-core evidence contains 240 overlap rows: 180 three-family rows and
60 budget controls. Normal/collision streams reduce each selected list to two;
reference streams retain three. Over-budget controls retain three in all modes.
There are also 630 retained cursor rows, 1080 candidate rows (360 collision),
90 mixed-domain/long-name rows, 330 workloads, 120 update-barrier observations
and 22 policy unit executions. Repeated observer vectors are not independent
mutations. Original registrations, semantic checks and deadlines remain.

`make verify` runs once on the original source: 166 passes / eleven native-Meson
capability skips on macOS. Its portable inputs are unchanged by the correction.
Both sets of eleven compiler logs have zero affected-file/changed-line
diagnostics; unrelated legacy warnings remain recorded. Source arithmetic,
borrowed lifetimes, merge/removal ownership, C line widths and final repository
hygiene receive local review. Maintainer review remains pending.

## Retained broad failure

OpenBSD AMD64 `bsd-x - fvwm3:image-svg-isolation-xvfb` fails after 7.6338 seconds
with `ConnectionRefusedError [Errno 61]` at `image_svg_live.py:84`, opening the
MFL endpoint after pathname existence. The unchanged fixture uses path readiness
then a direct connection. Its startup configuration uses only
`Style * TileCascadePlacement` plus MFL; it does not exercise the new proofs.
The observation belongs to IMAGE-01 and remains unresolved. The readiness
pattern resembles the separate hardening fixture repaired in PR #104, but that
is not evidence of this fixture's cause. No unchanged SVG retry or repair is
added to this batch, and no graphics/VESA cause is established.

The original failed row/guard and prevented fresh install remain. Its original
authenticated default server passes built style/update/EWMH/real-client checks;
the final bounded built source passes all four affected entries and twenty
real-client cases. The missing fresh stage and two staged geometry invocations
remain gaps. No broad all-green qualification is claimed.

[Machine receipt](VERIFICATION-2026-10-18-STYLE-OVERLAP-COMBINED.json) contains
both raw scopes, archive identities, measurements and diagnostic reviews.

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
