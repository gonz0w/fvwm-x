# Periodic core observation: current property helper

Owner Cam; due performance ledger5/5. Source through merged PR340
`649bf8591163324942737a839bddd84aadb675a2`. This is a core-only observation in
the retained Linux ARM64 GCC debugoptimized AppleHV/container profile.
[Raw profiles, inputs and sizes](PERFORMANCE-2026-11-02-PROPERTY-CORE.json).

The control relink exactly reproduced the accepted PR327 core SHA
`eee5966cabef5b205fb042b62c8ef4086d91fe4356fbff803df735052c934674`.
Then one current `FProperty` object was compiled with its actual frozen headers
and retained generated configuration. The link map records its allocated
contribution and excludes allocated old FProperty/FSelection/content-policy
objects. The same retained MFL SHA
`3033aaa23933405db7f0baacee91b9718b2419697e9ef7791b3275a196ba5810`
was used for both sides. All reused input/member/provider hashes are guarded.

One control link, one header-dependency preprocess, one compile and one candidate
link precede timing; no full WM/module build or install. The candidate SHA is
`f39aae6254ac132afeb94560b88097b63db94a9bb053e8ac28ccad5c40cc505c`.
It is the reproduced PR327 core with current FProperty, not a fresh full-project
build. Other changed implementations remain outside its link closure.

Three alternating pairs use 128 samples per idle/title/resize case, requested
10,000 load requests/second, and an owned Xvfb1280×720×24. No other VM load was
present. Timing bands overlap; **no causal speedup or regression is established**.

| Case | Baseline command median ms [range] | Candidate command median ms [range] |
| --- | ---: | ---: |
| idle | 0.343 [0.315–0.362] | 0.336 [0.306–0.378] |
| title | 0.350 [0.257–0.363] | 0.303 [0.303–0.305] |
| resize | 0.265 [0.263–0.282] | 0.278 [0.255–0.285] |

Requested-load achieved rates remain about 10,000/second. Scheduled10ms results,
CPU, p95s and raw samples remain separate in JSON; timer latency is not command
latency. Core text changes from1,182,784 to1,183,674 bytes (+890), data/BSS are
unchanged, full debug executable grows3,232 bytes, and observed RSS changes from
7,532 to7,540 KiB (+8). Debug path/section bytes are not runtime allocation.
These are scoped observations, not hardware/whole-desktop or clipboard costs.

Build plus observation/teardown/export took35.295 seconds after lab readiness.
All65 exported hashes verify, including compact six raw profiles and bounded
full diagnostic trees. Failed/timeout collection was prepared but not exercised:
the container would be stopped before host filesystem export, with original
stage failures retained. Actual run passes; owned container and machine are
confirmed stopped. No new UI/theme/screenshots or additional platform matrix.
Reset the periodic ledger at delivery; the new candidate remains retained for
the next relevant comparison.
