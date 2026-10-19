# Style field ordering and verified-byte reuse

STYLE-01 #38; 2026-10-19. Implemented; focused Linux checks pass.
One combined six-target checkpoint is pending. Base is merged PR #113,
`971416e5fbb5978cc8c7736bb233c15a6c221a85`.

Three related improvements share this checkpoint:

1. Evaluate explicit literal fields before explicit wildcard fields. Preserve
   resource/class/name/icon order within each group, the preceding compatibility
   OR, numeric rejection, configuration syntax and final matching results.
2. Reuse already-cached subject byte lengths to reject impossible literal/fixed/
   minimum lengths before a strcmp or raw prefix comparison. Unknown lengths
   retain existing lazy scans; trailing-star/all-star paths add no new scan.
3. Start general cursor and escaped fixed/single-star walks after the already
   verified initial literal prefix. No wildcard or quote precedes this boundary;
   raw/decoded prefix widths agree and no retry owner can move before it.

The selected rejected conjunction drops 32 length scans and 32 prefix comparisons
across 32 actual-core lookups. The cached-length workload removes 32 prefix
comparisons while retaining its preceding owner's 32 required length scans.
Initialized results, positive/negative conjunction controls, independent original
predicate parity, deletion and default restoration pass. No retained metadata,
allocation, global cache, shared wildcard consumer, rule precedence, X publication,
update/grab barrier or lifetime change is introduced. The explicit mask/group
checks add fixed work; these selected counters are not a universal latency claim.
LP64 metadata/subject layouts remain 184/120 bytes.

An isolated GCC -O0 coverage probe compares identical long-prefix cases against
unchanged and candidate production helpers. It measures 81920 fewer loop-body
iterations: 32768 escaped span, 32768 unescaped cursor and 16384 escaped cursor
iterations. These are instrumented work counts, not production timing. The
probe covers 24 field/pattern cases with 4096-byte prefixes and their paired
prefix-mismatch controls. Empty-tail, quoted/trailing escapes, fixed/single-star
and general cursor behavior retain an independent legacy oracle. A final reviewed
candidate-only check reuses the baseline. The final source differs only in one
shortened invariant comment; executable bytes and line count are unchanged.

Pure coverage adds 24000 cold/warm independently varied mixed-field oracle cases
plus repeated calls, preserving existing unescaped/escaped differential and
overlap witnesses. Actual-core style/update probes and 32 actual-client cases
pass on the Linux development profile. Initial focused failures are retained:
one supposed decoded-length mismatch had equal widths, and C11's `??)` trigraph
changed a fixture command's closing parenthesis. Corrected test inputs preserve
the intended baseline without changing production behavior to satisfy a counter.

The known OpenBSD AMD64 CI asset worker two-second deadline failure from PR #113
and required SVG/MFL startup refusal from PR #109 stay CI-01/IMAGE-01. This batch
does not retry either for green. Both required entries stay explicitly omitted,
with no fresh install or two staged geometry invocations for that profile. CI
Python inputs remain unchanged from PR #113; its failure was at
`test_worker_refuses_unlocked_url_without_opening_it`, outside the WM runtime.
The CI cause is unproved. Parent CI runtime:
`2cf1f2f470d65cf1ebfb44dfe69a5edce11e7118`; source archive:
`1da503ccf2609ac8f94baa287d5c21705ab7a74c1f3adf1969f53dddede4d73a`;
failed full capture:
`bf17c4db44dc5a9b56150853aac21c41392c663928469e98727dcffe103da759`.
Parent SVG runtime: `97c728358660675353de35f866c833c926b4f203`;
archive: `c5a8dc9783c78e30a16ae790397e304148bf7388318081b77e7ac028e7b0451a`.

Warm owner-controlled builds and the verified OpenBSD storage relocation from
PR #113 are reused. Source/build/archive space is checked before writes and
capture resolves its owning root. No VM disk, package, graphics, timeout or
other owner's work changes. Native-CPU sanitizers, physical hardware/toolkits/
compositors and release acceptance remain separate; VESA #93 stays deferred.
Raw source-bound evidence remains under
`builddir/codex-2026-10-19/style-field-order/`.
