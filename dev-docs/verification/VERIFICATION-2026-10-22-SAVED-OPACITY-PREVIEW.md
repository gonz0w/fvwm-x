# Saved launcher-opacity preview: focused checkpoint

Date: 2026-10-22. Owners: CFG-01 / IPC-02. **Focused checks pass; shared
integration is pending.** Base/integrated reference is merged PR163,
`cb380f9b3ccd910678a767d4d768731c748edf81`. This is one coherent feature batch,
not complete configuration-center or release acceptance.

## Behavior and reviewed contracts

- Register integer `launcher_opacity` (0–100, default 94) in native configuration,
  editor and MFL validation/set/reset/query. MFL admits at most eight values and
  bounded integral signed-int JSON; the owner enforces setting type and range.
  Complete schema-1 files load with the default without a write; explicit saves
  write schema 2. Invalid/future data stays untouched. No user migration or FVWM
  configuration rewrite is required; older helpers cannot read schema 2.
- Native F2 **Keep and save** first confirms the core lease, then writes only
  this preference using existing private nonblocking lock/revision/atomic replace.
  Helper generation and file revision are checked before apply, before confirm
  and at save. Confirmation/file persistence are separate outcomes: writer
  refusal after confirmation reports runtime kept / preference not saved.
  There is no full-theme replay after save, or claim of completed rendering.
- F4 transfers the validated grouped draft into opacity-only preview, retires
  its editor/token, then recreates the captured draft with a fresh token when
  freshness permits. Other fields remain unsaved until Apply. Older callbacks
  cannot publish; newer preferences discard the stale draft. Saved and effective
  wallpaper state retain their separate ownership.
- An independent native Form 16-second Nop timeout closes the consumer after
  helper death. Core deadline/module retirement still owns recovery. One lease,
  bounded reply wait and queued control remain; Revert takes priority.
- Reproduced prerequisite: ordinary editor destruction stamped appearance
  freshness despite removing no WindowStyle, so confirmation conflicted. Stamp
  actual removal in the internal helper; explicit destroy commands still stamp
  even a no-op. Actual-X proves both ordinary and real-definition retirement.
- Screenshot review found a swallowed percent substitution in the new field
  label and an inherited negative-position notice below the screen. Use literal
  “percent” and native centered notice placement. The corrected actual-X fixture
  asserts the partial-outcome notice lies within the 1440×900 display.

No packet/catalog ABI change, new mandatory dependency or blocking core work is
introduced. Generic IPC saved-preview remains unavailable (`preview: false`);
`native_preview: true` identifies this owned opacity consumer. Full appearance,
image-resource and general saved-preview adapters remain open.

## Executed gate

One reused Linux ARM64 GCC 13.3.0 incremental profile passes **nine registered
entries**, including **74 desktop policy cases**: desktop-policy, mfl-request,
core-query-json, preview-policy, desktop-baseline-xvfb, mfl-request-xvfb,
mfl-preview-xvfb, mfl-core-state-xvfb and mfl-settings-xvfb. The final selected
run takes 52.012 seconds; reconfigure 0.633 seconds and incremental compile
0.170 seconds. Existing deliberate wait/death/expiry controls dominate this
run. No full all-target rebuild, installation or BSD boot is repeated.

Four scoped Clang ASan/UBSan exercises pass with leak detection enabled:
MFL request policy and actual-X preview/core-state/settings fixtures using the
current instrumented core and MFL. Settings executes the current configured
Python helper. Diagnostics are empty. Python and FvwmForm are not instrumented;
the known native Form/fontconfig/Xft exit-allocation finding remains Later.
These guest runs do not establish native-CPU Linux sanitizer qualification.

Actual-X checks numeric rejection/reset/schema 2, scoped saving and restart,
draft return and later Apply, Revert/no-write, locked-writer partial outcome,
helper-death rollback and orphan-dialog retirement. Core-state independently
checks ordinary-window retirement versus real WindowStyle removal and explicit
destroy commands. Request/read-only/continuation and prior runtime-lease
regressions remain selected. Five native screenshots (full editor plus four
new opacity/draft/outcome views) are captured, hash checked and visually
inspected; labels, controls and failure notice are readable on the test display.
Screenshots are functional review evidence, not hardware/compositor acceptance.

`make check` and diff/whitespace review pass at publication. AsciiDoc manuals
for FvwmDesktop/FvwmMFL/fvwm3 render before/after; mandoc retains the exact same
inherited diagnostics and statuses (Desktop 3, MFL 4, core 3). The receipt records
source/roff identities and diagnostic counts; no clean formatter claim is made.
Portable 177 passes/14 skips belong to merged PR163's shared checkpoint and are
not relabeled as execution for this candidate. No maintenance harness changes
require another `make verify` this batch.

## Source and evidence

The final UI-corrected freeze contains **2212 files**; archive SHA-256:
`c60c310b6966e92bfc494cb94056958159273320163a2b45a745f67d934aefbb`.
All eight changed runtime/test paths are recorded with hashes/modes in the
[receipt](VERIFICATION-2026-10-22-SAVED-OPACITY-PREVIEW.json). The GCC gate guards
complete frozen source hashes/modes before and after; the sanitizer gate guards
hashes. GCC config.h is unchanged across reconfigure. No C/helper/fixture change
follows this final freeze. Final report/status-only amendments are separate.

Canonical host evidence lives under
`builddir/codex-2026-10-22/saved-opacity-preview/ui-correction/`:

| Archive | Payloads | SHA-256 |
| --- | ---: | --- |
| fvwm-x-linux-arm64-svg-evidence.tgz | 61 | 5dfb79b48625f07f6e50163f412161050396cb7ca1775b07085dd9b71256e8f3 |
| asan.tgz | 29 | 6555a02903dca17fa78b86d4b3709f208d311e9cf150e9b60a61600e3e1b6d44 |

Both archives drain through gzip EOF/CRC and verify every regular payload's
length/hash/mode against the complete capture manifest. Commands, logs, source,
configuration, compiler metadata and executable identities are preserved there;
the versioned receipt summarizes them. Initial successful pre-visual-review
captures are preserved separately in the parent directory, not substituted for
the corrected run. Guest captures/duplicate archives and two source archives
retire only after canonical hash/mode proof. Source, incremental builds, stages,
performance references, VM disks and canonical host evidence stay intact.

## Failures retained and limits

Development retains the original registry/schema expectation failures, scoped
reset fixture assumption, incorrect editor-choice/untargeted two-owner query,
reproduced preview-conflict from empty window cleanup, and the probe's incorrect
CARDINAL type for `_NET_CLIENT_LIST` (corrected to WINDOW). Temporary diagnostic
prints used to identify the conflict are removed before freeze. The first frozen
checkpoint passes, then screenshot review produces the two presentation fixes
above; the corrected selected and scoped gates pass. Original logs/input maps,
initial/corrected sources and screenshots remain distinct. No failed attempt is
reported as a pass and no detector suppression is added.

Cycle: PR163 resets to **0/3 merged**, **1/3 after this merge**. Six-target and
alternate-profile integration, full installs/default-X, owner/milestone/release,
native-CPU/hardware/toolkit/compositor acceptance stay pending at their boundaries.
No changed packet/install/discovery/default-server contract requires those early
here. Broader owners #60/#70 stay open. Performance PR159 is **4/5 merged**, **5/5
after this merge**; compare existing qualified binaries next run. This checkpoint
makes no speed/size improvement claim; tuning remains deferred.
