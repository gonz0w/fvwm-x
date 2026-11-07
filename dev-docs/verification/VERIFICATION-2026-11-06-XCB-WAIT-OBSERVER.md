# Private XCB wait observer and AMD64 package costs

This checkpoint implements the private observer selected after the
[wait-source assessment](VERIFICATION-2026-11-06-WAITS-INSTALLED-COSTS.md)
for [#211](https://github.com/gonz0w/fvwm-x/issues/211) and
[#212](https://github.com/gonz0w/fvwm-x/issues/212), and extends the
installed package calculation to FreeBSD AMD64.

## Private observation scope

The fixture patches a separately extracted libxcb 1.15 source tree.
It adds a standalone observer object and hooks in the reply-unavailable
branch and partial-read wait path. One active scope observes one
connection in the calling thread. A fixed 64-record array retains
request sequences, completion and wait counts. Invalid nesting,
unfinished scopes and overflow make the result invalid.

The reply context starts after output flush. Its first unsuccessful
reply poll counts one logical reply wait. Repeated calls to the
connection wait increment a separate counter. The partial counter
records EAGAIN wait-branch entries while a matching reply context is
active. It describes partial packet waits during reply processing.
Such reads may consume an earlier packet on that connection.

The six native controls cover an asynchronous request, a client-buffered
reply, an outstanding direct XCB reply, an outstanding Xlib reply, a
checked-void request and work on another connection. The outstanding
controls release their owned server grab after an atomic acknowledgement
from the actual hook. The buffered control retrieves a later reply
before measuring the earlier queued reply.

The observer API and reply API must resolve to the same private library
file. The probe also checks that Xlib resolves its reply dependency to
that instance. Private source, compiler, generator and library identities
belong to the frozen job.

## FreeBSD AMD64 installed package costs

The existing G12 collector and calculator ran against the retained
FreeBSD AMD64 profile in an isolated COW child. All eight comparison
executables and 32 library files matched their G6 hashes. The snapshot
contains 603 packages and 1,082 dependency edges. Every name, version
and edge count resolved, and the package manifest matched before and
after collection.

| Package closure baseline | Native bytes | Candidate bytes | Added bytes |
| --- | ---: | ---: | ---: |
| Buttons drawing | 33,163,920 | 392,746,666 | 359,582,746 |
| Form drawing | 34,856,032 | 394,438,778 | 359,582,746 |
| Buttons layout | 33,163,920 | 33,163,920 | 0 |
| Buttons drawing with recorded librsvg package in both root sets | 464,403,732 | 464,403,732 | 0 |

The byte totals are nominal package flat sizes for complete dependency
closures. The Cairo comparison adds the same ten packages as ARM64,
including GLib and its declared Python dependencies. Requiring the
recorded librsvg package in both baselines includes those packages
already. Clay adds zero packages. The retained logical file and process
RSS measurements have their own scopes.

The data-only capture passed all 53 queries. All query groups retired,
QEMU exited normally with status zero, and the backing chain and firmware
sources retained their admission identities. All 107 exported files and
the archive matched their recorded hashes.

Reviewed package plan SHA-256
`5abda25846d619032166a1e46b7f1722ea8a58f65a820d43301cab49fdd9e528`.
Dispatch receipt SHA-256
`f4bac9d4b3a05806ebba4b4a8dc920557ba1b7b9097c0c80d0cbf7e67596fd01`.
Package manifest SHA-256
`bdaa20e8a94fde807a5091996279cf1a1758b050dcad8651862460539f73727f`.

## Verification state and build prerequisite

The observer and exact-source patch installer passed separate source review.
Four focused Python patch tests passed using the pinned source archive.
The pure C observer unit also passed on macOS ARM64 with assertions enabled.
These checks cover counter state, connection/request filtering, invalid nesting,
unfinished scopes, record limits and refusal to reapply a source patch.

At the initial draft checkpoint Linux native qualification remained pending.
The first attempt patched and
configured the private source, then stopped before C compilation because
fresh extraction timestamps caused Make to try regenerating shipped
Autotools inputs. The single reviewed correction preserved the archive's
regular-file timestamps in a fresh source directory.

The retry patched and configured successfully and started the observer
object and core compilation. It stopped at the missing generated
`xproto.h`. The selected direct `libxcb.la` target bypassed the release
Makefile's generated-source ordering. The observer unit, probe build and
six native controls were not reached. The native attempt limit was reached;
the result is a draft source checkpoint.

Source inspection identifies the next build prerequisite. First generate
`xproto.c`, `bigreq.c` and `xc_misc.c` in a separate Make invocation.
Their recipes also write the matching headers. Check all six files before
building the private core target. A combined parallel generation/library
invocation would retain the race. This correction was documented for the
next selected checkpoint; the continuation below records its execution.

The source is libxcb 1.15 with the matching Ubuntu generator patch.
The isolated generator uses pinned xcb-proto 1.16.0 XML and Python source,
with a private pkg-config file pointing at that source tree.
The observer, unit and probe files are developer fixtures.
The system library and production install paths retain their own evidence.
A controlled fragmented-reply case is still needed for native partial-read
qualification. FVWM draw-operation integration, complete implicit waits,
kernel sleep and broader consumer/release qualification remain open.

Both native attempts verified the existing Linux tool/library profile,
recorded the Xlib/XCB bridge package version and its file identity, and
retired all invoked command groups. The bridge was observed at admission,
rather than supplied by a historical file pin. Its SHA-256 was
`29c7234b66338d3069707aa4108175287e21e68677ed3de078ab92cf0386cdd4`.
The package version was `2:1.8.7-1build1`.

The Linux raw package metadata now records 529 installed packages with
versions, installed sizes, dependency alternatives and providers. Its
complete closure calculation remains pending. The two build attempts each
exported 13 diagnostic files with matching archive and file hashes.
Both holder receipts confirmed container and machine shutdown.

## Source and continuation

Source base `e56489d442303fc14a0c437cce977a5f90cf243b`.
The initial Linux plan SHA-256 is
`04ed1898f333a9187e271f942c51889eddd8ee9561dd6daeee01a51a85534217`.
The single correction plan SHA-256 is
`5eeb43cd41e5c9ce56ad8fb6b5808ce4c28f276bc6ebecb2177ec02f3b6e582a`.
The source libxcb archive SHA-256 is
`1cb65df8543a69ec0555ac696123ee386321dfac1964a3da39976c9a05ad724d`.
The xcb-proto archive SHA-256 is
`d9c7f010b1105fc3858bf07b5169b2dd8e7493c6652b1fe45f3321d874f291d7`.

Artifacts are under `builddir/codex-2026-11-06/r1-xcb-wait-observer`.
The package result is in `packages/freebsd-amd64-run01/export`.
Linux attempt exports are in `linux/capture/export` and
`linux/repair01/capture/export`. At draft publication the latest private source/build in the
stopped container was `/tmp/fvwm-appearance-a4-20261102-01/g13-xcbwait02`.
It retains the current state for the next build prerequisite check.
The earlier private tree can retire at that next owned session while
its compact host diagnostics remain.

Both FreeBSD package profiles now have complete snapshot calculations.
Linux and OpenBSD package costs and complete wait qualification remain open.
The periodic whole-WM ledger remains at 4/5.

## Native continuation — 2026-11-07

Cam selected completion of draft PR364 with the diagnosed protocol-generation
correction. The lead retained native execution and integration. Separate
review cleared the frozen inputs before each run. The budget was one attempt
plus one diagnosed, separately reviewed correction, using the existing single
Linux ARM64 guest and 900-second private build limit.

A separate Make invocation generated `xproto.c`, `bigreq.c` and `xc_misc.c`.
All three sources and their paired headers were nonempty regular files and
were hashed before the second invocation built `libxcb.la`. The native C
observer unit passed with assertions enabled, and the probe compiled with
`-Wall -Wextra -Werror`. The generated headers resolved the original build
failure without installing packages or changing the system XCB library.

The first continuation run passed asynchronous, buffered and direct-XCB
controls, then failed the Xlib reply-context count. Pinned Xlib 1.8.7
`_XReply` retains its last successful pending-request node. The next query
visits that prior node before retrieving its current reply. Pinned XCB marks
the older sequence complete when the newer reply arrives. The original
one-context expectation therefore mischaracterized this Xlib path.

The correction requires two exact request records, using
`LastKnownRequestProcessed` after warmup and `NextRequest` before the measured
query. The prior record must retire without a reply after at least one wait;
the current record must return its buffered reply with zero waits. Both must
complete. The exactly-one-logical-wait expectation remains unchanged.
Failure diagnostics now identify the observed flags and reply counts.

The corrected run passed the native observer unit and all six controls:

| Control | Reply contexts | Logical waits | Connection-wait calls |
| --- | ---: | ---: | ---: |
| Asynchronous request | 0 | 0 | 0 |
| Buffered reply | 1 | 0 | 0 |
| Outstanding direct XCB | 1 | 1 | 1 |
| Outstanding Xlib | 2 | 1 | 1 |
| Checked void | 1 | 1 | 1 |
| Other connection | 0 | 0 | 0 |

All partial-read counters were zero. The controller acknowledged the actual
wait hook before releasing its owned server grab in each outstanding case.
The probe verified that its observer/reply APIs and Xlib's reply dependency
resolved to the same private library. All recorded tool/library identities
matched the frozen profile; the Xlib bridge identity matched before and after.
This is native Linux ARM64 execution in the existing profile, with Xvfb.

The successful job retired all 13 command groups. Its 30 exported files and
archive matched their recorded hashes; the failed continuation's 27 files
and archive also matched. Both launches confirmed container and machine
shutdown. No system installs, package installs or WM runs occurred. Broader
platform, sanitizer, theme and performance reruns were not selected because
this continuation changes the private qualification fixture only.

First continuation plan SHA-256
`9ad8d1121d70eeeba1f17746b949fa25fd7c345d057aca04a6ff1d69d63f6634`.
Successful correction plan SHA-256
`3a724186630128241f495bd28c6778f9a5da4f122e880c752e20a595264f93c8`.
Successful dispatch receipt SHA-256
`457b16b032f3901f1a93b7658b4aea1f21b35d86b00c8bcfc91f4c5c6dde8c2f`.
Private library SHA-256
`571373c623fac9d56f5221a95455fe8172b4ffccb0ccb8037525cd4e3a0781f1`.
Probe SHA-256
`7ef516d02f45e88c7d903e0b8faede71ed648772b98e20bbde19e0a66b2fd238`.

Source fixture base `113d818a884d8ed5f654b355b18dfed28c3fa51f`, plus the
reviewed probe correction in this delivery. Frozen probe source SHA-256
`c7a8fd055a70d835719798edde089516ca300a7d9c7ecd77170905be1809ca10`.
Artifacts are under `builddir/codex-2026-11-07/r1-xcb-native`, with the
successful capture in `repair01/capture/export/evidence`. The latest private
build remains in the stopped container at
`/tmp/fvwm-appearance-a4-20261102-01/g13-xcbwait04`.

This completes the selected native logical-wait fixture qualification.
Controlled fragmented-packet qualification, FVWM draw-operation integration,
complete implicit-wait coverage and remaining Linux/OpenBSD package closures
remain open under #211/#212. These results do not establish kernel sleep,
whole-process multithread coverage or release acceptance.
