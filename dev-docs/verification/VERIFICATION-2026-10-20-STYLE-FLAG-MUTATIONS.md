# Style flag mutation checkpoint, 2026-10-20

The three-operation combined checkpoint is complete; required broader gaps
remain explicit below. STYLE-01 #38 / CLEAN-01 #34 remain broad open owners.
The preceding [read-only predicates](VERIFICATION-2026-10-19-STYLE-FLAGS.md)
are accepted in [PR #126](https://github.com/gonz0w/fvwm-x/pull/126).

## Three related operations and ownership

- Union: nine simplification calls now accept typed complete style flag records.
  They combine interference, explicit/default summaries and cleanup masks with
  the same operands and ordering as the former generic byte helper.
- Intersection: selected-field cleanup derives its separate local mask through
  a typed operation before releasing the same owned fields.
- Clearing: three explicit/default/change-mask calls and default normalization
  use one typed operation. Clearing still follows field cleanup; normalization
  still removes explicit bits from defaults at its original call boundary.

Private fvwm/style_flags_mutate.h implements these operations with bounded
uint64_t memcpy loads/stores and a byte tail based on sizeof the actual record.
It assumes no word alignment or pointer-cast aliasing. The destination may be
one or both inputs or a disjoint complete record; shifted partial overlap is
outside this private contract. All thirteen former calls use complete disjoint
or exactly aliased records. Their unused integer return reductions and generic
length/casts are removed. The separate normalization call has the same bits.

Configuration names/arguments, record layouts, module protocols, resource
ownership, default-summary expressions, list/index lifetime, merge decisions
and command/X/error/update/grab barriers retain their existing behavior.
No allocation, global state, callback or dependency is introduced. This does
not repair or otherwise change the independently noted default-summary policy.

## Focused tests and measurements

The new independent pure unit retains the original plain-char byte expressions.
Each execution checks 998,430 cases: every byte-value pair, every bit position,
mixed representations and all five partitions of destination/first/second
records. It compares untouched inputs and representation padding as well as
results. Three-page mmap guards exercise both record edges; non-destination
pages are read-only. Typed objects retain their required alignment.

Ten focused Linux ARM64 GCC entries pass, including the existing core style,
command ownership, update, index-maintenance and retention probes. Existing
core fixtures exercise both merge directions, subset retirement, selected-field
cleanup and precedence against independent linear traversal/witnesses.

The paired same-binary Linux ARM64 GCC operation probe alternates three samples
for each of three operations and five alias partitions, retaining calls through
volatile function pointers. Selected word/byte median ratios are 0.112–0.190.
Those opaque-call guest measurements do not establish whole-WM performance or
physical native-CPU latency. Existing inlined call sites can already optimize.
The captured production style object changes from 36,972 to 36,940 allocated
executable bytes. The pre-change production source and WM hashes match PR126's canonical qualification archive. No universal
latency claim is made from either observation.

## Combined checkpoint and source binding

Three typed flag union/intersection/clearing operations pass one combined
checkpoint: 2045/2045 standard results (2035 fresh / ten retained by exact
source and twelve binary identities), plus 451/451 optional entries across six
targets/ten profiles. Eleven mutation drivers verify 10,982,730 reference
cases; fifteen actual-core drivers retain independent
style-order/simplification coverage. Four scoped guest sanitizers, nine fresh
full stages and four authenticated BSD displays pass. Three required OpenBSD
AMD64 omissions and its full-install/two staged-geometry gap remain without
retries.

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

Across the ten profiles, all 2,489 preceding registrations preserve their order;
style-flags-mutate is added exactly once per profile. The resulting 2,499
registrations include three required OpenBSD AMD64 omissions. All 2,496
executed standard/optional results pass. Ten focused entries are retained
only after exact source/modes, twelve binary hashes and six core evidence
sets match; their rows are distinct from fresh results. Publication repeats
no matrix or focused suite.

Eleven mutation drivers execute 10,982,730 cases. The ten unchanged
read-only policy drivers retain 13,513,390 comparisons. Fifteen core
style drivers retain 180 independent ordered-merge cases,
17,310 wrapper cases and 1,920 parsed
conditions, with broader simplification/ownership/update checks unchanged.
Four scoped guest ASan/UBSan entries pass: pure mutations with leak detection,
and existing EWMH state/style-profile/window-update core fixtures under their
scoped leak policy. Native-CPU sanitizer acceptance remains separate.

Nine fresh full stages verify 135 code and
88 catalog identities with normal rpath handling.
Four authenticated BSD displays pass 38 fixture invocations,
clean server exits and unauthenticated-client refusals. Staged geometry passes
18 invocations / 126 cases; real-client
style consumers remain covered. Meson prerequisite builds remain enabled for
tests; installation uses --no-rebuild after successful checks. Portable make
verify runs once: 166 passes and eleven native-Meson skips. All eleven compile
logs remain; no warning falls in a changed C/header line.

Accepted Git base: 434b54b52afc5d8db1db61e05d245aa56a12ee12.
Runtime commit: 58cc88521c8e7b2c250951dd0fb18178dd28bdcb.
Runtime tree: 5720a13285ca6dbfb08680f398820cab20a7820c.
Frozen archive: 2fa4951eaf8ccd379d2551aa8d64b8adca1283c9a520f5405ad12080054a95ee, 2049 regular inputs.
The archive was prepared while PR126's merge was pending. Its preparation base
is b41e59e2d8a8b31556e862281955bfef7c0f4348; the accepted Git base has the identical tree. Every frozen input
and canonical mode matches the signed runtime tree. Final changes are only
documentation/receipts. The final PR records its signed/DCO head and tree.
The [machine receipt](VERIFICATION-2026-10-20-STYLE-FLAG-MUTATIONS-COMBINED.json) binds input, runner, capture,
registration, command, binary, stage and retained-result identities.

## Required gaps and storage

OpenBSD AMD64 executes 247 passing entries. Three known required omissions
remain: CI asset worker timeout, SVG/MFL startup refusal and PR120 legacy
property startup refusal/single unchanged control. Their original identifiers
remain in the receipt. No retries, deadline changes or cause claims are added.
The full install and two staged-geometry invocations remain unqualified there;
eight authenticated default-X built-client fixtures pass. Earlier reports
retain their original source-scoped evidence.

The owner reserve preflight initially failed below 1 GiB. All 550 regular
members and the full gzip footer/CRC of PR126's preserved canonical host archive
were verified before removing 357,888,533 duplicate
guest capture/archive bytes. Source, builds, stages, VM disks and canonical
host evidence remain. Free space returned above the unchanged reserve; source
uploads use owner storage to preserve Linux AMD64's small /tmp margin.

Native-CPU sanitizer, physical GPU/input/mixed-monitor, toolkit/compositor
and release acceptance remain separate. VESA #93 stays deferred; Actions
remain disabled. The review QEMU/VNC remains closed. Broader STYLE/CLEAN,
plans/grabs and other tracker owners remain open.
