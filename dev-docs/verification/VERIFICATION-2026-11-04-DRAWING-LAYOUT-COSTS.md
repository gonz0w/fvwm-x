# Drawing and layout platform costs and R1 decisions

Date: 2026-11-04. Owners: GRAPHICS-02 [#211](https://github.com/gonz0w/fvwm-x/issues/211)
and LAYOUT-01 [#212](https://github.com/gonz0w/fvwm-x/issues/212). G6 selects four
related outcomes: reconcile existing comparisons, collect platform costs, record
finite production decisions, and map remaining acceptance. This report does not
close either issue or qualify a release.

## Decision and evidence boundary

Retain the existing native drawing and responsive layout consumers for R1.
Reject production integration of the evaluated private
[Cairo painter](../decisions/CAIRO-R1-INTEGRATION-DECISION-2026-11-04.md) and
[Clay row solver](../decisions/CLAY-R1-INTEGRATION-DECISION-2026-11-04.md).
The measured workloads show no demonstrated speed gain; Cairo adds process RSS,
and Clay adds arena and adapter ownership while removing no production code.
This is a decision about the evaluated operations, not a universal renderer or
solver ranking. Both experiments remain default-disabled and non-installed;
ordinary build and runtime dependencies and existing isolated SVG remain intact.

Reuse the [G5 checkpoint](VERIFICATION-2026-11-04-DRAWING-LAYOUT-CHECKPOINT.md)
for Linux ARM64 correctness, three theme profiles/nine screenshots, 55 successful
GUI invocations covering 54 selected cases plus an affected positive, and fifteen
performance runs. Historical passes retain their exact source/ELF bindings and
failures. G6 adds no UI and repeats no GUI suite. Reuse the earlier
[two native consumers](VERIFICATION-2026-10-26-NATIVE-LAYOUT.md), preserving their
mixed-scale, text and broader support limits.

## Inputs and measurement

Current canonical source is merged PR356, revision
`5bfed2f08aa0511ccdc2b5e4514ac01136befd44`. The canonical tar contains 3601 regular
Git files, 98648888 expanded bytes, SHA-256
`9194fac19837ad64260e678c26fc9d68819d129a323d2a9603b054844e947d0f`;
manifest SHA-256
`aae9e9088a0c42d784c367785171c0c4931605e1a98f5274a8196a12e1d2d1f3`.
Linux ARM64 reuses G5 source23 and twenty executable/configuration anchors;
only README and development-preview prose differ among source23's 2505 pins.
No current-source rebuild or fresh runtime claim is assigned to that reuse.

Owned evidence root, relative to the outer workspace:
`builddir/codex-2026-11-04/r1-drawing-layout-decisions`. The structured companion
retains exact raw report hashes, source/tool/profile identities and cost
projections. Raw commands, outputs and failed attempts remain in that root.

The bounded collector SHA-256 is
`507d9ec24d7f2b27268babbdb2756ec5e7d4a74ba81a1fdecc668c73fb823dbc`.
Six offline parser oracles pass once on macOS; this is host metadata-parser
coverage, not product qualification. Native starts require the actual receipt
and separately reviewed frozen inputs. Collection reads existing build maps,
ELF sections, direct linkage, resolved libraries, package ownership, pkg-config
metadata and retained license files. It never executes a module entry point or
infers process RSS from files. Optional failures remain gaps.

Complete executable file bytes include debug/file metadata and are not resident
memory. Text/data/BSS are section observations. Resolved library bytes count each
real file once within a module closure, including a loader only when actually
observed. They are logical file lengths, not filesystem blocks, package size,
unique machine cost or process residency. Package Installed-Size is a package
manager's nominal installed size, not bytes newly required by hypothetical
adoption. Missing ownership of a private PNG build is not a zero-size package.

## Platform results

All six profiles have eight observed outputs. Five compile the current-source
modules; Linux ARM64 reuses the source-bound G5 outputs. These are module build
and metadata results, not six-target runtime/install/release qualification.
[Structured costs, hashes, exact options and raw provenance](VERIFICATION-2026-11-04-DRAWING-LAYOUT-COSTS.json)
retain all eight executable identities, sections, direct NEEDED lists, deduplicated
resolved library sets, package/license references and unavailable cells.

| Target | Release / kernel | Compiler and Meson | Execution / build scope | Collection |
| --- | --- | --- | --- | --- |
| linux-amd64 | Debian GNU/Linux 13 (trixie) / 6.12.113+deb13-cloud-amd64 | gcc 14.2.0, ld.bfd 2.44; Meson 1.7.0 | emulated AMD64 CPU (TCG); eight-module build + metadata | partial metadata |
| linux-arm64 | Ubuntu 24.04.5 LTS / 6.18.10-200.fc43.aarch64 | gcc 13.3.0, linker version unavailable; Meson 1.7.0 | native ARM64 CPU; G5 reuse; metadata only | partial metadata |
| freebsd-arm64 | FreeBSD / 15.1-RELEASE-p4 | clang 19.1.7, ld.lld 19.1.7; Meson 1.10.2 | native ARM64 CPU; eight-module build + metadata | partial metadata |
| freebsd-amd64 | FreeBSD / 15.1-RELEASE-p4 | clang 19.1.7, ld.lld 19.1.7; Meson 1.10.2 | emulated AMD64 CPU (TCG); eight-module build + metadata | partial metadata |
| openbsd-arm64 | OpenBSD / 8.0 | clang 22.1.6, ld.lld 22.1.6; Meson 1.12.1 | native ARM64 CPU; eight-module build + metadata | metadata collected |
| openbsd-amd64 | OpenBSD / 8.0 | clang 22.1.6, ld.lld 22.1.6; Meson 1.12.1 | emulated AMD64 CPU (TCG); eight-module build + metadata | metadata collected |

Cairo adds the same four observed library files to each paired drawing control:
Cairo, Pixman, XCB Render and XCB SHM. Clay adds no library to its native-layout
control's observed set. The following increments are within each profile;
file layout, compiler and package differences prevent cross-platform speed or
memory ranking. Negative BSS deltas are section/layout observations, not a
resident-memory saving. The private controls include evaluation instrumentation.

| Target | Cairo ELF increment, Buttons / Form bytes | Cairo extra resolved library bytes | Clay ELF increment bytes | Clay text / data / BSS increment bytes |
| --- | ---: | ---: | ---: | ---: |
| linux-amd64 | +23,184 / +19,080 | 2,143,256 | +310,752 | +123,300 / +224 / +1,360 |
| linux-arm64 | +14,768 / +14,760 | 2,175,432 | +326,688 | +102,584 / +456 / +1,256 |
| freebsd-arm64 | +16,064 / +16,048 | 1,950,536 | +574,616 | +132,258 / +604 / -359 |
| freebsd-amd64 | +16,592 / +16,576 | 2,180,000 | +555,528 | +160,170 / +340 / +689 |
| openbsd-arm64 | +13,408 / +13,392 | 4,373,552 | +571,384 | +149,788 / +2,004 / +1,193 |
| openbsd-amd64 | +14,040 / +13,992 | 5,072,480 | +556,000 | +175,556 / +1,596 / +441 |

Full observed file/section costs follow; resolved-library columns include the
loader where observed. FreeBSD ldd omits it: its column is an **observed subtotal**,
and complete loader-inclusive totals remain unavailable. None is RSS or package
cost. A Meson declared dependency is not proof a process loads it; direct NEEDED
and actual resolved loader maps supply that evidence.

| Target | Module | ELF file bytes | Text / data / BSS bytes | Observed resolved-library file bytes |
| --- | --- | ---: | ---: | ---: |
| linux-amd64 | FvwmButtons | 1,770,136 | 462,502 / 12,976 / 14,944 | 8,066,208 |
| linux-amd64 | FvwmButtons-cairo-eval | 1,863,144 | 485,711 / 13,336 / 44,192 | 10,209,464 |
| linux-amd64 | FvwmButtons-clay-layout-eval | 2,090,840 | 587,507 / 13,208 / 16,368 | 8,066,208 |
| linux-amd64 | FvwmButtons-native-eval | 1,839,960 | 480,147 / 13,088 / 44,192 | 8,066,208 |
| linux-amd64 | FvwmButtons-native-layout-eval | 1,780,088 | 464,207 / 12,984 / 15,008 | 8,066,208 |
| linux-amd64 | FvwmForm | 1,822,888 | 456,788 / 70,024 / 14,392 | 8,232,296 |
| linux-amd64 | FvwmForm-cairo-eval | 1,912,728 | 480,725 / 70,384 / 43,640 | 10,375,552 |
| linux-amd64 | FvwmForm-native-eval | 1,893,648 | 475,129 / 70,136 / 43,640 | 8,232,296 |
| linux-arm64 | FvwmButtons | 1,857,896 | 453,206 / 13,872 / 14,808 | 8,391,736 |
| linux-arm64 | FvwmButtons-cairo-eval | 1,927,336 | 475,261 / 14,216 / 44,040 | 10,567,168 |
| linux-arm64 | FvwmButtons-clay-layout-eval | 2,129,264 | 553,303 / 14,336 / 16,128 | 8,391,736 |
| linux-arm64 | FvwmButtons-native-eval | 1,912,568 | 470,257 / 13,968 / 44,040 | 8,391,736 |
| linux-arm64 | FvwmButtons-native-layout-eval | 1,802,576 | 450,719 / 13,880 / 14,872 | 8,391,736 |
| linux-arm64 | FvwmForm | 1,923,120 | 452,141 / 70,096 / 14,272 | 8,517,448 |
| linux-arm64 | FvwmForm-cairo-eval | 1,993,016 | 469,852 / 70,440 / 43,504 | 10,692,880 |
| linux-arm64 | FvwmForm-native-eval | 1,978,256 | 464,848 / 70,192 / 43,504 | 8,517,448 |
| freebsd-arm64 | FvwmButtons | 1,812,328 | 438,202 / 12,512 / 15,151 | 8,349,848 |
| freebsd-arm64 | FvwmButtons-cairo-eval | 1,899,824 | 459,591 / 12,776 / 47,551 | 10,300,384 |
| freebsd-arm64 | FvwmButtons-clay-layout-eval | 2,397,808 | 572,049 / 13,124 / 17,368 | 8,349,848 |
| freebsd-arm64 | FvwmButtons-native-eval | 1,883,760 | 455,171 / 12,528 / 47,887 | 8,349,848 |
| freebsd-arm64 | FvwmButtons-native-layout-eval | 1,823,192 | 439,791 / 12,520 / 17,727 | 8,349,848 |
| freebsd-arm64 | FvwmForm | 1,831,256 | 438,634 / 68,688 / 18,407 | 8,556,448 |
| freebsd-arm64 | FvwmForm-cairo-eval | 1,923,192 | 460,907 / 68,952 / 45,847 | 10,506,984 |
| freebsd-arm64 | FvwmForm-native-eval | 1,907,144 | 456,487 / 68,704 / 46,167 | 8,556,448 |
| freebsd-amd64 | FvwmButtons | 1,808,936 | 459,709 / 12,200 / 18,263 | 8,638,904 |
| freebsd-amd64 | FvwmButtons-cairo-eval | 1,895,000 | 482,726 / 12,464 / 44,951 | 10,818,904 |
| freebsd-amd64 | FvwmButtons-clay-layout-eval | 2,376,432 | 621,600 / 12,548 / 17,288 | 8,638,904 |
| freebsd-amd64 | FvwmButtons-native-eval | 1,878,408 | 477,546 / 12,216 / 46,055 | 8,638,904 |
| freebsd-amd64 | FvwmButtons-native-layout-eval | 1,820,904 | 461,430 / 12,208 / 16,599 | 8,638,904 |
| freebsd-amd64 | FvwmForm | 1,833,344 | 455,585 / 68,480 / 17,831 | 8,842,104 |
| freebsd-amd64 | FvwmForm-cairo-eval | 1,922,880 | 479,786 / 68,744 / 47,415 | 11,022,104 |
| freebsd-amd64 | FvwmForm-native-eval | 1,906,304 | 474,606 / 68,496 / 44,423 | 8,842,104 |
| openbsd-arm64 | FvwmButtons | 1,739,736 | 486,032 / 19,520 / 18,503 | 15,200,360 |
| openbsd-arm64 | FvwmButtons-cairo-eval | 1,823,728 | 509,262 / 19,952 / 44,543 | 19,573,912 |
| openbsd-arm64 | FvwmButtons-clay-layout-eval | 2,322,072 | 637,571 / 21,548 / 17,968 | 15,200,360 |
| openbsd-arm64 | FvwmButtons-native-eval | 1,810,320 | 504,649 / 19,704 / 45,311 | 15,200,360 |
| openbsd-arm64 | FvwmButtons-native-layout-eval | 1,750,688 | 487,783 / 19,544 / 16,775 | 15,200,360 |
| openbsd-arm64 | FvwmForm | 1,772,360 | 489,989 / 76,128 / 18,031 | 15,338,288 |
| openbsd-arm64 | FvwmForm-cairo-eval | 1,861,720 | 514,203 / 76,560 / 47,191 | 19,711,840 |
| openbsd-arm64 | FvwmForm-native-eval | 1,848,328 | 509,590 / 76,312 / 43,863 | 15,338,288 |
| openbsd-amd64 | FvwmButtons | 1,676,360 | 481,588 / 19,200 / 14,783 | 13,542,552 |
| openbsd-amd64 | FvwmButtons-cairo-eval | 1,757,160 | 505,506 / 19,632 / 44,231 | 18,615,032 |
| openbsd-amd64 | FvwmButtons-clay-layout-eval | 2,243,840 | 659,047 / 20,820 / 17,424 | 13,542,552 |
| openbsd-amd64 | FvwmButtons-native-eval | 1,743,120 | 500,077 / 19,384 / 45,823 | 13,542,552 |
| openbsd-amd64 | FvwmButtons-native-layout-eval | 1,687,840 | 483,491 / 19,224 / 16,983 | 13,542,552 |
| openbsd-amd64 | FvwmForm | 1,706,888 | 480,577 / 75,848 / 15,167 | 13,663,432 |
| openbsd-amd64 | FvwmForm-cairo-eval | 1,791,656 | 505,759 / 76,280 / 43,383 | 18,735,912 |
| openbsd-amd64 | FvwmForm-native-eval | 1,777,664 | 500,346 / 76,032 / 44,943 | 13,663,432 |

BSD runtime remains unavailable with the Linux/proc-only fixture. Linux ARM64's
existing process/performance observations retain G5's scope. No fresh runtime,
RSS, idle, wakeup or screenshot claim follows from these module builds.

## Build, installation and maintenance costs

All compared native modules remain C11; the private Cairo path uses the selected
Cairo C/Xlib API. Clay is the pinned optional version 0.14 C header and zlib
license, not a C++/cgo/FFI dependency. Ordinary C, Meson/Ninja, pkg-config and
approved Go build requirements remain. Go/private PNG setup in fresh profiles belongs to the existing project build,
not a dependency newly required by Clay. The canonical PNG bootstrap runs its
36 upstream tests plus version/ABI probes. They are dependency qualification, not
WM/project runtime coverage. Earlier descriptions mentioning only the small
probe were incomplete; the actual bootstrap recipe and receipts remain binding.
An owned 20-second/1 MiB read-only process snapshot identifies AMD64 preparation
as upstream `make check`/`pngimage` work; no WM/module entry point is exercised.
Python is the existing developer harness, not a new installed runtime.

The Cairo experiment directly loads Cairo in a module process. Existing Cairo
through the isolated SVG worker does not make this process cost zero. Retained
pkg-config dependency/private-dependency metadata and package copyright files
record the actual environment; this is not a vulnerability/advisory audit or a
claim that backported packages are secure. No dependencies/defaults are replaced.

Clay source is commit `e6cc36941ab2af5d81107617039d6f527a1c660b`; the retained
303701-byte header and 891-byte license have the exact pins in the
[comparison contract](../decisions/CLAY-LAYOUT-COMPARISON-2026-11-04.md).
The experiment adds a bounded arena and copied numeric adapter while keeping
native text measurement, input/focus, clipping, rendering, action, virtualization
and fallback. Cairo adds scratch surfaces, X resources and error/teardown paths.
Both add comparison fixtures and upgrade work; neither removes production code.
A source-line reduction is therefore not a demonstrated maintainability benefit.

Six private module outputs are absent from the collected install maps. Their
actual product installation increment is zero **by design**. This is neither a
fresh install test nor a zero-cost claim for development dependencies,
hypothetical adoption, library loading or retained source. The ordinary two
modules retain their existing install entries.

## Failures, reuse and remaining acceptance

FreeBSD ARM64's first warm attempt fails at Meson configuration with
`Unknown option: "graphics-eval"`, before any module compile or test. Source
hashes/modes were verified. The failure and logs are retained; normal shutdown completes with parent
unchanged. After the corrected build passes, only its superseded failure image
retires. The bounded installed-source diagnostic confirms Meson 1.10.2 validates cached
CLI values before loading current project options. Both new options were absent;
the source/build paths and current options hash match. One refresh keeps the
original flags except those two new values, verifies the newly loaded disabled
options and current paths, then uses the original full configuration. The corrected
ARM build and AMD first attempt pass without wiping or deleting build caches.
The first diagnostic dispatch is rejected before guest start because the host
has two active guests, one belonging to another project; it remains untouched.
The same single native diagnostic subsequently passes when capacity is available;
normal shutdown completes with parent unchanged.

Linux AMD64 compiles the eight selected modules in 191.4 seconds, reusing its
incremental profile. These command timings and cumulative child peak-RSS
observations are not a matched cold-build cost comparison. Shutdown's Linux
`-p` argument was invalid; the recorded bounded force-stop fallback reaps QEMU,
with parent unchanged. Build/metadata results remain scoped and no runtime claim
is attached. Linux ARM64 collects missing metadata from unchanged outputs and
stops the owning container/machine without another build.

The frozen criteria retain these distinctions:

| Criterion | Result and remaining boundary |
| --- | --- |
| Finite drawing/layout correctness, interaction, lifetime and three themes | G5 Linux ARM64 passes for its named cases; shared clipping/contrast and mixed-scale/accessibility limits remain. |
| Two production native layout consumers | Accepted panel and browser evidence retained; this comparison does not complete broader consumer qualification. |
| Repeated same-workload performance | G5 scope retained, no speedup; all-platform runtime comparison and complete cold-build costs remain unavailable. |
| Direct/transitive/build/install costs | Actual target rows and gaps above; file metadata is not runtime cost or full install/release qualification. |
| Complete implicit synchronous roundtrips | Unqualified and pending. Issued-request counters cannot substitute; no gate is waived by non-adoption. |
| Adopt/defer/reject | Two scoped R1 rejection decisions; no production integration or source deletion follows. |
| Issue and milestone closure | Pending frozen acceptance, including broader native-consumer/support evidence; #211/#212 stay open. |

Periodic whole-WM ledger remains 4/5 after PR356. G6's cost/decision documentation
does not reset it or justify another whole-WM run. Full install/default-server,
physical hardware/toolkits/compositors and release gates remain separate. Daybreak
#257/#258 remain parked. No screenshots are needed for G6's unchanged UI.

OpenBSD ARM64's first evidence helper fails before configure because its 64 MiB
trusted-tool identity cap rejects the 125,362,240-byte base compiler. The original
failure and normal QEMU exit 0 are retained. One reviewed correction records actual
tool stat and streams hashes of existing regular build tools up to 256 MiB; other
file/output/build limits remain unchanged. Corrected ARM and AMD first-attempt
module builds pass. These OS compiler bytes are an existing build-tool cost,
not a new WM runtime dependency. Both guests shut down cleanly.

Linux package-owner alias/private-PNG queries preserve their failed chunks;
canonical owner queries and copyright files remain available. FreeBSD's aggregate
pkg-format query and private-PNG ownership queries fail; per-owner package info,
versions and license fields are retained where available. FreeBSD loader identity
is not collected, so its complete loader-inclusive totals are unavailable.
OpenBSD retains actual base/Xenocara paths and matching package packing lists and
descriptions; exact license and nominal installed-package sizes may be unavailable.
These gaps are not silently zero or a complete dependency/license qualification.
All eight ELF and per-profile pkg-config costs remain source-bound in the JSON.

Only the two superseded, stopped ARM64 failure images retire after their
replacement builds pass, releasing 3,812,622,336 allocated bytes. Compact failure
and diagnostic logs, all latest builds, parent disks and dependency/SDK caches
remain; retention.json records the exact paths and admission. Other projects
are untouched. A retired image is not an available reproducer.

The latest OpenBSD build snapshots remain for inspection/extraction, but the
canonical runner removes ephemeral SSH keys; automatic warm-SSH reuse is not
qualified. A separate 543,806-byte AMD64 PNG cache retains the qualified prefix,
source/compiler/library receipt and raw 36-test suite log. Its SHA-256 is
`3f93238145cd407a38d6dc0137312b375f27d72a0935fa4d0ca8400ac16121c9`.
The read-only export uses 180 seconds/32 MiB archive and 64 MiB expanded bounds and
contains no credentials. A future same-profile reuse must verify platform,
compiler/source/library identities and loader results; retaining it does not
skip those checks or change the canonical bootstrap policy.

## Integration checks

Applicable source/link/whitespace checks and separate integrated/exact signed-head
review are required before delivery. Canonical signature/tree/parent and Cam
actual-author DCO must verify under standing R1 merge authority. Agent roles were
available; actual backend model routing and any workflow cost savings remain
unverified. No hosted checks/status publishers or unrelated lab resources changed.
