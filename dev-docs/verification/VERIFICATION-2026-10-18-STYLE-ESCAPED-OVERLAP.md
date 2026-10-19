# Escaped selector disjointness — 2026-10-18

Owner: STYLE-01 [#38](https://github.com/gonz0w/fvwm-x/issues/38).
Merged PR #110 (`c0310c1edb5ad0deff6090cfdd97f56a980c739c`) supplies the
prepared escaped widths and constant-stack matching used by this checkpoint.
Three related improvements pass a composed affected checkpoint;
[PR #111](https://github.com/gonz0w/fvwm-x/pull/111) is ready for review.

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
   proof. Otherwise undecided larger pairs remain unknown, retaining their records.

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
Focused Linux/OpenBSD policy/core/update checks and 29 actual-client cases pass.
The baseline fixture initially misused EscapeString and crashed; two later policy
checks used invalid disjoint examples. These test-only mistakes were corrected
before final focused passes. Original failures remain source-bound.
One full matrix serves the complete three-story batch.

## Combined checkpoint and retained limits

Warm builds serve six targets/ten profiles, scoped Linux ARM64 Clang ASan/UBSan,
staged real clients/geometry and authenticated default BSD displays.
The known required OpenBSD AMD64 `image-svg-isolation-xvfb` startup refusal from
PR #109 remains under IMAGE-01. Omit exactly that one required test without a
retry for green; retain its fresh-install/staged-geometry gap. Built core/update/
EWMH/real-client checks still run on that default display. No VESA cause/repair
claim; #93 stays deferred. Native-CPU sanitizers, physical hardware, toolkit/
compositor and release acceptance remain separate.

Raw source, scripts and evidence:
`builddir/codex-2026-10-18/style-escaped-overlap/`.

## Exact source scopes and local review

Original signed author-DCO runtime: `c2d8c628ebc34f553a8005970b44df49f5d9236d`,
tree `58da1cd9af11e3160a64846ca8f5966eb618eff8`; archive SHA256:
`2d3338c3289e16ce16e3907ae16ce5448f41a948d040692ca6a75b79b026ccf8`.

Final signed author-DCO runtime: `f044e53d46c94bd1ea57d81ed0a686cec8c565e3`, tree
`f8f2e5e4ebdcf52dd0791bf3d700d16d19f8fa35`; archive SHA256:
`e7e8d9a829fb47debdcab7e7c908b67c56385dda5c31454935db01d91c320e8d`.
Both archives contain 1993 regular inputs with exact normalized Git bytes/modes.
Guest pre/postchecks and captured scripts bind each source. Final documentation/
evidence updates preserve every tested non-Markdown input of the final source.

Review found avoidable decoded comparisons for plain literal pairs, whose initial
spans already decide equality. The final correction retains their existing early
return and includes the C test assertion line wrap in its tested input. Only
`fvwm/style_match.c` and `tests/test_style_match.c` differ between the two tested
sources. No whole suite repeats: four owning style/filter/profile/update entries
per profile, fresh staged style consumers, default BSD style checks and scoped
sanitizers qualify the correction. Unaffected EWMH and generic geometry breadth
retain the original source. Do not flatten these source scopes.

## Combined and affected results

| Profile | Original broad passed / executed | Final affected passed | Final fresh stage |
| --- | --- | --- | --- |
| freebsd-amd64/default | 243 / 243 | 4 / 4 | yes |
| freebsd-arm64/default | 243 / 243 | 4 / 4 | yes |
| fvwm-x-linux-arm64-svg/clang | 255 / 255 | 4 / 4 | yes |
| fvwm-x-linux-arm64-svg/gcc | 255 / 255 | 4 / 4 | yes |
| fvwm-x-linux-arm64-svg/minimal | 203 / 203 | 4 / 4 | yes |
| fvwm-x-linux-arm64-svg/software | 234 / 234 | 4 / 4 | yes |
| linux-amd64/clang | 255 / 255 | 4 / 4 | yes |
| linux-amd64/gcc | 255 / 255 | 4 / 4 | yes |
| openbsd-amd64/default | 242 / 242 | 4 / 4 | prevented by required parent gap |
| openbsd-arm64/default | 243 / 243 | 4 / 4 | yes |

Original breadth passes **1991/1991 executed standard and 437/437 optional**
entries. The known omitted required SVG entry makes this **1991/1992 required
standard coverage**. The final affected source passes **40/40 entries** across
all ten profiles, plus four scoped Linux ARM64 Clang/compiler-rt ASan/UBSan
entries. Policy units use leak detection; `_exit` core probes retain leak-disabled
scope. Original sanitizer observations stay separate from these final passes.

The known OpenBSD AMD64 SVG/MFL startup refusal remains under IMAGE-01 with its
original PR #109 source/archive. It is omitted without retrying for green.
Fresh install and two staged geometry invocations remain unperformed there on
both sources; built core/style/update/EWMH and real-client evidence retains its
exact source. No graphics/VESA cause or repair is claimed.

Each source has nine fresh stages binding **135 code/88 catalog identities** to
tested builds and Meson's expected install-rpath processing. Final stages compare
9 changed and 214 unchanged identities against original stages; byte changes
include debug metadata and are not independent functional changes. Each source
also has ten actual-client invocations / **290 cases**: nine staged invocations
and one built OpenBSD AMD64 invocation. Each covers 17 matching, 9 simplification
and 3 indexed-selection cases. These are separate source-bound executions.

Four authenticated default BSD displays pass for each source, with unauthorized
client refusal, fresh vendor logs and clean teardown. Original breadth has
22 fixtures, including EWMH and staged geometry. Final correction has 12 owning
style/profile/update/client fixtures; generic geometry is not repeated. Original
eighteen geometry invocations / 126 cases at 1x/1.5x remain bound to the original
source, with the two missing OpenBSD AMD64 invocations explicit.

For each source, fifteen complete core drivers emit 420 overlap/budget rows,
including **180 escaped-proof observations**: 90 normal/collision rows retain
two records and 90 reference rows retain three. Initialized results, precedence,
repeat simplification and removal stay asserted. Each scope also retains 810
escaped matcher rows, 630 cursor rows, 1080 index rows (360 collision), 90 subject
rows, 330 workloads and 120 update observations. Repeated vectors are not
independent mutations; no universal elapsed or allocation reduction is inferred.

`make verify` runs once: 166 portable passes / eleven native-Meson skips. Eleven
compiler logs per source have zero affected-file/changed-line diagnostics;
unrelated legacy warnings remain recorded. The corrected source reuses each
profile's just-refreshed Meson/dependency setup; no build/profile inputs changed.
Local review covers decoded byte/quote/empty-tail semantics, bounds/arithmetic,
borrowed lifetimes, compatibility ORs and unchanged X/update/barrier owners.
Maintainer review remains pending.

The initial controller selected five guest jobs. A released local worker slot
after an OpenBSD AMD64 SSH banner broken pipe brought observed native concurrency
to six while its detached job continued. A fresh
capacity check confirms headroom on 18 CPUs/128 GiB; join-only reconnection
preserves native jobs rather than repeating them. Each guest keeps one source/
build owner at a time. Two affected workers start only after that guest's broad
archive validates, keeping at most six native jobs across the six targets.
Ninja uses `-j2`; Meson tests remain serial per guest. Original controller and
preparation evidence remain. No VM restart, disk/package change, hosted runner,
full-suite repetition or success-status publisher is introduced.

[Machine receipt](VERIFICATION-2026-10-18-STYLE-ESCAPED-OVERLAP-COMBINED.json)
separates original breadth, final affected coverage, exact source/member/script
identities, original failures and retained limits.
