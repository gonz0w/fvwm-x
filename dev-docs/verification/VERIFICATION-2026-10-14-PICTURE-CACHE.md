# Image-cache variant lookup — 2026-10-14

Owner: CACHE-01 [#33](https://github.com/gonz0w/fvwm-x/issues/33).
Status: combined checkpoint locally qualified; ready for PR review.
[Combined machine receipt](VERIFICATION-2026-10-14-PICTURE-CACHE-COMBINED.json).
Broader path/population/cost profiling remains open. The user deferred the
separate OpenBSD/VESA cause investigation #93 until much later.

## Change and review boundary

A production lookup for the oldest of sixty-five same-name image generations
performed sixty-five file-stamp checks. Variant attributes, scale, target width,
target height and generation now reject incompatible candidates before the
stamp read. The same lookup performs one stamp check. Name/path resolution and
the stamp check for every otherwise matching candidate remain in place.

The three related stories are the controlled baseline, the cheap-key filter and
reload/refusal/retained-pixel coverage. Review of the existing stamp helper
confirms that it does not mutate picture records. Cache keys, list order,
reference counts, loader/view ownership and generation semantics are unchanged.
No configuration migration, new cache index, persistent filesystem cache,
mandatory dependency or installed option is introduced. This count reduction
does not establish a desktop latency or CPU improvement.

The affected implementation is `libs/Picture.c::PCacheFvwmPictureAtScale`.
The [new unit probe](../../tests/test_picture_cache.c) checks a 65-variant hit,
each key field, a different pathname, failed replacement, disappearance/retry
and SVG option-prefix stamp paths when enabled. The
[native-X probe](../../tests/test_resource_cache_x.c) rewrites a valid bitmap,
checks new and retained old pixels, deletes the file, checks lookup refusal and
continues using retained pixmaps. Its
[driver](../../tests/live_x/resource_cache_live.py) can borrow an authenticated
existing display without owning or stopping that server. Meson registers the
new controlled probe in the existing library test family.

## Frozen source and combined local qualification

Code/build scope is five paths over accepted base
`4fecfda6c288e4d1dc460144173303252a1d9a94`. The frozen archive is
`b50d63422e7eb326471a59c897b190d7faee40b5a5131ce94f547fc7d13f7553` with 1936 regular-file hashes/modes.
Only the owning Meson block is included; unrelated shared-checkout desktop,
parser and test edits are excluded. All five staged Git file contents match the
tested source snapshot. Existing accepted builds, sources and guest disks remain
preserved; this batch owns its incremental build directories.

Every profile below has passing coverage of its complete registered suite and
staged install. OpenBSD AMD64 composes 235 original passes with one unchanged
targeted policy retry. Linux AMD64 Clang composes 247 original passes with one
unchanged menu-interaction retry. Both initial failures remain recorded below
and in the machine receipt. No full matrix was repeated for either retry.
Installed native executables and catalogs are compared with their own tested
build outputs after the normal install-rpath transformation, then independently
checked from archived installed bytes. Standard profiles disable optional Go;
unchanged Go compiler/helper qualification retains its earlier owner and scope.

| Target / compiler or feature profile | Passing entries | Installed code | Catalogs |
| --- | ---: | ---: | ---: |
| fvwm-x-linux-arm64-svg/gcc | 248 | 15 | 11 |
| fvwm-x-linux-arm64-svg/clang | 248 | 15 | 11 |
| fvwm-x-linux-arm64-svg/software | 227 | 15 | 11 |
| fvwm-x-linux-arm64-svg/minimal | 196 | 15 | 0 |
| linux-amd64/gcc | 248 | 15 | 11 |
| linux-amd64/clang | 248 | 15 | 11 |
| freebsd-arm64/default | 236 | 15 | 11 |
| openbsd-arm64/default | 236 | 15 | 11 |
| freebsd-amd64/default | 236 | 15 | 11 |
| openbsd-amd64/default | 236 | 15 | 11 |

The eight standard compiler profiles cover all six Linux/FreeBSD/OpenBSD
AMD64/ARM64 targets and pass 1936 entries. The Linux
ARM64 software and minimal profiles pass another 423.
Together they verify 150 installed native-code identities
and 99 catalogs. These are native target binaries
running inside the local development VMs/container; emulated timings are not
physical-desktop performance measurements.

Four owned default BSD Xorg/Xenocara servers each pass the cache pixel/reload
probe and installed-WM geometry smoke checks at 1× and 1.5× scale:
12 fixture runs, four unauthenticated-client refusals,
3 clean server exits and retained vendor/dependency
observations. OpenBSD AMD64 remained alive through all fixtures, then required
the existing harness's bounded SIGKILL during teardown; that is recorded as
forced cleanup, not a clean exit. These are routine acceptance starts, with no
further #93 source/cause investigation.

Linux ARM64 Clang ASan/UBSan passes two entries with the whole supporting
library and probes instrumented. The controlled cache unit enables leak
detection; the real-X resource test disables it for existing process/global
font/X allocations. Both halt on sanitizer errors. The earlier Linux AMD64
two-object scoped sanitizer controls remain separate focused evidence.

The isolated PR checkout passes `make verify`: 166
portable checks and 11 explicit native-Meson skips on
macOS. Its changed/staged patch and current local documentation links pass.
The earlier [focused receipt](VERIFICATION-2026-10-14-PICTURE-CACHE.json) records
the initial five-target stage; its then-pending combined gate is superseded by
the combined receipt above.

The per-profile command sequence is `meson setup` (debugoptimized, live-X
enabled, optional Go/manuals disabled), `meson compile -C BUILD -j 2`,
`meson test -C BUILD --no-rebuild --num-processes 2 --print-errorlogs`,
`meson install -C BUILD --no-rebuild --destdir STAGE`, then installed
`fvwm3 --version`. The receipt preserves each exact setup, compiler and
dependency environment. The default-server batch runs
`resource_cache_live.py --display :199` and
`geometry_transition_live.py --smoke --smoke-quarters 4` / `6` against the
installed WM/MFL. Sanitizer commands and effective options are in the receipt.
All commands/logs and executed orchestration scripts remain in the six local
archives; the source files above provide the maintained tests.

## Retained preparation failures and limits

- An early direct Linux link referenced a vanished historical private XPM
  prefix. Already verified package bytes were restored to this batch's private
  directory, with package hashes/metadata checked; no system install changed.
- Git archive modes retained group-write bits that safe extraction removed.
  The final archive normalizes those modes while retaining executable bits;
  the original preflight failure and archives remain.
- The initial Linux ARM64 VM start did not persist after its launching command.
  Keeping its existing VM in a persistent terminal restored the sixth target;
  no missing-target pass was inferred.
- The instrumented Clang link needed its installed compiler runtime for an
  existing 128-bit arithmetic helper. `meson configure` applied
  `-rtlib=compiler-rt`; the same objects relinked and both checks passed with
  undefined-symbol enforcement retained. Original link attempts remain.
- Shared-root `make verify` stopped at pre-existing generated parser whitespace
  outside this batch. Those files remain unchanged; the isolated PR checkout's
  complete `make verify` passes.
- The first OpenBSD AMD64 default-server scheduler reached its waiting deadline
  before the standard suite completed; no server fixture had started. That
  timeout remains in local evidence. The standard suite was left running and
  only the pending default-server acceptance was dispatched afterward.
- OpenBSD AMD64's initial full suite passed 235 entries and failed one existing
  `ci-asset-contract` subprocess at its two-second deadline. The unchanged
  failed entry passed alone with the same deadline. The complete initial log,
  one-entry retry, exact composition and resumed install remain archived;
  neither product nor test source was changed for the recovery.
- Linux AMD64 Clang's initial full suite passed 247 entries and failed the
  existing menu-interaction submenu timing assertion. That unchanged fixture
  passed alone with the same assertion. Both sets of menu logs and the full
  initial/one-entry retry records are retained; no timing or product source
  changed, and no cause is inferred from the successful repeat.

Full native-CPU release sanitizer execution, physical GPU/input/toolkit and
third-party theme evidence remain with their existing acceptance owners. This
component does not close all CACHE-01 profiling or establish a server repair.
No hosted CI, status publisher, release or upstream notification is introduced.

Ignored local evidence: `builddir/codex-2026-10-14/picture-cache/`, including the
source manifests, commands, original attempts, staged-code identity check,
six completed artifact archives and strict combined validator. The combined
receipt records archive hashes and exact source/profile/results; complete gzip
streams and actual installed bytes are validated locally.
