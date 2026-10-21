# Native BSD verification

The release targets are FreeBSD 15.1 and OpenBSD 8.0 only. Both OpenBSD 8.0
VM profiles pass the [baseline checkpoint](verification/VERIFICATION-2026-10-20-OPENBSD-80.md);
retained 7.9 results are historical. ARM64 guests
run under hardware-accelerated QEMU on the macOS development host; AMD64
guests use QEMU CPU emulation on the same host.
The approved release matrix is Linux, FreeBSD, and OpenBSD on both AMD64
and ARM64, initially using current BSD releases. Verification runs locally;
GitHub hosts code and review with Actions disabled. The owned QEMU recipes
cover all four BSD profiles. Record exact source, guest, patch and feature
identities separately from native-CPU and physical-hardware release coverage.

Run `sh tests/run_on_bsd.sh` inside a guest with this source tree. It builds
the core and all modules, runs policy tests and a native Xvfb session, then
checks a staged installation. It does not install into the guest's system.
The X session maps real Xlib clients and checks ordinary, fixed-size, and
maximized shade/unshade behavior in all eight directions. A second Xvfb
test exercises actual font and image cache lookup, scale and generation
separation, target dimensions, reference counts, and bitmap metadata.

## Guest dependencies

On FreeBSD:

```sh
pkg install -y meson ninja pkgconf python3 perl5 libevent fontconfig \
    iceauth libICE libX11 libXext libXft libXrandr libXt libxkbcommon xtrans \
    libXcursor libXrender png librsvg2-rust cairo xorg-vfbserver xrandr dejavu
```

On OpenBSD, install the compiler and all Xenocara sets during installation,
then:

```sh
pkg_add -I meson ninja libevent libxkbcommon png librsvg
```

OpenBSD supplies pkg-config and DejaVu fonts in its base/Xenocara sets.
Both systems supply GNU libiconv through the rendering dependency chain.
Meson must select the ports header and library together: FreeBSD also has
libc iconv, and its builtin probe does not see `/usr/local/include`.

## Current release recipes

Use the [owned QEMU recipes](ci/README.md) for fresh pinned release inputs.
OpenBSD profiles now select 8.0 and refuse a different release/kernel identity.
For signed system-update preparation, run `bsd-prepare-core.sh`, then
`bsd-prepare-x-startup.sh` and normal signed `pkg_add -u` in the job's preparation
script; pair it with `--reboot-after-prepare`. The
[8.0 checkpoint](verification/VERIFICATION-2026-10-20-OPENBSD-80.md) preserves
that exact executed script, source identities and results on both architectures.
AMD64 default-X fixtures also require the explicit `--openbsd-vga-test` option.

## Retained local lab examples

The paths below describe the earlier installed lab layout. Its OpenBSD disks
contain 7.9 and are retained historical evidence, not 8.0 release inputs.
Use the current recipes above for OpenBSD qualification; the native gate now
refuses 7.9. Preserve these older disks and their associated builds.

The verification images, private VM key, environment records, and test
archives are under the ignored `builddir/qemu-bsd/`. Official image SHA256
checksums were verified before boot. The guests each use a new 32 GiB
virtual disk and 4 GiB RAM. ARM64 guests use four virtual CPUs and AMD64
guests use two. SSH forwards bind only to loopback: ARM64 FreeBSD port
2241 and OpenBSD port 2242; AMD64 FreeBSD port 2251 and OpenBSD port 2252.

From the project root, restart the installed FreeBSD guest with:

```sh
qemu-system-aarch64 -machine virt,accel=hvf -cpu host -smp 4 -m 4096 \
    -bios /opt/homebrew/share/qemu/edk2-aarch64-code.fd \
    -drive if=virtio,format=qcow2,file=builddir/qemu-bsd/freebsd/FreeBSD-15.1.qcow2 \
    -nic user,hostfwd=tcp:127.0.0.1:2241-:22 -nographic -monitor none
```

For the installed OpenBSD guest, use the same command with the disk
`builddir/qemu-bsd/openbsd/disk.qcow2` and SSH port 2242. Do not attach
installation media when booting the installed system.

FreeBSD AMD64 default scfb checks require UEFI's framebuffer. Boot its installed
image with an explicit disk boot priority (a BIOS boot supports Xvfb but supplies
no scfb screen):

```sh
qemu-system-x86_64 -machine pc,accel=tcg -cpu max -smp 2 -m 4096 \
    -drive if=pflash,format=raw,readonly=on,file=/opt/homebrew/share/qemu/edk2-x86_64-code.fd \
    -drive if=pflash,format=raw,file=builddir/qemu-bsd/freebsd-amd64/efi-vars.fd \
    -drive if=none,id=systemdisk,format=qcow2,file=builddir/qemu-bsd/freebsd-amd64/FreeBSD-15.1.qcow2 \
    -device virtio-blk-pci,drive=systemdisk,bootindex=1 \
    -nic user,hostfwd=tcp:127.0.0.1:2251-:22 \
    -device qemu-xhci -device usb-kbd -device usb-tablet -vga std \
    -nographic -monitor none
```

OpenBSD AMD64 uses `qemu-system-x86_64 -machine q35,accel=tcg -cpu max`,
its `openbsd-amd64/disk.qcow2` virtio disk, and port 2252, with the same VGA
and USB input devices. It boots through BIOS with the configured pc0 console.

To expose a framebuffer and input devices for standard ARM64 Xorg checks,
add `-device ramfb -device qemu-xhci -device usb-kbd -device usb-tablet`.
FreeBSD also needs `pkg install -y xorg-server xf86-video-scfb`. Its default
Xorg driver detection uses scfb successfully on both architectures.
OpenBSD AMD64 needs a graphics console (`set tty pc0` in `/etc/boot.conf`)
for its VGA device; a serial-only boot does not configure wsdisplay.

On a dedicated X display with no existing WM, run the native session probe:

```sh
python3 tests/live_x/geometry_transition_live.py --smoke --display :97 \
    --fvwm builddir-bsd/fvwm3 \
    --mfl builddir-bsd/modules/FvwmMFL/FvwmMFL \
    --artifacts-dir builddir-bsd/tests/live-x-artifacts/default-xorg
```

Use the installed versioned Python executable on OpenBSD. The external
display is managed by the caller. OpenBSD ARM64 wsfb reports zero RandR
monitors; FVWM-X handles this with its root-sized single-screen fallback.
Use `--smoke-quarters 6` to also check explicitly configured 1.5x scaling.
Add `--interactive` for server-generated pointer drags, opaque resizes,
ConfigureNotify feedback, and command resize/restore checks. This mode
requires libXtst (`pkg install -y libXtst` on FreeBSD; Xenocara supplies it
on OpenBSD). The default-server checks passed on both BSD architectures
at 1x and 1.5x; each run retains frame/client observations and WM logs.

Measure command and timer responsiveness on the same dedicated display:

```sh
python3 tests/live_x/responsiveness_profile.py --display :97 \
    --fvwm builddir-bsd/fvwm3 \
    --mfl builddir-bsd/modules/FvwmMFL/FvwmMFL \
    --artifacts-dir builddir-bsd/default-responsiveness
```

The profiler exercises idle, title-change, and resize-request workloads.
It records raw round-trip samples and achieved request rates. CPU accounting
is available on Linux; BSD records timings without a CPU percentage. Omitting
`--display` starts a private Xvfb server. The Linux integration gate uses a
short run to catch starvation without enforcing benchmark percentiles.

The guest source directory is `/root/fvwm-x`. To run a gate again:

```sh
ssh -p 2241 -i builddir/qemu-bsd/id_ed25519 \
    -o UserKnownHostsFile=builddir/qemu-bsd/known_hosts root@127.0.0.1 \
    'cd /root/fvwm-x && sh tests/run_on_bsd.sh'
```

Transfer changed source files before testing; the guest source is a copy,
not a live mount. Use port 2242 for OpenBSD. Shut guests down through SSH
after verification. A QEMU process whose OS has halted can then be stopped.

## Independent server startup checks

Use the [local startup recipes](ci/README.md) to separate an Xorg startup
failure from installed WM/client behavior. On an isolated existing guest:

```sh
python3 tests/run_bsd_default_x.py --server-only --repeat 8 \
    --disposable-vm --artifacts-dir builddir-bsd/server-startup
```

Each attempt has its own result and authentication. One failure fails the
aggregate; later successes retain their own evidence. The updated-image
comparison uses signed system updates and an explicit verified reboot in a
fresh owned image. It preserves retained guests, earlier failed evidence and
the configured default driver. No FVWM configuration changes are needed.

## Coverage limits

Linux Xorg dummy tests exercise actual monitor crossings at 1.25x, 1.5x,
and 2x. They cover 96 shaded round trips, reserved work areas, and six
maximize/fullscreen restore round trips. The native BSD gate checks the
BSD toolchain, libraries, module startup, X server operation, and install
layout. Separate default-server probes also exercise pointer grabs and
client resize feedback. They do not establish physical GPU, monitor
hotplug, compositor, real GTK/Qt toolkit behavior, or physical input devices.
