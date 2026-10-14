# Bounded colorset effect preparation — 2026-10-06

Tracker: IMAGE-01 / INPUT-01. Status: implemented, focused checks passed;
production activation and combined qualification pending.

The [approved effect contract](../decisions/COLORSET-EFFECTS-PROPOSAL.md) is
implemented in the optional core image adapter. File-backed color colorsets
stage the complete requested revision and retain the published revision until
source, shape and derived effect resources are ready. Superseded and failed
requests cannot publish. First loads retain absent-image behavior. Normal
startup still does not enable the adapter in this checkpoint.

Software effects reuse the existing one-active-worker process transport and
picture scheduler. The existing private executable now also works without SVG
support. Its separate X connection queries pinned source/mask/alpha resources
and server RGB16 colors in 256-pixel tiles. It does not allocate colors or change
X resources. Typed, little-endian results preserve legacy integer rounding,
mask/alpha behavior, undithered output allocation and exact background-pixel
reuse. No source-file reload or source-RGB tint substitution is used.

Hardware rendering and simple copies target private pixmaps. The shared owned
X acknowledgement covers creation/rendering and also orders source requests
before a software worker can read them through its separate connection. Parent
upload and palette allocation remain bounded. File-effect commands no longer
need their synchronous final XSync barrier. Root transparency, gradients,
bitmap conversion and unrelated image callers retain their existing paths.

Exact color allocation references and source backing survive through actual
child reap. Published effects retain their background allocation. Cancelled
commit-batch views retain revision contexts until their bindings retire, even
when a sibling publishes first. The legacy initial saved zero background is
represented as an unallocated value rather than adopting another owner's
allocation. Existing delayed colorset cleanup retains whole picture backing.
Terminal event-loop turns cancel and reap an effect reader before closing X;
these turns dispatch no commands, owner commits or window lifecycle work.

The default software deadline is five seconds and the verified worker memory
limit is 1024 MiB. Finite build overrides are 1–60 seconds and 128–4096 MiB.
The deadline covers launch, X connection/readback, queries, blending and output;
queue waiting and bounded parent upload are separate. Request dimensions use
the existing 64 Mi-pixel/32768-dimension limits. Output is at most 512 MiB plus
a header, acquired only for active work. Checked dimensions precede output
allocation; malformed, partial and excess data fail closed. Failed output
storage is released before advancing the queue. The worker is not an OS or
X-server permission sandbox.

## Exact source and focused gates

Frozen source:
`builddir/codex-2026-10-06/svg-colorset-effects/source-colorset-effects-final.tgz`.
SHA-256: `c25e8ea31a2612a2c58db41ab38e487d5fdebe9261c5384b4d266548efdd6b7e`.
The snapshot extends the preceding focused packet checkpoint with scoped
source/build/manual additions. All six profile trees match all 1260 frozen
file hashes and modes. Metadata captures built binary identities (18 normal,
18 sanitizer, 17 software, 11 SVG-disabled and 12 per BSD profile).

| Profile | Result |
| --- | --- |
| Linux ARM64, Clang 18.1.3, PNG 1.6.59, XRender | 13 focused entries pass |
| Same source, scoped local ASan/UBSan | 10 policy/direct-X entries with leaks enabled and 3 actual-WM entries with core leaks disabled pass |
| Linux ARM64, GCC 13.3.0, WM XRender disabled, Xcursor enabled | 13 focused entries pass |
| Linux ARM64, Clang, SVG disabled | 7 focused entries pass, including the real worker and actual-WM PNG effects |
| FreeBSD ARM64, Clang 19.1.7, XPM disabled | 8 focused native/process/nested-X entries pass |
| OpenBSD ARM64, Clang 19.1.7 | Same 8 focused entries pass |

The 62 suite executions cover checked blend/protocol policy, exact allocation
retention, existing averages/packet/store behavior, bounded ARGB conversion,
scheduler ownership, independent legacy pixel comparisons and actual colorset
publication. Direct X probes cover 24/8-bit displays; ARGB also covers the strict
palette profile. Worker comparisons include opaque, binary-mask and partial
alpha pixels, added alpha, tint and source dither/no-alpha options. Hardware and
software queue results are compared against the existing independent public
renderer with the same source and palette state.

Actual-WM fault injection covers first-load absence, Plain cancellation,
published-state retention, cached source use after deleting the file, shared
source owners, a foreground-only superseding command, later matching effects,
partial/mismatched output, complete output followed by a stalled child, refusal,
recovery, delayed cleanup and Quit. New-client management, module commands and
a scheduled command finish during the stall in approximately 0.082–0.102
seconds in these functional observations; this is not a latency benchmark.
The complete-output/stalled-child controls refuse in approximately 5.06–5.11
seconds, including observer overhead, and retain the old packet/resources.
Quit leaves no live effect reader. The deterministic direct-core regression
holds a cancelled view across publication and exercises its late binding on
both 24/8-bit displays under leak-enabled sanitizers.

Evidence beside the source: `evidence-normal.tgz`, `evidence-asan.tgz`,
`evidence-software.tgz`, `evidence-no-svg.tgz`, `evidence-freebsd-arm64.tgz`
and `evidence-openbsd-arm64.tgz`. Accepted logs use `fvwm-effects-final`.
The source/mode manifests, owned-file list and freeze/collection scripts are
also retained. Fixture artifact directories can include earlier retained runs;
final acceptance logs, metadata and each final evidence.json identify the
accepted profile. Initial optional-profile failures are preserved separately
in `source-initial-optional-gates.tgz` and
`evidence-initial-optional-failures.tgz`.

## Corrections and remaining qualification

The first older colorset observer assumed immediate ordinary PNG effects. It
now waits for acknowledged publication. The SVG-disabled test graph initially
lacked the actual-core fixture; it now supplies the same fixture for that
profile. New probes also needed their existing X include dependencies and a
core context header. These were fixture/build failures, corrected before the
acceptance runs. The ownership review found and corrected a cancelled-batch
context lifetime before production activation; its deterministic regression
passes. No new runtime library or second worker pool was added.

This checkpoint does not qualify the full Linux/BSD AMD64/ARM64 matrix, installed
behavior, BSD default X servers, normal startup activation, a formal latency
benchmark, native Linux release sanitizer CI or physical hardware. Those gates
remain with the combined activation batch. Existing earlier image-owner reports
retain their own frozen source and scope.
