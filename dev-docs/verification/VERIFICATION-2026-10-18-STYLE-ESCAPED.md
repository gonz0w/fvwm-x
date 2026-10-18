# Escaped style matching — 2026-10-18

STYLE-01 #38 continues after merged PR #109,
`0664c0ef9b7b645568bee11b0045fba47a003eb1`, as one three-story checkpoint.
Implementation and focused Linux policy/core/update/real-client checks pass;
one combined six-target checkpoint is pending. Early OpenBSD ARM64 also passes four affected entries and 26 client cases.

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
subject stays 120 bytes. No pointer, heap buffer or retained match result is
added. Existing metadata rebuild/copy/free owners, ordered merge/index updates,
compatibility ORs, X/server grabs, dispatch and destruction barriers remain.

## Measurements and focused checks

An unchanged-production baseline emits nine selected actual-core rows with one
legacy matcher entry each. The candidate emits the same expected hit/miss results
with zero legacy entries, including normal/reference/forced-collision streams,
complete initialized lookup comparisons and default restoration after removal.
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

Freeze one source for all six Linux/BSD AMD64/ARM64 targets, ten compiler/feature
profiles, scoped Linux sanitizers, authenticated default BSD displays and fresh
stages. Reuse warm builds and run the broad matrix once for all three stories.
The known unchanged OpenBSD AMD64 `image-svg-isolation-xvfb` startup refusal
from PR #109 remains under IMAGE-01. Do not retry it for a clean matrix; explicitly
omit that one required test and keep its fresh-install/staged-geometry gap open.
Built core/update/EWMH/real-client checks still run on its default display.
No VESA cause or repair claim; #93 stays deferred. Physical native-CPU sanitizers,
hardware/toolkits/compositors and release acceptance remain separate.

Raw scripts, source and exact evidence are under
`builddir/codex-2026-10-18/style-escaped-patterns/`.
