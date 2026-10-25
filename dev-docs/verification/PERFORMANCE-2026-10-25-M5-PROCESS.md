# Periodic M5 comparison against PR204

The fifth implementation checkpoint since the PR204 reference is ready for its
periodic observation. Reuse existing browser bytes and one small metadata-scan
workload; no benchmark build, tuning or platform matrix. The current browser
SHA-256 exactly matches PR204's qualified current binary:
`c1a424baded9a1b1a90c6d1d46cb878f03a01df93068b29a157efd73583dac72`.
The reference copy is reconstructed from those identical preserved bytes.
This is a runtime/environment observation of the same implementation, not a
comparison between different browser algorithms or new Script helper behavior.

One warmup each precedes five alternating pairs over 4096 empty files. A fixed
outer alarm bounds the runner; `os.wait4` supplies per-child CPU/RSS. Source,
bytes, section sizes, individual samples and guest identity are retained in the
[machine record](PERFORMANCE-2026-10-25-M5-PROCESS.json).

| Observation | PR204-identical reference | Current |
| --- | ---: | ---: |
| Median wall | 9.452 ms | 8.773 ms |
| Median user + system CPU | 10.480 ms | 10.172 ms |
| Median maximum RSS | 25952 KiB | 25952 KiB |
| Browser file | 5,623,589 bytes | 5,623,589 bytes |

Identical executable bytes rule out an implementation speed/size claim. The
small timing differences describe measurement variation only; do not compare
absolute timings/RSS across dates or infer physical desktop input latency.
An initial runner checked the wrong JSON response field during warmup; change
only `entries` to the existing `items` contract, retain the failure and repeat
the small observation. No production change or extra build is involved.

The current M5 checkpoint becomes the periodic reference at 0/5 when merged.
Required release/native-CPU qualification and later performance tuning retain
their separate owners. Script child/descriptor correctness and its measured
binary cost are recorded with the [M5 acceptance](VERIFICATION-2026-10-25-M5-SCRIPT-PROCESS.md).
