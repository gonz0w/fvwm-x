# Window-tree walks — 2026-10-20

DATA-01 #6 / CLEAN-01 #34. Base: merged
[PR #130](https://github.com/gonz0w/fvwm-x/pull/130), `39af8814108c0e7457c23d26b8847e51613aea40`.
One coherent three-operation checkpoint; broader audits remain open.

## Result and behavior

Three ancestor/pointer/frame lookup improvements pass one combined checkpoint:
2061/2061 standard results (2055 fresh / six retained by unchanged
runtime/test/build inputs and eight binary identities), plus 455/455 optional
entries across six targets/ten profiles. Fifteen real-X drivers verify 45
hierarchy/pointer/reparent observations; ten boundary-policy profiles and three
scoped guest sanitizers pass. Nine fresh stages and four authenticated BSD
displays pass. Three required OpenBSD AMD64 omissions and its full-install/two
staged-geometry gap remain without retries.

1. Native-panel top-ancestor lookup now shares an operation-local parent-query
   owner. It initializes the returned parent, releases every owned child reply,
   rejects missing/self parents or inconsistent roots, and admits at most 4096
   parent queries. None/top-level inputs retain their previous None result;
   successful calls keep the original parent order and top-level result.
2. Core pointer lookup uses the same finite parent-query owner, while still
   checking the existing X context before requesting each parent. A query or
   budget refusal returns no managed window. No global lookup cache, context
   replacement, new server grab, query reordering or dispatch change is added.
3. IconMan frame lookup shares that parent-query lifetime and uses widened
   coordinate sums checked against int bounds. It publishes offsets only after
   reaching the frame. Failed attributes, invalid/exhausted walks or refused
   coordinates leave the original manager window and caller offsets available,
   replacing the earlier partial translation and potentially overflowing sums.

The budget bounds query count, not wall time, bytes in one Xlib reply, or one
blocking X request. Concurrent snapshots are not a transactional tree: same-root
reparenting can still change the path, but cannot make these loops unbounded.
No configuration command/name/argument, module packet, installed Perl metadata,
X/error/publication barrier or mandatory dependency changes. The uncalled
recursive client-window helper was inventoried and left outside this batch;
there is no runtime finding or deletion claim for it.

## Focused and boundary coverage

Six incremental Linux ARM64 checks pass: window-tree, window-tree-xvfb,
window-children, window-update-xvfb, iconman-transport-xvfb and
buttons-transport-xvfb. The combined GCC profile retains them through exact
production/test/build input equality and eight binary hashes. The source-bound
frozen runner asserts every binary after its ordinary combined build. All eight
binaries are also captured and compared after qualification, with another exact
source hash/mode check; this performs no build or test repetition. The later manual/tracker/report text
changes no runtime/test/build input. Retained results are not fresh executions.

Wrapped Xlib policy checks cover ordinary/top-level/None walks, exactly 4096
queries, a required 4097th query, a two-node changing-parent trace, self parents,
inconsistent roots, query failure, attribute failure, initialized parent output,
owned child cleanup and unchanged offset outputs. Positive/negative int edges
and a widened attribute-plus-border sum refuse without large allocation.
Every controlled query allocates one reply, and all exits leave none outstanding.
These are operation bounds and ownership witnesses, not measured WM latency.

Actual-core fixtures query a real 65-level X tree and its frame offsets,
exercise unmanaged and registered pointer contexts, then reparent the subtree
and verify fresh ancestor/frame results. The same driver runs on ten standard/
optional profiles, four authenticated default BSD displays and the scoped
sanitizer profile. Existing module transport and core update tests remain.

## Combined checkpoint

| Target/profile | Executed entries | Passed |
| --- | ---: | ---: |
| freebsd-amd64/default | 252 | 252 |
| freebsd-arm64/default | 252 | 252 |
| fvwm-x-linux-arm64-svg/clang | 264 | 264 |
| fvwm-x-linux-arm64-svg/gcc | 264 | 264 |
| fvwm-x-linux-arm64-svg/minimal | 212 | 212 |
| fvwm-x-linux-arm64-svg/software | 243 | 243 |
| linux-amd64/clang | 264 | 264 |
| linux-amd64/gcc | 264 | 264 |
| openbsd-amd64/default | 249 | 249 |
| openbsd-arm64/default | 252 | 252 |

All 2499 predecessor registrations retain relative order, with exactly two new
registrations on each profile: 2519 total, 2516 executed passes and three known
required omissions. Six standard results are retained and 2055 execute fresh.
Three scoped guest ASan/UBSan checks pass: window-tree with leak detection,
actual-core window-tree-X and IconMan transport with the existing X leak policy.
The captured instrumented library/profile is separate from native-CPU sanitizer
acceptance on the macOS host.

Nine fresh stages verify 135 code and 88
catalog identities after ordinary RPATH adjustment. Compared with PR130's full
qualified outputs, 27 change, 196
remain identical and 0 are uncompared. Four authenticated
BSD default displays pass 58 fixture commands, refuse an
unauthenticated client and exit cleanly. Eighteen staged geometry invocations
retain 126 cases. OpenBSD AMD64's built-core default-X pass does not close its
required full-install/two staged-geometry gap. Interactive review VNC stays closed.

The six canonical archives validate gzip EOF/CRC, every capture payload,
registrations, exact runner/source identities, results, binaries and installs.
All 2065 frozen regular input hashes/modes match signed Git before
and after native work. Seven runners freeze before dispatch. At most two heavy
guests use warm per-profile builds and Ninja -j2; slow OpenBSD AMD64 starts first
to overlap the other targets. No checks or deadlines are removed for speed.
Portable verification runs once: 166 passes / eleven native-Meson skips.
Source/ownership review, changed-line compiler diagnostics and final documentation
checks pass. Publication runs no additional matrix.

## Provenance and remaining work

Signed author-DCO runtime `3ee37c1d739aacfe4975d067fa7f36e332711a1b`, tree `eae5f68f299ccbab74594b0f84f4d6249f649d89`.
Archive `04b2e9c870c3d6e31e43e4eaf2d5af0f0e1805297418a24891ba12451577a3bc`; 2065 regular inputs.
[Machine receipt](VERIFICATION-2026-10-20-WINDOW-TREE-WALKS-COMBINED.json) binds source, profiles, scripts and
artifact identities. Raw evidence: `builddir/codex-2026-10-20/window-tree-walks`.
Final upkeep edits guides/report/receipt data and precisely describes root/budget
refusal in the two manuals; runtime, tests and Meson remain exact. All 26 manuals render through the existing Asciidoctor with exact final inputs.
The first isolated preparation omitted doc/index.adoc.in; that failed archive is
retained. Supplying the frozen index input and repeating only the documentation
check passes. Documentation edits do not repeat runtime tests.

Linux owner capacity initially measured 701042688 free bytes, below the unchanged
1 GiB floor. Full canonical host gzip/CRC and 1870-file comparison permitted
retirement of only the completed PR130 duplicate capture/archive:
409773854 bytes; free space became 1116491776.
Host canonical archives, source/build/stage trees and VM disks remain intact.
The existing logical OpenBSD /home owner links resolve to /usr/obj. A separate
placement check found only 91449344 / 422182912 bytes free on /home (ARM64/AMD64),
below the unchanged 1 GiB owner floor. The established /usr/obj owners retain
their verified available capacity; no OpenBSD source/build/stage/evidence tree
was moved or removed, and frozen runners, checks and deadlines stay unchanged.
The first observation was while ARM64 remained queued; the persisted capacity
receipt was collected as that target started.

The three inherited required OpenBSD AMD64 omissions (CI asset-worker timeout,
SVG/MFL startup refusal and PR120 legacy-property startup refusal) remain without
retries. Their original controls/cause scope, full-install and two staged geometry
gaps remain. No old failure is relabelled as a new pass. Native BSD kernels run
under QEMU and Linux is macOS-hosted; native-CPU, physical GPU/input/mixed monitors,
actual toolkit/compositor and release checks remain separate. VESA #93 is deferred,
Actions disabled, and broad DATA/CLEAN/IMAGE owners remain open.
