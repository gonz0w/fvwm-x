# Phase 4: WM-Owned UI Metrics and Resources - Research

**Researched:** 2026-08-31
**Domain:** Scale-aware FVWM-owned chrome metrics, font/image/cursor caches, interaction hit targets, and the supported Meson feature profile
**Confidence:** HIGH for in-repo seams; MEDIUM for Xft/Xcursor per-monitor application details that official docs describe as display-global

<user_constraints>
## User Constraints

No Phase 4 CONTEXT.md exists (discuss-phase has not run). The following inherited locked decisions constrain this phase. Planner must not reopen them.

### Locked Decisions (inherited)

From PROJECT.md / REQUIREMENTS.md / STATE.md / Phase 1–3:

- v0.1 keeps all X11, ICCCM, EWMH, and module geometry in actual pixels. [VERIFIED: .planning/STATE.md:88-88]
- Scale FVWM-owned UI and geometry, not arbitrary client pixels. [VERIFIED: .planning/PROJECT.md:90-90]
- Prefer a coherent FVWM-X contract over legacy config or module compatibility when they conflict. [VERIFIED: .planning/PROJECT.md:92-92]
- Pure policy requires native unit coverage; X-visible behavior requires nested-X integration on native Linux CI. [VERIFIED: .planning/PROJECT.md:94-95]
- Authoritative X state stays single-threaded; no blocking monitor or scale work in the main event loop. [VERIFIED: .planning/PROJECT.md:86-89]
- Do not add compositing; integrate with external compositors. [VERIFIED: .planning/PROJECT.md:91-91]
- Per-monitor mutation of global Xft DPI, XSETTINGS, or toolkit environment is out of scope. [VERIFIED: .planning/REQUIREMENTS.md:76-76]
- Phase 2 scale domain is exact quarter steps 1x–4x (`MONITOR_SCALE_QUARTERS_MIN` 4 through `MONITOR_SCALE_QUARTERS_MAX` 16); conversions round nearest with half ties away from zero; overflow fails explicitly; never feed a rounded result back into the next conversion. [VERIFIED: libs/monitor_scale.h:9-10; libs/monitor_scale.h:94-98]
- Status v4 and module packets remain actual-pixel interfaces without scale metadata. [VERIFIED: .planning/STATE.md:127-127]
- Geometry and ownership contracts precede UI resources; X-visible transitions follow destination UI metrics. [VERIFIED: .planning/STATE.md:90-90]
- Automatic DPI thresholds, ownership hysteresis, and toolkit transition feedback require empirical calibration during their owning phases. [VERIFIED: .planning/STATE.md:158-158]

### the agent's Discretion

All Phase 4 policy details not locked above, including:

- File and API boundaries for pure metric conversion versus cache adapters
- Whether Picture/Flocale caches are extended in place or wrapped
- Resource generation counter representation
- Which monitor supplies scale for cursors, snap distances, and iconified windows
- How independently rounded font versus titlebar metrics avoid clipping and gaps
- XLFD/core-font handling when Xft `pixelsize` is the supported path
- Exact Meson `-D` flag set that constitutes the supported profile beyond the four TEST-05 features

### Deferred Ideas (OUT OF SCOPE)

- GEO-02 logical-size preservation across scale transitions — Phase 5. [VERIFIED: .planning/REQUIREMENTS.md:109-109]
- GEO-04 drag across scales / pointer-anchor / ownership oscillation — Phase 5. [VERIFIED: .planning/REQUIREMENTS.md:111-111]
- GEO-06 maximize, fullscreen, unmaximize, restore, monitor-transition geometry — Phase 5. [VERIFIED: .planning/REQUIREMENTS.md:112-112]
- SCALE-06 runtime scale reload and affected-window updates — Phase 6. [VERIFIED: .planning/REQUIREMENTS.md:25-25]
- Module scale metadata and module-owned UI — Phase 7. [VERIFIED: .planning/REQUIREMENTS.md:113-115]
- Nested-X layout matrix, client-toolkit, compositor variants — Phase 8. [VERIFIED: .planning/REQUIREMENTS.md:117-119]
- Arbitrary client-content scaling; RandR output transforms as scale; EWMH/ICCCM/strut/module geometry in logical units. [VERIFIED: .planning/REQUIREMENTS.md:69-77]
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|------------------|
| UI-01 | Titlebars, borders, title fonts, and window buttons render at the owning monitor's scale | Convert source `title_height`, `boundary_width`, handle width, and title-font pixel metrics through `monitor_scale_apply` using `fw->m->effective_scale.factor`. [VERIFIED: fvwm/geometry.c:261-262; fvwm/decorations.c:320-327; libs/FScreen.h:104-104] |
| UI-02 | Fonts and image assets scaled exactly once from source, with caches separated by target scale, dimensions, and generation | Current `FlocaleLoadFont` and `PCacheFvwmPicture` key only on name/path + stamp + `fpa_mask`. Extend keys; rasterize/open at the target size from the original source. [VERIFIED: libs/Flocale.c:1431-1446; libs/Picture.c:126-128] |
| UI-03 | Menus, icons, iconified-window UI, and move/resize feedback render at the active monitor's scale | Menus already resolve a monitor via `FSCREEN_XYPOS`; icons and rubber-band outlines currently use unscaled source pixels. [VERIFIED: fvwm/menus.c:2017-2019; fvwm/ewmh_icons.c:661-668] |
| UI-04 | Appropriately sized WM cursors, resize handles, title-button targets, and invisible interaction regions | Default cursors are core `XCreateFontCursor` glyphs; title buttons follow `title_thickness`; pan-frame windows are the invisible edge hit targets. [VERIFIED: fvwm/cursor.c:289-300; fvwm/fvwm.h:796-796; fvwm/virtual.c:1270-1279] |
| UI-05 | Scale-aware pan zones, edge actions, snapping distances, and drag thresholds | `edge_thickness` is stored as 0–2 source pixels; `DEFAULT_MOVE_THRESHOLD` is 3; snap proximity/grid are style pixels. Scale those integers; do not change paging math. [VERIFIED: fvwm/virtual.c:2071-2077; libs/defaults.h:62-62; libs/defaults.h:259-262] |
| UI-06 | Seamless fractional-scale chrome without clipped text, titlebar gaps, stale resources, or cursor hot-spot displacement | Derive chrome from one scaled composition; cache by generation; scale `xhot`/`yhot` with the image and clamp inside bounds. [VERIFIED: fvwm/geometry.c:261-314; libs/PictureImageLoader.c:1044-1057] |
| TEST-05 | Supported build profile explicitly enables Xcursor, XRender, PNG, and SVG paths | Meson features are currently `auto`; CI setup does not pass `-Dxcursor=enabled` etc. Assert `HAVE_XCURSOR`, `HAVE_XRENDER`, `HAVE_PNG`, `HAVE_RSVG`. [VERIFIED: meson.options:67-67; meson.options:80-101; .github/workflows/ccpp.yml:47-47] |
</phase_requirements>

## Summary

Phase 4 should be planned as one allocation-free metric converter plus target-aware resource caches, then thin single-threaded wiring into existing chrome, menu, icon, cursor, pan-frame, and snap/drag call sites. Phase 2 already attaches a total `effective_scale` to every runtime `struct monitor`. Phase 3 already owns the window and publishes actual-pixel protocol. This phase consumes those results. It does not iterate mapped windows to preserve logical client size, does not reload scale configuration, and does not change module packets.

Today every FVWM-owned visual is a 1x pixel constant. Title thickness is `title_font->height + EXTRA_TITLE_FONT_HEIGHT` optionally raised to `decor->title_height`. Borders and handles come from style `BorderWidth`/`HandleWidth` clamped to `MAX_BORDER_WIDTH`. Fonts cache by name string only. Pictures cache by path, file stamp, and `fpa_mask` — not size or scale. Cursors are a process-global `XCreateFontCursor` table. Pan frames store `edge_thickness` in `{0,1,2}`. Snap and `MoveThreshold` are unscaled integers.

Xft point size and Xcursor default size are display-global. Per-monitor scale therefore cannot be implemented by writing `Xft.dpi` or calling `XcursorSetDefaultSize`. Open Xft fonts at an explicit `pixelsize`, load Xcursor images at an explicit nominal size, and rasterize SVG/PNG once to the target pixel rectangle.

**Primary recommendation:** Add `libs/wm_ui_metrics.[ch]` that converts source chrome integers through `monitor_scale_apply`, extend Picture and Flocale caches with `(scale quarters, target dimensions, generation)`, apply owning-monitor scale at window chrome setup and active-monitor scale at menu/icon/feedback/cursor sites, and make CI pass `-Dxcursor=enabled -Dxrender=enabled -Dpng=enabled -Dsvg=enabled`.

## Assumption-Delta Decision

| Question | Decision | Planner consequence |
|----------|----------|---------------------|
| Should chrome scale change EWMH frame extents or module `title_height` units? | **NO CHANGE TO UNITS.** Scaled chrome is still actual X11 pixels. GEO-07 forbids logical protocol fields. [VERIFIED: fvwm/fvwm.h:782-796; .planning/REQUIREMENTS.md:76-77] | After scaling, published extents equal the new pixel chrome. Do not add scale fields to packets. |
| Should this phase rescale mapped windows when they change owner? | **NO.** Destination UI metrics exist so Phase 5 can apply them. [VERIFIED: .planning/STATE.md:90-90] | Apply scale at setup, menu popup, icon create, pan-frame init, and redraw of already-correct geometry. Do not add a transition coordinator. |
| Should global Xft DPI or Xcursor default size be mutated? | **NO.** Both are display-wide; REQUIREMENTS forbid per-monitor Xft DPI mutation. [CITED: https://manpages.debian.org/bookworm/libxft-dev/Xft.3.en.html; https://manpages.debian.org/bookworm/libxcursor-dev/Xcursor.3.en.html] | Pass explicit `pixelsize` / Xcursor load size. Never `XcursorSetDefaultSize` or `XftDefaultSet` DPI writes. |
| Should SVG be scaled by transforming a 1x pixmap through XRender? | **NO.** UI-02 requires scale-once from source. librsvg can rasterize into a target viewport. [CITED: https://gnome.pages.gitlab.gnome.org/librsvg/Rsvg-2.0/method.Handle.render_document.html] | Render SVG at target pixels; scale PNG/XPM from the original buffer once; cache the result. |
| Should TEST-05 rely on Meson `auto` finding the libraries? | **NO.** Explicit `enabled` is the supported profile. [CITED: https://mesonbuild.com/Build-options.html] | CI and the documented profile pass `-D<feature>=enabled`. A native test asserts the four `HAVE_*` macros. |

## Project Constraints (from AGENTS.md)

- Target X11/XRandR 1.5+ only; do not introduce a Wayland path. [VERIFIED: AGENTS.md:18-21]
- Keep authoritative X state single-threaded and preserve event-loop ordering. [VERIFIED: AGENTS.md:20-22]
- Perform no blocking monitor or scale work in the main event loop. [VERIFIED: AGENTS.md:24-25]
- Scale FVWM-owned UI and geometry, not arbitrary client pixels, and do not add compositing. [VERIFIED: AGENTS.md:27-30]
- Prefer the coherent FVWM-X contract over incompatible legacy behavior. [VERIFIED: AGENTS.md:29-30]
- Give pure policy native unit coverage and X-visible behavior nested-X integration coverage on native Linux CI. [VERIFIED: AGENTS.md:32-33]
- Route local macOS verification through Podman x86_64 emulation; leave AddressSanitizer to native Linux CI. [VERIFIED: AGENTS.md:35-36]
- Use C11, repository formatting, lower snake-case APIs, explicit status results, close-to-use declarations in new code, and `fvwm_debug(__func__, ...)` for recoverable diagnostics. [VERIFIED: AGENTS.md:154-181; AGENTS.md:196-210]
- Document user-visible chrome/scale behavior in `doc/fvwm3_manpage_source.adoc`. [VERIFIED: AGENTS.md:214-220]
- Include generated `config.h` first in each C implementation. [VERIFIED: AGENTS.md:377-387]

## Architectural Responsibility Map

| Capability | Primary Tier | Secondary Tier | Rationale |
|------------|-------------|----------------|-----------|
| Integer chrome metric conversion | Shared support library (`libs/`) | Core decorations | Pure `monitor_scale_apply` over source integers; no X. [VERIFIED: libs/monitor_scale.h:94-98] |
| Titlebar, border, button, handle geometry | Core (`fvwm/geometry.c`, `fvwm/decorations.c`, `fvwm/borders.c`) | Metric helper | `title_thickness` and `boundary_width` already live on `FvwmWindow`. [VERIFIED: fvwm/fvwm.h:784-796] |
| Title font rasterization | Shared `Flocale`/`Fft` | Core `setup_window_font` | Fonts are opened through `FlocaleLoadFont`; cache must become scale-aware. [VERIFIED: fvwm/add_window.c:1695-1723] |
| Image/SVG/PNG raster cache | Shared `Picture*` | Core icons/menus/buttons | `PCacheFvwmPicture` is the existing cache seam. [VERIFIED: libs/Picture.c:89-151] |
| WM cursors and hot spots | Core `fvwm/cursor.c` plus `PImageLoadCursorFromFile` | Xcursor | Default table is process-global; file cursors already speak Xcursor. [VERIFIED: fvwm/cursor.c:289-300; libs/PictureImageLoader.c:1033-1058] |
| Menus, iconified UI, move/resize outlines | Core menus/icons/`move_resize.c` | Active-monitor lookup | Menus already query `FSCREEN_XYPOS`. [VERIFIED: fvwm/menus.c:2017-2019] |
| Pan zones, edge actions, snap, drag threshold | Core `virtual.c` / `move_resize.c` / `functions.c` | Metric helper | Stored as pixel integers per monitor or `Scr`. [VERIFIED: libs/FScreen.h:144-145; fvwm/screen.h:421-421] |
| Supported feature profile | Build (`meson.build`, CI) | Native `HAVE_*` test | TEST-05 is a configure-time contract, not runtime chrome. [VERIFIED: meson.options:67-101] |
| Native verification | Meson C assertion targets | Existing live-X as non-regression | UI-01–UI-06 policy is native; nested-X chrome matrix is Phase 8. [VERIFIED: .planning/REQUIREMENTS.md:117-117] |

## Standard Stack

### Core

| Library / facility | Version | Purpose | Why Standard |
|--------------------|---------|---------|--------------|
| C11 | `c_std=c11` | Exact integer metric conversion | Repository language standard. [VERIFIED: AGENTS.md:43-46] |
| `monitor_scale_apply` | in-repo Phase 2 | Quarter-unit pixel conversion | Locked conversion contract. [VERIFIED: libs/monitor_scale.h:94-98] |
| `monitor->effective_scale` | in-repo Phase 2 | Per-monitor total scale result | Already initialized to safe 1x. [VERIFIED: libs/FScreen.h:104-104] |
| Xft (required) | Meson `dependency('xft', required: true)` | Title/menu/icon text | Required dep; `HAVE_XFT` is unconditional when the build succeeds. [VERIFIED: meson.build:217-227] |
| Xcursor (feature) | Meson option `xcursor` | ARGB/theme cursors and hot spots | TEST-05 requires the path enabled. [VERIFIED: meson.options:86-89; meson.build:394-397] |
| XRender (feature) | Meson option `xrender` | Alpha drawing used by Picture/Xft | TEST-05 requires the path enabled. [VERIFIED: meson.options:98-101; meson.build:412-415] |
| libpng >=1.2 (feature) | Meson option `png` | PNG chrome/icons | TEST-05 plus default-config PNGs. [VERIFIED: meson.build:327-330; default-config/meson.build:8-16] |
| librsvg >=2.13.92 (feature) | Meson option `svg` plus one Cairo backend | SVG raster at target size | TEST-05; existing `PImageLoadSvg`. [VERIFIED: meson.build:354-385; libs/PictureImageLoader.c:151-203] |
| `FImageSizeIsSafe` | in-repo | Reject hostile decoded dimensions | Already gates SVG/PNG/XPM. [VERIFIED: libs/ImageSafety.h:8-10; libs/PictureImageLoader.c:96-97] |
| Meson native tests | Meson 1.5.1+ | Deterministic metric/cache fixtures | Established `-UNDEBUG` assertion pattern. [VERIFIED: tests/meson.build:1-40] |

### Supporting

| Facility | Purpose | When to Use |
|----------|---------|-------------|
| `FScreenGetTopologySnapshot()` / `fw->m` | Owning-monitor scale | Window chrome. [VERIFIED: libs/FScreen.h:104-104] |
| `FSCREEN_XYPOS` / `FSCREEN_CURRENT` | Active-monitor scale | Menus, cursors, move/resize feedback. [VERIFIED: libs/FScreen.h:25-31; fvwm/menus.c:2017-2019] |
| `fvwm_debug(__func__, ...)` | Recoverable cache/overflow diagnostics | Conversion overflow, missing font, failed image. [VERIFIED: AGENTS.md:196-210] |
| Existing live-X harness | Non-regression of topology/scale suites | Do not add a Phase 4 live-X chrome matrix; TEST-02 is Phase 8. [VERIFIED: .planning/REQUIREMENTS.md:117-117] |

### Alternatives Considered

| Instead of | Could use | Tradeoff |
|------------|-----------|----------|
| Explicit `pixelsize` per scale | Mutate `Xft.dpi` / `XcursorSetDefaultSize` | Display-global; forbidden by REQUIREMENTS and would fight mixed-DPI. [CITED: Xft(3); Xcursor(3)] |
| Scale-once source raster | XRender `Picture` transform every draw | Repeats scaling, fights UI-02, depends on compositor-like filtering. |
| New image library | GdkPixbuf / Imlib2 | Extra dependency; Picture+librsvg already decode PNG/SVG. [VERIFIED: libs/meson.build:29-33] |
| Keep `auto` Meson features | Explicit `enabled` | `auto` can silently drop Xcursor/SVG when a CI image lacks a pkg-config file. TEST-05 forbids that. [CITED: https://mesonbuild.com/Build-options.html] |

**Installation:** None. This phase adds no external package and needs no Package Legitimacy Audit. [VERIFIED: libs/meson.build:1-80]

## Package Legitimacy Audit

Not applicable — no packages are installed.

**Packages removed due to [SLOP] verdict:** none
**Packages flagged as suspicious [SUS]:** none

## Architecture Patterns

### System Architecture Diagram

```text
Source chrome integers (config/style/defaults)
        |
        v
monitor_scale_apply(source, factor) ---- overflow --> log + keep last good metric
        |
        v
wm_ui_metrics (title, border, handle, font px, snap, threshold, pan)
        |
        +--> owning monitor (fw->m->effective_scale)
        |         |
        |         v
        |    setup_title_geometry / SelectDecor / title buttons / handles
        |
        +--> active monitor (FSCREEN_XYPOS / pointer)
                  |
                  v
             menus / iconified UI / rubber-band / WM cursors

Font source name  --> Flocale/Fft open at pixelsize=scaled --> cache key (name, quarters, generation)
Image source file --> decode original --> scale or SVG-render once --> cache key (path, w, h, quarters, generation, fpa)
Cursor source     --> Xcursor load at nominal size OR scale ARGB + hot spot --> cache key (name, quarters, generation)

X11 seams stay pixels: _NET_FRAME_EXTENTS, ConfigWinPacket title_height/boundary_width
```

### Recommended Project Structure

```text
libs/
├── wm_ui_metrics.h      # source->pixel metric conversion + generation type [ASSUMED]
├── wm_ui_metrics.c      # wrappers over monitor_scale_apply with overflow status [ASSUMED]
├── Picture.c            # extend cache key
├── Flocale.c            # extend font cache key; Xft pixelsize rewrite
└── PictureImageLoader.c # SVG/PNG/cursor target-size load
fvwm/
├── decorations.c        # scale BorderWidth/HandleWidth at SelectDecor
├── geometry.c           # scale title composition
├── add_window.c         # load scaled title/icon fonts
├── borders.c            # draw using scaled title_thickness / button geos
├── menus.c / menustyle.c
├── icons.c / ewmh_icons.c
├── cursor.c
├── virtual.c            # scale pan-frame thickness at map time
└── move_resize.c        # scale snap + rubber-band line metrics
tests/
├── test_wm_ui_metrics.c
└── test_supported_features.c
```

Names follow `test_<unit>.c` and `libs/` lower-snake-case. [VERIFIED: AGENTS.md:152-172]

### Pattern 1: Scale From Source, Never From a Rounded Result

**What:** Every metric conversion calls `monitor_scale_apply` on the configured source integer. Never scale a previously scaled title thickness, font pixel size, or pixmap.

Phase 2 conversion contract, quoted verbatim. [VERIFIED: libs/monitor_scale.h:94-98]

```c
/* DATA_M4K8Q2NX_START */
/* Always convert from the retained exact source, never a rounded result. */
monitor_scale_conversion_status
monitor_scale_apply(int source, monitor_scale_factor factor, int *converted);
monitor_scale_conversion_status
monitor_scale_remove(int source, monitor_scale_factor factor, int *converted);
/* DATA_M4K8Q2NX_END */
```

`monitor_scale_apply` initializes `*converted` to 0 and returns `MONITOR_SCALE_CONVERSION_OVERFLOW` or `MONITOR_SCALE_CONVERSION_INVALID_ARGUMENT` without a plausible in-range value. [VERIFIED: libs/monitor_scale.c:243-254]

On overflow, keep the last good metric (or the unscaled source if none exists) and `fvwm_debug`. Do not clamp to `INT_MAX`.

Owning-monitor factor is `fw->m->effective_scale.factor`. [VERIFIED: libs/FScreen.h:104-104]

### Pattern 2: Compose Fractional Chrome From One Scaled Result

**What:** Do not independently round font height, extra padding, and explicit `TitleStyle` height then hope they fit.

Current unscaled composition, quoted verbatim. [VERIFIED: fvwm/geometry.c:261-314]

```c
/* DATA_G6H2E4RA_START */
	decor_size = fw->decor->title_height;
	font_size = fw->title_font->height + EXTRA_TITLE_FONT_HEIGHT;
/* ... */
	extra_size = (decor_size > 0) ? decor_size - font_size : 0;
	*offset = min_offset;
	if (fw->decor->min_title_height > 0 &&
	    font_size + extra_size < fw->decor->min_title_height)
	{
		extra_size = fw->decor->min_title_height - font_size;
	}
	if (extra_size > 0)
	{
		*offset += extra_size / 2;
	}
	*size = font_size + extra_size;
/* DATA_G6H2E4RA_END */
```

`EXTRA_TITLE_FONT_HEIGHT` is 3 source pixels. [VERIFIED: libs/defaults.h:117-119]

```c
/* DATA_E9U4I6XM_START */
#define EXTRA_TITLE_FONT_HEIGHT            3 /* pixels */
#define EXTRA_TITLE_FONT_WIDTH             3 /* pixels */
#define MIN_FONT_HEIGHT                    (EXTRA_TITLE_FONT_HEIGHT + 2)
/* DATA_E9U4I6XM_END */
```

Recommended composition (planner-lock in discuss-phase):

1. Open the title font at `pixelsize = monitor_scale_apply(source_font_px, factor)`.
2. `title_thickness = font->height + monitor_scale_apply(EXTRA_TITLE_FONT_HEIGHT, factor)`.
3. If `decor->title_height > 0`, replace with `monitor_scale_apply(decor->title_height, factor)` and **center** the already-scaled font; if the scaled explicit height is smaller than the font, raise the titlebar to the font (no clipping).
4. Scale `BorderWidth`/`HandleWidth` the same way in `SelectDecor`.

Default border and handle, quoted verbatim. [VERIFIED: libs/defaults.h:287-288]

```c
/* DATA_B5C8Z2WH_START */
#define DEFAULT_BORDER_WIDTH               1 /* pixels */
#define DEFAULT_HANDLE_WIDTH               7 /* pixels */
/* DATA_B5C8Z2WH_END */
```

`SelectDecor` currently copies those unscaled. [VERIFIED: fvwm/decorations.c:320-327]

```c
/* DATA_D4P6X9VG_START */
	border_width = (SHAS_BORDER_WIDTH(sflags)) ?
		SGET_BORDER_WIDTH(*pstyle) : DEFAULT_BORDER_WIDTH;
	if (border_width > MAX_BORDER_WIDTH)
	{
		border_width = MAX_BORDER_WIDTH;
	}
	handle_width = (SHAS_HANDLE_WIDTH(sflags)) ?
		SGET_HANDLE_WIDTH(*pstyle) : DEFAULT_HANDLE_WIDTH;
/* DATA_D4P6X9VG_END */
```

Clamp **after** scaling, against the existing `MAX_BORDER_WIDTH` 500 source-pixel cap scaled once, or keep the 500 cap as a scaled-pixel cap. Recommend: treat 500 as a source cap, scale, then additionally reject values that do not fit in `short` (`title_thickness` / `boundary_width` are `short`). [VERIFIED: fvwm/fvwm.h:784-796]

```c
/* DATA_W1J7N3UF_START */
	short boundary_width;
	short unshaped_boundary_width;
	short corner_length;
	/* title font */
	FlocaleFont *title_font;
	/* /Y coordinate to draw the title name */
	short title_text_offset;
	short title_length;
	/* Note: if the type of this variable is changed, do update the
	 * CONFIGARGSNEW macro in module_interface.c, libs/vpacket.h and too!
	 */
	short title_thickness;
/* DATA_W1J7N3UF_END */
```

Title-button X windows already size to `title_thickness`. Scaling thickness scales the hit target. [VERIFIED: fvwm/borders.c:4391-4393]

### Pattern 3: Target-Aware Font and Image Caches

**What:** Cache identity must include target scale, target dimensions, and a generation integer.

Current picture object and agreement test, quoted verbatim. [VERIFIED: libs/PictureBase.h:116-132; libs/PictureBase.h:158-158]

```c
/* DATA_P7R1C5WB_START */
typedef struct FvwmPictureThing
{
	struct FvwmPictureThing *next;
	char *name;
	unsigned long stamp;  /* should be FileStamp */
	unsigned long fpa_mask;
	Pixmap picture;
	Pixmap mask;
	Pixmap alpha;
	unsigned int depth;
	unsigned int width;
	unsigned int height;
	unsigned int count;
	Pixel *alloc_pixels;
	int nalloc_pixels;
	Bool no_limit;
} FvwmPicture;
/* DATA_P7R1C5WB_END */
```

```c
/* DATA_A3T9L6HF_START */
#define PICTURE_FPA_AGREE(p,fpa) (p->fpa_mask == fpa.mask)
/* DATA_A3T9L6HF_END */
```

Current lookup, quoted verbatim. [VERIFIED: libs/Picture.c:126-128]

```c
/* DATA_C2V8D4JK_START */
		if (!*p1 && !*p2 && !isFileStampChanged(&p->stamp, real_path)
		    && PICTURE_FPA_AGREE(p,fpa))
		{
/* DATA_C2V8D4JK_END */
```

Current font cache, quoted verbatim. [VERIFIED: libs/Flocale.h:140-150; libs/Flocale.c:1431-1446]

```c
/* DATA_F5N1Y8QM_START */
typedef struct _FlocaleFont
{
	struct _FlocaleFont *next;
	char *name;
	int count;
	XFontStruct *font;      /* font structure */
	XFontSet fontset;       /* font set */
	FftFontType fftf;       /* fvwm xft font */
	FlocaleCharset *fc;     /* fvwm charset of the font */
	FlocaleCharset *str_fc; /* fvwm charset of the strings to be displayed */
	int height;             /* height of the font: ascent + descent */
/* DATA_F5N1Y8QM_END */
```

```c
/* DATA_L9W3B7PS_START */
	while (flf)
	{
		char *c1, *c2;

		for (c1 = fontname, c2 = flf->name; *c1 && *c2; ++c1, ++c2)
		{
			if (*c1 != *c2)
			{
				break;
			}
		}
		if (!*c1 && !*c2)
		{
			flf->count++;
			return flf;
		}
		flf = flf->next;
	}
/* DATA_L9W3B7PS_END */
```

**Font path:** `FftGetFont` already calls `XftNameParse` / `XftFontMatch`. [VERIFIED: libs/Fft.c:155-171] Xft `XFT_SIZE` is points converted using display DPI; `FC_PIXEL_SIZE` is device pixels. [CITED: https://manpages.debian.org/bookworm/libxft-dev/Xft.3.en.html] Rewrite the opened pattern to an integer `pixelsize` produced by `monitor_scale_apply`. Do not call `XftDefaultSet` to change `FC_DPI`.

**Image path:**

- SVG: render once into a `w x h` cairo image using the existing `PImageLoadSvg` size grammar or `rsvg_handle_render_document` viewport. [CITED: https://gnome.pages.gitlab.gnome.org/librsvg/Rsvg-2.0/method.Handle.render_document.html] Keep `FImageSizeIsSafe` with `PIMAGE_MAX_DIMENSION` 32768 and `PIMAGE_MAX_PIXELS` 64M. [VERIFIED: libs/PictureImageLoader.c:96-97]
- PNG/XPM: decode original, then scale to target using integer `monitor_scale_apply` on width and height independently, or scale as a single blit from the original buffer. Never PNG→1.25 pixmap→1.5 pixmap.
- Mini/icon wanted sizes are source pixels 16 and 56. [VERIFIED: fvwm/ewmh_icons.c:661-668]

```c
/* DATA_I8O1S4YN_START */
#define MINI_ICON_WANTED_WIDTH  16
#define MINI_ICON_WANTED_HEIGHT 16
#define MINI_ICON_MAX_WIDTH 22
#define MINI_ICON_MAX_HEIGHT 22
#define ICON_WANTED_WIDTH 56
#define ICON_WANTED_HEIGHT 56
#define ICON_MAX_WIDTH 100
#define ICON_MAX_HEIGHT 100
/* DATA_I8O1S4YN_END */
```

**Generation:** store an unsigned `wm_ui_resource_generation` starting at 1 on `ScreenInfo` or the scale-config object. Phase 4 never increments it after startup. Phase 6 will increment on reload so stale pixmaps/fonts cannot be reused. Include generation in every cache key now so the field is not a later ABI scrape.

Default `fpa`/1x/generation-1 lookups must keep working for modules that still call `PCacheFvwmPicture` without scale (Phase 7).

### Pattern 4: Owning Versus Active Monitor

| Surface | Requirement | Monitor | Existing seam |
|---------|-------------|---------|----------------|
| Titlebars, borders, title fonts, window buttons | UI-01 | Owning `fw->m` | `setup_title_geometry`, `SelectDecor` |
| Menus | UI-03 | Active (menu origin / pointer) | `FSCREEN_XYPOS` already used. [VERIFIED: fvwm/menus.c:2017-2019] |
| Icons / iconified UI | UI-03 | Active (icon geometry's monitor, else pointer) | `fvwm/icons.c` |
| Move/resize rubber-band | UI-03 | Active (pointer) | `draw_move_resize_grid` in `move_resize.c` |
| Title-button targets, resize handles | UI-04 | Owning (same as chrome) | `title_thickness` / `boundary_width` |
| WM cursors | UI-04 | Active (pointer) | `CreateCursors` + `CMD_CursorStyle` |
| Pan zones / edge actions | UI-05 | That pan frame's monitor | `initPanFrames` |
| Snap distances / `MoveThreshold` | UI-05 | Window owner for snap; `Scr.MoveThreshold` scaled by pointer monitor for drag start | `DoSnapMonitor`, `functions.c` |

`fscreen_scr_t` already names `FSCREEN_CURRENT` and `FSCREEN_XYPOS`. [VERIFIED: libs/FScreen.h:25-31]

Do not use `monitor_get_all_widths()` as a scale space.

### Pattern 5: Cursors and Hot Spots

Default cursors are core font glyphs, one table for the process. [VERIFIED: fvwm/cursor.c:289-300]

```c
/* DATA_U2Y6C8QR_START */
Cursor *CreateCursors(Display *disp)
{
	int i;
	/* define cursors */
	cursors[0] = None;
	for (i = 1; i < CRS_MAX; i++)
	{
		cursors[i] = XCreateFontCursor(disp, default_cursors[i]);
	}

	return cursors;
}
/* DATA_U2Y6C8QR_END */
```

File/theme cursors already call `FcursorFilenameLoadImages(path, FcursorGetDefaultSize(dpy))` and may overwrite `xhot`/`yhot`. [VERIFIED: libs/PictureImageLoader.c:1044-1057]

Xcursor `XcursorGetDefaultSize` / `XcursorSetDefaultSize` are **per Display**, not per RandR monitor. `xhot`/`yhot` must be inside width/height. Load APIs take an explicit `size`. [CITED: https://manpages.debian.org/bookworm/libxcursor-dev/Xcursor.3.en.html]

Plan:

1. Never call `XcursorSetDefaultSize` to implement scale.
2. Compute `nominal = monitor_scale_apply(16, factor)` (16 is Xcursor's documented 16-point baseline when guessing from DPI) or keep source hotspot/size from the file and scale both. [ASSUMED: 16 px 1x nominal]
3. `XcursorFilenameLoadImages(path, nominal)` so the theme's nearest size is chosen once.
4. `xhot_scaled = monitor_scale_apply(xhot_source, factor)` then clamp to `[0, width-1]`.
5. Cache `Cursor` objects per `(shape or path, quarters, generation)`.
6. Core `XC_*` glyphs cannot be scaled; when Xcursor is enabled (TEST-05), load the theme equivalent via `XcursorShapeLoadCursor` at the nominal size. If Xcursor is absent, keep `XCreateFontCursor` and document 1x-only core glyphs.

`CRS_MAX` enumerates every WM cursor context. [VERIFIED: fvwm/cursor.h:16-44]

### Pattern 6: Pan Zones, Snap, Drag Thresholds

Pan-frame thickness is stored as source 0–2 and mapped as a 1-pixel-minimum window even when 0 (disabled). [VERIFIED: fvwm/virtual.c:1229-1239; fvwm/virtual.c:2071-2077; libs/FScreen.c:914-915]

```c
/* DATA_E1H5P9WK_START */
	if (val < 0 || val > 2)
	{
		fvwm_debug(__func__,
			   "EdgeThickness arg must be between 0 and 2,"
			   " found %d",val);
		return;
	}
/* DATA_E1H5P9WK_END */
```

Keep the user-facing 0–2 **source** range. At `initPanFrames`, if source is 0, leave disabled. Otherwise `edge_px = monitor_scale_apply(source, m->effective_scale.factor)` and size the four `PanFrame` windows to that pixel thickness. Do not raise the config maximum in this phase.

Snap and drag defaults, quoted verbatim. [VERIFIED: libs/defaults.h:62-62; libs/defaults.h:259-262]

```c
/* DATA_T3Q7A1YJ_START */
#define DEFAULT_MOVE_THRESHOLD             3 /* pixels */
/* DATA_T3Q7A1YJ_END */
```

```c
/* DATA_N2R5K8ZL_START */
#define DEFAULT_SNAP_ATTRACTION            0 /* snap nothing */
#define DEFAULT_SNAP_ATTRACTION_MODE     0x3 /* snap all */
#define DEFAULT_SNAP_GRID_X                1 /* pixels */
#define DEFAULT_SNAP_GRID_Y                1 /* pixels */
/* DATA_N2R5K8ZL_END */
```

Scale `snap_proximity`, `snap_grid_*`, `MoveThreshold`, `MENU_TRIANGLE_WIDTH`/`HEIGHT`, `DEFAULT_MENU_BORDER_WIDTH`, and keyboard shortcut move distances the same way. Time-based constants (`DEFAULT_CLICKTIME`) stay milliseconds.

Menu style `BorderWidth` is `unsigned char`. [VERIFIED: fvwm/menustyle.h:60-61; fvwm/menustyle.h:246-246] Scaled values must fit; clamp with a diagnostic rather than wrapping.

### Pattern 7: Supported Feature Profile (TEST-05)

Current options default to `auto`. [VERIFIED: meson.options:67-67; meson.options:80-101]

```meson
# DATA_O7L2F5CM_START
option('png', type: 'feature', value: 'auto', description: 'Enable png support')
# ...
option('svg', type: 'feature', value: 'auto', description: 'Enable svg support')
option('xcursor', type: 'feature', value: 'auto', description: 'Enable Xcursor support')
option('xrender', type: 'feature', value: 'auto', description: 'Enable XRender support')
# DATA_O7L2F5CM_END
```

Meson `enabled` is `required : true`; `auto` is `required : false`; `disabled` never looks up the dependency. [CITED: https://mesonbuild.com/Build-options.html]

CI currently:

```bash
meson setup builddir --buildtype=debugoptimized -Dhtmldoc=true -Dmandoc=true
```

[VERIFIED: .github/workflows/ccpp.yml:47-47]

SVG also requires one of `cairo`, `cairo-svg`, `libsvg-cairo`. [VERIFIED: meson.build:362-385]

Supported profile:

```bash
meson setup builddir \
  -Dxcursor=enabled -Dxrender=enabled -Dpng=enabled -Dsvg=enabled \
  -Dcairo=enabled
```

Apply the same four (plus cairo) flags to the matrix, sanitizer, and live-x jobs. Native test `test_supported_features.c` includes `config.h` and asserts:

```
HAVE_XCURSOR == 1
HAVE_XRENDER == 1
HAVE_PNG == 1
HAVE_RSVG == 1
```

A local `builddir/config.h` already defines those four as 1, proving the image *can* supply them, but TEST-05 is about the **explicit** profile, not accidental `auto`. [VERIFIED: builddir/config.h:72-76; builddir/config.h:138-148]

```c
/* DATA_H9S3V1DB_START */
#define HAVE_PNG 1
#define HAVE_RSVG 1
#define HAVE_XCURSOR 1
#define HAVE_XRENDER 1
/* DATA_H9S3V1DB_END */
```

Xft is already a required dependency (`all_req_deps` includes `'xft'`), so it is not a TEST-05 flag. [VERIFIED: meson.build:217-217]

### Anti-Patterns to Avoid

- **Mutating `Xft.dpi` or `XcursorSetDefaultSize`:** display-global; breaks mixed-DPI and violates REQUIREMENTS. [CITED: Xft(3); Xcursor(3)]
- **Scaling a cached 1x pixmap to 1.25 then to 2:** UI-02 forbids chained rounding.
- **Cache key = name only:** 1x and 2x would share a too-small pixmap/font. [VERIFIED: libs/Picture.c:126-128; libs/Flocale.c:1431-1446]
- **Logical units in `title_thickness` / packets:** GEO-07. [VERIFIED: fvwm/fvwm.h:782-796]
- **Blocking image decode on the event loop beyond existing loaders:** no new filesystem walks, no subprocess `convert`.
- **Implementing Phase 5 window transitions or Phase 6 reload.**
- **Scaling module UI (FvwmButtons, FvwmPager, …):** Phase 7.
- **Raising `EdgeThickness` user range:** keep 0–2 source; scale at map time.
- **Using `double` for metric conversion:** Phase 2 forbids it.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Pixel conversion | Local `* scale / 4` | `monitor_scale_apply` | Locked rounding/overflow. [VERIFIED: libs/monitor_scale.h:94-98] |
| Image dimension overflow | Ad-hoc `w * h` | `FImageSizeIsSafe` | Already the SVG/PNG gate. [VERIFIED: libs/ImageSafety.h:8-10] |
| SVG raster at size | Custom SVG parser | Existing `PImageLoadSvg` + librsvg viewport | Already decodes and size-checks. [VERIFIED: libs/PictureImageLoader.c:151-368] |
| Cursor ARGB upload | Raw `XCreatePixmapCursor` for theme cursors | `FcursorImageLoadCursor` / `XcursorFilenameLoadImages` | Hot spots and animation are specified. [CITED: Xcursor(3)] |
| Font matching | Second Fontconfig stack | `FftGetFont` / `FlocaleLoadFont` with pixelsize | Existing Xft path. [VERIFIED: libs/Fft.c:155-171] |
| Monitor lookup | New RandR query | `fw->m` or `FSCREEN_XYPOS` | Phase 1/3 own topology and ownership. |
| Test harness | pytest / new nested-X runner | Meson C assertions; live-X only as non-regression | TEST-05 is native/configure. [VERIFIED: tests/meson.build:1-40] |

**Key insight:** The expensive bugs are (1) using a display-global DPI/cursor size, (2) caching resources without the target scale in the key, and (3) rounding font and titlebar independently so fractional scale clips or gaps. One converter and one cache-key contract prevent those from leaking into every drawing file.

## Common Pitfalls

### Pitfall 1: Display-global DPI as per-monitor scale
**What goes wrong:** Both monitors jump to the last written `Xft.dpi`.
**Why:** `XftDefaultSubstitute` fills `FC_DPI` from the X screen, not a RandR Monitor. [CITED: https://manpages.debian.org/bookworm/libxft-dev/Xft.3.en.html]
**How to avoid:** `pixelsize` only.
**Warning signs:** `XftDefaultSet`, `XCURSOR_SIZE`, `XcursorSetDefaultSize`.

### Pitfall 2: Cache collision across scales
**What goes wrong:** A 2x title button draws the 1x PNG stretched or the 1x font.
**Why:** Picture/Flocale keys ignore size. [VERIFIED: libs/Picture.c:126-128]
**How to avoid:** Key on quarters + dimensions + generation.
**Warning signs:** Tests that load the same name at 1x and 2x and compare pointers for equality.

### Pitfall 3: Independent rounding leaves a gap or clips
**What goes wrong:** Font 13px into a 12px titlebar, or a 1px empty band.
**Why:** `font_size` and `decor_size` rounded separately. [VERIFIED: fvwm/geometry.c:261-314]
**How to avoid:** Pattern 2 composition; never clip.
**Warning signs:** Tests that only check font height or only title thickness.

### Pitfall 4: Hot spot left at 1x in a 2x cursor
**What goes wrong:** Click point is the upper-left of the scaled image.
**Why:** Current loader copies source `xhot`/`yhot` unless out of bounds. [VERIFIED: libs/PictureImageLoader.c:1048-1055]
**How to avoid:** Scale then clamp.
**Warning signs:** No test that `xhot == monitor_scale_apply(src_xhot, factor)`.

### Pitfall 5: `unsigned char` menu border wrap
**What goes wrong:** `BorderWidth` 200 at 2x wraps in `unsigned char`.
**Why:** `ST_BORDER_WIDTH` is `unsigned char`. [VERIFIED: fvwm/menustyle.h:246-246]
**How to avoid:** Scale into `int`, clamp to `MAX_MENU_BORDER_WIDTH` 50 scaled or keep 50 as scaled-pixel max. [VERIFIED: libs/defaults.h:67-67]

### Pitfall 6: `short` title_thickness overflow
**What goes wrong:** Truncation of a huge explicit `TitleStyle` height after 4x.
**Why:** Field is `short`. [VERIFIED: fvwm/fvwm.h:796-796]
**How to avoid:** Conversion overflow path; do not store a truncated value.

### Pitfall 7: Scaling `EdgeThickness` config range
**What goes wrong:** Users can no longer set 0/1/2; pan frames become huge.
**Why:** Command rejects `val > 2`. [VERIFIED: fvwm/virtual.c:2071-2077]
**How to avoid:** Store source; scale at map.

### Pitfall 8: Implementing Phase 5 in this phase
**What goes wrong:** Windows resize on owner change before the transition coordinator exists.
**How to avoid:** No `monitor_scale_apply` of `g.normal` client size.
**Warning signs:** Edits to `update_fvwm_monitor` that change `g.frame` size.

### Pitfall 9: TEST-05 as a runtime test only
**What goes wrong:** CI image has the libs via `auto`, but a packager `-Dxcursor=disabled` still ships.
**How to avoid:** Explicit `-D*=enabled` plus `HAVE_*` assertions that fail the supported-profile test if a macro is 0.

### Pitfall 10: SVG render viewport left at intrinsic size
**What goes wrong:** SVG raster stays 1x; cairo surface is 2x and looks tiny or blurry.
**Why:** Current `rsvg_handle_render_document` viewport uses `dim.width`/`dim.height`, with cairo transforms afterwards. [VERIFIED: libs/PictureImageLoader.c:413-424]
**How to avoid:** Target pixel viewport equals the scaled size; still run `FImageSizeIsSafe`.

## Code Examples

### Proposed metric converter

Behavioral skeleton only; identifiers are planner discretion. [ASSUMED]

```c
typedef struct {
	unsigned int generation;
} wm_ui_resource_generation;

monitor_scale_conversion_status
wm_ui_scale_px(int source, monitor_scale_factor factor, int *px);
```

Every call must use `monitor_scale_apply`. [VERIFIED: libs/monitor_scale.h:94-98]

### Proposed cache key extras

[ASSUMED] fields to add beside existing `name`/`stamp`/`fpa_mask`:

```c
unsigned int scale_quarters; /* MONITOR_SCALE_QUARTERS_MIN..MAX */
unsigned int target_width;
unsigned int target_height;
unsigned int generation;
```

`scale_quarters` must be the same domain as `monitor_scale_factor.quarters`. [VERIFIED: libs/monitor_scale.h:9-19]

```c
/* DATA_Q4D8M2LC_START */
#define MONITOR_SCALE_QUARTERS_MIN 4U
#define MONITOR_SCALE_QUARTERS_MAX 16U
/* ... */
typedef struct
{
	unsigned int quarters;
} monitor_scale_factor;
/* DATA_Q4D8M2LC_END */
```

### Existing SVG safety gate to keep

[VERIFIED: libs/PictureImageLoader.c:365-368]

```c
/* DATA_V6B1R3NZ_START */
	if (w <= 0 || h <= 0 ||
	    !FImageSizeIsSafe(
		    (uint64_t)w, (uint64_t)h, PIMAGE_MAX_DIMENSION,
		    PIMAGE_MAX_PIXELS, &pixel_count))
/* DATA_V6B1R3NZ_END */
```

## Likely Files and Exact Existing Integration Symbols

| File | Existing symbols | Planned role |
|------|------------------|--------------|
| `libs/monitor_scale.h` | `monitor_scale_apply`, `monitor_scale_factor` | Only legal converter. [VERIFIED: libs/monitor_scale.h:94-98] |
| `libs/FScreen.h` | `effective_scale`, `edge_thickness`, `PanFrame*` | Read factor; scale pan frames. [VERIFIED: libs/FScreen.h:104-165] |
| `fvwm/geometry.c` | `get_title_font_size_and_offset`, `set_window_border_size` | Compose scaled title/border. [VERIFIED: fvwm/geometry.c:247-317; fvwm/geometry.c:534-546] |
| `fvwm/decorations.c` | `SelectDecor` | Scale BorderWidth/HandleWidth. [VERIFIED: fvwm/decorations.c:310-327] |
| `fvwm/add_window.c` | `setup_window_font`, `setup_title_geometry` | Load scaled fonts. [VERIFIED: fvwm/add_window.c:1665-1723] |
| `fvwm/borders.c` | `border_draw_decorations`, title button geos | Consume scaled thickness. [VERIFIED: fvwm/borders.c:4388-4394] |
| `libs/Flocale.c` / `libs/Fft.c` | `FlocaleLoadFont`, `FftGetFont` | Cache key + pixelsize. [VERIFIED: libs/Flocale.c:1405-1446; libs/Fft.c:155-171] |
| `libs/Picture.c` / `PictureImageLoader.c` | `PCacheFvwmPicture`, `PImageLoadSvg`, `PImageLoadCursorFromFile` | Cache key + target raster. [VERIFIED: libs/Picture.c:89-151] |
| `fvwm/cursor.c` | `CreateCursors`, `CRS_*` | Per-scale cursor table. [VERIFIED: fvwm/cursor.c:289-300] |
| `fvwm/menus.c` / `menustyle.c` | `FSCREEN_XYPOS`, `ST_BORDER_WIDTH` | Active-monitor menu metrics. [VERIFIED: fvwm/menus.c:2017-2019] |
| `fvwm/icons.c` / `ewmh_icons.c` | wanted 16/56 sizes | Scale icon targets. [VERIFIED: fvwm/ewmh_icons.c:661-668] |
| `fvwm/virtual.c` | `initPanFrames`, `CMD_EdgeThickness` | Scale mapped thickness. [VERIFIED: fvwm/virtual.c:1220-1285] |
| `fvwm/move_resize.c` | `DoSnapMonitor`, `CMD_MoveThreshold` | Scale snap/threshold. [VERIFIED: fvwm/move_resize.c:2363-2437] |
| `fvwm/functable.c` | `edgethickness`, `movethreshold` | Unchanged commands; scaled at use. [VERIFIED: fvwm/functable.c:226-232; fvwm/functable.c:367-367] |
| `meson.options` / `meson.build` / `.github/workflows/ccpp.yml` | feature `auto` | Explicit enabled profile. |
| `libs/meson.build` | `libfvwm3_sources` | Add metrics unit once. [VERIFIED: libs/meson.build:1-64] |
| `tests/meson.build` | assertion targets | Register `wm-ui-metrics` and `supported-features`. [VERIFIED: tests/meson.build:1-51] |
| `doc/fvwm3_manpage_source.adoc` | Style/MenuStyle/Cursor/EdgeThickness | Document that listed pixel metrics follow monitor scale. |

## State of the Art

| Old/current approach | Phase 4 approach | Impact |
|----------------------|------------------|--------|
| 1x pixel constants in `defaults.h` / styles | `monitor_scale_apply` from those sources | UI-01/UI-05 |
| Font cache by name | Font cache by name+pixelsize+generation | UI-02 |
| Picture cache by path+stamp+fpa | Plus scale+dimensions+generation | UI-02 |
| `XCreateFontCursor` global table | Xcursor load at explicit size; scaled hot spots | UI-04/UI-06 |
| Meson `auto` features | Explicit `enabled` + `HAVE_*` test | TEST-05 |
| Display-global Xft DPI | Forbidden; pixelsize only | mixed-DPI |

**Deprecated/outdated for this phase:** treating `Xft.dpi` or `XCURSOR_SIZE` as the scale model; stretching cached 1x pixmaps every expose; putting logical units in `title_thickness`.

## Assumptions Log

| # | Claim | Section | Risk if Wrong |
|---|-------|---------|---------------|
| A1 | Titlebar height is composed from scaled font + scaled extra, with explicit TitleStyle height scaled as a whole and never below the font. | Pattern 2 | Discuss-phase may prefer scaling only the explicit height. |
| A2 | File names `wm_ui_metrics` / `test_wm_ui_metrics.c`. | Structure | Planner may choose different identifiers. |
| A3 | 16 px is the 1x Xcursor nominal size. | Pattern 5 | Theme matching may prefer 24/32; tests should lock one nominal. |
| A4 | Snap uses owning-monitor scale; `MoveThreshold` uses pointer-monitor scale. | Pattern 4 | Mixed-DPI drags may feel inconsistent if the opposite pairing is wanted. |
| A5 | Iconified UI uses the monitor containing the icon geometry. | Pattern 4 | UI-03 says "active"; pointer vs icon monitor may differ. |
| A6 | Resource generation starts at 1 and is not incremented in Phase 4. | Pattern 3 | If reload is accidentally in scope, stale caches survive. |
| A7 | `MAX_BORDER_WIDTH` 500 is a source cap that is then scaled. | Pattern 2 | Treating 500 as a scaled-pixel cap changes 4x behavior. |
| A8 | Core `XC_*` cursors are replaced by Xcursor theme shapes when Xcursor is enabled. | Pattern 5 | Theme may lack FVWM shapes; fallback to core glyphs at 1x. |
| A9 | cairo is enabled alongside svg in the supported profile. | Pattern 7 | A different SVG backend (`cairo-svg` / `libsvg-cairo`) could satisfy meson. |

## Open Questions

1. **What is the locked titlebar composition rule for fractional scale?**
   - What we know: unscaled code adds `EXTRA_TITLE_FONT_HEIGHT` then optional explicit height. [VERIFIED: fvwm/geometry.c:261-314]
   - What's unclear: whether discuss-phase prefers "never clip, may grow" versus "exact scaled TitleStyle even if font clips".
   - Recommendation: never clip (Pattern 2). UI-06 forbids clipped text.

2. **Which monitor scales cursors and MoveThreshold?**
   - What we know: UI-04/UI-05 say "appropriately sized" without owning vs active.
   - Recommendation: pointer/active, because both are interaction-local.

3. **Should XLFD/core fonts be rewritten or rejected for scaled chrome?**
   - What we know: Xft is required; Flocale still loads `XFontStruct`/`XFontSet`. [VERIFIED: libs/Flocale.h:145-147]
   - Recommendation: Xft `pixelsize` is the supported path; XLFD best-effort pixel field rewrite; if that fails, keep 1x and diagnose.

## Environment Availability

| Dependency | Required By | Available | Version | Fallback |
|------------|-------------|-----------|---------|----------|
| Python 3 | Linux router | ✓ | 3.14.7 | — [VERIFIED: local `python3 --version`, 2026-08-31] |
| Podman | macOS native-Linux route | ✓ | 6.1.0 | Existing wrapper. [VERIFIED: local `podman --version`, 2026-08-31] |
| `tests/run_in_linux.py --self-test` | Native tests | ✓ | exit 0 | — [VERIFIED: local run, 2026-08-31] |
| Meson | Build/test | ✗ on host | — | Podman Linux route. [VERIFIED: local `command -v meson`, 2026-08-31] |
| Ninja | Meson backend | ✗ on host | — | Podman Linux route. [VERIFIED: local `command -v ninja`, 2026-08-31] |
| Apple clang | Local syntax | ✓ | 21.0.0 | Production remains Linux GCC/Clang. [VERIFIED: local `cc --version`, 2026-08-31] |
| clang-format | Formatting | ✓ | 21.0.0 via xcrun | Linux image lacks it. [VERIFIED: local `xcrun clang-format --version`, 2026-08-31] |
| ctx7 CLI | Docs seam | ✗ | — | Official man pages via WebFetch, tagged [CITED]. [VERIFIED: local `command -v ctx7`, 2026-08-31] |

**Missing dependencies with no fallback:** none.

**Missing dependencies with fallback:** host Meson and Ninja — use `python3 tests/run_in_linux.py`.

Step 2.6 otherwise: no new native libraries required; Xcursor/XRender/PNG/SVG are existing Meson features.

## Validation Architecture

### Test Framework

| Property | Value |
|----------|-------|
| Framework | Meson test runner; plain C assertion executables. [VERIFIED: tests/meson.build:1-51] |
| Config file | `tests/meson.build` |
| Quick run command | `python3 tests/run_in_linux.py -- meson test -C builddir wm-ui-metrics supported-features monitor-scale --print-errorlogs` [ASSUMED] |
| Full native suite | `python3 tests/run_in_linux.py -- meson test -C builddir --print-errorlogs` |
| Live-X (non-regression only) | `python3 tests/run_in_linux.py --live-x -- meson test -C builddir --suite live-x --print-errorlogs` |

Nyquist validation is enabled (`workflow.nyquist_validation` is true). [VERIFIED: .planning/config.json:24-24]

### Phase Requirements → Test Map

| Req ID | Behavior | Test Type | Automated Command | File Exists? |
|--------|----------|-----------|-------------------|-------------|
| UI-01 | Title/border/handle/font px at 1x, 1.25x, 1.5x, 2x, 4x from source 1/7/13 match `monitor_scale_apply` | unit | `meson test -C builddir wm-ui-metrics` | ❌ Wave 0 |
| UI-02 | Same source name at two scales yields two cache entries; generation mismatch misses | unit | `meson test -C builddir wm-ui-metrics` | ❌ Wave 0 |
| UI-03 | Menu border/triangle/icon wanted sizes use active factor | unit | `meson test -C builddir wm-ui-metrics` | ❌ Wave 0 |
| UI-04 | Hot spot scaled and clamped; handle width scaled | unit | `meson test -C builddir wm-ui-metrics` | ❌ Wave 0 |
| UI-05 | edge 1→2 at 2x; MoveThreshold 3→5 at 1.5x; snap 8→16 at 2x; edge 0 stays 0 | unit | `meson test -C builddir wm-ui-metrics` | ❌ Wave 0 |
| UI-06 | Explicit title height below scaled font is raised; overflow initializes output 0 | unit | `meson test -C builddir wm-ui-metrics` | ❌ Wave 0 |
| TEST-05 | `HAVE_XCURSOR`, `HAVE_XRENDER`, `HAVE_PNG`, `HAVE_RSVG` are 1 in the supported profile | unit / configure | `meson test -C builddir supported-features` | ❌ Wave 0 |
| GEO-07 guard | No logical fields in `vpacket.h` / `CONFIGARGS` | other | `git diff` of those files | ✅ files exist |

### Sampling Rate

- **Per task commit:** focused named C target via `tests/run_in_linux.py`
- **Per wave merge:** full wrapped native suite
- **Phase gate:** full native suite green; existing live-X suite as non-regression; no new live-X chrome cases required

### Wave 0 Gaps

- [ ] `tests/test_wm_ui_metrics.c` — UI-01..UI-06 integer/cache-key fixtures
- [ ] `tests/test_supported_features.c` — TEST-05 `HAVE_*` assertions
- [ ] `wm-ui-metrics` and `supported-features` registrations in `tests/meson.build`
- [ ] CI `meson setup` lines gain `-Dxcursor=enabled -Dxrender=enabled -Dpng=enabled -Dsvg=enabled -Dcairo=enabled`

Existing coverage that must stay green: `monitor-scale`, `monitor-scale-config`, `monitor-topology`, `monitor-ownership`, `icccm-size-hints`, `image-safety`.

This phase is **not** a rename/migration of runtime identifiers; Runtime State Inventory is omitted.

**UI hint:** ROADMAP marks this phase `UI hint: yes`, but the UI is native X11 chrome, not a web/app surface. Do not spawn a HTML `UI-SPEC`. Document pixel chrome contracts in AsciiDoc instead.

## Security Domain

`workflow.security_enforcement` is enabled. [VERIFIED: .planning/config.json:47-47]

### Applicable ASVS Categories

| ASVS Category | Applies | Standard Control |
|---------------|---------|-----------------|
| V2 Authentication | no | No new auth surface. |
| V3 Session Management | no | No session tokens. |
| V4 Access Control | no | Existing local WM authority unchanged. |
| V5 Input Validation | yes | Image dimensions, font names, cursor files, and style integers are untrusted; reuse `FImageSizeIsSafe` and `monitor_scale_apply` overflow. [VERIFIED: libs/ImageSafety.h:8-10] |
| V6 Cryptography | no | No crypto. |

### Known Threat Patterns for C/X11 Chrome

| Pattern | STRIDE | Standard Mitigation |
|---------|--------|---------------------|
| Hostile PNG/SVG dimensions | Denial of service | `FImageSizeIsSafe` before cairo/png alloc. [VERIFIED: libs/PictureImageLoader.c:365-368] |
| Huge TitleStyle / BorderWidth after 4x | Denial of service | Conversion overflow; `short` range check. [VERIFIED: fvwm/fvwm.h:784-796] |
| Crafted cursor hotspot outside image | Tampering | Xcursor requires `xhot <= width`; clamp after scale. [CITED: Xcursor(3)] |
| Font name / image path injection into logs | Repudiation | Encode names if new one-line diagnostics are added (cJSON pattern from Phase 1–2). |
| SVG script/external entity | Tampering | librsvg is the decoder; do not add a second parser; do not disable size checks. |

Image and font files come from user config and the image path. Treat decoded width/height as untrusted even after a successful loader return.

## Sources

### Primary (HIGH confidence)

- `libs/monitor_scale.h` / `libs/monitor_scale.c` — conversion contract. [VERIFIED: libs/monitor_scale.h:9-98; libs/monitor_scale.c:243-254]
- `libs/FScreen.h` — `effective_scale`, pan frames, `fscreen_scr_t`. [VERIFIED: libs/FScreen.h:25-31; libs/FScreen.h:104-165]
- `fvwm/geometry.c`, `fvwm/decorations.c`, `fvwm/add_window.c`, `fvwm/borders.c`, `fvwm/fvwm.h` — chrome metrics. [VERIFIED: cited ranges above]
- `libs/Picture.c`, `libs/PictureBase.h`, `libs/Flocale.c`, `libs/Fft.c` — caches. [VERIFIED: cited ranges above]
- `fvwm/cursor.c`, `libs/PictureImageLoader.c`, `libs/Fcursor.h` — cursors. [VERIFIED: cited ranges above]
- `fvwm/virtual.c`, `fvwm/move_resize.c`, `libs/defaults.h` — pan/snap/threshold. [VERIFIED: cited ranges above]
- `meson.options`, `meson.build`, `.github/workflows/ccpp.yml`, `builddir/config.h` — TEST-05. [VERIFIED: cited ranges above]
- Phase 1–3 RESEARCH/STATE/REQUIREMENTS/ROADMAP/PROJECT.

### Secondary (MEDIUM — official docs via WebFetch; classify-confidence rates webfetch LOW even with --verified)

Treat as [CITED], not [VERIFIED]:

- Xcursor 1.2.1 man page — image size, hot spots, `XcursorGetDefaultSize`. [CITED: https://manpages.debian.org/bookworm/libxcursor-dev/Xcursor.3.en.html]
- Xft 2.3.6 man page — `XFT_SIZE` points vs `FC_PIXEL_SIZE`, `XftDefaultSubstitute` DPI. [CITED: https://manpages.debian.org/bookworm/libxft-dev/Xft.3.en.html]
- Meson Build options — feature `enabled`/`auto`/`disabled`. [CITED: https://mesonbuild.com/Build-options.html]
- librsvg `rsvg_handle_render_document` — viewport fit. [CITED: https://gnome.pages.gitlab.gnome.org/librsvg/Rsvg-2.0/method.Handle.render_document.html]

### Tertiary (LOW)

- Graph store exists (`stale: false`, ~18h old) but titlebar/cursor/cache queries returned no nodes. Continue from opened source.
- Assumed 16 px Xcursor 1x nominal and owning-vs-active pairings in the Assumptions Log.

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH — no new dependencies; Meson feature flags and `HAVE_*` macros were read.
- Architecture: HIGH — chrome, cache, cursor, pan, and snap seams were opened with line ranges.
- Xft/Xcursor per-monitor method: MEDIUM — official man pages cited; display-global DPI/size is clear; 16 px nominal is assumed.
- Fractional composition rule: MEDIUM — UI-06 forces no clipping; exact formula needs discuss-phase lock.
- Pitfalls: HIGH — cache-key and 1x-constant bugs are quoted from current source.

**Research date:** 2026-08-31
**Valid until:** 2026-09-30 (stable in-repo C/X11 chrome; recheck if Phase 2 scale APIs or Meson feature defaults change)

**Graph note:** `.gsd-graph` last_build 2026-08-30T17:35:25Z, `stale: false`, age ~18h — queries for UI/cache terms returned empty; all discrete claims above come from opened source files.
