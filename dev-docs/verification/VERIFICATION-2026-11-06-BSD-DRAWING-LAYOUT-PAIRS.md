# Remaining BSD drawing and layout comparisons

This checkpoint extends the
[OpenBSD AMD64 paired measurements](VERIFICATION-2026-11-06-OPENBSD-AMD64-DRAWING-LAYOUT.md)
for [#211](https://github.com/gonz0w/fvwm-x/issues/211) and
[#212](https://github.com/gonz0w/fvwm-x/issues/212). The selected profiles
are FreeBSD ARM64 and AMD64 and OpenBSD 8.0 ARM64. Each uses one build,
private Xvfb, Slate dark and the same tracing-disabled fixture.
ARM64 uses QEMU HVF and FreeBSD AMD64 uses QEMU TCG.

The first block runs ordinary, native drawing, Cairo, native layout and
Clay. The second block reverses that order. Each case drives six actions
per module. Each layout case also drives six resizes to the verified
client size and prepared-rectangle endpoint. Both modules are sampled
at four workload and idle endpoints and must exit before the core.
The ordinary case supplies the baseline. The private native controls
provide the matched comparisons for the two candidate backends.

Action and resize times include IPC, server scheduling and harness
queries. Displayed CPU and RSS come from the bound processes. The
results describe each profile and its guest execution mode.

## Results

All 30 cases passed. The new runs captured 360 actions, 72 reflows and
240 process observations. Both modules exited in every case. Each cell
below gives the first and second block medians in milliseconds, with
six observations per median.

| Profile | Variant | Buttons action | Form action | Layout reflow |
| --- | --- | ---: | ---: | ---: |
| freebsd-arm64 | Ordinary | 0.921 / 1.115 | 12.996 / 12.123 | — |
| freebsd-arm64 | Native drawing | 1.093 / 1.062 | 12.145 / 12.516 | — |
| freebsd-arm64 | Cairo | 1.081 / 1.073 | 11.853 / 12.356 | — |
| freebsd-arm64 | Native layout | 1.028 / 1.147 | 12.827 / 13.283 | 59.221 / 59.200 |
| freebsd-arm64 | Clay | 0.852 / 1.022 | 13.536 / 12.560 | 59.016 / 57.696 |
| freebsd-amd64 | Ordinary | 8.628 / 8.235 | 20.246 / 21.085 | — |
| freebsd-amd64 | Native drawing | 8.463 / 8.238 | 20.080 / 19.861 | — |
| freebsd-amd64 | Cairo | 8.752 / 8.952 | 19.192 / 20.161 | — |
| freebsd-amd64 | Native layout | 8.238 / 8.254 | 20.155 / 20.950 | 73.298 / 74.344 |
| freebsd-amd64 | Clay | 8.841 / 9.087 | 20.062 / 20.861 | 74.619 / 72.980 |
| openbsd-arm64 | Ordinary | 1.777 / 1.623 | 21.124 / 20.554 | — |
| openbsd-arm64 | Native drawing | 1.597 / 1.606 | 20.955 / 21.237 | — |
| openbsd-arm64 | Cairo | 2.135 / 1.975 | 20.949 / 20.917 | — |
| openbsd-arm64 | Native layout | 1.612 / 1.697 | 19.735 / 21.109 | 63.305 / 62.122 |
| openbsd-arm64 | Clay | 1.534 / 1.488 | 20.449 / 20.918 | 62.872 / 62.012 |

The next table gives candidate minus matched native results for each block.
RSS uses the sample taken after the workload.

| Profile | Cairo Buttons action Δ (ms) | Cairo Buttons RSS Δ (KiB) | Cairo Form RSS Δ (KiB) | Clay reflow Δ (ms) | Clay Buttons RSS Δ (KiB) |
| --- | ---: | ---: | ---: | ---: | ---: |
| freebsd-arm64 | -0.012 / +0.011 | +1320 / +1420 | +1396 / +1420 | -0.205 / -1.504 | +168 / +144 |
| freebsd-amd64 | +0.289 / +0.714 | +1312 / +1428 | +1384 / +1408 | +1.321 / -1.364 | +220 / +204 |
| openbsd-arm64 | +0.538 / +0.369 | +800 / +748 | +808 / +776 | -0.433 / -0.110 | +140 / +172 |

Cairo used more RSS in all six new pairs. Buttons action medians were
close on FreeBSD ARM64 and higher with Cairo in both FreeBSD AMD64 and
OpenBSD ARM64 blocks. Clay used more Buttons RSS in all six pairs.
Its reflow median was lower in both ARM64 profiles, with the largest
difference 1.504 ms. On FreeBSD AMD64 it was higher in the first block
and lower in the second. These small samples describe the selected
workloads and their observed variation. R1 retains native drawing
and native layout.

The workload CPU values below are the minimum and maximum displayed
increments in centiseconds. Startup is the observed core MFL endpoint
range in milliseconds. All 60 sampled 0.4-second idle intervals recorded
zero displayed CPU increment. Wakeups, peak memory and implicit X11
roundtrips remain unavailable.

| Profile | Buttons workload CPU | Form workload CPU | Core MFL startup |
| --- | ---: | ---: | ---: |
| FreeBSD ARM64 | 0–1 | 1–1 | 52–60 |
| FreeBSD AMD64 | 3–12 | 1–7 | 209–362 |
| OpenBSD ARM64 | 1–2 | 0–1 | 56–68 |

## Build, loader and package observations

The FreeBSD 15.1-RELEASE-p4 guests used Clang 19.1.7 and retained builds
in separate COW children. The OpenBSD 8.0 ARM64 guest used Clang 22.1.6
and the stock pinned preparation. Each build selected ten targets.
All profiles used Go 1.27.2 and private PNG 1.6.59.

| Profile | Compile | Job | Loader file bytes | Package records |
| --- | ---: | ---: | ---: | ---: |
| FreeBSD ARM64 HVF | 0.27 s incremental | 70.57 s | 116,920 | 596 |
| FreeBSD AMD64 TCG | 126.02 s incremental | 257.98 s | 120,400 | 603 |
| OpenBSD ARM64 HVF | 22.00 s after 6.93 s configure | 182.91 s | 446,208 | 41 |

Each guest retained ten executable identities and ten `ldd` records.
FreeBSD's loader path is `/libexec/ld-elf.so.1`. The AMD64 file hash is
`305a0b7947a890c653e32a1ed9fc50c100416658dc422f2ca8de9ebcda9ddb6f`.
ARM64 matched the earlier [FreeBSD loader identity](VERIFICATION-2026-11-05-FREEBSD-DRAWING-LAYOUT.md).
OpenBSD's records name `/usr/libexec/ld.so`, with ARM64 file hash
`823f0ec9ffaec4d03717fdd053e9501ae0223733b922e0ccd3ad1b0a8e1d0978`.
FreeBSD supplied nominal package sizes. OpenBSD supplied local package names
and versions. Incremental installed dependency costs remain open.

## Evidence and continuation

Source revision `7794e94d4047f456f94b11f4d5bfc7962762663b`, tree
`766d5355464154beadb13435f7b61ec51e1bbdf7`, with 3,611 source entries.
Source manifest SHA-256
`5ad74d3dc0ba8b3275accc0523c99f16643813f199bbda1d37515d29e1b77c58`.
The source changes since the measured OpenBSD AMD64 revision affect only
the tracker, verification index and its report. Its original result remains
bound to its own source and guest.

Reviewed warm plan SHA-256
`a86adda18acbd8921d9551985d84d073603a610a9907ac72e009e73524a29ce2`.
The first warm plan left the read-only firmware files unpinned. Preflight
caught the gap and both firmware hashes were added before any FreeBSD run.
Reviewed OpenBSD plan SHA-256
`1de9fee2e76a4e456f51e53afd305f5bed7b1244e2736d973f40925ba3ad1c68`.

| Profile | Dispatch receipt SHA-256 | Exported files |
| --- | --- | ---: |
| FreeBSD ARM64 | `2f741854f0286e69f0efc20495bc06a4ca5bcc1e497346b0b5b3d2b5fa0358dc` | 93 |
| FreeBSD AMD64 | `d235a8050d692c6c65faf4885a4f46d4873b66216c1c130a0fe3c25a26846965` | 93 |
| OpenBSD ARM64 | `97722ead2aca1e2a7338eef3aa653bf1d8098efe3a688449722d80cfc38335c8` | 123 |

The guest artifacts and the per-case source, binary, Xvfb and process
observations are under `builddir/codex-2026-11-06/r1-bsd-paired`.
All three export archives and their 309 files matched the receipts.
All owned command groups retired and all three QEMU processes exited
with status zero. Both FreeBSD backing chains and firmware sources
matched their admission identities after teardown. The current guest
images are stopped.

The four BSD profiles now have paired evidence for this fixture.
Implicit waits, installed costs, broader consumers and release
qualification remain open. The periodic whole-WM ledger remains at 4/5.
