# Style work baseline — 2026-10-15

Owner: STYLE-01 [#38](https://github.com/gonz0w/fvwm-x/issues/38).
Scope: three profiling stories, before a production optimization. The merged
[PR #98](https://github.com/gonz0w/fvwm-x/pull/98) establishes the
[ordering contract](../decisions/WINDOW-UPDATE-BARRIERS-2026-10-15.md).
[Machine receipt](VERIFICATION-2026-10-15-STYLE-PROFILE-COMBINED.json).

## Measurements and interpretation

The new noninstalled `fvwm-style-profile-fixture` links the actual core objects.
Its startup wrapper replaces initial capture with controlled client/rule setup.
Every workload starts a fresh core process. The real command engine, style
parser, simplification, lookup, bulk update and X property operations execute.
Link wrappers forward `strcmp`, `matchWildcards`, `MyXGrabServer` and
`MyXUngrabServer` unchanged while collecting operation-local observations.
Counters and timers are fixture-private; production objects gain no hooks.

| Rules | Distinct-ID simplification comparisons | Wildcard calls for 32 ordered lookups | Wildcard calls for eight managed-window updates |
| --- | --- | --- | --- |
| 64 | 2016 | 6080 | 1536 |
| 128 | 8128 | 12224 | 3072 |
| 256 | 32640 | 24512 | 6144 |

1. **Simplification:** unique compatibility IDs with the same option mask
   require `n*(n-1)/2` ID string comparisons in one pass, with no removable
   rule. The measured prefix counts only comparisons between workload IDs;
   it excludes parsing/setup and is not a count of every core instruction.
   A post-pass lookup checks the final exact match. A separate interleaved
   wildcard/exact/wildcard control preserves border/handle precedence across
   simplification; deleting the exact rule reveals the earlier wildcard value.
2. **Lookup:** each of 32 synchronous lookups selects the final rule. Earlier
   exact compatibility names miss the resource/class/name fields; the final
   one matches the resource field. This gives `32*(3*n-2)` wildcard calls.
   A stack-local copy borrows one live window's initialized EWMH/monitor state
   during lookup only. It never enters the managed list or publishes identity.
   This is one deterministic late-match workload, not a representative theme
   distribution or coverage of every structured-ID selector.
3. **Bulk update:** vary rule count at eight windows, then window count at
   256 rules. At 8/16/32 windows the latter requires 6144/12288/24576 wildcard
   calls, all within one outer server-grab helper interval. Existing clients
   retain border 3 before `UpdateStyles`; after it, frame/client dimensions
   and all four `_NET_FRAME_EXTENTS` values agree on border 7. A clean repeat
   performs no additional grab or workload matching.

These operation counts agree across the qualified profiles. Elapsed times are
recorded, never used as pass thresholds or speedup claims. For example, the
Linux ARM64 GCC instrumented run takes 0.026/0.117/0.474 ms for simplification
at 64/128/256 rules; its 256-rule update intervals are 5.722/12.807/27.513 ms
at 8/16/32 windows. Other samples vary substantially, including nonmonotonic
rule-count timings. Emulation, synchronous X calls and instrumentation affect
these observations; they do not establish physical input latency or isolate
CPU time. `grabbed_ns` starts after the real outer grab helper returns and
stops before calling its ungrab helper. It excludes acquisition/release waits
and is not an exact server-side lock-duration measurement. No X request count
or profiler attribution is claimed.

The bounded next candidate is cheaper rejection of distinct IDs during
simplification, with an operation-owned lifetime and unchanged merge traversal.
Measure it against this baseline and add duplicate/conflicting/structured-ID
coverage before acceptance. Ordered matching and narrowing the bulk grab remain
separate measured candidates; this checkpoint adds no index, cache, update plan,
coalescing, deferred work or production optimization.

## Verification boundary

The frozen source archive is based on merged `fb1866bb7` plus exactly three
test/build paths: `tests/style_profile_start.c`,
`tests/live_x/style_profile_live.py` and one additive noninstalled block in
`tests/meson.build`. All 1952 regular-file hashes/modes were checked on each
target before configure/build. Archive SHA-256:
`1f64f3367e5f9706c86ab088c1bb25baac66ed6436169d48a65eb3f7a01f113b`.
Final report/tracker/receipt upkeep is outside that runtime input archive.
No production C/header, configuration syntax, dependency or installed target
changes. No user manual change is needed for a noninstalled developer probe.

Ten profiles run `style-profile-xvfb` and the unchanged `window-update-xvfb`:
Linux ARM64 GCC/Clang/software/minimal; Linux AMD64 GCC/Clang; FreeBSD and
OpenBSD ARM64/AMD64 default compilers. The four authenticated default BSD X
servers also run both fixtures, refusing unauthorized clients and exiting
cleanly. Linux ARM64 Clang ASan/UBSan additionally runs the new probe with the
existing compiler-rt profile. Leak detection is disabled because the fixture
terminates with `_exit`; no leak qualification is claimed. Together these are
20 focused Meson entries, eight default-X entries, one sanitizer entry and
165 workload observations. The independent window-update regression retains
its explicit/outer-loop/retirement checks.

The 150 production code and 99 catalog build/stage identities match the
accepted predecessor before and after affected builds/tests. They are checked
against retained source-bound evidence, not installed again. Required broad
runtime/install evidence is retained for these byte-identical targets. This is
an affected profiling checkpoint, not a fresh full matrix or release result.
The [parent full-suite report](VERIFICATION-2026-10-15-WINDOW-UPDATES.md)
continues to retain Linux AMD64 GCC `topology-xorg-dummy`'s 100 ms pressure
bound and OpenBSD AMD64 `ci-asset-contract`'s two-second negative-worker bound.
Neither is retried, changed or relabeled passing here. Native-CPU Linux
sanitizer, physical GPU/input/toolkit and release acceptance remain separate.
OpenBSD AMD64 VESA cause investigation #93 remains deferred.

## Reproduction and evidence

In a configured Linux/BSD owning build with live-X tests enabled:

```sh
meson compile -C builddir -j 2 fvwm-style-profile-fixture fvwm-window-update-fixture
meson test -C builddir --no-rebuild --num-processes 1 --print-errorlogs style-profile-xvfb window-update-xvfb
```

The driver also accepts `--display` for an existing authenticated display.
Each invocation writes eleven raw records plus its core/server logs under
`tests/live-x-artifacts/style-profile`. Use the same instrumented workload and
compiler/profile when comparing a future candidate; don't compare sanitizer or
emulated samples with ordinary physical timing.

Local owner evidence is preserved under
`builddir/codex-2026-10-15/style-profile/`: frozen source/manifest, executed
remote/default/sanitizer runners, six compressed qualification archives, raw
Meson/driver/default-server logs and the strict combined validator. The machine
receipt records archive hashes, profile observations and installed identities.
OpenBSD links retain six warnings from unchanged legacy core/library string
functions; the new fixture compiles without compiler warnings. The collector
also retains the two Meson test-name formats used by the installed versions.
Two initial source-sync guards stopped before configure/testing: archive mode
normalization and the already merged manual delta. Both inputs/logs remain.
An earlier four-profile probe passed before adding independent window scaling
and the precedence control; its inputs remain separate from final acceptance.
Portable `make verify` passes 166 checks with eleven explicit native-Meson
skips. Repository links and branch diff checks also pass; native/runtime claims
are limited to the scope above.
