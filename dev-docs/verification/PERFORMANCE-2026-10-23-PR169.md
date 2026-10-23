# Periodic performance and size observation — PR164 to PR169

The five-merge observation compares existing qualified Linux ARM64 GCC binaries.
The core and FvwmMFL are **byte-for-byte identical** to the preserved PR164
reference: 5,396,632 and 418,512 bytes respectively. Their previous latency/CPU
observations remain applicable to those exact inputs; repeating a timing run
would measure environment noise rather than a code change. No new speedup or
regression is claimed. [Exact hashes and sizes](PERFORMANCE-2026-10-23-PR169.json),
[prior timing observations](PERFORMANCE-2026-10-23-PR164.md).

Feature work lives in the optional consumer and native Form. The PR168-to-169
browser helper changes from 5,361,409 to 5,361,449 file bytes; allocated text and
read-only data grows 3,426,836 to 3,429,744 bytes (+2,908, about 0.085%). Data and
BSS are unchanged. Debug-containing file size is not resident memory. The
current native Form is 1,598,608 file bytes, with 391,986 allocated text/read-only,
14,432 data and 14,000 BSS bytes. This becomes a useful feature-size reference;
no matched pre-list Form binary comparison or browser responsiveness benchmark
is asserted. The browser did not exist at PR164.

This checkpoint does not establish physical-hardware/native-CPU performance or
a codebase-wide readability score. No compiler, benchmark or test matrix is
repeated for this comparison. PR169 becomes the periodic reference at **0/5**;
the next implementation PR advances it only when merged. Tuning stays deferred.
