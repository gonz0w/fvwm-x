# Patched optional Go compiler adoption — 2026-10-14

Owner: LIB-03 / [#12](https://github.com/gonz0w/fvwm-x/issues/12), scoped child
[#91](https://github.com/gonz0w/fvwm-x/issues/91). Status: affected implementation and
combined local qualification complete, with retained server-startup failures. The user approved the current patched Go
baseline unless major issues emerge. No major SDK regression was observed. The independently failing OpenBSD AMD64
VESA server startup remains a separate tracked reliability issue.

## Change and compatibility

Optional Go-enabled builds now require stable Go 1.26.9 or newer in that series,
or stable Go 1.27.2 and newer. The shared Python policy validates the selected
executable's `go env GOVERSION` with `GOTOOLCHAIN=local`. Configuration refuses
old/prerelease/malformed versions with an upgrade/disable diagnostic and never
downloads a toolchain. Disabled Go never invokes the compiler; absent automatic
Go remains optional. `go.mod` keeps its 1.21 language directive and the six
used dependency pins. No FVWM configuration command/argument, theme format,
first-party C/Go source or mandatory GUI dependency changes.

The existing BSD Go preparation recipe now uses fixed official Go 1.27.2 SDKs
instead of the old OpenBSD Go 1.26.2 package path. It verifies exact size/SHA256
before extraction, refuses existing SDK directories and keeps system/earlier
compilers intact. Its normal download path has finite base-tool socket waits,
file limits and three attempts; the QEMU preparation deadline remains the total
job bound. No separate hard total-download deadline is claimed. An optional
regular archive allows offline preparation. Version, compiler/archive/license
identities, license and environment selection are retained with job evidence;
large SDK bytes stay outside the provider's collected build trees.

The preceding [dependency audit/vendor cleanup](VERIFICATION-2026-10-14-GO-DEPENDENCIES.md)
remains a separate frozen observation inside the same PR. Its 29 unused Logrus
files are removed; 426 retained third-party payload files/licenses and all six
used module versions are unchanged. The [approved decision](../decisions/GO-TOOLCHAIN-BASELINE-PROPOSAL.md)
records the external compiler requirement and acceptance plan.

## Inputs and qualification composition

Evidence root: `builddir/codex-2026-10-14/go-baseline/`. The 1462-file native
runtime/build/test/manual/license snapshot has SHA256
`29a4fa137eee98883805765fca4017ad8c6cbd4d3545a8c9b96c26722b5e07a4`.
Current developer/root Markdown and hosting metadata are excluded. Exact input
hashes/modes, SDK pins, executed commands, complete logs, C reuse identities,
staged executable identities and findings are recorded in the machine receipt
[published in the validated receipt](VERIFICATION-2026-10-14-GO-BASELINE.json).

The SDK recipe was discovered as a required downstream consumer after that
freeze. Its five shell/test inputs, actual new Meson registration block and
Makefile test entry receive separate focused qualification. Composition checks
permit only those two test-registration changes and the appended maintenance
README note among the 1462 frozen inputs; all other runtime/build input bytes
remain identical. This does not trigger a
second full Go/WM build matrix.

Eight official SDK archives are downloaded and checked against recorded
[official metadata](https://go.dev/dl/?mode=json): Go 1.27.2 for six targets and
the private macOS audit host, plus Go 1.26.9 for a real Linux ARM64 minimum check.
SDKs and compiler caches are batch-owned. System SDKs/packages, other chats'
source/build/VM state and previous evidence are preserved.

The six native Go profiles disable optional image flags at configure time and
reuse the previously accepted XDG checkpoint's unchanged core/MFL/Form/Script
executables. Every runtime C/header input is compared with that source before
reuse; executable hashes are checked again afterward. No C rebuild or claim
that this repeats a full image/compiler/distribution matrix follows. Native
Go PNG/XML policy paths remain in the unit/CLI checks. Existing full-feature
image/C acceptance retains its original scope.

## Combined local results

The final receipt validates the following affected scope. Original failures
remain present; the OpenBSD AMD64 X replacement controls are detailed below:

| Control | Actual passing scope |
| --- | --- |
| Compiler boundaries | Eight policy/real-Meson cases per target; actual selected program/local toolchain, patched acceptance, old automatic refusal and disabled-Go non-invocation |
| Native profile | Eighteen registered entries per target, including six affected nested-X consumers; 108 total entries |
| Offline Go | Ten unit packages/519 test-subtest records per target; vet on all six, no skipped units |
| Staging | Seven tested executables per target match all 42 staged outputs, with patched compiler build information |
| Installed consumers | Prompt, Command, xlock, directory, converter, Script helper and desktop CLI checks on each target |
| Linux race | Twenty race package results, both race Command peer checks and two actual-WM race invocations |
| Default BSD X | Twenty-four installed-client/helper fixture invocations on four default servers; twenty unauthenticated-client refusals |
| Actual minimum | Go 1.26.9 Linux ARM64 offline build, ten package/519 test-subtest unit records, vet, Prompt peer and source/symbol scan |
| SDK recipe | Four refusal cases and actual Meson registration on each target; four native BSD extractions, including one fresh FreeBSD ARM64 download and three verified local archives |
| Advisory checks | Six native source/symbol scans with actual Go 1.27.2 SDKs, plus the actual 1.26.9 minimum; preserve module-level observations and require no affected package/symbol findings |

All native builds/tests use vendored module inputs, a separate empty module
cache, `GOTOOLCHAIN=local`, `GOPROXY=off` and `GOSUMDB=off`. Test-only SDK inputs
compile once before the runtime deadlines. Builds, tests, install and race
checks reuse the appropriate new compiler cache. No module/SDK fetching occurs
during these build/test steps. Recipe downloads are explicit preparation.

Local `make verify` passes 145 portable checks with eleven explicit native-Meson
skips on macOS. The new compiler policy and recipe tests run with actual Meson
on the six native targets; the local skips are not counted as native passes.

## Advisory scope

The scanner is built with patched Go from pinned `golang.org/x/vuln@v1.9.0`
source. All 308 cached source files match its previously authenticated module
zip. Direct module-source builds report scanner version `v0.0.0`; exact source
and target binary hashes are recorded rather than inventing a release-binary
signature. The existing frozen official Go database has last-modified value
`2026-10-08T22:31:09Z`; the [preceding report](VERIFICATION-2026-10-14-GO-DEPENDENCIES.md)
records its download identity and relevant unmodified OSV records.

Each completed scan has three module-level observations for pinned x/sys/x/text.
Their affected Windows/norm/precis packages are not imported on the supported
targets. Module presence is not package/function reachability, and JSON-mode
exit zero is not used as a clean-result test. The final classifier checks the
actual finding messages and source-mode compiler version. This is a dated
known-advisory observation, not a general security certification.

## Retained failures and limits

- Initial qualification-driver attempts used incorrect documentation option
  names, a dangling not-yet-created compiler-cache target, and the wrong Meson
  source-identity metadata field. These fail before substantive native Go
  compilation. Each original archive is retained. Corrected runs reuse the
  successful setup/source/SDK directories and validate the actual Meson metadata.
- Offline `go install module@version` for the audit tool attempted a module
  deprecation lookup and refused network-disabled operation. Building directly
  from the same verified pinned cached source succeeds without fetching. Product
  module graphs and normal offline builds are unchanged.
- OpenBSD AMD64 Xorg aborts twice during VESA/DDC startup, before any WM or
  Go client launches. Both original server logs and failed archives remain.
  The earlier accepted Go 1.27.0 client baseline passes; three unchanged current
  Command/Prompt runs pass, as do three unchanged current Script runs and the
  remaining desktop fixture. C/client hashes, X configuration and deadlines
  are unchanged. The cause is unestablished. These add eight successful fixture
  invocations and five unauthenticated refusals beyond the nominal table,
  including the old-client baseline. [LIB-07 #93](https://github.com/gonz0w/fvwm-x/issues/93)
  owns the unresolved server reliability; this checkpoint does not fix Xorg or
  claim every attempted default-server startup succeeded.
- New SDK compilation starts a cache for its compiler version; old cache bytes
  are neither deleted nor mislabeled as current results. AMD64 runs use the
  existing emulated guests. No native-CPU C sanitizer or physical-hardware claim
  is made.
- The SDK/bootstrap changes receive affected native qualification. They do not
  relabel the earlier fresh-image PNG/package/provider checkpoint as a current
  whole-source rerun, activate Actions, publish a release, or certify every
  unknown third-party compiler advertising a sufficient version.
