# CI-01 fixed PNG bootstrap — 2026-10-13

Status: **locally qualified**. The approved bootstrap, affected helper/build
corrections and fresh BSD provider standard/Go/install/default-X checkpoints
are accepted in the scopes below. Workflow adoption and actual native release
CI remain open. No publication or remote CI result is claimed.

The user approved the [existing dependency's source bootstrap](../decisions/CI-PNG-INPUT-PROPOSAL-2026-10-13.md).
The libpng minimum and FVWM configuration contract remain unchanged. No
application C/header/Go implementation, mandatory library or SDK minimum changes.

## Accepted coverage

| Scope | Accepted result |
| --- | --- |
| Helper policy | Final `02789055`: 23 asset + 26 QEMU + 7 PNG checks on each of six OS/architecture targets: 336 passes |
| Linux dependency | AMD64/ARM64, GCC/Clang: four actual PNG builds, each upstream36, eagerly bound PNG/librsvg probe and private-loader checks |
| Linux Go | Both architectures: Go17, seven installed client identities each, installed utility controls, actual race-client peer/WM checks and two core/worker PNG consumers each |
| FreeBSD standard | Both architectures: 233 full-suite entries, complete core/worker install and four built/staged PNG consumers each |
| OpenBSD standard | Each architecture retains 232 passes and adds a fresh four-entry affected completion, complete install and PNG4; composed 233-entry standard checkpoint, not a second full-suite run |
| BSD Go/default-X | Four fresh provider profiles: Go17, seven installed clients and PNG2 each; six default-X fixtures and five unauthenticated-client refusals each |
| Go build boundary | Six native component profiles compile ten test binaries each, then run actual uncached transport tests and native gate-policy controls |
| Fixture corrections | Twelve verification-profile cases and eighteen complete Script-helper control repetitions on six targets; original inner/application deadlines and strict file/diagnostic assertions retained |

The BSD standard profiles qualify sixteen built/staged PNG consumer records;
four BSD Go and two Linux Go profiles add twelve built consumer records.
All loaded library hashes match their qualified private builds. The four BSD
Go profiles qualify 24 default-X fixtures and twenty cookie-less refusals.
The six complete Go profiles qualify 102 registered entries and 42 installed
client identities. These counts distinguish profiles, fixture runs and policy
cases rather than combining them into a single inflated test total.

## Implemented boundary

`tools/maintenance/ci_assets.py` adds one fixed release to the bounded verified
downloader; the worker refuses alternate PNG size/hash tuples. Original thirteen
image/runtime and five boot pins remain unchanged. A real cold download matches
the [official libpng release](https://www.libpng.org/pub/png/libpng.html):
1,074,540 bytes, SHA256
`d80dd2a38a37f803cb9b6ac7b14bd6e74ddc3b654780a8380bdf93523fdb4389`.

`qemu_bsd.py --png-bootstrap` consumes those bytes. Host xz expansion checks the
exact 6,880,256-byte tar payload, SHA256
`de225c673e1148bfa30f3fd16fcb4adf36507026ab60941ca9c9d7f95374f3d3`.
The guest independently verifies it before bounded regular-member extraction.
`ci_png.py` builds a job-private prefix, requires upstream36, retains
source/license/compiler/build/installed-library identities, executes an eagerly
bound PNG/librsvg probe and records actual loaded library hashes. Each build
phase has a 1,200-second deadline and 64 MiB hard file/output limit; owned process
groups are reaped and failure logs retained.

OpenBSD's vendor package names the libpng 1.6 API `libpng.so.18.2`. The private
build uses that explicit version and permits its local alias only after all
vendor PNG exports exist in the patched library. Unknown ABI names refuse
preparation. Consumer checks compare actual loaded PNG hashes to the compiled
receipt: standard jobs check built/staged core and worker; Go jobs stage clients
and check built core/worker. The loader environment belongs to the disposable
job, without changing production/system loader policy.

Core preparation keeps its independent package list. Go preparation obtains the
existing SDK/default-server packages first, then prepares the common dependency.
OpenBSD Meson/Go downloads complete locally with finite file limits, gzip/tar
integrity checks and receipts before normal signed `pkg_add` installation.
Ordinary dependencies receive at most three retries; partial packages refuse
preparation. This is the same official release feed and vendor trust mechanism,
not independent publisher-hash certification or a signature/backport waiver.

Go producers/tests share a build-local cache. The non-installed, non-default
`FvwmPrompt-test-binaries` target compiles all ten test executables before the
unchanged 180-second runtime gate; acceptance still executes the runtime cases.
[Compile-only testing and directory outputs exist in Go 1.21](https://pkg.go.dev/cmd/go@go1.21.13#hdr-Testing_flags),
so the optional SDK minimum remains unchanged. Six component runtime gates take
0.46–31.14 seconds; the final fresh OpenBSD ARM64 provider takes 30.00 seconds.
Compilation results alone are not runtime passes.

The verification-profile fixture now permits 75 seconds around four separately
bounded 15-second wrapper calls. The original OpenBSD AMD64 component takes
37.74 seconds, beyond the former 30-second outer allowance. Inner and application
limits do not change. The staged Script replacement fixture opens and holds its
writer before replacing the input path, establishing the helper's reader and
avoiding an unrelated EOF/request cycle. Replacement refusal, neighbor/file
preservation and process/diagnostic assertions remain strict.

## Executed source and evidence

Work directory: `builddir/codex-2026-10-13/ci-png-bootstrap`.
The accepted QEMU donor is `52008da25fb9d55d70b475ac716e2c40f578df64edf68bb51e4cfbb7e990b953`;
its accepted application donor remains `04c1ba2f`. All 1,082 C/header/Go files
and modes remain identical. Another chat's sources, builds and guests were
preserved and excluded. Each new provider job expands a fresh verified original
image; prepared/expanded disks are not caches.

| Target/profile | Exact executed source | Final evidence |
| --- | --- | --- |
| FreeBSD AMD64 standard | `1a5bd78926d90ea165501409c040e318ee4c3f52cf734b28b5256bdf67417f85` | `freebsd-amd64-fresh-v2/guest-evidence.tgz`; standard accepted, later initial Go timeout retained |
| FreeBSD ARM64 standard | Same `1a5bd789` | `freebsd-arm64-fresh-v2/guest-evidence.tgz`; standard accepted, later initial Go timeout retained |
| FreeBSD AMD64 Go | `127339e1ccedd9abd2e76f1df3fb3929f233a8d1712d2295fe8c065ae67b2807` | `freebsd-amd64-go-validation.json`; transport107.45s |
| FreeBSD ARM64 Go | `d243a91818680c0157372e5972b683c1b823b9acf87007061f9015983281af18` | `freebsd-arm64-go-validation.json`; transport41.32s |
| OpenBSD AMD64 standard | Retained `fde45477`; affected `23c6cefd98aa708097e9496c99cd38b65f649fe56600b6b54dfc0e1e2e6f990e` | `openbsd-amd64-core-affected-v13-validation.json` |
| OpenBSD ARM64 standard | Retained `7a5d7b2c`; affected same `23c6cefd` | `openbsd-arm64-core-affected-v13-validation.json` |
| OpenBSD AMD64 Go | Same `127339e1` | `openbsd-amd64-go-validation.json`; transport127.99s |
| OpenBSD ARM64 Go | `cec11a43f9691f0ec92c05039ac6014b8b1da789e403fb6fa987bfe3c90a8092` | `openbsd-arm64-go-affected-v14-validation.json`; transport30.00s |
| Linux AMD64/ARM64 Go | Same `127339e1` | `linux-go-final/amd64-validation.json`, `arm64-validation.json`; transport92.31s/0.99s |
| Linux dependency | `6313cb890e0a42c3dd58f8e2ef21f735cadfda483908c7d1ac0462315068026c` | `linux-followup/` compiler/build/loader receipts |

The OpenBSD affected script uses the public standard feature/setup flags,
compiles/stages the complete core, then executes only verification-profile and
three actual PNG/SVG controls. Independent audits compare its compiler,
dependency versions and selected configuration with each retained 232-pass
archive, and verify unchanged application sources, all installed library and
consumer hashes, 233 registrations and the repaired fixture's 75-second limit.
It adds no public skip-tests mechanism and does not claim a fresh full233 run.

`checkpoint-validation.json` checks earlier helper/dependency/FreeBSD standard
archives. `policy-adoption/`, `go-build-boundary-v2/`, `profile-fixture-final/`
and `script-fixture-final/` retain exact native sources, platform records,
logs/archives and final validation records. The latter two qualify twelve cases
and eighteen complete controls respectively. `preservation-v14.json` checks
1,516 files, seventeen owning changes and all 1,082 unchanged application
identities. `working-adoption-validation.json` confirms current owning files and
Meson blocks match the executed source, preserving unrelated shared changes.

A later Linux candidate-only change adds existing `png-consumers.json` and
`png-loader-*.log` artifacts in all three profiles. Its exact difference from
executed `cec11a43` is recorded in `linux-artifact-path-validation.json`;
executable job steps are identical. Candidates remain outside active workflows.

## Retained failures and corrections

- Early OpenBSD ARM64 upstream36/runtime version passed, but eagerly bound
  `ldd` aborted. Observations now unset eager binding while the actual probe
  remains eagerly bound. The original abort and corrected private-loader
  observations remain; its cause is unestablished.
- Initial provider runs were stopped when review caught that Go stages clients
  only. Three owned runners received SIGTERM and reaped; another had failed
  signed preparation. The Go recipe now explicitly builds the SVG worker and
  checks built consumers, leaving installed core checks with the standard job.
- OpenBSD streamed Meson/Go truncation, canonical-name rejection, ordinary DNS
  and partial dependency failures remain. The complete-package correction uses
  normal vendor signatures. OpenBSD `uname -m` reports `arm64`; its package
  directory is `aarch64`, and the earlier wrong guard is corrected.
- Both first FreeBSD standard profiles pass233/install/PNG4, then their initial
  Go transport reaches180 seconds. Shared producer cache alone does not resolve
  fresh ARM64 test preparation; explicit test compilation fixes the build/runtime
  boundary. Actual runtime limits and application sources stay unchanged.
- Fresh FreeBSD ARM64 v11 retains one xlock incomplete-I/O failure. Its exact
  executable passes three unchanged 27-case controls on the older existing p4
  guest; final fresh p3 v12 passes the entire installed/default-X family. No
  xlock limit/code changes were made; the original cause remains unestablished.
- The first six-target build-boundary harness collided with the root `build`
  wrapper file. `build-go` fixes the isolated output directory, retaining the
  initial setup logs and frozen sources.
- Both original OpenBSD standard runs retain232 and the outer profile timeout.
  Fresh affected completions repair that allowance and qualify install/PNG;
  valid unaffected evidence is retained rather than rerun.
- Original OpenBSD ARM64 v11 passesGo17 then fails the staged Script replacement
  diagnostic before FIFO construction completes. Reader readiness fixes the
  fixture. Eighteen unchanged strict native controls and fresh v14's entire
  staged/default-X family pass; arbitrary ENOENT is not accepted as success.
- Linux's initial collector requested consumer receipts under the PNG prefix
  instead of the build directory. Corrected collection preserves the completed
  gates without rebuild/retest; initial return2 and correction remain separate.

All accepted provider scopes retain exact source/scripts, console/run/install
logs, installed executable identities and consumer observations. Owned job keys
are removed and VMs reaped; failed trials and disks remain evidence. The two
initial FreeBSD combined jobs retain their later Go failure status rather than
being relabeled successful whole jobs.

## Remaining gates

Actual native Linux release/ASan/UBSan CI, candidate workflow adoption, inherited
container contents, physical hardware/toolkits and publication remain separate.
Local QEMU/Podman results and configured YAML do not establish a passing remote
native CI run. This checkout has no Git remote. The destination for the prepared
CI workflows is requested before proceeding with that external gate; no remote,
credentials or publication destination is invented.
