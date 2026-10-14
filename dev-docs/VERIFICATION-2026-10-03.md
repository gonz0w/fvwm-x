# Local verification — 2026-10-03

FVWM-X targets Linux, FreeBSD, and OpenBSD on AMD64 and ARM64. This
checkout has no Git remote. The results below are local execution evidence;
configured GitHub Actions jobs have not run.

## Results

| OS and CPU | Local runtime | Result |
| --- | --- | --- |
| Linux AMD64 | Podman, x86_64 CPU emulation | 30/30 tests; staged installation passed |
| Linux ARM64 | Native CPU in Podman Linux VM | GCC and Clang each 30/30; staged installations passed |
| FreeBSD 15.1 AMD64 | QEMU TCG, native FreeBSD kernel/toolchain | 21/21; staged installation passed |
| FreeBSD 15.1 ARM64 | QEMU HVF, native FreeBSD kernel/toolchain | 21/21; staged installation passed |
| OpenBSD 7.9 ARM64 | QEMU HVF, native OpenBSD kernel/toolchain | 21/21; staged installation passed |
| OpenBSD 7.9 AMD64 | QEMU TCG, native OpenBSD kernel/toolchain | 21/21; staged installation passed |

The AMD64 Linux compiler was GCC. ARM64 Linux exercised GCC and Clang.
BSD used the compilers supplied by each guest. Every BSD image or installer
was checked against its official SHA256 before boot. Guest SSH forwards
bind only to the host loopback address.

## Behavior exercised

Linux's real Xorg dummy suite crosses physical RandR outputs at 1.25x,
1.5x, and 2x. Geometry coverage includes 96 shaded round trips across all
eight shade directions, fixed-size clients, maximized clients, reserved
work areas, and six maximize/fullscreen restore round trips. The original
transition code failed the first added shaded case; the repaired code
passes. Native BSD Xvfb sessions cover 24 shade/unshade cases each with
real clients and FvwmMFL command delivery.

The resource-cache probe calls the production font and image loaders on
a real X display. Equal filename/font bytes in independent storage must
reuse the same object. Scale, generation, and each target dimension must
separate image entries; scale and generation must separate font entries.
Checks also cover actual raster/font sizes, reference counts, release,
and initialized bitmap metadata. Removing generation comparisons in the
isolated Linux copies made both cache tests fail; restoring the original
code made them pass.

## Changes and evidence

Shaded transitions now preserve full logical geometry before projecting
the collapsed destination frame. Maximized transitions initialize the
monitor rectangle before intersecting it with the work area. BSD builds
select the GNU iconv header and library together, avoiding a libc/ports
mismatch. Bitmap loading initializes `no_limit` on its fallback path.

Local logs and archives are retained under ignored directories:

- `builddir/codex-2026-10-03/`: Linux compiler logs, negative controls,
  and integration artifacts.
- `builddir/qemu-bsd/{freebsd,openbsd}/`: ARM64 guest evidence.
- `builddir/qemu-bsd/{freebsd-amd64,openbsd-amd64}/`: AMD64 guest evidence.

## Remaining release evidence

BSD Xvfb results establish native toolchain, Xlib/Xenocara compatibility,
module startup, resource loading, and installation. They do not establish
physical GPU acceleration, hotplug, mixed-density physical monitors,
external compositors, actual GTK/Qt toolkit feedback, or physical input
devices. Separate default-console Xorg checks cover server-generated
pointer input and Xlib ConfigureNotify feedback.
Native Linux sanitizer jobs are configured for both CPUs but remain
unexecuted here. A passing local matrix alone is not a release-ready claim.

### Default BSD Xorg servers

The dedicated-display session also ran against the standard BSD Xorg
server with default driver detection and no custom Xorg configuration:

| Server and CPU | QEMU graphics | Result |
| --- | --- | --- |
| FreeBSD AMD64 | UEFI framebuffer, scfb | 24/24 cases each at 1x and 1.5x |
| FreeBSD ARM64 | UEFI ramfb, scfb | 24/24 cases each at 1x and 1.5x |
| OpenBSD AMD64 | Graphics console, default VGA driver | 24/24 cases each at 1x and 1.5x |
| OpenBSD ARM64 | UEFI ramfb, wsfb | 24/24 cases each at 1x and 1.5x with fallback |

OpenBSD ARM64's wsfb server reports RandR 1.6 and a working 800x600 root
screen, but `xrandr --listmonitors` returns zero monitors. A direct WM
startup originally confirmed exit 101 in `FScreenInit`: the initial
topology could not be committed. This was separate from the passing
build/Xvfb gate.

The user approved a root-sized single-screen fallback. It retains the
RandR 1.5+ requirement, creates `FVWM-X-Root` only for an empty initial
topology, tracks cached root dimensions while in fallback mode, and yields
to real monitors. Failed queries and later empty queries after real
topology keep the last good snapshot. Native unit tests cover those
transitions, no-op generations, invalid root geometry, and allocation
failure. The refresh path adds no server round trip for root dimensions.

Testing explicit 1.5x scaling found a second issue: initial decoration
binding happened before placement selected a monitor, but fonts and
borders changed after the first frame was created. The first unshade
shrunk a 200x120 client to 196x105. Initial window creation now binds the
owning monitor's resources before creating the frame and recording logical
client dimensions. Native wsfb and Linux nested-X cases preserve the
client dimensions through all eight shade directions at 1.5x.
The final geometry setup also keeps the already bound fonts rather than
unloading and reopening them after frame creation.

Linux CI now includes both 1x and 1.5x zero-monitor startup scenarios,
using the production topology code with a wrapped RandR query on Xvfb.

FreeBSD's scfb driver emits a harmless gamma diagnostic before monitor
records. The test harness records it and excludes only that known message
from strict monitor parsing. Linux Xorg transition coverage still passes.

GSD workflow enforcement was removed at the user's request. Historical
planning files remain available; the Codex harness and these verification
entry points govern ongoing work.

### Interactive geometry and resize feedback

The Linux interactive Xorg gate generates real button/motion events with
XTest. It holds the button through four monitor crossings for ordinary
and fixed-size clients at each of 1.25x, 1.5x, and 2x: 24 crossings in
total. Client dimensions, the scaled pointer anchor, the retained WM grab,
and grab release are observed through Xlib. A client also echoes actual
ConfigureNotify dimensions through genuine ConfigureRequests in normal,
maximized, and fullscreen states; later genuine resizes must still update
its retained size. This is controlled Xlib feedback, not GTK/Qt evidence.

Opaque resizing exposed a retained-size bug: a visible 200x120 client grew
to 280x180, then shrank to 250x150 on a 1.25x monitor because its old normal
size was still cached. Accepted Resize and ResizeMove operations now
record the new normal dimensions at the owning scale. The regression also
covers command resizes, shaded resizes, and ResizeMaximize/
ResizeMoveMaximize preserving the normal restore size. This adds no X
round trip or blocking work to the resize completion path.

All four default BSD Xorg servers passed seven interactive scenarios each
at 1x and 1.5x, including wsfb's root fallback. These exercise real server
grabs and visible resizing on a single screen; mixed-scale physical RandR
crossings remain Linux Xorg coverage. FreeBSD needed the libXtst package;
OpenBSD's Xenocara sets supply it.

Python integration tests now declare their executable dependencies in
Meson, ensuring direct `meson test` rebuilds changed WM/probe binaries.
Overlapping local Linux verification jobs exhausted the 2 GiB Podman VM
and killed Xorg processes. Those runs are invalid evidence; final Linux
gates run sequentially after recovery with two compiler jobs.


### Responsiveness profiling and scheduler starvation

A new end-to-end profiler sends commands through the real FvwmMFL socket,
waits for the returned Echo packet, and alternates immediate commands with
10 ms scheduled commands. A separate process and X connection generate
paced title changes or ConfigureRequests at a requested 10,000 requests/s.
Worker readiness is acknowledged explicitly before sampling. CPU percentage
uses Linux process accounting; BSD reports it as unavailable. Each run keeps
raw samples, achieved workload rate, and process/server logs.

The original scheduler used the most recent timestamp-bearing X event as
its clock. ConfigureRequest events have no timestamp, so a resize flood
prevented a 10 ms scheduled command from completing within the profiler's
five-second deadline. Deadlines now use CLOCK_MONOTONIC, with rounded-up,
clamped descriptor timeouts. Executing scheduled work no longer requests
an X server timestamp. Periodic jobs rearm after execution without catching
up missed intervals. Native unit tests cover elapsed deadlines, immediate
execution, timeout rounding, and integer limits.

With the timer fix, reducing the X event budget from 64 to 16 makes descriptor,
module, and timer work run more frequently under an X event backlog. A repeated
local Linux ARM64 GCC comparison, with 64 samples per command type and mode,
measured the following resize-flood round trips:

| X events before polling | Immediate median / p95 | Scheduled 10 ms median / p95 | WM CPU |
| --- | --- | --- | --- |
| 64 | 15.91 / 18.02 ms | 25.32 / 32.00 ms | 31.59% |
| 16 | 2.58 / 6.83 ms | 14.51 / 16.04 ms | 30.61% |

An earlier comparison also reduced the immediate median from 14.76 ms to
2.28 ms with similar CPU use. These are local VM measurements, not physical
hardware guarantees. The repository now uses a 16-event budget. The Linux
`scheduler-responsiveness-xvfb` regression requires completion within a loose
five-second socket deadline; it does not gate on benchmark percentiles.

Profiling also exposed signed narrowing of extended MFL event flags on
64-bit systems. The matcher now preserves unsigned wire width and distinguishes
extended event types from overlapping ordinary-event bits. Native tests cover
those collisions; geometry integration checks verify a real socket Echo reply.

Meson configuration generation now preserves config.h when its contents have
not changed. A reconfiguration adding a test rebuilt only the affected test
and module rather than recompiling every source file.

All four native BSD default Xorg servers completed the three responsiveness
workloads with eight immediate and eight scheduled replies per workload,
using the final 16-event batch and the worker readiness handshake. Evidence
is saved in each guest's `responsiveness-evidence.tar.gz` beside the earlier
resize evidence. The guests were shut down cleanly after copying the logs.

The starvation regression was also checked against the original scheduler in
an isolated Linux ARM64 build with the final 16-event batch. It failed after
5.56 seconds with a socket timeout during resize load. Restoring the monotonic
scheduler made the same regression pass in 0.68 seconds. Linux measurements,
compiler gates, staged-install logs, and the negative control are archived in
`builddir/codex-2026-10-03/responsiveness/{arm64,amd64}-evidence.tar.gz`.

The next responsiveness design decision concerns modal Move/Resize input loops.
They currently wait on selected X input events rather than servicing the
normal module/timer multiplexer. Servicing background commands during a drag
requires explicit lifetime and geometry guards for the dragged window.
