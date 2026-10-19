# Incremental style lookup maintenance

Owner: STYLE-01 #38. Base: merged PR #117.

Status: implemented / focused checks passed / combined qualification pending.

## Batch and invariants

1. Append a rule to an existing bucket tail in constant work. Allocate and build
   an index lazily at the existing threshold; ordinal exhaustion discards the
   derived index before increment and permits a compact cold rebuild.
2. Unlink a retired rule from its bucket before freeing it. Bidirectional
   derived links keep deletion independent of unrelated rules and preserve
   indexed precedence through ordinal gaps and linear/index threshold crossings.
3. Preserve the surviving indexed position during simplification's equal-ID
   record move. Remove the old owner before copying resources, then restore the
   survivor's list/lookup links and ordinals. Subset retirement and both merge
   directions retain their existing semantic choices.

The core list remains authoritative. No callback/window retains an index or
borrowed selector beyond the existing synchronous owner. Rule IDs are immutable
when published. Domain bits conservatively persist until a cold rebuild; empty
buckets can add hashing work, never candidates or stale rule pointers. There is
no retained window-match result, dirty-domain owner or update-plan cache.

The tradeoff is one extra pointer per rule and 256 bucket-tail pointers plus one
ordinal per allocated index (8 bytes/rule and 2056 bytes/index on the measured
64-bit ABI). Small lists still use their existing linear lookup threshold.
No configuration migration, dependency or installation default changes. Global
update/grab, query/publication and resource lifetime barriers stay with their
existing owners. This does not move check_window_style_change out of its pass.

## Focused evidence

The unchanged-core baseline performs 16 lookup allocations after appends, 16
following deletions and one following a simplification move, in both ordinary
and forced-collision modes. The first candidate performs zero in those same
workloads. Every compared initialized result matches the reference full list
walk. Seven focused Linux checks and 41 real-client cases pass.

The final fixture adds subset retirement and the opposite simplification merge
direction. That fixture-only extension passes without a production rebuild.
It checks complete initialized style results, normal and forced-collision bucket
ownership, repeated append/delete cycles, below-threshold lookup, deletion of all
rules, and applied border/frame/client geometry plus _NET_FRAME_EXTENTS.
The controlled collision keeps exact matching as the final predicate.

The initial baseline preparation stops on the prior report hash before copying
sources or compiling/testing. The previous scheduler correction intentionally
updated its fixture alone, leaving the report at the original production source.
The reconciled prior manifest records precisely that mixed documentation/fixture
scope. The resumed baseline passes; the stopped preparation is retained.

Owner artifacts: builddir/codex-2026-10-19/style-index-maintenance/.
Sources, modes, producer scripts, compiler options and runtime receipts are frozen
at the combined checkpoint. Focused evidence does not establish full platform
qualification. Measurements count selected avoided work, not universal latency.

## Required limitations

One combined six-target/ten-profile checkpoint follows. Preserve the known
OpenBSD AMD64 CI-01 two-second worker failure from PR #113 and IMAGE-01 SVG/MFL
startup refusal from PR #109 as required omissions, including its fresh-install
and staged-geometry gap, without retries for green. Causes are unproved. VESA
#93 stays deferred. Emulated checks do not establish physical native-CPU
sanitizer, hardware/toolkit/compositor or release acceptance.
