# EWMH state traversal batch — 2026-10-15

Owner: STATE-01 [#37](https://github.com/gonz0w/fvwm-x/issues/37).
Status: all affected checks pass; [PR #102](https://github.com/gonz0w/fvwm-x/pull/102)
is ready for review. Full platform gaps remain explicit.

## Behavior and ownership

1. Client and stacking snapshots use one traversal each and operation-owned
   inline storage for up to 64 X11 window IDs. Larger snapshots grow with checked
   byte/count arithmetic. Bottom-to-top stack output follows `stack_prev`,
   equivalent to the old reversed `stack_next` output. Main-list and ring
   cardinality remain independent. Refusal frees only owned storage, logs once
   and leaves the preceding property intact until a later ordinary publication;
   it replaces the old fatal allocation behavior for these snapshots. No partial
   prefix is published. Larger snapshots can make more allocations than the old
   count/allocate/fill path; no universal elapsed improvement is claimed.
2. Paired static/dynamic work-area refresh uses one window walk and the existing
   sticky/ignore/monitor selection. Both results still use independent checked
   rectangle validation. Single-mode updates change only their own owner;
   destruction and query refresh retain their original call/publication points.
   Even an IGNORE_WORKING_AREA query retains its refresh side effect.
3. Atom-ID dispatch uses fixed scoped/ALL indexes built from the existing static
   atom tables. Original order breaks equal-ID ties, preserving first-list and
   first-entry priority, None and unresolved interning behavior. Initialization
   withdraws readiness, publishes original IDs and rebuilds indexes before
   making them available; early lookups retain the original ordered scan. The
   sorted copies never reorder the name tables or change supported atoms.
   Storage is compile-time bounded by the table sizes and retains only static
   atom records. Equal-key rewind can be linear in duplicate count; no universal
   constant-time or elapsed-speedup claim is made.

All X/window state remains single-threaded. No command, event-loop, query,
property-notification, cache-generation, configuration syntax or packet-layout
contract changes. Empty/repeated snapshots still replace properties and repair
external changes. No persistent window result cache, new dependency, deferred
publication, dirty domain or generic property-write suppression is introduced.
Existing _NET_WORKAREA/desktop/strut conformance gaps and later EWMH-01 scope stay
with their owners. Broader state coalescing and physical responsiveness remain
unqualified by this checkpoint.

## Frozen input and focused checks

Base: merged PR #101 `3438783b984ecb891477a14691505ba27ed3bd91`.
Signed author-DCO runtime commit: `773958f73893fb8bd3e2084f4d3d881fce7f870a`,
tree `c4ab474b4648f9bdd5d71580ed2da65b757371af`.
Archive SHA-256: `8f3054763ecd7ecde7a841fb96940c97159d4e017927de44b361bb22d772be2b`.
Every one of 1971 regular file hashes/modes matches that commit.
Ten runtime/build/test paths differ; later status/evidence documents are outside
this frozen input. Changed guest files get fresh mtimes. Existing profile builds,
VM disks, prior stages and original evidence are retained.

The pure policy unit verifies signed-Xlib count/byte bounds, inline/heap growth,
first-spill and realloc refusal, populated-prefix ownership, exact Window values,
idempotent cleanup and fresh reuse. The atom policy makes 66000 deterministic
query comparisons over 1000 rebuild iterations, using an all-None fixture and a
seeded duplicate-ID fixture. These
are repeated regression fixtures, not 66000 independent mutations; they cover
priority, None, wide IDs and misses. Actual-core tests separately change the
resolved/unresolved mapping across initialization. Existing monitor-ownership
units remain.

The actual-core fixture runs normal and unresolved-interning modes on one owned
display, including None-to-resolved reinitialization. Synthetic records exercise
actual publication owners; real root-property readback checks ordering, empty
replacement, independently sized lists, refusal, repeat/external repair and
destruction/query barriers. One thousand differential strut cases per mode
cover filtering and independently invalid results. Existing real-client EWMH
inventory, atom startup and window-update integration remain enabled.

Per fixture invocation, 24 paired client rows cover 0/1/64/65/128/256 records;
12 work-area rows cover 64/128/256 records; four atom rows cover misses and
reinitialization. Client append/allocation counts and reference entry visits are
actual instrumentation. Atom indexed calls are not binary-search comparison
counts; tie rewind is not separately counted. Reference work-area visits are
instrumented; the combined one-walk claim is source-reviewed. Elapsed values
are observations without thresholds, not byte-identical predecessor timing.

Focused Linux policy/core/update/inventory/atom-startup entries pass. The final
incremental Linux build uses 11 compile/link steps, rather than PR #101's broad
331-step header rebuild. Focused OpenBSD ARM64 policy/core/update entries pass.
No full qualification is inferred from these focused results.

Preparation attempts remain preserved: the first source-sync preflight stopped
on newly merged CONTRIBUTING.md before changing source or running tests; a later
Meson target name was ambiguous and stopped before compilation; the early BSD
unit lacked X11 include paths, corrected by the project's existing dependency
set. Earlier successful policy/core checks and all failed logs/input manifests
remain distinct from the final focused passes.

## Combined qualification

Eight affected/retained entries pass all ten compiler/feature profiles:
`ewmh-state-policy`, `ewmh-state-xvfb`, `ewmh-inventory-xvfb`, `atom-batch-xvfb`,
`window-update-xvfb`, `style-match`, `style-id-filter` and `style-profile-xvfb`.
The original full runs execute 1992 standard entries
(1990 pass) and 437 optional entries
(437 pass). These are original observations: no failed row is
replaced, relabeled or hidden. Known broader failures are not retried for a
green result. Focused preparation failures remain in the preceding section.

| Profile | Original full pass/fail | Fresh stage |
| --- | --- | --- |
| freebsd-amd64/default | 243/0 | yes |
| freebsd-arm64/default | 243/0 | yes |
| fvwm-x-linux-arm64-svg/clang | 255/0 | yes |
| fvwm-x-linux-arm64-svg/gcc | 255/0 | yes |
| fvwm-x-linux-arm64-svg/minimal | 203/0 | yes |
| fvwm-x-linux-arm64-svg/software | 234/0 | yes |
| linux-amd64/clang | 254/1 | unperformed |
| linux-amd64/gcc | 255/0 | yes |
| openbsd-amd64/default | 242/1 | unperformed |
| openbsd-arm64/default | 243/0 | yes |

Full platform qualification remains incomplete. Original full-scope failures and
prevented fresh installs remain with their broader owners:

- linux-amd64/clang: `topology-xorg-dummy`.
- openbsd-amd64/default: `verification-profile`.

Linux AMD64 Clang's topology case exceeds the existing 100 ms xrandr pressure
operation bound at `pressure-semantic-position`. The earlier GCC failure was
at `pressure-noise`; both original phases remain distinct. Six current topology
artifacts and their original testlog identity are preserved. No root cause or
unchanged elapsed behavior is proved, and no retry or fresh Clang install is
claimed. Its GCC sibling passes and receives its two staged geometry runs.

OpenBSD AMD64's `verification-profile` fails the unchanged 15-second timeout
around its mock Linux-profile shell entry, followed by a late mock-child file
error after temporary-directory cleanup. This verification helper does not run
the tested WM; its original trace remains with CI-01 #17. It is not retried.

The receipt retains exact stdout/stderr, duration, source/profile and commands.
No new deadline or VM hardware change is included. Profiles with unresolved
full-suite failures are not freshly installed. Passing Linux sibling profiles
still receive their own originally required geometry checks; skipped uninstalled
profiles remain explicit. Failed-profile BSD core probes use the new build's
fixtures and worker with captured hashes, not an old WM/fresh-install claim.
VERIFY-01 #5 / CI-01 #17 retain broader readiness/helper/release work. The earlier
PR #101 failures and controls remain historical evidence; a current pass does
not establish their cause.

OpenBSD AMD64's original dependency refresh reached its configuration summary,
wrote a fresh dependency cache and build metadata, then exceeded the 300-second
orchestration limit before compilation/tests began. Its original stdout,
controller trace and cache/output hashes are preserved. A continuation reused
that just-written cache with ordinary reconfiguration, the same 300-second
limit and unchanged functional-test deadlines. It started only when at least
five original dispatches had ended, preserving the two-guest limit. Full tests
then began once; no full-suite row is replaced by this preparation continuation.


Eight fresh stages verify 120 code and 77 catalog identities against tested
build bytes and Meson install metadata, including rpath processing. All 197
identities have predecessor bindings: eight change and 189 remain identical.
There is no shared core-record/header layout change. Compared unchanged native
consumers retain their tested bytes. Staged version checks pass. Geometry passes
16 invocations at 100%/150%, covering 112 pointer/configure-feedback/resize/shaded
cases. Missing stages and their prevented geometry are not called passes.

Four owned authenticated BSD default-X servers pass 18
fixture-script invocations, with unauthorized-client refusal, copied vendor logs
and clean exits on each. Each server serves style, window-update and EWMH-state
core probes plus fresh staged geometry when accepted. The EWMH script itself
starts two fresh core processes on that display. Counts are script invocations,
not a claim that each script has only one process. VESA cause work remains
deferred; these are existing default-server acceptance checks.

The new probe passes 15 invocations/
30 normal/missing modes: 360 client rows,
180 work-area rows, 60 atom rows,
150 positive contract markers and
30000 differential strut-case executions. Retained style checks add
308 workload rows and 84 matching-cost rows;
window-update controls add 120 observations. There are
31 compiled policy-unit invocations in this scope. Raw instrumented
elapsed values are not uninstrumented predecessor latency ratios.

Scoped Linux ARM64 Clang ASan/UBSan passes three entries: state policy with leak
checking and EWMH/window-update core fixtures with leak checking disabled for
`_exit`. This is not general leak, native-CPU or release sanitizer qualification.
Portable `make verify` passes 166 checks with eleven explicit native-Meson skips.
Later documentation upkeep uses repository/link/whitespace checks and preserves
this frozen runtime evidence. Creating the PR does not trigger another matrix.

Source review covers stack-ring publication callers, independent main/ring
cardinality, Xlib element/byte bounds, allocation refusal and exact cleanup,
strut selection and independent validation, single-mode/destruction/query
barriers, immutable atom tables, duplicate priority, None, init fallback and
reinitialization. Original name tables and supported-atom/protocol inventory
remain unchanged. Retained OpenBSD linker diagnostics name unchanged legacy
string-call sites (96 AMD64/90 ARM64 lines, including repeated fixture links).
Two Linux ARM64 Meson warnings concern sanitizer/b_lundef and deliberately
disabled PNG. No diagnostic names the affected C sources. These diagnostics are
preserved; no warning-free baseline or vulnerability assessment is claimed.

All review/tests run locally with Actions disabled. No native-CPU/hardware,
physical toolkit/compositor, new protocol conformance or release acceptance is
claimed. Other chats' source, builds, VM disks and prior evidence are preserved.
VESA #93 cause investigation remains deferred.

[Machine receipt](VERIFICATION-2026-10-15-EWMH-STATE-COMBINED.json). Owner evidence:
`builddir/codex-2026-10-15/ewmh-state-batch/`.
