# Style type dependency boundaries — 2026-10-19

MAINT-02 / CLEAN-01, [PR #119](https://github.com/gonz0w/fvwm-x/pull/119).
Base merged PR118 `201bb193873a2bf863a09786dc467f12f744d069`.
Signed/DCO source `42c6deaf8d4ea88205c057a7030904984e42b2ae`,
tree `6c72270596f0fe61b83b4dcc34b791c8662d44ca`.
One coherent three-boundary development-throughput checkpoint; the final record
isolation includes removal of the unused screen umbrella import.

## Change and measured work

1. `fvwm.h` forward-declares `window_style`; its complete record and special
   flags live in `style_types.h`. The style API/accessors and direct
   field/constant consumers import that layout. The unused umbrella import in
   `screen.h` is removed; geometry/virtual consumers include the API directly.
   Existing borrowed-pointer interfaces remain valid without importing it.
   Placement/snap types used by shared window state remain in the common header.
2. `style_id.h` supplies the unchanged selector ID with only the X11 ID type
   dependency. The matcher implementation, filter API and matcher unit stop
   importing the full window-manager state. Immutable metadata no longer
   reaches unrelated common-header consumers.
3. Owning style lists and their index pointer/definition live together in
   `style.c`. List layout changes now affect only its two core objects.
   Field types/order, allocation, links, matching predicates, precedence,
   resources and update/barrier ordering are unchanged. Configuration, package
   dependencies, feature/install defaults and compiler-cache behavior remain
   unchanged.

Actual Ninja object dependencies / restored-timestamp dry runs:

| Target / profile | Common-header dependents before → style-record dependents after | Metadata dependents after | Matcher API dependents after | Private owner compile tasks |
| --- | --- | --- | --- | --- |
| fvwm-x-linux-arm64-svg/gcc | 217 → 38 | 42 | 8 | 2 |
| fvwm-x-linux-arm64-svg/clang | 217 → 38 | 42 | 8 | 2 |
| fvwm-x-linux-arm64-svg/software | 214 → 38 | 42 | 8 | 2 |
| fvwm-x-linux-arm64-svg/minimal | 212 → 38 | 42 | 8 | 2 |
| linux-amd64/gcc | 217 → 38 | 42 | 8 | 2 |
| linux-amd64/clang | 217 → 38 | 42 | 8 | 2 |
| freebsd-arm64/default | 216 → 38 | 42 | 8 | 2 |
| freebsd-amd64/default | 216 → 38 | 42 | 8 | 2 |
| openbsd-arm64/default | 216 → 38 | 42 | 8 | 2 |
| openbsd-amd64/default | 216 → 38 | 42 | 8 | 2 |

The before count is the former location of the style record in `fvwm.h`;
common window-state edits intentionally retain broad fanout. This measures
compile work, not an end-to-end latency promise. New type/layout changes still
rebuild their actual consumers. Every timestamp probe restores original source
mtime and a final Ninja no-op verifies no build remains pending.

## Affected verification and source binding

- Ten compiler/feature profiles build across Linux, FreeBSD and OpenBSD,
  AMD64/ARM64. Linux includes GCC/Clang and ARM64 software/minimal profiles.
  All **70/70** selected native entries pass: style matching, ID filtering,
  actual-core profiling, window-update ordering, index maintenance, command
  ownership and style-update work. No tests or registrations are removed;
  all **2469** registrations and the distinct
  75-source assertion-enabled core remain.
- **5844** object pairs retain identical allocated
  code/data sections. The ten matcher-test objects differ only in assertion
  line constants after removal of an include: a separate comparison-only
  compile retaining that line as whitespace and its original diagnostic file
  name matches the baseline sections in every profile. The executed test source
  is unchanged by that comparison. All ten production WMs and all
  **150** build outputs intended for
  installation retain allocated-section parity; non-ELF target output bytes
  are exact. The comparison-only OpenBSD compiler input changes retguard
  section labels; their names are normalized while every guard cell's count,
  type, flags, size and bytes remain equal. Existing compiler hardening
  flags are retained.
  Debug sections and GNU build-ID notes are excluded, and whole-file
  hashes are retained separately.
- Ten affected fresh/updated owned stages verify **249**
  code/catalog identities against Meson build outputs and expected install-rpath
  transformation. Installed WM version probes pass. This is scoped install
  identity evidence; it does not close the broader OpenBSD AMD64
  install/staged-geometry gate described below.
- Compile-log review finds no new affected-file diagnostic. The unchanged
  OpenBSD matcher-test `strcpy` linker warning is retained and matched to
  PR118's compile log (its source line shifts from 1430 to 1429). Other
  existing diagnostics remain in the machine receipt. Declaration/macro/consumer
  review, added-header 80-column audit, whitespace and repository hygiene pass.
- `make verify` runs once on macOS: **166 passes / 11 explicit native-Meson
  capability skips**. This is portable foundation evidence, not native runtime
  acceptance. Final report/status updates use hygiene/input audits only.

Initial archive `c94afd0d014c5ead373a07c5eb270ec22714a0aa4b88144fcf913d3c26692a9b`
and final signed-source archive `67f5acb670a94e9bfa5036ba2cb6012dfbf297b5e2b13139615494ebe0c8ef48`
each contain **2017** regular inputs. Two inherited space-before-tab indentation
prefixes in the new style-record header were normalized after the commit hook
caught them. The first dependency measurements then found an unused `style.h`
umbrella import in `screen.h`; a final bounded amendment removes it and imports
`style.h` directly in the two macro/API consumers that relied on it. Intermediate
format-only receipts remain separately preserved. Final continuations compile
all affected objects and verify final source bytes/modes before and after
execution. Earlier passing configure/test commands are retained only with native
code/data parity against the baseline; profiles not reached by the first attempt
execute for the first time. No successful test repeats for these dependency-only
amendments. Final documentation deltas receive a separate input audit and do
not change production/test inputs.

Initial collectors stopped after successful builds/tests when they required
byte-identical sections even in the test object's shifted diagnostic line
constants. OpenBSD comparison-only stdin compilations also have input-derived
retguard section names; a later collector amendment normalizes only those names,
retaining all guard-cell metadata/bytes and exact other section comparisons.
Original attempt archives/logs and intermediate failures remain intact. The
narrow line-preserved comparison resolves these collector expectations; no
assertion, deadline, test outcome or production behavior is changed for green.
The composed receipt records original attempt outcomes separately from final
continuations, preserving successful phases rather than replaying them.

## Retained scope and limits

Broader unchanged runtime/default-X/scoped-sanitizer evidence stays with
[PR118's index-maintenance checkpoint](VERIFICATION-2026-10-19-STYLE-INDEX-MAINTENANCE.md).
Its raw standard result remains 2019/2022 executed, with 445/445 optional passes;
three unchanged isolated controls pass separately. This checkpoint neither
reclassifies those original deadlines nor repeats their fixtures. The two known
required OpenBSD AMD64 CI-worker/SVG omissions and broader install/staged-geometry
qualification gap retain their owners. No repeated omission retry, full runtime
matrix, authenticated default-server scenario, sanitizer, physical display,
toolkit/compositor or release gate is claimed here. Code/data parity plus the
focused native regressions support this bounded header-only reuse. Native-CPU
sanitizers, broader audits and release acceptance remain open; VESA #93 is deferred.

Capacity preflight bounds execution to two heavy native targets with two Ninja
jobs each, reserving shared-host capacity. The Linux AMD64 scratch check initially
found less than the existing 256 MiB threshold. Two completed PR118 archives
were hash/mode/mtime-verified and moved to that owner's larger backing volume,
with their original paths preserved as readable symlinks. No evidence was deleted,
threshold lowered or other owner's build/VM state changed. New checkpoint
artifacts use the larger owner volume. Original preflight/relocation receipts
and all native commands remain in the local owner evidence.

Receipts: [machine summary](VERIFICATION-2026-10-19-STYLE-TYPE-BOUNDARIES.json).
Raw original/intermediate/final archives, source manifests, graphs, command logs, section
comparisons, line-only proof inputs and self-review remain under
`builddir/codex-2026-10-19/style-type-boundaries/`. Local review only; Actions
and remote status publishers stay disabled. MAINT-02/CLEAN-01 remain open.
