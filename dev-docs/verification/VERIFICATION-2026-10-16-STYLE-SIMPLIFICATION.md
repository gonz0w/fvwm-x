# Repeated-ID style simplification — 2026-10-16

Owner: STYLE-01 [#38](https://github.com/gonz0w/fvwm-x/issues/38), following
merged [PR #103](https://github.com/gonz0w/fvwm-x/pull/103). The three-item
checkpoint is locally qualified in [PR #104](https://github.com/gonz0w/fvwm-x/pull/104).
VESA #93 cause work remains deferred. This is not release or physical hardware
qualification. [Machine receipt](VERIFICATION-2026-10-16-STYLE-SIMPLIFICATION-COMBINED.json).

## Three substantive improvements

1. Prepare the canonical selector fingerprint once with immutable ID metadata.
   Reject unequal fingerprints before exact ID equality; equal fingerprints
   still require the original exact predicate. The filter reuses this hash.
2. Use two fixed earliest-occurrence summaries to stop scans before any
   possible earlier equal ID. Hash collisions lower the bound; stored lower
   bounds clamp downward above 65535, imposing no rule-count limit.
   Every pass assigns fresh one-based list ordinals.
   Old-to-new record moves restore the destination position; new-to-old moves
   retain the earlier position. Deleted records leave harmless gaps.
3. Prove disjointness only from explicit constraints on the same resource,
   class, name or icon field: different literals or anchored prefix/suffix
   spans. Compatibility name selectors OR three fields, so different names
   alone prove nothing. Escaped suffixes stay unknown; numeric policy stays.

No summary retains pointers into retired styles. The original subset removal,
field ownership, both merge directions, UseStyle and DestroyStyle remain.
Configuration syntax, precedence, command order, update/grab barriers, packet
layout, cache generations and X publication are preserved. No new user option
or manual syntax is introduced.

## Measured actual-core work

| Workload | Prior policy | Candidate |
| --- | --- | --- |
| Repeat separated by 64 unrelated names: ID-equality strcmp calls | 65 | 1 |
| Repeat following a 64-rule unrelated prefix: overlap-policy calls | 65 | 1 |
| Disjoint middle rule with conflicting fields: surviving probe-rule visits | 3 | 2 |

These counters are scoped: overlap span memcmp calls, preparation hashing and
filter initialization have their own costs. They are not all string/byte work,
and instrumented elapsed times are observations, not speedup acceptance.
The independent old-policy baseline extracts only the unchanged numeric overlap
predicate to count its calls. Its source, runner and original observations stay
retained. It is a semantic/work baseline, not byte-identical elapsed timing.

Forced all-zero fingerprints retain exact lookup/retirement outcomes while
performing 2081/2146/2 equality comparisons in the three workloads. This
intentionally collides every ID and proves conservative fallback, not a
constant worst-case bound. Distinct-reference profiles share cached equality,
disable repeat/first-bound filtering and preserve the old numeric-only overlap;
32640 overlap calls assert the 256-rule reference traversal. They no longer
claim to disable the cached-equality improvement.

## Combined local qualification

Original observations pass 1991/1992 standard and 437/437 optional entries across six targets / ten profiles. The one failed startup fixture is corrected and passes on unchanged binaries; its original failure remains visible. 4 scoped sanitizer entries pass.

- 10 fresh stages bind 150 code and
  99 catalog identities to tested output, including the
  actual install-rpath adjustment. 60 installed identities
  differ from the preceding final checkpoint; 189 match,
  and 0 lack a preceding bound identity. The fresh
  FreeBSD workspace changes debug/build paths, so byte changes do not imply
  functional changes to every module.
- 4 authenticated default BSD X displays pass
  24 fixture invocations, reject cookie-less clients,
  preserve vendor logs and exit cleanly. One display serves each BSD target.
- 10 staged WM/MFL invocations pass
  70 retained wildcard and
  30 new literal/prefix/suffix simplification
  apply/remove cases. EWMH frame extents and actual client/frame geometry agree.
  20 100%/150% geometry invocations pass
  140 cases.
- 330 retained style workloads, 90 old
  matching-cost rows, 90 wildcard-cost rows and
  90 new simplification rows include
  45 forced-collision rows.
  120 update-barrier cases retain the existing ordering.
- 14 retained state invocations / 28 modes
  cover 336 client rows, 168 work-area rows,
  56 atom rows and 28000 strut comparisons. Repeated
  deterministic fixtures are not independent mutations.
- Portable foundation passes 166 checks with eleven explicit native-Meson skips.
  This is separate from the native binary/X gates. Compiler review covers all
  eleven compile logs; no diagnostic lies on changed source lines. Legacy
  diagnostics remain retained; this is not a warning-free or advisory claim.

Pure coverage retains the original matcher as its oracle, all selector flag
combinations, inactive-field hash equality, collision/clamped rank bounds and
symmetric conservative overlap/common-match witnesses. Actual-core controls
exercise both merge directions, repeated passes, selector replacement and
owned strings. Sanitizers use Linux ARM64 Clang/compiler-rt ASan+UBSan: pure
style-match/style-id-filter with leak checking, and the two `_exit` core fixtures
with leak checking disabled. This is scoped guest evidence, not the separate
physical native-Linux sanitizer/release qualification.

## Retained preparation and development failures

OpenBSD AMD64's original hardening-xvfb fails before its property/descriptor
checks: the socket pathname exists but Connection.connect is refused. The WM
and module are alive in the captured process snapshot. A socket bind publishes
its pathname before listen is ready; path existence is an insufficient fixture
readiness predicate. The preserved failed directory also contains a stale
property-probe log from a predecessor, which is not counted as current evidence.

The narrowly corrected hardening_live.py uses the established actual-connection
predicate at all three existing seven-second waits. It catches only transient
missing/refused endpoints; Connection closes failed sockets. Every property,
descriptor, symlink-lock, duplicate/restart and signal assertion is unchanged,
as are its five-second socket and seven-second readiness bounds. No production
security policy, WM code or module binary changes for this correction.

The unchanged control passes all eight checks. A deterministic refused first connection
makes the original fixture fail before property work; the corrected fixture
passes every assertion after three injected refusals, one per initial/restarted
connection. The correction is signed at 19262d3e6929dc6be18c5bd4ba608426997bfd21. Its exact old/new
hashes, original full failure and all three control invocations remain in the
receipt. Nine passing original profile checks retain their exact old-driver
scope. Only this failing fixture is rechecked; no full suite is repeated. After
the corrected check passes, OpenBSD AMD64 receives its previously blocked fresh
stage and one default-server/client/geometry gate. The original failed full
row and coordinator guard are preserved, not rewritten as a full-suite pass.

Three guests stopped at documentation-sync preflight because the borrowed
runner omitted the already accepted public GOVERNANCE.md change from its
allowlist. No source mutation, build or test had started. The original failures
and runner remain recorded. Correcting that documentation allowance continues
only those unstarted gates; passing full suites are not repeated.

The first new development client invocation used nonexistent build/bin/FvwmMFL
and timed out before connecting a client. The invocation was corrected to
Meson's modules/FvwmMFL/FvwmMFL output. The same source and compiled WM pass all
seven retained and three new cases without recompilation or repeating other
checks. Both failed setup and successful probe artifacts are retained.

The former FreeBSD AMD64 VM had stopped before this checkpoint. Automatic boot
checks on a new disposable child found filesystem damage before WM tests. Both
original qualified disk and failed boot child are preserved; repair uses a
second child. The first read-only check returned zero but still reported a
link-count correction, so it is not called clean. A second offline repair and
read-only check pass both status and diagnostic-text review. A fresh 1978-file
source tree and fresh build exclude the damaged old build outputs. The
additional whole-package checksum inventory times out at 240 seconds with no
mismatch in its partial log; it is incomplete, not passed, and is not retried.
It does not prove base/private-library/advisory completeness. The subsequent
fresh compiler/build/test/install/default-X results are bound independently.
The detached VM console preserves the recovered guest beyond this chat's PTY.

## Source binding, cadence and limits

Base is 2f47112c0ebdae0efdf6f73d814118408c17434d. Qualified runtime is
122b03bf10f9c77fe17528636c692c938fd1951e; archive
58219e1c788450d20f3418eab57c02edb1679f78064f55a1c18d24790482ea8a contains 1978 regular files, each byte/mode matched
to that signed commit. Guests verify the complete manifest before and after.
Raw archives, executed recipes, commands, binaries, installed expected copies,
failed attempts and source manifests remain under
builddir/codex-2026-10-16/style-simplification-batch; the machine receipt binds
them by hash and records every compiler/profile scope.

After qualification, one same-line C header comment is corrected from metadata
being derived only from strings to the entire immutable ID (including flags
and numeric WindowID). The receipt records the exact old/new header hashes and
single-line replacement; line count and executable preprocessor tokens are
unchanged. Every other production/build input remains byte-identical. The
Python startup-readiness delta is separately bound above; other test code stays
unchanged. Final documentation/receipt updates cause no repeated runtime tests.

The filter grows from 4 to 8 KiB; cached fingerprint and scratch ordinal add
16 bytes per style on the 64-bit targets, with metadata growing 144 to 152 bytes.
Collision-heavy/repeated IDs and unresolved patterns can still require
quadratic work. General wildcard costs, broader lookup indexes, update plans
and narrower grabs remain measured follow-ups. One broad matrix serves the
three substantive items, with at most two full guest jobs; bounded boot and
read-only preparation can overlap them. No functional deadline is increased.
Physical GPUs/input, external toolkits/compositors and release acceptance keep
their separate gates. Prior source-scope failures retain their original reports;
a later passing checkpoint does not establish their causes or close #5/#17.
