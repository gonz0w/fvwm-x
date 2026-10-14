# Build and documentation adoption verification

This batch adopts the compatible `./build` entry point and shared documentation
manifest/extractor. Other maintenance generators, installed utility ports,
Perl protocol repair and runtime cleanup remain separate backlog items.
After this checkpoint, the user deferred the public website migration to
[DOCS-01: static website and manual rewrite](../decisions/WEBSITE-MANUALS-BRIEF.md).
The verified output candidate remains available; its public behavior change
is not approved and does not block other maintenance work.

## Result and scope

This build/manual adoption checkpoint is complete for the recorded profiles.
All six target evidence archives match the frozen source hashes and modes;
the remaining qualification limits are listed below. Other maintenance
items remain open; the public website replacement is deferred.

| Target | Default tests | Minimal tests | Feature tests | Staged files (default/minimal/features) |
| --- | ---: | ---: | ---: | --- |
| Linux ARM64 GCC | 75/75 | 72/72 | 75/75 | 96 / 95 / 151 |
| Linux AMD64 GCC, Go disabled | 75/75 | 72/72 | 75/75 | 95 / 95 / 148 |
| FreeBSD ARM64 | 64/64 | 61/61 | 64/64 | 96 / 95 / 151 |
| FreeBSD AMD64 | 64/64 | 61/61 | 64/64 | 96 / 95 / 151 |
| OpenBSD ARM64 | 64/64 | 61/61 | 64/64 | 96 / 95 / 151 |
| OpenBSD AMD64 | 64/64 | 61/61 | 64/64 | 96 / 95 / 151 |

Every listed profile has identical predecessor/replacement Meson options and
installed files. All six targets pass 25 maintenance-tool checks and actual
prefix-option ordering. Linux ARM64 Clang separately passes 75/75 default
profile tests with matching installed files. A final configure/compile/install
check leaves both ARM64 compiler artifacts identical to their tested gates.
Three optional-feature targets are absent in each minimal profile.

All four default BSD X servers pass ten FIFO protocol, deadline and recovery
outcomes each. A real Linux ARM64 build and staged install also passes with
spaces in the build directory, install prefix and DESTDIR; its installed
binary reports the expected version. This does not qualify a full desktop
session using a prefix containing spaces.

`build` is now a POSIX-shell entry point for the Python standard-library
wrapper. Its short options, default `compile` directory, explicit install and
explicit `-f` wipe remain available. Argument values retain spaces and setup
option ordering. Changed options reconfigure a selected build without an
implicit wipe; verbose failures propagate their actual status. Default logs
use private temporary files. `--dry-run`, `--test`, `--jobs` and `--log` are
additive developer options. Bash, shell `eval` and GNU `nproc` are no longer
needed by this entry point.

Meson and the standalone renderer use `tools/maintenance/manuals.json`.
Validated section extraction replaces grep in active Meson targets. The
manifest is a configure dependency. Man-only builds now include FvwmPrompt
when Go is enabled; HTML indexes include the existing desktop manual.
Legacy extraction and website helpers remain available for comparison and
rollback. No CI/release publishing workflow has switched.

The feature matrix found and repaired BSD gettext discovery: select the
ports library directory and matching headers, then verify the gettext entry
points link. `nls=disabled` continues to exclude the dependency. `nls=auto`
now discovers an installed BSD ports gettext implementation as intended.
No mandatory desktop library was added.

The minimal-profile test fixes reflect existing runtime policy. The optional
rendering-profile assertion runs when its four backends exist. Without iconv,
EWMH names are deliberately ignored before a property read; the live check
records that absence and always exercises the bounded ICCCM-name path.
Raw property, legacy-property and all applicable X checks remain enabled.

## Evidence and comparisons

Evidence is under `builddir/codex-2026-10-05/tooling-adoption/`.
`accepted-results.json` records the six archive checksums and accepted
comparisons. All six targets pass actual Meson/standalone documentation and
manifest-reconfiguration checks. Linux additionally compares the legacy
website script and flat fresh-directory renderer. The four default BSD X
servers produce 40 passing FIFO outcomes. All four task-owned snapshots are
shut down and their exact saved QEMU processes are confirmed exited.
The frozen source starts at the verified FIFO baseline, overlays this batch's
owned changes, and excludes concurrent desktop/launcher work. It includes
1,169 checked source hashes and executable modes. It is a development
verification archive, not a clean committed release of the whole checkout.

Each actual FVWM profile invokes the predecessor and replacement against the
same configured source/build, compares all Meson options and every staged
installed file, and runs the native policy and applicable real-X tests.
The predecessor is invoked through its Bash interpreter on BSD, where its
hard-coded `/bin/bash` shebang is not the package's location. A test-only
`nproc` bounds the predecessor to two jobs; the replacement uses `--jobs 2`.
All Go builds use the committed vendor tree with network/module fetching off.

The actual Asciidoctor comparisons cover both formats with Go, man-only with
Go, HTML without Go, and standalone reproducible output. Old/new Meson manual
pages are compared byte for byte. Standalone output deliberately omits
Asciidoctor update metadata; comparisons normalize only the HTML footer and
man-page date metadata, preserving the rest of the manual. Index differences
are the added desktop link. The previously missing man-only Prompt page is
checked separately. A manifest edit must trigger real Ninja/Meson
reconfiguration without a manual setup.

On Linux, the legacy website script runs against a disposable source copy.
The replacement preserves the existing page fragments/front matter and adds
the desktop page. Its index keeps the legacy title, version text and links.
The flat fresh-directory candidate is rendered with real Asciidoctor, refuses
repeat output without changing it, and produces reviewable pages under
`builddir/codex-2026-10-05/tooling-adoption/website-review/`.

## Failures retained and limitations

Initial comparisons exposed an optional-feature assertion in minimal builds,
an iconv-dependent live-test expectation, and missing BSD gettext discovery.
Fixture setup also needed Git for the inventory/archive tests and Bash to run
the predecessor. A truncated OpenBSD Ruby package download was retried in
its disposable snapshot. Failed-setup directories and copied source mtimes
were corrected before accepting results.

AMD64 BSD live-X timing failures under concurrent documentation rendering
are retained. Workloads are separated for their final runs; test deadlines
are not increased. Initial standalone comparisons detected Asciidoctor date
metadata differences; those specific differences were inspected before
normalization. No runtime failure is dismissed solely as a timing issue.

Linux AMD64's Go compiler fails under QEMU user emulation with invalid address
packing; disabling Go's garbage collector and guest-address reservation did
not resolve it. This matches an [upstream QEMU report](https://gitlab.com/qemu-project/qemu/-/issues/2560).
The AMD64 Linux profiles explicitly disable Go and must not be counted as
Go-enabled qualification. Go-enabled Linux AMD64 still needs a working
native/full-system execution environment. Project build defaults are not
changed to work around the verification environment.

Native Linux CI ASan/UBSan execution remains unrun. Physical mixed-DPI
hotplug/compositor qualification and clean committed release packaging remain
separate gates. The original project has no remote and its Git metadata is
read-only in this task. No project commit, push or publication occurs.

Only task-owned snapshots on ports 2291–2294 are used. Completed guests are
shut down, evidence is collected and their exact saved QEMU processes are
released. Other chats' guests/ports and original source/build trees are
preserved.
