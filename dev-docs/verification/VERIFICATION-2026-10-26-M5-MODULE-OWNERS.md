# M5 Ident, Buttons and IconMan resource owners

Status: composed focused gate and separate source/raw/report review pass. Owner: CLEAN-01 #34. This complete three-module owner
slice follows merged PR254, `546a7acf506b4082ea45891ec2458b20e52010b0`.
The canonical signature/DCO and reviewed tree
`0caf8644e8c523df7e225cc8c8e0e32ebc1434c0` are verified; its branch is retired.
The broad ownership audit remains open.

## Changes and ownership

Ident frees replaced font/foreground/background configuration and monitor names.
Each popup retires its list and borrowed drawing aliases before its GC, font and
successful named-color references. Retarget and terminal cleanup share that
order. Colorset pixels stay borrowed; default colors use literal fallbacks while
configuration slots own only allocated strings. Terminal scratch/configuration
storage is cleared after popup retirement. List columns borrow live target text.
The existing popup loop does not dispatch target ConfigureWindow replacements.

Buttons releases normal, active, pressed and background picture slots, plus
root/nested container backgrounds. The old background branch released the normal
icon instead, and the iterator excluded the root container. Each populated slot
owns one acquired cache reference even when cached pointers compare equal.
Transparent container backgrounds remain borrowed. Existing swallow/session
cleanup ordering and configuration/presentation remain unchanged.

IconMan releases visible name, visible icon name and monitor strings alongside
its other five owned strings, after existing focus/tip/hash/button detachment.
There is no shared teardown framework, new dependency or configuration migration.

## Old controls and focused results

Before candidate upload, 580 accepted input identities match the Linux source.
Preserved old module objects and headers link with the accepted PR254 shared
library; these are old selected-owner controls, not a freshly rebuilt baseline
qualification. Ident retains 18 selected heap owners after three popups in each
color mode, plus three fonts and three GCs; named mode also retains six acquired
color references. IconMan retains three of nine watched record/string owners.
Buttons distinct pictures release three of seven acquisitions with one excess
release; cached aliases release four of seven. The old Buttons observer defers
actual destruction until reporting to measure the wrong-slot path safely.

The combined checkpoint uses existing GCC13.3 debugoptimized Linux ARM64 and
Clang18.1.3 address/undefined sanitizer caches:

- IconMan's actual record owner passes in both profiles: zero of nine selected
  owners retained; repeated replacement and an empty partial record are covered.
  The sanitizer unit enables leak detection and halt-on-error.
- Eleven actual-X ownership cases pass per profile. Ident covers named/borrowed
  colors, default and one-sided colors, repeated configuration/monitor updates,
  three popups and early EOF before any popup. Every selected heap owner retires;
  each populated font/GC/named-color owner has a matching release. Colorsets
  acquire no named-color owner. Buttons covers distinct and aliased acquisitions
  with both counted and actual immediate destruction. Each has seven releases
  for seven acquisitions, with zero excess/retained references; aliases share
  one picture. The missing-image case releases six acquisitions and records
  exactly one refused PressIcon load.
- Seven existing normal X regression groups pass: Ident configuration,
  transport and bounded queued-X EOF service; IconMan transport/tooltips and
  queued-X continuation; Buttons keyboard/atomic updates and session readiness.
  Exact subcase observations remain in the machine receipt and raw JSON.

All 13 corrected commands pass in 20.578 seconds of fixture command time.
This excludes baseline work, dispatch and review; it is not a WM performance or
general development-speed measurement. The first candidate build fails because
Ident lacks the PictureFreeColors declaration include. Adding PictureUtils.h
corrects it before any candidate runtime passes. The initial failure is retained.

Baseline observer preparation also retains failures: a pre-created output
directory, missing IconMan console initialization, a probe namespace collision,
colorset temporary GCs exceeding the popup-only observer count, and a Buttons
nested-background fixture using the wrong existing directive form. Fixes correct
observer/fixture setup; passed unaffected controls are retained. These failures
do not establish additional product defects. No expected product result is
weakened to obtain a pass.

Sanitized actual-X module runs enable ASan/UBSan halt-on-error with whole-process leak
detection disabled; exact selected ownership assertions remain active. No whole
module/font-cache leak-free claim follows. The Ident list-node observer uses a
size/call-sequence heuristic: its 15 watched allocations are a sample, not an
exhaustive count of every list node. Full-list retirement is source-reviewed;
only the selected sample has runtime ownership assertions. Corrected logs contain no sanitizer
or runtime-error diagnostic. Nonfatal X keyboard diagnostics remain in raw logs.

## Evidence, cadence and limits

All 584 final input hashes/modes match the owned source, including eight changed
production/test/build files. Canonical host evidence is
`builddir/codex-2026-10-26/m5-next-batch/native-evidence.tgz`: 382 payloads plus
manifest; every payload hash/size/mode and gzip CRC is verified. It preserves old
source/objects/binaries, executed observers, failures/continuations, source maps,
compiler profiles, link commands, output identities and raw evidence.
[Machine receipt](VERIFICATION-2026-10-26-M5-MODULE-OWNERS.json).
The 382 verified guest payloads, manifest and duplicate archive are retired
after canonical verification (29,045,007 bytes); source/build caches, stages,
VM disks and performance references remain owned and retained.

No fresh BSD/AMD64, complete compiler/feature/install/theme, physical hardware,
compositor/toolkit or release qualification is claimed. No new UI is introduced,
so screenshots and the new-primitives theme matrix are not required. No global
X-server failure or every module exit path is newly qualified. The PR253
performance reference is 1/5 after PR254 and will be 2/5 after this batch merges;
no comparison is due and no speed gain is claimed.

#6/#9/#25/#34 remain open; #31/#35/#36 remain completed. Parked Backer/pathname
work and deferred Form font cleanup are excluded. The parked request-filter facts
are recorded on #6/#34 and in the tracker; no rejected task is retried.
Requested Astra/high analysis and Sol/high implementation/separate review follow
local routing; actual runtime model metadata remains unavailable/unverified.
