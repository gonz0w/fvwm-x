# STYLE-01: shared subject work, 2026-10-18

Owner: STYLE-01 / GitHub #38. Based on merged PR #105 (`53da645f6`).
Three related improvements are locally qualified with composed evidence in
[PR #106](https://github.com/gonz0w/fvwm-x/pull/106). VESA #93 remains deferred.

## Behavior and ownership

1. Compatibility equality work is deferred until a non-universal compatibility
   rule or compatibility index query needs it. Numeric rejects, universal rules
   and explicit-only matching leave it untouched. Representatives preserve the
   original resource/class/name order, including NULL and empty values.
2. An ordered index rebuild records its necessary-key domain union. Queries
   hash only those domains; operation-local field hashes are reused across
   priority lists and explicit/compatibility tags. Equal compatibility fields
   share their first representative. Hashes remain conservative candidates;
   final matching and ordinal merging remain authoritative.
3. Each string hash also publishes its length for later wildcard minimum/suffix
   checks in the same subject. Reinitializing identity clears every cache,
   even when borrowed pointer addresses are reused.

No configuration syntax, protocol, X state, command ordering, update barrier or
server-grab scope changes. No retained window result or new global cache. Index
invalidation discards the domain union with its roots before mutation/free.
The FNV hash is now untagged until bucket selection; collision distributions
can differ. Forced all-key collisions still visit every rule in original order and compare
the complete merged style with the original walk.

## Measured byte work

An isolated instrumented copy of the production matcher counts non-NUL hash-byte
steps, actual `strcmp` comparison positions and `strlen` bytes. The same driver
uses four distinct 16-KiB values with a long common prefix. Baseline production
computes its candidate buckets once; candidate queries the same domains twice
without resetting the subject, checking reuse across lists.

| Requested domains | Baseline hash / equality / length bytes | Candidate |
| --- | --- | --- |
| Numeric only | 114688 / 49152 / 0 | 0 / 0 / 0 |
| Explicit name plus suffix wildcard | 114688 / 49152 / 16384 | 16384 / 0 / 0 |
| Compatibility only | 114688 / 49152 / 0 | 49152 / 49152 / 0 |
| All domains plus suffix wildcard | 114688 / 49152 / 16384 | 65536 / 49152 / 0 |

These are byte-work observations, not end-to-end latency or a universal speedup.
Compatibility equality still scans equal/near-equal strings when needed. Hash
collisions/fallbacks retain linear candidate work; mutation can require a linear
index rebuild. General wildcard and broader update-plan costs remain open.

## Verification scope

Focused Linux policy, style-ID filter, actual-core style profile and update
barrier checks pass. The profile compares complete merged records against the
original matcher/traversal, including forced all-key collisions. New cases
exercise two key domains in one list, retire and restore its last name-key rules,
and match a 16-KiB name through both indexed and linear paths. Exhaustive pure
checks cover all 64 domain subsets, selector flags, NULL/empty and duplicate
values; cached hash lengths avoid wrapped `strlen` calls. Existing supervised
real-client checks cover thirteen style apply/remove cases and final frame
geometry. The first local preflight stopped before build/test on the preceding
PR's recorded formatting delta; its continuation accepts only those known bytes.

Early OpenBSD ARM64 affected policy/core checks and thirteen real-client cases
pass, including the changed internal metadata/subject ABI. Measured 64-bit
`sizeof` changes are subject 80 to 120 bytes, index 2048 to
2056 bytes, and pattern metadata unchanged at 160 bytes. Allocator rounding is
not measured; the selector domain fills existing pattern padding. Candidate-bucket
assembly is now per enabled list, with cached string work shared across them.

One combined six-target/ten-profile run is complete. Portable verification ran
once for this checkpoint. Native-CPU sanitizer, physical hardware, toolkit,
compositor and release acceptance remain separate.

## Combined checkpoint

Frozen signed/DCO runtime: `5d063475ba6ccfa61987f31079b42bdd719efb12`;
source tree `d99df9351e8e1a68f3adfbad5382dbd0fcbe79cf`.
Exactly 1,982 regular files match its tracked bytes and normalized Git 644/755
modes; source archive SHA256:
`e6a597973accbdd82e10a544b6dd77ed65cdde1a3720aa30b97646f5e02f7088`.
Later edits are documentation/evidence only. Guest source is checked before and
after execution; captured file hashes and executed runners are independently
validated. [Machine receipt](VERIFICATION-2026-10-18-STYLE-SUBJECT-COMBINED.json).

The Linux ARM64 GCC actual-core 256-rule / 32-lookup observations also reduce
matcher visits: numeric 416 to 96, explicit 224–288 to 96–192, and compatibility
192–288 to 96–192 across four targets each. These compare the preceding qualified
runtime with this candidate; numeric identities/hash collisions can vary.
They measure candidate visits, not every byte operation or elapsed latency.

| Gate | Observed result |
| --- | --- |
| Six-target standard scope | 1991/1992 original entries pass; the one failed entry passes an unchanged isolated control |
| Software/minimal profiles | 437/437 entries pass on Linux ARM64 |
| Fresh private installs | 10 stages, 150 code/99 catalog identities; tested output matches Meson install-rpath transformation |
| Authenticated BSD default X | 5 displays / 25 fixture invocations; cookie-less clients refused; clean server exits |
| Staged real clients | 10 invocations / 130 style cases |
| Staged geometry | 20 invocations / 140 cases at 100% and 150% |
| Style probes | 330 retained workloads, 1080 lookup cost rows including 360 forced collisions; 90 mixed-domain/long-name rows |
| Update barriers | 120 retained observations |
| Scoped ASan/UBSan | 4 Linux ARM64 Clang/compiler-rt entries pass; pure units leak-enabled, `_exit` core fixtures leak-disabled |
| Portable foundation | 166 passes / 11 native-Meson skips, once; repository/lint checks pass |
| Compiler review | 11 logs; no affected-file or changed-line diagnostics; legacy warnings retained |

## Original failure and unchanged control

OpenBSD AMD64 originally records 242/243 standard passes. The common button-release
fixture passes root/title/focus, then reaches its existing five-second menu release
checkpoint without observing grab restoration. Paging and worker/termination
observations are not counted from that failed invocation. Original artifacts,
failed row, prevented install and full-guard exit remain unchanged. The source,
controller and interaction owner are bound by the machine receipt; cause of the
delay is unproven. This fixture uses nested Xvfb and is not evidence of a VESA cause.

One isolated control passes all eight release/service/worker/termination markers.
It preserves all 1,982 source hashes/modes, five binary identities, original
working directory, environment variables, arguments/assertions, the outer 65-second
limit and inner five-second limits. Only its output directory changes. No full
suite is repeated and no timeout or assertion is weakened. The control runs after
all full target jobs finish. A separate fresh no-rebuild install and one private
authenticated display complete staged style clients and 100%/150% geometry;
already passing core/default suites are retained. The original failed row stays
failed in its archive and the supplemental passing evidence stays separate.

No full suite repeats serve individual stories, PR creation or documentation.
The earlier PR105 failures/controls stay with that checkpoint and are not erased
by these new observations. Guest OS binaries under virtualization/emulation do
not establish physical native-CPU sanitizer or release acceptance. Installed
byte differences can include debug metadata from shared headers; they do not
prove a functional change to every module.
