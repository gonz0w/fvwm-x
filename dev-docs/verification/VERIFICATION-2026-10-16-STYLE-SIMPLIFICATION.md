# Repeated-ID style simplification — 2026-10-16

Owner: STYLE-01 [#38](https://github.com/gonz0w/fvwm-x/issues/38), following
merged [PR #103](https://github.com/gonz0w/fvwm-x/pull/103). Three related
improvements are implemented. Focused Linux checks pass; the combined
six-target qualification is pending. VESA #93 cause work remains deferred.

## Three substantive improvements

1. Prepare the existing selector fingerprint once with immutable selector
   metadata and use it to reject unequal IDs before full string equality.
   Equal fingerprints still require the original exact predicate; collisions
   never establish equality. The simplification filter reuses this fingerprint.
2. Record conservative earliest-ID bounds in two fixed hash tables. One-based
   list ordinals are assigned at each pass. Backward scans stop before the
   lower bound; collisions and clamping at 65535 can only retain extra work.
   Record moves retain the destination list position. Removed records leave
   harmless ordinal gaps; neither summary owns rule pointers or strings.
3. Prove disjointness of explicit constraints on the same resource, class,
   name or icon field using literal or anchored prefix/suffix spans. Different
   compatibility names alone prove nothing because they OR three identity
   fields. Unknown/escaped overlap stays conservative; numeric ID policy stays.

The original subset removal, field ownership and both merge directions remain.
No change to configuration syntax, precedence, matching semantics, commands,
server grabs, update barriers, packet layout or X publication is intended.

## Focused evidence

An actual-core baseline preserves the prior hash/filter/matching behavior.
Only the numeric overlap policy is extracted unchanged into its pure helper
so its calls can be counted. One fresh process exercises the three workloads:

| Workload | Baseline | Focused candidate |
| --- | --- | --- |
| Repeat separated by 64 unrelated names | 65 string comparisons | 1 |
| Repeat after a 64-rule unrelated prefix | 65 overlap checks | 1 |
| Disjoint middle rule with conflicting fields | 3 lookup rules | 2 |

`builddir/codex-2026-10-16/style-simplification-batch` retains the baseline,
focused source archives, complete regular-file hashes/modes, runners, commands,
logs and assertions. First focused Linux checks pass style-match,
style-id-filter, style-profile-xvfb and window-update-xvfb. The second focused
run passes those four with the overlap policy enabled. These are incremental
Linux ARM64 native binaries under emulation, not physical native CPU evidence.

Pure coverage checks every selector-flag combination's equality hash,
collision/clamped rank bounds, symmetric conservative overlap and common-match
witnesses against the original matcher. Actual-core probes cover retirement,
both merge directions, repeated passes, UseStyle and DestroyStyle. The final
fixture adds forced hash collisions; the early OpenBSD ARM64 four-entry check
and Linux actual-core collision check pass. Seven retained wildcard and three
new literal/prefix/suffix real-client checks pass on the unchanged Linux build.
The initial development invocation supplied a nonexistent `build/bin/FvwmMFL`
path and timed out before connecting a client. Correcting only the invocation
to Meson's `modules/FvwmMFL/FvwmMFL` output passes without source changes,
recompilation or repeating the other checks. Both attempts are retained.

## Costs, limits and remaining scope

The fixed per-pass filter grows from 4 KiB to 8 KiB. Cached fingerprints and
scratch ordinals add 16 bytes per style on the 64-bit targets; metadata grows
from 144 to 152 bytes. Every initial list position is reassigned before use.
Dense collisions, long unresolved overlaps and repeated IDs can still require
quadratic work. Preparation hashes each active selector; the extra filter
initialization and overlap comparisons are not a universal time improvement.
No persistent style-result cache or hard bound on the legacy matcher is added.

One broad checkpoint is planned for the coherent three-story batch. Later
corrections rerun only invalidated checks. Native-CPU sanitizers, physical X11,
toolkit/compositor and release acceptance retain their separate ownership.
