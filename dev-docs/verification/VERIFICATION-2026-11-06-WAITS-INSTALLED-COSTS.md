# Reply-wait feasibility and installed package costs

This checkpoint addresses the remaining observation and cost questions for
[#211](https://github.com/gonz0w/fvwm-x/issues/211) and
[#212](https://github.com/gonz0w/fvwm-x/issues/212). It follows the
[four-profile BSD paired fixture](VERIFICATION-2026-11-06-BSD-DRAWING-LAYOUT-PAIRS.md).

## Reply-wait observation

The retained Linux ARM64 profile records Xlib package
`2:1.8.7-1build1`, XCB `1.15-1ubuntu2` and Cairo `1.18.0-3build1`.
Their exact library files remain pinned in the
[G6 cost data](VERIFICATION-2026-11-04-DRAWING-LAYOUT-COSTS.json).
The matching Xlib and XCB source archives and packaging changes were
retrieved from the Ubuntu archive. Their hashes matched their downloaded
source descriptors. This source inspection supplies the following
feasibility findings. [Xlib source descriptor](https://archive.ubuntu.com/ubuntu/pool/main/libx/libx11/libx11_1.8.7-1build1.dsc) ·
[XCB source descriptor](https://archive.ubuntu.com/ubuntu/pool/main/libx/libxcb/libxcb_1.15-1ubuntu2.dsc)

XCB's static `wait_for_reply()` flushes pending output, polls for the
requested reply, and enters `_xcb_conn_wait()` when that reply is absent.
Both public reply functions reach this branch, as does
`xcb_request_check()`. Xlib's `_XReply()` calls
`xcb_wait_for_reply64()` and may process earlier pending requests first.

The reply-specific branch is internal. The connection-wait declaration
sits inside XCB's conditional hidden-visibility region. The packaging export
list supplies the public reply APIs. A counter at a public entry would
include buffered replies, and one limited to the two reply APIs would miss
the direct static call from `xcb_request_check()`. Counting Xlib and XCB
entries together would also duplicate nested work.

The lower connection-wait routine serves event and output work as well.
Its implementation selects a condition wait, poll or select. Separately,
`read_block()` can wait for the remainder of a partially received packet.
A counter at the reply branch therefore describes logical reply waits.
Complete blocking-episode coverage also needs the partial-read path and
connection/request attribution. Entering a poll call alone cannot establish
that the kernel descheduled the thread.

The existing source and captured library metadata supply no qualified
interposable seam covering that complete requirement. Native probe execution
stopped at this prerequisite. The selected asynchronous, buffered-reply,
outstanding Xlib, direct-XCB and other-connection controls remain unexecuted.
A private matching-source XCB build with explicit observation at the reply
and partial-read branches is the concrete next implementation candidate.
Its complete coverage and library binding need qualification before the
five controls and candidate measurements can establish implicit waits.

| Source input | SHA-256 |
| --- | --- |
| Xlib 1.8.7 original archive | `793ebebf569f12c864b77401798d38814b51790fce206e01a431e5feb982e20b` |
| Xlib Ubuntu packaging diff | `f475e5203caa43e2881df9f0f25cd5a1c6dfb1ad77d9eb057e3dbe17a46e8563` |
| XCB 1.15 original archive | `1cb65df8543a69ec0555ac696123ee386321dfac1964a3da39976c9a05ad724d` |
| XCB Ubuntu packaging diff | `d45e55f604af83b47f621c97b49885f35b518909bb3b9f9c877cfd3daddfb5ee` |

The inspected locations are Xlib `src/xcb_io.c` around
`_XReply()`, XCB `src/xcb_in.c` around `wait_for_reply()`,
`read_block()` and `xcb_request_check()`, and XCB
`src/xcb_conn.c` and `src/xcbint.h`.

## Installed package dependency closures

One FreeBSD 15.1 ARM64 guest supplied 596 installed package records and
1,067 declared dependency edges. Every dependency name and version resolved,
and every package's edge count matched its declared count. The local package
manifest matched before and after capture. All eight compared executable
files and 32 resolved library files matched their retained G6 hashes.

The new pure `package_costs.compare()` helper validates the complete
snapshot, computes dependency closures, deduplicates shared packages and
returns additions and removals. It rejects absent records, version
mismatches and invalid sizes before returning a result. Cycles are allowed.
Seven focused tests passed through the maintenance test discovery entry.

The roots below are the packages owning the exact resolved library files
of each native and candidate module. The byte totals sum nominal installed
package flat sizes, including their declared dependencies.

| Comparison baseline | Native bytes | Candidate bytes | Added bytes |
| --- | ---: | ---: | ---: |
| Buttons native drawing package closure | 32,773,196 | 392,230,269 | 359,457,073 |
| Form native drawing package closure | 34,621,428 | 394,078,501 | 359,457,073 |
| Buttons native layout package closure | 32,773,196 | 32,773,196 | 0 |
| Buttons drawing with recorded librsvg package in both baselines | 459,791,462 | 459,791,462 | 0 |

The two drawing comparisons add ten packages in this package snapshot.
Cairo declares GLib as a dependency. GLib declares Python and its packaging
module. Those records explain most of the added nominal bytes.
The following rows show the actual declared chain and package sizes.

| Package | Version | Flat bytes |
| --- | --- | ---: |
| cairo | 1.18.2,3 | 3,950,592 |
| glib | 2.86.4,2 | 111,131,077 |
| python312 | 3.12.14 | 229,707,606 |
| py312-packaging | 26.2 | 1,169,815 |
| pixman | 0.46.4 | 759,484 |

The remaining added packages are libffi, lzo2, mpdecimal, pcre2 and readline.
With `librsvg2-rust` version 2.62.3 in the baseline root set, the two closures
already include all ten packages. This conditional comparison addresses
package reuse when SVG is required. The separate process loading and RSS
results remain in the paired reports. Each comparison retains the complete
package sets in its local result.

The six base-system library files total 2,661,848 logical bytes. The private
PNG library is a separate 890,776-byte file. Both remain outside the package
sum and are shared by native and candidate paths. The package graph still
contains the vendor PNG dependency declared by its packages.
Executable file sizes, source assets, staged product files and filesystem
allocation require their own accounting. The package calculation covers
this exact FreeBSD ARM64 snapshot. Other platform package closures and
complete staged/release acceptance remain open.

## Capture, repair and continuation

The first host admission stopped before creating or booting a guest because
another project VM was active. A reviewed admission allowed this data-only
capture alongside one other VM within the existing total-two resource limit.
Its timing carries diagnostic scope.

The first native attempt stopped at the completeness gate. `pkg query`
emitted one empty dependency row for each of 172 packages declaring zero
dependencies. The single reviewed retry skips that exact placeholder only
when the declared count is zero. It retains partial-empty, missing-version
and edge-count failures. The retry passed all 53 package queries, rechecked
library identities and retired every query process group.
Both native guests exited normally with QEMU status zero and unchanged
backing chains. The superseded failed disk was retired, releasing 7,405,568
allocated bytes. Its graph and diagnostics remain.

Source base `a27795528b7a0fcdf8b69ad22227736ad9717d2b`.
Reviewed native retry plan SHA-256
`346a3a88e33bee317accf7e8827b93b283a98fc5a2c5582b728580810de81c1f`.
Successful dispatch receipt SHA-256
`a48319224f4986406e082b1cb3c6b3e501becf7ca2617b5ca9af68ff2c47cf25`.
Native package result SHA-256
`bd325976f950e9ddb211708159c22d0e3812014af6ffd4fdc24e453aff6a94f8`.
Manifest SHA-256
`6285518935cb85307d51e6538b7e62767936a71b229c9d732bef287d964c0eef`.
All 107 exported files and their archive matched the recorded hashes.

Artifacts are under
`builddir/codex-2026-11-06/r1-waits-installed-costs`.
The successful export is
`repair01/freebsd-arm64-run01/export/g12-package-cost01`.
The source archives, descriptor hashes and extracted wait implementations
are under `library-source`. The full package closure sets and the source
bindings are retained in `package-comparisons.json`.

The wait seam remains unqualified. A matching private XCB observation
build and its five controls are the next proposed wait work. Package
closure capture can extend to the remaining selected platforms using the
same complete-metadata contract. R1 retains the native rendering and
layout decisions. Both owner issues remain open for their pending criteria.
