# Files-menu browser handoff — 2026-10-28

Owner: Cam; FILE-INTEGRATION-01 [#67](https://github.com/gonz0w/fvwm-x/issues/67),
R1 Major portable consumer adoption. Base merged PR291
`14e4b2cdf435bd6ba86d4af409aa14ba5b18d553`; candidate
`codex/r1-file-integration`. Focused checks and separate integrated source/evidence review pass.
PR291 closes #64; #65/#68 were already closed. Their stale tracker rows are reconciled.

## Delivered behavior

- Files offers **Open this folder in browser**. Its opaque current-menu token
  binds the committed directory to literal `--reuse -- PATH` arguments. A stale
  menu cannot open a newly committed folder. Existing matching-provider refresh
  and core-owned presentation are reused; folder menus remain the default.
- Explicit `FVWM_FILE_BROWSER` wins. Otherwise the desktop prefers its adjacent
  development peer, then discovers the installed command on PATH. Meson installs
  Desktop in its module directory and the browser in bindir. Missing optional
  peers do not prevent login; self-worker/capture resolution is unchanged.
- At most eight browser launches remain outstanding until their exact Wait
  results are consumed. One waiter releases the existing global application
  slot; only the resident loop changes browser admission and emits notices.
  Start is not completion. Independent browsers have no controller timeout and
  survive controller retirement. Failed/uncertain handoffs never trigger retries.
- Files-menu titles and entries use at most48 sanitized display runes, including
  directory prefix and ellipsis. Original paths/names, indices, tokens and full
  argv remain unchanged. Actual Classic menu width falls from2162 to722 on the
  same1440-pixel-wide profile. This is neither arbitrary-font/screen fitting nor
  a WM performance result.

No configuration migration, dependency, resident service, MIME-default write,
general association engine, bus or FileManager1 adoption. The
[desktop manual](../../doc/FvwmDesktop.adoc) and
[browser manual](../../doc/fvwm-file-browser.adoc) explain use and recovery.

## Source-bound focused verification

The existing owned Linux ARM64 container runs kernel
`6.18.10-200.fc43.aarch64` and Go1.27.2. The unchanged qualified minimum-SDK
Go1.26.9 profile is reused. This is virtualized development evidence, not fresh
native-CPU, physical or complete release qualification.

| Check | Actual result |
| --- | --- |
| Before implementation | Old production code fails the isolated menu-action regression; exact original package/test hashes are retained. |
| Incremental build | Current Desktop, desktop worker and browser build through the existing Meson/Ninja cache. Only affected Desktop rebuilds repeat after corrections. |
| Go policy/lifecycle | Final controller has150 named case/subcase passes in each normal, race and minimum-SDK run. It covers stale/foreign/closed tokens, literal long/non-UTF8 argv, eight live/queued launches, exact completion identity, nonzero exits, no retry, post-Close Wait, display boundaries and preserved references. |
| Applicable earlier Go results | Main's4 normal/race/floor cases, unchanged browser's130 normal/race cases and runtimepath's2 normal/race cases remain source-bound to their earlier passing packages. Four-package vet and final affected-controller vet pass. These are composed counts, not a newly repeated full suite. |
| Native themes | Slate dark, Dune light and Classic mono18 all pass real keyboard menu activation, correct literal folder, one provider/window after reuse, deiconify/present, stale-token refusal, missing/bad-interpreter/nonzero-launcher notices and usable menus afterward. All24 captures are retained; three are published and separately inspected. |
| Installed consumer | Real scoped Meson `desktop-controller,file-browser` install and desktop-entry bytes pass. With the helper override absent and stage bindir on child PATH, the installed module launches the exact installed browser argv. Reuse preserves the provider/window; native Escape retires it and the parent reaps it while Desktop/core remain alive. |
| Manuals/repository | Both affected AsciiDoc manpages render without stderr. `make check lint`, Python syntax and whitespace checks pass. |

The three corrected theme workflows run together in9.40s; the final installed
continuation takes2.40s on this lab. These are test-run durations, not a window
manager latency benchmark or a general timing guarantee. SDK compilation,
source preparation and review are separate work.

Installed profiles use unchanged build-tree core/native modules and source theme
data around actual installed Go entries. This is not a full system install,
new default handler or fresh six-target matrix. The portable process APIs retain
their earlier BSD qualification; broader #67 support remains open. No C changes
call for another C sanitizer run. The new concurrent completion path has actual
Linux race coverage. No unknown post-admission transport fault is manufactured
on real X; the unchanged Go reuse policy supplies that refusal coverage.

## Retained failures and corrections

The initial test setup used noexec `/dev/shm` for Go temporary executables.
Builds passed; product tests did not execute. Using the existing executable
filesystem corrects setup without changing mounts or permissions.

The first actual normal run finds a notice-oracle failure:72-rune diagnostic
wrapping splits `unknown outcome` across rows. Three short lines preserve the
meaning, make the notice readable and satisfy the unchanged assertion.

Slate/Dune pass the first UI run. Enlarged Classic fails the existing pixel
sample guard. A diagnostic captures the actual2162x280 menu and root image
before the same guard fails; the Files-only display correction then passes all
three themes. Pixel bounds and test data remain unchanged.

The first installed run creates actual browser Form windows but its observer
checks only the desktop's main-thread children. The corrected bounded scan
revalidates parent/child PID, start time and argv across all desktop threads.
Final receipts observe browser3565 under Desktop3515, created from thread3519,
while the main-thread children list is empty. The original six-second predicate,
exact executable/argv, single-browser, reuse and Wait assertions remain. Only
this installed continuation repeats; passing Go/theme/build/install results stay.

## Evidence, captures and remaining acceptance

The [compact receipt](VERIFICATION-2026-10-28-FILES-BROWSER-HANDOFF.json) binds
runtime freeze04 and installed-only observer freeze05 to exact inputs/results.
The final guard covers1852 build-source files and586 Go files. Development
tracking/gallery changes after the freeze do not replace tested runtime bytes.

Canonical archive SHA-256:
`5b5f6a4c8f16324856af64f8ff17555dcc4f5b8b071663e00a366687f0b15f3c`.
All465 regular members, hashes and permission modes, plus gzip CRC, are verified
before exact guest duplicates retire. The5 MB host archive preserves passed and
failed attempts. Source, builds, install stage, caches and VM disks remain;
the owned Linux lab stops normally. Other owned labs were already stopped.
Canonical local owner: `builddir/codex-2026-10-28/r1-file-integration`.

[Three inspected captures](../../doc/screenshots/files-browser-handoff-manifest.json)
show readable menu actions/shortened titles in Slate and enlarged Classic; Dune
shows the browser after the receipt-backed reuse workflow. A still image does
not prove reuse. Panel counters are whole-guest values. Assistive/IME, arbitrary
themes/fonts/screens, hardware and broad release acceptance stay with their owners.

#67 remains open for its complete role/adapter/support acceptance, including
FileManager1 under #77 and file-transfer qualification. #51's broader consumer
acceptance is unchanged. #257/#258 remain parked. Performance comparison is not
due:3/5 after PR291,4/5 after the next implementation merge.

## Separate review

`temper_reviewer` accepts integrated tree
`ed3fe696b2f81c702833434bf5e758feeeb34a19` against the stated base,
including all23 changed files, runtime/fixture inputs,465-member archive,
actual results, manuals and inspected captures. No actionable findings remain.
The reviewer confirms #67's reuse/present criterion can be marked complete;
its role, adapter and full support criteria remain open. Subsequent changes
record this review/status only. The PR records the final commit/tree.
This is a read-only agent second examination, not independent human assurance;
requested roles were available and backend model/effort is unattested.
