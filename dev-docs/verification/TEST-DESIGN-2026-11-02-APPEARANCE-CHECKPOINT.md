# A4 shared appearance checkpoint test design

2026-11-02 · #214 / #215 · Authored design; completed focused Linux results
and retained failures are in the [checkpoint report](VERIFICATION-2026-11-02-APPEARANCE-CHECKPOINT.md). Base: merged PR348 `765191d7b3ba8e6d2f6936cb205f3a853ef83445` plus
the identified A4 working source at execution. Metadata discovery passed and
stopped; it is not a native execution result. No new runtime pass or screenshot
is claimed by this design. The lead records actual inputs, results and gaps.

## Finite execution boundary

The planned 15 shared native jobs comprise five affected Go packages each in
ordinary and race mode (10 jobs), the component-metrics fixture across all
three profiles as one job, ordinary tray, A3 acquisition positive and control
as two jobs, and modern tray appearance. Record the exact package names and
commands in the reviewed execution plan and result. Meson uses the current
profile with PNG, SVG and session D-Bus enabled, NLS and docs disabled. Native C
is uninstrumented; Go race runs are separate. No every-OS, install or full-release
matrix, new dependency or asset adoption is selected. C2–C5 clipboard remains
separate.

Pin the exact source tree and working diff, fixture and configuration hashes,
binary paths and hashes, build profile and environment before execution. Record
the composed core, desktop, worker, tray and private positive/control binaries
actually invoked, not merely intended artifacts. Preserve logs, observations,
configs and captures with their hashes; distinguish failed, skipped and
unavailable checks. Keep the expected bad control separate from product-positive
acceptance. Review the frozen native plan before dispatch; a new input or failure
that changes coverage requires the lead's disposition under the stopping rules.

## Oracles

- Run `theme_composition_live.py --component-metrics` for Slate dark, Dune light
  and enlarged classic. The authored case observes settled actual X menu and
  Form/control geometry plus separate IPC metadata. Compact versus spacious
  changes menu and Form height; strong border enlarges the menu but leaves Form
  geometry unchanged; larger font enlarges menu and Form/control geometry.
  Returning to inherit restores the sampled baseline. Controls must be contained,
  the panel palette retained, and the actual product launcher captured. A
  submitted command alone is not a rendered result. Inspect the three profile
  captures for clipping and legibility and retain their configuration and hashes.
- An existing schedule Form retains its geometry across a density change, while
  a reopened schedule Form adopts it. The settings editor draft is instead
  destroyed on generation change: a stale editor is not the retained witness.
- During an actual resource preview, changed metric and icon intent reports not
  submitted. For ordinary recovery, terminal recovery must precede the new
  launcher/tray publication and reopened geometry must grow. For an external
  colorset conflict, automatic restoration is refused, publication remains
  blocked and the old Form tuple is retained. An early command or changed tuple
  is a counterexample, not a successful recovery.
- The modern appearance case changes `desktop_icon_theme` through independent
  red, blue and empty/hicolor fixture themes. Require matching actual pixels,
  stable live cell identity, unchanged application icon theme, correct resolved
  metadata and mapped tray. Missing/disabled-format and text fallback remain
  independently observable; submitted metadata alone does not establish paint.
- The A3 acquisition positive holds a completed menu batch, changes the theme
  while item and menu identities remain unchanged, releases it and requires a
  fresh exact-path load with zero old-path loads. The deliberately weakened
  control must admit an old path and also reach a distinct fresh load. A stuck
  batch, changed identity, overflow or observer error is not the expected control
  outcome. Pixel screenshots do not prove load admission.
- The ordinary tray case checks Refresh applies changed padding while preserving
  runtime Hide and Show despite opposite stored visibility, and restores default
  padding. Its existing keyboard and fallback assertions remain part of the job;
  an eventual mapped window alone is insufficient.

On every outcome release and disarm the acquisition gate, restore its fixture
profile, dismiss menus and owned Forms, release grabs, retire the private menu,
and terminate and reap owned processes. Record cleanup failures and confirm the
owned lab is stopped. Preserve evidence on failure; do not erase shared caches or
unrelated resources. This plan does not itself establish those cleanup outcomes.

Prior source/build evidence and its limits: [A1 desktop metrics](VERIFICATION-2026-11-02-DESKTOP-METRICS.md),
[A2 desktop icons](VERIFICATION-2026-11-02-DESKTOP-ICONS.md), and
[A3 tray icon observer](VERIFICATION-2026-11-02-TRAY-ICON-OBSERVER.md).
