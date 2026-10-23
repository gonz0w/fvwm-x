# Collection and file-browser foundations

Date: 2026-10-23. Owners: FILE-VIEW-01 / FILE-DATA-01 / FILE-ACTION-01.
**Focused checks pass; full browser handoff and shared integration remain pending.**
Base is merged PR164, `6f7e964a0e6a41d1e3e89f2e9fd9898e764bf650`.
[Source/commands/identities receipt](VERIFICATION-2026-10-23-FILE-BROWSER-FOUNDATIONS.json).

## Delivered foundation

Three related supporting contracts have one working native paged consumer:

- Domain-neutral metadata/selection: stable IDs distinct from labels/positions,
  validate-before-publish replacement, bounded selection and surviving-ID focus/
  selection preservation. At most 4096 entries, 256 selected and twelve visible
  rows. Clipboard can reuse metadata policy; protected payloads remain separate.
- File adapter: original filename bytes, directory/entry observations and stable
  IDs. Local directory enumeration/metadata stay in a separate process; no
  filesystem work enters the WM or browser control loop. One child/result slot
  and one latest navigation replacement prevent supersession growth. Ten-second
  timeout/cancel retain ownership until reap; kernel calls may delay termination.
  Refuse an over-limit directory as a whole, including hidden entries; no silent
  truncated prefix is sorted/published. Private records are limited to 4 MiB.
- Typed actions: current rendered controls and opaque IDs are required; recheck
  root/entry observations before open/navigation. Literal executable arguments
  preserve quotes, newlines, metacharacters and invalid UTF-8 bytes. Symlinks and
  special-file activation are explicitly unavailable. Application started is a
  handoff, not proof of opening/completion or an atomic pathname lease.

The optional stdlib-only Go executable fvwm-file-browser reuses native FvwmForm
and existing MFL framing/serialized writes. FvwmCommand's unchanged endpoint
policy moves into its existing transport package and keeps original tests.
No mandatory toolkit, new dependency, packet change, core policy change or
replacement of existing folder menus/defaults occurs. Per-render aliases bind
native resource/configuration lifetime; callback generations reject old controls.
The example provides Home/Up/Back/path, hidden toggle, manual refresh, paging,
selection, open, terminal, cancellation and close. Five-minute idle/native Form
bounds are documented, including window-manager close leaving a helper until
its idle deadline. ASCII display escapes never replace original identity bytes.

This is supporting work and an actual usable example, not complete FILE owners.
Virtual scrolling/grid, generic files IPC provider, full browser handoff,
mutations/watchers, clipboard transfers/retention/memory protection/encryption
remain with their owners. Ordinary file-path Echo/Form callbacks are unsuitable
for sensitive clipboard payloads. No protected-content claim is made.

## Executed and retained scope

| Gate | Passing scope |
| --- | --- |
| Linux ARM64 Go 1.27.2 | Five packages / 21 top tests through the registered policy entry; vet passes |
| Native FreeBSD 15.1 ARM64 | Same five packages / 21 top tests, vet, actual build and scanner CLI |
| Native OpenBSD 8.0 ARM64 | Same five packages / 21 top tests, vet, actual build and scanner CLI; pinned offline SDK, current 8.0 cached image |
| Linux Go race | Five packages / 21 top tests, no failures/skips or race diagnostics |
| Existing command-client entry | One successful registered pass retained after unchanged Go/client inputs and executable identity checks |
| Native Linux X | Staged executable runs the real owner/worker/Form/core/MFL; paging/selection, folder navigation, exact literal argv, stale refusal, blocked FIFO/cancel/reap/refresh, responsiveness and close pass |
| Scoped packaging | file-browser tag installs exactly executable + man page with matching built identities; disabled-Go configure/docs omit both without C compilation |
| Local portable upkeep | make verify: 177 passes / 14 explicit unavailable-native-tool skips; final repository/diff/link review |

The three ordinary target profiles plus Linux race produce 20 package outcomes
and 84 top-test executions, not 84 distinct tests. New cases cover empty/hidden/
over-limit directories, invalid filename bytes, replaced roots/items, symlink
refusal, atomic model replacement, stale/off-page/duplicate selection, worker
oversize/malformed records, cancellation/reaping, one latest navigation, cancelled
output and literal arguments. All Go runtime/test inputs match the final source
on the two native BSD runs; their private ordinary builds do not claim a native
Meson/full install/default-server matrix.

Four final native screenshots are captured, hashed and visually inspected. The
paged consumer/escaped labels and all controls fit the 1440×1000 test display.
The final native-X driver takes 0.988 seconds, scoped race 9.581 seconds and
incremental compile 0.124 seconds; these exclude development and environment
preparation. They do not establish responsiveness on hardware or a full browser
performance result. Existing core/MFL binaries are exactly qualified PR164;
all frozen C/header inputs are unchanged. The current native Form identity is
recorded. No native C rebuild/sanitizer suite repeats for this Go-only batch;
the known Form exit-allocation gap stays CLEAN-01 / Later.

The new optional manual is built by Meson, staged with its executable and rendered
by native FreeBSD mandoc. Status 3 retains the Asciidoctor preamble's mso/LINKSTYLE
errors and section-spacing warnings, plus five source-line-length style notes.
Rendered text/options/examples are inspected; this is not a clean-lint claim.
The manual manifest and existing Go-enabled docs switch share selection; no
build/release default is changed.

## Source, failures and evidence

The corrected 2225-file archive SHA-256 is
`cd3d627371310523dba9579fedf6de410d29dabd9ce69ee43a3e84fbd7f93599`.
Sixteen changed runtime/build/test input hashes/modes are in the receipt. The
only correction after the first freeze is test registration: Go requires -C
before -json/-count. Product/fixture/Go-source bytes remain unchanged; the
already successful command-client entry is retained with an unchanged-executable
binding. Only the failed policy entry and remaining race/install/UI gates run
in continuation. Final updates are documentation/status/receipts only.

Original development captures retain compiler/import and macOS non-UTF-8-name
limitations, use of the disabled-Go profile, partial cache exhaustion, native
probe XID reuse/retired-window observation, incomplete button-press/release
synchronization and probe field/conditional mistakes. Per-view resource aliases
and display escaping are qualified before freeze. A private BSD runner quoting
error is corrected before its unchanged native checks. Failed attempts are not
passes. Exact borrowed-window BadWindow reads are handled only by the observer;
unexpected observer errors are empty in the final actual-X run. Native buttons
wait for their real pointer grab before synthetic release.

Canonical artifacts live under
`builddir/codex-2026-10-23/file-browser-foundations/`:

| Archive | Payloads | SHA-256 |
| --- | ---: | --- |
| evidence.tgz | 32 | 30c08da71657ccc398411ac53ce34aa5c4488b5f195cbce9dcb974eb90b14b5d |
| freebsd-final.tgz | 11 | 5f4d63977929465896d93eaa281d33bd639196ba1c52d6cb2f2c88db140ab78b |
| openbsd-arm64-final-job/guest-evidence.tgz | 15 | 87468d70d6aaeed0d1ae06b157732d292828fc40f5739afbdd3524d2d2bcf57a |

All canonical payloads drain through gzip EOF/CRC and verify complete
length/hash/mode proofs. OpenBSD compares 439 source hashes before/after and
verifies every captured payload against guest hash/stat lists. Its run and
QEMU exit are zero and the job key is removed. FreeBSD guards 505 source files;
Linux guards the complete frozen source before/after. SDK identities are pinned
and reused; builds never fetch a module/toolchain. Completed guest capture/
archive/source-archive duplicates retire only after canonical proof. The scoped
install stage is moved intact to its preserved owner path. Source, SDK/cache/
builds, dedicated performance references, VM disks and host evidence remain.

## Remaining work and cadence

Integrated reference remains PR163 (`cb380f9b3ccd910678a767d4d768731c748edf81`).
PR164 is 1/3 merged; this batch becomes 2/3 on merge. Shared six-target/AMD64/
alternate-profile integration and full installs/default-X/milestone/release
acceptance remain due at their boundaries. Native-CPU Linux sanitizers and
hardware/toolkit/compositor evidence are separate. No broader owner is closed.

The due [PR159→PR164 observation](PERFORMANCE-2026-10-23-PR164.md) uses existing
qualified binaries: command-latency ranges overlap, CPU changes are mixed, and
core/MFL allocated sizes are recorded without a general speedup claim. PR164
resets performance to 0/5 merged, 1/5 after this batch. Further opacity expansion,
general maintenance and performance tuning remain deferred. Next work advances
the collection/provider handoff and clipboard-specific transfer/lifetime layers.
