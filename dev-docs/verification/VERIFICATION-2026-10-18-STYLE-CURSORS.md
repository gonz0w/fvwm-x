# Style wildcard families — 2026-10-18

STYLE-01 #38 continues from merged PR107
`03c0a180bd7c74d3782f242cd3c6e9ec3392a44e` as one three-story checkpoint.
Implementation and focused checks are complete; one combined qualification
checkpoint is pending. This report does not yet claim its completion.

## Implementation and invariants

- Unescaped question patterns without a star have an exact byte count. A known
  unequal value length rejects before the matcher; equal-length patterns check
  only the span between the already verified literal ends.
- One unescaped star with question marks separates two fixed-width spans. The
  prepared minimum proves non-overlap; direct anchored checks replace retries
  over value suffixes. Existing metadata supplies the lengths and literal ends.
- General unescaped matching retains the latest star's pattern/value cursors.
  Each retry advances that value cursor; matching uses constant call-stack space.
  Literal spans can be revisited, so this is not a universal linear-time claim.
  Text exhaustion accepts only an empty remaining pattern or exactly one star:
  `**` fails empty text and `a**` fails `a`, matching the original predicate.

`libs/wild.c` and escaped/trailing-backslash behavior retain their existing owner.
The original implementation remains the independent oracle. Configuration names,
arguments, wildcard byte semantics, selector conjunctions/compatibility OR,
priority, rule invalidation and DRAW/INPUT/X barriers are unchanged. No new
configuration limit, retained window result or asynchronous ownership is added.

## Measured work and cost

The unchanged actual-core baseline reaches the recursive matcher in all seven
selected cases. The same indexed-list fixture compares complete initialized
style results with the independent predicate, including borrowed title fields,
then removes/rebuilds rules and compares again. Final probes cover normal,
reference and forced-collision streams. Isolated copies of the exact production
C source measure loop/call work; shared prefix/suffix/length checks and preparation
remain outside these counters. Counts are not elapsed-time or instruction claims.

| Case | Original work | Candidate work |
| --- | --- | --- |
| 256-question pattern, value one byte too long | 262 legacy body iterations | exact-length rejection; 0 span/cursor iterations |
| One-star questions, 16,390-byte hit/miss | 16,383 / 16,385 recursive calls | 3 / 2 fixed-span checks; no recursive call |
| 512 stars plus question | maximum active-call depth 513 | depth 1; both perform 519 body/cursor iterations |
| Multi-star miss against 24 value bytes after prefix | 55,455 calls / 66,363 legacy body iterations | 35 cursor iterations / 20 retries, depth 1 |
| Escaped literal-star control | 1 call / 9 body iterations | unchanged |

On the measured Linux ARM64 ABI, selector metadata grows 160 → 168 bytes and
`window_style` grows 768 → 776 bytes. The synchronous subject stays 120 bytes.
Allocator rounding and whole-session memory were not measured. Two classification
masks are inline; the matcher allocates no per-match storage and retains no input.
Diagnostic input/source/transform identities remain in the owning raw archive.
The first diagnostic attempt stopped at a missing copied-header search path;
its original log is retained, and the corrected attempt changes only that path.

## Focused evidence and pending checkpoint

- Incremental Linux policy checks pass after each coherent family. Existing
  NULL/empty/escape/high-byte/all-selector/random/overlap/index checks remain.
  New policy coverage compares 344,043 unescaped pattern/value pairs with the
  unchanged legacy predicate and checks closed-form 65,536-star cases without
  asking the recursive reference to consume that call stack.
- Linux and early OpenBSD ARM each pass the four policy/filter/actual-core/update
  entries and 17 supervised real-client style cases: 11 wildcard, three
  simplification and three indexed cases. Each actual-core driver validates 42
  new family observations with normal/reference/collision ordering.
- A separate native macOS design probe compared 5,570,475 short pattern/value
  pairs before adoption. It is supplemental model evidence, separate from the
  actual production-policy and guest qualification above.
- One frozen combined six-target/ten-profile build/test/install/default-X and
  scoped sanitizer checkpoint is pending. Known unrelated failures remain with
  their original scopes; do not repeatedly rerun them for a clean matrix.
  Physical native-CPU sanitizer, hardware/toolkit/compositor and release gates
  remain separate. VESA #93 cause work stays deferred.

Raw focused archives, exact snapshots, measurement copies and scripts remain
under `builddir/codex-2026-10-18/style-wildcard-cursors/`. Current acceptance
status is authoritative in the tracker. No full matrix has run for each story.
