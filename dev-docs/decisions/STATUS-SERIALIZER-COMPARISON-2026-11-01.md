# Status serializer comparison

Owner: Cam; LIB-06 #48. **R1 decision: retain patched cJSON and defer yyjson
integration.** The decision is based on the bounded comparison below; all six
OS/architecture comparison observations are validated, with execution profiles and limits in the target report. #48 closure remains subject to
separate final review and integration. This decision changes no production
dependency, serializer, parser or installed behavior.

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

The frozen yyjson writer uses lowercase control escapes and nonfinite-to-null;
its default invalid-UTF-8 refusal is a measured candidate difference. Source
inspection also identifies an unmeasured option combination:
`ALLOW_INVALID_UNICODE`, `ESCAPE_UNICODE` and `LOWERCASE_HEX` can replace each
invalid input byte while also escaping valid Unicode. The current patched
baseline leaves valid UTF-8 literal. This alternative is not established by the
frozen measurements, and it is not evidence that a custom normalizer would be
necessary. A later integration may deliberately change module wire encoding
under the project's configuration compatibility scope, with consumers, docs
and behavior qualified together. It must measure and verify that option or any
adapter on the real inputs and account for its code, copies and time.
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

## Decision and measured scope

The retained [source observation](../verification/VERIFICATION-2026-11-01-LIB06-SOURCE.md)
and the [target comparison](../verification/VERIFICATION-2026-11-01-LIB06-TARGETS.md)
separate source-bound observations, failures, profile results and limits. The
candidate's native-allocator loops were faster in the observed synthetic cases;
the copied-string mode had a higher tracked requested-memory peak in the
ordinary, large-string and nested-record cases. Candidate probe code size was
also larger in the observed profiles. These are useful comparison results, not
a measured end-to-end FVWM-X speedup or proof that JSON is a production
bottleneck. Tracked peaks exclude allocator overhead and input workspace, and
whole-process RSS has different limits. Borrowing can reduce some synthetic
peaks but adds an owner-lifetime requirement that these probes do not establish
for asynchronous production callers.

An isolated run of the unchanged MFL binary compared actual window-name output
with independent expected bytes and the baseline probe. Ordinary, valid Unicode
and legacy-byte cases matched; 256 separate records retained their expected
output, and an unterminated packet was refused before a valid sentinel. Its
private module process and socket were retired. This was a synthetic packet
fixture, without a real core or X server; its 256 separate records are not the
probe's 256-record array wire format. The retained source-bound status builder
unit and status-stream results are distinct earlier component evidence, not a
fresh full-core qualification.

The current MFL also parses request, settings and result JSON. Replacing only
its writer would therefore keep cJSON and add a second implementation, its
ownership/failure paths and an integration to maintain. The comparison does not
show enough production benefit or code simplification to justify that addition
for R1. No candidate production adapter was built, so its actual integration
code size, installed bytes and performance are unavailable. The decision removes
no production code or dependency; the incremental installed product size from
this evaluation is zero.

### Dependency and maintenance costs

The pinned yyjson source is 424,765 bytes of C and 330,896 bytes of headers; its
MIT notice is 1,084 bytes. The input manifest identifies the exact source, API
reference and notice hashes. The prototype directly compiles C and uses the
platform C library and math library where needed; it adds no C++, FFI or Go
dependency. Per-profile dynamic dependencies and binary sections are recorded
in the target comparison. Probe executable sizes include the harness and debug
information and cannot be used as installed product deltas.

An adopted writer would add direct source, license-notice, upgrade, security
maintenance and integration ownership while the current parser remains. A
reproducible offline build would need a verified source archive and notice, with
build and package integration reviewed for every supported profile. Package
availability and backports across all targets were not established by these
prototype builds. A library already installed for another component would not
eliminate per-process linkage and maintenance costs.

### Profile and acceptance limits

The four BSD profiles and Linux ARM64 have completed source-bound capture and
host-oracle validation. Linux AMD64 is validated on Debian 13.7 under TCG. ARM
guests use the existing Apple hypervisor environment; AMD64 BSD profiles use
QEMU TCG. These are guest observations, not native-hardware qualification.
The Linux AMD64 comparison uses the existing Debian 13 TCG profile; it cannot
qualify Ubuntu 24.04 or native hardware. The target report records the actual
OS, compiler, measurements and execution mode for each completed profile.

This pure-library comparison does not qualify the full window manager, install,
default X server, themes or native Linux sanitizers. Those checks remain with
their existing milestone or release owners. The first FreeBSD ARM evidence
export failed and supplied no qualified result; its fresh captured continuation
is reported separately in the target evidence. A deferral is a decision to
retain the current implementation, not evidence of candidate integration.

### Finite later follow-up

Reconsider a candidate only if a measured production hot path or a concrete
reduction in maintained complexity justifies it. Compare full request or
response work and integration costs, then qualify valid and invalid keys and
values, duplicate keys, supplementary characters, output expansion and cap
crossings, ownership and failure/refusal paths. Include shipped consumers,
license/build metadata, offline inputs and affected supported profiles in any
adoption plan. This follow-up is not an additional R1 prerequisite.
