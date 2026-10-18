# Escaped selector disjointness — 2026-10-18

Owner: STYLE-01 [#38](https://github.com/gonz0w/fvwm-x/issues/38).
Merged PR #110 (`c0310c1edb5ad0deff6090cfdd97f56a980c739c`) supplies the
prepared escaped widths and constant-stack matching used by this checkpoint.
Three related improvements are implemented; combined qualification is pending.

## Behavior and ownership

1. Decoded exact/minimum lengths now prove escaped selectors disjoint when a
   fixed-width selector is shorter than the other's required byte minimum.
   This works across escaped, unescaped and literal families.
2. Fixed-width proofs decode quoted bytes and distinguish literal quoted `?`
   and `*` from unquoted question wildcards. A contradictory literal position
   proves disjointness. A trailing backslash still describes no matching value.
   Each pattern's raw-byte scan is bounded at 4096, in addition to decoded width;
   an exhausted allowance retains the conservative unknown result.
3. Literal membership reuses prepared escaped span/cursor matching with a
   synchronous borrowed subject. A bounded raw-width scan and overflow-safe
   `(raw width + 1) * (literal length + 1)` allowance of 4096 bound this optional
   proof. Larger pairs remain unknown, retaining their original records.

Only constraints on the same explicit field establish disjointness. Compatibility
resource/class/name ORs retain their existing conservative owner. Prefix/suffix
proofs, ordered merge/copy/free behavior, indexes/collisions, lookup/removal,
DRAW/INPUT/server-grab/dispatch barriers and configuration grammar remain.
No shared `libs/wild.c` change, new metadata, allocation, persistent pointer,
result cache, state owner or configuration limit is introduced. Ordinary matching
continues to accept long patterns beyond the optional proof allowance.

## Baseline, focused checks and review

The unchanged-production baseline retains three records in each of three selected
escaped lists. Candidate normal and forced-collision streams retain two; the
reference owner keeps three. Initialized lookup results, three identities per
list, repeated simplification and removal/default restoration are asserted.
Counts measure records visited, not universal latency or allocation reductions.

Pure tests retain all-selector/NULL/empty/high-byte/random matching controls,
344043 unescaped and 396396 escaped legacy comparisons. A new finite language
check compares 97344 ordered field/pattern pairs against 781 independently
computed legacy witnesses, checking symmetry and no false-negative proofs.
Fixed pairs and literal membership have complete witnesses in that finite domain;
this does not claim all wildcard languages are decided. Existing 28900-pair
unescaped tests and conservative compatibility/index checks remain.

Raw-byte boundary checks cover 4096/4098-byte quoted fixed patterns, a decoded
suffix that understates raw retry width, and a long escaped star run. Normal
matching remains independent of the optional proof budget. Real-client extents
exercise three added simplification cases through ordinary UpdateStyles/removal.
Focused checks and their preparation failures remain source-bound in the owner
receipts. One combined matrix follows the complete three-story batch.

## Combined checkpoint and retained limits

Use warm builds for six targets/ten profiles, scoped Linux ARM64 Clang
ASan/UBSan, staged real clients/geometry and authenticated default BSD displays.
The known required OpenBSD AMD64 `image-svg-isolation-xvfb` startup refusal from
PR #109 remains under IMAGE-01. Omit exactly that one required test without a
retry for green; retain its fresh-install/staged-geometry gap. Built core/update/
EWMH/real-client checks still run on that default display. No VESA cause/repair
claim; #93 stays deferred. Native-CPU sanitizers, physical hardware, toolkit/
compositor and release acceptance remain separate.

Raw source, scripts and evidence:
`builddir/codex-2026-10-18/style-escaped-overlap/`.
