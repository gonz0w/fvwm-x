# Decoded style anchors and zero-slack disjointness

STYLE-01 #38; 2026-10-19. Affected local checks pass; the broader checkpoint
retains the unchanged CI-01 worker timeout and required IMAGE-01 SVG
omission/install gap. Base is merged PR #112,
`fe4a3ee5e891f3874554fa0f30ddf9a0f167e6ab`.

Three related improvements share this checkpoint:

1. Decoded anchored heads compare required literal positions up to the first
   unquoted star. Questions bridge a byte; quoted questions/stars stay literal.
2. Decoded anchored tails align required positions after the last unquoted star
   from the end, including questions, quoted bytes and unequal raw/decoded widths.
3. When a fixed-width pattern's exact value length equals the other pattern's
   byte minimum, every unquoted star must consume zero bytes. Interior literal
   contradictions can then prove disjointness even without useful edge anchors.

Only constraints on the same explicit field prove disjointness. Compatibility
resource/class/name ORs, numeric identities, precedence, repeated simplification,
merge/copy/free/removal and DRAW/INPUT/grab/query/publication owners remain.
Existing literal membership and fixed-width pairs retain their fast paths.
Raw literal anchors and simple unescaped single-star pairs avoid unnecessary
decoding. No retained selector metadata, allocation, result cache, shared wildcard
consumer or configuration syntax/limit changes are introduced. The measured
metadata/subject layouts remain 184/120 bytes on LP64.

Each optional scan observes at most 4096 raw bytes per pattern, including its
terminator, followed by a constant number of linear decoded walks. An incomplete
scan or trailing escape stays conservative. Raw lengths 4095/4096 exercise the
boundary; ordinary matching still distinguishes the longer patterns. This is
not a total 4096-instruction allowance or a universal latency/linear-time claim.

The unchanged-production baseline retains three records in each selected head,
tail and zero-slack list. Candidate normal/forced-collision streams retain two;
independent reference owners retain three. Initialized results across three
names, repeated simplification, deletion/default restoration and index parity
are asserted. The zero-slack baseline initially placed its middle selector
outside the fixture's probe-name prefix counter. A corrected equivalent workload
keeps every record in that counter; original failed snapshots remain retained.
Production behavior was unchanged by the fixture correction.

Pure coverage retains 344043 unescaped and 396396 escaped legacy comparisons,
28900 unescaped/97344 escaped overlap pairs and prior budget/compatibility checks.
New coverage adds 4096 ordered field/pattern pairs against 3906 independently
computed legacy witnesses, symmetry, targeted head/tail/interior/high-byte and
unequal decoded-tail cases, plus raw-boundary/ordinary-matching controls. These
finite witnesses do not establish complete wildcard-language intersection.
Actual-core probes and 32 real-client cases pass on Linux. Early OpenBSD AMD64
checks pass in its relocated warm build (nine affected compile/link steps).

Before qualification, the inactive owner-controlled OpenBSD AMD64 tree moved
from `/home/fvwm-style-wildcards-recovery-20261016` to
`/usr/obj/fvwm-style-owner-20261019`. All 8680 entries / 731609402 regular bytes,
file modes/mtimes and symlink targets are verified before removing the duplicate;
the original path remains an alias. Source, warm builds and every earlier
checkpoint are preserved. Other owners, VM disks, packages and graphics remain
unchanged. Both build and archive filesystem capacity are checked before writes;
collection resolves its root before packing. No full-suite repetition is caused
by this preparation.

Raw snapshots/scripts/logs are under
`builddir/codex-2026-10-19/style-anchor-proofs/`.
The known required OpenBSD AMD64 `image-svg-isolation-xvfb` startup refusal stays
IMAGE-01 with its omitted required entry/fresh-install/two staged-geometry gap.
Original source `97c728358660675353de35f866c833c926b4f203`, archive
`c5a8dc9783c78e30a16ae790397e304148bf7388318081b77e7ac028e7b0451a`.
No retry for green, SVG root-cause or VESA cause work is included; #93 stays
deferred. Native-CPU sanitizer, physical hardware/toolkit/compositor and release
qualification remain separate.

## Frozen checkpoint and results

Three decoded style proofs pass the affected scope of one combined checkpoint:
1990/1991 executed standard (one retained CI-worker timeout) and 437/437
optional entries across six targets/ten profiles, four scoped sanitizers, nine
fresh stages and four authenticated default BSD displays. OpenBSD AMD64 retains
the unchanged CI-01 two-second worker failure and IMAGE-01 required SVG
omission/install gap.

Signed author-DCO runtime: `2cf1f2f470d65cf1ebfb44dfe69a5edce11e7118`.
Tree: `134d13801d3c9da9747fb32c45fd51525b04bed4`.
Archive SHA256: `1da503ccf2609ac8f94baa287d5c21705ab7a74c1f3adf1969f53dddede4d73a`; all 1997 regular inputs
match exact Git bytes/modes. Pre/post source hashes, script identities, captured
logs, fixtures and installed bytes are checked. Final upkeep changes only docs
and evidence; tested non-Markdown inputs remain exact.
[Machine receipt](VERIFICATION-2026-10-19-STYLE-ANCHORS-COMBINED.json).

| Scope | Result |
| --- | --- |
| Affected style/update entries | 40/40 across all ten profiles |
| Standard executed / required | 1990/1991 executed; 1990/1992 including one CI-worker failure and one required SVG omission |
| Optional software/minimal | 437/437 |
| Scoped Linux ARM64 Clang ASan/UBSan | 4/4; two pure policy checks leak-enabled, process-owning style/update fixtures leak-disabled |
| Fresh stage identities | 9 stages, 135 code and 88 catalogs |
| Authenticated default BSD X | 4 displays / 22 fixture invocations; clean server exits and unauthenticated-client refusal |
| Staged geometry | 18 invocations / 126 cases |
| Actual-core style overlap observations | 600 rows, including 180 new head/tail/zero-slack rows |
| Actual-client cases | 320 cases, including 120 simplification cases |
| Retained EWMH regression | 14 complete drivers / 28 normal and missing-intern modes |
| Portable repository checks | 166 passes / eleven explicit native-Meson skips |
| Compiler review | 11 logs / zero affected-file or changed-line diagnostics |

One matrix serves all three stories. Six existing owner-controlled environments
run at most six independent jobs (Ninja two jobs, Meson tests serial), following
a capacity check on 18 logical CPUs / 128 GiB. Source/build work stays serialized
within each guest and warm builds are reused. Source/archive space is checked
before writes and capture resolves its owner path. No guest restart, package
refresh, VM hardware/graphics change or functional deadline adjustment occurs.
No full-suite retry or repeat for documentation/PR creation is required. The
unchanged CI asset worker times out at its two-second subprocess bound in
`test_worker_refuses_unlocked_url_without_opening_it`. This same failure is
retained under CI-01 in the earlier window-update/style-filter reports. Its
Python inputs are unchanged and do not invoke this batch's C runtime. The cause
is unproved; no retry for green is performed. The original 241/242 OpenBSD
executed passes and failing traceback remain captured.

The OpenBSD AMD64 broad gate retains its nonzero exit for the CI-worker failure
and omitted required entry; successful artifact collection is separate. Every other execution phase
passes. The omitted SVG entry is not an executed pass. Its fresh install
and two staged geometry invocations remain unperformed; built style, update,
EWMH and real-client checks pass on its authenticated Xenocara display. Earlier
focused baseline-counter failures remain captured separately. No SVG or VESA
cause follows from this work. Broader STYLE-01 shared-glob/update-plan/grab work
remains open for separately measured ownership and observer contracts.
