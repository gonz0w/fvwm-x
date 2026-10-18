# Ordered style candidate lookup — 2026-10-18

Owner: STYLE-01 [#38](https://github.com/gonz0w/fvwm-x/issues/38), following
merged [PR #104](https://github.com/gonz0w/fvwm-x/pull/104), main
`db1ec7a39109b82c3404927941ffff5a950af8d4`.
Locally qualified with composed evidence in [PR #105](https://github.com/gonz0w/fvwm-x/pull/105).
[Machine receipt](VERIFICATION-2026-10-18-STYLE-LOOKUP-COMBINED.json).
VESA #93 remains deferred. This is not release or physical hardware evidence.

## Three related improvements

1. Numeric WindowID candidates use an ordered hash chain before the existing
   matcher checks all remaining conjuncts. Unrelated window-ID rules need no
   matcher visit.
2. Explicit literal resource/class/name/icon predicates choose one necessary
   field as the candidate key. The matcher still checks every active constraint.
   Priority lists retain their original order.
3. Literal compatibility selectors share one key domain and query resource,
   class and name as an OR. Query buckets are deduplicated, including accidental
   collisions and equal values in different fields. Each rule is merged once.

These share one bounded dispatch mechanism. There are 256 buckets per indexed
priority list, with bucket zero for selectors without an indexable necessary
predicate. Up to nine queried chains are merged by fresh list ordinals, retaining
original rule order, field ownership and precedence. Hashes establish candidates,
never matches. Escaped/general wildcard rules use fallback unless an independent
numeric/literal conjunct supplies a necessary key. No configuration syntax,
command/update barrier, server grab, packet, cache generation or X state changes.

Small lists and predominantly general-wildcard lists retain linear traversal:
there must be at least 32 keyed rules and at least as many keyed as fallback
rules in that priority list. List counts describe allocated rule owners. Tail
merges keep the immutable key. Add/remove invalidates the only index roots;
record moves invalidate before replacing a record. Rebuilding overwrites every
intrusive link and ordinal before use. Windows/callbacks retain no index pointer
or cached result. UseStyle and DestroyStyle retain their existing exact-ID paths.

On 64-bit targets the metadata and intrusive link/rank add 24 bytes per rule;
each list adds two counts and one pointer (24 bytes). An enabled index allocates
one fixed 2-KiB head table, lazily rebuilt in one linear walk after mutation.
Hashing subject values and merging up to nine streams have their own costs.
Collision-heavy/fallback traversal can remain linear per lookup; repeated
mutation can repeatedly rebuild. No universal elapsed-time speedup is claimed.

## Measurements and focused scope

A pre-change actual-core probe records 8,224 matcher visits for 32 lookups over
256 rules plus the default rule, separately for numeric, explicit and legacy
selectors. It was frozen before production edits. The candidate's first focused
256-rule observations reduce this to 416 numeric visits, 224–288 explicit visits
and 192–288 compatibility visits. Numeric IDs are allocated by the live X server,
so exact collision counts can vary by invocation. These are matcher visits, not
all byte work; the first measured lookup includes index construction.

The actual-core oracle disables candidate selection and executes the preserved
original predicate, comparing the entire initialized merged style record.
Coverage includes every explicit field, all three compatibility alternatives,
equal/NULL/empty subject fields, changed identities, fallback/literal precedence,
owned TitleFormat strings, repeated simplification/removal and UseStyle.
Test-only all-key collisions retain original results and full traversal counts.
The pure matcher differential also proves every matching ID is in its subject's
candidate set for all selector flag combinations and randomized field tuples.

Focused Linux policy/matching/update checks pass, with original failures retained.
OpenBSD ARM64 affected builds/policy/core checks and actual-client geometry pass.
The supervised real-client driver keeps 40 unrelated rules in each explicit,
compatibility and numeric layer, then checks apply/remove via EWMH extents and
actual frame geometry. The final focused Linux additions pass all four affected entries and the
supervised client checks; combined qualification is recorded below.

The first baseline dispatch stopped in preflight before build/test because guest
source retained PR104's earlier header comment and startup fixture. The accepted
two final PR104 deltas were synchronized before the unstarted baseline. The first
candidate's policy and update checks passed, but its driver wrongly expected
indexing for 64 explicit rules spread over four 16-rule lists. The corrected
assertion preserves the intentional small-list linear path; production behavior
was not changed for that assertion. Failed artifacts remain retained.

## Combined acceptance

Three ordered candidate paths share one checkpoint. Original observations pass 1989/1992 standard and 437/437 optional entries across six targets / ten profiles; 4 scoped sanitizer entries pass. 10 fresh stages bind 150 code/99 catalog identities. 5 authenticated default BSD displays and 10 staged real-client invocations pass. One full matrix serves the batch; hashing/collision/cold-build and release limitations remain explicit.

- 1080 ordered lookup rows include
  360 forced all-key collision rows. Original
  full-result comparisons and all predicate/order assertions remain enabled.
- 330 retained style workloads,
  90 older matching rows,
  90 wildcard rows and
  90 simplification rows retain their exact
  counters. Indexed lookups can reduce older traversal/string counters further;
  they no longer claim the old linear visit counts.
- 10 staged WM/MFL invocations retain
  70 wildcard cases and
  30 simplification cases, plus
  30 new indexed numeric/explicit/compatibility
  apply/remove cases. Each uses 240 unrelated rules to keep all six selector
  layers indexable. Extents and actual frame/client geometry agree.
- 20 staged 100%/150% geometry invocations pass
  140 cases. 5 authenticated BSD displays serve the core,
  real-client and geometry probes; cookie-less clients are refused and server
  exits/logs are retained.
- 120 update-order cases preserve DRAW-01 barriers.
  Retained EWMH state checks include 14 invocations,
  336 client rows, 168 work-area rows,
  56 atom rows and 28000 strut comparisons.
  Repeated deterministic cases are not independent mutations.
- Installed identities include 46 changed,
  203 unchanged and
  0 without a prior bound identity. The actual
  install-rpath transformation is checked against each tested output.
- Portable foundation passes 166 checks with eleven explicit native-Meson
  skips, executed once. Compiler review covers 11 logs,
  0 affected-file diagnostics and
  0 changed-line diagnostics. Legacy warnings stay
  retained; this is not a warning-free or advisory claim.

Scoped ASan/UBSan uses Linux ARM64 Clang/compiler-rt: pure matcher/filter
coverage enables leak checking; actual-core style and update fixtures use
`_exit` and disable leak checking. Native-CPU sanitizers, physical GPU/input,
toolkit/compositor and release qualification retain their separate owners.

## Source binding after formatting review

The combined run consumes the exact `f49b9d3e1` archive throughout. A subsequent
publication-only correction wraps new lines in four C/header files to the
80-column convention and tightens equivalent comments. A retained normalized
C token/concatenated-string proof binds both byte hashes for each file; no
executable token or string value changes. Debug/assert line locations can differ.
Guest source/builds and frozen archives remain unchanged. Formatting uses source
review/hygiene checks and does not trigger another matrix. The machine receipt
identifies this delta separately from the qualified runtime.

## Retained full-profile failures and controls

- linux-amd64/gcc: fvwm3:live-x / geometry-transition-xorg-dummy; Completed separately after one unchanged focused control passed; original full result remains failed.
  All 102 geometry cases pass on unchanged binaries; no source/deadline changes or full-suite repetition.
- openbsd-amd64/default: bsd-x - fvwm3:button-release-common-xvfb, fvwm3:ci-asset-contract; Completed separately after both unchanged controls passed, with selected staged default-X clients/geometry; original full result remains failed.
  All 8 button-release/worker/termination markers and all 23 asset checks pass unchanged; no source/deadline changes or full-suite repetition.

The original AMD64 GCC geometry entry remains failed: Xorg missed its
ten-second display-fd readiness deadline before the 200% WM instance
started; 125% and 150% passed 34 cases each. Stale earlier scenario
files are excluded by the original run timestamps. One unchanged
focused control passes all 102 original cases with identical source,
binaries, environment and deadlines. Only then is GCC installed without
rebuilding and its staged clients checked. Original failure/guard and
the separate passing supplement retain their own hashed artifacts;
no full profile or matrix is repeated. The cause of the server delay
remains unproven; this is unrelated to the deferred VESA cause work.

OpenBSD AMD64 originally fails the menu release-grab five-second check
after root/title/focus cases pass, and the independent Python asset
worker startup exceeds its two-second limit. Both unchanged controls
pass on the same five binary identities and all 1,980 source file hashes/modes.
All eight button-release/worker/termination markers and 23 asset checks
remain enabled. Only then is its install completed without rebuilding.
One supplemental authenticated default display checks the fresh staged
WM/MFL and two geometry scales; already passing core/default fixtures
are retained. Original failures/guards remain failed in the raw records,
and no full profile is repeated. The causes of the delays are unproven.
