# Ordered style candidate lookup — 2026-10-18

Owner: STYLE-01 [#38](https://github.com/gonz0w/fvwm-x/issues/38), following
merged [PR #104](https://github.com/gonz0w/fvwm-x/pull/104), main
`db1ec7a39109b82c3404927941ffff5a950af8d4`.
Implemented with focused checks passed; combined qualification is pending.
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
supervised client checks; combined qualification remains pending.

The first baseline dispatch stopped in preflight before build/test because guest
source retained PR104's earlier header comment and startup fixture. The accepted
two final PR104 deltas were synchronized before the unstarted baseline. The first
candidate's policy and update checks passed, but its driver wrongly expected
indexing for 64 explicit rules spread over four 16-rule lists. The corrected
assertion preserves the intentional small-list linear path; production behavior
was not changed for that assertion. Failed artifacts remain retained.

## Combined acceptance

Pending one frozen six-target/ten-profile checkpoint, fresh installs, authenticated
BSD default displays, staged client checks and affected scoped sanitizers.
Native-CPU sanitizer, physical GPU/input, toolkit/compositor and release coverage
retain their separate owners. A PR does not trigger another matrix.

## Source binding after formatting review

The combined run consumes the exact `f49b9d3e1` archive throughout. A subsequent
publication-only correction wraps new lines in four C/header files to the
80-column convention and tightens equivalent comments. A retained normalized
C token/concatenated-string proof binds both byte hashes for each file; no
executable token or string value changes. Debug/assert line locations can differ.
Guest source/builds and frozen archives remain unchanged. Formatting uses source
review/hygiene checks and does not trigger another matrix. Final evidence will
identify this delta separately from the qualified runtime.
