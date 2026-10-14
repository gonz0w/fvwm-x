# Phase 4: WM-Owned UI Metrics and Resources - Pattern Map

**Mapped:** 2026-08-31
**Files analyzed:** 28 new/modified files
**Analogs found:** 27 / 28

## Scope Extracted

Phase 4 adds one allocation-free metric converter (`libs/wm_ui_metrics.[ch]`), extends Picture/Flocale caches with `(scale quarters, target dimensions, generation)`, wires owning-monitor scale into window chrome and active-monitor scale into menus/icons/cursors/feedback/pan/snap, and makes CI pass the supported Meson feature profile. It does **not** rescale mapped client geometry, increment generation after startup, mutate `Xft.dpi`/`XcursorSetDefaultSize`, or change `libs/vpacket.h` / `CONFIGARGS`.

`libs/monitor_scale.h` is an input contract only: every conversion calls `monitor_scale_apply`. Never reimplement quarter arithmetic.

## File Classification

| New/Modified File | Role | Data Flow | Closest Analog | Match Quality |
|-------------------|------|-----------|----------------|---------------|
| `libs/wm_ui_metrics.h` (new) | utility | transform | `libs/monitor_scale.h` + `libs/icccm_size_hints.h` | exact |
| `libs/wm_ui_metrics.c` (new) | utility | transform | `libs/monitor_scale.c` apply/init-0 | exact |
| `tests/test_wm_ui_metrics.c` (new) | test | transform | `tests/test_monitor_scale.c` + `tests/test_icccm_size_hints.c` | exact |
| `tests/test_supported_features.c` (new) | test | request-response | `tests/test_monitor_scale.c` (`config.h` include) + `tests/test_image_safety.c` (assert `main`) | role-match |
| `libs/PictureBase.h` | model | CRUD | existing `FvwmPicture` / `PICTURE_FPA_AGREE` | exact in-place seam |
| `libs/Picture.c` | service | CRUD | existing `PCacheFvwmPicture` lookup | exact in-place seam |
| `libs/Picture.h` | service | CRUD | existing `PCacheFvwmPicture` signature | exact in-place seam |
| `libs/Flocale.h` | model | CRUD | existing `_FlocaleFont` | exact in-place seam |
| `libs/Flocale.c` | service | CRUD | existing `FlocaleLoadFont` name-only cache | exact in-place seam |
| `libs/Fft.c` | service | transform | existing `FftGetFont` `XftNameParse`/`XftFontMatch` | exact in-place seam |
| `libs/PictureImageLoader.c` | service | file-I/O, transform | existing `PImageLoadSvg` + `PImageLoadCursorFromFile` | exact in-place seam |
| `fvwm/geometry.c` | service | transform | existing `get_title_font_size_and_offset` / `set_window_border_size` | exact in-place seam |
| `fvwm/decorations.c` | service | transform | existing `SelectDecor` BorderWidth/HandleWidth | exact in-place seam |
| `fvwm/add_window.c` | controller | request-response | existing `setup_window_font` / `setup_title_geometry` | exact in-place seam |
| `fvwm/borders.c` | component | request-response | existing title-bar geos from `title_thickness` | exact in-place seam |
| `fvwm/menus.c` | controller | request-response | existing `FSCREEN_XYPOS` origin | exact in-place seam |
| `fvwm/menustyle.h` | model | transform | existing `unsigned char BorderWidth` | exact in-place seam |
| `fvwm/icons.c` | controller | request-response | existing icon-geometry `FSCREEN_XYPOS` | exact in-place seam |
| `fvwm/ewmh_icons.c` | controller | transform | existing `MINI_ICON_WANTED_*` / `ICON_WANTED_*` | exact in-place seam |
| `fvwm/cursor.c` | controller | request-response | existing `CreateCursors` | exact in-place seam |
| `fvwm/virtual.c` | controller | request-response | existing `initPanFrames` / `CMD_EdgeThickness` | exact in-place seam |
| `fvwm/move_resize.c` | controller | request-response | existing `DoSnapMonitor` / `CMD_MoveThreshold` | exact in-place seam |
| `fvwm/functions.c` | controller | request-response | existing `CheckActionType` `Scr.MoveThreshold` | exact in-place seam |
| `fvwm/screen.h` | model | transform | existing `ScreenInfo.MoveThreshold` | exact in-place seam |
| `libs/meson.build` | config | batch | existing `libfvwm3_sources` | exact in-place seam |
| `tests/meson.build` | config | batch | existing `monitor_scale_test` registration | exact in-place seam |
| `.github/workflows/ccpp.yml` | config | batch | existing `meson setup` matrix/sanitizer/live-x | exact in-place seam |
| `doc/fvwm3_manpage_source.adoc` | config | file-I/O | existing MonitorScale pixel-contract paragraph | exact in-place seam |

Unchanged guards (do not treat as work items): `libs/vpacket.h`, `CONFIGARGS` / `fvwm/module_interface.c`, `fvwm/functable.c`, `meson.options` (keep `auto`; profile is CI `-D*=enabled`).

## Pattern Assignments

### `libs/wm_ui_metrics.h` (utility, transform)

**Analog:** `libs/monitor_scale.h:1-10,55-60,94-98` and `libs/icccm_size_hints.h:1-13,34-36`

Copy the compact public-contract shape: uppercase include guard from subsystem+filename, standard-library / in-repo types only, named status enum, explicit functions. Do not expose arithmetic.

**Imports / guard pattern** (`libs/monitor_scale.h:1-19`):
```c
#ifndef FVWM_MONITOR_SCALE_H
#define FVWM_MONITOR_SCALE_H

#include "libs/monitor_topology.h"

#include <stdbool.h>
#include <stddef.h>

#define MONITOR_SCALE_QUARTERS_MIN 4U
#define MONITOR_SCALE_QUARTERS_MAX 16U
```

**Status-enum + output-parameter API** (`libs/icccm_size_hints.h:8-13,34-36`):
```c
typedef enum
{
	ICCCM_CONSTRAIN_OK,
	ICCCM_CONSTRAIN_OVERFLOW,
	ICCCM_CONSTRAIN_INVALID_ARGUMENT
} icccm_constrain_status;

icccm_constrain_status
icccm_constrain_size(const icccm_size_hints *hints, int *width, int *height,
    rectangle *frame);
```

**Only legal converter** (`libs/monitor_scale.h:94-98`):
```c
/* Always convert from the retained exact source, never a rounded result. */
monitor_scale_conversion_status
monitor_scale_apply(int source, monitor_scale_factor factor, int *converted);
monitor_scale_conversion_status
monitor_scale_remove(int source, monitor_scale_factor factor, int *converted);
```

New header should:

- Guard as `FVWM_WM_UI_METRICS_H`.
- Include `"libs/monitor_scale.h"`.
- Expose `wm_ui_resource_generation` (`unsigned int generation`, start at 1).
- Expose `wm_ui_scale_px` (or equivalent) that **must** call `monitor_scale_apply`.
- Optionally expose title-composition / cache-key helpers as pure integer functions (no `Display *`, no X).
- Reuse `monitor_scale_conversion_status`; do not invent a parallel overflow enum unless composition needs extra states.

---

### `libs/wm_ui_metrics.c` (utility, transform)

**Analog:** `libs/monitor_scale.c:1-22,243-254` and `libs/icccm_size_hints.c:1-36`

**GPL + `config.h` first** (`libs/monitor_scale.c:1-22`):
```c
/* -*-c-*- */
/* This program is free software; ... GPL notice ... */

#include "config.h"

#include "libs/monitor_scale.h"

#include <limits.h>
#include <string.h>
```

**Initialize output to 0 before converting** (`libs/monitor_scale.c:243-254`):
```c
monitor_scale_conversion_status
monitor_scale_apply(int source, monitor_scale_factor factor, int *converted)
{
	if (converted != NULL) {
		*converted = 0;
	}
	if (!factor_is_valid(factor)) {
		return MONITOR_SCALE_CONVERSION_INVALID_ARGUMENT;
	}

	return convert_ratio(source, factor.quarters, 4, converted);
}
```

**Defensive NULL outputs** (`libs/icccm_size_hints.c:27-36`):
```c
static void
zero_outputs(int *width, int *height)
{
	if (width != NULL) {
		*width = 0;
	}
	if (height != NULL) {
		*height = 0;
	}
}
```

Rules for the converter:

- C11, close-to-use declarations, `bool`/`true`/`false` (self-contained helper, like `ImageSafety.c`).
- No `double`, no local `* scale / 4`, no feeding a rounded result back.
- Overflow returns `MONITOR_SCALE_CONVERSION_OVERFLOW` with `*px == 0`. Callers keep last-good / unscaled source and `fvwm_debug(__func__, ...)`; the converter itself does not log.
- Title composition (UI-06): scaled font height + scaled `EXTRA_TITLE_FONT_HEIGHT`; if explicit `decor->title_height` scales below the font, raise the titlebar (never clip).
- Hot-spot helper: `monitor_scale_apply(xhot_source, factor)` then clamp to `[0, width-1]`.
- Edge 0 stays 0; do not scale a disabled pan frame into a 1 px window here (that mapping stays in `initPanFrames`).

---

### `tests/test_wm_ui_metrics.c` (test, transform)

**Analog:** `tests/test_monitor_scale.c:1-24,269-351,368-385` and `tests/test_icccm_size_hints.c:1-23`

**File header + includes** (`tests/test_monitor_scale.c:1-24`):
```c
/* -*-c-*- */
/* GPL notice */

#include "config.h"

#include "libs/monitor_scale.h"

#include <assert.h>
#include <limits.h>
#include <stddef.h>
#include <string.h>
```

**Named `static void test_<behavior>(void)` + explicit `main()`** (`tests/test_monitor_scale.c:368-385`):
```c
int
main(void)
{
	test_valid_committed_dpi_resolves_automatic_factor();
	/* ... one call per test_* ... */
	return 0;
}
```

**Overflow initializes output 0** (`tests/test_monitor_scale.c:310-350`):
```c
	assert(
	    monitor_scale_apply(INT_MAX, (monitor_scale_factor){5}, &converted)
	    == MONITOR_SCALE_CONVERSION_OVERFLOW);
	assert(converted == 0);
```

**Round-half-away-from-zero fixtures already locked** (`tests/test_monitor_scale.c:269-286`):
```c
	assert(monitor_scale_apply(1, (monitor_scale_factor){6}, &converted)
	       == MONITOR_SCALE_CONVERSION_OK);
	assert(converted == 2);
	assert(monitor_scale_apply(3, (monitor_scale_factor){6}, &converted)
	       == MONITOR_SCALE_CONVERSION_OK);
	assert(converted == 5);
```

Cover UI-01..UI-06 as integer/cache-key fixtures (no X):

| Req | Fixture |
|-----|---------|
| UI-01 | source 1/7/13 at 1x, 1.25x, 1.5x, 2x, 4x equal `monitor_scale_apply` |
| UI-02 | same name at two quarters → two cache-key identities; generation mismatch misses |
| UI-03 | menu border/triangle/icon wanted sizes use the supplied active factor |
| UI-04 | hot spot scaled then clamped; handle width scaled |
| UI-05 | edge 1→2 at 2x; MoveThreshold 3→5 at 1.5x; snap 8→16 at 2x; edge 0 stays 0 |
| UI-06 | explicit title height below scaled font is raised; overflow output 0 |

Do **not** compare cache pointers for equality across scales (that is the collision pitfall).

---

### `tests/test_supported_features.c` (test, request-response)

**Analog:** `tests/test_monitor_scale.c:16` (`#include "config.h"`) + `tests/test_image_safety.c:1-19,82-90` (assert `main`)

No existing `HAVE_*` assertion target. Copy the compact assert-main shape and include generated `config.h` first:

```c
#include "config.h"

#include <assert.h>

static void
test_supported_profile_macros(void)
{
	assert(HAVE_XCURSOR == 1);
	assert(HAVE_XRENDER == 1);
	assert(HAVE_PNG == 1);
	assert(HAVE_RSVG == 1);
}

int
main(void)
{
	test_supported_profile_macros();
	return 0;
}
```

Register with `include_directories: includedirs` so `config.h` resolves (same as `monitor_scale_test` in `tests/meson.build:20-29`). Linking `libfvwm3` is optional; macros alone do not need it.

---

### `libs/PictureBase.h` + `libs/Picture.c` + `libs/Picture.h` (model/service, CRUD)

**Analog:** in-place cache object and lookup.

**Object to extend** (`libs/PictureBase.h:116-132,158`):
```c
typedef struct FvwmPictureThing
{
	struct FvwmPictureThing *next;
	char *name;
	unsigned long stamp;  /* should be FileStamp */
	unsigned long fpa_mask;
	Pixmap picture;
	/* ... width/height/count ... */
} FvwmPicture;

#define PICTURE_FPA_AGREE(p,fpa) (p->fpa_mask == fpa.mask)
```

Add beside existing fields (do not reorder pixmap members):

```c
unsigned int scale_quarters; /* MONITOR_SCALE_QUARTERS_MIN..MAX */
unsigned int target_width;
unsigned int target_height;
unsigned int generation;
```

**Lookup to extend, not replace** (`libs/Picture.c:113-135`):
```c
	while (p)
	{
		/* name walk */
		if (!*p1 && !*p2 && !isFileStampChanged(&p->stamp, real_path)
		    && PICTURE_FPA_AGREE(p,fpa))
		{
			p->count++;
			free(path);
			return p;
		}
		p=p->next;
	}
```

Agreement must also require matching `scale_quarters`, `target_width`/`target_height`, and `generation`. Default callers of `PCacheFvwmPicture` without scale (modules, Phase 7) must keep working: treat missing extras as 1x (`MONITOR_SCALE_QUARTERS_MIN`) and generation 1.

**Public signature** (`libs/Picture.h:44-46`) stays compatible. Prefer a new scaled entry point or defaulted extras rather than breaking module callers.

**Imports pattern** (`libs/Picture.c:28-46`): keep `#include "config.h"` first, then X headers, then `"Picture.h"` / `"log.h"`.

---

### `libs/Flocale.h` + `libs/Flocale.c` + `libs/Fft.c` (model/service, CRUD/transform)

**Font object to extend** (`libs/Flocale.h:140-150`):
```c
typedef struct _FlocaleFont
{
	struct _FlocaleFont *next;
	char *name;
	int count;
	XFontStruct *font;
	XFontSet fontset;
	FftFontType fftf;
	/* ... */
	int height;
```

Add `scale_quarters` and `generation` (fonts have no independent target width/height; pixelsize is the scale).

**Name-only cache — the collision to fix** (`libs/Flocale.c:1431-1446`):
```c
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
```

Match name **and** quarters **and** generation.

**Xft open path** (`libs/Fft.c:16-33,155-171`):
```c
#include "config.h"
/* ... */
#include "Flocale.h"
#include "log.h"

FftFontType *FftGetFont(Display *dpy, char *fontname, char *module)
{
	/* ... */
	if ((src_pat = XftNameParse(fontname)) == NULL ||
	    (load_pat = XftFontMatch(dpy, fftscreen, src_pat, &result)) == NULL)
		goto cleanup;
```

After match, rewrite the pattern to integer `FC_PIXEL_SIZE` / Xft `pixelsize` from `monitor_scale_apply(source_font_px, factor)`. **Never** `XftDefaultSet` / DPI writes.

`FlocaleLoadFont` signature (`libs/Flocale.h:263`) currently has no scale argument. Prefer a new scaled loader or a thread-local/process default generation+factor passed explicitly; do not add a global DPI. XLFD/core fonts: best-effort pixel-field rewrite; on failure keep 1x and `fvwm_debug`.

---

### `libs/PictureImageLoader.c` (service, file-I/O, transform)

**Safety gate to keep** (`libs/PictureImageLoader.c:96-97,365-368`):
```c
#define PIMAGE_MAX_DIMENSION 32768U
#define PIMAGE_MAX_PIXELS (64U * 1024U * 1024U)

	if (w <= 0 || h <= 0 ||
	    !FImageSizeIsSafe(
		    (uint64_t)w, (uint64_t)h, PIMAGE_MAX_DIMENSION,
		    PIMAGE_MAX_PIXELS, &pixel_count))
```

**SVG loader** (`libs/PictureImageLoader.c:149-208,413-424`): keep `PImageLoadSvg` and librsvg. Change the render viewport so `vp.width`/`vp.height` equal the **scaled** target pixels, not intrinsic `dim.width`/`dim.height`. Do not cairo-scale a 1x raster into a 2x surface as the scale implementation.

**Cursor load** (`libs/PictureImageLoader.c:1033-1058`):
```c
Cursor PImageLoadCursorFromFile(
	Display *dpy, Window win, char *path, int x_hot, int y_hot)
{
	/* First try the Xcursor loader (animated cursors) */
	if ((fcis = FcursorFilenameLoadImages(
		path, FcursorGetDefaultSize(dpy))))
	{
		for (i = 0; i < fcis->nimage; i++)
		{
			if (x_hot < fcis->images[i]->width && x_hot >= 0 &&
			    y_hot < fcis->images[i]->height && y_hot >= 0)
			{
				fcis->images[i]->xhot = x_hot;
				fcis->images[i]->yhot = y_hot;
			}
		}
```

Replace `FcursorGetDefaultSize(dpy)` with an explicit nominal size (`monitor_scale_apply(16, factor)`). Scale then clamp `xhot`/`yhot`. Use `FcursorFilenameLoadImages` / `FcursorImageLoadCursor` from `libs/Fcursor.h:11-17`; never `XcursorSetDefaultSize`.

`FImageSizeIsSafe` analog (`libs/ImageSafety.c:3-16`): keep that gate; do not hand-roll `w * h`.

---

### `fvwm/geometry.c` + `fvwm/decorations.c` + `fvwm/add_window.c` (service/controller, transform)

Owning-monitor factor is `fw->m->effective_scale.factor` (`libs/FScreen.h:104`).

**Title composition seam** (`fvwm/geometry.c:247-314`):
```c
void get_title_font_size_and_offset(...)
{
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
}
```

Do **not** independently round font, extra, and explicit height. Compose via `wm_ui_metrics`: font already opened at scaled pixelsize; add scaled `EXTRA_TITLE_FONT_HEIGHT` (`libs/defaults.h:117-119`); if scaled explicit `title_height` is smaller than font, raise (UI-06). Store the result in `fw->title_thickness` (`short` — `fvwm/fvwm.h:782-796`). On conversion overflow, keep last-good / unscaled and `fvwm_debug`; never truncate into `short`.

**Border/handle seam** (`fvwm/decorations.c:310-331`):
```c
void SelectDecor(FvwmWindow *t, window_style *pstyle, short *buttons)
{
	border_width = (SHAS_BORDER_WIDTH(sflags)) ?
		SGET_BORDER_WIDTH(*pstyle) : DEFAULT_BORDER_WIDTH;
	if (border_width > MAX_BORDER_WIDTH)
	{
		border_width = MAX_BORDER_WIDTH;
	}
	handle_width = (SHAS_HANDLE_WIDTH(sflags)) ?
		SGET_HANDLE_WIDTH(*pstyle) : DEFAULT_HANDLE_WIDTH;
```

Clamp **source** against `MAX_BORDER_WIDTH` 500 (`libs/defaults.h:287-290`), then `monitor_scale_apply`, then reject values that do not fit in `short`. Pass scaled width to `set_window_border_size` (`fvwm/geometry.c:534-546`).

**Font load seam** (`fvwm/add_window.c:1665-1723`):
```c
void setup_title_geometry(FvwmWindow *fw, window_style *pstyle)
{
	get_title_font_size_and_offset(...);
	fw->title_thickness = width;
	fw->title_text_offset = offset;
	fw->corner_length = fw->title_thickness + fw->boundary_width;
}

void setup_window_font(FvwmWindow *fw, window_style *pstyle, Bool do_destroy)
{
	if (S_HAS_WINDOW_FONT(SCF(*pstyle)) &&
	    SGET_WINDOW_FONT(*pstyle) &&
	    (fw->title_font =
	     FlocaleLoadFont(dpy, SGET_WINDOW_FONT(*pstyle), "fvwm")))
```

Load the title (and icon) font at the owning-monitor pixelsize **before** `setup_title_geometry`. Do not scale `g.normal` / `g.frame` client size here (Phase 5).

---

### `fvwm/borders.c` (component, request-response)

**Analog:** in-place consumer of already-scaled fields (`fvwm/borders.c:4388-4394`):
```c
	ret_td->bar_g.width = new_g->width - 2 * fw->boundary_width;
	ret_td->bar_g.height = ret_td->layout.title_g.height;
	ret_td->bar_g.x = fw->boundary_width;
	ret_td->bar_g.y = ret_td->layout.title_g.y;
```

Title-button X windows already size to `title_thickness`. After chrome setup stores scaled shorts, drawing/hit-targets follow. Do not re-scale in the draw path.

---

### `fvwm/menus.c` + `fvwm/menustyle.h` + `fvwm/icons.c` + `fvwm/ewmh_icons.c` (controller, request-response/transform)

**Active-monitor lookup** (`fvwm/menus.c:2017-2019` + `libs/FScreen.c:1320-1328`):
```c
	fscr.xypos.x = pmp->screen_origin_x;
	fscr.xypos.y = pmp->screen_origin_y;
	FScreenGetScrRect(&fscr, FSCREEN_XYPOS, &sx, &sy, &sw, &sh);
```

```c
	case FSCREEN_XYPOS:
		m = FindScreenOfXY(arg->xypos.x, arg->xypos.y);
```

Prefer `FindScreenOfXY` / `monitor_by_xy` (`libs/FScreen.h:183,283`) when the factor is needed, not just the rectangle. Do not use `monitor_get_all_widths()` as a scale space.

**Menu border width is `unsigned char`** (`fvwm/menustyle.h:60-61,244-246`):
```c
	unsigned char ReliefThickness;
	unsigned char TitleUnderlines;
	unsigned char BorderWidth;
```

Scale into `int`, clamp to scaled `MAX_MENU_BORDER_WIDTH` (`libs/defaults.h:66-67`) with `fvwm_debug`; never wrap. Same for `MENU_TRIANGLE_WIDTH`/`HEIGHT` and `DEFAULT_MENU_BORDER_WIDTH` (`libs/defaults.h:66,75-76`). Time constants (`DEFAULT_CLICKTIME`) stay milliseconds.

**Icon geometry already uses XYPOS** (`fvwm/icons.c:1725-1729`):
```c
    get_icon_geometry(t, &g);
    fscr.xypos.x = g.x + g.width / 2;
    fscr.xypos.y = g.y + g.height / 2;
    FScreenGetScrRect(&fscr, FSCREEN_XYPOS, &sx, &sy, &sw, &sh);
```

Iconified UI: monitor containing icon geometry (else pointer). Scale wanted sizes at create, not on every expose.

**Wanted sizes are source pixels** (`fvwm/ewmh_icons.c:661-668`):
```c
#define MINI_ICON_WANTED_WIDTH  16
#define MINI_ICON_WANTED_HEIGHT 16
#define ICON_WANTED_WIDTH 56
#define ICON_WANTED_HEIGHT 56
```

`monitor_scale_apply` those constants (and max caps) with the active factor, then pass into `FEwmhIconFindBest` (`libs/ImageSafety.h:12-15`). Keep `FImageSizeIsSafe`.

---

### `fvwm/cursor.c` (controller, request-response)

**Imports** (`fvwm/cursor.c:18-39`): `#include "config.h"` first, then `"libs/PictureImageLoader.h"`, `"cursor.h"`, `"libs/log.h"`.

**Process-global table** (`fvwm/cursor.c:289-300` + `fvwm/cursor.h:16-44`):
```c
Cursor *CreateCursors(Display *disp)
{
	int i;
	cursors[0] = None;
	for (i = 1; i < CRS_MAX; i++)
	{
		cursors[i] = XCreateFontCursor(disp, default_cursors[i]);
	}
	return cursors;
}
```

When `HAVE_XCURSOR`, load theme equivalents via `XcursorShapeLoadCursor` / `Fcursor*` at explicit nominal size; cache per `(shape or path, quarters, generation)`. Active/pointer monitor supplies the factor (`FindScreenOfXY` / `FSCREEN_CURRENT`). If Xcursor is absent, keep `XCreateFontCursor` (1x-only core glyphs). Never `XcursorSetDefaultSize`.

---

### `fvwm/virtual.c` (controller, request-response)

**Keep 0–2 source range** (`fvwm/virtual.c:2071-2077,2084-2086`):
```c
	if (val < 0 || val > 2)
	{
		fvwm_debug(__func__,
			   "EdgeThickness arg must be between 0 and 2,"
			   " found %d",val);
		return;
	}
	/* ... */
	m->virtual_scr.edge_thickness = val;
	initPanFrames(m);
```

**Scale at map time** (`fvwm/virtual.c:1220-1279`):
```c
	edge_thickness = ref->virtual_scr.edge_thickness;
	saved_thickness = edge_thickness;
	if (edge_thickness == 0)
	{
		edge_thickness = 2;
	}
	/* ... create PanFrame windows at edge_thickness ... */
	ref->virtual_scr.edge_thickness = saved_thickness;
```

Keep storing source 0–2 on `virtual_scr.edge_thickness` (`libs/FScreen.h:144-145`). If source is 0, leave disabled (today it still creates 2 px windows then restores 0). At map, `edge_px = monitor_scale_apply(source, m->effective_scale.factor)` for that pan frame's monitor. Do not raise the config maximum.

---

### `fvwm/move_resize.c` + `fvwm/functions.c` + `fvwm/screen.h` (controller, request-response)

**Snap uses stored pixel integers** (`fvwm/move_resize.c:2367-2382`):
```c
static void DoSnapGrid(FvwmWindow *fw, rectangle self, position *new)
{
	int grid_x = self.x / fw->snap_grid_x * fw->snap_grid_x;
```

Scale `snap_proximity` / `snap_grid_*` with the **owning** monitor (`fw->m->effective_scale.factor`) at use, from style **source** integers (`libs/defaults.h:259-262` conceptually; style stores unscaled). Do not persist a scaled value back into the style.

**MoveThreshold is a source integer on `Scr`** (`fvwm/move_resize.c:3310-3321`, `fvwm/screen.h:421`, `fvwm/functions.c:762`):
```c
void CMD_MoveThreshold(F_CMD_ARGS)
{
	if (GetIntegerArguments(action, NULL, &val, 1) < 1 || val < 0)
		Scr.MoveThreshold = DEFAULT_MOVE_THRESHOLD; /* 3 */
	else
		Scr.MoveThreshold = val;
}
```

Keep storing source. Scale by **pointer/active** monitor at drag-start (`CheckActionType`, rubber-band). Command table (`fvwm/functable.c:367`) stays unchanged.

Put `wm_ui_resource_generation` on `ScreenInfo` (`fvwm/screen.h`) starting at 1; Phase 4 never increments it.

---

### `libs/meson.build` + `tests/meson.build` (config, batch)

**Library source list** (`libs/meson.build:1-64,55-57`):
```meson
    'monitor_ownership.c',
    'monitor_scale.c',
    'monitor_topology.c',
```

Insert `'wm_ui_metrics.c'` next to `monitor_scale.c` (alphabetically among `monitor_*` / `w*` as the local list already is mostly alpha).

**Native test that links `libfvwm3`** (`tests/meson.build:20-29`):
```meson
monitor_scale_test = executable(
    'test-monitor-scale',
    'test_monitor_scale.c',
    include_directories: includedirs,
    dependencies: all_found_deps,
    link_with: libfvwm3,
    c_args: ['-UNDEBUG'],
)

test('monitor-scale', monitor_scale_test)
```

Copy for `wm-ui-metrics` (`test_wm_ui_metrics.c`, `link_with: libfvwm3`, `-UNDEBUG`). Copy a slimmer variant for `supported-features` (needs `includedirs` for `config.h`; `link_with` optional). Four-space indent (`.editorconfig` for Meson).

---

### `.github/workflows/ccpp.yml` (config, batch)

**Three setup sites** (`.github/workflows/ccpp.yml:47,67-70,98`):
```yaml
meson setup builddir --buildtype=debugoptimized -Dhtmldoc=true -Dmandoc=true
# ...
meson setup builddir \
  --buildtype=debug \
  -Dgolang=disabled \
  -Db_sanitize=address,undefined
# ...
meson setup builddir -Dlive-x-tests=true -Dgolang=disabled
```

Append the same supported-profile flags to **all three**:
`-Dxcursor=enabled -Dxrender=enabled -Dpng=enabled -Dsvg=enabled -Dcairo=enabled`.

Do not change `meson.options` defaults (`auto` at `meson.options:67,80-101`). Detection in `meson.build:327-416` already maps `enabled` → `required: true` and sets `HAVE_XCURSOR` / `HAVE_XRENDER` / `HAVE_PNG` / `HAVE_RSVG`.

---

### `doc/fvwm3_manpage_source.adoc` (config, file-I/O)

**Existing scale contract to extend** (`doc/fvwm3_manpage_source.adoc:7505-7515`):
```
Scale-sensitive integer conversion rounds to nearest with exact half ties
away from zero for both signs.  Overflow is an explicit failure and never
wraps or clamps.  Each conversion uses the retained exact source value;
...
These scales apply to FVWM-owned UI and later FVWM geometry policy.  X11,
ICCCM, EWMH, and existing module geometry remain actual root pixels
```

Document that listed chrome pixel metrics (title thickness, borders, handles, fonts, menus, icons, cursors/hot spots, EdgeThickness mapped size, snap, MoveThreshold) follow the owning or active monitor scale, remain actual X11 pixels, and that Xft DPI / Xcursor default size are not mutated. Keep the GEO-07 sentence that packets stay unscaled.

## Shared Patterns

### `config.h` first + GPL on new `.c` files
**Source:** `libs/monitor_scale.c:1-18`, `dev-docs/DEVELOPERS.md`
**Apply to:** `libs/wm_ui_metrics.c`, `tests/test_wm_ui_metrics.c`, `tests/test_supported_features.c`
Headers get include guards only (no GPL), matching `libs/monitor_scale.h`.

### Only `monitor_scale_apply` converts pixels
**Source:** `libs/monitor_scale.h:94-98`, `libs/monitor_scale.c:243-254`
**Apply to:** every chrome/menu/icon/cursor/pan/snap/threshold site
Never `double`, never `* quarters / 4` locally, never chain rounded results.

### Overflow: init 0, keep last good, log
**Source:** `libs/monitor_scale.c:243-250`; caller policy from RESEARCH (not clamp to `INT_MAX`)
**Apply to:** metric converter callers in `geometry.c`, `decorations.c`, `virtual.c`, menus, cursors
```c
if (converted != NULL) {
	*converted = 0;
}
```
On `MONITOR_SCALE_CONVERSION_OVERFLOW` / `INVALID_ARGUMENT`, keep last-good (or unscaled source) and `fvwm_debug(__func__, ...)`.

### Image dimension safety
**Source:** `libs/ImageSafety.c:3-16`, `libs/PictureImageLoader.c:96-97,365-368`
**Apply to:** SVG/PNG/XPM/cursor rasters
Keep `FImageSizeIsSafe` with `PIMAGE_MAX_DIMENSION` 32768 and `PIMAGE_MAX_PIXELS` 64M.

### Owning vs active monitor
**Source:** `libs/FScreen.h:25-31,104,183,283`; `libs/FScreen.c:1312-1328`; `fvwm/menus.c:2017-2019`
**Apply to:** all UI-01 vs UI-03/UI-04/UI-05 sites

| Surface | Monitor |
|---------|---------|
| Title/border/font/buttons/handles | `fw->m->effective_scale.factor` |
| Menus, cursors, MoveThreshold, rubber-band | `FindScreenOfXY` / `FSCREEN_CURRENT` |
| Icons | icon-geometry monitor else pointer |
| Pan frames | that frame's `m->effective_scale.factor` |
| Snap | owning `fw->m` |

### Cache key extras
**Source:** `libs/Picture.c:126-128`, `libs/Flocale.c:1431-1446` (the seams to extend)
**Apply to:** Picture, Flocale, cursor tables
Key `(name/path, scale_quarters, target w/h if any, generation, existing stamp/fpa)`. Generation starts at 1 on `ScreenInfo` and is not incremented in this phase. Default 1x/generation-1 must still hit for unscaled module callers.

### Forbidden display-global DPI
**Source:** RESEARCH anti-patterns; `libs/Fft.c:155-171`; `libs/Fcursor.h:11-12`
**Apply to:** Fft, cursor, PictureImageLoader
Do not call `XftDefaultSet`, `XcursorSetDefaultSize`, or write `Xft.dpi` / `XCURSOR_SIZE`. Pass explicit `pixelsize` / load size.

### Protocol units stay actual pixels
**Source:** `fvwm/fvwm.h:782-796`
**Apply to:** all chrome writes
`title_thickness` / `boundary_width` remain `short` actual X11 pixels after scaling. Do not edit `libs/vpacket.h` or `CONFIGARGS`.

### Native tests, no new live-X chrome matrix
**Source:** `tests/meson.build:20-29`; `tests/test_monitor_scale.c`
**Apply to:** Wave 0 tests
Meson C assertions via `python3 tests/run_in_linux.py`. Existing live-X is non-regression only.

### Formatting
**Source:** `.clang-format` / `.editorconfig` / `fvwm/event_loop_policy.c`
New C: LLVM/Mozilla braces, tabs width 8, 80 columns, `PointerAlignment: Right`. Meson: four spaces.

## No Analog Found

| File | Role | Data Flow | Reason |
|------|------|-----------|--------|
| `tests/test_supported_features.c` (HAVE_* asserts) | test | request-response | No existing test asserts Meson `HAVE_*` macros. Copy assert-`main` from `tests/test_image_safety.c` and `config.h` include from `tests/test_monitor_scale.c`. Planner should also use RESEARCH Pattern 7. |

Per-scale cursor tables and resource-generation fields are new data on existing seams (`CreateCursors`, `ScreenInfo`, `FvwmPicture`, `FlocaleFont`), not new files.

## Metadata

**Analog search scope:** `libs/`, `fvwm/`, `tests/`, `doc/`, `.github/workflows/`, `meson.build`, `meson.options`, `libs/meson.build`, `tests/meson.build`
**Files scanned:** ~35 (headers, converters, caches, chrome, CI, docs)
**Pattern extraction date:** 2026-08-31
