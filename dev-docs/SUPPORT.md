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

The current baselines are FreeBSD 15.1 with packaged Xorg and **OpenBSD 8.0**,
with the compiler and Xenocara sets. OpenBSD follows new releases; 7.9 is out
of scope. Advance its fixed inputs and both architecture gates together,
recording fresh qualification before making support claims. Both OpenBSD 8.0 VM architectures
pass the [local baseline checkpoint](verification/VERIFICATION-2026-10-20-OPENBSD-80.md).
Earlier 7.9 results remain historical evidence.
Linux architecture gates use Ubuntu 24.04. Existing GCC/Clang Ubuntu and Alpine coverage remains in place.
Older BSD release lines are outside this initial verification baseline.

## Required gates

These gates qualify accumulated integration and release acceptance; they are not
all feature-PR merge requirements. A normal PR uses one incremental Linux profile
with affected policy/X checks and early affected-target checks for portability.
Batch related changes before one focused Linux gate. Review accumulated risk
after roughly five merged batches or a substantial handoff; that review is not
an automatic all-OS checkpoint. Platform checks follow affected contracts. Full
compiler/feature/install/default-X coverage is required at milestone/release
boundaries, or earlier for a changed contract that needs it. Track deferred gates
explicitly; focused PR results do not establish complete platform acceptance.
Follow the [test cadence](WORKFLOW.md#implementation-batches-and-test-cadence),
and [accepted decision](decisions/BATCHED-CLEANUP-CADENCE-2026-10-23.md).

| Target | Architectures | Build and X gate | CI |
| --- | --- | --- | --- |
| Linux | AMD64, ARM64 | `tests/run_on_linux.sh` | `linux-architectures.yml` |
| FreeBSD | AMD64, ARM64 | `tests/run_on_bsd.sh` | `bsd.yml` |
| OpenBSD | AMD64, ARM64 | `tests/run_on_bsd.sh` | `bsd.yml` |

Linux uses native runners for GCC and Clang on both architectures, with
separate native AddressSanitizer/UndefinedBehaviorSanitizer jobs. Its real
Xorg dummy scenarios cover monitor/scale changes and geometry transitions.
BSD CI uses real FreeBSD/OpenBSD kernels under QEMU, selecting `x86_64` or
`aarch64` explicitly. Each job records its source-bound results; retain raw
artifacts under the [current-artifact rule](WORKFLOW.md#current-artifacts-and-retention).

Run the Linux gate after installing dependencies with
`tests/install_linux_dependencies.sh` and Meson 1.7.0 or a supported newer
version. The installer script is for an Ubuntu guest or CI runner and
requires root. Install/select the required patched Go SDK before the gate; the
Ubuntu dependency script does not assert that a distribution Go package meets
the baseline. Go is required for all profiles. See [BSD.md](BSD.md) for BSD dependencies and local QEMU use.

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

## Focused installed XDG probe

`tests/live_x/xdg_installed_live.py` checks real installed XDG consumers with
current built core/MFL binaries. It requires Python 3, Xvfb, X11 development
headers, the base file(1) classifier and an already qualified build. Configure
that owned build with an actual private Meson `--prefix`; a relocated DESTDIR
does not change the helper directory linked into Go consumers. Install the
existing `desktop-controller,desktop-menu,file-browser` tags. Keep normal
dependency and SDK requirements from INSTALL.md.

From the matching source root, compile the two independent probes against its
generated config.h, then run the opt-in fixture. Use a new evidence directory:

```sh
cc -std=c11 -D_POSIX_C_SOURCE=200809L -UNDEBUG -Wall -Wextra -Werror \
    -I/path/to/build -I. tests/live_x/xdg_installed_client.c \
    -o /path/to/owned/client $(pkg-config --cflags --libs x11)
cc -std=c11 -D_POSIX_C_SOURCE=200809L -UNDEBUG -Wall -Wextra -Werror \
    -I/path/to/build -I. tests/live_x/startup_notification_client.c \
    -o /path/to/owned/producer $(pkg-config --cflags --libs x11)
python3 tests/live_x/xdg_installed_live.py \
    --prefix /path/to/private-prefix --fvwm /path/to/build/fvwm3 \
    --mfl /path/to/build/modules/FvwmMFL/FvwmMFL \
    --client /path/to/owned/client --producer /path/to/owned/producer \
    --execution-profile 'actual OS/architecture and CPU execution mode' \
    --artifacts-dir /path/to/new-evidence
```

The fixture uses isolated HOME/XDG roots and reports real child exits and cleanup
failures in `evidence.json`. Preserve a failure before correcting its cause and
repeating only affected checks. This command does not itself establish full
build/install provenance, default-server, theme, compiler, hardware or release
qualification. The [installed checkpoint](verification/VERIFICATION-2026-10-29-XDG-INSTALLED.md)
records those boundaries and the separate target-specific artifact proofs.


## Focused installed notification probe

The optional notification module can be checked with the current built core,
MFL, test client and capture helper. Build with `session-dbus=enabled`, install
the `notifications` tag into a private prefix, and keep the installed module and
its build from the same source revision. The fixture needs Python 3, a private
D-Bus daemon, Xvfb and the X11/XTest dependencies used by the build. Set the
normal runtime library path if that private build uses non-system libraries.
Use a new evidence directory from the matching source root:

```sh
python3 tests/live_x/notify_live.py \
    --build /path/to/build \
    --notify /path/to/private-prefix/libexec/fvwm3/VERSION/FvwmNotify \
    --execution-profile native-cpu \
    --artifacts-dir /path/to/new-notification-evidence
```

Choose `emulated-cpu` when the guest CPU is emulated. The label is supplied by
the caller; the result also records the observed OS and architecture. The
fixture uses private X and D-Bus resources, exercises actual installed-module
protocol and keyboard controls, and preserves failure evidence. It does not by
itself qualify a full installed desktop, physical hardware, assistive provider
or release profile. BSD default-server instructions are in [BSD.md](BSD.md);
the [installed acceptance report](verification/VERIFICATION-2026-10-30-NOTIFICATION-ACCEPTANCE.md)
records the scope and target-specific results.

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
three repetitions in alternating order. Keep `comparison.json` and compact
source/compiler/dependency receipts. Keep raw runs through review or unresolved
regression analysis, then retire them under the retention rule. Retain only the
runnable baseline and inputs needed for the next comparison, replacing its
predecessor after verification.
RSS is unavailable outside Linux `/proc`; CPU/memory and latency reflect the
recorded environment. Physical hardware remains separate qualification.

The installed-session observer is a separate opt-in acceptance tool:

```sh
python3 tests/live_x/desktop_session_live.py --observe-only \
    --prefix /path/to/stage/usr/local --version VERSION \
    --cgroup-parent /sys/fs/cgroup \
    --trace-parent /sys/kernel/tracing/instances \
    --artifacts-dir /path/to/new-session-observation
```

Use an owned Linux test guest and existing writable delegated cgroup/tracefs
parents. Both kernel options are optional; unavailable accounting remains null
with a reason. The helper creates fresh private resources and never mounts
tracefs, enables controllers or changes global tracing. The observer and X
server stay outside the measured cgroup. Preserve `ownership.json`, cleanup
receipts, raw proc snapshots, phase boundaries and scheduler trace alongside
`evidence.json` while the observation is under review or needed by an unresolved
acceptance check. Afterwards retain compact results and retire unneeded raw
host/guest artifacts under the retention rule.

Sampling starts just after core launch, covers readiness, twelve seconds of
idle (including regular five-second desktop status work), settings editor/query
activity and logout. PID/task start identities protect deltas and reparented
children; missing fields, identity races, sample gaps and limits remain visible.
RSS/PSS sums are samples across each poll, not simultaneous physical memory or
an unsampled peak. CPU is counted once per process; context switches include
all readable threads and are not wakeups. Optional cgroup CPU includes joined
short-lived descendants; `memory.peak` is cumulative charged memory, including
kernel/cache and excluding pages charged elsewhere, not RSS/PSS. Admission
starts immediately before exec. Private scheduler fork/exit attribution counts
selected-task wakeup tracepoint occurrences, separately from new-task wakeups;
loss, truncation, ambiguous birth or parse failure refuses complete counts.

`--gc-observe` selects a separate, longer resident `GODEBUG=gctrace=1` run.
It does not force collection. Worker stderr is outside that GC scope; absent
cycles are not zero pauses. Keep this instrumented workload out of the paired
latency comparison. Synthetic XTest-to-core Echo timing excludes physical input
and painting. These observations do not establish native-CPU or hardware/release
qualification. Pure contracts run through `make verify`; no rebuild or OS matrix
is implied by updating this observer.

Add `--workload-observe` for a fixed session activity checkpoint: 512 additional
fixture applications (513 DesktopEntry files exercise the existing 512-result
cap), 96 private HOME directories and eight discovery/search/files rounds.
Each discovery must complete through the existing operation API; search and
files menus must acquire and release their X grab. Three settings-editor cycles,
twelve settings queries and twelve final key-to-core Echo samples remain.
One input sample per discovery records the last observed operation state before
input; that snapshot does not prove the worker was still running when input
arrived. These synthetic timings exclude physical input and painting.

Run this workload once normally and, separately, once with `--gc-observe`.
Both use the same fixed activity. Keep the instrumented run out of latency
comparisons; never force GC, tune GOGC or increase load after seeing the result.
Zero observed resident cycles leaves pause latency unknown. Worker stderr stays
outside scope. Menu captures show existing UI with the private dataset, not new
theme qualification. The M2 acceptance handoff records existing qualified
subsets and the remaining native-CPU/owner acceptance separately.

The [native ARM64 session checkpoint](verification/VERIFICATION-2026-10-25-M2-NATIVE-ACCEPTANCE.md)
adds actual AArch64 ELF/process evidence and the existing installed session checks
under Apple hardware virtualization. It qualifies the exercised instruction paths,
not bare metal, native AMD64/Ubuntu-kernel/sanitizer coverage or physical/release
acceptance. Full-session owner acceptance remains separate. Historical emulated
observations retain their own environment and qualification flags.

The [process/session startup checkpoint](verification/VERIFICATION-2026-10-20-PROCESS-STARTUP.md) records six-target/ten-profile constructor qualification, real session integration, scoped guest sanitizers and unchanged inherited OpenBSD gaps. Its source/profile scope does not establish native-CPU or physical/release acceptance.

The [swallowed-client session checkpoint](verification/VERIFICATION-2026-10-20-SESSION-CALLBACKS.md) records composed six-target qualification, unchanged inherited OpenBSD gaps and scoped guest sanitizers. Native-CPU and physical/release acceptance remain separate.

The [core-record checkpoint](verification/VERIFICATION-2026-10-20-CORE-RECORD-BOUNDARIES.md) records affected six-target/ten-profile checks, exact allocated code/data and scoped install parity. Broader PR134 runtime/default-X/sanitizer evidence and inherited gaps retain their original scope.

## Focused standalone tray probe

Build the core, FvwmMFL, FvwmTray, `tray-client` and capture helper, then install
with the `tray` tag into a private prefix. No session bus is required. From the
matching source root, with Python 3, Xvfb and the existing X11/XTest dependencies:

```sh
python3 tests/live_x/tray_live.py \
    --build /path/to/build \
    --tray /path/to/private-prefix/libexec/fvwm3/VERSION/FvwmTray \
    --execution-profile native-cpu \
    --artifacts-dir /path/to/new-tray-evidence
```

Use `emulated-cpu` for emulated CPUs. Add `--xorg-config
tests/live_x/xorg-dummy.conf` where the private Xorg dummy profile is available
to check two-output scale/primary and work-area changes. The fixture owns its
private display and synthetic plug clients; `--display` instead requires a
caller-owned disposable display. Linux pidfd checks authenticate the exact
module lifetime before the deliberate abrupt-exit case; unsupported platforms
record that case unavailable. The probe does not establish real toolkit,
container/bar, full XEmbed, six-target or release qualification. See the
[source-bound checkpoint](verification/VERIFICATION-2026-10-30-XEMBED-TRAY.md).
