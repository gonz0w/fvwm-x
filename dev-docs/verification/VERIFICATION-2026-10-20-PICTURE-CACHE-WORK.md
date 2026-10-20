# Picture-cache lookup and retirement — 2026-10-20

CACHE-01 #33 / CLEAN-01 #34. Base: merged [PR #128](https://github.com/gonz0w/fvwm-x/pull/128),
`9b1431e2e340d1ad28311566b67fd4624e6f2fa3`. One coherent three-operation runtime checkpoint.
Broader cache/path/population profiling remains open.

## Result and contract

Three picture-cache variant/name/retirement improvements pass one combined
checkpoint: 2045/2045 standard results (2038 fresh / seven retained by exact
source and nine binary identities), plus 451/451 optional entries across six
targets/ten profiles. Selected pathname comparisons fall 65 to one/two; two
predecessor walks fall 16,384/32,385 to zero. Five scoped guest sanitizers,
nine fresh full stages and four authenticated BSD displays pass. Three required
OpenBSD AMD64 omissions and its full-install/two staged-geometry gap remain
without retries.

| Selected workload | Previous | Current |
| --- | ---: | ---: |
| Pathname comparisons for oldest of 65 generations | 65 | 1 |
| Pathname comparisons for oldest of 65 distinct long names | 65 | 2 |
| Predecessor visits releasing 64 uncached records beside 256 cached records | 16,384 | 0 |
| Predecessor visits retiring 256 cached records oldest first | 32,385 | 0 |

The same controlled C probe runs against two source-instrumented production
copies using the real native compile/link commands. Only counters are inserted
in the pathname comparison and predecessor-walk sites. Baseline source and WM
identities bind to the preceding qualified runtime; the candidate Picture.c
hash binds to this frozen checkpoint. These are operation counts, not
uninstrumented wall-clock, whole-WM or physical native-CPU latency results.

The three substantive changes are:

1. Variant attributes, rational scale, requested size and generation reject
   incompatible entries before reading their pathnames. The existing cache
   list and its newest-first result precedence remain authoritative.
2. Published records retain a 64-bit fingerprint of their immutable name.
   Lookup compares the first compatible candidate directly, keeping the common
   head-hit path free of a request hash. Only a longer search computes one
   request fingerprint; mismatches reject candidates, while every hash match
   still requires complete string equality and the existing file-stamp check.
3. Published records retain their incoming cache link. Final release repairs
   that link and the surviving successor in constant work, then clears membership
   before resource release or view callbacks. Non-final references retain
   membership. Uncached records and views need no search; raw data cleanup also
   withdraws any membership. Native pixmap publication uses the same owner.

Two internal fields are appended to FvwmPicture: a link pointer and uint64_t
fingerprint. The bound Linux ARM64 DWARF records grow from 192 to 208 bytes;
this adds sixteen bytes per record,
without a separate allocation or index. Existing fields keep their types/order.
All production constructors zero-initialize records; publication sets the new
fields. Shipped name assignments happen during construction or before async
acknowledgement/publication. Embedded module snapshots are field consumers,
not cache members, and no raw picture-record packet or installed Perl metadata
changes. Clones continue to retain the same record, rather than copying links.

Path search remains per request. Every otherwise matching entry still receives
its current filesystem stamp check; changed/missing files and failed replacements
retain existing behavior. SVG option prefixes stay in name equality while stamp
reads use the real path. Old backing pixels/references remain usable after reload.
Deferred views still publish after acknowledgement, and release their subscription,
owner binding and backing in the existing order. No new global cache authority,
persistent filesystem cache, configuration migration, dependency, user option,
command/X publication barrier or main-loop work is introduced.
The disabled pixmap-reuse loop is removed alongside its publication migration;
no performance benefit is claimed for code the compiler could already eliminate.

## Focused and combined qualification

Seven focused Linux ARM64 GCC entries pass: picture cache, real resource-cache
reload/pixels, bounded ARGB conversion, deferred pictures, core async adaptation,
image owners and module packets. They are retained only after exact frozen
source/modes and nine fixture/helper/WM hashes match. Existing test registrations
are extended in place, with no new suite entry or changed deadline.

The controlled cache unit forces a fingerprint collision and checks exact-name
refusal, empty/high-byte names, four deterministic retirement sequences, 516
final retirements and 33,024 surviving-picture lookups per invocation. It checks
retained references, cached/uncached pixmaps, raw-record cleanup and a view
callback that recursively releases its cached neighbour. Existing variant keys,
reload/refusal and SVG stamp-path cases remain.

| Target/profile | Executed standard or optional entries | Passed |
| --- | ---: | ---: |
| freebsd-amd64/default | 250 | 250 |
| freebsd-arm64/default | 250 | 250 |
| fvwm-x-linux-arm64-svg/clang | 262 | 262 |
| fvwm-x-linux-arm64-svg/gcc | 262 | 262 |
| fvwm-x-linux-arm64-svg/minimal | 210 | 210 |
| fvwm-x-linux-arm64-svg/software | 241 | 241 |
| linux-amd64/clang | 262 | 262 |
| linux-amd64/gcc | 262 | 262 |
| openbsd-amd64/default | 247 | 247 |
| openbsd-arm64/default | 250 | 250 |

All 2,499 preceding registrations retain per-profile order. The 2,496 executed
standard/optional results pass; three required OpenBSD AMD64 omissions remain.
Seven focused results are retained and 2038 standard results
execute fresh. Optional profiles execute their registered feature scope.
Actual-core style/update/state/command/index and real-client checks remain in
the full checkpoint; this batch does not replace them with a cache-only suite.

Five scoped guest ASan/UBSan entries pass: cache policy with leak detection,
and resource-cache/ARGB/deferred-picture/core-async X fixtures with their scoped
leak policy. The async synthetic worker retains its existing unsanitized Meson
fixture override; the cache/picture library and client probes are instrumented.
Native-CPU sanitizer acceptance remains separate.

Nine fresh full install stages verify 135 native code and
88 catalog identities after normal RPATH adjustment.
Compared with PR128’s qualified scoped-install outputs, 90 identities
change and 133 remain identical; 0
are uncompared. Default BSD Xorg/Xenocara uses four authenticated private displays
and passes 54 fixture commands with unauthenticated-client
refusals and clean server exits. In addition to existing core/geometry checks,
each display exercises actual resource-cache reload, ARGB pixels, deferred views
and the core async adapter with a separately bound synthetic worker. Built test
probes are identified separately from fresh installed WM/module consumers.
Eighteen staged geometry invocations retain 126 cases. The OpenBSD AMD64 built
core can run its default-server checks while its required full-install gap stays
open. The user-closed interactive review VM/VNC remains closed.

Every guest checks all 2057 frozen regular input hashes/modes
before/after its work. Six canonical archives validate full gzip EOF/CRC,
source/runner/capture bindings, registrations, binaries and installed identities.
Seven executed runners are frozen, and at most two heavy guests use Ninja -j2.
Portable make verify runs once: 166 passes and eleven explicit native-Meson
capability skips. Local source/ownership review has no findings; compiler logs,
changed-line diagnostics and final documentation checks are retained.
No full matrix repeats per item or merely for PR creation.

## Evidence and remaining gaps

Signed author-DCO implementation: `9fb7bdf23facd1a12ccdae5596211d534208e76f`;
tree `a4739f3a896f83f68aa6acf9564a1d95a27670a6`. Archive SHA256:
`dc20de900e7e19dc17cf5c46652a9f7eae95484ed234bbbcfed829706f0f91c1` (2057 regular files).
All Git file bytes/modes match the frozen archive; final upkeep alters Markdown
and the machine receipt only. [Machine receipt](VERIFICATION-2026-10-20-PICTURE-CACHE-WORK-COMBINED.json)
records exact profile/source/install/script identities and operation witnesses.
Raw evidence remains under `builddir/codex-2026-10-20/picture-cache-work`.

Only completed task-owned duplicate guest capture data was retired after full
canonical host gzip/CRC and individual file verification: 207,705,903 bytes.
Host originals, source/build/stage trees and VM disks remain. Free owner space
was restored to 1,403,142,144 bytes; 1 GiB owner / 256 MiB temporary floors remain.

The three existing required OpenBSD AMD64 omissions remain with their original
owners: CI asset-worker timeout, SVG/MFL startup refusal and PR120 legacy-property
startup refusal. Its original unchanged control and unproved cause remain;
there are no new retries or deadline changes. Full install and two staged
geometry probes remain unqualified there. Earlier failures/controls retain their
original source/profile scopes. Linux is macOS-hosted; BSD native kernels run
under QEMU. Native-CPU sanitizers, physical GPU/input/mixed monitors, actual
toolkit/compositor and release checks remain separate. VESA #93 stays deferred,
Actions stays disabled, and broader CACHE-01/CLEAN-01 scopes stay open.
