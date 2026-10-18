# Style wildcard families — 2026-10-18

STYLE-01 #38 continues from merged PR107
`03c0a180bd7c74d3782f242cd3c6e9ec3392a44e` as one three-story checkpoint.
One combined six-target/ten-profile checkpoint is complete: locally qualified.
All original entries pass; the native-CPU/hardware/release limits below remain
separate from this local checkpoint.

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

## Focused evidence and combined checkpoint

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
- The signed frozen implementation is `a0c164140e5a829118cfc55ab476cfb75da131aa`, tree
  `23e15751d7beff6efc88c03d7defd3b69a48f7f6`. Its 1987 regular tracked inputs match
  normalized Git bytes/modes; archive SHA256 is
  `44a8245d37857b50882aa3657b31fbf1e28a07026338c395449df7a03c96bce8`.
  Every guest pre/postcheck binds the same source. Later status/report changes
  leave every non-Markdown tested input unchanged.

| Profile | Original passed / entries | Fresh staged install |
| --- | --- | --- |
| freebsd-amd64/default | 243 / 243 | yes |
| freebsd-arm64/default | 243 / 243 | yes |
| fvwm-x-linux-arm64-svg/clang | 255 / 255 | yes |
| fvwm-x-linux-arm64-svg/gcc | 255 / 255 | yes |
| fvwm-x-linux-arm64-svg/minimal | 203 / 203 | yes |
| fvwm-x-linux-arm64-svg/software | 234 / 234 | yes |
| linux-amd64/clang | 255 / 255 | yes |
| linux-amd64/gcc | 255 / 255 | yes |
| openbsd-amd64/default | 243 / 243 | yes |
| openbsd-arm64/default | 243 / 243 | yes |

The original matrix passes 1992/1992 standard and
437/437 optional entries. All eight owning/retained style, update and
EWMH entries pass every profile. Four scoped Linux ARM Clang/compiler-rt
ASan+UBSan entries pass: policy tests enable leak detection; `_exit` core probes
retain their existing leak-disabled scope. These guest results are separate
from physical native-CPU and general leak/release qualification.

10 fresh stages verify 150 code and 99 catalog identities against tested
builds and Meson's expected install-rpath transform. 4 authenticated BSD default
server displays complete with fresh vendor logs, cookie-less refusal and clean
teardown; 24 fixture invocations pass. Staged real clients supply
10 invocations / 170 style cases; staged geometry supplies
20 invocations / 140 cases at 1x/1.5x. Repeated observer vectors are
not independent mutations.

Actual-core evidence includes 630 family rows (normal/reference/forced-collision),
1080 candidate rows, 90 mixed-domain/long-name rows,
330 retained workloads and 120 update-barrier observations. Existing simplification,
state and scoped observations remain source-bound in the machine receipt.
Test registrations, semantic checks and deadlines remain. Work-count assertions
require the new routes; retained earlier counters keep their original baseline.
No full profile is repeated for a story, PR or report update.

`make verify` runs once: 166 passes / 11 native-Meson capability skips on macOS.
All eleven compiler logs are reviewed with zero affected-file/changed-line
diagnostics; unrelated legacy warnings remain in the receipt. Declaration,
ownership, byte arithmetic, added C/header line widths and whitespace/self-review
checks pass. This is owner self-review; maintainer review remains pending.

No original full-scope failure remains in this checkpoint.

Physical native-CPU sanitizer, hardware/toolkit/compositor and release gates
remain separate. VESA #93 cause work stays deferred.

Raw focused archives, exact snapshots, measurement copies and scripts remain
under `builddir/codex-2026-10-18/style-wildcard-cursors/`. Current acceptance
status is authoritative in the tracker. One full matrix serves the three
substantive families.
[Machine receipt](VERIFICATION-2026-10-18-STYLE-CURSORS-COMBINED.json).

## Interrupted connection continuation

The final FreeBSD AMD64 full suite finished after the local execution connection
was interrupted. Its original 243 test rows and test log are preserved. Only
uncompleted post-test install, identity, default-X and capture steps resumed;
the original post-test implementation and assertions were reused. No build or
full suite repeated. Exact continuation/source/script identities are in the
machine receipt; the lost original outer exit status is not reported as a pass.

The first reconnect fetched the archive before capture finished writing it. That
partial local copy is retained separately; the completed archive was fetched
after the capture marker and verified against the guest SHA256. This collection
correction did not repeat any build, test, install or default-X command.
