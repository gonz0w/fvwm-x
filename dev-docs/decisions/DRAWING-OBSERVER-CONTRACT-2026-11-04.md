# Private drawing comparison observer

G3 for GRAPHICS-02 [#211](https://github.com/gonz0w/fvwm-x/issues/211)
adds a bounded source-level observation path for the existing Form and Buttons
comparison prototypes. It supports the later G2–G5 evaluation; it does not select
Cairo or Clay for production. The `graphics-eval` Meson feature defaults to
disabled. Its evaluation module targets are private and non-installed; ordinary
modules remain the external behavior and cost baseline. See the
[prototype scope](DRAWING-COMPARISON-PROTOTYPE-2026-11-03.md).

## Collection and interpretation

Set `FVWM_GRAPHICS_EVAL_TRACE=1` in a private evaluation module's environment to
collect its trace. Each module keeps at most 256 copied value records, with two
slots reserved for overflow and terminal facts. Counters saturate; overflow,
saturation, clock failure, missing or partial output cannot qualify evidence.
There is no widget pointer retention, per-frame I/O or observation timer. On
orderly exit, the observer exports once to its owned stderr log, including for
choice-only Form startup. Export does not access X or widgets. The JSONL stream
identifies schema 1, PID, module and backend; the
[parser](../../tests/live_x/graphics_eval_records.py) defines its exact fields and
rejects unowned or incomplete streams. A failed or partial write is not a pass.

Records carry sequence and monotonic time, transition tokens, identity and
revision, RGB values, lifecycle and operation facts. Selected draw-operation
request deltas span the whole operation, including native fallback; paint-only
issued requests are separate. These are Xlib issued-request counts, not implicit
synchronous roundtrips. No new synchronization barrier measures those roundtrips;
a complete supported measurement remains a prerequisite. Own Cairo scratch,
copy and resource request/retirement counters describe this prototype, not heap
usage or total process/server resources. Ordinary binaries do not supply these
counters; absence must not be represented as zero.

`FVWM_GRAPHICS_EVAL_MOTION=full` selects full motion; the private default is
reduced motion. Presence of `FVWM_GRAPHICS_EVAL_REFUSE` injects refusal. These
controls and the trace are private evaluation inputs, not user configuration.

## Fixture and evidence

The Linux [comparison fixture](../../tests/live_x/graphics_eval_live.py) builds
private `ModulePath` symlink views of ordinary, native and Cairo modules. Its
Slate dark, Dune light and enlarged Classic profiles are controlled subsets of
the native-layout fixture, not complete theme qualification. It drives positive,
equal, replacement, press, move, resize, palette, unmap, destroy and choice-only
scenarios; reduced motion and refusal are separate controls. Qualification binds
admission, identity, revision and RGB to the relevant token and the owned
stimulus interval and reason. Interruption needs evidence of the targeted work
active before cancellation; a missed stimulus is inconclusive, not a pass.

Correctness uses instrumented evaluation modules; performance disables observation
and compares against ordinary binaries. Retain repeated samples and variability.
Command receipt latency includes harness/query overhead and is not input-to-photon
latency. Linux `/proc` observations cover owned processes and do not prove zero
wakeups. The fixture source defines the input-record schema and records the source,
generated configuration, binaries, tools and server/profile identity; those inputs
must be frozen and checked for the actual comparison.

For a future authorized run, supply an owned compiled profile and a schema-1 input
record satisfying `input_record()` in the fixture, then use a dedicated artifacts
directory, for example:

```sh
python3 tests/live_x/graphics_eval_live.py --build /path/to/owned-build \
  --input-record /path/to/input-record.json --artifacts-dir /path/to/artifacts \
  --profile slate-dark --variant native --scenario positive --mode correctness
```

This is a setup template, not an executed result. Source review identified and
corrected token/warmup false qualification and a receipt that could precede outer
cleanup failure. Fourteen offline host contract tests and one affected native
Linux ARM64 compilation passed; see the [source/unit/build report](../verification/VERIFICATION-2026-11-04-DRAWING-OBSERVERS.md).
The [G5 Linux checkpoint](../verification/VERIFICATION-2026-11-04-DRAWING-LAYOUT-CHECKPOINT.md)
now records composed native C/real-X execution, nine theme captures and repeated
component observations, with the failed attempts and scoped evidence reuse.
It also fixes Cairo scratch allocation after destination destruction. All-six-
target support/cost, complete implicit-roundtrip measurement and the adoption
decisions remain open; #211 remains open. The setup above is still a template,
not a substitute for the source/profile-bound receipts.

The controlled flat Buttons row keeps Frame/Padding as child defaults. Its
queried outer prepared rectangles start at the root origin and fill the client
width and height; GetInternalSize's child content insets are not query geometry.
G5 corrects the original source-only inset assumption and retains its ordinary
real-X assertion failure; the earlier authored fixture was not runtime evidence.
