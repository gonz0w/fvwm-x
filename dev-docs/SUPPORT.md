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

Configured CI jobs are not passing evidence. The checkout currently has no
Git remote, so local logs and CI execution must be reported separately.
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
The [2026-10-04 verification](VERIFICATION-2026-10-04.md) adds bounded MFL stream
transport and configure-request regression evidence to the existing matrix.
