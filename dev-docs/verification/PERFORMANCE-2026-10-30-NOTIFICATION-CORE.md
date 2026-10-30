# Notification checkpoint core performance — 2026-10-30

The due five-merge paired observation compares retained merged PR305
(`8402e098`) with merged PR310 (`bcf6d52d`). Results are mixed: title command
medians fell, while idle and resize medians rose with overlapping three-run
ranges. This observation establishes neither a universal improvement nor a
regression or cause. It is not a performance tuning result.

[Receipt, comparison, raw samples and binary identities](PERFORMANCE-2026-10-30-NOTIFICATION-CORE.json),
SHA256 `04fa6a5181907fc25227b3b183bf7e0131b7654691cb8058631efe1ba1713e53`.
The measured source closure before and after the run was
`6e53c981d6c1a2cd5a4789b6b0f9aae428a07d55d26cd77c6e9d32f03a88e116`;
this is the qualified source used for the measurement, not a claim about later
fixture-only changes.

## Method and observations

The Linux ARM64 GCC debugoptimized profile used an owned Xvfb and three pairs
in alternating order, with 128 command and timer samples per case. Title and
resize requested 10,000 requests per second. Each core used its corresponding
retained or current MFL binary; the drivers were not identical. The candidate
incremental build preceded measurement. The summary is the median of run
medians; ranges span the three run medians.

| Measurement | PR305 | PR310 |
| --- | ---: | ---: |
| Idle command median ms (range) | 0.2916 (0.2831–0.3012) | 0.3522 (0.2523–0.3533) |
| Title command median ms (range) | 0.3472 (0.3185–0.3529) | 0.2906 (0.2807–0.2908) |
| Resize command median ms (range) | 0.2594 (0.2496–0.2640) | 0.2685 (0.2150–0.2789) |
| Idle 10 ms timer median ms | 12.6062 | 12.4809 |
| Title 10 ms timer median ms | 11.9429 | 11.6980 |
| Resize 10 ms timer median ms | 12.4750 | 12.4317 |
| Median per-run peak core RSS KiB | 7,464 | 7,460 |
| Unstripped core file bytes | 5,689,360 | 5,693,008 |
| Core text + data + BSS bytes | 1,499,482 | 1,488,963 |

The unstripped file increased by 3,648 bytes; it includes debug metadata and is
not resident memory. The `size` total decreased by 10,519 bytes (about 0.70%):
text decreased by 10,551 bytes, data increased by 32 bytes, and BSS remained
306,336 bytes. Timer medians were around 12 ms and CPU percentages were similar
in this measured profile. CPU samples use coarse 100 Hz ticks. Median measured
title rates were about 9,996 and 9,992 requests/s; resize rates about 9,995
and 9,996 requests/s.

## Identities and limits

The PR305 core SHA256 is
`d66a52661b4507fa5084241ae540438e6948d0ae70acd12a622212852cbc7a0b`;
PR310 core is `0fb343aa0512dc13c32373671642656e2181f321d86872765ffd881f052c068e`.
Their respective MFL hashes are
`ea499dcffe250ef913ec6ebdce7e3dd13ffb5453bf00041c494649467dbd9dd0` and
`f9e4009245440a9ceb1741278bc2ccd9f9a31b534c9ab7ebe2ed2971e501dbca`.
The receipt records paths, exact commands, options, profile, binary sizes and
profiler inputs; raw profiles retain the samples. The next reference is
`/tmp/fvwm-module-waits-20261015/m5-performance-notification-20261030`.
The counter resets to 0/5 after this comparison through PR310; the current
candidate code becomes 1/5 only after merge.

The profiler does not start the notification module. This core comparison does
not measure notification feature cost, the whole desktop or physical GPU
performance. The #76 action-row floor change affects the module, not this core
measurement. Correctness, feature cost and platform qualification are separate
from this timing observation; the full OS matrix was not rerun for it.
