/* -*-c-*- */
/* Fvwm colorset technology is Copyright (C) 1999 Joey Shutup
 * http://www.streetmap.co.uk/streetmap.dll?Postcode2Map?BS24+9TZ
 * You may use this code for any purpose, as long as the original copyright
 * and this notice remains in the source code and all documentation
 */
/* This program is free software; you can redistribute it and/or modify
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

#include "config.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <X11/Intrinsic.h>

#include "libs/fvwmlib.h"
#include "libs/FShape.h"
#include "libs/Colorset.h"
#include "libs/Parse.h"
#include "libs/log.h"
#include "libs/PictureBase.h"
#include "libs/Graphics.h"
#include "libs/Grab.h"
#include "libs/PictureGraphics.h"
#include "libs/XError.h"

#include "colorset_store.h"
#include <limits.h>
#include <stdbool.h>

static colorset_store colorsets = {
	.limit = FVWM_COLORSET_LIMIT, .record_size = sizeof(colorset_t)
};
static colorset_t empty_colorset;

colorset_t *FindColorset(int id)
{
	return colorset_store_find(&colorsets, id);
}

colorset_t *GetColorset(int id)
{
	/* Consecutive field reads usually share an ID. Cache only exact hits;
	 * sparse records are never moved or deleted during process lifetime. */
	static int cached_id = -1;
	static colorset_t *cached;
	colorset_t *cs;

	if (cached != NULL && cached_id == id)
	{
		return cached;
	}
	cs = FindColorset(id);
	if (cs != NULL)
	{
		cached_id = id;
		cached = cs;
		return cs;
	}
	cs = FindColorset(0);
	return cs != NULL ? cs : &empty_colorset;
}

colorset_t *CreateColorset(int id)
{
	bool created;
	colorset_t *cs = colorset_store_ensure(&colorsets, id, &created);
	if (cs == NULL)
	{
		fvwm_debug(__func__, "Cannot allocate colorset %d (limit %d)",
			id, FVWM_COLORSET_LIMIT);
	}
	return cs;
}

size_t ColorsetCount(void)
{
	return colorsets.count;
}

int ColorsetIdAt(size_t position)
{
	return colorset_store_id_at(&colorsets, position);
}

Bool ParseColorsetId(const char *text, char **end, int *id)
{
	return colorset_store_parse_id(text, 10, id, end);
}

int GetColorsetArgument(char *action, char **rest, int *id)
{
	char *token = NULL, *tail;
	int parsed;
	char *next = GetNextToken(action, &token);
	Bool valid = ParseColorsetId(token, &tail, &parsed) && *tail == '\0';

	if (rest != NULL)
	{
		*rest = next;
	}
	if (valid)
	{
		*id = parsed;
	}
	free(token);
	return valid ? 1 : 0;
}

/* stretch the src rectangle to the dest ractangle keeping its aspect so that
 * it fills the destination completely. */
static int get_aspect_dimensions(
	int *ret_w, int *ret_h, int dest_w, int dest_h, int src_w, int src_h)
{
	double ax;
	double ay;

	ax = (double)dest_w / (double)src_w;
	ay = (double)dest_h / (double)src_h;
	if (ax >= ay)
	{
		/* fit in x direction */
		*ret_w = dest_w;
		*ret_h = (src_h * dest_w) / src_w;
		return 0;
	}
	else
	{
		/* fit in y direction */
		*ret_w = (src_w * dest_h) / src_h;
		*ret_h = dest_h;
		return 1;
	}
}

/* Modules borrow resource IDs from the core and copy set zero as their
 * undefined-set default. No intervening IDs are allocated. */
Bool AllocColorset(int n)
{
	if (n < 0)
	{
		return False;
	}
	if (FindColorset(n) != NULL)
	{
		return True;
	}
	colorset_t *zero = FindColorset(0);
	if (zero == NULL)
	{
		zero = CreateColorset(0);
		if (zero == NULL)
		{
			return False;
		}
	}
	colorset_t *cs = n == 0 ? zero : CreateColorset(n);
	if (cs == NULL)
	{
		return False;
	}
	if (n != 0)
	{
		*cs = *zero;
	}
	return True;
}

/*
 * DumpColorset() returns a char * to the colorset contents in printable form
 */
static char csetbuf[270];
char *DumpColorset(int n, colorset_t *cs)
{
	snprintf(csetbuf, sizeof(csetbuf),
		"Colorset "
		"%x %lx %lx %lx %lx %lx %lx %lx %lx %lx "
		"%x %x %x %x %x %x %x %x %x %x %x",
		n, cs->fg, cs->bg, cs->hilite, cs->shadow, cs->fgsh, cs->tint,
		cs->icon_tint, cs->pixmap, cs->shape_mask,
		cs->fg_alpha_percent, cs->width, cs->height, cs->pixmap_type,
		cs->shape_width, cs->shape_height, cs->shape_type,
		cs->tint_percent, cs->do_dither_icon, cs->icon_tint_percent,
		cs->icon_alpha_percent);
	return csetbuf;
}

/*
 * LoadColorset() takes a strings and stuffs it into the array
 */
int LoadColorset(char *line)
{
	colorset_t *cs;
	int n;
	char *tail;
	unsigned long values[20];

	if (line == NULL)
	{
		return -1;
	}
	if (!colorset_store_parse_id(line, 16, &n, &tail) || n < 0)
	{
		return -1;
	}
	line = tail;

	/* Preserve DumpColorset's 20-field order and native resource widths.
	 * The first nine fields are pixels/pixmaps; the rest are unsigned
	 * integer attributes. Keep Perl metadata in sync if this order changes.
	 * Unlike scanf, strtoul reports overflow before a value can narrow. */
	for (unsigned i = 0; i < 20; i++)
	{
		while (isspace((unsigned char)*line))
		{
			line++;
		}
		if (*line == '-')
		{
			return -1;
		}
		errno = 0;
		values[i] = strtoul(line, &tail, 16);
		if (tail == line || errno == ERANGE ||
		    (i >= 9 && values[i] > UINT_MAX) ||
		    (*tail != '\0' && !isspace((unsigned char)*tail)))
		{
			return -1;
		}
		line = tail;
	}
	/* Refuse invalid X11 extents before creating an ID/changing state. */
	if (values[10] > UINT16_MAX || values[11] > UINT16_MAX ||
	    values[13] > UINT16_MAX || values[14] > UINT16_MAX)
	{
		return -1;
	}
	if (!AllocColorset(n))
	{
		return -1;
	}
	cs = GetColorset(n);
	cs->fg = values[0];
	cs->bg = values[1];
	cs->hilite = values[2];
	cs->shadow = values[3];
	cs->fgsh = values[4];
	cs->tint = values[5];
	cs->icon_tint = values[6];
	cs->pixmap = values[7];
	cs->shape_mask = values[8];
	cs->fg_alpha_percent = values[9];
	cs->width = values[10];
	cs->height = values[11];
	cs->pixmap_type = values[12];
	cs->shape_width = values[13];
	cs->shape_height = values[14];
	cs->shape_type = values[15];
	cs->tint_percent = values[16];
	cs->do_dither_icon = values[17];
	cs->icon_tint_percent = values[18];
	cs->icon_alpha_percent = values[19];

	return n;
}

/* scrolls a pixmap by x_off/y_off pixels, wrapping around at the edges. */
Pixmap ScrollPixmap(
	Display *dpy, Pixmap p, GC gc, int x_off, int y_off, int width,
	int height, unsigned int depth)
{
	GC tgc;
	XGCValues xgcv;
	Pixmap p2;

	if (p == None || p == ParentRelative || (x_off == 0 && y_off == 0))
	{
		return p;
	}
	tgc = fvwmlib_XCreateGC(dpy, p, 0, &xgcv);
	if (tgc == None)
	{
		return p;
	}
	XCopyGC(dpy, gc, GCFunction | GCPlaneMask| GCSubwindowMode |
		GCClipXOrigin | GCClipYOrigin | GCClipMask, tgc);
	xgcv.tile = p;
	xgcv.ts_x_origin = x_off;
	xgcv.ts_y_origin = y_off;
	xgcv.fill_style = FillTiled;
	XChangeGC(
		dpy, tgc, GCTile | GCTileStipXOrigin | GCTileStipYOrigin |
		GCFillStyle, &xgcv);
	p2 = XCreatePixmap(dpy, p, width, height, depth);
	if (p2 == None)
	{
		return p;
	}
	XFillRectangle(dpy, p2, tgc, 0, 0, width, height);
	XFreeGC(dpy, tgc);

	return p2;
}

/* sets a window background from a colorset
 * if width or height are zero the window size is queried
 */
void SetWindowBackgroundWithOffset(
	Display *dpy, Window win, int x_off, int y_off, unsigned int width, unsigned int height,
	colorset_t *colorset, unsigned int depth, GC gc, Bool clear_area)
{
	Pixmap pixmap = None;
	Pixmap mask = None;
	union {
		XID junk;
		unsigned int ui_junk;
		int i_junk;
	} XID_int;
	if (0 == width || 0 == height)
	{
		if (!XGetGeometry(
			    dpy, win, &XID_int.junk,
			    &XID_int.i_junk, &XID_int.i_junk,
			    (unsigned int *)&width,
			    (unsigned int *)&height,
			    &XID_int.ui_junk,
			    &XID_int.ui_junk))
		{
			return;
		}
	}

	if (FHaveShapeExtension && colorset->shape_mask)
	{
		mask = CreateBackgroundPixmap(
			dpy, None, width, height, colorset, 1, None, True);
		if (mask != None)
		{
			FShapeCombineMask(
				dpy, win, FShapeBounding, 0, 0, mask,
				FShapeSet);
			XFreePixmap(dpy, mask);
		}
	}
	if (!colorset->pixmap)
	{
		/* use the bg pixel */
		XSetWindowBackground(dpy, win, colorset->bg);
		if (clear_area)
		{
			XClearArea(dpy, win, 0, 0, width, height, True);
		}
	}
	else
	{

		pixmap = CreateOffsetBackgroundPixmap(
			dpy, win, x_off, y_off, width, height, colorset,
			depth, gc, False);
		if (pixmap)
		{
			XSetWindowBackgroundPixmap(dpy, win, pixmap);
			if (clear_area)
			{
				XClearArea(dpy, win, 0, 0, width, height, True);
			}
			if (pixmap != ParentRelative)
			{
				XFreePixmap(dpy, pixmap);
			}
		}
	}

	return;
}

Bool UpdateBackgroundTransparency(
	Display *dpy, Window win, int width, int height,
	colorset_t *colorset, unsigned int depth, GC gc, Bool clear_area)
{
	if (!CSETS_IS_TRANSPARENT(colorset))
	{
		return False;
	}
	else if (!CSETS_IS_TRANSPARENT_PR_PURE(colorset))
	{
		SetWindowBackgroundWithOffset(
			dpy, win, 0, 0, width, height, colorset, depth, gc,
			True);
	}
	else
	{
		XClearArea(dpy, win, 0,0,0,0, clear_area);
	}

	return True;
}

void SetWindowBackground(
	Display *dpy, Window win, int width, int height,
	colorset_t *colorset, unsigned int depth, GC gc, Bool clear_area)
{
	SetWindowBackgroundWithOffset(
		dpy, win, 0, 0, width, height, colorset, depth, gc, clear_area);

	return;
}

void GetWindowBackgroundPixmapSize(
	colorset_t *cs_t, int width, int height, int *w, int *h)
{
	if (cs_t->pixmap == None)
	{
		*w = *h = 1;
	}
	else
	{
		*w = cs_t->width;
		*h = cs_t->height;
		switch (cs_t->pixmap_type)
		{
		case PIXMAP_STRETCH_ASPECT:
			get_aspect_dimensions(
				w, h, width, height, cs_t->width, cs_t->height);
			break;
		case PIXMAP_STRETCH_X:
			*w = width;
			break;
		case PIXMAP_STRETCH_Y:
			*h = height;
			break;
		default:
			break;
		}
	}
}

static int is_bad_gc = 0;
static int BadGCErrorHandler(Display *dpy, XErrorEvent *error)
{
	if (error->error_code == BadGC)
	{
		is_bad_gc = 1;

		return 0;
	}
	else
	{
		int rc;

		/* delegate error to original handler */
		rc = ferror_call_next_error_handler(dpy, error);

		return rc;
	}
}

/* create a pixmap suitable for plonking on the background of a part of a
 * window */
Pixmap CreateOffsetBackgroundPixmap(
	Display *dpy, Window win, int x, int y, int width, int height,
	colorset_t *colorset, unsigned int depth, GC gc, Bool is_shape_mask)
{
	Pixmap pixmap = None;
	Pixmap cs_pixmap = None;
	XGCValues xgcv;
	static GC shape_gc = None;
	GC fill_gc = None; /* not static as dpy may change (FvwmBacker) */
	int cs_width;
	int cs_height;
	Bool cs_keep_aspect;
	Bool cs_stretch_x;
	Bool cs_stretch_y;

	if (colorset->pixmap == ParentRelative && !is_shape_mask &&
	    colorset->tint_percent > 0)
	{
		FvwmRenderAttributes fra;

		fra.mask = FRAM_DEST_IS_A_WINDOW | FRAM_HAVE_TINT;
		fra.tint = colorset->tint;
		fra.tint_percent = colorset->tint_percent;
		MyXGrabServer(dpy);
		pixmap = PGraphicsCreateTransparency(
			dpy, win, &fra, gc, x, y, width, height, True);
		MyXUngrabServer(dpy);
		if (pixmap == None)
		{
			return ParentRelative;
		}
		return pixmap;
	}
	else if (colorset->pixmap == ParentRelative && !is_shape_mask)
	{
		return ParentRelative;
	}
	else if (CSETS_IS_TRANSPARENT_ROOT(colorset) && colorset->pixmap
		 && !is_shape_mask)
	{
		int sx,sy;
		int h,w;
		XID dummy;

		cs_pixmap = colorset->pixmap;
		cs_width = colorset->width;
		cs_height = colorset->height;
		if (CSETS_IS_TRANSPARENT_ROOT_PURE(colorset))
		{
			/* check if it is still here */
			union {
				XID junk;
				unsigned int ui_junk;
				int i_junk;
			} XID_int;
			/* a priori we should grab the server, but this
			 * cause PositiveWrite error when you move a
			 * window with a transparent title bar */
			if (!XGetGeometry(
				    dpy, colorset->pixmap, &XID_int.junk,
				    &XID_int.i_junk, &XID_int.i_junk,
				    (unsigned int *)&w, (unsigned int *)&h,
				    &XID_int.ui_junk,
				    &XID_int.ui_junk) ||
			    w != cs_width || h != cs_height)
			{
				return None;
			}
		}
		XTranslateCoordinates(
			dpy, win, DefaultRootWindow(dpy), x, y, &sx, &sy,
			&dummy);
		pixmap = XCreatePixmap(dpy, win, width, height, Pdepth);
		if (!pixmap)
		{
			return None;
		}
		/* make sx and sy positif */
		while (sx < 0)
		{
			sx = sx + cs_width;
		}
		while (sy < 0)
		{
			sy = sy + cs_height;
		}
		/* make sx and sy in (0,0,cs_width,cs_height) */
		while (sx >= cs_width)
		{
			sx = sx - cs_width;
		}
		while (sy >= cs_height)
		{
			sy = sy - cs_height;
		}
		xgcv.fill_style = FillTiled;
		xgcv.tile = cs_pixmap;
		xgcv.ts_x_origin = cs_width-sx;
		xgcv.ts_y_origin = cs_height-sy;
		fill_gc = fvwmlib_XCreateGC(
			dpy, win, GCTile | GCTileStipXOrigin |
			GCTileStipYOrigin | GCFillStyle, &xgcv);
		if (fill_gc == None)
		{
			XFreePixmap(dpy, pixmap);
			return None;
		}
		XSync(dpy, False);
		is_bad_gc = 0;
		ferror_set_temp_error_handler(BadGCErrorHandler);
		XFillRectangle(dpy, pixmap, fill_gc, 0, 0, width, height);
		if (
			is_bad_gc == 0 &&
			CSETS_IS_TRANSPARENT_ROOT_PURE(colorset) &&
			colorset->tint_percent > 0)
		{
			FvwmRenderAttributes fra;

			fra.mask = FRAM_HAVE_TINT;
			fra.tint = colorset->tint;
			fra.tint_percent = colorset->tint_percent;
			PGraphicsRenderPixmaps(
				dpy, win, pixmap, None, None,
				Pdepth, &fra, pixmap,
				fill_gc, None, None,
				0, 0, width, height,
				0, 0, width, height, False);
		}
		XSync(dpy, False);
		if (is_bad_gc == 1)
		{
			is_bad_gc = 0;
			XFreePixmap(dpy, pixmap);
			pixmap = None;
		}
		XFreeGC(dpy,fill_gc);
		/* If a replaced root pixmap prevented GC creation, releasing that
		 * GC also produces BadGC. Drain cleanup before restoring the caller's
		 * handler so the expected transient error stays within this scope. */
		XSync(dpy, False);
		ferror_reset_temp_error_handler();
		is_bad_gc = 0;
		return pixmap;
	}
	if (!is_shape_mask)
	{
		cs_pixmap = colorset->pixmap;
		cs_width = colorset->width;
		cs_height = colorset->height;
		cs_keep_aspect =
			(colorset->pixmap_type == PIXMAP_STRETCH_ASPECT);
		cs_stretch_x = (colorset->pixmap_type == PIXMAP_STRETCH_X)
			|| (colorset->pixmap_type == PIXMAP_STRETCH);
		cs_stretch_y = (colorset->pixmap_type == PIXMAP_STRETCH_Y)
			|| (colorset->pixmap_type == PIXMAP_STRETCH);
	}
	else
	{
		/* In spite of the name, win contains the pixmap */
		cs_pixmap = colorset->shape_mask;
		win = colorset->shape_mask;
		if (shape_gc == None)
		{
			xgcv.foreground = 1;
			xgcv.background = 0;
			/* create a gc for 1 bit depth */
			shape_gc = fvwmlib_XCreateGC(
				dpy, win, GCForeground|GCBackground, &xgcv);
		}
		gc = shape_gc;
		cs_width = colorset->shape_width;
		cs_height = colorset->shape_height;
		cs_keep_aspect = (colorset->shape_type == SHAPE_STRETCH_ASPECT);
		cs_stretch_x = !(colorset->shape_type == SHAPE_TILED);
		cs_stretch_y = !(colorset->shape_type == SHAPE_TILED);
	}

	if (cs_pixmap == None)
	{
		xgcv.foreground = colorset->bg;
		fill_gc = fvwmlib_XCreateGC(dpy, win, GCForeground, &xgcv);
		/* create a solid pixmap - not very useful most of the time */
		pixmap = XCreatePixmap(dpy, win, 1, 1, depth);
		XFillRectangle(dpy, pixmap, fill_gc, 0, 0, 1, 1);
		XFreeGC(dpy,fill_gc);
	}
	else if (cs_keep_aspect)
	{
		Bool trim_side;
		int big_width, big_height;
		Pixmap big_pixmap;
		int px, py;

		/* make a pixmap big enough to cover the destination but with
		 * the aspect ratio of the cs_pixmap */
		trim_side = get_aspect_dimensions(
			&big_width, &big_height, width, height, cs_width,
			cs_height);
		big_pixmap = CreateStretchPixmap(
			dpy, cs_pixmap, cs_width, cs_height, depth, big_width,
			big_height, gc);

		/* work out where to trim */
		px = trim_side ? (big_width - width) / 2 : 0;
		py = trim_side ? 0 : (big_height - height) / 2;

		pixmap = XCreatePixmap(dpy, cs_pixmap, width, height, depth);
		if (pixmap && big_pixmap)
		{
			XCopyArea(
				dpy, big_pixmap, pixmap, gc, px, py, width,
				height, 0, 0);
		}
		if (big_pixmap)
		{
			XFreePixmap(dpy, big_pixmap);
		}
	}
	else if (!cs_stretch_x && !cs_stretch_y)
	{
		/* it's a tiled pixmap, create an unstretched one */
		if (!is_shape_mask)
		{
			pixmap = XCreatePixmap(
				dpy, cs_pixmap, cs_width, cs_height, depth);
			if (pixmap)
			{
				XCopyArea(
					dpy, cs_pixmap, pixmap, gc, 0, 0,
					cs_width, cs_height, 0, 0);
			}
		}
		else
		{
			/* can't tile masks, create a tiled version of the
			 * mask */
			pixmap = CreateTiledPixmap(
				dpy, cs_pixmap, cs_width, cs_height, width,
				height, 1, gc);
		}
	}
	else if (!cs_stretch_x)
	{
		/* it's an HGradient */
		pixmap = CreateStretchYPixmap(
			dpy, cs_pixmap, cs_width, cs_height, depth, height, gc);
	}
	else if (!cs_stretch_y)
	{
		/* it's a VGradient */
		pixmap = CreateStretchXPixmap(
			dpy, cs_pixmap, cs_width, cs_height, depth, width, gc);
	}
	else
	{
		/* It's a full window pixmap */
		pixmap = CreateStretchPixmap(
			dpy, cs_pixmap, cs_width, cs_height, depth, width,
			height, gc);
	}

	if (x != 0 || y != 0)
	{
		Pixmap p2;

		p2 = ScrollPixmap(
			dpy, pixmap, gc, x, y, width, height,
			depth);
		if (p2 != None && p2 != ParentRelative && p2 != pixmap)
		{
			XFreePixmap(dpy, pixmap);
			pixmap = p2;
		}
	}

	return pixmap;
}

/* create a pixmap suitable for plonking on the background of a window */
Pixmap CreateBackgroundPixmap(Display *dpy, Window win, int width, int height,
			      colorset_t *colorset, unsigned int depth,
			      GC gc, Bool is_shape_mask)
{
	return CreateOffsetBackgroundPixmap(
		dpy, win, 0, 0, width, height, colorset, depth, gc,
		is_shape_mask);
}

/* Draws a colorset background into the specified rectangle in the target
 * drawable. */
void SetRectangleBackground(
	Display *dpy, Window win, int x, int y, int width, int height,
	colorset_t *colorset, unsigned int depth, GC gc)
{
	SetClippedRectangleBackground(
		dpy, win, x, y, width, height, NULL, colorset, depth, gc);

	return;
}

/* Draws a colorset background into the specified rectangle in the target
 * drawable. */
void SetClippedRectangleBackground(
	Display *dpy, Window win, int x, int y, int width, int height,
	XRectangle *clip, colorset_t *colorset,
	unsigned int depth, GC gc)
{
	GC draw_gc;
	Pixmap pixmap2;
	Pixmap pixmap = None;
	static int last_depth = -1;
	static GC last_gc = None;
	XGCValues xgcv;
	Pixmap clipmask = None;
	GC clip_gc = None;
	Bool keep_aspect = (colorset->pixmap_type == PIXMAP_STRETCH_ASPECT);
	Bool stretch_x = (colorset->pixmap_type == PIXMAP_STRETCH_X)
		|| (colorset->pixmap_type == PIXMAP_STRETCH);
	Bool stretch_y = (colorset->pixmap_type == PIXMAP_STRETCH_Y)
		|| (colorset->pixmap_type == PIXMAP_STRETCH);
	int dest_x, dest_y, dest_w, dest_h;

	if (clip)
	{
		dest_x = clip->x;
		dest_y = clip->y;
		dest_w = clip->width;
		dest_h = clip->height;
	}
	else
	{
		dest_x = x;
		dest_y = y;
		dest_w = width;
		dest_h = height;
	}
	if (CSETS_IS_TRANSPARENT_PR_TINT(colorset))
	{
		XClearArea(dpy, win, dest_x, dest_y, dest_w, dest_h, False);
		PGraphicsTintRectangle(
			dpy, win, colorset->tint, colorset->tint_percent,
			win, True, gc, None, None,
			dest_x, dest_y, dest_w, dest_h);
		return;
	}
	if (CSETS_IS_TRANSPARENT_PR_PURE(colorset))
	{
		XClearArea(dpy, win, dest_x, dest_y, dest_w, dest_h, False);
		/* don't do anything */
		return;
	}
	if (CSETS_IS_TRANSPARENT_ROOT(colorset))
	{
		/* FIXME: optimize this ! */
		x = y = 0;
		width = width + dest_x;
		height = height + dest_y;
	}
	/* minimize gc creation by remembering the last requested depth */
	if (last_gc != None && depth != last_depth)
	{
		XFreeGC(dpy, last_gc);
		last_gc = None;
	}
	if (last_gc == None)
	{
		last_gc = fvwmlib_XCreateGC(dpy, win, 0, &xgcv);
	}
	draw_gc = last_gc;
	last_depth = depth;

	if (FHaveShapeExtension && colorset->shape_mask != None)
	{
		clipmask = CreateBackgroundPixmap(
			dpy, 0, width, height, colorset, 1, None, True);
		if (clipmask)
		{
			/* create a GC for clipping */
			xgcv.clip_x_origin = x;
			xgcv.clip_y_origin = y;
			xgcv.clip_mask = clipmask;
			clip_gc = fvwmlib_XCreateGC(
				dpy, win, GCClipXOrigin | GCClipYOrigin |
				GCClipMask, &xgcv);
			draw_gc = clip_gc;
		}
	}

	if (!colorset->pixmap)
	{
		/* use the bg pixel */
		XSetForeground(dpy, draw_gc, colorset->bg);
		XFillRectangle(
			dpy, win, draw_gc, dest_x, dest_y, dest_w, dest_h);
	}
	else
	{
		pixmap = CreateBackgroundPixmap(
			dpy, win, width, height, colorset, depth, gc, False);
		if (keep_aspect)
		{
			/* nothing to do */
		}
		if (stretch_x || stretch_y)
		{
			if (!stretch_x && colorset->width != width)
			{
				pixmap2 = CreateStretchXPixmap(
					dpy, pixmap, colorset->width, height,
					depth, width, gc);
				XFreePixmap(dpy, pixmap);
				pixmap = pixmap2;
			}
			if (!stretch_y && colorset->height != height)
			{
				pixmap2 = CreateStretchYPixmap(
					dpy, pixmap, width, colorset->height,
					depth, height, gc);
				XFreePixmap(dpy, pixmap);
				pixmap = pixmap2;
			}
		}
		else
		{
			pixmap2 = CreateTiledPixmap(
				dpy, pixmap, colorset->width, colorset->height,
				width, height, depth, gc);
			XFreePixmap(dpy, pixmap);
			pixmap = pixmap2;
		}

		if (pixmap)
		{
			/* Copy the pixmap into the rectangle. */
			XCopyArea(
				dpy, pixmap, win, draw_gc,
				dest_x - x, dest_y - y, dest_w, dest_h,
				dest_x, dest_y);
			XFreePixmap(dpy, pixmap);
		}
	}

	if (FHaveShapeExtension)
	{
		if (clipmask != None)
		{
			XFreePixmap(dpy, clipmask);
		}
		if (clip_gc != None)
		{
			XFreeGC(dpy, clip_gc);
		}
	}

	return;
}
