# Style wildcard matching — 2026-10-16

Owner: STYLE-01 [#38](https://github.com/gonz0w/fvwm-x/issues/38), after merged
[PR #102](https://github.com/gonz0w/fvwm-x/pull/102). Implemented; focused Linux
checks pass. One combined checkpoint is pending. VESA #93 remains deferred.

## Three substantive improvements

1. Prepare a minimum byte length for unescaped wildcard selectors. Reject
   shorter values without invoking the recursive general matcher.
2. Prepare the literal suffix after the final wildcard. Reject an anchored
   suffix mismatch after the checked minimum-length bound permits subtraction.
3. Resolve exactly one unescaped star with no question mark from the checked
   prefix, suffix and non-overlap bound, including successful matches.

Metadata belongs to immutable selector strings; replacements and existing merge
paths retain the prepared-owner contract. Subject byte lengths are lazy, computed
at most once per needed field in one synchronous lookup/mark operation. Literal,
universal, numeric and prefix-rejected rules need no new length scans. Fresh
subjects invalidate lengths after identity changes. Escapes and remaining
multi-star/question-mark cases retain the original wildcard owner, including
its existing multi-star/empty-value behavior. No configuration syntax, rule
precedence, update/grab boundary, X publication or resource generation changes.

On these 64-bit targets metadata grows from 40 to 144 bytes per style and the
operation-local subject from 48 to 80 bytes. Preparation does additional linear
work; successful general patterns still invoke the old matcher. Call reductions
are workload-specific; they are not a uniform elapsed-time or recursion bound.
No persistent match cache, index, deferred update or new allocation is added.

## Focused development evidence

One incremental Linux ARM64 GCC development profile passes `style-match`,
`style-id-filter`, `style-profile-xvfb` and `window-update-xvfb`. The initial
pass is retained, followed by the lazy-length refinement's affected pass.
Policy compares 699776 exhaustive short-pattern/selector/subject combinations
and 100000 seeded independently varied field cases against the original
predicate. Added controls compare each new path with the preceding prepared
matcher, plus non-overlap, fallback, NULL and byte semantics.

The actual core exercises real command parsing, lookup, retirement, precedence,
UseStyle/simplification ownership and update barriers. Three paired 64-rule
workloads compare the current matcher with precisely this batch's metadata
disabled: minimum rejection, suffix rejection and successful single-star lookup
each reduce 64 general-matcher calls to zero. Styles are created, checked,
removed and checked again through the command engine. Existing original-legacy
comparisons remain independent retained coverage.

Owner evidence is under `builddir/codex-2026-10-16/style-wildcard-batch`;
source manifests, commands, captured logs and failed attempts are retained.

## Combined checkpoint

Pending one frozen six-target, ten-profile full build/test/install checkpoint,
four authenticated default BSD X servers, installed geometry and scoped Linux
ASan/UBSan checks. Stable owner builds are reused with changed-file timestamps;
no VM disk or predecessor stage/evidence is replaced. Maximum two guest jobs.
Original failures stay visible and prevent the affected profile's fresh install;
known failures are not retried merely to obtain green. Native-CPU sanitizer,
physical GPU/input/toolkit/compositor and release acceptance remain separate.
