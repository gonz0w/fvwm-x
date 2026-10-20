# FVWM-X platform support

The first release targets Linux, FreeBSD, and OpenBSD on AMD64 (x86_64)
and ARM64 (aarch64). All six combinations must build the WM and modules,
run native policy tests, exercise real X clients, and stage a valid install.
Support means native binaries running against each operating system's X11
stack; native Wayland remains outside the project scope.

The matrix is a target, not a universal driver guarantee. RandR 1.5+
framebuffer servers with no monitor topology use a root-sized monitor
named `FVWM-X-Root`. Real RandR topology replaces it when available.
See the dated verification report for native default-Xorg evidence.

The initial BSD baseline is FreeBSD 15.1 with packaged Xorg and OpenBSD 7.9
with the compiler and Xenocara sets. Linux architecture gates use Ubuntu
24.04. Existing GCC/Clang Ubuntu and Alpine coverage remains in place.
Older BSD release lines are outside this initial verification baseline.

## Required gates

These gates qualify a combined acceptance checkpoint or release; they are not a
requirement to rerun every target after each story. During a coherent development
batch, use affected builds/tests and early checks for platform-sensitive changes.
Follow the [test cadence](WORKFLOW.md#implementation-batches-and-test-cadence),
and distinguish focused development passes from complete matrix qualification.

| Target | Architectures | Build and X gate | CI |
| --- | --- | --- | --- |
| Linux | AMD64, ARM64 | `tests/run_on_linux.sh` | `linux-architectures.yml` |
| FreeBSD | AMD64, ARM64 | `tests/run_on_bsd.sh` | `bsd.yml` |
| OpenBSD | AMD64, ARM64 | `tests/run_on_bsd.sh` | `bsd.yml` |

Linux uses native runners for GCC and Clang on both architectures, with
separate native AddressSanitizer/UndefinedBehaviorSanitizer jobs. Its real
Xorg dummy scenarios cover monitor/scale changes and geometry transitions.
BSD CI uses real FreeBSD/OpenBSD kernels under QEMU, selecting `x86_64` or
`aarch64` explicitly. Every job preserves its own test evidence.

Run the Linux gate after installing dependencies with
`tests/install_linux_dependencies.sh` and Meson 1.7.0 or a supported newer
version. The installer script is for an Ubuntu guest or CI runner and
requires root. See [BSD.md](BSD.md) for BSD dependencies and local QEMU use.

## Evidence and remaining release checks

Standards qualification follows [STANDARDS.md](STANDARDS.md): pin adopted clauses
and record independent client/fixture results for named OS/build/service profiles.
Test advertised atoms/capabilities, malformed input, ownership/lifetime and
feature-present/absent behavior. Reuse existing tests at the owning combined
checkpoint. The portable default and optional D-Bus/assistive profiles need
separate results; unavailable providers cannot be reported as supported.

Default theme acceptance follows [THEME-STANDARDS.md](THEME-STANDARDS.md): numeric
role/state contrast plus actual rendered transparency, target geometry, keyboard,
enlarged-text and scale checks. Screen-reader/IME/toolkit/browser/physical display
behavior requires actual provider/hardware evidence. Documentation-only or token
checks do not qualify those paths. Public HTML accessibility is DOCS-01-owned.

Configured CI jobs are not passing evidence. GitHub hosts `gonz0w/fvwm-x` for code, issues and PR review; all verification
runs locally. Record native CPU/platform results separately from emulated ones.
The dated verification reports in `dev-docs/` record results actually
obtained. Older planning summaries remain historical context.

BSD's native Xvfb session checks real client mapping, framing, module startup,
and all eight shade directions, plus real image/font cache behavior.
Separate default-server probes cover real pointer grabs, opaque resizes,
and Xlib ConfigureNotify feedback on both BSD architectures. Linux's
interactive Xorg gate also checks repeated mixed-scale pointer crossings.
Physical GPU, mixed-monitor hotplug, external compositor, actual GTK/Qt
toolkit feedback, and physical input devices remain release verification
work. Pure policy and all authoritative X state stay single-threaded.

Current priorities and decisions live in [PROJECT-TRACKER.md](PROJECT-TRACKER.md).
The [2026-10-04 verification](verification/VERIFICATION-2026-10-04.md) adds bounded MFL stream
transport and configure-request regression evidence to the existing matrix.

Shared native setup now lives in `tests/run_native_gate.sh`; Linux/BSD wrappers
retain their entry points. Its Linux `--sanitizers` profile keeps installation
separate from instrumented tests. See [CI candidates and upkeep](ci/README.md)
and the [affected test-infrastructure checkpoint](verification/VERIFICATION-2026-10-12-COVERAGE-CI.md).
These local results do not establish unrun native release sanitizer or hardware
qualification. Hosted CI is not required; preserve the same evidence standards.

The optional `tests/run_native_gate.sh --go-clients` gate and authenticated
`tests/run_bsd_default_x.py --disposable-vm` driver are locally qualified on the
six-target matrix; see the [client-profile/default-X report](verification/VERIFICATION-2026-10-12-CI-PROFILES.md).
They reuse qualified default-server hardware setup; provider-specific VM profiles
and actual native sanitizer execution remain separate acceptance gates.
GitHub Actions is disabled under the [local-review decision](decisions/LOCAL-REVIEW-GITHUB-2026-10-13.md).

## Performance observations

The opt-in comparison consumes two already qualified builds on the same quiet
Linux profile. Run it every five merged implementation checkpoints, or earlier
for a change expected to affect responsiveness or memory; see the
[workflow cadence](WORKFLOW.md#periodic-performance-comparisons).

```sh
python3 tests/compare_performance.py \
    --baseline-fvwm /path/to/reference/fvwm3 \
    --baseline-mfl /path/to/reference/FvwmMFL \
    --baseline-revision REFERENCE_COMMIT \
    --candidate-fvwm /path/to/current/fvwm3 \
    --candidate-mfl /path/to/current/FvwmMFL \
    --candidate-revision CURRENT_COMMIT \
    --profile linux-arm64-gcc-default \
    --artifacts-dir /path/to/new-comparison-directory
```

No build, install, correctness matrix or timing threshold is added. Each side
uses an owned Xvfb server with the same profiler, 128 samples per workload and
three repetitions in alternating order. Preserve `comparison.json`, all raw
run directories and the source/compiler/dependency receipts for both builds.
RSS is unavailable outside Linux `/proc`; CPU/memory and latency reflect the
recorded environment. Physical hardware remains separate qualification.
