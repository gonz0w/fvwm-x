# Optional desktop pinboard checkpoint — 2026-10-27

Owner: Cam; DESKTOP-ICONS-01 [#64](https://github.com/gonz0w/fvwm-x/issues/64).
Candidate `codex/r1-desktop-pinboard`, base merged PR289
`8d996ce9ac2f5578702e1299d6762be0b4d5c27e`. Implementation and the focused
combined checkpoint pass. Separate integrated source/evidence review accepts tree
`27d675c8944afe4638abc3855caa87abeed7bd50`, including all eighteen captures;
only reviewed status metadata follows. No actionable findings remain. This agent
examination is not human merge approval or complete six-target qualification.

## Delivered behavior

The [accepted contract](../decisions/PINBOARD-2026-10-27.md) contains ten bounded
outcomes. An optional Go owner/isolated worker and native shaped desktop canvas
provide persistent local references, create/relabel/move/unpin/reload, keyboard,
pointer and context actions, named icons, grid placement and explicit recovery.
Ordinary Form dialogs retain full reference details. Unpin preserves targets;
there is no automatic Desktop-directory takeover or default resident.

The native canvas preserves root bindings, prior focus, stacking and wallpaper;
it refuses conflicting desktop ownership. Bounded generation-bound publication
and a renewable ownership lease prevent stale actions/cells. Root drawable
admission and oversized icon cropping keep painting inside actual admitted cells.
The shared work-area extraction preserves Form's callers. No core dispatch,
threading or packet ABI extension is introduced.

See the [manual](../../doc/fvwm-pinboard.adoc),
[three-theme gallery](../../doc/development-preview.md#optional-desktop-pinboard)
and [twelve exact capture identities](../../doc/screenshots/pinboard-manifest.json).
The related [file-job supplement](VERIFICATION-2026-10-27-FILE-JOBS-COMPLETION.md)
finishes both frozen #68 coverage rows without changing file-job runtime; its
five earlier target results are reused. #68 closes on this reviewed PR's merge.

## Focused verification

All runtime execution is in locally emulated Linux/BSD environments. Host macOS
checks do not substitute for native Linux sanitizer qualification.

| Profile | Outcome and scope |
| --- | --- |
| Linux ARM64 GCC | Affected module/Form/Go build; four C policies: pinboard, Form scale, module monitor and module UI scale. Final changed pinboard policy rebuilt and passed. |
| Go1.27.2 | Three affected packages pass normal and race tests at freeze02; freeze03 only adds pure Font quoting, with fresh command-package normal/floor passes. The concurrency source is unchanged; race results are reused by exact diff. |
| Go1.26.9 floor | Three packages pass; later Font quoting command tests also pass on this minimum SDK. No host SDK substitution. |
| Linux native X | Final C6c run passes ownership/lease, SHAPE holes/root input, focus/stacking, foreign desktop changes, context-menu pointer handoff, preview identity and opaque/alpha icon bounds. |
| Slate dark1.25x | Full final fixture14 passes all ten case groups using SVG icons. |
| Installed Dune light1x | Full fixture14 passes with PNG icons, installed owner/browser/native entries. |
| Enlarged Classic2x | Full fixture14 passes with explicit legacy colors and quoted scalable Mono18 font. |
| Scoped Clang ASan/UBSan | Final module and pure policy pass; full fixture14 Slate pass. Core and peer modules are normal builds; leak detection is disabled. |
| FreeBSD15.1-RELEASE-p4 ARM64 | Affected C/Go checks and final default authenticated Xorg800x600 C6c native test pass, including lease/context preview/opaque-alpha checks. This is not the full Go-owner UI or installed profile. |
| Manual/install | Asciidoctor manpage generation and scoped real Meson install pass. Installed Go/browser bytes match builds; native ELF differs only by97 RPATH metadata bytes, with all other bytes identical. Data-only installed CLI passes. Current build disables mandoc, so installed manual profile is not claimed. |
| Repository | `make check test lint` passes:299 Python test entries, with20 explicitly reported host-native Meson/pkg-config skips. Later strict negative-monitor parser self-test passes separately; final doc/metadata lint is checked after integration. |

Each full theme run covers real data-only CRUD and target preservation, missing/
root/literal-byte references, actual icon pixel/readback checks, keyboard/pointer
moves and context menus, corrupt/revision/lock recovery, monitor/scale/work-area/
workspace updates, off-root fallback without preference rewrite, restart and
actual child Wait/reap while the concurrent browser remains alive. Full UI runs
explicitly skip the redundant lease case; the final native run exercises it.
Dune and Classic restart tests record the original owner exiting by SIGKILL
after the fixture's bounded stop escalates (five-second total deadline, half
reserved for SIGTERM). Persistence and subsequent lease/selection cleanup pass;
these runs do not qualify graceful owner shutdown within that deadline. Slate
and sanitizer original owners exit0. Restarted owners exit1 after intentional
native-child loss; duplicate-owner and conflict CLI refusals also exit1.

Final Linux native C SHA-256:
`50ffb2a1ee4730efbe932f5b1f8988c07f684dd42f9201cd6697243206aacc0d`.
Final full-UI fixture SHA-256:
`715647492454488ed401e4f823a42c683bb87a30acf52e6dcb91842493e81528`.
The earlier final native-only fixture13 is
`16fde27085655b4293ac128b2ae1d3593669be55c9679d1ba9e32ef2ed566104`.
FreeBSD also uses fixture13 and its earlier shared harness: later parser-only
negative-coordinate and Go-tail cleanup branches were not executed there.

## Failures retained and corrected

Original failed attempts remain in the canonical evidence. Product corrections
address self-feeding config requests, canonical Font options, unmatched/retired
reply tags, quoted family names, root drawable admission, oversized picture
clipping, implicit pointer grab handoff and preview hit/paint identity. Relevant
old-code reproductions fail before the corresponding fixes; focused affected
checks and final native/UI runs pass afterward.

Fixture corrections remain distinguishable: literal-byte digest, explicit SVG
helper environment, bounded whole-query restart after native invalidation,
Classic actual font and strut timing, LP64 property readback, work-idle/menu
bounds and exact module alias cleanup. No assertion was weakened to claim
acceptance. Early wrong build/source paths, missing include/test executable,
old cached BSD core, and initial filesystem/storage preflight failures are
retained. FreeBSD's current core was rebuilt incrementally; unchanged MFL was
verified. A SHAPE-absent server precondition could not be produced by this Xvfb;
refusal under that server condition remains unverified.

## Evidence and remaining acceptance

Canonical raw evidence is retained in local owning checkpoint
`builddir/codex-2026-10-27/r1-file-jobs-followup`. Archive verification checks
exact payload membership, hashes, modes and gzip CRC; Linux also checks1166
source inputs including all583 Go files. Runtime-source closure aggregate:
`aecd4e088c887d4002a13251ad9fda1115805bd57b52e8f46e8dd1a665d18eb8`.

- Linux:2393 payloads, archive SHA-256
  `ccdd1d91c3126ab94fa74e96b134f464e34a466931c407a9073cdb8a42d95c98`.
- FreeBSD:137 payloads, archive SHA-256
  `be88b534570510b88e5c6c78d6322439ee6bba5242d6657e464024c8e9ca8660`.
- Install records and final identities are separately retained on the host;
  current stage, builds, source, caches and VM disks remain intact. Only exact
  verified guest evidence duplicates are retired; owned guests stop afterward.

Keep #64 open for Linux AMD64, FreeBSD AMD64 and both OpenBSD affected gates,
full BSD owner/install profiles and remaining release acceptance. Cell labels
can clip; the full manager remains available. Arbitrary themes, physical/input/
assistive hardware and external compositor/toolkit behavior are not qualified.
These scoped sanitizer runs establish neither native-CPU execution nor leak
freedom. No speed or resource improvement is inferred from screenshots or
whole-guest panel counters. Performance comparison is not due:1/5 after PR289,
2/5 after this next merge. #257/#258 remain parked.
