# Shared wildcard matching checkpoint

Owner: STYLE-01 [#38](https://github.com/gonz0w/fvwm-x/issues/38).
Base: merged PR #114, `192efcedfce52058052d51b734abbb93b2579c0e`.
Affected combined checkpoint passes; required CI/SVG omissions remain.

## Three related improvements

1. General plain wildcard patterns keep the latest star's pattern/text retry
   cursors instead of recursive calls. Each retry consumes another actual byte.
2. Escaped patterns use the same constant-storage cursor owner, distinguishing
   quoted `*`/`?` from wildcards and refusing a final escape without reading past
   the terminator. Neither family allocates or retains pointers after returning.
3. A leading star followed only by a nonempty unescaped literal compares that
   suffix at the subject's end. It does not retry the literal at each position.
   Other families retain the general matcher; no prepared metadata is retained.

The public `matchWildcards` name, inputs and integer results remain. NULL text
matches only NULL or exactly `*`; NULL pattern matches every non-NULL text. Empty
text accepts only empty or exactly `*`, so `**` and `a**` exhaustion keep their
historical results. Wildcards consume bytes across path separators, not Unicode
characters. Latest-star retry is sufficient: after a later star is reached, it
can absorb every additional byte an earlier star could have consumed. Termination
still accepts only one raw trailing star; it never skips a run of stars.

The shared helper serves conditional name/environment/property/string checks,
bindings, module names/aliases/configuration, Wait, FvwmAuto, Buttons and IconMan.
Shipped call sites were inventoried; their NULL/empty conventions remain valid.
The existing prepared style matcher and its metadata, order and lifetime remain
unchanged. Core commands still execute through the shared engine; grabs,
publications, configuration syntax and dependencies do not change. No AsciiDoc
option/feature update is required because supported configuration behavior is
preserved. This closes a measured shared-glob component, not STYLE-01's update
planning, grabs or every module's release acceptance.

## Focused evidence and measurement

A separate test-only copy of the original recursive helper is the independent
oracle. It covers every pattern through five bytes and every subject through
four bytes over `ab*?\`, plus 200000 seeded longer combinations, NULL/empty,
quoted/trailing escapes, repeated stars and high bytes: 3254855 comparisons pass.
Candidate-only plain and escaped tests exercise 65536 stars with fixed cursor
storage, including success, mismatch and empty-text controls. This is not a
promise of linear running time for arbitrary patterns.

Linux GCC incremental checks pass `wild`, `style-match`, `style-id-filter`,
`style-profile-xvfb` and `window-update-xvfb`. The real client retains 32 existing
style/index/removal cases and adds nine shared conditional application/removal
cases: suffix/plain/escaped hits and misses, legacy empty-tail and deep stars.
Each observes actual `_NET_FRAME_EXTENTS` and frame/client geometry. Other module
consumers remain covered by the combined native builds/regressions; these nine
cases do not assert exhaustive runtime coverage of every module call site.

| Isolated GCC 13.3 `-O0` workload | Original entries / loop iterations | Candidate entries / loop iterations |
| --- | --- | --- |
| Plain multi-star miss | 3302 / 4787 | 1 / 19 |
| Escaped multi-star miss | 794 / 1025 | 1 / 16 |
| Long literal suffix hit | 2051 / 4198401 | 1 / 0 |

The suffix path still scans pattern/text lengths and compares the suffix in
libc. Its zero body iterations do not mean zero work. These are selected operation
counts, not optimized production timings or a universal speedup. The standalone
coverage build uses its own objects and never mutates the warm Meson build.
Original/candidate source, commands, gcov counts and probe bytes are archived.
The first coverage preparation missed the source include directory; that compile
failure and script are retained. Adding the include path fixed only the isolated
probe; production source and successful focused results remained unchanged.
The first host dispatch was blocked by sandbox access to the local Podman socket;
no native job started until ordinary authorized guest access was granted.

Ignored owner: `builddir/codex-2026-10-19/shared-wildcards/`. Source archives bind
inputs by hash and mode and changed native files get fresh mtimes. One combined
six-target checkpoint reuses the existing owner builds, preserves the two
known required OpenBSD AMD64 CI/SVG omissions, and never retries them for green.
Native-CPU sanitizer, hardware/toolkit/compositor and release coverage remain
separate. VESA #93 stays deferred.

## Frozen checkpoint and results

Three shared wildcard improvements pass 50/50 affected entries across six
targets/ten profiles. One combined checkpoint passes 1998/1998 executed
standard and 439/439 optional entries, five scoped sanitizers, nine fresh
stages and four authenticated default BSD displays. Two known required OpenBSD
AMD64 CI/SVG entries stay omitted, preserving its install/staged-geometry gap
without retries.

Signed author-DCO runtime: `183a0964e4b2e07ac186252cf2638b313939eefb`.
Tree: `f5656ec62e5f492e6f36cd28ff1df79d4455b0c1`.
Archive SHA256: `7cfedaa76beed53745382b18eb83e25cab4435f468c85fbb72c45e091f15ae43`; 2002 regular Git inputs.
Hash/mode pre/post checks, scripts, logs, native binaries and install identities
bind the results to that exact frozen scope. [Machine receipt](VERIFICATION-2026-10-19-SHARED-WILDCARDS-COMBINED.json).

| Scope | Result |
| --- | --- |
| Affected matcher/style/update entries | 50/50 across ten profiles |
| Executed / required standard entries | 1998/1998 executed; 1998/2000 including retained omissions |
| Optional software/minimal | 439/439 |
| Scoped Linux ARM64 Clang ASan/UBSan | 5/5; wild/style-match/style-id-filter leak-enabled, process-owning style/update leak-disabled |
| Fresh stages | 9 / 135 code / 88 catalog identities |
| Authenticated default BSD X | 4 displays / 22 fixtures; clean exits and unauthenticated-client refusal |
| Staged geometry | 18 invocations / 126 cases |
| Actual client conditions | 90 new cases across ten complete invocations |
| Retained actual client style/index cases | 320 |
| Retained EWMH | 14 drivers / 28 modes |
| Portable repository checks | 166 passes / eleven explicit native-Meson skips |
| Compiler diagnostic review | Eleven compile logs; zero diagnostics in affected C files |

Every profile passes 3254855 independent comparisons and candidate-only 65536-star
plain/escaped controls. The original recursive oracle remains test-only; no
production limit or configuration migration is added. Broad module, binding,
transport and conditional regressions retain their existing registrations.

Review caught overlong C lines. Final upkeep wraps those lines and shortens an
invariant comment in `libs/wild.c`; `tests/test_wild.c` also splits one ASCII
message into adjacent literals. A strict source-token comparison, including
literal concatenation, verifies identical C tokens/expressions/strings. An
isolated final Linux unit passes; GCC and Clang preprocessing confirms identical
non-whitespace production output. Test assertion diagnostics/debug locations
can change. The tested guest trees/builds are untouched, and no full suite is
repeated. All other production inputs remain exact. These equivalent formatting
deltas and their original/final hashes are explicit in the machine receipt.

Warm builds use Ninja two jobs and serial Meson tests, at most six independent
guest jobs after checking 18 logical CPUs, 128 GiB and current workload. Existing
owner storage is reused with source/build/archive capacity checks. No guest
restart, disk/package/graphics change, deadline change or full-suite repeat is
introduced for implementation items, formatting, docs or PR creation.

OpenBSD AMD64 executes 242 passing entries; its broad gate retains exit 1 for
two known omitted required entries while successful capture is recorded separately.
It has no fresh install/two staged geometry invocations; built core/client probes
pass on authenticated Xenocara. CI-01 retains the unchanged two-second asset
worker timeout from PR #113 (`2cf1f2f470d65cf1ebfb44dfe69a5edce11e7118`,
archive `1da503ccf2609ac8f94baa287d5c21705ab7a74c1f3adf1969f53dddede4d73a`).
IMAGE-01 retains SVG/MFL startup refusal from PR #109
(`97c728358660675353de35f866c833c926b4f203`, archive
`c5a8dc9783c78e30a16ae790397e304148bf7388318081b77e7ac028e7b0451a`).
Neither omission is an executed pass or retried for green. Causes remain unproved;
no graphics/VESA or CI-timeout causal claim is made. Broader STYLE-01,
native-CPU sanitizer, hardware/toolkit/compositor and release checks remain open.
