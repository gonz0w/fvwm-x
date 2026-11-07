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

Linux native qualification remains pending. The first attempt patched and
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
invocation would retain the race. This correction is documented for the
next selected checkpoint and remains unexecuted.

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
`linux/repair01/capture/export`. The latest private source/build in the
stopped container is `/tmp/fvwm-appearance-a4-20261102-01/g13-xcbwait02`.
It retains the current state for the next build prerequisite check.
The earlier private tree can retire at that next owned session while
its compact host diagnostics remain.

Both FreeBSD package profiles now have complete snapshot calculations.
Linux and OpenBSD package costs and the wait qualification remain open.
The periodic whole-WM ledger remains at 4/5.
