# Private Clay layout comparison

G4 prepares a bounded experiment for LAYOUT-01
[#212](https://github.com/gonz0w/fvwm-x/issues/212), alongside the native layout
baseline and private drawing comparison. The existing small integer policy is the
production baseline. This experiment does not decide adoption or close #212.

## Source and build boundary

The selected upstream Clay commit is `e6cc36941ab2af5d81107617039d6f527a1c660b`.
Its version 0.14 header, blob `7d9cf4cc7a918d1bdd766867c180851c949a3310`,
is 303,701 bytes with SHA-256
`54aef6d5f96b14e296bebb6db4a755543e98fd7e6c188411afb2f22a4b4f7728`.
The zlib license is 891 bytes with SHA-256
`5dd30f9f52093076b7574458f5efb8242fe6cbbd4669d4a2914e7616810d0df8`.
The lead retains these unchanged under `tests/third_party/clay`. The initial
portable C11 profile disables upstream SIMD.

The `layout-eval` feature is default-disabled. Its private
`FvwmButtons-native-layout-eval` and `FvwmButtons-clay-layout-eval` targets are
non-installed. Ordinary and shared targets and installed consumers acquire no
Clay dependency. The isolated experiment uses `libs/clay_layout_eval.{c,h}` and
`modules/FvwmButtons/buttons_layout_eval.{c,h}` at the existing Buttons reflow
seam. This private-only work needs no user manual option.

## Compared policy and ownership

The Clay candidate accepts only a non-compressed row with equal positive weights.
Clay GROW with minima equalizes smaller controls before distributing surplus;
native weighted layout adds surplus to each minimum. Equal weights with unequal
minima can therefore produce different widths. This is an explicit policy
comparison, not a sizing-parity claim. Native final widths are not fed into Clay.
Unequal weights, compression, explicit refusal, capacity errors, invalid input or
invalid output fall back to native layout, with atomic publication of validated
rectangles. Candidate rectangles must have matching stable IDs, finite ordered
bounds, the expected height, positive adjacent integer widths and exact content
coverage before publication.

Inputs are copied numeric values; the private adapter retains one arena/context,
limited to 256 controls, 512 elements and 512 cached words, with extents from 1
through 32767 and a 2 MiB hard arena ceiling. The actual minimum arena has not yet
been measured. No arena-backed result pointer is retained, and the current Clay
context is cleared before arena retirement. Native Flocale measurement, icons,
drawing, input, hit testing, focus, action and update owners remain in place.
Form collection remains a second native baseline without a Clay port. The
experiment adopts no Clay text, input, scroll, transition or rendering elements.

`FVWM_LAYOUT_EVAL_TRACE=1` enables a bounded once-exit private witness;
presence of `FVWM_LAYOUT_EVAL_REFUSE` forces fallback. A correctness witness
contains `layout_eval: 1`, `pid`, `backend`, `calls`, `native`, `clay`,
`fallback`, `errors`, `arena_bytes`, `refused`, `metadata` and `complete: 1`. These are
experiment controls and records, not user configuration.

## Evidence and limits

Input/output/refusal/ownership C oracles compile; real-X fixture changes are
authored. Both remain unexecuted. Six focused offline host JSONL/update-tag tests and
restored affected compilation are recorded in the [source/check report](../verification/VERIFICATION-2026-11-04-CLAY-LAYOUT.md). The existing
[graphics fixture](../../tests/live_x/graphics_eval_live.py) uses ordinary drawing
with a private native or Clay layout variant, shared row geometry, input and
capture, and the existing Form normal 16-item collection. The separate
[native layout fixture](../../tests/live_x/native_layout_live.py) retains the
full 4096-item native browser baseline. Correctness collects the once-exit
witness; performance disables observation and uses fitted, uninjected controls.

For a future authorized run, supply the same schema-1 input record described in
the [drawing observer contract](DRAWING-OBSERVER-CONTRACT-2026-11-04.md) and an
owned compiled profile and artifacts directory. For example:

```sh
python3 tests/live_x/graphics_eval_live.py --build /path/to/owned-build \
  --input-record /path/to/input-record.json --artifacts-dir /path/to/artifacts \
  --profile slate-dark --variant ordinary --layout clay --layout-case fit \
  --scenario positive --mode correctness
```

The fixture also offers `--layout native|clay`, `--layout-case
fit|weighted|compressed|update`, `--layout-refuse`, and positive, resize or move
scenarios across the three existing profiles. This recipe has not been executed. UI behavior, runtime, three-theme comparisons,
source/profile/ELF binding and repeated costs remain queued for G5, with the
existing support and cost gates. No speed gain or adoption is established. The
experiment adds an arena, adapter, provenance and upgrade maintenance burden and
removes zero production code; those costs belong in the later decision.

Source review found that lifetime call counts could falsely qualify an ignored
title update. The corrected update oracle requires the once-exit committed
metadata tag to match the known updated Title/ActiveTitle/PressTitle inputs.
The private producer hashes at most 256 controls and 4095 bytes per field,
stages the tag synchronously without retaining pointers, and commits it only
with successful row preparation. This FNV-1a32 test tag is not a security digest
or a promise of collision resistance. Fixed old/new test tags differ; unchanged
Clay widths remain valid. The negative ignored-update host regression passed; real-X update qualification
remains queued.
