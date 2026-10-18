# Escaped style matching — 2026-10-18

STYLE-01 #38 continues after merged PR #109,
`0664c0ef9b7b645568bee11b0045fba47a003eb1`, as one three-story checkpoint.
Implementation and focused Linux policy/core/update/real-client checks pass;
the combined six-target checkpoint is complete in its affected scope. Early
OpenBSD ARM64 also passes four affected entries and 26 client cases.

## Three related improvements

- Escaped fixed widths prepare a decoded minimum byte width, reject unequal
  subject lengths and compare quoted literal bytes and unquoted question marks
  directly. A trailing backslash remains an impossible literal slot/non-match.
- Exactly one unquoted star splits two decoded fixed-width spans. The prepared
  minimum prevents overlap; anchored checks avoid recursive suffix retries.
  Quoted stars/questions are literal bytes rather than wildcard separators.
- General escaped patterns use operation-local latest-star/value retry cursors,
  with constant call-stack use. A trailing escape reaches only its terminator.
  Exhaustion preserves the legacy single-raw-trailing-star rule, including the
  multiple-star empty-tail quirk. Cursor retries can revisit spans; no universal
  linear-time, latency or allocation reduction is claimed.

No configuration names, arguments, quoting layers, wildcard byte semantics or
shared `libs/wild.c` consumer changes. Three inline masks keep escaped family
metadata separate from unescaped overlap proofs and literal indexing. On the
measured ABI, selector metadata grows from 168 to 184 bytes, while the borrowed
subject stays 120 bytes. No pointer, separate heap buffer or retained match
result is added. Existing style allocations/copies can grow with the inline record. New
escaped paths can add one lazy full-value length scan per field per lookup
after prefix acceptance. Existing metadata rebuild/copy/free owners, ordered
merge/index updates, compatibility ORs, X/server grabs, dispatch and destruction barriers remain.

## Measurements and focused checks

An unchanged-production baseline emits nine selected actual-core rows with one
legacy matcher entry each. Candidate normal/forced-collision streams record
zero legacy entries; reference controls retain one. All preserve the expected
hit/miss results, complete initialized lookup comparisons and restoration after
removal.
All current before/after semantics remain assertions; elapsed time is observational.
The probe runs inside existing index children; no registrations/deadlines weaken.

Pure tests retain all-selector/NULL/empty/high-byte/escape/random differential,
344043 unescaped legacy comparisons, overlap/index and 4096-work-budget coverage.
New cases cover quoted literals/questions/stars/backslashes, impossible trailing
escapes and non-overlap. An additional 396396 escaped pattern/value comparisons
use the unchanged legacy matcher; closed forms check 65536 quoted literals and
65536-star escaped patterns without asking the recursive oracle to consume a
matching call stack. Long patterns remain supported without a new input cap.

Linux focused checks pass four policy/filter/core/update entries and twenty-six
real-client cases: seventeen matching, six simplification and three indexed.
Real-client additions cover each escaped family through actual frame extents,
existing UpdateStyles and removal. Early OpenBSD ARM64 passes the same checks and covers the changed inline
metadata/native core independently. Their raw receipts stay source-bound.

## Combined checkpoint and retained limits

One frozen source covers all six Linux/BSD AMD64/ARM64 targets, ten compiler/
feature profiles, scoped Linux sanitizers, authenticated default BSD displays
and fresh stages. Warm builds serve one broad matrix for all three stories.
The known unchanged OpenBSD AMD64 `image-svg-isolation-xvfb` startup refusal
from PR #109 remains under IMAGE-01. Do not retry it for a clean matrix; explicitly
omit that one required test and keep its fresh-install/staged-geometry gap open.
Built core/update/EWMH/real-client checks still run on its default display.
No VESA cause or repair claim; #93 stays deferred. Physical native-CPU sanitizers,
hardware/toolkits/compositors and release acceptance remain separate.

Raw scripts, source and exact evidence are under
`builddir/codex-2026-10-18/style-escaped-patterns/`.

## Completed source-bound qualification

Signed author-DCO source: `95c3131a74bd8de8c6ecdaf958c110a0adfbe92b`, tree
`d76176a0d9e708f631d9abdf7ad6cf10b948b1af`. All 1991 regular inputs match
normalized Git bytes/modes; source archive SHA256:
`54a4feaf68ba9cd8863d043ef995ee76a0b1e9715d34326519c3f3281ac5cc0c`.
Each guest pre/postcheck binds this source. Final documentation/evidence changes
leave every tested non-Markdown input exact.

| Profile | Passed / executed | Required omitted | Fresh staged install |
| --- | --- | --- | --- |
| freebsd-amd64/default | 243 / 243 | 0 | yes |
| freebsd-arm64/default | 243 / 243 | 0 | yes |
| fvwm-x-linux-arm64-svg/clang | 255 / 255 | 0 | yes |
| fvwm-x-linux-arm64-svg/gcc | 255 / 255 | 0 | yes |
| fvwm-x-linux-arm64-svg/minimal | 203 / 203 | 0 | yes |
| fvwm-x-linux-arm64-svg/software | 234 / 234 | 0 | yes |
| linux-amd64/clang | 255 / 255 | 0 | yes |
| linux-amd64/gcc | 255 / 255 | 0 | yes |
| openbsd-amd64/default | 242 / 242 | 1 | prevented by required parent gap |
| openbsd-arm64/default | 243 / 243 | 0 | yes |

**1991/1991 executed standard and 437/437 optional** entries pass. One additional
required standard SVG fixture is explicitly omitted on OpenBSD AMD64; counting
that gap, standard coverage is **1991/1992 required entries**. This is not a
fully green broad qualification. The raw omission receipt links PR #109's failed
source/archive and IMAGE-01 owner; the original failure stays in its report.
No retry, fixture repair or graphics/VESA cause work is added here.

All eight owning/retained style/update/EWMH entries pass all ten profiles.
Four scoped Linux ARM Clang/compiler-rt ASan+UBSan entries pass: policy uses leak
detection, while `_exit` core probes retain leak-disabled scope. Guest execution
is separate from physical native-CPU sanitizer/general leak qualification.

9 fresh stages bind 135 code/88 catalog identities to tested builds and
Meson's expected install-rpath transformation. Four authenticated BSD default
displays pass fresh vendor logs, cookie-less refusal and clean teardown with
22 fixture invocations. Ten actual-client invocations pass 260 style cases:
**nine staged and one built** (OpenBSD AMD64). Eighteen generic geometry
invocations pass 126 cases at 1x/1.5x. OpenBSD AMD64 fresh install and its two
staged geometry invocations remain unperformed because of the parent gap.

Actual-core evidence contains 810 escaped-family rows: 540 normal/collision rows
with zero legacy matcher entries and 270 reference controls with one entry each.
All nine selected hit/miss expectations, complete initialized result comparisons,
repeated index invalidations and restoration after removal remain assertions.
There are also 630 retained unescaped-cursor rows, 240 overlap/budget controls,
1080 candidate rows (360 forced collision), 90 borrowed-subject rows, 330 retained
workloads and 120 update-barrier observations. Repeated observer vectors are not
independent mutations; no universal elapsed-time gain is inferred.

`make verify` runs once: 166 passes / eleven native-Meson capability skips on
macOS. All eleven compiler logs have zero affected-file/changed-line diagnostics;
unrelated legacy warnings remain recorded. Added C/header widths, arithmetic,
borrowed-lifetime/metadata owners and repository hygiene receive local review.
Maintainer review remains pending. No full matrix repeats per story, PR or docs.

[Machine receipt](VERIFICATION-2026-10-18-STYLE-ESCAPED-COMBINED.json) records
source/member/script identities, exact raw rows, omissions and remaining limits.
