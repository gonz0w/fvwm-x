# STYLE-01: shared subject work, 2026-10-18

Owner: STYLE-01 / GitHub #38. Based on merged PR #105 (`53da645f6`).
Three related improvements are implemented. Focused Linux and OpenBSD ARM64 checks pass;
combined qualification is pending. VESA #93 remains deferred.

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
can differ. All-key collision controls retain the full original traversal and
complete merged-style comparison.

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
pass, including the changed internal metadata/subject ABI. The candidate adds
40 bytes to a stack-local subject and 8 bytes to an enabled index on the checked
64-bit ABI; the selector domain fills existing pattern padding. Candidate-bucket
assembly is now per enabled list, with cached string work shared across them.

One combined six-target/ten-profile run, scoped sanitizers, fresh private stages
and authenticated BSD default-X checks are pending. Portable verification runs
once at that checkpoint. Native-CPU sanitizer, physical hardware, toolkit,
compositor and release acceptance remain separate.
