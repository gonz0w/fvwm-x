# Periodic comparison: merged PR253 to PR264

The due five-checkpoint observation is complete using preserved binaries in the
same owned Linux ARM64 normal GCC environment. No speed improvement is established.
The new high-contrast feature is separate from this accumulated merged-code view.
[Machine receipt](PERFORMANCE-2026-10-26-PR264.json) retains every paired sample,
observer identities, binary sizes and source-count scope.

## Matched observation

Reuse the previous method: one warmup per side, five alternating pairs, 4,096
browser entries, and the same 4,096-self-Set Script/ready marker with a 20 ms settle.
Verify old/current artifact identities before copying the PR264 reference. No
benchmark binaries were rebuilt. Current core/Script identities match accepted
PR256 evidence; browser bytes still match the preserved PR253 reference. The VM
and owned container were stopped initially and resumed without replacement.

| Median observation | PR253 | PR264 | Interpretation |
| --- | ---: | ---: | --- |
| Script ready latency | 52.878 ms | 52.770 ms | Ranges overlap: 52.468–55.339 / 52.407–55.739 ms. |
| Script status RSS | 5,252 KiB | 5,248 KiB | Aggregate resident mappings; no allocation claim. |
| Script anonymous RSS | 928 KiB | 928 KiB | Unchanged median. |
| Script smaps private dirty | 928 KiB | 1,440 KiB | +512 KiB; distinct aggregate, not proof of heap growth or leak. |
| Browser scan elapsed | 8.791 ms | 8.680 ms | Identical-code environment telemetry. |
| Browser launch-child peak RSS | 33,712 KiB | 33,712 KiB | Includes launch footprint; not isolated Go RSS. |

Script CPU remains below one scheduler tick. Ready latency includes harness
scheduling. Status and smaps observations use different accounting and are kept
separate. The private-dirty increase is retained without attributing an allocation
cause, changing an expectation or rerunning to obtain a preferred number.

| Size inventory | PR253 | PR264 | Change |
| --- | ---: | ---: | ---: |
| Core text+data+bss | 1,157,173 B | 1,157,637 B | +464 B |
| Core debug file | 5,386,800 B | 5,387,608 B | +808 B |
| Script text+data+bss | 567,830 B | 568,974 B | +1,144 B |
| Script debug file | 2,207,672 B | 2,211,368 B | +3,696 B |

File size includes debug/build data; section sums and runtime memory are different
measures. The receipt separates runtime C/header deltas from C tests. Recent Go
menu/desktop growth has its own accepted cost rows and is not measured by these
Script/browser cases. Stable selected telemetry cannot establish whole-desktop
memory parity or responsiveness. No core idle/title/resize/redraw, X-request,
true-wakeup, full-session, installed-cost or physical-CPU workload runs here;
#209/#29/#226/#5 retain those finite acceptance owners.

## Retention and cadence

PR264 canonical merge is `c40b27c2c40cfda3eb5f3b6fd6817f8ccf90c9ec`, reviewed
tree `b0c5c180182a865ba5341b80804eaf6fd7ed2116`; GitHub signature and author DCO
are verified. The observer/driver hashes and raw samples are included in the
combined [high-contrast checkpoint](VERIFICATION-2026-10-26-HIGH-CONTRAST.md)
archive. No failed timing sample is omitted. Final review corrected two temporary driver
reference labels using the verified PR253 paths/hashes; original raw labels and
samples remain in the immutable archive, with the correction recorded in JSON. Preserve
`m5-performance-pr264/{core,script,browser}` as the next reference, retaining older
references. The counter resets to 0/5 at PR264; the high-contrast implementation
becomes 1/5 on its merge. This observation is not complete release qualification.
