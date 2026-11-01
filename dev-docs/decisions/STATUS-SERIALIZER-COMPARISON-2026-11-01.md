# Status serializer comparison

Owner: Cam; LIB-06 #48. Status: bounded comparison source in development;
**no serializer adoption decision**. The tracker owns execution and acceptance.
The current patched cJSON remains the production dependency and request parser.

## Frozen inputs and scope

[Input manifest](../verification/LIB06-2026-11-01-INPUTS.json) binds current cJSON
1.7.19 plus project print/UTF-8 patches, current producer/transport sources and
[yyjson 0.13.0](https://github.com/ibireme/yyjson/tree/6447536015f3d600f3d65323b10976103b337ca7).
Its annotated tag resolves to that exact commit. Unmodified C/header, MIT notice
and API reference are retained in the owned private comparison cache with
SHA-256 identities. They are not installed or added to production linkage.
This freezes a candidate, not a vulnerability or dependency-adoption verdict.

The October 7 baseline report's no-production-parser observation is historical.
Current MFL parses request/settings/result JSON in `request.c` and `FvwmMFL.c`.
A status-writer result cannot establish cJSON retirement. Keep the actual parser
and other production writers until their own changed integration is qualified.

The smallest comparison is a non-installed probe, built from the same source
against either actual patched cJSON or the exact external yyjson source. It
constructs native documents directly; it does not convert a cJSON DOM into a
second DOM or introduce a production compatibility layer. Separate candidate
modes copy strings or borrow caller-retained synthetic strings. Both use the
same field order and inputs. Borrowed strings stay valid through writing and
document disposal; they are not evidence that real asynchronous owners permit
borrowing. A separate deliberate input-mutation control establishes copied
independence and borrowed observation without freeing either input.

## Finite corpus and oracle

| Case | Observable |
| --- | --- |
| Ordinary MFL-shaped window-name record | Nested object order, literal window identity, controls/quotes/backslash and valid UTF-8. |
| Unicode and legacy value | Valid multibyte text; per-invalid-byte replacement versus candidate refusal. |
| Legacy keys | Two different invalid keys both become `\ufffd`; preserve duplicate keys and their order in the oracle. |
| Numbers | Signed limits, zero, fraction, finite double extremes, nonfinite-to-null and exact trusted raw `UINT64_MAX`. |
| Large/stress | A 64 KiB string and 256 nested records with 256-byte names; count full construction/write/disposal. |
| Queue/status boundary | Encoded length one below or exactly at 256 KiB/1 MiB; include newline and an MFL queue already holding 128 bytes. |

The Python oracle uses strict UTF-8, rejects nonstandard NaN/Infinity tokens and
preserves ordered object pairs. Semantic numeric outcomes and exact integer
identity are checked separately from lexical exponent/negative-zero forms.
Per-invalid-byte escaping is independently fixed for the baseline. Candidate
byte differences and explicit refusal remain observations, never silently
normalized byte-parity passes. A successful allocation-failure trial must still
produce the whole expected document. A partial object is a failure.

Transport results in this probe are **empty/partly occupied queue models** from
the pinned production constants, not calls into either actual transport. Neither
serializer applies those caps before construction. Larger construction/output
allocations must remain visible even when the eventual transport would refuse.
Current status and MFL limits include one newline; acceptance is strict `< cap`
for the encoded document, plus MFL remaining queue room.

## Allocation, ownership and timing

Tracked trials use aligned headers, checked requested-byte arithmetic, explicit
budgets and per-ordinal allocation failures. First observe uninterrupted call
count, then inject at every observed position in each small ordinary/legacy/
numeric record, even if an earlier injection succeeds. Refusal or complete valid
output must leave no tracked allocation. Small budgets exercise early refusal.
The instrumented resize deliberately counts simultaneous old/new requested
storage and copying. Requested-byte high-water excludes tracking headers,
allocator metadata/rounding, stack and the separately reported fixed input
workspace; it is not resident memory or a complete allocator footprint.

Custom cJSON hooks disable its ordinary realloc fast path. Therefore tracked
CPU/elapsed fields are diagnostic only. Repeated native-allocator trials use no
tracking/failure hooks and report allocation counters as unavailable. They time
construction, writing and disposal of ordinary/large/stress fixtures, alternating
backend order between repetitions. Each native child also reports whole-process
`getrusage` peak RSS in raw platform units, including loader/input overhead;
normalize units using that profile's OS contract. The runner separately records
launch-to-exit wall time, including process startup and orchestration, rather
than mislabeling the in-process loop as startup latency.

The pinned yyjson writer uses lowercase control escapes and nonfinite-to-null;
default invalid-UTF-8 refusal is intentionally retained as a candidate difference.
No candidate string-normalization adapter is supplied by this source prototype.
Any later adapter must retain this result and count its own code/copies/time.
The finite probe exits, so persistent-service idle wakeups are not applicable;
production process idle costs remain a separate integration observation.

## Manual build and observation

Work in the repository root with an existing configured build and verified
candidate cache. The baseline target is explicitly selected, non-installed,
excluded from default builds and not registered as an always-passing test:

```sh
meson compile -C "$BUILD" status-serializer-probe
```

Build the candidate with the same compiler, optimization and feature profile,
using `tests/status_serializer_probe.c`, the pinned `src/yyjson.c`, the existing
build's `config.h`, repository includes and candidate `src` includes. Define
`SERIALIZER_YYJSON`; link the platform math library when required. Keep default
reader/writer options recorded; do not silently disable features to reduce size.
The candidate source asserts the frozen yyjson version, while the execution
preflight must verify full hashes, not only the version macro.

```sh
python3 tests/compare_status_serializers.py \
  --baseline "$BASELINE_PROBE" --candidate "$CANDIDATE_PROBE" \
  --output "$NEW_OBSERVATION_DIR" --repetitions 3 --iterations 20
```

The output directory must be new. Exit 0 means the bounded observation/oracle
checks completed; inspect recorded mismatches. It is not an adoption, parity,
production-consumer or supported-profile pass. Preserve first failures. Freeze
compiler/linker/config/options, source/dependency closure and binaries around
execution. No whole WM, install, default-X or theme suite is implied.

## Remaining decision evidence

Synthetic shape equivalence does not establish actual production builder or
independent-client MFL parity. Those remain explicit comparison acceptance.
Before deciding, bind ordinary/stress/refusal behavior to actual current
producers, report complete adapters and parser-retention costs, and compare
like-for-like results across all six supported targets. No library switch or
adopt/defer/reject is inferred from a baseline pass or upstream benchmark.

| Supported target | Current comparison state |
| --- | --- |
| Linux AMD64 | Pending source-bound build/observation and execution-mode identity. |
| Linux ARM64 | Pending source-bound build/observation and execution-mode identity. |
| FreeBSD AMD64 | Pending source-bound build/observation and execution-mode identity. |
| FreeBSD ARM64 | Pending source-bound build/observation and execution-mode identity. |
| OpenBSD AMD64 | Pending source-bound build/observation and execution-mode identity. |
| OpenBSD ARM64 | Pending source-bound build/observation and execution-mode identity. |

Record native/emulated/unavailable execution separately, repeated variability,
CPU/latency/allocation/RSS, encoded and executable/code sizes, actual installed
integration delta, full direct/transitive source/build/runtime/license cost,
offline/bootstrap and maintenance burden. A probe's file size is not an installed
product saving, and a shared library already present in another process is not
free. A decision must explain code/policy removed and adapters/ownership paths
added. Adopt only after its separate integration decision and exact qualification;
defer/reject retains cJSON and names any finite later follow-up. #48 stays open
until its frozen criteria are supported.
