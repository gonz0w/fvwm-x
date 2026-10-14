# Build reuse and PR batching — 2026-10-13

MAINT-02 child [#87](https://github.com/gonz0w/fvwm-x/issues/87) consolidates
three related outcomes: repeated-option reuse, removal of the redundant install
build check, and one-PR checkpoint policy. Base:
`867418c81d3620dd8ef945d40932413ef92da325` (source-tool PR #86 already merged).
Final commit/tree/base are recorded in the PR, avoiding a report-only follow-up
commit to embed its own hash. The source hashes below identify tested inputs.

## Resulting behavior and boundaries

The wrapper reads Meson's own option and source/build reports. Exact requested
boolean/integer/string values can reuse an existing tree, with the last repeated
option and explicit prefix retaining their order. Changed/unknown/array options,
missing/malformed/failed/foreign metadata and explicit reconfigure/wipe retain
ordinary setup. The existing build flags, parallelism and Python floor remain.
Ninja still handles source/build-description regeneration.

Install uses `--no-rebuild` after successful compile/test. Tests deliberately
retain their prerequisite build: optional Go test binaries in
`bin/FvwmPrompt/meson.build` have `build_by_default: false`. Skipping that step
would omit required cold-build inputs. A failed compile or test still prevents
installation. The final fixture includes a non-default C test target to verify
this independent Meson contract without adding a production dependency.

AGENTS, workflow, contributor and GitHub guides now make one coherent checkpoint
one PR, normally three to five related stories. Keep related docs/evidence and
review corrections together while that PR is open. Reuse owner-controlled
compiler/feature builds; give changed synced sources fresh timestamps, refresh
changed dependency detection and use fresh trees when invalidation is uncertain.
The native qualification gate retains its deliberate dependency-cache refresh.
Docs/status-only corrections use focused hygiene/link/branch checks, retaining
unchanged runtime evidence. PR creation itself triggers no rebuild or matrix.

## Verification

- Final macOS source suite: ten policy passes, two explicit missing-native-Meson
  skips. Existing wrapper/tool regressions pass 29/29 before the final checkpoint.
- Final `make verify`: 121 passes, seven skips (five existing native-Meson NLS
  fixtures and two new real-Meson fixtures on macOS). Existing NLS acceptance
  remains separate; the new real-Meson fixtures actually run on all six targets.
- Final affected checkpoint: 72/72 passes, no skips, across Linux, FreeBSD and
  OpenBSD AMD64/ARM64. Each target runs ten policy cases and two real Meson/C
  integrations. The integrations check one explicit setup across three equal
  option invocations, automatic build-description regeneration, changed option
  setup, explicit forced setup, non-default test prerequisite creation and
  preservation of the previously installed binary after a failing rebuilt test.
- Installed Meson 1.7.0 on both Linux targets and 1.10.2 on four BSD targets
  expose the supported install no-rebuild switch; target capability receipts
  are retained. No packages/toolchains were added or changed.
- Earlier development receipts are retained separately. Initial OpenBSD AMD64
  exceeded a 90-second outer deadline; the unchanged retry passed in 149.590
  seconds with a finite 240-second allowance. The final affected checkpoint
  uses that finite allowance. Source/build ownership and the test-only target
  correction changed tested inputs, so those earlier receipts are not reused
  as final acceptance. No full WM/platform matrix or existing VM was restarted.
- An expanded guide-link check found inherited absent documentation/evidence
  targets in the published base. New links from this batch pass; the broader
  source/document distribution reconciliation remains MAINT-02 work. The
  inherited gaps are recorded locally in `doc-link-gaps.json`; this report
  does not claim whole-repository link completeness.
- Repository hygiene, branch/staged whitespace and Python syntax pass. Final
  tested helper identities match the candidate; runtime C/header/Go files,
  module/configuration formats, feature/dependency defaults and native gate
  scripts are unchanged.

| Tested input | SHA256 |
| --- | --- |
| `tools/maintenance/common.py` | `8b42f45cd946c1823b2cc9bbbad36c63e2c76ed7b34ee478b1ea884442964826` |
| `tools/maintenance/build.py` | `d9be2a18740b934af973cd1cf1ac398f5226d9c1150a9a6d40ab91d8a2af0ae0` |
| `tools/maintenance/tests/test_build_reuse.py` | `bfdfd3906f83cd9920857b370f549440d12c790445b7424b4babd798fd9e6799` |

Receipts: [VERIFICATION-2026-10-13-BUILD-EFFICIENCY.json](VERIFICATION-2026-10-13-BUILD-EFFICIENCY.json).
Raw logs, frozen input manifests and earlier attempts remain in the ignored
`builddir/codex-2026-10-13/build-efficiency/`. Focused results measure build-step
avoidance, not a whole-WM wall-time speedup. MAINT-02, release tooling and native
physical/release coverage retain their remaining scopes. No Actions, hosted
runner, cache service, new mandatory dependency or release publication is added.
