# Periodic comparison: PR269 reference to native feedback

The due comparison is complete on the final repaired browser binary. No speed
improvement is established. [Raw pairs, identities and section sizes](PERFORMANCE-2026-10-27-FORM-FEEDBACK.json)
remain the authority; the selected medians below are not timing gates.

The retained `m5-performance-bindings-20261026/{core,script,browser}` files match
PR269's recorded SHA-256 values. The same owned Linux ARM64 normal GCC cache
supplies the candidate. Each side gets one warmup, then five alternating pairs:
a 4,096-empty-file browser worker scan with `os.wait4`, and the existing standalone
Script workload with 4,096 self-Set references, ready marker, 20 ms settle and
`/proc` observations. No benchmark rebuild or full test matrix is required.

| Median | PR269 reference | Final candidate |
| --- | ---: | ---: |
| Browser worker elapsed | 8.606 ms | 8.534 ms |
| Browser launch-child peak RSS | 27,684 KiB | 27,684 KiB |
| Script ready latency | 52.940 ms | 52.939 ms |
| Script status RSS | 5,232 KiB | 5,232 KiB |
| Script private dirty | 928 KiB | 1,440 KiB |
| Core debug file bytes | 5,479,448 | 5,539,656 |
| Script debug file bytes | 2,211,368 | 2,211,368 |
| Browser file bytes | 5,634,701 | 5,709,309 |

Worker ranges overlap (8.340–9.050 versus 8.396–9.101 ms). Script is byte-identical;
its +512 KiB private-dirty observation remains unexplained variability, with no
allocation or leak cause inferred. RSS highwater and file/debug bytes are not
isolated allocation measurements. Ready latency includes harness scheduling;
coarse CPU ticks and section sums remain in the raw receipt.

The first comparison preceded the reply-admission repair. Its raw results and
measured binaries are preserved separately. Only the small comparison repeats
after that repair so these results bind the final browser ELF. The worker
algorithm is unchanged; this workload does not measure interactive Form feedback,
core responsiveness, whole-session wakeups, native CPU, physical displays or
release resource budgets. Native interaction correctness has its own
[checkpoint](VERIFICATION-2026-10-27-FORM-FEEDBACK.md).

Preserve `m5-performance-feedback-final-20261027/{core,script,browser}` as the next
reference. The separately qualified current Form is retained there as `form` for
future observations; this comparison does not execute it. Its current debug-file
size is 1,764,008 bytes. PR269's historical qualification records 1,636,240 bytes;
that is a source-bound file-size observation, not a fresh Form performance pair.
The due 5/5 comparison is satisfied; reset the periodic cadence to 0/5 on this
candidate's merge. No performance or agent-workflow savings claim is made.
