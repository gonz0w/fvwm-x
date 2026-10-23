# Native browser directory-watch checkpoint

Status: focused qualification passes; ready for review. Owners: FILE-DATA-01 /
FILE-VIEW-01. Base: merged PR173
`be13d246442c0a324f84c6747be268aaa1c3dee1`.
[Contract](../decisions/BROWSER-DIRECTORY-WATCH-2026-10-23.md),
[source-bound receipt](VERIFICATION-2026-10-23-BROWSER-DIRECTORY-WATCH.json).

## Delivered batch

- Opt-in `--watch auto` / F8 observes one directory in an isolated native child:
  Linux inotify through the opened descriptor and BSD kqueue vnode events.
  Filesystem opens, registration and reads stay outside the control loop. The
  original enumeration/action worker remains separately owned. One observer
  slot, one event channel and one latest desired folder retain cancellation/reap
  ordering; superseded events cannot invalidate a replacement directory.
- Native invalidations coalesce into one dirty bit, at most two observer change
  records and one admitted background snapshot per second. Existing user work
  keeps its slot. Reconciliation after registration closes the scan/arm gap;
  30-second reconciliation covers missed events and BSD child metadata limits.
  Existing stable-ID/native publication retains windows, drafts and selection.
- The fixed status label and F8 distinguish off/starting/native/manual states.
  Loss, registration/transport errors and failed automatic refresh retain the
  snapshot and require manual recovery. Successful F5/navigation can re-arm an
  enabled observer. Cancel disables watching and cancels owned children.
  Ten-second startup bounds and sixty-second observer leases retain orphan
  cleanup; background work does not renew the helper's user-idle deadline.

Manual refresh remains the default. No C/core/MFL/Form input, packet ABI,
mandatory dependency, default configuration, file mutation or storage change.
Protected clipboard content/private delivery and general files-provider IPC
remain separate; observer records contain constant metadata states only.

## Focused verification

Five affected Go packages pass on Linux ARM64 (**62 top-level cases**), FreeBSD
ARM64 (**61**) and OpenBSD **8.0 ARM64 (61)**; Linux race repeats 62: **246 final
top-case executions / 20 package outcomes**. The Linux-only extra case validates
inotify lengths/overflow/loss. Vet/build pass on all three. Early Linux/FreeBSD
native observer probes precede browser adoption; their extra executions are not
added to the 246. Qualified Go SDKs are Linux 1.26.9 and BSD 1.27.2.

Actual watcher checks exercise registration, synthetic file creation, directory
rename/loss, finite quiet lease expiry, stale/malformed requests and process
cancellation/reap. Oversize and out-of-order private output are killed/reaped.
Pure controls cover burst coalescing, busy-worker deferral, periodic consistency,
late/stale events, replacement ownership, startup timeout, option validation,
toggle and cancellation. Existing literal-path/actions/model/transport tests
remain in the same affected package gate.

Four composed native-X drivers pass: shipped **Slate dark**, shipped **Dune
light**, a **9x15 classic/minimal** configuration on Linux, and the FreeBSD
fixture. Each captures **27 screenshots**. Automatic addition preserves native
window/child IDs, selection and retained drafts; directory rename reports
manual fallback; F5 recovery re-arms watching. Existing list/grid, 4096-entry,
range/filter, live-update, properties, cancellation and literal-action cases
remain included. New watch views in all three themes and the Dune fallback are
inspected for fit/readability/action reachability. Two unmodified examples join
the [public gallery](../../doc/development-preview.md#opt-in-watching-native-refresh-and-manual-fallback)
with a [separate image manifest](../../doc/screenshots/watch-manifest.json).

The authoritative optional-Go Meson helper target builds a binary identical to
the directly tested Linux helper. Its existing Go cache is reused. The browser
manual generates and renders with host mandoc; directory watching, controls,
options, advisory consistency, failure and ownership limits are documented.
Repository/link/lint/whitespace and README budget checks pass. Native core/MFL/
Form and SDK fingerprints match the preceding qualified checkpoint; no native
C rebuild, ASan repetition, full install or matrix rerun is needed for this batch.

## Retained failures and precise completions

The initial Linux/FreeBSD UI runs pass the new watching cases, then fail to
observe the next large browser: a late idle publication from the closed instance
is mistaken for that new instance. The observer now excludes retired instance
IDs. Only the affected UI/unperformed manual work repeats; passing Go/race/vet/
build results and helpers are retained. This is the sole frozen-input change,
in `tests/live_x/file_browser_live.py`; product/manual inputs are unchanged.

The corrected three-theme run passes, then the Meson linker reports no space
in the Linux guest. Its generated manual succeeds. After complete host-archive
verification, **163** guest capture/archive duplicates retire under the user's
cleanup authorization. Sources, SDK/cache, build outputs and VM disks remain.
Only the pending Meson helper/manual target repeats and passes; the helper is
byte-identical to the tested binary. No UI/Go suite repeats for this storage
repair. Original observer and storage failures remain in the preserved archives.

Preliminary macOS diagnostics use its preinstalled Go 1.27.0, outside the
supported compiler baseline; they are not target qualification. An APFS fixture
refuses a non-UTF-8 temporary filename, corrected to a portable literal quote/
newline. A synthetic stream peer is updated to read the new constant watch
observation. Linux/BSD target gates above run with supported SDKs; existing
non-UTF-8 filename/action acceptance remains included there.

## Source, evidence and remaining gates

Both frozen scopes contain **2298 files**. Initial archive:
`0c602b6e62c1dfbf7a369b3f8c14399e8c545d48718ef7a8fcde7014cf53e810`.
Fixture-only completion archive:
`d34b15796111bfc4188f6d08d9b382970fe9d0bfdbbf9fdc1a7cee36a0b75654`.
All **15 changed Go/manual/fixture inputs** match the published candidate; all
Go inputs match the final scope. OpenBSD records browser-source hashes before
and after its native run. Gzip EOF/CRC, exact inventories and file hashes/modes
verify the four initial/completion archives (**40/34/121/42 files**), the OpenBSD
capture (**13**) and the manual-only completion. Later guide/gallery/receipt
edits do not alter tested runtime/manual/fixture inputs.

Canonical host evidence:
`/Users/cam/DEV/fvwm3/builddir/codex-2026-10-23/browser-directory-watch`.
Owned QEMU jobs are shut down and the OpenBSD ephemeral key is removed. Linux's
shared development container stays available. Failed and passing host evidence
is preserved; guest duplicate retirement leaves native inputs/builds intact.

The shared integration reference remains PR172: **1/3 after PR173, 2/3 on this
merge**. Performance remains PR169: **4/5 merged, 5/5 on this merge**; compare
preserved binaries next run, with no benchmark build or speedup claim here.
Full six-target accumulation and compiler/feature/install/default-X/owner/
release gates retain their required boundaries. Local guest results are not
native-CPU sanitizer, physical display, third-party theme, toolkit/compositor or
assistive-technology acceptance. File-provider/consumer handoff, mutation jobs
and protected clipboard content remain open.
