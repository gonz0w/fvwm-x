# Development build reuse — 2026-10-18

MAINT-02 / CLEAN-01, [PR #107](https://github.com/gonz0w/fvwm-x/pull/107).
Base merged PR106 `b23cb8d51e63dca51931287d42f18150ba84ec4f`.
Signed/DCO implementation `ead65d8f2bb2618936d076e16e6291416d60bb50`,
tree `fa700c533f71832ffda5f2f4c913ea9aee3eaa25`.
One coherent three-story maintenance checkpoint; final documentation deltas do
not change the tested build/header inputs.

## Changes and measured work

1. The Linux link-wrap WM shares all 74 production core objects and compiles
   only its two probes. The fault-mode macro is consumed only by the XRR probe;
   linker wrappers and feature/hardening flags remain unchanged. The separate
   assertion-enabled actual-core build keeps `-UNDEBUG`, with 74 core sources
   plus its owning probe; its dependent fixtures still share those objects.
2. `style_match_types.h` holds the unchanged immutable enum/inline metadata.
   The operation subject/API stays in `style_match.h` and is included directly
   by its consumers. Common records need only the metadata; `fvwm.h` now imports
   `stdbool.h` explicitly for its own fields.
3. `style_lookup_index` is opaque in shared records and defined in `style.c`,
   its existing allocation/invalidation/release owner. Its two fields and the
   containing pointer are unchanged. Index layout edits now rebuild the owner's
   two core objects instead of invalidating common record users.

All six Linux profiles now compile 2 wrapper probes instead of 76 sources
(74 duplicate core compilations removed per complete core build). Saved baseline
objects supply 370 pairwise checks of identical allocated code/data sections
between the former duplicate core and production objects. The first GCC development
profile had already adopted reuse before the combined baseline capture; its
separate pre-change four-profile graph receipt records the same original count.

Actual Ninja dependency/dry-run results:

| Target / profile | Operation API dependents, before → after | Private index owner compile tasks |
| --- | --- | --- |
| fvwm-x-linux-arm64-svg/gcc | 214 → 7 | 2 |
| fvwm-x-linux-arm64-svg/clang | 275 → 7 | 2 |
| fvwm-x-linux-arm64-svg/software | 272 → 7 | 2 |
| fvwm-x-linux-arm64-svg/minimal | 270 → 7 | 2 |
| linux-amd64/gcc | 275 → 7 | 2 |
| linux-amd64/clang | 275 → 7 | 2 |
| freebsd-arm64/default | 213 → 7 | 2 |
| freebsd-amd64/default | 213 → 7 | 2 |
| openbsd-arm64/default | 213 → 7 | 2 |
| openbsd-amd64/default | 213 → 7 | 2 |

The immutable metadata header still reaches 209–214 objects, as required by
inline record ownership. Every dry-run restores original source timestamps and
a final Ninja no-op confirms that no rebuild is left pending.

These are compiler/dependency work counts, not an end-to-end latency promise.
Metadata/record-layout changes intentionally retain their broader dependency
fanout. No compiler cache, unity/PCH mode, package, feature/default, configuration
migration or test removal is introduced.

## Affected checkpoint and retained scope

- All ten affected compiler/feature profiles build and pass 70 selected native
  test entries on Linux, FreeBSD and OpenBSD AMD64/ARM64. Linux covers GCC,
  Clang, software-render and minimal feature profiles; each Linux profile runs
  all five existing wrapper regressions plus the four style/core regressions.
  BSD runs the four owning style/core regressions. The focused development run's
  five wrapper passes are recorded separately, not added to that final total.
- All 1502 comparable core/probe objects and all ten WM executables retain
  identical allocated code/data section bytes. Debug sections and GNU build-ID
  notes are excluded from that comparison; complete file hashes are recorded
  separately. This is runtime-section parity, not complete binary identity.
- Ten fresh stages verify 249 installed code/catalog identities against the tested
  build and the expected Meson rpath transform. Each installed WM version probe
  passes. All 2429 test registrations across the ten profiles remain, along with
  the separate 75-source assertion-enabled fixture. Compiler logs contain no affected-file
  diagnostic; existing unrelated warnings are retained in the machine receipt.
- `make verify` runs once: 166 passes and 11 explicit native-Meson capability
  skips on macOS. It is portable foundation evidence, not native runtime proof.
  Self-review, declaration/macro ownership, added C/header 80-column audit,
  whitespace and repository hygiene pass.

The frozen archive contains 1984 regular tracked inputs with normalized Git
modes; SHA256 `bed556976699335e25f9c6363203dc9283576c7de56403accebb1408d410ce8e`.
Each guest checks source bytes/modes before and after execution. The implementation
commit matches every archived input. Final report/status-only changes get a
separate input audit and hygiene check, with no repeated native qualification.

Original receipt collection on Linux ARM/AMD GCC and both ARM BSD profiles
stopped after successful compile/tests/install: the collector treated relative catalog install directories
as absolute. The continuation corrects that path handling and the same collector's
rpath-helper call signature. Original partial archives/logs remain intact;
continuations load the original baseline and skip only exact commands already
recorded successful. No product input, assertion, deadline or test result changes;
successful configure/build/test/install commands are retained rather than rerun.
The continuations also finish profiles not yet reached by the original attempts.
The repair changes only evidence collection.

Broader unchanged runtime/default-X/scoped-sanitizer evidence is retained from
[PR106's subject-work checkpoint](VERIFICATION-2026-10-18-STYLE-SUBJECT.md),
including its original OpenBSD AMD64 menu deadline failure and passing unchanged
isolated control. This maintenance checkpoint does not reclassify that failure
or rerun a full matrix, default-server scenario, sanitizer or hardware gate.
All test registrations remain; the affected scope is supported by unchanged
compiled code/data sections and focused regressions. Native-CPU sanitizers,
physical display/toolkit/compositor checks, broader audits and release acceptance
retain their owners. VESA #93 cause work remains deferred.

Receipts: [machine summary](VERIFICATION-2026-10-18-DEVELOPMENT-BUILD-REUSE.json).
Raw archives, baseline/current graphs, source manifests, executed collectors,
original failed collection attempts and continuation receipts remain under
`builddir/codex-2026-10-18/development-build-reuse/`. Local review only; Actions
and status publishers remain disabled. MAINT-02/CLEAN-01 stay open beyond this
bounded checkpoint.
