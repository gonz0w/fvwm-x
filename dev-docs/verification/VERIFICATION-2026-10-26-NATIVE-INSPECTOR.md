# Native inspector inventory verification — 2026-10-26

Base: `c540a99c810c02be1df62b7d4b22cf9dec929519` (merged PR265). Owner: Cam.
Candidate: the exact 34-file SHA-256 input maps in the [receipt](VERIFICATION-2026-10-26-NATIVE-INSPECTOR.json).
The final map is checked against the owned checkout and native source before archival.

## Results

One reused native Linux ARM64 cache, GCC 13.3.0, Go 1.27.2 and Meson 1.7.0.
This is native Linux execution in the Podman VM, distinct from emulated BSD
or macOS execution. No new full platform/install campaign was run.

- Affected normal builds pass: Form, Desktop, worker, Prompt and Form policy.
- Seven affected Go packages pass with race detection. After the focus correction,
  only the Desktop build and controller race package were repeated; other inputs
  remain bound to the passing pre-focus map.
- Form input policy, scoped native ASan/UBSan with leak detection, generated
  command parity, legacy native Form input and actual Prompt regressions pass.
- All six inspector sections pass across Slate dark, Dune light and classic
  enlarged-text configs: bounded sources/overrides/truncation, literal values,
  filtered command discovery, actual-pixel layout, unchanged settings/files,
  keyboard navigation/focus return, absent/lost/restarted MFL, failed refresh
  retaining old data and WM_DELETE during pending work.
- Three AsciiDoc manuals render to man pages; host mandoc rendering passes.

All nine settings/theme/layout captures were visually inspected. Inspector text
and controls fit all three configs; the existing enlarged desktop bar still clips
some captions. This is a known separate layout gap, not broad accessibility
acceptance. Captures and profile receipts are in the canonical archive.

## Failures, review and limits

The first native fixture expected a colon that Send_ConfigInfo normalizes away;
the observation decoder was corrected. The next run exposed lost keyboard focus
on Form replacement under the baseline GrabFocusOff style; a lazy view-specific
focus rule and regression test fix it. The final three-profile run passes.
Initial review found lossy filter quoting and a close/frame-token race; literal
hex initializers and stable view close identities fix both.

An initial pure model test caught wallpaper appearance auto validation; the
existing appearance registry now validates it. A Python parity test initially
failed before assertions due to an import path; corrected. macOS socket tests
cannot bind in the sandbox and Form compilation lacks X11/Intrinsic.h; native
Linux race and C gates cover those paths. No package/stub workaround was used.

No atomic desktop snapshot, exact runtime source origin, per-widget/window
attribution, full owner closure, helper-death Form retirement, six-target
qualification, release/install acceptance or measured RSS/performance gain is
claimed. The viewer is opt-in with one bounded job and one coalesced refresh;
there is no new dependency or resident. PR264 periodic comparison is not due.

## Static costs

Comparable reused-cache file sizes (debug/Go metadata included):

| Binary | Before bytes | After bytes | Delta |
|---|---:|---:|---:|
| FvwmForm | 1,619,032 | 1,627,040 | +8,008 |
| FvwmDesktop | 5,300,364 | 5,557,105 | +256,741 |
| fvwm-desktop-worker | 4,947,539 | 5,810,081 | +862,542 |
| FvwmPrompt | 5,066,806 | 5,071,777 | +4,971 |

ELF text/data/BSS, binary hashes and dynamic dependencies are retained in the
receipt. Core is unchanged at 5,387,608 bytes. File/section bytes are not RSS;
the worker/catalog functionality increases binary size and no memory-parity
claim is made.

## Evidence and upkeep

Canonical host archive: `/Users/cam/DEV/fvwm3/builddir/codex-2026-10-26/r1-native-inspector/native-evidence.tgz`.
SHA-256 `4b8d779733f5b9b2f87a5a13cc1b2e7c8bbc14dcb4f5c2927a093ab719af8ecb`; 1,145,970 bytes; all 944 inventoried files verified.

Requested routing followed PROJECT.md: bounded model/UI/Form implementation
and separate read-only final review, with lead-owned native execution. Requested
model/effort routing is not independently attested runtime metadata. Final independent review by `/root/r1_inspector_review` found no unresolved
findings across the complete batch, source maps, 944 payloads, documentation and
three representative captures. Requested routing: gpt-6.1-sol/high, effective
runtime settings unverified. Final input-map SHA-256 is
`62bfd6080a02634685028268e4a51244375de3ce931dacfc3399ce4c0c7574a3`.
The two initial P2 findings are resolved; no suites/native/network/resource
control or edits were performed by the reviewer.

All 944 guest files were rechecked against their inventory before removing only
the guest evidence directory/archive (5,605,821 bytes). Host archive, source,
build caches, VM disks and performance references remain. The owned container
and VM are stopped after both Podman connections reported no running containers.
