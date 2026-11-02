# Directory backend comparison and R1 disposition

Owner Cam; FILE-DATA-01 #66. Source is merged PR340
`649bf8591163324942737a839bddd84aadb675a2`, tree
`c697e42bcff495cffdcf2280a28471bf53423774`, plus the unchanged native
supervisor source pinned in the [machine receipt](VERIFICATION-2026-11-02-DIRECTORY-BACKEND-COMPARISON.json).
No product source changed during qualification. Separate integrated data and
contract review passed; signed delivery retains its own identity checks.

## Result

Retain the existing database-independent Go directory pipeline for R1. Defer
SQLite/FTS5 integration and do not introduce the evaluation-only JSON cache into
the product. No storage schema, daemon, migration or new dependency is installed.
The [decision](../decisions/DIRECTORY-BACKEND-EVALUATION-2026-11-02.md) records
why and the boundary of any future pickup.

One combined checkpoint passes **42 native oracle cases, 64 verification
invocations and 192 matched cost invocations**, with six snapshot-control rows
and 36 SQLite-control rows. Four profiles provide SQLite/FTS5. Both frozen
FreeBSD profiles lack its headers/library/pkg-config; their SQLite result is
**unavailable**, independently of passing the compiled-disabled refusal.

Existing directory/provider/watch component acceptance remains source-scoped in
[foundations](VERIFICATION-2026-10-23-FILE-BROWSER-FOUNDATIONS.md),
[watching](VERIFICATION-2026-10-23-BROWSER-DIRECTORY-WATCH.md) and the
[provider cycle](VERIFICATION-2026-10-23-BROWSER-PROVIDER-CYCLE.md).
This comparison does not rerun or broaden those filesystem/UI/install results.

## Frozen workload and profiles

The manually tagged, non-installed [probe](../../bin/FvwmPrompt/cmd/fvwm-metadata-eval/README.md)
reuses actual filemodel/collection normalization, sorting, filtering and
reconciliation. Four deterministic public corpora have 0, 12, 256 and 4096
entries; 4097 admission, raw invalid UTF-8/control names, duplicate labels,
distinct IDs, metadata extremes and generation changes have independent oracles.
No directory or file content is scanned. Each process uses three fixed query
batches; each cost case has three fresh process repetitions with rotated order.
Every cost digest matches a successful verification and cross-backend reference.

| Target | Actual execution/profile | SQLite |
| --- | --- | --- |
| Linux ARM64 | Ubuntu 24.04.5 userspace; Fedora 6.18.10 kernel in AppleHV VM/container; GCC 13.3; Go 1.27.2 | 3.45.1 Ubuntu `alt1` runtime; authentic private same-version header |
| Linux AMD64 | Debian 13.7, QEMU TCG emulation; GCC and Go 1.27.2 pinned in receipt | 3.46.1 Debian `alt1` runtime; authentic private same-version header |
| FreeBSD ARM64 | FreeBSD 15.1, QEMU HVF; clang 19.1.7; Go 1.27.2 | Unavailable in frozen stock profile |
| FreeBSD AMD64 | FreeBSD 15.1, QEMU TCG; clang 19.1.7; Go 1.27.2 | Unavailable in frozen stock profile |
| OpenBSD ARM64 | OpenBSD 8.0, QEMU HVF; clang 22.1.6; Go 1.27.2 | Stock 3.53.2 header/runtime; FTS5 observed |
| OpenBSD AMD64 | OpenBSD 8.0, QEMU TCG; clang 22.1.6; Go 1.27.2 | Stock 3.53.2 header/runtime; FTS5 observed |

These are guest execution results. TCG results are emulated, and no physical
hardware, complete installed consumer or native Linux sanitizer qualification is
claimed. The receipt binds all Go/module/helper sources, SDK archives/binaries,
compiler and header/library/source IDs, features, executable sizes and hashes.
Private Linux headers are byte-identical to authentic upstream archives; the C
amalgamations match the published [3.45.1](https://www.sqlite.org/releaselog/3_45_1.html)
and [3.46.1](https://www.sqlite.org/releaselog/3_46_1.html) SHA3 records. Header
and distribution `alt1` source IDs remain separately recorded; this is not a
patch/vulnerability assessment. Only private build links were created.

## Repeated measured costs

The table uses 4096 entries. Wall time includes the supervised process invocation;
query time is the sum of its three literal-query batches. Brackets show the
three-process wall range. Compare backends within a target, not OS/CPU speeds.
RSS is the raw native high-water value through the last measured phase.

| Target | Backend | Process wall ms, median [range] | Literal batches ms, median | Final-phase raw RSS |
| --- | --- | ---: | ---: | ---: |
| linux-arm64 | memory | 92.39 [91.12–92.87] | 62.94 | 11952 |
| linux-arm64 | snapshot | 121.95 [121.08–122.34] | 65.36 | 14052 |
| linux-arm64 | sqlite | 165.31 [164.68–168.47] | 99.13 | 15092 |
| linux-amd64 | memory | 2581.00 [2425.68–3091.05] | 1732.72 | 13904 |
| linux-amd64 | snapshot | 3763.98 [3701.15–3939.31] | 1741.15 | 16136 |
| linux-amd64 | sqlite | 4765.02 [4580.01–5021.87] | 2862.85 | 18448 |
| freebsd-arm64 | memory | 85.78 [84.27–86.39] | 59.37 | 12664 |
| freebsd-arm64 | snapshot | 112.27 [110.54–112.86] | 59.91 | 14328 |
| freebsd-amd64 | memory | 4473.10 [4068.29–5503.33] | 2830.07 | 13204 |
| freebsd-amd64 | snapshot | 5214.50 [4771.03–7783.44] | 2515.25 | 14300 |
| openbsd-arm64 | memory | 118.60 [115.80–121.57] | 61.61 | 12044 |
| openbsd-arm64 | snapshot | 151.23 [140.55–161.45] | 65.08 | 14656 |
| openbsd-arm64 | sqlite | 238.39 [236.96–240.25] | 132.01 | 15592 |
| openbsd-amd64 | memory | 2259.30 [2238.12–2278.73] | 1499.54 | 13228 |
| openbsd-amd64 | snapshot | 3015.03 [2975.50–3079.64] | 1510.52 | 15084 |
| openbsd-amd64 | sqlite | 5073.94 [5060.40–5137.84] | 3143.61 | 17576 |

All four available SQLite profiles use more observed memory and take longer on
the current literal-query workload than the in-memory pipeline. FTS token-prefix
queries have measured wins, but use different semantics and add index build,
file, binding and lifecycle costs. They do not establish a browser speedup.
The JSON cache adds codec/publication work to a live model that already works
without persistent storage. Full 0/12/256/4096 phase CPU, Go allocations,
provider counters, file bytes and repeated ranges are in the JSON receipt.

The native supervisor forks from a small C image before Go exec. This removes
Python's inherited RSS floor; the original Linux readings remain preserved and
are unsuitable for backend footprint comparison. Do not subtract a floor or
interpret Go allocation counters as total memory. FreeBSD short invocations can
report zero RSS; these are marked unavailable, not zero-footprint results.
OpenBSD CPU accounting is coarse, often zero or 10,000 microseconds.

Empty-corpus process wall is a startup-inclusive workload observation. It does
not isolate cold loader/startup time. Backend initialization/load is separately
reported. SQLite reopen checks schema/generation; JSON reopen decodes/rebuilds
the model, with subsequent view observation outside that phase. Do not present
those phase differences as an equal full-restart speedup. SQLite FULL/DELETE
and rename-based snapshots also have different durability work; neither result
qualifies power-loss recovery. No background service exists in this experiment,
so production idle/wakeup work is not applicable. It measures complete bounded
ID projections, not pagination, rendering or product round trips.

## Correctness, simplicity and dependency costs

Native independent goldens cover eight sort orders, literal and token IDs,
complete numeric/raw-byte metadata after reopen, updates, stale generations and
admission limits. Actual SQLite controls cover busy, explicit read-only open,
bounded full, partial replacement/full rollback, uncommitted close, committed
reopen, corrupt, truncated and wrong-schema files. Snapshot invalid/rebuild
controls are separate. Page limits prevent deliberate filesystem exhaustion.
No expectation was weakened to hide a failing product result.

The existing Go pipeline remains necessary for byte-preserving identity,
normalization, selection and optional-provider fallback. SQLite removes **zero
production lines** in this scope, while its prototype adds 611 lines of binding,
schema/query/readback code, 332 control lines and 119 SQLite-specific test lines.
Adoption would also own cgo/target headers, connection/transaction cleanup,
schema/invalidation/recovery and package/upgrade work. No new Go driver/module,
C++ stack or GUI toolkit is used.

The receipt lists complete observed required executable/loaded-file closure,
including base libraries already installed; their per-process loading is not
free. OpenBSD's CGO-disabled baseline itself loads libc/pthread/loader files;
SQLite also loads its engine, math and zlib libraries. SDK compressed and expanded
regular-file bytes, source/license hashes and actual header/library/compiler
identities are retained. These are file costs, not resident memory or all files
in a distribution package. Complete package payload sizes were not measured.
The tagged prototype adds **zero normal installed files/bytes and zero mandatory
dependencies**. A hypothetical adopted backend's package/install qualification
is not claimed. The project GPL, approved Go SDK license and SQLite public-domain
header notices remain; base-library package notices/update ownership are unchanged.
All builds used frozen offline inputs; no package, loader or system-link change.

## Original failures, cleanup and limits

- First FreeBSD ARM run: preparation and five oracles passed, then a shell-driver
  guard clobbered its caller's expected value. Preserve original run1/QEMU0 and
  logs. The corrected guard uses subshell scope and distinct step variables.
  Product source and expectations are identical; all three old native binary
  hashes match the final passing checkpoint.
- Original Linux ARM cost launch: correctness/digests passed, but Python imposed
  an RSS floor. Reuse exact binaries and retained oracles/controls; repeat only
  the 36 cost samples with the unchanged C supervisor.
- First Debian placement: the 986 MiB `/tmp` tmpfs refused the 1 GiB preflight
  before SDK copying or compilation. The successful job used existing `/root`
  filesystem space (about 31 GiB free) with the same bound; no disk expansion.

All exports are hash-verified; owned containers/machines/QEMU processes are
confirmed stopped or reaped. Twelve superseded discovery/failed-attempt image
files were retired after archive/hash/duplicate checks. Retired allocated bytes:
21,090,004,992; observed host free-space increase: 20,480,000,000 bytes. Preserve
all compact original failures, current builds, reference binaries, SDKs and
canonical caches. Private receipts/scripts are under
`builddir/codex-2026-11-02/r1-directory-backend-comparison`.

No new UI: theme checks/screenshots are not applicable. Required current release,
physical, full install, native sanitizer and consumer gates retain their owners.
The [due core observation](PERFORMANCE-2026-11-02-PROPERTY-CORE.md) is separate
from these public-metadata timings.
