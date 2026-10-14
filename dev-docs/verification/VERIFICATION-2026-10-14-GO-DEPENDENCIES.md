# Optional Go dependency audit and vendor cleanup — 2026-10-14

Owner: LIB-03, scoped child [#91](https://github.com/gonz0w/fvwm-x/issues/91).
The affected offline checkpoint is locally qualified. The compiler-baseline
proposal remains a dependency decision; this report does not certify all Go
versions accepted by the current build or close the broader LIB-03 owner.

## Result and source scope

The published baseline is `93b13894fc67dc004be53746e8284e424e08b821`.
Removing the unused direct Logrus requirement and regenerating `go.sum` and
`vendor/` removes 29 vendor files and nine obsolete checksum lines. The 426 retained third-party source/data/license files and six used module
versions are unchanged; the vendor manifest is regenerated. No first-party Go/C/header source, FVWM configuration contract,
language floor, Meson default or installed entry point changes. This is a
dependency-graph cleanup, not a runtime performance claim.

The affected archive contains 498 files with exact hashes/modes, including the
complete optional Go module and the existing Prompt peer fixture. Its SHA256 is
`7744b03c165089500e90f3502eb738a97e96b4c66df905dacd2b5706bd77ae23`.
[Machine-readable evidence](VERIFICATION-2026-10-14-GO-DEPENDENCIES.json) records
the source manifest, executed commands, complete-log hashes, native binary
identities, scan findings and tool/database identities. Raw logs, archives,
scanner binaries and the frozen database remain in
`builddir/codex-2026-10-14/go-audit/` in the owning local workspace.

## Advisory analysis

The official [Go vulnerability checker](https://go.dev/doc/security/vuln/)
is developer audit tooling, not a build/runtime dependency. The Darwin checker
is `govulncheck@v1.9.0` built with Go 1.27.0. The Linux ARM64 checker is built
from the same pinned module's cached source; it reports scanner version
`v0.0.0` because it was built in the module directory. Both binary hashes are
recorded; do not infer identical binaries or an upstream binary signature.

The [official database snapshot](https://go.dev/doc/security/vuln/database)
was fetched at `2026-10-14T17:56:51Z`; its last-modified value is
`2026-10-08T22:31:09Z`. The 3,439,277-byte zip has SHA256
`931855e697c9adf12b79d205c7f7f1f8e1e080f936647757c4ea246546438073`.
All scans use this local snapshot. A zero JSON-mode exit is not a clean result:
findings are classified from the structured messages.

Six target-specific source/symbol scans run on macOS with Go 1.27.0,
`CGO_ENABLED=0`, and explicit Linux/FreeBSD/OpenBSD ARM64/AMD64 targets. None
reports a reachable affected symbol. This is cross-target source analysis,
separate from the native execution below. A seventh macOS-only baseline is
retained locally. Native target package lists corroborate the dependency scope.

| Pinned dependency observation | Disposition for the six supported targets |
| --- | --- |
| x/text 0.9.0: GO-2026-5970 (`unicode/norm`) and GO-2026-6629 (`secure/precis`) | Neither affected package is vendored or imported. Readline uses `width` and `transform`. No affected symbol is reported; do not force an unrelated Go-floor increase by blindly selecting the newest x/text. |
| x/sys 0.15.0: GO-2026-5024 (`windows.NewNTUnicodeString`) | The affected Windows package is not in these target import graphs. Its files remain in the multi-platform vendor tree; version presence alone is not target reachability. |
| Logrus 1.9.4 | No production/test import or target dependency. Remove the unused requirement and regenerate vendor data. |
| Go 1.27.0 standard library | Version-level observations remain. One package-level `os` finding, GO-2026-6604, is Windows-specific; no affected symbol is reported on the selected targets. This is not a recommendation to retain an unpatched SDK. |

The exact relevant advisory records are preserved in
[receipt data](receipts/go-dependencies-2026-10-14/).

The existing Linux ARM64 SDK is Go 1.22.2. A native source/symbol scan with
that actual SDK reports 83 module, 13 package and 24 symbol finding records;
the symbol records cover seven unique advisories. These are conservative call
graphs, not seven demonstrated exploitable defects:

| Older-toolchain symbol observation | Source-level triage |
| --- | --- |
| GO-2025-3750, GO-2026-4971 | Described failures concern Windows; Linux is outside that condition. |
| GO-2024-2824, GO-2024-2887 | Generic `net` paths appear in the graph. Production Command/Prompt explicitly dial Unix sockets; this audit does not demonstrate a DNS/IP-classification input path. |
| GO-2026-4602 | Described failure requires files opened through `os.Root`. The project does not use that API, which is absent from Go 1.22.2. |
| GO-2026-6088 | XML token decoding is called, with project-owned byte/node/depth limits and no DecodeElement/custom unmarshaler. The upstream issue concerns recursive DecodeElement handling; no reproducer or claim of immunity is made here. |
| GO-2025-3956 | `exec.LookPath` is called for external application metadata. Empty action argv is refused, but this checkpoint does not establish that every dot/dot-dot input condition is excluded. Keep the old-compiler support risk open. |

Use patched, maintained compilers instead of accumulating ad hoc standard-library
workarounds. Go's [release policy and current patch releases](https://go.dev/doc/devel/release)
support a concrete [optional compiler baseline proposal](../decisions/GO-TOOLCHAIN-BASELINE-PROPOSAL.md).
It is not implemented by this checkpoint. A fresh Go 1.21 compiler execution
remains unverified; source syntax compatibility is not a security-support claim.

## Combined affected native checks

| Check | Actual result |
| --- | --- |
| Offline builds | Seven executable builds on each target, 42 native outputs |
| Go units | Ten packages and 519 test/subtest pass records per target; 60 package results and 3114 test/subtest records total; no unit skips/failures |
| Static checks | `go vet -mod=vendor ./...` passes on all six targets |
| Prompt peer contracts | Ten existing observations per target, 60 total |
| Dependency identity | Every native Prompt binary reports exactly the same six used module versions; no Logrus dependency |
| Input preservation | All six target runs preserve the 498 frozen file hashes/modes; all complete evidence streams/log hashes validate |
| Local policy | `make verify`: 137 portable passes and seven existing native-Meson skips; local Go units also pass ten packages/519 test/subtest records |

Linux ARM64 uses the existing Ubuntu container and Go 1.22.2. Linux AMD64 and
the four BSD QEMU guests use their isolated Go 1.27.0 SDKs. Platform/kernel
details and executable identities are in the receipt. `GOTOOLCHAIN=local`,
`GOPROXY=off`, `GOSUMDB=off`, `-mod=vendor` and a separate empty module cache
are explicit. No dependency or compiler is fetched during native checks.
Existing owner-controlled compiler caches are reused; no WM/C rebuild, install,
full distribution matrix or X-server run is required for deleting an unused
dependency. Previous native/X acceptance retains its original source scope.

## Retained failures and limits

- The Linux ARM64 Podman VM was stopped. The first restart reported success;
  later inspection showed it stopped, with cause unestablished. A second restart
  in a persistent session and starting its existing container restored access. No VM image or guest package was replaced.
- The first tidy preview could not create a lock in the sandboxed shared module
  cache. A batch-local module cache then acquired checksum-verified copies of
  the already pinned dependencies; `go mod verify` passes and regenerated
  retained vendor bytes match the baseline. No module version was upgraded.
- The ARM64 native runner completed builds/units/vet/peer checks, then reached
  an absent optional `file` utility. Its original failed archive is retained.
  Only the remaining identity/scan steps ran afterward: existing ELF magic and
  `go version -m` replace the unavailable utility; no package was installed and
  successful builds/tests were not repeated.
- Receipt validation initially expected module checksums in vendor-mode binary
  build information and boolean peer results. Actual Go output contains module
  versions without those sums; peer results use the existing `passed` string.
  The validator was corrected against those formats; input/checksum provenance
  remains a separate check. Production inputs were unchanged.
- No exploited input, general vulnerability-free claim, patched-SDK execution,
  fresh Go 1.21 run, native Linux C sanitizer or physical-hardware result is
  established. Date/database/tool/target limits are deliberate.
