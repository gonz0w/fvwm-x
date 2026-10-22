# Legacy strut and root work-area safety — 2026-10-21

Owners: EWMH-01 #18 / DATA-01 #6 / INPUT-01 #9; R11/W03/W06 in the
[requirement register](STANDARDS-2026-10-21-PROTOCOL-REQUIREMENTS.md).
Status: **locally qualified composed checkpoint; PR147 ready for maintainer
review**. [Machine receipt](VERIFICATION-2026-10-21-WORKAREA.json).

## Repairs and contract

Three related repairs validate exactly four CARDINAL/32 root-edge distances
before signed storage, release both reservation records when the property is
absent, and publish current dynamic reservations in the bounded per-desktop
root snapshot. Malformed replacements retain accepted records. An all-zero
value retains the static reservation used by auto-hiding panels while releasing
the dynamic one. A typed acquisition result distinguishes malformed data from
absence, including a queued delete superseded by a new current property.

Publication reuses the existing buffer capped at 1024 desktop records/server
request capacity. One window walk collects desk-local and shared reservations;
one bounded desktop walk builds contained rectangles. Opposing reservations
from different clients can overlap, leaving a contained empty area. No new XSync,
callback/event dispatch, configuration restriction or retained window pointer is
introduced. Existing per-monitor internal work-area policy remains unchanged.
Mutation, deletion, base/style/desk/sticky transitions and destruction refresh
root state through the existing single-threaded owners.

This adopts the legacy full-edge subset of EWMH 1.5 sections
[3.9](https://specifications.freedesktop.org/wm/1.5/ar01s03.html) and
[5.9](https://specifications.freedesktop.org/wm/1.5/ar01s05.html).
Root coordinates remain actual pixels relative to the viewport; page movement
does not add virtual-page offsets. No partial-strut capability is advertised.

## Composed qualification

Base is merged PR146, `0165d40e51f001091776ea3fc776983bc147e3ad`. Initial implementation `4c63c0f4c`
uses source archive `4c46686cda9764a2968f583f6da770179588aafeba0fa0d7922b08d7245995f2`.
The final sticky-page correction `96b5d18d271cfcf9f6fedf856e00bcd8c6a035b5` uses
`2683d01c7955e71d85a07a3cdeadb08959bd40b975aad353c35d845e772d9e0b`. Both scopes contain
2133 tracked regular inputs. Final C/header, Meson, fixture and
AsciiDoc inputs match the final tested source exactly; remaining upkeep is
Markdown and this standalone receipt.

One broad run passes **2372 standard / 30 selected optional
entries** on Linux GCC/Clang AMD64/ARM64 and FreeBSD/OpenBSD 8.0 AMD64/ARM64.
Linux ARM64 software/minimal profiles are selected affected coverage, not another
full feature matrix. Ten initial installs pass built/staged ELF, catalog/data,
loader/version and staged desktop/work-area/geometry/style checks. Four
authenticated BSD default servers pass 56 fixture groups,
refuse unauthenticated clients and stop with exit zero.

Review independently reproduced stale root state after `StickAcrossPages off`.
The old guard only refreshed reservations while the window remained sticky.
The correction removes that guard and adds independent off/on and
all-desktop/local-desktop assertions. It changes only `fvwm/ewmh.c` and
`tests/live_x/workarea_live.py` from the initial source. The original broad
results remain bound to that source. **90 affected continuation
entries** qualify the correction on all six targets/ten profiles without
repeating unrelated suites. Ten final installs verify 150 ELF
identities, 77 catalogs and 700 data outputs. Final staged controls
include 30 work-area groups, 30 desktop
groups, 20 geometry groups at 1x/1.5x and
10 style groups. Four final authenticated BSD displays pass
32 affected/default groups and stop with exit zero.
OpenBSD correction verification runs on /home through conventional symlinks;
the collector receives a node/hash/mode-identical materialized capture.

Focused development passes five entries on Linux and FreeBSD ARM64 after the
final builder correction, including actual-core static/dynamic/missing ownership.
Four scoped guest ASan/UBSan entries pass initially and again for the final
correction; fatal/leak diagnostics have distinct exit codes. Final sanitizer
binary hashes remain exact after qualification. The preserved PR146 installed
core (SHA256 `67c05fbdcdf8f8e425861ed9153a0809c9af712161d17c8dac9e3384a71cb8a3`)
fails the new root-property checks, establishing the preceding visible gap;
this is not an exploit claim or three independent safety-crash reproductions.

Portable `make verify` runs once: 191 registered,
177 pass / 14 capability skips. The same
26 generated man pages are hash-verified; the two affected style/combined pages
render with native FreeBSD mandoc. Final runtime changes do not alter AsciiDoc.
Local diff/ownership review, repository/link/whitespace checks and signed
contributor/DCO commits complete the handoff. No hosted checks or status
publishers are used.

## Preserved failures and storage

The first development runner selected an unregistered test, so no tests ran.
Its copied previous testlog is explicitly unexecuted. Corrected selection passes.
A subsequent linear-builder edit passed the anonymous BaseStrut record to an
ewmh_strut helper; both compilers rejected it and no tests ran. Explicit field
initialization fixes that mismatch. Original failure archives and affected
completions remain canonical.

Linux ARM64's broad runner initially refused its 500 MiB disk preflight before
source adoption or qualification. The source postcheck then reported the
expected unadopted Markdown difference. Both traces are retained in
`linux-arm64-preflight-failure.tgz`; no product result is counted from this
attempt. Every node/mode/hash of two completed 2026-10-15 guest capture copies
was verified against their preserved canonical host archives before retiring
303865602 duplicate bytes. The unchanged candidate then ran its previously
unstarted gate. The original collector reached its collection deadline while
that resumed run was still working; its error is retained and a separate
collector retrieves the completed result without rerunning tests.
No source tree, build, install stage, performance reference, VM
disk or other owner's work was removed. Current completed guest captures and
temporary cache copies are likewise retired only after canonical validation.

Canonical evidence is under `/Users/cam/DEV/fvwm3/builddir/codex-2026-10-21/workarea-safety`. Receipts name each archive/hash, full
payload verification, actual profile/command rows, staged identities,
unauthenticated refusal, QEMU exit status and ephemeral-key cleanup. Broad and
affected correction evidence remain separate; there was no second full matrix.

## Remaining scope

Partial struts, nonsticky-page panels, full topology and independent multi-monitor
root authority remain open. These root-property results do not qualify every
maximize/placement clause or all existing internal monitor geometry. Property
writes retain request order without a multi-property atomic transaction. The
broad EWMH/DATA/INPUT owners stay open. Native-CPU Linux sanitizers, physical
hardware/toolkits/compositors and final release acceptance remain separate.
PR144 remains the performance reference at 2/5 subsequent merged implementation
checkpoints; this unmerged PR does not count. No comparison or tuning is due.
