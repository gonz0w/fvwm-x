# Style matching batch — 2026-10-15

Owner: STYLE-01 [#38](https://github.com/gonz0w/fvwm-x/issues/38).
Status: all affected checks pass; [PR #101](https://github.com/gonz0w/fvwm-x/pull/101)
is ready for review. Full platform gaps remain explicit.
PR #100 is merged.
[Machine receipt](VERIFICATION-2026-10-15-STYLE-MATCHING-COMBINED.json),
[Previous filter evidence](VERIFICATION-2026-10-15-STYLE-FILTER.md),
[update contract](../decisions/WINDOW-UPDATE-BARRIERS-2026-10-15.md).

## Five substantive improvements

1. Literal and universal selector metadata is prepared once when a rule is
   published. Exact strings use equality and exact `*`/NULL patterns retain
   their wildcard-owner semantics without invoking its engine.
2. A wildcard/escape-free anchored prefix rejects incompatible window values
   before the existing matcher. Successful prefixes and all remaining wildcard/
   escape syntax still use `matchWildcards`; it is unchanged.
3. Each synchronous lookup identifies equal resource/class/title values once,
   including distinct allocations with equal bytes and NULL representatives.
   Compatibility selectors match each distinct value once in original order.
4. A mismatched WindowID rejects the selector conjunction before string work.
5. The parser now guards WindowID with its own presence flag. Previously Name
   preceding WindowID discarded the numeric selector, incorrectly broadening
   the rule. Both documented selector orders now require both predicates.
   The manual documents this existing conjunction contract. Duplicate IDs
   follow the same invalid-keyword policy as other repeated selector fields.

The first four improve separate measured matching costs; the fifth repairs a
related defect exposed by their core probe. Supporting tests/docs are not counted
as additional implementation items. This is one combined acceptance checkpoint.

## Ownership and preserved behavior

Each rule owns 40 bytes of derived metadata on the supported 64-bit targets:
four prefix lengths and two bit masks. It stores no pointers or allocated data.
Selectors are immutable after publication. Adjacent merges retain their owning
ID and metadata; simplification's full-record moves copy both. UseStyle copies
style values without changing the destination ID. Metadata is prepared after
that command parsing and before publication. Removal frees the owning strings
through the existing owner, with no separate metadata cleanup.

Window identities are borrowed only inside one lookup or destruction-mark
operation. Equal-field representatives are rebuilt each time, so title/class/
resource/icon changes and window retirement need no persistent cache invalidation.
Matching and merging invoke no callbacks within that interval. The destruction
path prepares its operation-local patterns once before marking current windows.

Prefix rejection examines only bytes preceding the first `*`, `?` or backslash;
a later wildcard cannot repair a mismatched anchored prefix. No escape is
interpreted by the prefilter. Exact strings, empty strings, NULL values/patterns,
repeated stars, malformed trailing escapes and byte-oriented matching retain the
existing owner's answers. Numeric rejection changes only evaluation order of a
pure conjunction. Deduplicating equal strings preserves a pure OR result.

The ordered rule lists, equality/simplification merge decisions, configuration
syntax and matching precedence remain with existing owners. No pointer/server
grab, X ordering barrier, resource/cache generation or event-loop owner changes.
There are no new runtime dependencies, options, globals or deferred plans.
Literal comparisons and successful wildcard matching still scan bytes; wildcard
worst-case behavior and repeated/colliding-ID simplification remain open. No
universal elapsed-speedup or physical-responsiveness claim is made.

Subject preparation adds up to three string-equality checks per lookup, and a
successful wildcard prefix is examined again by the existing matcher. Fewer
wildcard-engine calls therefore do not imply a uniform elapsed improvement.

## Source and development checks

Base: merged PR #100 `b135fa461e693bab2d6afe55231626606338ad86`.
Signed author-DCO runtime commit: `50601999b0a8539dc2f0881978bdf50eb7f445a2`,
tree `14f0e13f06ad4ef095c4f5d9a68332f5114dd36f`.
Archive SHA-256: `7e2c164b59bbae69c707b6b06b803ef50eefe4bb79c74c9f4d5c71c35312b752`.
Every one of 1962 regular-file hashes/modes matches that commit. Ten owning
manual/runtime/build/test paths differ from the base; later workflow/status/
report upkeep is outside this frozen input. Changed guest inputs receive fresh
mtimes; source hashes/modes are checked before/after the checkpoint.

`style-match` checks 799776 differential cases against the original predicate:
all 781 patterns of length zero through four over `ab*?` and backslash, all 64
selector flag combinations, 14 NULL/empty/ASCII/non-ASCII window values, and
100000 fixed-seed cases with independently varied fields and NULL patterns.
Additional work-count assertions exercise each matching optimization and fresh
metadata/subject preparation after replacements. It needs no X server.

The actual-core paired probe preserves the old matcher and old simplification
traversal as link-wrapper controls. All eleven shapes still run once per path;
three additional matching-cost controls reuse an existing core process rather
than spawning more. For 64 rules, incompatible prefixes use 0 versus 192 wildcard
calls; equal compatibility fields use 64 versus 192; wrong WindowIDs use 0 versus
64. Existing 64/128/256 literal workloads use 0 versus 6080/12224/24512 calls for
32 lookups. The `comparisons` counter also includes literal pattern/window
`strcmp` calls when both values carry the probe prefix: those lookup rows now
record 2048/4096/8192 comparisons. They are not simplification ID comparisons;
only the simplification rows retain that narrower interpretation. Matching under a bulk update's unchanged grab uses 0 versus
`3 * rules * windows` wildcard calls in those literal workloads; equality work
still exists inside that grab. These counts do not establish a time ratio.

The probe checks both Name/WindowID orders with independently mismatched names
and IDs, copied UseStyle values after source removal, adjacent merge and
simplification metadata lifetime, structured/wildcard precedence, deletion and
fresh lookup identities. Existing frame/EWMH geometry and explicit/outer-loop
window-update regression remain enabled. The first focused attempt passes the
new policy, ID-filter and update checks but reproduces the parser defect. A
second attempt stops at fixture compilation because a private parser has no
public declaration; assertions were changed to exercise command-visible
conjunction results. The third attempt passes all four selected entries.
All earlier logs and input manifests remain preserved.

## Combined qualification

All four affected entries (`style-match`, `style-id-filter`,
`style-profile-xvfb` and `window-update-xvfb`) pass all ten compiler/feature
profiles. Default-X core checks pass all four BSD targets. The full runs execute
1976 standard entries (1967 pass) and 433
optional entries (433 pass). These are the original full runs;
no failed record is replaced or relabeled. Previously known failures are not
retried; the separate new-case controls are described below.

| Profile | Full pass/fail | Fresh stage |
| --- | --- | --- |
| freebsd-amd64/default | 241/0 | yes |
| freebsd-arm64/default | 241/0 | yes |
| fvwm-x-linux-arm64-svg/clang | 253/0 | yes |
| fvwm-x-linux-arm64-svg/gcc | 253/0 | yes |
| fvwm-x-linux-arm64-svg/minimal | 201/0 | yes |
| fvwm-x-linux-arm64-svg/software | 232/0 | yes |
| linux-amd64/clang | 251/2 | yes after two unchanged controls |
| linux-amd64/gcc | 253/0 | yes |
| openbsd-amd64/default | 234/7 | unperformed |
| openbsd-arm64/default | 241/0 | yes |

One isolated control invocation covers only the three newly observed OpenBSD
AMD64 GUI failures: 2 pass and 1 remain
failed. Source files, built programs, commands and test deadlines are unchanged
before/after; the control receipt preserves both case observations and original
artifacts before the rerun. Four previously observed readiness/helper failures
are not retried. These controls do not relabel the original full run, permit a
fresh install, establish root cause or close platform qualification. The control
runs only after the owning jobs are idle and a guest slot is free.

The colorset original's seven functional evidence markers all pass despite its
180-second outer timeout; its unchanged control completes in about 146 seconds.
The XSMP socket failure occurs before that test launches the core. The remaining
new image-effects case fails at a scheduled MFL echo's five-second socket wait,
not a style/geometry assertion. One separate reference fixture substitutes the
predecessor `style.c` implementation from the merged base while retaining the
current structure layout, other core objects, software-rendering fixture and
workers. It reproduces the same scheduled-echo timeout with the original
45-second outer limit and unchanged internal deadlines. Candidate source and
core objects remain unchanged. This shows the new matcher is not required to
reproduce the failure; it is not a byte-identical predecessor build or a root-
cause determination. No further retry, deadline increase or unrelated fix is
included; the five remaining OpenBSD AMD64 gaps retain their existing owners.

Linux AMD64 Clang's original full run also fails two geometry invocations while
waiting for the dummy X server's display-fd readiness, before launching FVWM.
The following scale-policy/topology cases pass. One isolated invocation covers
only these two newly observed cases: 2 pass, with identical source,
binaries, command arguments and deadlines. Both original rows and artifacts
remain intact; these observations do not establish the startup failure's cause.

Only after both controls pass does the original full-run/passing-control
composition permit the previously pending Clang fresh install. Its identities
validate against the tested bytes and Meson rpath processing. The four originally
required GCC/Clang staged geometry invocations then pass once, at 100%/150%.
No full suite, compile, known-failure retry or test deadline is repeated/changed.
The table still shows the original Clang failures alongside that composed stage.

Full platform qualification remains incomplete. The following broader failures
and their prevented fresh-install/staged-geometry coverage remain explicit:

- openbsd-amd64/default: `ci-asset-contract`, `image-colorset-effects-xvfb`, `live-x-harness-self-test`, `module-selection-xvfb`, `verification-profile`.

Their raw stdout/stderr, duration, commands and profile remain in the machine
receipt and preserved archives. Existing broad timing/helper failures remain
with VERIFY-01 #5 / CI-01 #17; unchanged failures are not repeatedly retried for
a green matrix, and no deadline or VM hardware change is made. The failed
profile's default-X checks use newly compiled core fixtures and the newly built
SVG/effect worker whose path/hash is recorded; no predecessor WM or fresh
installed-WM acceptance is claimed for it. Profiles with unresolved full-suite failures are not freshly installed.

9 fresh stages validate 135 code and
88 catalog identities against tested build bytes and the
authoritative Meson install metadata, including expected rpath processing.
Compared with the predecessor's available bound identities,
63 installed identities change and
160 remain identical. The core-header metadata addition
also rebuilds its native module consumers; changed debug/type bytes are not
misrepresented as unchanged binaries. Staged version checks pass.

Staged geometry passes 18 invocations at 100%/150%,
covering 126 pointer, configure-feedback, resize and shaded
cases. Four authenticated owned BSD default-server runs serve
14 fixture invocations, including fresh staged geometry
where installation is accepted, with four unauthorized-client refusals and
four clean server exits. Reusing one display per target replaces the previous
separate core/geometry server startups; it does not omit either probe.

The combined set records 330 paired workload rows,
90 matching-cost rows,
120 update-ordering observations and
22 policy-unit invocations. Scoped Linux ARM64 Clang
ASan/UBSan passes four affected entries. The two pure units retain leak checking;
the two `_exit` core fixtures disable it. No general leak or native-CPU release
sanitizer qualification is claimed. Portable `make verify` passes 166 checks
with eleven explicit native-Meson skips. Later documentation upkeep uses only
repository/link/whitespace checks and preserves the frozen runtime evidence.

The two affected pages (`fvwm3all` and `fvwm3styles`) render with Linux ARM64
Asciidoctor 2.0.20 and format with FreeBSD ARM64 mandoc at 80 columns; both new
selector examples are readable. Lint exits with status three on both pages, as on the merged
base: its GNU-roff `mso`/`LINKSTYLE` errors, date/paragraph warnings and other
style notices are unchanged. The new paragraph adds two roff-source line-length
STYLE notices per page because markup expands the input lines; formatted wrapping
passes. No clean manual-lint or full manual-package qualification is claimed.
The initial general `fvwm3` page assertion failed because that page excludes the
styles section; its successful render/failed assertion and native output remain
preserved. Only the verification target changed, with no project source edit.

Source review checks the selector truth table, exact NULL/
escape behavior, prefix-rejection proof, full WindowID comparison, immutable-rule
metadata through both merge directions, borrowed subject lifetime and shipped
lookup/destruction callers. AddWindow zero-initializes its record before the
early lookup, including the not-yet-populated icon name. Wildcard parsing,
matching precedence, event ordering, update barriers and native packet formats
remain with their existing owners. Recompiling the header's consumers surfaces
missing `get_first_pos` initializer warnings in unchanged `placement.c` (the
omitted callback remains zero initialized), plus legacy OpenBSD linker warnings
from unchanged string helpers. The retained Clang sanitizer configuration warning
is kept with its effective-options evidence. No diagnostic names the new matcher,
its unit or the updated core probe in the reviewed build logs. All raw diagnostics
remain available; this is not a blanket warning-free build claim.

All review/tests run locally; Actions remains disabled. Emulated sanitizer/
platform results do not establish native-CPU, hardware, toolkit or release
acceptance. VESA #93 cause investigation remains deferred. Other chats' source,
builds, VM disks, predecessor installations and raw evidence are preserved.

Owner evidence: `builddir/codex-2026-10-15/style-matching-batch/`.
