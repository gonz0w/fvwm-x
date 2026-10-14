# Slate SVG artwork — approved 2026-10-06

Owner: THEME-01 / DESKTOP-01, with IMAGE-01 retaining decoder ownership.
The user requested adoption of the richer SVG concepts, replacement of shipped
PNG artwork where appropriate, and actual running-desktop screenshots.

## Adopted scope

- Replace the shipped geometric icons, palette strips, eight wallpapers and
  FVWM-X badge with self-contained SVG. Preserve Slate/Dune/Grove/Ink, light/dark,
  semantic states, native controls, settings and local configuration hooks.
- Use rounded badges, simple recognizable silhouettes, palette accents and
  subtle surface gradients. Keep 24-pixel small symbols within the existing
  launcher rail and 40-pixel Session icons within its current tile geometry.
- Generate committed assets with the existing Python stdlib tooling. Use local
  paths/shapes/gradients only; no embedded fonts, external images, filters or
  scripts, and no generation at login.
- Render wallpaper at the current viewport's actual pixel dimensions using
  existing SVG filename options; refresh it when desktop topology is refreshed.
- Keep PNG encoding for actual screenshots and existing PNG/XPM/client-image
  compatibility. Native titlebar vector primitives remain native drawing.
- Retain the optional SVG build feature: complete artwork uses `-Dsvg=enabled`;
  renderer-disabled builds retain labels/actions and a plain palette background.
  This changes installed default artwork, not arbitrary third-party themes or
  the global image codec/dependency policy.

## Safety and responsiveness

Use the accepted isolated renderer with its existing finite memory/time/input
limits. This theme change does not activate the unfinished asynchronous core
adapter, change worker limits or close IMAGE-01. Static packaged assets reduce
the complexity of the shipped input; user-supplied SVG still needs its owning
security/responsiveness qualification.

SVG decoding exposed redundant same-path launcher icon refreshes. Retain
unchanged icons within an overlay, invalidate on reopening or explicit theme
reload, and update changed palette/state paths. Preserve result-generation and
stale-click rejection semantics; do not shorten or bypass these barriers.

Implementation and native Linux evidence are in the
[SVG theme report](../verification/VERIFICATION-2026-10-06-SLATE-SVG.md).
Six-target qualification, full theme acceptance and native sanitizer CI remain
separate checkpoints.
