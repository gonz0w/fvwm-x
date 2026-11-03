# Settings projection: reply-size and allocation observation

The fixed-cap reply repair trades smaller wire output for more temporary work
in this four-setting query microbenchmark. Three paired process medians on one
Darwin ARM64 host show +15.245 µs/query, +6113 allocated bytes/query and a
327-byte smaller reply. This supplies a bounded cost observation for PERF-01 #29
and BUILD-PROFILES-01 #226; it establishes no whole-desktop speed/RSS result.
[Samples and receipts](PERFORMANCE-2026-11-02-SETTINGS-PROJECTION.json) ·
[Exact benchmark source](PERFORMANCE-2026-11-02-SETTINGS-PROJECTION.go.txt).

| Median, three processes per side | PR348 baseline | A4 codec | Candidate/baseline |
| --- | ---: | ---: | ---: |
| ns/query | 33756 | 49001 | 1.452 |
| Allocated bytes/query | 24522 | 30635 | 1.249 |
| Allocations/query | 215 | 395 | 1.837 |
| Wire bytes/query | 1933 | 1606 | 0.831 |

Each process performs 500 measured operations with GOMAXPROCS=1, Go1.27.2,
Apple M5 Max, Darwin/ARM64, offline vendored inputs and the same tool cache.
Order is baseline/candidate/candidate/baseline/baseline/candidate. The host
preflight finds no QEMU/vfkit processes; all owned labs are already stopped.
All six processes exit0, with stopped process groups and no timeout. Individual
baseline samples range 33678–38220 ns/query; candidate samples 47176–50178.
Three small samples have no confidence interval or universal causal guarantee.

The immutable baseline is PR348 `765191d7b3ba8e6d2f6936cb205f3a853ef83445`.
The candidate is the A4 codec integrated in PR349
`d2624fb79d0e7634054a6c134a1f31602a76a8c1`; all 788 non-injected candidate
snapshot inputs match the current checkout. Both sides use the same injected
benchmark: a ready synthetic provider, default saved/effective values, no UI
submission and `group:desktop`. Exact independent standalone reconstruction and
the existing 8192-byte cap are checked before timing; four records must match.
The source overlay changes only this fixture on both sides and leaves the
original snapshot files intact. No unit suite or X/VM is started.

The original fixture omitted ready=true and failed with provider_not_ready
before timing. Preserve its failed baseline receipt and stopped process group;
it is excluded from these medians. The separately reviewed one-field correction
retains all reconstruction/budget checks and reuses the original source trees.
Earlier quiet preflights were unavailable while a foreign VM ran; it was preserved.

Source inspection attributes the relevant production delta to scoped
metadata/value projection in `ipc.go`: additional group/metric metadata passes
and merge maps are consistent with the higher temporary allocation count. These
queries run in the isolated FvwmDesktop helper. The fixed-cap correctness repair
is retained; #29/#226 own its release-budget disposition. No tuning change or
new performance target is selected by this observation.

The 17% wire reduction here concerns a four-setting query. The separate complete
30-setting boundary matrix in the [A4 report](VERIFICATION-2026-11-02-APPEARANCE-CHECKPOINT.md)
measures the actual cap repair. Allocated bytes are not resident memory. No
startup, idle, title/resize, paint, Linux timing, BSD or native C sanitizer result
is inferred. The general WM/session comparison remains due; this supplemental
observation does not reset its counter or start another VM.
