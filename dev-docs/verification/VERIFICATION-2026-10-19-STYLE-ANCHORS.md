# Decoded style anchors and zero-slack disjointness

STYLE-01 #38; 2026-10-19. Implemented and focused checks passed;
one combined six-target checkpoint is pending. Base is merged PR #112,
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

Before qualification, the inactive owner-controlled OpenBSD AMD64 tree moves
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
