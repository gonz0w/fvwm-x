/* -*-c-*- */
/* Copyright (C) 2002 the late Joey Shutup.
 *
 * http://www.streetmap.co.uk/streetmap.dll?postcode2map?BS24+9TZ
 *
 * No guarantees or warranties or anything are provided or implied in any way
 * whatsoever.  Use this program at your own risk.  Permission to use this
 * program for any purpose is given, as long as the copyright is kept intact.
 */
/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see: <http://www.gnu.org/licenses/>
 */

/* ---------------------------- included header files ---------------------- */

#include "config.h"
#include <assert.h>
#include <stdint.h>

#include "libs/log.h"
#include "libs/fvwm_x11.h"
#include "libs/fvwmlib.h"
#include "libs/Parse.h"
#include "libs/picture_types.h"
#include "libs/FShape.h"
#include "libs/ColorUtils.h"
#include "libs/Picture.h"
#include "libs/PictureUtils.h"
#include "libs/Graphics.h"
#include "libs/PictureGraphics.h"
#include "libs/picture_async.h"
#include "image_async.h"
#include "libs/picture_color.h"
#include "libs/FRenderInit.h"
#include "libs/Strings.h"
#include "libs/Grab.h"
#include "colorset.h"
#include "query_json.h"
#include "externs.h"
#include "fvwm.h"
#include "cursor.h"
#include "functions.h"
#include "commands.h"
#include "misc.h"
#include "screen.h"
#include "module_interface.h"
#include "execcontext.h"
#include "builtins.h"

/* ---------------------------- local definitions -------------------------- */

/* ---------------------------- local macros ------------------------------- */

/* ---------------------------- imports ------------------------------------ */


/* ---------------------------- included code files ------------------------ */

/* ---------------------------- local types -------------------------------- */

/* When fvwm destroys pixmaps it puts them on a list and only destroys them
 * after some period of inactivity. This is necessary because changing colorset
 * options rapidly may result in a module redrawing itself due to the first
 * change while the second change is happening. If the module renders something
 * with the colorset affected by the second change there is a chance it may
 * reference pixmaps that FvwmTheme has destroyed, bad things would happen */
struct junklist
{
	struct junklist *prev;
	Pixmap pixmap;
	FvwmPicture *picture;
};

struct root_pic
{
	Pixmap pixmap;
	Pixmap old_pixmap;
	int width;
	int height;
};

/* ---------------------------- forward declarations ----------------------- */

/* ---------------------------- local variables ---------------------------- */

/* Conservative owner revision: admission, allocation and async settlement all
 * invalidate observations. Exhaustion disables queries instead of wrapping. */
static uint64_t colorset_generation = 1;

static void colorset_query_changed(void)
{
	if (colorset_generation)
		colorset_generation++;
}

bool colorset_query_token(char output[MODULE_QUERY_GENERATION_LEN + 1])
{
	return fvwm_query_token(colorset_generation, output);
}

static char *black = "black";
static char *white = "white";
static char *gray = "gray";

static struct junklist *junk = NULL;
static Bool cleanup_scheduled = False;
static struct root_pic root_pic = {None, None, 0, 0};

static char *csetopts[] =
{
	"Foreground",
	"Fore",
	"fg",

	"Background",
	"Back",
	"bg",

	"Hilight",
	"Hilite",
	"hi",

	"Shadow",
	"Shade",
	"sh",

	"fgsh",
	"fg_alpha",
	"fgAlpha",

	/* these strings are used inside the cases in the switch below! */
	"Pixmap",
	"TiledPixmap",
	"AspectPixmap",

	/* these strings are used inside the cases in the switch below! */
	"Shape",
	"TiledShape",
	"AspectShape",

	/* switch off pixmaps and gradients */
	"Plain",
	/* switch off shape */
	"NoShape",

	/* Make the background transparent, copies the root window background */
	"Transparent",
	"RootTransparent",

	/* tint for the Pixmap or the gradient */
	"Tint",
	"PixmapTint",  /* ~ Tint */
	"ImageTint",   /* ~ Tint */
	"TintMask",    /* ~ Tint (backward compatibility) */
	"NoTint",      /* ~ Tint without argument */

	"fgTint",
	"bgTint",

	/* Dither the Pixmap or the gradient */
	"Dither",
	"NoDither",

	/* alpha for the Pixmap or the gradient */
	"Alpha",
	"PixmapAlpha",  /* ~ Alpha */
	"ImageAlpha",   /* ~ Alpha */

	/* Icon stuff */
	"DitherIcon",
	"NoDitherIcon",
	"IconTint",
	"NoIconTint",
	"IconAlpha",

	NULL
};

/* ---------------------------- exported variables (globals) --------------- */

/* ---------------------------- local functions ---------------------------- */
static
Pixmap get_root_pixmap(Atom prop)
{
	Atom type;
	int format;
	unsigned long length, after;
	unsigned char *reteval = NULL;
	int ret;
	Pixmap pix = None;

	ret = XGetWindowProperty(
		dpy, Scr.Root, prop, 0L, 1L, False, XA_PIXMAP,
		&type, &format, &length, &after, &reteval);
	if (
		ret == Success && type == XA_PIXMAP && format == 32 &&
		length == 1 && after == 0)
	{
		pix = (Pixmap)(*(long *)reteval);
	}
	if (reteval)
	{
		XFree(reteval);
	}
	return pix;
}

void update_root_pixmap(Atom prop)
{
	static Atom a_rootpix = None;
	int w = 0;
	int h = 0;
	XID dummy;
	Pixmap pix;

	if (a_rootpix == None)
	{
		a_rootpix = XInternAtom(dpy,"_XROOTPMAP_ID", False);
	}
	XSync(dpy, False);
	if (prop != 0)
	{
		pix = get_root_pixmap(prop);
		if (pix && !XGetGeometry(
			dpy, pix, &dummy, (int *)&dummy, (int *)&dummy,
			(unsigned int *)&w, (unsigned int *)&h,
			(unsigned int *)&dummy, (unsigned int *)&dummy))
		{
			pix = None;
		}
	}
	else
	{
		pix = get_root_pixmap(a_rootpix);
		if (pix && !XGetGeometry(
			dpy, pix, &dummy, (int *)&dummy, (int *)&dummy,
			(unsigned int *)&w, (unsigned int *)&h,
			(unsigned int *)&dummy, (unsigned int *)&dummy))
		{
			pix = None;
		}
	}
	root_pic.pixmap = pix;
	root_pic.width = w;
	root_pic.height = h;
#if 0
	fprintf(stderr,"Get New Root Pixmap: 0x%lx %i,%i\n",
		root_pic.pixmap, w, h);
#endif
}

static void add_to_junk(Pixmap pixmap)
{
	struct junklist *oldjunk = junk;

	junk = fxmalloc(sizeof(struct junklist));
	junk->prev = oldjunk;
	junk->pixmap = pixmap;
	junk->picture = NULL;
	if (!cleanup_scheduled)
	{
		const exec_context_t *exc;

		exc = exc_create_null_context();
		CMD_Schedule(NULL, exc, "3000 CleanupColorsets", NULL);
		exc_destroy_context(exc);
		cleanup_scheduled = True;
	}

	return;
}

static char *get_simple_color(
	char *string, char **color, colorset_t *cs, int supplied_color,
	int special_flag, char *special_string)
{
	char *rest;
	free(*color);
	*color = NULL;
	rest = GetNextToken(string, color);
	if (*color)
	{
		if (special_string && StrEquals(*color, special_string))
		{
			free(*color);
			*color = NULL;
			cs->color_flags |= special_flag;
			cs->color_flags &= ~supplied_color;
		}
		else
		{
			cs->color_flags |= supplied_color;
			cs->color_flags &= ~special_flag;
		}
	}
	else
	{
		cs->color_flags &= ~(supplied_color | special_flag);
	}

	return rest;
}

static void SafeDestroyPicture(Display *disp, FvwmPicture *picture)
{
	/* Keep the whole immutable backing, including its colors, alive for
	 * modules that still have the previous colorset packet. Never mutate
	 * cached/shared backing to transfer individual pixmaps into junk. */
	FvwmPicture *snapshot = picture_snapshot(picture);

	if (snapshot != NULL)
	{
		add_to_junk(None);
		junk->picture = snapshot;
	}
	PDestroyFvwmPicture(disp, picture);
}

enum colorset_color_slot
{
	COLORSET_FG, COLORSET_BG, COLORSET_HILITE, COLORSET_SHADOW,
	COLORSET_FGSH, COLORSET_TINT, COLORSET_ICON_TINT,
	COLORSET_FG_TINT, COLORSET_BG_TINT,
	COLORSET_FG_SAVED, COLORSET_BG_SAVED, COLORSET_COLOR_COUNT
};

static Pixel *colorset_color_slot(colorset_t *cs, unsigned slot)
{
	Pixel *slots[] = {&cs->fg, &cs->bg, &cs->hilite, &cs->shadow,
	    &cs->fgsh, &cs->tint, &cs->icon_tint, &cs->fg_tint, &cs->bg_tint,
	    &cs->fg_saved, &cs->bg_saved};

	return slots[slot];
}

static picture_color *colorset_color_capture(colorset_t *cs, unsigned slot)
{
	if (cs->color_owners[slot] == NULL)
	{
		cs->color_owners[slot] = picture_color_adopt(Pcmap,
		    *colorset_color_slot(cs, slot), True);
	}
	return cs->color_owners[slot];
}

static void colorset_color_release(colorset_t *cs, unsigned slot)
{
	if (cs->color_owners[slot] != NULL)
	{
		picture_color_release(dpy, cs->color_owners[slot]);
		cs->color_owners[slot] = NULL;
	}
	else if (slot < COLORSET_FG_SAVED)
	{
		PictureFreeColors(dpy, Pcmap, colorset_color_slot(cs, slot),
		    1, 0, True);
	}
}

static void colorset_color_save(colorset_t *cs, unsigned current,
	unsigned saved)
{
	colorset_color_release(cs, saved);
	cs->color_owners[saved] = picture_color_retain(
	    colorset_color_capture(cs, current));
	*colorset_color_slot(cs, saved) = *colorset_color_slot(cs, current);
}

struct colorset_preparation
{
	colorset_t requested;
	colorset_t *published;
	FvwmPicture *effect;
	int id;
	Bool refused;
	Bool effect_needed;
	size_t references;
};

static void free_colorset_background(colorset_t *cs, Bool do_free_args);
static void free_colorset_shape(colorset_t *cs);
static void colorset_preparation_publish(
	struct colorset_preparation *preparation);

/* Wrap already published legacy background resources once. The source picture
 * owns its alpha; this wrapper owns only private background/mask/palette
 * data. */
static void colorset_background_capture(colorset_t *cs)
{
	if (cs->rendered_picture != NULL &&
	    cs->rendered_picture->picture != cs->pixmap)
	{
		SafeDestroyPicture(dpy, cs->rendered_picture);
		cs->rendered_picture = NULL;
	}
	if (cs->rendered_picture == NULL && cs->pixmap != None &&
	    cs->pixmap != ParentRelative && cs->pixmap != root_pic.pixmap &&
	    cs->pixmap != root_pic.old_pixmap &&
	    (cs->picture == NULL || cs->pixmap != cs->picture->picture))
	{
		cs->rendered_picture = PLoadFvwmPictureFromPixmap(dpy,
		    Scr.NoFocusWin, NULL, cs->pixmap, cs->mask, None,
		    cs->width, cs->height, cs->nalloc_pixels, cs->pixels, False);
		cs->pixels = NULL;
		cs->nalloc_pixels = 0;
	}
}

static colorset_t *colorset_preparation_begin(
	int id, colorset_t *published)
{
	if (published->preparation != NULL)
	{
		return &published->preparation->requested;
	}
	colorset_background_capture(published);
	struct colorset_preparation *preparation =
	    fxcalloc(1, sizeof(*preparation));

	preparation->references = 1; /* Published revision slot. */
	preparation->published = published;
	preparation->id = id;
	preparation->requested = *published;
	colorset_t *requested = &preparation->requested;

	requested->preparing = True;
	requested->preparation = preparation;
	requested->pixmap_args = published->pixmap_args ?
	    fxstrdup(published->pixmap_args) : NULL;
	requested->gradient_args = published->gradient_args ?
	    fxstrdup(published->gradient_args) : NULL;
	requested->picture = picture_snapshot(published->picture);
	requested->shape_picture = picture_snapshot(published->shape_picture);
	requested->rendered_picture =
	    picture_snapshot(published->rendered_picture);
	for (unsigned i = 0; i < COLORSET_COLOR_COUNT; i++)
	{
		if (i < COLORSET_FG_SAVED)
		{
			colorset_color_capture(published, i);
		}
		requested->color_owners[i] = picture_color_retain(
		    published->color_owners[i]);
	}
	published->preparation = preparation;
	return requested;
}

/* A commit batch temporarily retains cancelled views. Their bindings must
 * keep revision context alive even if a sibling callback publishes it. */
static void colorset_preparation_retain(
	struct colorset_preparation *preparation)
{
	if (preparation != NULL)
	{
		assert(preparation->references > 0 &&
		    preparation->references < SIZE_MAX);
		preparation->references++;
	}
}

static void colorset_preparation_release(void *context)
{
	struct colorset_preparation *preparation = context;

	if (preparation != NULL)
	{
		assert(preparation->references > 0);
		if (--preparation->references == 0)
		{
			free(preparation);
		}
	}
}

static picture_async_commit colorset_effect_policy(FvwmPicture *view,
	void *context)
{
	struct colorset_preparation *preparation = context;

	return preparation->published->preparation == preparation &&
	    preparation->effect == view ? PICTURE_ASYNC_APPLY :
	    PICTURE_ASYNC_DISCARD;
}

const char *colorset_query_update_state(int id)
{
	const colorset_t *cs = FindColorset(id);

	if (!cs)
		return "unavailable";
	if (!cs->preparation)
		return "idle";
	return cs->preparation->refused ? "refused" : "pending";
}

static void colorset_effect_settled(FvwmPicture *view, Bool applied,
	void *context)
{
	struct colorset_preparation *preparation = context;
	colorset_t *cs = &preparation->requested;

	if (preparation->effect != view)
	{
		return;
	}
	colorset_query_changed();
	preparation->effect = NULL;
	if (applied)
	{
		if (cs->rendered_picture != NULL)
		{
			SafeDestroyPicture(dpy, cs->rendered_picture);
		}
		cs->rendered_picture = picture_snapshot(view);
		cs->pixmap = cs->rendered_picture->picture;
		cs->mask = None;
		preparation->effect_needed = False;
	}
	else
	{
		preparation->refused = True;
	}
	PDestroyFvwmPicture(dpy, view);
	colorset_preparation_publish(preparation);
}

static Bool colorset_prepare_effect(colorset_t *cs)
{
	struct colorset_preparation *preparation = cs->preparation;
	preparation->effect_needed = True;
	FvwmPicture *view = image_async_effect(cs->picture,
	    colorset_color_capture(cs, COLORSET_BG),
	    colorset_color_capture(cs, COLORSET_TINT), cs->tint_percent,
	    cs->image_alpha_percent);

	if (view == NULL)
	{
		preparation->refused = True;
		fvwm_debug(__func__, "colorset %d effect refused",
		    preparation->id);
		return False;
	}
	preparation->effect = view;
	colorset_preparation_retain(preparation);
	if (!picture_async_bind(view, colorset_effect_policy,
	    colorset_effect_settled, preparation, colorset_preparation_release))
	{
		preparation->effect = NULL;
		preparation->refused = True;
		colorset_preparation_release(preparation);
		PDestroyFvwmPicture(dpy, view);
		return False;
	}
	return True;
}

static void colorset_preparation_publish(
	struct colorset_preparation *preparation)
{
	colorset_t *requested = &preparation->requested;
	colorset_t *published = preparation->published;

	if (preparation->refused || preparation->effect != NULL ||
	    requested->pending_picture != NULL ||
	    requested->pending_shape != NULL)
	{
		return;
	}
	/* All requested resources are acknowledged. Move the revision as one
	 * publication; previous module resources retain their delayed owners. */
	free_colorset_background(published, True);
	free_colorset_shape(published);
	for (unsigned i = 0; i < COLORSET_COLOR_COUNT; i++)
	{
		colorset_color_release(published, i);
	}
	colorset_query_changed();
	*published = *requested;
	published->preparation = NULL;
	published->preparing = False;
	int id = preparation->id;

	colorset_preparation_release(preparation);
	BroadcastColorset(id);
	update_fvwm_colorset(id);
}

static void cancel_colorset_picture(FvwmPicture **slot)
{
	FvwmPicture *old = *slot;

	*slot = NULL;
	PDestroyFvwmPicture(dpy, old);
}

typedef struct
{
	colorset_t *colorset;
	int id;
	Bool shape;
	struct colorset_preparation *preparation;
} colorset_image_owner;

static void colorset_image_owner_release(void *context)
{
	colorset_image_owner *owner = context;

	colorset_preparation_release(owner->preparation);
	free(owner);
}

static void parse_colorset_update(int n, char *line,
	FvwmPicture *prepared, unsigned int prepared_type);

static picture_async_commit colorset_image_policy(FvwmPicture *view,
	void *context)
{
	colorset_image_owner *owner = context;
	colorset_t *cs = owner->colorset;
	FvwmPicture *current = owner->shape ? cs->pending_shape :
	    cs->pending_picture;

	return current == view && (owner->preparation == NULL ||
	    owner->preparation->published->preparation == owner->preparation) ?
	    PICTURE_ASYNC_APPLY : PICTURE_ASYNC_DISCARD;
}

static void free_colorset_shape(colorset_t *cs)
{
	if (cs->shape_picture != NULL)
	{
		/* Modules may still render a previous colorset packet. Keep its
		 * entire backing alive through the existing delayed cleanup. */
		SafeDestroyPicture(dpy, cs->shape_picture);
		cs->shape_picture = NULL;
	}
	else if (cs->shape_mask != None)
	{
		add_to_junk(cs->shape_mask);
	}
	cs->shape_mask = None;
}

static Bool apply_colorset_shape(colorset_t *cs, FvwmPicture *picture,
	unsigned int type)
{
	if (!picture_is_usable(picture))
	{
		return False;
	}
	if (picture->depth != 1 && picture->mask == None)
	{
		fvwm_debug(__func__, "shape pixmap must be of depth 1");
		return False;
	}
	FvwmPicture *snapshot = picture_snapshot(picture);

	if (snapshot == NULL)
	{
		return False;
	}
	free_colorset_shape(cs);
	/* The loader already acknowledged this immutable mask. Borrow it
	 * directly: no second allocation/copy or publication barrier. */
	cs->shape_picture = snapshot;
	cs->shape_mask = snapshot->mask != None ? snapshot->mask :
	    snapshot->picture;
	cs->shape_width = picture->width;
	cs->shape_height = picture->height;
	cs->shape_type = type;
	return True;
}

static void colorset_image_settled(FvwmPicture *view, Bool applied,
	void *context)
{
	colorset_image_owner *owner = context;
	colorset_t *cs = owner->colorset;
	FvwmPicture **slot = owner->shape ? &cs->pending_shape :
	    &cs->pending_picture;
	int id = owner->id;

	if (*slot != view)
	{
		return;
	}
	colorset_query_changed();
	*slot = NULL;
	if (applied && owner->shape)
	{
		if (apply_colorset_shape(cs, view,
		    cs->pending_shape_type))
		{
			if (!cs->preparing)
			{
				BroadcastColorset(id);
				update_fvwm_colorset(id);
			}
		}
		else if (cs->preparing)
		{
			cs->preparation->refused = True;
		}
	}
	else if (applied)
	{
		Bool staged = cs->preparing;

		/* Publish an immutable snapshot, not the scheduler's mutable
		 * lease. Reuse final tint/average/color processing without
		 * parsing or dispatching a reconstructed command. */
		parse_colorset_update(id, NULL, picture_snapshot(view),
		    cs->pending_pixmap_type);
		PDestroyFvwmPicture(dpy, view);
		if (!staged)
		{
			update_fvwm_colorset(id);
		}
		return;
	}
	else if (cs->preparing)
	{
		cs->preparation->refused = True;
	}
	if (owner->shape && cs->preparing)
	{
		struct colorset_preparation *preparation = cs->preparation;

		PDestroyFvwmPicture(dpy, view);
		colorset_preparation_publish(preparation);
		return;
	}
	PDestroyFvwmPicture(dpy, view);
}

static void bind_colorset_image(int id, colorset_t *cs,
	FvwmPicture *view, Bool shape)
{
	colorset_image_owner *owner = fxmalloc(sizeof(*owner));

	*owner = (colorset_image_owner){cs, id, shape,
	    cs->preparing ? cs->preparation : NULL};
	colorset_preparation_retain(owner->preparation);
	if (!picture_async_bind(view, colorset_image_policy,
	    colorset_image_settled, owner, colorset_image_owner_release))
	{
		/* A pending loader lease can be bound only once. */
		colorset_image_owner_release(owner);
		PDestroyFvwmPicture(dpy, view);
		return;
	}
	FvwmPicture **slot = shape ? &cs->pending_shape :
	    &cs->pending_picture;

	cancel_colorset_picture(slot);
	*slot = view;
}

static void free_colorset_background(colorset_t *cs, Bool do_free_args)
{
	if (cs->rendered_picture != NULL)
	{
		SafeDestroyPicture(dpy, cs->rendered_picture);
		cs->rendered_picture = NULL;
		cs->pixmap = cs->mask = None;
	}

	if (do_free_args)
	{
		cancel_colorset_picture(&cs->pending_picture);
	}
	if (cs->picture != NULL)
	{
		if (cs->pixmap != None && cs->pixmap != ParentRelative &&
		    cs->pixmap != root_pic.pixmap &&
		    cs->pixmap != root_pic.old_pixmap &&
		    cs->picture->picture != cs->pixmap)
		{
			add_to_junk(cs->pixmap);
		}
		SafeDestroyPicture(dpy, cs->picture);
		cs->picture = NULL;
		cs->pixmap = None;
		cs->alpha_pixmap = None; /* alaways equal to picture->alpha */
	}
	if (cs->pixmap && cs->pixmap != ParentRelative &&
	    cs->pixmap != root_pic.pixmap && cs->pixmap != root_pic.old_pixmap)
	{
		add_to_junk(cs->pixmap);
	}
	cs->pixmap = None;
	if (cs->mask)
	{
		add_to_junk(cs->mask);
		cs->mask = None;
	}
	if (cs->alpha_pixmap)
	{
		add_to_junk(cs->alpha_pixmap);
		cs->alpha_pixmap = None;
	}
	if (cs->pixels && cs->nalloc_pixels)
	{
		PictureFreeColors(
			dpy, Pcmap, cs->pixels, cs->nalloc_pixels, 0, False);
		free(cs->pixels);
		cs->pixels = NULL;
		cs->nalloc_pixels = 0;
	}
	if (do_free_args)
	{
		free(cs->pixmap_args);
		cs->pixmap_args = NULL;
		free(cs->gradient_args);
		cs->gradient_args = NULL;
		cs->is_maybe_root_transparent = False;
		cs->pixmap_type = 0;
	}
}

static Bool colorset_uses_async_effects(const colorset_t *cs)
{
	/* Depth-one pictures retain the legacy bitmap path. */
	return cs->preparing && Pdepth > 1 && cs->picture != NULL &&
	    cs->picture->depth == Pdepth;
}

static void reset_cs_pixmap(colorset_t *cs, GC gc)
{
	if (colorset_uses_async_effects(cs))
	{
		cs->pixmap = cs->picture->picture;
		return;
	}

	if (Pdepth == cs->picture->depth)
	{
		XCopyArea(dpy, cs->picture->picture, cs->pixmap, gc,
			  0, 0, cs->width, cs->height, 0, 0);
	}
	else
	{
		XCopyPlane(dpy, cs->picture->picture, cs->pixmap, gc,
			   0, 0, cs->width, cs->height, 0, 0, 1);
	}

	return;
}

static void parse_pixmap(
	Window win, GC gc, colorset_t *cs, FvwmPicture *picture,
	Bool *pixmap_is_a_bitmap)
{
	cs->picture = picture;
	if (!picture_is_usable(picture))
	{
		return;
	}
	if (cs->picture->depth != Pdepth)
	{
		*pixmap_is_a_bitmap = True;
		cs->pixmap = None; /* build cs->pixmap later */
	}
	/* copy the picture pixmap into the public structure */
	cs->width = cs->picture->width;
	cs->height = cs->picture->height;
	cs->alpha_pixmap = cs->picture->alpha;

	if (colorset_uses_async_effects(cs))
	{
		cs->pixmap = cs->picture->picture;
		return;
	}
	if (!*pixmap_is_a_bitmap)
	{
		cs->pixmap = XCreatePixmap(dpy, win,
					   cs->width, cs->height,
					   Pdepth);
		XSetClipMask(dpy, gc, cs->picture->mask);
		XCopyArea(dpy, cs->picture->picture, cs->pixmap, gc,
			  0, 0, cs->width, cs->height, 0, 0);
		XSetClipMask(dpy, gc, None);
	}
	if (cs->pixmap)
	{
		if (cs->picture->mask != None)
		{
			/* make an inverted copy of the mask */
			cs->mask = XCreatePixmap(dpy, win,
						 cs->width, cs->height,
						 1);
			if (cs->mask)
			{
				XCopyArea(dpy, cs->picture->mask, cs->mask,
					  Scr.MonoGC,
					  0, 0, cs->width, cs->height, 0, 0);
				/* Invert the mask. We use it to draw the
				 * background. */
				XSetFunction(dpy, Scr.MonoGC, GXinvert);
				XFillRectangle(dpy, cs->mask, Scr.MonoGC,
					       0, 0, cs->width, cs->height);
				XSetFunction(dpy, Scr.MonoGC, GXcopy);
			}
		}
	}
}

static void parse_shape(Window win, colorset_t *cs, int id, int i,
	char *args, int *has_shape_changed)
{
	char *token;
	FvwmPicture *picture;
	FvwmPictureAttributes fpa = {.mask = FPAM_NO_ALPHA};
	unsigned int type = csetopts[i][0] == 'T' ? SHAPE_TILED :
	    csetopts[i][0] == 'A' ? SHAPE_STRETCH_ASPECT : SHAPE_STRETCH;

	if (!FHaveShapeExtension)
	{
		return;
	}
	token = PeekToken(args, &args);
	picture = token != NULL ?
	    PCacheFvwmPicture(dpy, win, NULL, token, fpa) : NULL;
	if (picture != NULL && picture->is_view && picture->pending)
	{
		cs->pending_shape_type = type;
		bind_colorset_image(id, cs, picture, True);
		return;
	}
	cancel_colorset_picture(&cs->pending_shape);
	/* Keep immediate non-deferred refusal/clear behavior unchanged. */
	*has_shape_changed = True;
	free_colorset_shape(cs);
	cs->shape_type = type;
	if (picture == NULL && token != NULL)
	{
		fvwm_debug(__func__,
					    "can't load picture %s", token);
	}
	else if (picture != NULL)
	{
		apply_colorset_shape(cs, picture, type);
		SafeDestroyPicture(dpy, picture);
	}
}

static void parse_simple_tint(
	colorset_t *cs, char *args, char **tint, int supplied_color,
	int *changed, int *percent, char *cmd)
{
	char *rest;

	*changed = False;
	rest = get_simple_color(args, tint, cs, supplied_color, 0, NULL);
	if (!(cs->color_flags & supplied_color))
	{
		/* restore to default */
		*percent = 0;
		*changed = True;
		cs->color_flags &= ~(supplied_color);
	}
	else if (!GetIntegerArguments(rest, NULL, percent, 1))
	{
		fvwm_debug(__func__,
			   "%s must have two arguments a color and an integer",
			   cmd);
		return;
	}
	*changed = True;
	if (*percent > 100)
	{
		*percent = 100;
	}
	else if (*percent < 0)
	{
		*percent = 0;
	}
}

/* ---------------------------- interface functions ------------------------ */

void cleanup_colorsets(void)
{
	struct junklist *oldjunk = junk;

	while (junk)
	{
		if (junk->picture != NULL)
		{
			PDestroyFvwmPicture(dpy, junk->picture);
		}
		else
		{
			XFreePixmap(dpy, junk->pixmap);
		}
		oldjunk = junk;
		junk = junk->prev;
		free(oldjunk);
	}
	cleanup_scheduled = False;
}

/* translate a colorset spec into a colorset structure */
static void parse_colorset_update(int n, char *line,
	FvwmPicture *prepared, unsigned int prepared_type)
{
	int i;
	int w;
	int h;
	int tmp;
	int percent;
	colorset_t *cs;
	char *optstring;
	char *args;
	char *option;
	char *tmp_str;
	char *fg = NULL;
	char *bg = NULL;
	char *hi = NULL;
	char *sh = NULL;
	char *fgsh = NULL;
	char *tint = NULL;
	char *fg_tint = NULL;
	char *bg_tint = NULL;
	char *icon_tint = NULL;
	Bool have_pixels_changed = False;
	Bool has_icon_pixels_changed = False;
	Bool has_fg_changed = False;
	Bool has_bg_changed = False;
	Bool has_sh_changed = False;
	Bool has_hi_changed = False;
	Bool has_fgsh_changed = False;
	Bool has_fg_alpha_changed = False;
	Bool has_tint_changed = False;
	Bool has_fg_tint_changed = False;
	Bool has_bg_tint_changed = False;
	Bool has_icon_tint_changed = False;
	Bool has_pixmap_changed = False;
	Bool has_shape_changed = False;
	Bool has_image_alpha_changed = False;
	Bool pixmap_is_a_bitmap = False;
	Bool do_reload_pixmap = False;
	Bool is_server_grabbed = False;
	unsigned int requested_type;
	XColor color;
	XGCValues xgcv;
	Window win = Scr.NoFocusWin;
	static GC gc = None;

	/* initialize statics */
	if (gc == None)
	{
		gc = fvwmlib_XCreateGC(dpy, win, 0, &xgcv);
	}

	/* make sure it exists and has sensible contents */
	if (!alloc_colorset(n))
	{
		return;
	}
	colorset_query_changed();
	cs = GetColorset(n);
	if (image_async_enabled())
	{
		cs = colorset_preparation_begin(n, cs);
		struct colorset_preparation *preparation = cs->preparation;

		if (preparation->effect != NULL)
		{
			FvwmPicture *old = preparation->effect;

			preparation->effect = NULL;
			PDestroyFvwmPicture(dpy, old);
		}
		if (line != NULL)
		{
			preparation->refused = False;
		}
	}
	requested_type = cs->pending_picture != NULL ?
	    cs->pending_pixmap_type : cs->pixmap_type;
	if (prepared != NULL)
	{
		requested_type = prepared_type;
		do_reload_pixmap = True;
	}

	/*** Parse the options ***/
	while (line && *line)
	{
		/* Read next option specification delimited by a comma or \0.
		 */
		line = GetQuotedString(
			line, &optstring, ",", NULL, NULL, NULL);
		if (!optstring)
			break;
		args = GetNextToken(optstring, &option);
		if (!option)
		{
			free(optstring);
			break;
		}

		switch((i = GetTokenIndex(option, csetopts, 0, NULL)))
		{
		case 0: /* Foreground */
		case 1: /* Fore */
		case 2: /* fg */
			get_simple_color(
				args, &fg, cs, FG_SUPPLIED, FG_CONTRAST,
				"contrast");
			has_fg_changed = True;
			break;
		case 3: /* Background */
		case 4: /* Back */
		case 5: /* bg */
			get_simple_color(
				args, &bg, cs, BG_SUPPLIED, BG_AVERAGE,
				"average");
			has_bg_changed = True;
			break;
		case 6: /* Hilight */
		case 7: /* Hilite */
		case 8: /* hi */
			get_simple_color(args, &hi, cs, HI_SUPPLIED, 0, NULL);
			has_hi_changed = True;
			break;
		case 9: /* Shadow */
		case 10: /* Shade */
		case 11: /* sh */
			get_simple_color(args, &sh, cs, SH_SUPPLIED, 0, NULL);
			has_sh_changed = True;
			break;
		case 12: /* fgsh */
			get_simple_color(
				args, &fgsh, cs, FGSH_SUPPLIED, 0,NULL);
			has_fgsh_changed = True;
			break;
		case 13: /* fg_alpha */
		case 14: /* fgAlpha */
			if (GetIntegerArguments(args, NULL, &tmp, 1))
			{
				if (tmp > 100)
					tmp = 100;
				else if (tmp < 0)
					tmp = 0;
			}
			else
			{
				tmp = 100;
			}
			if (tmp != cs->fg_alpha_percent)
			{
				cs->fg_alpha_percent = tmp;
				has_fg_alpha_changed = True;
			}
			break;
		case 15: /* Pixmap */
		case 16: /* TiledPixmap */
		case 17: /* AspectPixmap */
			cancel_colorset_picture(&cs->pending_picture);
			free(cs->pixmap_args);
			cs->pixmap_args = NULL;
			free(cs->gradient_args);
			cs->gradient_args = NULL;
			cs->is_maybe_root_transparent = False;
			tmp_str = PeekToken(args, &args);
			if (tmp_str)
			{
				CopyString(&cs->pixmap_args, tmp_str);
				do_reload_pixmap = True;
				cs->gradient_type = 0;
				requested_type = csetopts[i][0] == 'T' ?
				    PIXMAP_TILED : csetopts[i][0] == 'A' ?
				    PIXMAP_STRETCH_ASPECT : PIXMAP_STRETCH;
			}
			else
			{
				free_colorset_background(cs, True);
				has_pixmap_changed = True;
			}
			break;
		case 18: /* Shape */
		case 19: /* TiledShape */
		case 20: /* AspectShape */
			parse_shape(win, cs, n, i, args, &has_shape_changed);
			break;
		case 21: /* Plain */
			has_pixmap_changed = True;
			free_colorset_background(cs, True);
			break;
		case 22: /* NoShape */
			cancel_colorset_picture(&cs->pending_shape);
			has_shape_changed = True;
			free_colorset_shape(cs);
			break;
		case 23: /* Transparent */

			/* This is only allowable when the root depth == fvwm
			 * visual depth otherwise bad match errors happen,
			 * it may be even more restrictive but my tests (on
			 * exceed 6.2) show that only == depth is necessary */

			if (Pdepth != DefaultDepth(dpy, (DefaultScreen(dpy))))
			{
				fvwm_debug(__func__, "can't do Transparent "
					   "when root_depth!=fvwm_depth");
				break;
			}
			has_pixmap_changed = True;
			free_colorset_background(cs, True);
			cs->pixmap = ParentRelative;
			cs->pixmap_type = PIXMAP_STRETCH;
			break;
		case 24: /* RootTransparent */
			if (Pdepth != DefaultDepth(dpy, (DefaultScreen(dpy))))
			{
				fvwm_debug(__func__,
					   "can't do RootTransparent "
					   "when root_depth!=fvwm_depth");
				break;
			}
			free_colorset_background(cs, True);
			has_pixmap_changed = True;
			cs->pixmap_type =
					    PIXMAP_ROOT_PIXMAP_PURE;
			do_reload_pixmap = True;
			tmp_str = PeekToken(args, &args);
			if (StrEquals(tmp_str, "buffer"))
			{
				cs->allows_buffered_transparency = True;
			}
			else
			{
				cs->allows_buffered_transparency = False;
			}
			cs->is_maybe_root_transparent = True;
			break;
		case 25: /* Tint */
		case 26: /* PixmapTint */
		case 27: /* ImageTint */
		case 28: /* TintMask */
			parse_simple_tint(
				cs, args, &tint, TINT_SUPPLIED,
				&has_tint_changed, &percent, "tint");
			if (has_tint_changed)
			{
				cs->tint_percent = percent;
			}
			break;
		case 29: /* NoTint */
			has_tint_changed = True;
			cs->tint_percent = 0;
			cs->color_flags &= ~TINT_SUPPLIED;
			break;
		case 30: /* fgTint */
			parse_simple_tint(
				cs, args, &fg_tint, FG_TINT_SUPPLIED,
				&has_fg_tint_changed, &percent, "fgTint");
			if (has_fg_tint_changed)
			{
				cs->fg_tint_percent = percent;
			}
			break;
		case 31: /* bgTint */
			parse_simple_tint(
				cs, args, &bg_tint, BG_TINT_SUPPLIED,
				&has_bg_tint_changed, &percent, "bgTint");
			if (has_bg_tint_changed)
			{
				cs->bg_tint_percent = percent;
			}
			break;
		case 32: /* dither */
			if (cs->pixmap_args || cs->gradient_args)
			{
				has_pixmap_changed = True;
				do_reload_pixmap = True;
			}
			cs->dither = True;
			break;
		case 33: /* nodither */
			if (cs->pixmap_args || cs->gradient_args)
			{
				has_pixmap_changed = True;
				do_reload_pixmap = True;
			}
			cs->dither = False;
			break;
		case 34: /* Alpha */
		case 35: /* PixmapAlpha */
		case 36: /* ImageAlpha */
			if (GetIntegerArguments(args, NULL, &tmp, 1))
			{
				if (tmp > 100)
					tmp = 100;
				else if (tmp < 0)
					tmp = 0;
			}
			else
			{
				tmp = 100;
			}
			if (tmp != cs->image_alpha_percent)
			{
				has_image_alpha_changed = True;
				cs->image_alpha_percent = tmp;
			}
			break;
                /* dither icon is not dynamic (yet) maybe a bad opt: default
		 * to False ? */
		case 37: /* ditherIcon */
			cs->do_dither_icon = True;
			break;
		case 38: /* DoNotDitherIcon */
			cs->do_dither_icon = False;
			break;
		case 39: /* IconTint */
			parse_simple_tint(
				cs, args, &icon_tint, ICON_TINT_SUPPLIED,
				&has_icon_tint_changed, &percent, "IconTint");
			if (has_icon_tint_changed)
			{
				cs->icon_tint_percent = percent;
				has_icon_pixels_changed = True;
			}
			break;
		case 40: /* NoIconTint */
			has_icon_tint_changed = True;
			if (cs->icon_tint_percent != 0)
			{
				has_icon_pixels_changed = True;
			}
			cs->icon_tint_percent = 0;
			break;
		case 41: /* IconAlpha */
			if (GetIntegerArguments(args, NULL, &tmp, 1))
			{
				if (tmp > 100)
					tmp = 100;
				else if (tmp < 0)
					tmp = 0;
			}
			else
			{
				tmp = 100;
			}
			if (tmp != cs->icon_alpha_percent)
			{
				has_icon_pixels_changed = True;
				cs->icon_alpha_percent = tmp;
			}
			break;
		default:
			/* test for ?Gradient */
			if (option[0] && StrEquals(&option[1], "Gradient"))
			{
				cs->gradient_type = toupper(option[0]);
				if (!IsGradientTypeSupported(cs->gradient_type))
					break;
				has_pixmap_changed = True;
				free_colorset_background(cs, True);
				CopyString(&cs->gradient_args, args);
				do_reload_pixmap = True;
				if (cs->gradient_type == V_GRADIENT)
				{
					cs->pixmap_type = PIXMAP_STRETCH_Y;
				}
				else if (cs->gradient_type == H_GRADIENT)
					cs->pixmap_type = PIXMAP_STRETCH_X;
				else
					cs->pixmap_type = PIXMAP_STRETCH;
			}
			else
			{
				fvwm_debug(__func__, "bad colorset pixmap "
					   "specifier %s %s", option, line);
			}
			break;
		} /* switch */
		free(option);
		option = NULL;
		free(optstring);
		optstring = NULL;
	} /* while (line && *line) */

	/*
	 * ---------- change the "pixmap" tint colour ----------
	 */
	if (has_tint_changed)
	{
		/* user specified colour */
		if (tint != NULL)
		{
			Pixel old_tint = cs->tint;
			colorset_color_release(cs, COLORSET_TINT);
			cs->tint = GetColor(tint);
			if (old_tint != cs->tint)
			{
				have_pixels_changed = True;
			}
		}
		else if (tint == NULL)
		{
			/* default */
			Pixel old_tint = cs->tint;
			colorset_color_release(cs, COLORSET_TINT);
			cs->tint = GetColor(black);
			if (old_tint != cs->tint)
			{
				have_pixels_changed = True;
			}
		}
	}

	/*
	 * reload the gradient if the tint or the alpha have changed.
	 * Do this too if we need to recompute the bg average and the
	 * gradient is tinted (perforemence issue).
	 */
	if ((has_tint_changed || has_image_alpha_changed ||
	     (has_bg_changed && (cs->color_flags & BG_AVERAGE) &&
	      cs->tint_percent > 0)) && cs->gradient_args)
	{
		do_reload_pixmap = True;
	}

	/*
	 * reset the pixmap if the tint or the alpha has changed
	 */
	if (!do_reload_pixmap &&
	    (has_tint_changed || has_image_alpha_changed  ||
	     (has_bg_changed && cs->alpha_pixmap != None)))
	{
		if (cs->pixmap_type == PIXMAP_ROOT_PIXMAP_PURE ||
		    cs->pixmap_type == PIXMAP_ROOT_PIXMAP_TRAN)
		{
			do_reload_pixmap = True;
		}
		else if (cs->picture != NULL && cs->pixmap)
		{
			XSetClipMask(dpy, gc, cs->picture->mask);
			reset_cs_pixmap(cs, gc);
			XSetClipMask(dpy, gc, None);
			has_pixmap_changed = True;
		}
	}

	/*
	 * (re)build the pixmap or the gradient
	 */
	if (do_reload_pixmap)
	{
		FvwmPicture *replacement = prepared;

		if (cs->pixmap_args != NULL && replacement == NULL)
		{
			FvwmPictureAttributes fpa = {0};

			fpa.mask = cs->dither ? FPAM_DITHER : 0;
			replacement = PCacheFvwmPicture(dpy, win, NULL,
			    cs->pixmap_args, fpa);
		}
		if (replacement != NULL && replacement->is_view &&
		    replacement->pending)
		{
			cs->pending_pixmap_type = requested_type;
			bind_colorset_image(n, cs, replacement, False);
			/* Tint/alpha still affect the retained old image.
			 * Start from its original pixels. */
			if (cs->picture != NULL && cs->pixmap != None &&
			    (has_tint_changed || has_image_alpha_changed ||
			     (has_bg_changed && cs->alpha_pixmap != None)))
			{
				XSetClipMask(dpy, gc, cs->picture->mask);
				reset_cs_pixmap(cs, gc);
				XSetClipMask(dpy, gc, None);
				has_pixmap_changed = True;
			}
		}
		else if (cs->preparing && cs->pixmap_args != NULL &&
		    replacement == NULL)
		{
			cs->preparation->refused = True;
			fvwm_debug(__func__, "can't load picture %s", cs->pixmap_args);
		}
		else
		{
			cancel_colorset_picture(&cs->pending_picture);
			free_colorset_background(cs, False);
			has_pixmap_changed = True;
			if (cs->pixmap_args != NULL)
			{
				cs->pixmap_type = requested_type;
				if (replacement == NULL)
				{
					fvwm_debug(__func__,
					    "can't load picture %s",
					    cs->pixmap_args);
				}
				parse_pixmap(win, gc, cs, replacement,
				    &pixmap_is_a_bitmap);
			}
			else if (cs->pixmap_type == PIXMAP_ROOT_PIXMAP_PURE ||
			    cs->pixmap_type == PIXMAP_ROOT_PIXMAP_TRAN)
			{
				cs->pixmap_type = 0;
				if (root_pic.pixmap)
				{
					cs->pixmap = root_pic.pixmap;
					cs->width = root_pic.width;
					cs->height = root_pic.height;
					cs->pixmap_type =
					    PIXMAP_ROOT_PIXMAP_PURE;
				}
			}
			else if (cs->gradient_args)
			{
				cs->pixmap = CreateGradientPixmapFromString(
				    dpy, win, gc, cs->gradient_type,
				    cs->gradient_args, &w, &h, &cs->pixels,
				    &cs->nalloc_pixels, cs->dither);
				cs->width = w;
				cs->height = h;
			}
		}
	}

	if (cs->picture != NULL && cs->picture->depth != Pdepth)
	{
		pixmap_is_a_bitmap = True;
	}

	/*
	 * ---------- change the background colour ----------
	 */
	if (has_bg_changed ||
	    (has_pixmap_changed && (cs->color_flags & BG_AVERAGE) &&
	     cs->pixmap != None &&
	     cs->pixmap != ParentRelative &&
	     !pixmap_is_a_bitmap))
	{
		Bool do_set_default_background = False;
		Pixmap average_pix = None;

		if (cs->color_flags & BG_AVERAGE)
		{
			if (cs->picture != NULL && cs->picture->picture != None)
			{
				average_pix = cs->picture->picture;
			}
			else if (cs->pixmap != ParentRelative)
			{
				average_pix = cs->pixmap;
			}

			if (average_pix == root_pic.pixmap)
			{
				int w2;
				int h2;
				XID dummy;

				MyXGrabServer(dpy);
				is_server_grabbed = True;
				if (!XGetGeometry(
				    dpy, average_pix, &dummy,
				    (int *)&dummy, (int *)&dummy,
				    (unsigned int *)&w2, (unsigned int *)&h2,
				    (unsigned int *)&dummy,
				    (unsigned int *)&dummy))
				{
					average_pix = None;
				}
				else
				{
					if (w2 != cs->width || h2 != cs->height)
					{
						average_pix = None;
					}
				}
				if (average_pix == None)
				{
					MyXUngrabServer(dpy);
					is_server_grabbed = False;
				}
			}
		}

		/* note: no average for bitmap */
		if ((cs->color_flags & BG_AVERAGE) && cs->picture != NULL &&
		    cs->picture->average_valid)
		{
			/* File-image summaries precede palette reduction/tint.
			 * Conversion already ignored alpha-zero RGB. */
			has_bg_changed = True;
			if (!cs->picture->average_has_pixels)
			{
				do_set_default_background = True;
			}
			else
			{
				Pixel old_bg = cs->bg;

				color.red = cs->picture->average_rgb[0];
				color.green = cs->picture->average_rgb[1];
				color.blue = cs->picture->average_rgb[2];
				colorset_color_release(cs, COLORSET_BG);
				PictureAllocColor(dpy, Pcmap, &color, True);
				cs->bg = color.pixel;
				if (old_bg != cs->bg)
				{
					have_pixels_changed = True;
				}
			}
		}
		else if ((cs->color_flags & BG_AVERAGE) && average_pix)
		{
			/* calculate average background color */
			XColor *colors;
			XImage *image;
			XImage *mask_image = None;
			unsigned int i2, j, k = 0;
			unsigned long red = 0, blue = 0, green = 0;
			unsigned long tred, tblue, tgreen;
			double dred = 0.0, dblue = 0.0, dgreen = 0.0;

			has_bg_changed = True;
			/* create an array to store all the pixmap colors in */
			/* Note: this may allocate a lot of memory:
			 * cs->width * cs->height * 12 and then the rest of the
			 * procedure can take a lot of times */
			colors = fxmalloc(
				cs->width * cs->height * sizeof(XColor));
			/* get the pixmap and mask into an image */
			image = XGetImage(
				dpy, average_pix, 0, 0, cs->width, cs->height,
				AllPlanes, ZPixmap);
			if (cs->mask != None)
			{
				mask_image = XGetImage(
					dpy, cs->mask, 0, 0, cs->width,
					cs->height, AllPlanes, ZPixmap);
			}
			if (is_server_grabbed == True)
			{
				MyXUngrabServer(dpy);
			}
			if (image != None && mask_image != None)
			{
				/* only fetch the pixels that are not masked
				 * out */
				for (i2 = 0; i2 < cs->width; i2++)
				{
					for (j = 0; j < cs->height; j++)
					{
						if (
							cs->mask == None ||
							XGetPixel(
								mask_image, i2,
								j) == 0)
						{
							colors[k++].pixel =
								XGetPixel(
									image,
									i2, j);
						}
					}
				}
			}
			if (image != None)
			{
				XDestroyImage(image);
			}
			if (mask_image != None)
			{
				XDestroyImage(mask_image);
			}
			if (k == 0)
			{
				do_set_default_background = True;
			}
			else
			{
				/* look them all up, XQueryColors() can't
				 * handle more than 256 */
				for (i = 0; i < k; i += 256)
				{
					XQueryColors(
						dpy, Pcmap, &colors[i],
						min(k - i, 256));
				}
				/* calculate average, add overflows in a double
				 * .red is short, red is long */
				for (i = 0; i < k; i++)
				{
					tred = red;
					red += colors[i].red;
					if (red < tred)
					{
						dred += (double)tred;
						red = colors[i].red;
					}
					tgreen = green;
					green += colors[i].green;
					if (green < tgreen)
					{
						dgreen += (double)tgreen;
						green = colors[i].green;
					}
					tblue = blue;
					blue += colors[i].blue;
					if (blue < tblue)
					{
						dblue += (double)tblue;
						blue = colors[i].blue;
					}
				}
				dred += red;
				dgreen += green;
				dblue += blue;
				/* get it */
				color.red = dred / k;
				color.green = dgreen / k;
				color.blue = dblue / k;
				{
					Pixel old_bg = cs->bg;

					colorset_color_release(cs, COLORSET_BG);
					PictureAllocColor(
						dpy, Pcmap, &color, True);
					cs->bg = color.pixel;
					if (old_bg != cs->bg)
					{
						have_pixels_changed = True;
					}
				}
			}
			free(colors);
		} /* average */
		else if ((cs->color_flags & BG_SUPPLIED) && bg != NULL)
		{
			/* user specified colour */
			Pixel old_bg = cs->bg;

			colorset_color_release(cs, COLORSET_BG);
			cs->bg = GetColor(bg);
			if (old_bg != cs->bg)
			{
				have_pixels_changed = True;
			}
		} /* user specified */
		else if (bg == NULL && has_bg_changed)
		{
			/* default */
			do_set_default_background = True;
		} /* default */
		if (do_set_default_background)
		{
			Pixel old_bg = cs->bg;

			colorset_color_release(cs, COLORSET_BG);
			cs->bg = GetColor(white);
			if (old_bg != cs->bg)
			{
				have_pixels_changed = True;
			}
			has_bg_changed = True;
		}

		if (has_bg_changed)
		{
			/* save the bg color for tinting */
			colorset_color_save(cs, COLORSET_BG, COLORSET_BG_SAVED);
		}
	} /* has_bg_changed */


	/*
	 * ---------- setup the bg tint colour ----------
	 */
	if (has_bg_tint_changed && cs->bg_tint_percent > 0 && bg_tint != NULL)
	{
		colorset_color_release(cs, COLORSET_BG_TINT);
		cs->bg_tint = GetColor(bg_tint);
	}

	/*
	 * ---------- tint the bg colour ----------
	 */
	if (has_bg_tint_changed || (has_bg_changed && cs->bg_tint_percent > 0))
	{
		if (cs->bg_tint_percent == 0)
		{
			Pixel old_bg = cs->bg;

			colorset_color_release(cs, COLORSET_BG);
			cs->bg = cs->bg_saved;
			cs->color_owners[COLORSET_BG] = picture_color_retain(
			    cs->color_owners[COLORSET_BG_SAVED]);
			if (old_bg != cs->bg)
			{
				have_pixels_changed = True;
				has_bg_changed = True;
			}
		}
		else
		{
			Pixel old_bg = cs->bg;

			colorset_color_release(cs, COLORSET_BG);
			cs->bg = GetTintedPixel(
				cs->bg_saved, cs->bg_tint, cs->bg_tint_percent);
			if (old_bg != cs->bg)
			{
				have_pixels_changed = True;
				has_bg_changed = True;
			}
		}
	}

	/*
	 * ---------- setup the fg tint colour ----------
	 */
	if (has_fg_tint_changed && cs->fg_tint_percent > 0 && fg_tint != NULL)
	{
		colorset_color_release(cs, COLORSET_FG_TINT);
		cs->fg_tint = GetColor(fg_tint);
	}

	/*
	 * ---------- change the foreground colour ----------
	 */
	if (has_fg_changed ||
	    (has_bg_changed && (cs->color_flags & FG_CONTRAST)))
	{
		if (cs->color_flags & FG_CONTRAST)
		{
			Pixel old_fg = cs->fg;

			/* calculate contrasting foreground color */
			color.pixel = cs->bg;
			XQueryColor(dpy, Pcmap, &color);
			color.red = (color.red > 32767) ? 0 : 65535;
			color.green = (color.green > 32767) ? 0 : 65535;
			color.blue = (color.blue > 32767) ? 0 : 65535;

			colorset_color_release(cs, COLORSET_FG);
			PictureAllocColor(dpy, Pcmap, &color, True);
			cs->fg = color.pixel;
			if (old_fg != cs->fg)
			{
				have_pixels_changed = True;
				has_fg_changed = 1;
			}
		} /* contrast */
		else if ((cs->color_flags & FG_SUPPLIED) && fg != NULL)
		{
			/* user specified colour */
			Pixel old_fg = cs->fg;

			colorset_color_release(cs, COLORSET_FG);
			cs->fg = GetColor(fg);
			if (old_fg != cs->fg)
			{
				have_pixels_changed = True;
				has_fg_changed = 1;
			}
		} /* user specified */
		else if (fg == NULL)
		{
			/* default */
			Pixel old_fg = cs->fg;

			colorset_color_release(cs, COLORSET_FG);
			cs->fg = GetColor(black);
			if (old_fg != cs->fg)
			{
				have_pixels_changed = True;
				has_fg_changed = 1;
			}
		}

		/* save the fg color for tinting */
		colorset_color_save(cs, COLORSET_FG, COLORSET_FG_SAVED);

	} /* has_fg_changed */

	/*
	 * ---------- tint the foreground colour ----------
	 */
	if (has_fg_tint_changed || (has_fg_changed && cs->fg_tint_percent > 0))
	{
		if (cs->fg_tint_percent == 0)
		{

			Pixel old_fg = cs->fg;

			colorset_color_release(cs, COLORSET_FG);
			cs->fg = cs->fg_saved;
			cs->color_owners[COLORSET_FG] = picture_color_retain(
			    cs->color_owners[COLORSET_FG_SAVED]);
			if (old_fg != cs->fg)
			{
				have_pixels_changed = True;
				has_fg_changed = 1;
			}
		}
		else
		{
			Pixel old_fg = cs->fg;

			colorset_color_release(cs, COLORSET_FG);
			cs->fg = GetTintedPixel(
				cs->fg_saved, cs->fg_tint,
				cs->fg_tint_percent);
			if (old_fg != cs->fg)
			{
				have_pixels_changed = True;
				has_fg_changed = 1;
			}
		}
	}


	/*
	 * ---------- change the hilight colour ----------
	 */
	if (has_hi_changed ||
	    (has_bg_changed && !(cs->color_flags & HI_SUPPLIED)))
	{
		has_hi_changed = 1;
		if ((cs->color_flags & HI_SUPPLIED) && hi != NULL)
		{
			/* user specified colour */
			Pixel old_hilite = cs->hilite;

			colorset_color_release(cs, COLORSET_HILITE);
			cs->hilite = GetColor(hi);
			if (old_hilite != cs->hilite)
			{
					have_pixels_changed = True;
			}
		} /* user specified */
		else if (hi == NULL)
		{
			Pixel old_hilite = cs->hilite;

			colorset_color_release(cs, COLORSET_HILITE);
			cs->hilite = GetHilite(cs->bg);
			if (old_hilite != cs->hilite)
			{
				have_pixels_changed = True;
			}
		}
	} /* has_hi_changed */

	/*
	 * ---------- change the shadow colour ----------
	 */
	if (has_sh_changed ||
	    (has_bg_changed && !(cs->color_flags & SH_SUPPLIED)))
	{
		has_sh_changed = 1;
		if ((cs->color_flags & SH_SUPPLIED) && sh != NULL)
		{
			/* user specified colour */
			Pixel old_shadow = cs->shadow;

			colorset_color_release(cs, COLORSET_SHADOW);
			cs->shadow = GetColor(sh);
			if (old_shadow != cs->shadow)
			{
				have_pixels_changed = True;
			}
		} /* user specified */
		else if (sh == NULL)
		{
			Pixel old_shadow = cs->shadow;

			colorset_color_release(cs, COLORSET_SHADOW);
			cs->shadow = GetShadow(cs->bg);
			if (old_shadow != cs->shadow)
			{
				have_pixels_changed = True;
			}
		}
	} /* has_sh_changed */

	/*
	 * ---------- change the shadow foreground colour ----------
	 */
	if (has_fgsh_changed ||
	    ((has_fg_changed || has_bg_changed) &&
	     !(cs->color_flags & FGSH_SUPPLIED)))
	{
		has_fgsh_changed = 1;
		if ((cs->color_flags & FGSH_SUPPLIED) && fgsh != NULL)
		{
			/* user specified colour */
			Pixel old_fgsh = cs->fgsh;

			colorset_color_release(cs, COLORSET_FGSH);
			cs->fgsh = GetColor(fgsh);
			if (old_fgsh != cs->fgsh)
			{
				have_pixels_changed = True;
			}
		} /* user specified */
		else if (fgsh == NULL)
		{
			Pixel old_fgsh = cs->fgsh;

			colorset_color_release(cs, COLORSET_FGSH);
			cs->fgsh = GetForeShadow(cs->fg, cs->bg);
			if (old_fgsh != cs->fgsh)
			{
				have_pixels_changed = True;
			}
		}
	} /* has_fgsh_changed */

	/*
	 * ------- the pixmap is a bitmap: create here cs->pixmap -------
	 */
	if (cs->picture != None && pixmap_is_a_bitmap &&
	    (has_pixmap_changed || has_bg_changed))
	{
		cs->pixmap = XCreatePixmap(
			dpy, win, cs->width, cs->height, Pdepth);
		XSetBackground(dpy, gc, cs->bg);
		XSetForeground(dpy, gc, cs->fg);
		reset_cs_pixmap(cs, gc);
	}

	if (colorset_uses_async_effects(cs) &&
	    (has_pixmap_changed || has_bg_changed || has_tint_changed ||
	    has_image_alpha_changed || cs->preparation->effect_needed))
	{
		colorset_prepare_effect(cs);
	}

	/*
	 * ------- change the masked out parts of the background pixmap -------
	 */
	if (!colorset_uses_async_effects(cs) &&
	    cs->pixmap != None && cs->pixmap != ParentRelative &&
	    (!CSETS_IS_TRANSPARENT_ROOT(cs)||
	     cs->allows_buffered_transparency) &&
	    (cs->mask != None || cs->alpha_pixmap != None ||
	     cs->image_alpha_percent < 100 || cs->tint_percent > 0) &&
	    (has_pixmap_changed || has_bg_changed || has_image_alpha_changed
	     || has_tint_changed))
	{
		/* Now that we know the background colour we can update the
		 * pixmap background. */
		FvwmRenderAttributes fra;
		Pixmap temp, mask, alpha;

		memset(&fra, 0, sizeof(fra));
		temp = XCreatePixmap(dpy, win, cs->width, cs->height, Pdepth);
		if (cs->picture != NULL)
		{
			mask = cs->picture->mask;
			alpha = cs->picture->alpha;
		}
		else
		{
			mask = None;
			alpha = None;
		}
		XSetForeground(dpy, gc, cs->bg);
		XFillRectangle(
			dpy, temp, gc, 0, 0, cs->width, cs->height);

		fra.mask = FRAM_HAVE_ADDED_ALPHA | FRAM_HAVE_TINT;
		fra.added_alpha_percent = cs->image_alpha_percent;
		fra.tint = cs->tint;
		fra.tint_percent = cs->tint_percent;
		PGraphicsRenderPixmaps(
			dpy, win, cs->pixmap, mask, alpha, Pdepth, &fra,
			temp, gc, Scr.MonoGC, Scr.AlphaGC,
			0, 0, cs->width, cs->height,
			0, 0, cs->width, cs->height, False);
		if (cs->pixmap != root_pic.pixmap)
		{
			add_to_junk(cs->pixmap);
		}
		cs->pixmap = temp;
		has_pixmap_changed = True;
		if (CSETS_IS_TRANSPARENT_ROOT(cs))
		{
			cs->pixmap_type = PIXMAP_ROOT_PIXMAP_TRAN;
		}
	} /* has_pixmap_changed */


	/*
	 * ---------- change the icon tint colour ----------
	 */
	if (has_icon_tint_changed)
	{
		/* user specified colour */
		if (icon_tint != NULL)
		{
			Pixel old_tint = cs->icon_tint;
			colorset_color_release(cs, COLORSET_ICON_TINT);
			cs->icon_tint = GetColor(icon_tint);
			if (old_tint != cs->icon_tint)
			{
				has_icon_pixels_changed = True;
			}
		}
		else
		{
			/* default */
			Pixel old_tint = cs->icon_tint;
			colorset_color_release(cs, COLORSET_ICON_TINT);
			cs->icon_tint = GetColor(black);
			if (old_tint != cs->icon_tint)
			{
				has_icon_pixels_changed = True;
			}
		}
	}

	/*
	 * ---------- send new colorset to fvwm and clean up ----------
	 */
	/* make sure the server has this to avoid races */
	if (prepared == NULL && !(colorset_uses_async_effects(cs) &&
	    cs->preparation->effect != NULL))
	{
		XSync(dpy, False);
	}

	/* inform modules of the change */
	if (!cs->preparing &&
	    (have_pixels_changed || has_pixmap_changed || has_shape_changed ||
	    has_fg_alpha_changed || has_icon_pixels_changed))
	{
		BroadcastColorset(n);
	}

	if (fg)
	{
		free(fg);
	}
	if (bg)
	{
		free(bg);
	}
	if (hi)
	{
		free(hi);
	}
	if (sh)
	{
		free(sh);
	}
	if (fgsh)
	{
		free(fgsh);
	}
	if (tint)
	{
		free(tint);
	}
	if (fg_tint)
	{
		free(fg_tint);
	}
	if (bg_tint)
	{
		free(bg_tint);
	}
	if (icon_tint)
	{
		free(icon_tint);
	}
	if (cs->preparing)
	{
		colorset_background_capture(cs);
		colorset_preparation_publish(cs->preparation);
	}
	return;
}

void parse_colorset(int n, char *line)
{
	parse_colorset_update(n, line, NULL, 0);
}

/* Allocate only the requested ID. Existing records remain stable and
 * refusal happens before any X resources or caller assignments are changed. */
Bool alloc_colorset(int n)
{
	colorset_t *ncs;

	if (n < 0)
	{
		/* Negative style IDs disable colorset use. */
		return True;
	}
	if (FindColorset(n) != NULL)
	{
		return True;
	}
	if (n != 0 && FindColorset(0) == NULL && !alloc_colorset(0))
	{
		return False;
	}
	ncs = CreateColorset(n);
	if (ncs == NULL)
	{
		return False;
	}
	if (n == 0)
	{
		update_root_pixmap(0);
	}
	if (PictureUseBWOnly())
	{
		char g_bits[] = {0x0a, 0x05, 0x0a, 0x05,
				 0x08, 0x02, 0x08, 0x02,
				 0x01, 0x02, 0x04, 0x08};
		/* monochrome monitors get black on white */
		/* with a gray pixmap background */
		ncs->fg = GetColor(black);
		ncs->bg = GetColor(white);
		ncs->hilite = GetColor(white);
		ncs->shadow = GetColor(black);
		ncs->fgsh = GetColor(white);
		ncs->tint = GetColor(black);
		ncs->icon_tint = GetColor(black);
		ncs->pixmap = XCreatePixmapFromBitmapData(
			dpy, Scr.NoFocusWin,
			&g_bits[4 * (n % 3)], 4, 4,
			PictureBlackPixel(), PictureWhitePixel(),
			Pdepth);
		ncs->width = 4;
		ncs->height = 4;
	}
	else
	{
		ncs->fg = GetColor(black);
		ncs->bg = GetColor(gray);
		ncs->hilite = GetHilite(ncs->bg);
		ncs->shadow = GetShadow(ncs->bg);
		ncs->fgsh = GetForeShadow(ncs->fg, ncs->bg);
		ncs->tint = GetColor(black);
		ncs->icon_tint = GetColor(black);
	}
	ncs->fg_tint = GetColor(black);
	ncs->bg_tint = GetColor(black);
	/* set flags for fg contrast, bg average */
	/* in case just a pixmap is given */
	ncs->color_flags = FG_CONTRAST | BG_AVERAGE;
	colorset_color_save(ncs, COLORSET_FG, COLORSET_FG_SAVED);
	/* Preserve the legacy initial saved background value of zero, but
	 * never mistake that unallocated sentinel for an owned color. */
	ncs->color_owners[COLORSET_BG_SAVED] = picture_color_borrow(Pcmap,
	    ncs->bg_saved);
	ncs->fg_alpha_percent = 100;
	ncs->image_alpha_percent = 100;
	ncs->icon_alpha_percent = 100;
	ncs->tint_percent = 0;
	ncs->icon_tint_percent = 0;
	ncs->fg_tint_percent = ncs->bg_tint_percent = 0;
	ncs->dither = (PictureDitherByDefault())? True:False;
	colorset_query_changed();
	return True;
}

void update_root_transparent_colorset(Atom prop)
{
	int i;
	colorset_t *cs;

	root_pic.old_pixmap = root_pic.pixmap;
	update_root_pixmap(prop);
#if 0
	if (!root_pic.pixmap)
	{
		return;
	}
#endif
	for (i = 0; i < ColorsetCount(); i++)
	{
		Bool root_trans = False;
		int id = ColorsetIdAt(i);
		cs = GetColorset(id);
		if (cs->preparation != NULL)
		{
			cs = &cs->preparation->requested;
		}
		if (cs->is_maybe_root_transparent &&
		    cs->allows_buffered_transparency)
		{
			parse_colorset(id, "RootTransparent buffer");
			root_trans = True;
		}
		else if (cs->is_maybe_root_transparent)
		{
			parse_colorset(id, "RootTransparent");
			root_trans = True;
		}
		if (root_trans && !image_async_enabled())
		{
			update_fvwm_colorset(id);
		}
	}
}

/* ---------------------------- builtin commands ---------------------------- */

void CMD_ReadWriteColors(F_CMD_ARGS)
{
	fvwm_debug(__func__, "ReadWriteColors is obsolete");

	return;
}

void CMD_Colorset(F_CMD_ARGS)
{
	int n;
	char *token;

	if (GetColorsetArgument(action, &token, &n) != 1)
	{
		return;
	}
	if (n < 0)
	{
		return;
	}
	if (token == NULL)
	{
		return;
	}
	if (!alloc_colorset(n))
	{
		return;
	}
	parse_colorset(n, token);
	if (!image_async_enabled())
	{
		update_fvwm_colorset(n);
	}

	return;
}

void CMD_CleanupColorsets(F_CMD_ARGS)
{
	cleanup_colorsets();
}
