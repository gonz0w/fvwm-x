# Local verification recipes and historical CI candidates

Owner: CI-01. The maintainer selected [local-only verification](../decisions/LOCAL-REVIEW-GITHUB-2026-10-13.md).
GitHub Actions is disabled; inherited workflows are archived. YAML candidates
remain inactive historical design inputs, not a queue for workflow adoption.
The shell/Python recipes and verified QEMU runner remain local developer tools.
Publication, release tags and credentials retain separate ownership.

[Action pins](ACTION-PINS-2026-10-12.json) record resolved commit identities and
SHA256 hashes of reviewed action entry points, package metadata and BSD release
configuration. The official repositories supply each source and tag record.
The review checks runner requirements, job permissions, checkout credentials,
architecture/release inputs and how VM runtimes are obtained. It is not a complete
review of bundled JavaScript or certification of transitive dependencies.

The [Linux candidate](linux-architectures.candidate.yml) preserves native
AMD64/ARM64 GCC/Clang, separate ASan/UBSan jobs and a Go client profile.
Standard and sanitizer jobs
use the same portable native setup and feature flags with the approved patched
PNG source build.
The [BSD candidate](bsd.candidate.yml) now uses the approved repository-owned
[QEMU runner](../../tools/maintenance/qemu_bsd.py), with four fixed image profiles
and separate core/Go jobs. The original action/AnyVM candidates remain historical
review inputs in their pin receipts; their runtime and cache lifecycle are no
longer used by this inactive candidate. [Current asset pins](ASSET-PINS-2026-10-20.json) select OpenBSD 8.0 only;
the original dated locks retain historical inputs. The runner consumes only the selected
image/profile, plus the independently verified [bootstrap key and ARM firmware
pins](BOOT-PINS-2026-10-20.json). It does not execute downloaded launch scripts.
Two inherited container identities remain observations, not contents certification.

The [asset preflight](../../tools/maintenance/ci_assets.py) now materializes
these fixed inputs through one verified acquisition path. It checks SHA256 and
size on fresh downloads and every cache hit, refuses unsafe cache entries, and
publishes independent private files only after the complete selection passes.
Downloads run in a reaped worker with a total deadline of at most 300 seconds.
All four BSD profile selections and the thirteen-input selection use the
existing lock; no downloaded Python, launch script or image is executed.

```sh
python3 tools/maintenance/ci_assets.py --profile freebsd-amd64 \
    --cache builddir/ci-assets/cache --output builddir/ci-assets/freebsd-amd64
# A fresh output directory is required, including when reusing verified cache:
python3 tools/maintenance/ci_assets.py --profile freebsd-amd64 \
    --cache builddir/ci-assets/cache --output builddir/ci-assets/offline --offline
```

The cache directory must belong to the caller and have mode 0700. Workspace
parent paths, committed job scripts and the source checkout are trusted; these
helpers are not privileged services for arbitrary callers. The runner snapshots
source/scripts privately and records the exact input hashes. Job scripts run
from files with process stdin at EOF; child programs cannot consume later shell
commands from a shared `sh -s` input stream. Every job expands a
new disk from verified compressed bytes; prepared/expanded disks are never reused.
Physical expansion is capped at 64 GiB and virtual disk geometry at 256 GiB.
Source archives accept only bounded regular files/directories, with no links,
special files or path traversal. The compressed source cap is 256 MiB, individual
members 128 MiB, aggregate data 4 GiB and member count 100000.

QEMU uses fixed arguments, one loopback-only SSH forward and an owned console
collector. Initial host-key acceptance applies only to the owned disposable
instance and its private host record. The published image bootstrap key is then
replaced by a generated job key; subsequent SSH requires that host record and
key. Generated authentication is removed after success, error or cancellation.
OpenBSD job storage lives on `/home`, with its conventional `/root/fvwm-ci`
source entry retained as a symlink inside the disposable guest.

Extraction has a 300-second deadline. Boot defaults to 600 seconds (at most
1800), preparation to 7200, and the job script to 7200 (at most 21600).
Collection is capped at 2 GiB and 180 seconds; logs/console at 64 MiB per sink.
Full regular-file sinks are refused, including ignored short writes at the hard
limit. The runner reaps only its own children and VM; it never kills another
chat's guests. Signals enter the same cleanup path. Artifacts exclude disks,
input key contents and generated private authentication.

```sh
python3 tools/maintenance/qemu_bsd.py --profile freebsd-arm64 \
    --source-archive /tmp/reviewed-source.tgz \
    --prepare-script dev-docs/ci/bsd-prepare-go.sh \
    --run-script dev-docs/ci/bsd-run-go.sh \
    --cache builddir/ci-assets/cache --png-bootstrap --output builddir/ci-job
```

The runner has affected local qualification on six targets, actual failure and
cancellation controls, and authenticated default-X startup probes on all four
provider setups. These are not installed core/client build gates.

The OpenBSD AMD64 default-X fixture retains its approved legacy VGA hardware
and default VESA driver, whose int10 initialization needs aperture access.
Ordinary jobs keep the image's setting unchanged. The explicit `--openbsd-vga-test` option is restricted to
that disposable profile: set guest `machdep.allowaperture=2`, power down cleanly,
boot the same owned disk, verify the value, then run the default-X fixture. Only
the candidate's OpenBSD AMD64 Go/default-X job opts in. This is an isolated test
setup, not a FVWM or host security default. See the
[OpenBSD aperture manual](https://man.openbsd.org/xf86.4) and
[provider report](../verification/VERIFICATION-2026-10-13-QEMU-PROVIDER.md).

The user approved the [fixed PNG source bootstrap](../decisions/CI-PNG-INPUT-PROPOSAL-2026-10-13.md).
Its fresh-provider standard/Go/install/default-X checkpoint is locally qualified;
see the [exact source scopes and retained corrections](../verification/VERIFICATION-2026-10-13-CI-PNG-BOOTSTRAP.md).
Actual native release CI and workflow adoption remain separate. With `--png-bootstrap`, the
runner acquires upstream libpng 1.6.59 through the finite verified cache/download
path. The original xz and exact tar payload have separately fixed hashes. Only
the host needs xz support; the guest receives the verified uncompressed tar.
Preparation builds and tests PNG in an owned prefix, retaining source/license,
compiler and installed-library identities. Build recipes source the job-local
loader environment. OpenBSD's existing 18.2 library name is mapped privately
only after checking its exported PNG API. The bootstrap executes an eagerly
bound PNG/librsvg probe. Consumer checks compare actual loaded PNG hashes with
the build receipt: the standard profile checks built and staged core/workers;
the Go profile checks built core/workers and stages its client family. The Go
recipe also runs default-X command, prompt, xlock, directory, Script helper and
desktop-menu fixtures. System packages and production loader settings stay
outside this bootstrap.

In the earlier provider trials, OpenBSD's streamed Meson and Go packages ended
early. The Meson bootstrap still completes its ordinary signed package before
`pkg_add` reads it, with finite file limits and archive integrity checks. The
older Go package path is retired after approval of the patched compiler baseline.

Go profiles now use [bsd-go-sdk.sh](bsd-go-sdk.sh) for official Go 1.27.2 SDKs
on both BSD systems/architectures. Fixed archive sizes/SHA256 come from the
[official Go download metadata](https://go.dev/dl/?mode=json), recorded with the
owning dependency evidence. This is explicit verification preparation, not a
configure/build-time fetch. The recipe accepts an optional existing regular
archive for offline preparation, checks exact size/hash before extraction,
refuses an existing SDK directory, and leaves system/earlier SDKs intact.
Fresh acquisition has finite base-tool socket waits, three attempts and a file
limit; the provider's overall preparation deadline still applies. No separate
hard total-download deadline is claimed by this shell recipe.

The job's `builddir-bsd-go-sdk` is outside the collected build trees. Its
version, archive/compiler/license identities, license and local environment file
are retained under `builddir-bsd`; the run recipe selects that same SDK. This
keeps large compiler bytes out of the 2 GiB evidence collection. Standard/core
profiles still disable Go. Native SDK/affected client qualification does not
repeat or relabel the earlier fresh-image PNG/package checkpoint.

For an intermittent default-server failure, use server-only checks before
attributing it to a client. `--server-only --repeat 8 --disposable-vm` needs
no build directory and starts no WM or application fixture. Repetition accepts
1–32 independent attempts with private authentication and separate evidence;
any failed attempt fails the whole run, even if later attempts succeed.
Cancellation stops repetition. Normal fixture modes still require their build
directory and accept one invocation. Results record the failing phase, server
status before/after teardown, installed executable/module hashes and OpenBSD's
installed patch inventory. A stale vendor log is not copied as new evidence;
authentication files and process cores are excluded.

The OpenBSD comparison recipes below apply signed vendor updates only inside
the runner's new disposable image. Preparation waits finitely for the system's
kernel-reordering task; it does not bypass the task or syspatch's lock. Pair
preparation with `--reboot-after-prepare`. The runner compares validated native
`kern.boottime` identities and refuses an unchanged boot, failed request or
expired boot deadline. Ordinary jobs retain their existing boot lifecycle.

```sh
python3 tools/maintenance/qemu_bsd.py --profile openbsd-amd64 \
    --source-archive /tmp/reviewed-startup-source.tgz \
    --prepare-script dev-docs/ci/bsd-prepare-x-startup.sh \
    --run-script dev-docs/ci/bsd-run-x-startup.sh \
    --reboot-after-prepare --openbsd-vga-test \
    --cache builddir/ci-assets/cache --output builddir/x-startup-job
```

The archive must include the startup/QEMU tests, their helpers and asset pin
files. These checks compare server startup and authentication; they do not
qualify installed WM/client behavior or identify a crash's cause by themselves.

Default-X startup probes are distinct from installed core/module/client
qualification. Neither a boot nor a package-name check establishes the patched
libpng baseline; current unverified 1.6.58 packages must fail configure. Host
package receipts, actual remote CI and release sanitizer results remain separate
gates.

Historical adoption gates (remote-workflow execution is superseded by local review):

1. Verify VM runtime/image asset hashes, selected compiler/dependency packages
   and the actual default BSD X server. The Go candidate includes the default-X gate,
   whose execution must be qualified. The reusable `tests/run_bsd_default_x.py` driver is qualified
   on the existing local QEMU framebuffer/input setups; candidate VM hardware
   remains distinct. In particular the OpenBSD AMD64 VESA fixture requires its explicit guest-only
   aperture opt-in. Startup probes do not qualify installed core/client behavior.
   The approved bootstrap/provider checkpoint now qualifies these local gates;
   repeat affected checks for changed inputs or recipes before adoption.
2. Review inherited Ubuntu/Alpine image digests or qualify their replacement.
   No digest was invented for the upstream mutable `latest` images.
3. Qualify actual native Linux sanitizer jobs and Prompt/Go release coverage.
   The core/sanitizer profiles retain `golang=disabled`; separate Go jobs enable
   the client/utility family, build with vendored inputs/toolchain fetching disabled,
   exercise the actual protocol and stage seven tagged clients/utilities. Linux also runs units/vet and a
   race-instrumented client against peer and actual-WM fixtures. Local profile
   qualification does not establish a passing Actions run.
4. Resolve the FVWM-X repository destination (this checkout has no Git remote),
   then run the candidate there. Configuration-only checks
   and the local six-target test-infrastructure checks do not establish that run.
5. Keep release publication, credentials and upstream notifications separate.
   Resolve those destinations before enabling replacement publishing jobs.

To update a pin, resolve the official tag to its full commit, compare action
inputs/entry points and release configurations, record file hashes and review
transitive download changes. Retain the old lock and source receipt, validate
candidate YAML and platform/profile coverage, then qualify affected CI jobs.
Do not replace a SHA merely because a moving tag advanced.

Portable gate usage from the source root:

```sh
sh tests/run_on_linux.sh builddir/linux-native
sh tests/run_on_bsd.sh builddir-bsd
sh tests/run_native_gate.sh --sanitizers builddir/sanitizers
sh tests/run_native_gate.sh --go-clients builddir/go-clients
# Only on an isolated BSD VM with a qualified default-X framebuffer/input setup:
python3 tests/run_bsd_default_x.py --disposable-vm \
    --build-directory builddir/go-clients --artifacts-dir builddir/default-x
```

These commands install no packages. Normal gates build, test and stage into the
chosen build directory. The Go profile builds Prompt, Command, fvwm-menu-xlock,
core, MFL, Form and Script, runs seventeen registered client/protocol/utility
entries, and stages seven tagged clients/utilities,
then tests those installed executables; sanitizer gates exercise instrumented
Linux binaries and do not stage an install. Go producers and the transport
test share one cache inside their build directory, avoiding a second cold SDK
compile during the 180-second test deadline. `FvwmPrompt-test-binaries` compiles
all test executables during build preparation; runtime tests remain required and
the test executables are not installed. BSD discovery appends standard package/X paths so an
explicit compiler or Meson environment retains precedence. PNG backport claims
still require the vendor evidence described in INSTALL.md. An OS-name check
cannot prove physical/native CPU execution; the calling CI environment owns that
claim. Wrong platforms and failed setup/build/tests stop before staging.

The default-X driver refuses a running Xorg, an occupied display or an existing
artifact directory. It starts the configured Xorg with TCP disabled and a private
MIT-MAGIC-COOKIE-1 authority file, using the existing bounded process owner for
teardown. It preserves logs and runs both installed clients against the real core
and MFL. It does not install packages, alter Xorg configuration, change VM hardware,
or qualify physical GPU/input behavior. Run it separately after the Go gate;
Xvfb results alone never count as default-X evidence. Use `--xlock-only` after
the Go gate to run the installed menu map/dismiss fixture on that default server
without repeating the clients or executing a screensaver action.

[Local qualification and retained failures](../verification/VERIFICATION-2026-10-12-CI-PROFILES.md)
record the final driver/profile source and its distinct native CI adoption gaps.

The [xlock utility checkpoint](../verification/VERIFICATION-2026-10-12-XLOCK-PORT.md)
adds the utility to this gate and retains its Go-disabled Perl fallback.

The Linux candidate uses the same approved PNG source via
`linux-prepare-png.sh` and `ci_assets.py --profile png-source`. Preparation and
loader qualification are job-local, including the selected compiler identity.
Native architecture/Go/sanitizer jobs still require their own actual CI evidence;
a passing dependency build is not a sanitizer or complete workflow result.
