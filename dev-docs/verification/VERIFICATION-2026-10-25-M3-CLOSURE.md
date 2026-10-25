# M3 target-owned dependency closure

Status: **combined local acceptance passed; maintainer review/integration pending**.
M3.4 completes the selected ownership criteria against merged PR206
(`856e9e4efb2a8439f714060fb2dc71273ab7a9cf`). All fifteen profiles validate:
99 pure entries, 105 registered real-X fixture dispatches, 24 additional program
checks and 240 native executable install identities. Fifteen actual full installs
contain 1412 regular files / 869,393,531 bytes in aggregate; those totals describe
the captured profiles, not a distribution size improvement.

The [machine manifest](VERIFICATION-2026-10-25-M3-CLOSURE.json) independently
rechecks archive SHA256/CRC, every captured regular payload's SHA256/size/mode,
source/retention bindings, native maps, installed identities and selected test
commands. [Dependency/license inventory](DEPENDENCY-INVENTORY-2026-10-25-M3.json)
composes six-target observations and retained before/after provenance. Final
separate review is recorded below; no release or license-compliance verdict is
claimed.

The owner is the current M3 implementation chat under
[PROJECT-TRACKER.md](../PROJECT-TRACKER.md#m34-issue-closure-checkpoint).
Canonical local evidence root, abbreviated **W**, is
`builddir/codex-2026-10-25/m3-closure/`. `W/source-final.json` freezes 2473
source files, SHA256
`feb4d1cadde3ccc5159f5edebfb6276677eed0d37adc60dd1a6cb21c7eaf7708`;
its source archive SHA256 is
`675eff734539b312d84d9bf1876506251089516bc75739aa1b58c59f04c7d03f`.
Post-freeze report/status changes must remain distinct from executed inputs.

## Scope and requirement-to-evidence map

This checkpoint closes accumulated production dependency ownership, rather than
selecting another partial target group. Each profile covers sixteen native
executables: core, twelve C modules, root setter, image worker and desktop
capture. Full actual `meson install --no-rebuild --destdir` stages also include
the installed Go executables, FvwmPerl/perllib, configuration and data. These are
complete installs, unlike the selected target copies in the preceding slices.
The distribution still ships Perl and retains Python/Meson developer tooling.

| Requirement | Evidence and current disposition |
| --- | --- |
| Actual target owners and full package dependency objects | [M3.1 worker](VERIFICATION-2026-10-25-M3-WORKER-LINKAGE.md), [M3.2 core/archive](VERIFICATION-2026-10-25-M3-ARCHIVE-LINKAGE.md), [M3.3 modules/widgets](VERIFICATION-2026-10-25-M3-MODULE-LINKAGE.md); retained symbol, map, extraction, compile and loaded-closure proofs. |
| Exact source/config/compiler/feature identity | Frozen manifest; each profile's environment, configure/introspection/compile/link receipts; registration-retention proof for prior passes. All fifteen source/profile bindings validate. |
| Six OS/architecture targets, compiler/reduced/optional profiles | Fifteen-profile register below. All fifteen validate; no incomplete profile counted as passed. |
| Full offline install and staged native identities | Per-profile `installed-files.json`, `native-staged.json`, install logs and complete archived stages; local SDK/vendor selection disables downloads. All fifteen complete stages validate. |
| Affected policy and staged real-X behavior | Selected pure tests, registered fixture commands rewritten to exact stage mappings, production smoke where selected; reduced coverage exposes newly registered fixtures. Six-target aggregate passes. |
| Static-library-transitive contract | Actual libfvwm3/Widgets extraction and full runtime dependency objects plus `pkg-config --static` inventory. All profiles reconcile; no all-static ELF profile is selected. |
| Package/license/installed-size inventory | Earlier M3 measured closure retained; per-stage inventories and selected-provider paths captured. Six-target library/package/provenance inventory is published; unknown notice fields remain explicit. |
| Dependency/interpreter floors | [Finite LIB-05 inventory](DEPENDENCY-FLOORS-2026-10-25.md): declared minima, actual exercised versions, unverified legacy Perl guards and separate M6/M7 ownership. No new floors or package removal. |
| Prior finite issue dispositions | [Issue reconciliation](ISSUE-CLOSURE-2026-10-25.md): #35 completed; #93 not planned for superseded unsupported-release scope. Preserve original failures and limits. |

## Validated profile register

| Target | Selected profiles | Validated profiles |
| --- | --- | --- |
| Linux ARM64 | default, reduced, Clang, cairo-svg backend | Four verified |
| Linux AMD64 | default, reduced, Clang | Three verified |
| FreeBSD ARM64 | default, reduced | Two verified |
| FreeBSD AMD64 | default, reduced | Two verified |
| OpenBSD ARM64 8.0 | default, reduced | Two verified |
| OpenBSD AMD64 8.0 | default, reduced | Two verified |

Linux/FreeBSD top-level profile receipts are `W/<target>/<profile>-verified.json`;
OpenBSD receipts are under `W/bsd/openbsd-{arm64,amd64}-job/`. These bind archive
hashes, payload verification and actual results. The machine manifest derives
the final counts from these completed archives rather than planning entries.
Reduced profiles disable automatic features and PNG/SVG/XPM/BiDi/NLS; selected
optional cairo-svg coverage keeps SVG enabled with cairo disabled. Actual
configured options and selected libraries, not profile names alone, define scope.

## Install identity, archive ownership and retained passes

Meson legitimately removes build-only RPATH at install. The install validator
therefore requires identical ELF layout/section metadata, size, non-RPATH
dynamic entries and every byte outside the precise RPATH string/dynamic-entry
regions. An arbitrary binary mismatch is not accepted. Per-profile stage maps,
loader output, environment and qualified private PNG paths remain separate from
ambient system metadata; staged fixtures reject unstaged production arguments.
Installed regular-file inventories must remain unchanged throughout testing.
Pure protocol/policy tests retain Meson's declared worker environment, which
selects the build worker for some entries. They are not described as wholly
staged execution. Installed worker identities and the staged real-X effect/SVG
fixture arguments independently cover the installed paths.

Static archive ownership is based on extracted members and complete dependency
objects, including shared-object and package-declared static/transitive needs.
A caller using one entry point does not erase the other dependencies of its
extracted object. This is the selected static-library contract; an all-static
ELF executable remains unqualified and outside the selected profile, not a
waived mandatory gate.

M3.1–3.3 already measured before/after link operands, extracted archives and
recursive loaded closure. Loaded sets remain exact and target sizes are neutral
apart from the recorded 32-byte FreeBSD ARM64 worker/decoder difference. Reuse
those accepted comparisons; this checkpoint does not claim package/license
removal, meaningful size or speed gains. Fresh installs measure current installed
closure, not an invented pre-change complete-install size comparison.

A real test-registration defect was discovered: non-session Form/Buttons/Pager/
Script and related fixtures were nested under `sm.found()`. Moving only that
registration block exposes 22 reduced-profile fixtures without changing product
code. Default fixture commands remain unchanged. Retain successful initial
default/reduced build, install and executed-test portions only where exact
config, compile/link commands, native binary identities, original fixture argv,
fixture dependency sets and installed identities prove equivalence. Each reduced
profile runs four selected X fixtures; exposing 22 registrations does not claim
that all 22 were freshly executed. Newly selected fixtures need actual runs.
Dependency-ID list ordering alone is incidental when the required sets and
commands are exact.

## Preserved failures and corrections

The canonical directories keep original receipts and correction scope distinct:

- A runner requested a configure-generated file as a Ninja target. The corrected
  build/install preparation treats it as configured output; the failed command
  remains visible rather than being called a product compilation failure.
- Initial byte-identity checking rejected expected install-time RPATH removal.
  The bounded ELF validator above replaces that incorrect expectation, retaining
  the original result and requiring exact equality outside the allowed regions.
- Production smoke used a one-line XBM fixture rejected by the existing parser.
  Reuse an already accepted multiline XBM grammar fixture and rerun only that
  smoke; do not change production parsing or count the invalid input as a pass.
- The SM registration guard was an actual coverage defect, not a runner-only
  artifact. Preserve its discovery, test-registration correction/tracker update
  and newly exposed fixture execution separately from retained initial passes.
- Retention initially treated dependency-ID ordering as semantic. Compare exact
  dependency sets alongside exact fixture commands; preserve the original
  failed comparison and its corrected proof without weakening product checks.

## Completion boundary

#35 is closed as completed and #93 as not planned; the lead's readback records
67 open issues after those dispositions. #93 is not described as fixed or
root-caused. #184/#49/#47 meet their selected owner criteria and are
linked for automatic closure on PR merge; they stay open during review. #11/#13 advisory/vendor work and
M4/backend, M6/framework and M7/tooling outcomes keep their distinct acceptance.

The selected link/install contract requires actual nested-X evidence across all
six targets; all selected profiles pass. It does not require a
fresh execution of the entire historical matrix. Native CPU qualification,
Linux native sanitizers, physical hardware/toolkits/compositors, broader default
server and release gates remain separately owned and unclaimed. Performance
reference PR204 is 2/5 after PR206; no comparison is due.

All three held Linux/FreeBSD VMs and both disposable OpenBSD jobs shut down
normally with QEMU status 0; the OpenBSD job keys are removed. Successful guest
capture/archive duplicates are retired only after canonical host CRC and every
payload hash/size/mode validate. Original failed captures and host evidence stay
preserved. Source, caches, existing stages and VM disks are not deleted.

The inventory includes current Linux package notice hashes/labels, FreeBSD
package license declarations, OpenBSD component/source provenance and verified
private PNG source/license hashes. Unknown/unfetched notices remain unknown;
retained package versions are distinguished from current selected metadata.
M3 changes no third-party package/version/payload or license obligations.
The AMD64 collector used unsupported `pkg_info -W`; its original nonzero status
and stderr are preserved. The composed inventory marks ownership unresolved,
separating path classification and retained component references from actual
file ownership. The current installed-package list can confirm package presence,
but does not prove that a particular library belongs to it. No VM rerun is needed
for this inventory-label correction.
A complete component inventory is not complete notice verification, legal
compatibility or advisory/release certification.

`make check lint`, affected links, final working/staged whitespace and frozen
production/test-input parity pass. A separate source/evidence reviewer assesses
the exact integrated candidate and finite acceptance before signed delivery.
Requested routing remains unverified where observed client metadata is
unavailable; this is agent review, not an independent human approval.
