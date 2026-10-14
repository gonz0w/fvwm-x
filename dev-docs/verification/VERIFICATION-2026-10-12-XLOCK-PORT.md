# fvwm-menu-xlock Go port — 2026-10-12

Owner: UTIL-01/RUNTIME-GO-01, execution stage 3. Status: affected local checkpoint qualified; final source/identity/driver
audit passes. The utility port has no configuration migration or new mandatory
dependency. Directory and converter ports remain separate checkpoints; this
report does not retire Perl or replace the full XDG menu provider.

## Source and implementation

Runtime source `c01935805c7830366d229f95e78c30c6ae011723d35619c6079f6cd7c30e50bd`
composes the accepted CI source `d751` with eleven owning paths, adding four
source files. Final source
`9ab4aabcb9464370a4db898a1a4fc26282418f94602edd58108d7a946f0f743b`
contains 1431 files and fourteen distinct owning paths. It adds the Go-gate
helper/contract update and changes only the xlock suite's outer deadline from
60 to 180 seconds. It also corrects bounded default-X readiness polling
and tests that contract. Go/C/header inputs are identical between runtime and
final source. The intermediate gate source is `5ebdb532724a2077d02f8ea5f06e903756699113438c96aabbc016b76ebd6a67`.
Frozen archives compose only owned shared-Meson blocks; other chats' edits
are excluded.

- Go-enabled builds install `fvwm-menu-xlock` from
  `bin/FvwmPrompt/cmd/fvwmmenuxlock`. Go-disabled builds retain the configured
  Perl utility. The separate `xlock-menu` install tag owns either executable.
- Options, abbreviated/case-insensitive names, parameter order, icons,
  special-mode grouping and byte-based width/truncation remain usable. Exact
  width-equals-length truncation is retained. The help introduction identifies
  a utility instead of a Perl script; version normalization is explicit in the
  candidate fixture and cannot replace the recorded reference.
- The Go producer directly executes `xlock -display NONE -help`, reads combined
  stdout/stderr, accepts help from nonzero exits and enforces five seconds and
  1 MiB. It reports failed probes before writing a menu. This removes the shell
  help pipeline. Generated `Exec xlock` actions and configured arguments remain.
- Formatting streams padding, stops on output errors and checks numeric widths.
  External description quotes/backslashes are escaped; configured format escapes
  remain literal. Recognized mode identifiers are restricted to letters, digits,
  underscore and hyphen; unrelated help/options are not generated as actions.
- The optional Go native gate builds/stages the utility and runs its parity,
  probe and menu fixtures alongside the existing clients. The BSD default-X
  driver adds `--xlock-only` without repeating unrelated client fixtures.

The implementation imports only the Go standard library, adds no module
requirement and keeps the existing optional Go 1.21 floor. Actual SDK versions
are recorded per target; this is not a separate minimum-SDK build claim.

## Affected qualification

Linux ARM64 uses the existing isolated Linux guest. Linux AMD64, FreeBSD
AMD64/ARM64 and OpenBSD AMD64/ARM64 use existing local QEMU guests. All builds
use offline Go settings and independent directories. The unchanged accepted
native core/MFL binaries are reused for affected X checks; exact identities
are verified. A separate fresh Linux ARM64 Go gate builds its own native core.

| Coverage | Scope |
| --- | --- |
| Registered affected tests | 30 successful logical entries across six targets, including the recovered OpenBSD AMD64 suite; 24 Go package results |
| Utility fixtures | 270 recorded observations across build, installed Go and installed Perl runs; 90 observations apply to xlock. The remaining observations retain the other utilities' legacy baselines, not their port status. |
| Focused CLI/probe cases | 324 cases across six built/installed Go utilities; another 54 cases execute Linux race-built utilities |
| Pure harness contracts | 84 cases: eight final default-X controls and six Go-gate controls on each target |
| Actual X | Six nested-X build fixtures and six installed fixtures: two Linux Xvfb and four authenticated default-BSD-X. Each maps/dismisses both generated menu layouts through real PipeRead; 24 menu cases. No generated lock actions execute. |
| BSD authentication | Four clients without the private authority cookie are refused |
| Profile installs | Twelve tagged Go-enabled/disabled stages; six installed Go ELF identities and six Perl identities match tested bytes, versions and executable modes |
| Linux concurrency controls | Eight race-unit package results, two actual race-built probe suites and two vet runs |
| Fresh Go gate | One fresh Linux ARM64 build runs eight registered entries, stages all three Go clients/utilities and exercises installed fixtures and the existing Command race/live controls |

Probe cases cover stderr help, nonzero help status, unknown help format,
external label escaping, malformed mode rejection, excessive help output,
timeout and missing executable. No screensaver or screen lock is invoked.
There is no new full C compiler/sanitizer matrix because C/header inputs are
unchanged. Prior full qualification remains authoritative for those inputs.
These local/emulated results are not evidence of actual native release CI or
physical BSD display hardware.

## Retained failures and corrections

1. The initial real-executable output-limit check found that embedding
   `bytes.Buffer` promoted `ReadFrom`, allowing `io.Copy` to bypass the capped
   `Write`. The buffer is now a named field. A normal stream-copy regression
   checks the limit, in addition to direct buffer and CLI controls.
2. Initial Linux ARM64/FreeBSD ARM64 menu fixtures called `close` on a pointer
   helper. Its connection belongs to the Client. The fixture cleanup was fixed;
   both initial failed trees/logs are retained. The corrected fixture passes
   all six targets and installed default-X checks.
3. OpenBSD AMD64's 27-case CLI suite exceeded the 60-second orchestration
   allowance under emulation. Its other four registered tests passed. The
   outer suite allowance is now 180 seconds; the utility's five-second probe
   deadline is unchanged. Only the timed-out suite is rerun; valid earlier
   package/parity/menu/default-X-contract results are retained.
4. The first OpenBSD AMD64 default-X recovery aborted on a two-second xrandr
   startup query timeout. `query_ready` now treats that timeout as not yet
   ready, with the unchanged two-second per-query and fifteen-second overall
   allowance. The final eight-case contract runs on all six targets, and all
   four BSD default servers rerun the affected installed menu fixture. The
   failed startup logs/artifacts are retained.
5. Local macOS vet initially could not write its default cache under the
   workspace sandbox. It passed with a workspace-owned cache. This does not
   substitute for native target vet/race results.

## Documentation and measured scope

The primary AsciiDoc manual documents build profiles, direct probe behavior,
limits, literal configured formatting and byte widths. Linux AMD64 renders
man/HTML from that source; FreeBSD ARM64 renders the identical generated man
page with mandoc. Both render successfully. Generated mandoc lint status 3
is retained for generator macros/metadata and generated-line style warnings;
this is not a clean
lint claim. HTML generation disables active webfonts and has no active remote
script/stylesheet dependency.

Twenty warm `--help` startup samples per implementation on Linux AMD64 and
FreeBSD ARM64 record wall/CPU/RSS data. Median wall times are about 39 ms/153 ms
(Go/Perl) on emulated Linux AMD64 and 0.63 ms/8.96 ms on FreeBSD ARM64. These
small, non-resident samples ran in the local test environment; they do not
establish general desktop latency, menu performance or lower memory use.

Evidence is retained under `builddir/codex-2026-10-12/xlock-port/`:
`source.tgz`, `scope.json`, `source-gate.tgz`, `scope-gate.json`, initial source
and gate archives, six native evidence archives, six installed-identity
archives, five gate follow-up archives, six final driver archives and two
manual-review archives and exact
checkpoint/recovery logs. `validate.py` checks source hashes/modes, unchanged
runtime inputs, the exact timeout-only derivation, registrations, outcomes,
installed identities, actual menu results and full archive trailers.

## Remaining work

Continue `fvwm-menu-directory`, then `fvwm-convert-2.6`, with their owning
fixtures and individual checkpoints. Go-disabled fallback/default/minimal
runtime policy and approved Perl API retirement retain stage 5 gates.
CI provider adoption, a Git remote, native release C sanitizer CI, physical
hardware and optional external screensaver integration remain separate work.
