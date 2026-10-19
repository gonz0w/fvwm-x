# Incremental style lookup maintenance

Owner: STYLE-01 #38. Base: merged PR #117.

Status: composed affected checkpoint passes; original deadline failures and required CI/SVG omissions remain explicit.

## Batch and invariants

1. Append a rule to an existing bucket tail in constant work. Allocate and build
   an index lazily at the existing threshold; ordinal exhaustion discards the
   derived index before increment and permits a compact cold rebuild.
2. Unlink a retired rule from its bucket before freeing it. Bidirectional
   derived links keep deletion independent of unrelated rules and preserve
   indexed precedence through ordinal gaps and linear/index threshold crossings.
3. Preserve the surviving indexed position during simplification's equal-ID
   record move. Remove the old owner before copying resources, then restore the
   survivor's list/lookup links and ordinals. Subset retirement and both merge
   directions retain their existing semantic choices.

The core list remains authoritative. No callback/window retains an index or
borrowed selector beyond the existing synchronous owner. Rule IDs are immutable
when published. Domain bits conservatively persist until a cold rebuild; empty
buckets can add hashing work, never candidates or stale rule pointers. There is
no retained window-match result, dirty-domain owner or update-plan cache.

The tradeoff is one extra pointer per rule and 256 bucket-tail pointers plus one
ordinal per allocated index (8 bytes/rule and 2056 bytes/index on the measured
64-bit ABI). Small lists still use their existing linear lookup threshold.
No configuration migration, dependency or installation default changes. Global
update/grab, query/publication and resource lifetime barriers stay with their
existing owners. This does not move check_window_style_change out of its pass.

## Focused evidence

The unchanged-core baseline performs 16 lookup allocations after appends, 16
following deletions and one following a simplification move, in both ordinary
and forced-collision modes. The first candidate performs zero in those same
workloads. Every compared initialized result matches the reference full list
walk. Seven focused Linux checks and 41 real-client cases pass.

The final fixture adds subset retirement and the opposite simplification merge
direction. That fixture-only extension passes without a production rebuild.
It checks complete initialized style results, normal and forced-collision bucket
ownership, repeated append/delete cycles, below-threshold lookup, deletion of all
rules, and applied border/frame/client geometry plus _NET_FRAME_EXTENTS.
The controlled collision keeps exact matching as the final predicate.

The initial baseline preparation stops on the prior report hash before copying
sources or compiling/testing. The previous scheduler correction intentionally
updated its fixture alone, leaving the report at the original production source.
The reconciled prior manifest records precisely that mixed documentation/fixture
scope. The resumed baseline passes; the stopped preparation is retained.

Owner artifacts: builddir/codex-2026-10-19/style-index-maintenance/.
Sources, modes, producer scripts, compiler options and runtime receipts are frozen
at the combined checkpoint. Focused evidence does not establish full platform
qualification. Measurements count selected avoided work, not universal latency.

## Required limitations

One combined six-target/ten-profile checkpoint and the controls below have
completed. Preserve the known
OpenBSD AMD64 CI-01 two-second worker failure from PR #113 and IMAGE-01 SVG/MFL
startup refusal from PR #109 as required omissions, including its fresh-install
and staged-geometry gap, without retries for green. Causes are unproved. VESA
#93 stays deferred. Emulated checks do not establish physical native-CPU
sanitizer, hardware/toolkit/compositor or release acceptance.

## Source-bound combined checkpoint

Three incremental style-index improvements pass a composed checkpoint: one
original broad run across six targets/ten profiles (2019/2022 executed
standard, 445/445 optional), followed by three unchanged isolated fixture
controls. Fifteen actual-core index drivers retain zero selected lookup
allocations after append, deletion and all three simplification paths. Three
scoped sanitizers, nine fresh stages and four authenticated BSD displays pass.
Two known required OpenBSD AMD64 CI/SVG omissions and its install/staged-
geometry gap remain without retries.

Signed author-DCO runtime source: `24d5e6b1cf9d121a53f272ee075dacced3d2a1b3`;
tree `7720100107f0ae4c262b7e59556675e6d805b74b`; archive
`9e1abb68c10b33e6cf35382a7028d053dcf878c68bde6d6e216f19abc677cdc5` (2014
regular files). The [machine
receipt](VERIFICATION-2026-10-19-STYLE-INDEX-MAINTENANCE-COMBINED.json) records
source/mode/script/log identities, compiler/options, tested/installed bindings
and exact remaining gaps. Final upkeep changes documentation/evidence only;
every production/header/Meson/test input remains exact to the frozen source.

| Scope | Result |
| --- | --- |
| Original standard executed | 2019/2022; 2019/2024 including two required omissions; raw failures stay failed |
| Unchanged isolated controls | 3/3, same source/binaries/backpressure 20-second and xrandr 100-ms limits; causes unproved |
| Software/minimal | 445/445 |
| Index maintenance | 15 drivers, ten groups plus final status each; zero selected lookup allocations |
| Scoped ASan/UBSan | 3/3 actual-core index maintenance, style profile and window-update barriers; process-fixture leak detection disabled |
| Fresh install | 9 stages / 135 code / 88 catalog identities, with expected native RPATH handling |
| Authenticated default BSD X | 4 displays / 34 fixtures; unauthenticated clients refused; clean server exits |
| Staged geometry | 18 invocations / 126 cases |
| Real-client regression | 10 complete invocations / 410 cases |
| Portable verification | 166 passes / eleven native-Meson skips, once |
| Compiler review | 11 compile logs; no diagnostics in affected C/header files |

One original matrix plus three unchanged isolated controls serves the three
improvements. Warm owner builds retain prerequisite compilation and source
synchronization refreshes changed modification times. No full suite repeats for
the final fixture extension, control, report or PR. Baseline and focused runs
retain their own frozen inputs; no failed/stopped preparation is promoted to a
passing execution.

Storage probes before dispatch find only 91 MB free on OpenBSD ARM64 /home and
422 MB on AMD64 /home. Evidence reuses the existing larger owner backing
volumes behind the conventional /home aliases. No predecessor source, build,
evidence, VM disk or package is moved or removed, and no space requirement is
lowered. The Linux AMD64 archive preflight passes without further storage
recovery.

OpenBSD AMD64 executes all 245 selected entries successfully out of 247
registered. Its required CI-01 timeout retains PR #113 source
`2cf1f2f470d65cf1ebfb44dfe69a5edce11e7118`, archive
`1da503ccf2609ac8f94baa287d5c21705ab7a74c1f3adf1969f53dddede4d73a`, and capture
`bf17c4db44dc5a9b56150853aac21c41392c663928469e98727dcffe103da759`. Its
required IMAGE-01 omission retains PR #109 source
`97c728358660675353de35f866c833c926b4f203` and archive
`c5a8dc9783c78e30a16ae790397e304148bf7388318081b77e7ac028e7b0451a`. Neither is
retried. Its broad gate exit remains 1 for the known omissions; capture/receipt
validation exit 0 does not erase that failure. Its fresh install and two staged
geometry probes remain unperformed; built probes pass on the authenticated
Xenocara display. Causes stay unproved; VESA #93 remains deferred. Native-CPU
sanitizer, physical hardware/toolkit/compositor and release acceptance remain
separate. STYLE-01 stays open for larger plans/grabs and release coverage.

The original Linux AMD64 GCC suite fails module-backpressure-xvfb at its
unchanged 20-second burst deadline. Original source-bound artifacts and
complete failed result remain retained. The timed burst sends only Nop/Echo
after startup; no style mutation or window creation exercises the new index
maintenance there. One isolated control on exactly unchanged GCC
core/wrapper/MFL/producer binaries and driver passes all 544 ordered echo
markers, independent-input fairness, 17-producer stop and
synchronous-unlock/recovery checks at the same deadline. The topology fixture
also exceeds its 100-ms xrandr-operation limit: the original mutation returns
status 0 after 104.04 ms. One unchanged isolated topology control passes at
that same limit, including sustained-pressure commit and epoch checks. ARM
Clang also expires while waiting for the same 100-ms pressure xrandr command.
Its one unchanged isolated topology control passes, followed by its first Clang
install/client/geometry stage, with source/binaries/limits unchanged. Timing
causes remain unproved; passing controls do not erase the original failure or
establish a universal latency bound. No production or fixture source changes,
deadline increases or further retries are made. Its previously blocked first
GCC install/client/geometry stage then passes with no rebuild. The raw
checkpoint retains seven stages and the composed result has nine; both scopes
are in the machine receipt.

Only derived-index maintenance becomes constant work. DestroyStyle still
searches its owning list and marks matching windows; simplification keeps its
existing semantic scan. A warmed index may remain allocated below the linear
threshold, including after the list becomes empty; it retains no retired rule
records.
