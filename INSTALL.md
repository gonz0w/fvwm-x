Installation Instructions
=========================

FVWM3 uses Meson as its build tooling. `./build` preserves its documented
short options and delegates to `tools/maintenance/build.py`, using Python's
standard library. Direct Meson use is supported. Documentation targets share
one manual manifest with the standalone renderer; other maintenance tools
remain explicit candidates. See [their guide](tools/maintenance/README.md).
Verification runs locally; GitHub Actions is disabled. Release publishing remains
a separate maintainer decision.

Dependencies
============

Docker
======

The `fvwm3` repository has a Dockerfile which can be used to build the
repository as an optional local environment. Inherited container references
require their own provenance review; they are not passing validation evidence.

Manually
========

FVWM3 has the following dependencies.  Note that across different
distributions, the development package names will differ.  The names listed
below are examples to help you find the appropriately named package for the
system in use.

## Core dependencies

* meson (>= 1.5.1)
* ninja-build
* pkg-config
* python3
* perl
* sed
* libevent-dev (>= 2.0)
* libfontconfig-dev
* libice-dev
* libx11-dev
* libxext-dev
* libxft-dev
* libxrandr-dev (>= 1.5)
* libxt-dev
* xtrans-dev

XKB key translation comes from libX11. Configuration checks its
`X11/XKBlib.h` header and `XkbKeycodeToKeysym` symbol together; libxkbcommon
is not a direct build or runtime requirement. Keep Xtrans (session transport
headers) and Xt (X11 type headers) installed.

Go is a required **build** dependency: stable 1.26.9 or newer in the 1.26
series, or stable 1.27.2 or newer. It builds FvwmPrompt, FvwmCommand, the
XDG/directory/xlock menu utilities, fvwm-file-browser and the installed Script
communication helper. Put the selected SDK's `bin` first in `PATH`. An absent,
old or prerelease compiler fails configuration; the disabled-Go profile has
been removed. Use a new build directory when an older Meson directory retains
the removed `golang` option; some Meson versions reject that cached option.
Configuration uses `GOTOOLCHAIN=local`; all utility builds use
vendored inputs with module/toolchain downloads disabled. Go is not required
on machines running the installed executables. The module's `go 1.21` directive
sets the source language, not the supported compiler baseline. See the
[approved baseline](dev-docs/decisions/GO-TOOLCHAIN-BASELINE-PROPOSAL.md).
Use a checksum-verified private SDK when OS packages lag; system packages need
not change. Bootstrap/install the SDK explicitly before configuring FVWM-X. Disposable Linux
jobs can use `sh dev-docs/ci/linux-go-sdk.sh` followed by
`. builddir/go-sdk-environment.sh`; BSD jobs use `bsd-go-sdk.sh` and
`builddir-bsd/go-sdk-environment.sh`. These explicit preparation recipes fetch
fixed checksum-verified SDKs; configure/build never performs that fetch.

## Optional dependencies

* asciidoctor
* libfreetype6-dev
* libfribidi-dev
* gettext runtime/development files and `msgfmt` (for NLS)
* libpng-dev (>= 1.6.59, or a verified vendor security backport)
* librsvg-dev
* libsm-dev
* libxcursor-dev
* libxfixes-dev
* libxpm-dev
* libxrender-dev

The optional `fvwm-file-browser` is a development consumer of FvwmForm/FvwmMFL;
it does not replace the default desktop Files menu. See the
[development screenshots and configuration examples](doc/development-preview.md)
and [browser manual](doc/fvwm-file-browser.adoc) for setup and current limits.

Every build installs the coordinated `fvwm-menu-desktop` provider and native
FvwmForm settings frontend. They need no Go, PyXDG, Pillow or ImageMagick at
runtime. Use `PipeRead 'fvwm-menu-desktop --configure'` in FVWM configuration.
The existing `Module FvwmPerl -l fvwm-menu-desktop-config.fpl` entry point is a
small adapter to that native owner; the remaining FvwmPerl framework is still
installed and tracked separately. Other Python/Perl runtime controllers and
build/developer tooling have their own retirement work; this change does not
make the entire distribution interpreter-free. M2's first slice installs the
private Go `fvwm-desktop-worker` beside FvwmDesktop for application discovery,
metrics and audio/power jobs. The controller and native screenshot boundary
still use Python; full controller/session acceptance remains open.

The pre-FVWM3 `fvwm-convert-2.6` converter is no longer built or installed.
FVWM3 configuration files remain supported. Migrate older configuration with
an older FVWM release before using FVWM-X; user files are never rewritten.
When upgrading an existing prefix, remove the old converter and
`share/fvwm3/fvwm-script-ComExample.pl` from that prefix using its previous
package manifest. A fresh stage has neither file; installing over a prefix does
not automatically delete files owned by an older package.

Native language support
=======================

`-Dnls=auto` enables translated UI when the gettext library and `msgfmt` are
available. If `msgfmt` is missing, configuration warns and disables NLS.
`-Dnls=enabled` requires both dependencies and fails if either is missing.
`-Dnls=disabled` builds without translated UI or catalog build/install targets;
it does not require gettext tools. Configuration summary reports the selected
support, rather than library availability alone.

Meson compiles the eleven catalogs listed in `po/LINGUAS` from `.po` sources.
Committed `.gmo` files are not build inputs. Catalog compilation omits fuzzy
translations. Template extraction and translation updates additionally need
`xgettext`, `msguniq`, `msgmerge` and `msginit`; see [po/README.md](po/README.md).

Image input limits
==================

Primary image files must be regular files; symlinks to regular files work.
The default source cap is 128 MiB. Set `-Dimage-input-limit=N` to select 1–1024
MiB. Compressed XPM input has a separate 128 MiB expanded cap and a 5-second
decompression deadline. Set `-Dimage-xpm-expanded-limit=N` (1–1024 MiB) and
`-Dimage-decompress-timeout=N` (1–60 seconds) to change these finite defaults.

Meson resolves the existing system `gzip` executable. Use
`-Dimage-gzip=/absolute/path/to/gzip` for an explicit trusted executable.
Uncompressed XPM still works without it.

SVG decoding uses the installed private `fvwm-image-svg-worker` executable.
The default total deadline is 5 seconds and the worker memory limit is 1024
MiB. Set `-Dimage-svg-timeout=N` (1–60 seconds) and
`-Dimage-svg-memory-limit=N` (128–4096 MiB) to change these finite defaults.
Linux and FreeBSD use the address-space limit; OpenBSD uses the data limit.
Stricter inherited limits remain in force. The worker refuses startup if it
cannot set and verify its limits. Relative SVG images and rendering options
retain their existing behavior within these bounds.

Development or relocated installations can set `FVWM_IMAGE_SVG_WORKER` to the
absolute path of a trusted worker built for the same FVWM version. The normal
installed path is compiled in. No shell or executable search path is used.

Core SVG, compressed XPM and ambiguous image requests complete asynchronously.
The core retains the previous image until its replacement succeeds; a first
load has no image while pending. Other image callers keep their existing
completion behavior. Processing remains isolated and bounded.
The worker is not a file-system sandbox. These bounds do not guarantee storage
latency or limit CPU time in other image decoders. See the installed `fvwm3(1)`
manual for runtime behavior.

The private worker also supports software colorset color-image effects when
SVG support is disabled. Preparation uses `-Dimage-effect-timeout=N` (1–60
seconds, default 5) and `-Dimage-effect-memory-limit=N` (128–4096 MiB, default
1024). The deadline starts at worker launch and covers X connection, readback,
color queries, blending and output. Queue wait and bounded parent upload are
separate. The same verified OS limits apply. `FVWM_IMAGE_EFFECT_WORKER` can
select an absolute trusted path to the matching worker for this operation.
The worker queries retained X resources; it does not allocate or change them.
It is not an X permission sandbox. Normal core startup enables asynchronous
preparation and acknowledged publication for file-backed color colorset
effects. Hardware rendering retains its existing arithmetic. Software rendering
uses queried server colors and its existing rounding. Combined platform
qualification is recorded in the image activation report under
`dev-docs/verification/`; release CI and physical checks remain separate.

PNG security baseline
=====================

PNG-enabled builds require libpng 1.6.59 or newer by default. An older found
library stops configuration, including automatic detection. Supply a patched
library to preserve PNG theme images. `-Dpng=disabled` explicitly omits PNG.

For an older library with verified vendor fixes, use
`-Dpng-security-backport=true`. This flag records the builder's assertion; it
does not check vulnerabilities or apply fixes. Record the exact package version
and vendor advisory in the build profile. Do not use it for an unpatched or
unverified package. The legacy API floor of 1.2 applies only with this assertion.
See the [dependency audit](dev-docs/DEPENDENCY-AUDIT-2026-10-05.md).

The Linux/BSD verification entry points honor an explicit `PKG_CONFIG_PATH`
before system paths. A private patched library also needs its runtime library
path, such as `LD_LIBRARY_PATH`, when running staged binaries. The entry points
clear cached dependency lookups on reconfiguration. To assert an audited vendor
backport with these entry points, set `FVWM_PNG_SECURITY_BACKPORT=true`; its
default is false. Record both library provenance and actual loaded paths.
The existing CI dependency commands may still supply older packages: those jobs
will fail configuration until their verified patched profile is provisioned.

Generating documentation
========================

Documentation generation is opt-in. Man pages are the recommended OS-install
output; HTML is an optional static export from the same AsciiDoc sources.
Neither format needs a new desktop runtime service. See the
[documentation policy](dev-docs/DOCUMENTATION.md).

To generate `fvwm3`'s documentation:

1. Install `asciidoctor`
2. To generate manpages:  pass `-m` to `build`
3. To generate HTML docs: pass `-w` to `build`

To compile both at once, pass `-d` to `build` (which implies `-m` and `-w`)

Installing From Git or a Release Tarball
===================

Compiling `fvwm3` is best used with the `./build` wrapper script.

```
./build -Dhtmldoc=false -mi
```

This builds, generates man pages, and installs. HTML remains disabled unless
explicitly enabled; use a separate build directory when reviewing another batch.

It is best to see the output from `./build -h` to understand its options,
but some common usages are:

```
# Optional: build both man and HTML pages
$ ./build -d

# Build and install
$ ./build -i

# Build with man pages and install
$ ./build -Dhtmldoc=false -mi

# Enable specific options (see `meson.options`), compile docs and install
$ ./build -Dbidi=disabled -Dhtmldoc=false -mi
```

The wrapper also accepts `--dry-run`, `--test`, `--jobs N` and `--log PATH`.
It does not install without `-i`. Changed `-D` options reconfigure the chosen
build without wiping it. Repeated identical scalar options reuse the configured
build; arrays, unknown options or missing option metadata use ordinary setup.
`--reconfigure` forces setup and `-f` explicitly requests wipe/reconfigure.
Tests retain their prerequisite build, including Go test binaries
excluded from the default build. Installation uses `--no-rebuild` after
compilation/testing. A compile or test failure prevents installation. Keep one owned
build directory per compiler/feature profile and refresh dependency detection
when dependency inputs change.
Verbose mode returns a failed command's status, and quiet failures point to
retained diagnostics. Values containing spaces remain one argument:

```sh
./build -b 'build with spaces' -p '/prefix with spaces' --dry-run
./build -b builddir-local --jobs 2 --test
```

Direct Meson use
================

For an isolated build without installation:

```sh
meson setup builddir-local -Dbidi=disabled
meson compile -C builddir-local
meson test -C builddir-local --print-errorlogs
```

Dependency detection and feature options are authoritative in `meson.build`
and `meson.options`. Package names above vary by OS. Build and optional
maintenance dependencies are separate from the desktop services discovered
at runtime; the default desktop uses the existing FVWM components and Python
standard library.

Native BSD verification
=======================

FreeBSD uses Xorg from packages; OpenBSD uses the X11 libraries and server
from its Xenocara sets. Install the build dependencies listed in
`.github/workflows/bsd.yml`, including Xvfb and DejaVu fonts, then run:

```
sh tests/run_on_bsd.sh
```

This builds the WM and modules, runs native policy tests and a nested Xvfb
session, and stages installation under `builddir-bsd/install-stage`. The
session checks all eight shade directions for ordinary, fixed-size, and
maximized clients. It requires no running desktop and does not install into
the system. Go and documentation generation are disabled for this gate.

The BSD gate checks native build and X session compatibility. Mixed-DPI
transitions and topology changes also require the Linux Xorg dummy tests;
physical monitor hotplug and compositor behavior still need desktop testing
on each target BSD. CI runs both BSD gates in virtual machines.

The release targets AMD64 and ARM64 on Linux, FreeBSD, and OpenBSD. OpenBSD
follows new releases; 8.0 is the current qualified baseline and 7.9 is out of
scope. Update fixed inputs and qualify both architectures before claiming a
newer release. The configured 8.0 VM profiles are locally qualified. See
[the support matrix](dev-docs/SUPPORT.md) for required gates and
[the BSD verification guide](dev-docs/BSD.md) for local QEMU use.

The optional Meson setting `-Dcolorset-limit=65536` raises the per-process
colorset count limit above the default 4096. The core and bundled modules use
the same configured limit (1 through 65536, including set zero). Sparse IDs
preserve large numeric theme references. This setting bounds the number of
colorset records; images and X resources have separate costs.
