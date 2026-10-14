# CI client profiles and default-BSD-X harness — 2026-10-12

Owner: CI-01. The affected local checkpoint is qualified. Candidates remain
inactive outside `.github/workflows`; this report is not a passing Actions run
or native Linux release C sanitizer result. No configuration migration, Go floor
change, mandatory package or runtime dependency was introduced.

## Changes

1. `tests/run_native_gate.sh --go-clients` selects an explicit optional profile.
   The existing core and C sanitizer profiles still disable Go. The new helper
   builds Prompt, Command, the core executable and MFL; runs five registered
   client/protocol entries; stages only the two tagged clients; and tests both
   installed executables. Linux also runs Go race units/vet and executes a
   race-instrumented Command client against peer and actual-WM fixtures.
   Toolchain/module fetching is disabled, with vendored inputs. Prompt's ordinary
   Meson build also disables fetching and now has its own install tag.
2. Candidate Linux jobs cover both architectures with separate Go coverage;
   BSD candidates retain both architectures and add core/Go profiles. Full-SHA
   action pins, read-only repository permissions and disabled checkout credential
   persistence remain. Neither candidate activates a release or default-X job.
3. `tests/run_bsd_default_x.py` reuses the actual Command/Prompt fixtures on the
   configured default Xorg in a disposable BSD VM. It refuses an existing Xorg,
   occupied display or existing evidence directory. Xorg uses a local socket,
   TCP-disabled transport and an ephemeral MIT-MAGIC-COOKIE-1 file. On OpenBSD,
   only `_x11` receives group read/search access for privilege-separated rereads;
   other users receive no authority-file access. Every final run verifies that
   a client without the cookie is refused. Existing bounded process ownership
   tears down only this invocation's processes; cookie bytes are not archived.
4. Prompt readiness waits for a successful MFL connection instead of pathname
   creation. Only startup refusal/missing-path errors are retried within the
   original ten-second readiness bound, while checking the core's lifetime.
   Commands/responses are not retried. The shared fixture Connection closes its
   socket when construction fails. Production MFL and Go transport are unchanged.
5. Thirteen selected official VM assets are download-verified; inherited public
   container identities are recorded separately. Fixed hash enforcement in the
   actual VM provider remains an adoption requirement.

## Scope and composition

Evidence root: `builddir/codex-2026-10-12/ci-profiles/`.

The accepted Command source `295e87d89e5d9f61de8ab9b94f2d694536ff215858b70b7c8681624c4e20a59b`
is the baseline. Final source `d75181366a6fc7af285f5f1b1fd121741881e227ef312036b1ed41aefc823d50`
contains 1427 frozen files through eleven owning paths, including only the new
registration block from the shared `tests/meson.build`. Other-thread Meson tails,
source, builds and VM disks are excluded/preserved. Source hashes/modes match on
all six targets. Generated Python bytecode is identified separately from frozen
source. All baseline C, headers and Go source bytes are unchanged.

The six initial client-profile checks use `d3372e6083c56f6cb47490aa157627d3c9f7366b4f295195152a1ade2ceb59c1`.
The later typed Meson target selector and driver/readiness corrections alter only
scripts/fixtures. Final affected controls qualify their precise composition;
unchanged builds and peer results are retained. Accepted core/MFL identities
match the preceding Command checkpoint. This is affected six-target qualification,
not six repeated complete C/distribution matrices.

## Results

| Control | Actual passing scope |
| --- | --- |
| Native Go package results | Three packages on each of six Linux/BSD targets; 18 results |
| Ordinary peer controls | 444 observations: 27 Command and 10 Prompt cases, both build-tree and installed executions on each target |
| Final pure harness contracts | 78 cases: six native-gate and seven default-X/socket-ownership cases per target |
| Final nested-X controls | Twelve actual Command/Prompt fixture invocations, two per target |
| Final default BSD X | Eight installed-client fixture invocations, two on each of four targets; four unauthenticated-client refusals |
| Linux race controls | Race units/vet on both architectures; 54 actual race-client peer cases and two real-X invocations |
| Tagged stages | Two installed native Go client identities per target; all twelve match tested executables and native ELF architecture |
| Fresh Go gate | Linux ARM64 setup through fresh core/MFL build, five registered entries, both installed clients and actual race-client checks pass; additional 101 peer observations and three X fixtures |
| Candidate structure | Both YAML documents parse; expected architectures/profiles, permissions and full-SHA actions validate; files remain inactive |
| Selected asset downloads | Thirteen match published SHA256 and size; four compressed images streamed/discarded, none booted |
| Inherited containers | Two public manifest digests verified against manifest bytes; contents not pulled/executed/qualified |

Linux ARM64 uses the existing Ubuntu 24.04 container/VM and Go 1.22.2. Linux
AMD64 uses the existing Debian 13 QEMU guest. BSD uses FreeBSD 15.1 and OpenBSD
7.9 QEMU guests with the existing isolated Go 1.27 SDKs. Warm target-local Go
caches are copied into this batch's independent directories, including Meson's
registered-test cache; no SDK was downloaded or installed. Existing patched PNG
inputs are reused. FreeBSD default-X logs select scfb on both architectures;
OpenBSD selects wsfb on ARM64 and vesa on AMD64.

`validation-summary.json` consumes complete gzip streams, checks frozen source
hashes/modes, source derivations, executed helper receipts, test registrations and
results, native core reuse, installed/tested client bytes and architecture, asset
receipts and manifest identities. The original successful checks and every failed
attempt remain available; a later failure does not erase preceding evidence.

## Retained failures and precise repairs

- The two initial ARM64 runs passed six of seven registered entries; the native
  gate mock launched the new helper from Meson's build directory. Its invocation
  now specifies the source root. Only that fixture was rerun before continuing.
- The fresh gate found `fvwm3` ambiguous between a static library and the core
  executable. The helper selects `fvwm3:executable`; the contract checks this
  selector and the actual fresh build passes. Initial setup/build logs remain.
- Initial OpenBSD AMD64 default-X Prompt startup reached a socket pathname before
  the listener accepted connections. Its logs also exposed the `_x11` authority
  reread permission failure. The final bounded connection-readiness and server-
  group authority corrections pass all six affected fixture profiles and all
  four authenticated default servers. No input/command retry masks this failure.
- The Linux ARM64 identity collector assumed the optional `file` utility existed.
  It now uses the guest's existing `readelf`; no package was added. Failed helper
  logs and the affected recovery are retained.

## Asset review and remaining CI gates

[Asset pins](../ci/ASSET-PINS-2026-10-12.json) record exact downloaded bytes for
AnyVM 0.7.2 and four VM image/profile/launch triples, from the official
[AnyVM release](https://github.com/anyvm-org/anyvm/releases/tag/v0.7.2),
[FreeBSD builder release](https://github.com/anyvm-org/freebsd-builder/releases/tag/v2.2.8)
and [OpenBSD builder release](https://github.com/anyvm-org/openbsd-builder/releases/tag/v2.1.0).
Publisher release/API receipts and locally computed hashes are retained. These
are reviewed identities for these inputs, not a complete transitive dependency
or image-content certification.

The [pinned FreeBSD action entry point](https://github.com/vmactions/freebsd-vm/blob/c46abacb49f09938ca4e1702d15d836285d694cc/index.js)
downloads versioned AnyVM before execution without a fixed digest check. Review
of the selected AnyVM source also leaves image/profile/cache identity enforcement
open. Its pinned firmware checks are distinct from those missing checks. Hash
observation alone does not make the current candidate fail closed on changed
release assets; finish enforcement before adoption, including cache hits and
other boot/SSH inputs.

The selected plain OpenBSD AMD64 profile chooses virtio VGA; it is not the
accepted local vesa setup. The selected AnyVM source itself describes lack of a
base OpenBSD AMD64 DRM driver for that default device. The local driver's eight
passes do not qualify the provider's images or framebuffer/input setup. Default-X
jobs stay inactive until that difference is resolved and actually tested.

Observed Ubuntu/Alpine `latest` manifest identities do not qualify inherited
container contents or change live workflows. Candidate package downloads and
fresh compiler/Go profiles still need their actual runner evidence. The checkout
has no Git remote. Actual Actions execution, native Linux release C sanitizers,
physical/toolkit/compositor checks and publishing/credential ownership remain
separate. Continue installed utility ports one at a time while these CI adoption
requirements remain explicit; no configuration/dependency decision blocks that
next local work.
