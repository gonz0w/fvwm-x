# Drawing core periodic comparison — 2026-11-02

The core comparison completed and was fully collected; its command-latency
median ranges overlap in all three cases. These observations do not establish
a causal speed gain. The broader checkpoint04 **failed**, with seven of its fifteen
runtime jobs failing. This comparison does not qualify that checkpoint.
[Raw comparison, profiles, hashes and receipts](PERFORMANCE-2026-11-02-DRAWING-CORE.json).

The same-host owned Xvfb comparison ran on native Linux ARM64 under AppleHV,
GCC debugoptimized, with SVG, PNG and session D-Bus enabled and NLS and docs off.
Three repetitions per side were interleaved baseline, candidate, candidate,
baseline, baseline, candidate. Each idle, title and resize case used 128 samples;
title and resize requested 10,000 operations per second. Preflight recorded no
other VM processes. The owned container and machine were confirmed stopped after
collection. The comparison is a scoped observation, not a full OS or desktop gate.

| Case | Side | Command median (ms) | Range of run medians (ms) | p95 (ms) |
| --- | --- | ---: | ---: | ---: |
| idle | baseline | 0.175 | 0.172–0.332 | 0.324 |
| idle | candidate | 0.172 | 0.170–0.312 | 0.205 |
| title | baseline | 0.171 | 0.170–0.305 | 0.202 |
| title | candidate | 0.170 | 0.170–0.299 | 0.211 |
| resize | baseline | 0.255 | 0.169–0.259 | 0.431 |
| resize | candidate | 0.166 | 0.164–0.271 | 0.197 |

Values are rounded from the comparison summary; timer samples and individual
profiles are retained in the raw record. Baseline was pre-G1 PR351 code/source07,
core SHA-256 `b551bd16bbd7461ecb410c1026f3daba4023b8caeeb3d1aab2d3f273b067de80`
(5,865,928 bytes). Candidate was G1 source10, core
`6fc3af38b03eeb311d10f00380b3be14e39c23cf337b9438c7e70eeb9fe64963`
(5,867,160 bytes). Both used MFL
`ac6151b52d75935a4fe9eb897761ac5be3cb4caa1efdf3da2fd63155aa1623ba`
(463,696 bytes).

Core text grew from 1,177,019 to 1,177,411 bytes (+392); data (36,548)
and BSS (313,712) were unchanged. The debugoptimized file grew 1,232 bytes.
Observed peak RSS was 7,576 versus 7,572 KiB, not evidence of a meaningful
allocation gain. Later fixture-only repairs preserve the core and MFL. Their accepted outcomes
are recorded in the [composed drawing/glyph checkpoint](VERIFICATION-2026-11-03-DRAWING-GLYPHS.md);
no performance rerun is claimed here.

The separate [query projection microbenchmark](PERFORMANCE-2026-11-02-SETTINGS-PROJECTION.md)
reported approximately 45.2% more time per query within its own scope. It is
not a measured whole-desktop change. The periodic ledger was 6/5 due
before this observation; delivery and any ledger reset belong to the lead.
