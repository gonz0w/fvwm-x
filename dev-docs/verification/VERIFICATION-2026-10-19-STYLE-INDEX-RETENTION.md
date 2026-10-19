# Style index retention checkpoint — 2026-10-19

Owner: STYLE-01 #38. Parent: merged PR119.

Status: composed affected checkpoint passes. The original startup refusal,
required OpenBSD CI/SVG omissions and install/staged-geometry gap remain explicit.

## Batch and behavior

1. Track six live necessary-key domains per derived index. Each cold-build or
   append contributes once; unlink decrements before retirement or a survivor
   move. The last contributor clears its bit without scanning survivors.
2. Release bucket storage when the owning category becomes empty. Nonempty
   threshold crossings retain incremental reuse; later population allocates a
   fresh table at the existing threshold.
3. Store only `style_id_flags` in the seven category records. Their initialization,
   subset predicate and category priority are unchanged.

The intrusive list remains authoritative, rule identities remain immutable,
and no callback/window retains a table pointer. All three simplification paths
use the shared unlink owner. Counter increments are bounded by live allocated
rule records; ordinal exhaustion still discards derived roots before increment.
Configuration syntax, matching, command/update order, X error/grab/publication
barriers, dependencies and package defaults remain unchanged. No user-facing
option or manual change is required.

## Controlled measurement

The baseline uses the same actual-core fixture with parent `style.c`. In an
Icon-category list containing two contributors to each Resource/Class/Name
domain and 64 Icon rules, deletion used to keep all four domain bits. Final
lookup hashes only the remaining live fields. Removing one contributor keeps
its bit; removing the second clears it. Reintroducing and retiring a domain
updates the table in place. With 16 KiB identity strings, the selected final
lookup falls from 65,536 to 16,384 hashed character steps. Both normal and forced
collision runs compare every initialized result byte with forced-linear lookup.

The empty table is freed exactly once, followed by a fresh allocation on reuse;
the baseline retains it across both populations. Cold/warm work remains explicit.
Reverse-insertion checks verify all seven categories, independent of command
insertion order. Existing maintenance checks retain actual client/frame geometry,
EWMH extents and all three simplification survivor paths.

Two isolated ARM Linux compiler objects measure category records at **96 → 56
bytes**, saving **280 requested bytes across seven records**. Domain counts add
**48 bytes per active index**: 4112 → 4160 bytes. These are selected work/layout measures,
not universal latency or net-memory guarantees. The matched-rule record layout
is unchanged by this batch.

The focused corrected Linux compile took 0.95 seconds in this local guest;
private production edits affect two core objects and share existing fixture
objects. This observation is not a build-time guarantee.

## Combined local evidence

Three related improvements pass a composed affected checkpoint: one original
broad run records 2029/2030 executed standard and 447/447 optional entries
across six targets/ten profiles, plus one unchanged isolated legacy-property
control. Fifteen actual-core retention drivers verify live-domain counts,
empty-table release and seven-category precedence; fifteen retained
maintenance drivers cover incremental mutation and simplification. Four scoped
sanitizers, nine fresh stages and four authenticated BSD displays pass. Two
known required OpenBSD AMD64 CI/SVG omissions and its install/staged-geometry
gap remain without retries.

- One frozen broad run; at most two heavy guests, Ninja two jobs per guest.
  Existing compiler/feature builds are reused; no matrix repeats for publication.
- 9 fresh composed stages verify 135 code and
  88 catalog identities against tested outputs after RPATH
  handling. 38 fixtures run on four authenticated default BSD
  displays; unauthenticated clients are refused and all servers cleanly exit.
- 18 staged geometry invocations / 126
  cases and 10 real-client invocations pass, including
  170 wildcard, 120
  simplification, 30 candidate and
  90 shared-condition cases.
- Four scoped address/undefined sanitizer checks cover the new retention owner,
  retained maintenance, style profile and window update. Process-fixture leak
  detection is disabled for existing WM process-lifetime allocations; the fixed
  storage lifetime is observed independently with wrapped allocation/free.
- `make verify` runs once: 166 passes / 11 explicit native-Meson skips. Manual
  local review has no outstanding findings. Final docs use repository/link and
  whitespace checks; all runtime/test inputs retain their frozen hashes/modes.

The original OpenBSD AMD64 `legacy-property-xvfb` entry fails with a first-MFL-
connection refusal (`ConnectionRefusedError`, 5.53 seconds), before any of its
five property checks. The core log shows initial client adoption and its startup
function. One isolated control in a released second guest slot, after five target
collections finish while Linux ARM64 continues, passes all five
checks on exactly unchanged WM/MFL/worker binaries, driver/helpers, original
environment (including `MALLOC_PERTURB_=132`) and 60-second fixture timeout.
Only the artifact destination differs. The original failure stays failed in
its raw scope; the refusal's cause remains unproven. No source, fixture,
functional deadline or full-suite repetition is involved.

The standard scope including the two required omissions is
2029/2032; executed scope is
2029/2030. OpenBSD AMD64's IMAGE-01 SVG/MFL startup
refusal and CI-01 two-second worker failure remain bound to their predecessor
sources (PR109/PR113), without retries. Its fresh full install and two staged
geometry probes remain unperformed; freshly built probes pass on authenticated
Xenocara. This checkpoint does not establish their cause or close those owners.
Original PR118 timing failures/controls stay in that historical source scope.

## Source and preserved attempts

- Base: `fb873c00319b7dd0f915e2ef5cecdd0104a231e5`.
- Signed/DCO implementation: `1e5c3ddd02651c7fbb151a0230a7755d1c654f7c`.
- Runtime tree: `811b91ee660b43bde1ca4fc8b25f30fe4c83c8ef`.
- Frozen archive SHA256: `9d5cf133a9dd330c3254889be2ac7d0bb11be0cbd8a3d84f953fe495be5a9afa`;
  2021 regular input files. All six guest collections validate
  every frozen source hash/mode and bind executed harnesses, outcomes and output
  identities. Final deltas are documentation/receipts only.

The initial focused compile caught an incomplete type declaration; only affected
compilation/checks repeat after correction. An initial layout collector asserted
a guessed 104-byte baseline after both compiles. Actual DWARF measured 96; both
completed objects and the original attempt were collected without recompilation.
No broad suite repeats for either correction.

Capacity preflight retained existing guests/builds/disks. Three completed
archive copies in the ARM guest were removed only after exact hash/size checks
against canonical host copies; an evacuation receipt preserves both paths and
identities. New combined output archives live on owner volumes, avoiding the
Linux AMD64 `/tmp` limit. No other chat's files or evidence were changed.

[Machine receipt](VERIFICATION-2026-10-19-STYLE-INDEX-RETENTION-COMBINED.json)
records exact profile commands, outcomes, scripts and local archives. Private
owner: `builddir/codex-2026-10-19/style-index-retention/`. Native-CPU sanitizer,
physical hardware/toolkit/compositor and release checks remain separate. Actions
is disabled. VESA #93 stays deferred. Broad STYLE-01 remains open.
