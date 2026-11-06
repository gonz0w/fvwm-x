# OpenBSD AMD64 drawing and layout comparisons

This checkpoint covers the paired workload for
[#211](https://github.com/gonz0w/fvwm-x/issues/211) and
[#212](https://github.com/gonz0w/fvwm-x/issues/212) on stock OpenBSD 8.0 AMD64
under QEMU TCG. It uses the shared private Xvfb fixture and the Slate dark
profile from the [ARM64 observation](VERIFICATION-2026-11-06-OPENBSD-DRAWING-LAYOUT.md).

The selected order was ordinary, native drawing, Cairo, native layout and
Clay layout, followed by the reverse order. Each case uses a private Xvfb
and module path, with six action samples per module and six reflows in each
layout case. The ordinary control uses the ordinary module binaries.
Native drawing and Cairo share the same private transition policy.
Native layout and Clay use ordinary drawing and their respective layout
controls. Tracing stays off for performance observations.

The report covers descriptive measurements from this emulated CPU profile.
Action latency includes IPC and harness overhead. Reflow latency includes
the actual resize and geometry queries through the prepared-rectangle
endpoint. Displayed CPU and RSS come from owned process samples.

## Paired results

All ten cases passed, with 120 action samples, 24 reflows and 80 process
samples. Both module children retired in each case and the core exited
successfully. Each cell below gives the first and second block medians in
milliseconds. Each median covers six samples.

| Variant | Buttons action | Form action | Layout reflow |
| --- | ---: | ---: | ---: |
| Ordinary | 6.488 / 6.491 | 23.128 / 23.359 | — |
| Native drawing | 6.313 / 6.513 | 23.765 / 23.310 | — |
| Cairo drawing | 7.097 / 7.521 | 23.256 / 23.838 | — |
| Native layout | 6.423 / 6.405 | 23.475 / 22.469 | 77.828 / 73.037 |
| Clay layout | 6.413 / 6.154 | 22.814 / 23.047 | 73.112 / 72.501 |

Compared with the paired native drawing case, Cairo increased the Buttons
median by 0.784 and 1.007 ms. The Form median moved in opposite directions
across the two blocks. At the post-workload sample, Cairo RSS was
768 and 800 KiB higher for Buttons and 828 and 836 KiB higher for Form.

Clay reflow medians were 4.716 and 0.536 ms lower than the corresponding
native layout medians. Buttons post-workload RSS was 84 and 188 KiB higher.
The size of the reflow difference varied between blocks. The two observations
and the emulated CPU are insufficient to estimate a hardware speedup.

Across the ten cases, Buttons displayed 3–9 centiseconds of workload CPU
and Form displayed 1–4. The sampled 0.4-second idle intervals recorded
zero displayed CPU increments for both modules. Wakeups, peak memory
and implicit X11 roundtrips remain unavailable. Startup to the core MFL
endpoint ranged from 459 to 718 ms. The mapped owned-invocation endpoints
ranged from 466 to 516 ms for Buttons and 334 to 510 ms for Form. Those
startup endpoints include harness and IPC work.

## Build and dependency observations

One build configured in 77.90 seconds and compiled the ten targets in
334.38 seconds. The complete job took 738.31 seconds, including boot,
preparation, compilation, all measurements, export and teardown.
The build used OpenBSD Clang 22.1.6, Meson 1.12.1, Go 1.27.2 and
private PNG 1.6.59. Meson used a debug-optimized build with LTO off.
Graphics and layout evaluations, session D-Bus, SVG and PNG were enabled.

The pinned AMD64 PNG archive restored 35 entries, including seven links
to regular files inside its prefix. The upstream 36/36 receipt comes from
the retained G6 build. This run verified the compiler and 12 installed
file hashes, matched 256 current vendor PNG API symbols, and compiled
and ran the current PNG/librsvg loader and version probe. The probe
resolved PNG to the private library with SHA-256
`7159bf410533b58020da66773522c2b01f32165adc11c598e44b9e31d815d8b9`.
The preflight rejected an extractor that accepted only basename link
targets. The corrected extractor was reviewed before the first and only
guest execution.

| Executable | Ordinary bytes | Native evaluation bytes | Candidate bytes |
| --- | ---: | ---: | ---: |
| Buttons drawing | 1,676,360 | 1,743,120 | 1,757,160 |
| Form drawing | 1,706,888 | 1,777,664 | 1,791,656 |
| Buttons layout | 1,676,360 | 1,687,840 | 2,243,840 |

These are file lengths of the debug-optimized executables. The core was
5,208,488 bytes and MFL was 429,640 bytes. The ten `ldd` records identify
`/usr/libexec/ld.so` at 343,624 bytes and SHA-256
`d3e8c243136a00a5125d4705b24295755a142e97ad667b209feaf35d28d1021c`.
The local package list has 42 entries, including Cairo 1.18.6, D-Bus
1.16.2p1v0 and librsvg 2.63.2v0. Incremental installed dependency costs
still require separate evidence.

## Evidence and continuation

Source revision `d78aa544f3ee4caba3214572c1cd99e264e9378a`,
tree `08bd1ed588f8baf35f66321419ec20154741dc8d`.
All 3,610 archived source entries matched their hashes and modes before
and after the build. Source manifest SHA-256
`95ac294fd5f6fe5a11727eec9fc080a31d5b4e1b8382919b28f1d717d7bb576b`.
Reviewed dispatch plan SHA-256
`933fa18b39879690be9abdf70eb9c61021b37c46f41ceda8fc02bfa9ac1e0531`.
Dispatch receipt SHA-256
`026859cda952a4979051ad63f4d8bc57600669d4b1dc396eddbca4a02b647834`.
Xvfb SHA-256 `679c712d6469250c93442aa0af218acebad8fc26d09cf3cb71f7c34aba46f643`.
Base `ps` SHA-256 `d563d3f9b7ce64d04cc314a8fa5d3a431e738f94f40b685f0152af3db0682eb4`.

Artifacts are under
`builddir/codex-2026-11-06/r1-openbsd-amd64-paired/attempt01`.
All 130 exported file hashes and the export archive matched. Each of the
25 tracked build and fixture command groups retired, QEMU exited with
status zero during teardown and the disposable guest disk is stopped.

R1 retains the existing native drawing and layout decisions. Other BSD
paired measurements, implicit waits, installed costs and broader consumer
and release qualification remain open. The run used the existing Slate
dark fixture. The periodic whole-WM ledger remains at 4/5.
