# Semantic theme composition and independent wallpaper

THEME-02 #214, R1: one checkpoint implements three related stories over the
accepted registry/controller. The complete theme owner remains open.

1. A pure, versioned projection validates all eight palette colors before
   publishing the existing fourteen colorsets (0–8 and 10–14). Named surface,
   muted, raised, border, accent, menu and panel roles preserve the existing
   mapping. Composition version 1 is separate from IPC version 1 and file schema 3.
2. `wallpaper_palette` (`follow`, `slate`, `dune`, `grove`, `ink`) and
   `wallpaper_appearance` (`follow`, `light`, `dark`) make the existing palette
   wallpaper independent of UI colors. Defaults follow existing behavior.
   `keep_wallpaper` has strongest precedence; new choices save while retained,
   then apply when retention ends. Module overrides retain their existing scope.
3. The existing menu, grouped editor and IPC metadata adopt both preferences.
   Metadata exposes requested effective choice, source, resolved selection policy,
   explicit/follow/retained resolution and retention constraints. The editor shows
   a retention hint; menu rows show resolution. No second store or service is added.

The shared wallpaper resolver serves candidate preparation and runtime painting.
The runtime invalidation key includes resolved wallpaper after the existing
preview-owned positions. Palette projection is preflighted before persistence or
commands; failed preparation does not advance saved/effective state. Authoritative
X/application state and dispatch ordering stay with existing owners.

Historical schema 1 and 2 files require their exact historical fields, gain follow
defaults and load without rewrite. Successful save writes complete schema 3;
unknown/invalid files remain preserved. Unversioned data retains its original
normalization. Existing directives, bindings and config formats remain usable;
new files cannot be read by older helpers. No personal config rewrite is needed.

Query metadata is stable for a settings generation: resolved `auto` means the
existing schedule policy, not a current painted phase. The existing minute tick
resolves actual light/dark appearance. This adds no clock revision, watch protocol
or claim that asynchronous submission proves repaint.

Typography, density, geometry, high contrast, icon roles and bounded asset
invalidation remain explicit #214 work. #75 owns shipped-theme acceptance;
#212 owns layout foundations. No mandatory toolkit, package, resident, image
engine or broad configuration migration is selected. Parked pathname/Backer work
is excluded. [Focused evidence](../verification/VERIFICATION-2026-10-26-THEME-COMPOSITION.md)
records actual native pixels, three inspected configurations, costs and limits.
