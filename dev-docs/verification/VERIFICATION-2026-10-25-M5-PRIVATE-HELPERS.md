# M5 unused private helpers — 2026-10-25

Owner: CLEAN-01 #34. Base: merged PR244
`55333b31a57b582e3ad269a01a9210421396348a`.
**Focused checks pass; broader acceptance remains pending.**

Retire six unused private-library functions: timeout_create/destroy/tick/rewind,
fqueue_remove_or_operate_from_end and getostype. Delete the timeout source/header
and build entry, plus getostype's exclusive uname probe/include. Eight source/build
files lose 205 lines. Tracked definition/declaration, type/header and build searches
find no consumer. libfvwm3 is static and not installed; no installed header/plugin
contract exists. Independent reconstruction verifies every retained source/build
byte after exactly those removals. Active queue operations, callback/death-row
helpers, descriptor limits, file stamps, configuration and pathname code remain.

## Combined focused gate

Reuse the owned Linux ARM64 normal GCC build in `fvwm-x-linux-arm64-svg`,
`/tmp/fvwm-module-waits-20261015/build-gcc`. This is local guest execution,
not separate native-CPU qualification. Before edits, 536 tracked C/header/build/
test inputs match the base; after two deletions and the fixture repair, 534 final
inputs match host/guest hashes and modes. Three baseline objects and actual
compiler commands/configuration are preserved.

The original grouped build completes core and all twelve C modules. Its test
link then fails because the unchanged module-runtime fixture omits the existing
appearance_preview_owner_gone callback. Preserve that failure. The bounded fixture
repair includes the real prototype and observes notification/lifetime only:
first kill before retirement, repeated-kill suppression, pinned cleanup, final
destruction and direct destroy. Its original sixteen tests and invocation order
remain byte-identical. The stub does not exercise preview recovery.

Continue only the four unfinished test targets, retaining the thirteen successful
production builds. fqueue, fd-width, event-loop-policy and module-runtime pass
4/4; seventeen final output identities are recorded. First grouped build:
12.687 command-seconds; remaining test build, units and comparator: 0.425 seconds.
These durations describe this run, not a performance comparison.

Actual-flag preprocessing retains **79,171 System tokens and 28,124 fqueue tokens**
exactly after removing only the named definitions/declarations and the now-unused
utsname structure/uname declaration. No diagnostic/token normalization is used.
The generated config differs only by the HAVE_UNAME definition and adjacent
separator. The private comparator initially expected the definition's newline
without that separator; preserve the first failure/script, inspect the exact
config diff and correct only that expectation. Successful preprocessing is reused,
not repeated. No production source change follows either gate failure.

The two retained object symbol sets lose exactly getostype and the queue-tail
wrapper with no additions; the final library archive also lacks all four timeout
definitions and its object member. All other global definitions in the two
changed objects remain. Retained source and preprocessing establish active-code
parity; no whole-object/binary byte equivalence is claimed.

Repository checks/lint and working/staged whitespace pass. Separate source,
fixture and final evidence review is clear; signed author-DCO delivery follows.

## Evidence and limits

Canonical private archive: `m5-private-helpers/native-evidence.tgz` under the
October 25 ignored development tree: 66 declared payloads plus their manifest,
448,949 bytes, SHA256
`109bcd3966bcfc8f5c25238ea017dbbdb2c335ca9d164c81a8e606f1d0a085ac`.
Every payload hash, size and mode is verified. It retains baseline objects/source,
compiler commands, input manifests, both original failures, scripts, final source,
raw build/test output, preprocessing, symbols and output identities. Verified
guest capture/archive duplicates are retired after another exact manifest and
active-reference check; source/build caches and the PR243 reference remain.

No active memory/X/rendering behavior changes: no new sanitizer, X fixture,
OS/architecture/compiler-feature/install or theme/screenshot pass is claimed.
Prior ownership/platform evidence keeps its original source/profile scope.
Full milestone/release/native-CPU and physical/toolkit/compositor acceptance stays
separate. Broader #34 remains open; #35/#36 remain completed and #93 not planned.
The separately blocked environment-expanded pathname proposal remains pending;
this independent retirement does not retry or resolve it.

The due comparison was completed in PR244 before this batch. PR243 is the retained
reference at 1/5 after merged PR244; this candidate would make 2/5 on merge.
No new performance gain or benchmark is claimed here.
