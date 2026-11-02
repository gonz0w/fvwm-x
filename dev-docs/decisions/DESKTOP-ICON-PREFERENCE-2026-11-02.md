# Desktop icon preference

2026-11-02 · THEME-02 #214 / ICON-01 #215 · Work in progress

Add `desktop_icon_theme` to the existing settings store and compact desktop
editor. Its `theme` value is empty or at most 128 ASCII letters, digits, periods,
underscores and hyphens; `.` and `..` and paths are invalid. Empty selects the
reserved SlateTray hicolor baseline. Explicit names select themes independently
of the application icon theme in XSETTINGS and explicit installed-menu CLI
profiles. Module overrides retain precedence. No new icon assets or dependencies
are selected.

The controller submits IconTheme before Refresh in the atomic configuration for
the owned SlateTray alias. Refresh preserves runtime Show/Hide/ToggleAt state;
explicit Reload retains its existing stored-visibility semantics. A change in
effective icon choice refreshes that alias;
an unchanged choice does not. During active preview recovery the old submitted
icon theme remains until guarded replay, and a conflict blocks publication.
Metadata describes submission, not rendered output. Missing themes retain the
existing hicolor, unthemed and text fallback; foreign XEmbed client pixels are
untouched.

Native item and menu icon results retain a local acquisition generation as well
as their item and menu identities. A theme, size or scale change invalidates old
results before picture loading, including when values return to earlier choices.
The service synchronizes changed local icon tuples before admitting results,
even when the bus model has no dirty event.
Generation overflow fails closed until the owner is recreated. Worker and picture
ownership remain unchanged; this does not unlink a shared disk cache.

Exact historical schemas 1 through 10 load without rewriting; schemas 1 through
9 gain the three version 10 metrics, and schemas 1 through 10 gain the empty
icon theme. The next successful save writes schema 11 with thirty preferences
and the retained wallpaper. Older helpers cannot read schema 11. The earlier
metrics decision records its original version 10 boundary.

Five affected offline Go compile/link commands and two affected corrected Linux C object
compiles pass. The host C attempt remains unavailable due to missing Intrinsic.h.
Both separately reviewed build-only labs are stopped; no runtime/tests or installs
ran. Final source/evidence review remains pending. The authored native delayed
result design still needs fixture implementation/execution. Runtime, race,
actual-X theme/fallback and three-theme/enlarged-text captures join the A1–A4
shared appearance checkpoint; clipboard qualification remains separate.
[Build evidence and limits](../verification/VERIFICATION-2026-11-02-DESKTOP-ICONS.md).
This slice does not close all of #214 or #215.
