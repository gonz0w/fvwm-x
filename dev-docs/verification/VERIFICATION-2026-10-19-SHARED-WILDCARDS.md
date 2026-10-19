# Shared wildcard matching checkpoint

Owner: STYLE-01 [#38](https://github.com/gonz0w/fvwm-x/issues/38).
Base: merged PR #114, `192efcedfce52058052d51b734abbb93b2579c0e`.
Implemented; focused Linux checks pass. One combined checkpoint is pending.

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
six-target checkpoint will reuse the existing owner builds, preserve the two
known required OpenBSD AMD64 CI/SVG omissions, and never retry them for green.
Native-CPU sanitizer, hardware/toolkit/compositor and release coverage remain
separate. VESA #93 stays deferred.
