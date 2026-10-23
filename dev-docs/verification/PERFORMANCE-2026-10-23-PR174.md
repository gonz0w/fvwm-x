# Periodic performance and size observation — PR169 to PR174

The due five-merge comparison uses preserved qualified Linux ARM64 binaries;
no compiler, benchmark build or test matrix is repeated. Core and FvwmMFL remain
**byte-identical** to PR169 (and PR164): 5,396,632 and 418,512 file bytes. Previous
core latency observations remain attached to those exact bytes, rather than
rerunning an unchanged core. This establishes no new core speedup.
[Measurements and identities](PERFORMANCE-2026-10-23-PR174.json).

| Feature binary | PR169 | PR174 | Change |
| --- | ---: | ---: | ---: |
| Browser file bytes | 5,361,449 | 5,523,577 | +162,128 (+3.02%) |
| Browser allocated text/read-only | 3,429,744 | 3,498,899 | +69,155 (+2.02%) |
| Browser data / BSS | 88,195 / 33,805,464 | 88,259 / 33,805,464 | +64 / unchanged |
| Form file bytes | 1,598,608 | 1,623,488 | +24,880 (+1.56%) |
| Form allocated text/read-only | 391,986 | 397,818 | +5,832 (+1.49%) |
| Form data / BSS | 14,432 / 14,000 | 14,704 / 14,048 | +272 / +48 |

Browser growth covers the intervening refresh/actions/live-native/watch features;
Form growth covers fixed labels, live state and timeout renewal. File size
includes debug information and is not resident memory. The prior Form section
counts come from the source-bound PR169 receipt; the current file is measured
now. The PR169 browser is still present and its exact recorded hash is verified.

A small new paired sample runs the existing PR169 and PR174 browser helpers on
the same 4096-empty-file directory, using `--inspect`, one warm-up each and five
alternating-order trials per binary. Native `os.wait4` records elapsed time,
process-tree CPU and maximum RSS; output is discarded, not retained as test data.
The first sampler attempt finds no external `time` utility and is replaced by
stdlib accounting, without installing a package. Synthetic files are removed.

| Metadata scan/worker sample | PR169 | PR174 |
| --- | ---: | ---: |
| Median wall time | 16.046 ms | 15.998 ms |
| Min–max wall time | 15.819–16.377 ms | 15.824–16.429 ms |
| Median maximum RSS | 29,288 KiB | 29,288 KiB |

Ranges overlap; this five-pair sample establishes neither a speedup nor a
consistent regression. It covers one-shot directory metadata/worker handoff,
not sustained watching, native UI input latency, large real-file storage,
physical hardware or a codebase-wide readability score. Go version/build
metadata and exact ELF hashes are preserved. The four current binaries are
copied and verified in the host reference archive before future builds replace
working outputs. Tuning remains deferred.

PR174 becomes the independent periodic reference at **0/5**. The next feature
merge advances it to **1/5**; the affected integration cycle remains separately
at **2/3** after PR174. A new transport feature cannot inherit these older MFL
bytes as its own performance evidence.
