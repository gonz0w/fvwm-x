/* -*-c-*- */
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
/*
  Changed 02/12/97 by Dan Espen:
  - added routines to determine color closeness, for color use reduction.
  Some of the logic comes from pixy2, so the copyright is below.
*/

/*
 *
 * Routines to handle initialization, loading, and removing of xpm's or mono-
 * icon images.
 *
 */

#include "config.h"
#include "picture_color.h"
#include "picture_cache_name.h"
#include <stdio.h>
#include <signal.h>
#include <ctype.h>

#include "ftime.h"

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Intrinsic.h>

#include "fvwmlib.h"
#include "System.h"
#include "Colorset.h"
#include "Picture.h"
#include "picture_types.h"
#include "PictureUtils.h"
#include "Fsvg.h"
#include "log.h"
#include "libs/wm_ui_metrics.h"

static FvwmPicture *FvwmPictureList=NULL;
static picture_loader_fn picture_loader;
static void *picture_loader_context;

void picture_set_loader(picture_loader_fn loader, void *context)
{
	picture_loader = loader;
	picture_loader_context = context;
}

void picture_cache_publish(FvwmPicture *picture)
{
	/* Complete both links without callbacks before publishing the head. */
	picture->next = FvwmPictureList;
	picture->cache_link = &FvwmPictureList;
	picture->cache_name_hash = picture->name != NULL ?
		picture_cache_name_hash(picture->name) : 0;
	if (picture->next != NULL)
	{
		picture->next->cache_link = &picture->next;
	}
	FvwmPictureList = picture;
}

static void picture_cache_unlink(FvwmPicture *picture)
{
	if (picture->cache_link == NULL)
	{
		return;
	}
	*picture->cache_link = picture->next;
	if (picture->next != NULL)
	{
		picture->next->cache_link = picture->cache_link;
	}
	picture->next = NULL;
	picture->cache_link = NULL;
}

FvwmPicture *PGetFvwmPicture(
	Display *dpy, Window win, char *ImagePath, const char *name,
	FvwmPictureAttributes fpa)
{
	char *path = PictureFindImageFile(name, ImagePath, R_OK);
	FvwmPicture *p;
	monitor_scale_factor factor = {MONITOR_SCALE_QUARTERS_MIN};

	if (path == NULL)
	{
		return NULL;
	}
	p = picture_loader != NULL ? picture_loader(dpy, win, path, fpa,
	    factor, 0, 0, WM_UI_RESOURCE_GENERATION_INITIAL, False,
	    picture_loader_context) : PImageLoadFvwmPictureFromFile(
	    dpy, win, path, fpa, factor, 0, 0);
	if (p == NULL)
	{
		free(path);
	}

	return p;
}

void PFreeFvwmPictureData(FvwmPicture *p)
{
	if (!p)
	{
		return;
	}
	picture_cache_unlink(p);
	if (p->alloc_pixels != NULL)
	{
		free(p->alloc_pixels);
	}
	if(p->name!=NULL)
	{
		free(p->name);
	}
	free(p);

	return;
}

FvwmPicture *PCacheFvwmPictureAtScale(
	Display *dpy, Window win, char *ImagePath, const char *name,
	FvwmPictureAttributes fpa, monitor_scale_factor factor,
	unsigned target_width, unsigned target_height, unsigned generation)
{
	char *path;
	char *real_path;
	FvwmPicture *p = FvwmPictureList;
	unsigned int scale_quarters = factor.quarters;
	uint64_t path_hash = 0;
	Bool compared_name = False, have_path_hash = False;

	/* First find the full pathname */
	if ((path = PictureFindImageFile(name, ImagePath, R_OK)) == NULL)
	{
		return NULL;
	}
        /* Remove any svg rendering options from real_path */
	if (USE_SVG && *path == ':' &&
	    (real_path = strchr(path + 1, ':')))
	{
		real_path++;
	}
	else
	{
		real_path = path;
	}

	/* See if the picture is already cached */
	while (p)
	{
		/* Reject incompatible variants before reading any name bytes. */
		if (!PICTURE_FPA_AGREE(p,fpa) ||
		    p->scale_quarters != scale_quarters ||
		    p->target_width != target_width ||
		    p->target_height != target_height ||
		    p->generation != generation || p->name == NULL)
		{
			p = p->next;
			continue;
		}
		/* The first compatible entry keeps the usual head-hit path cheap.
		 * Only a longer search computes the request's fingerprint. */
		if (compared_name)
		{
			if (!have_path_hash)
			{
				path_hash = picture_cache_name_hash(path);
				have_path_hash = True;
			}
			if (p->cache_name_hash != path_hash)
			{
				p = p->next;
				continue;
			}
		}
		compared_name = True;

		/* Only a matching variant needs a filesystem stamp check.
		 * Check every matching entry so changed files still reload. */
		if (strcmp(path, p->name) == 0
		    && !isFileStampChanged(&p->stamp, real_path))
		{
			p->count++; /* Put another weight on the picture */
			free(path);
			return p;
		}
		p=p->next;
	}

	/* Not previously cached, have to load it ourself. Put it first in list
	 */
	p = picture_loader != NULL ? picture_loader(dpy, win, path, fpa,
	    factor, target_width, target_height, generation, True,
	    picture_loader_context) : PImageLoadFvwmPictureFromFile(
	    dpy, win, path, fpa, factor, target_width, target_height);
	if(p)
	{
		p->scale_quarters = scale_quarters;
		p->target_width = target_width;
		p->target_height = target_height;
		p->generation = generation;
		if (!p->is_view)
		{
			picture_cache_publish(p);
		}
	}
	else
	{
		free(path);
	}

	return p;
}

FvwmPicture *PCacheFvwmPicture(
	Display *dpy, Window win, char *ImagePath, const char *name,
	FvwmPictureAttributes fpa)
{
	monitor_scale_factor factor = {MONITOR_SCALE_QUARTERS_MIN};

	return PCacheFvwmPictureAtScale(
		dpy, win, ImagePath, name, fpa, factor, 0, 0,
		WM_UI_RESOURCE_GENERATION_INITIAL);
}

void PDestroyFvwmPicture(Display *dpy, FvwmPicture *p)
{
	if (!p)
	{
		return;
	}
	/* Remove a weight */
	if(--(p->count)>0)
	{
		/* still too heavy? */
		return;
	}
	/* Withdraw the dying record before resource release or view callbacks.
	 * Neighbour retirement repairs the link of every surviving record. */
	picture_cache_unlink(p);
	if (p->is_view)
	{
		/* The subscription is removed before releasing backing. A view
		 * never owns the aliased pixmaps or allocated-color array. */
		if (p->view_release != NULL)
		{
			p->view_release(dpy, p, p->view_context);
		}
		if (p->view_owner_release != NULL)
		{
			p->view_owner_release(p->view_owner);
		}
		PDestroyFvwmPicture(dpy, p->backing);
		free(p->name);
		free(p);
		return;
	}

	picture_color_release(dpy, p->effect_background);
	/* Let it fly */
	if (p->prepared_cursor != None)
	{
		XFreeCursor(dpy, p->prepared_cursor);
	}
	if (p->alloc_pixels != NULL)
	{
		if (p->nalloc_pixels != 0)
		{
			PictureFreeColors(
				dpy, p->color_cmap != None ? p->color_cmap : Pcmap,
				p->alloc_pixels, p->nalloc_pixels,
				0, p->no_limit);
		}
		free(p->alloc_pixels);
	}
	if(p->name!=NULL)
	{
		free(p->name);
	}
	if(p->picture!=None)
	{
		XFreePixmap(dpy,p->picture);
	}
	if(p->mask!=None)
	{
		XFreePixmap(dpy,p->mask);
	}
	if(p->alpha != None)
	{
		XFreePixmap(dpy, p->alpha);
	}
	free(p);

	return;
}

FvwmPicture *PLoadFvwmPictureFromPixmap(
	Display *dpy, Window win, char *name, Pixmap pixmap,
	Pixmap mask, Pixmap alpha, int width, int height, int nalloc_pixels,
	Pixel *alloc_pixels, int no_limit)
{
	FvwmPicture *q;

	q = fxcalloc(1, sizeof(FvwmPicture));
	q->count = 1;
	q->name = name;
	q->next = NULL;
	q->stamp = pixmap;
	q->picture = pixmap;
	q->mask = mask;
	q->alpha = alpha;
	q->width = width;
	q->height = height;
	q->depth = Pdepth;
	q->nalloc_pixels = nalloc_pixels;
	q->alloc_pixels = alloc_pixels;
	q->no_limit = no_limit;
	q->color_cmap = Pcmap;
	return q;
}

FvwmPicture *PCacheFvwmPictureFromPixmap(
	Display *dpy, Window win, char *name, Pixmap pixmap,
	Pixmap mask, Pixmap alpha, int width, int height, int nalloc_pixels,
	Pixel *alloc_pixels, int no_limit)
{
	FvwmPicture *p;

	/* Not previously cached, have to load. Put it first in list */
	p = PLoadFvwmPictureFromPixmap(
		dpy, win, name, pixmap, mask, alpha, width, height,
		nalloc_pixels, alloc_pixels, no_limit);
	if(p)
	{
		p->scale_quarters = MONITOR_SCALE_QUARTERS_MIN;
		p->target_width = 0;
		p->target_height = 0;
		p->generation = WM_UI_RESOURCE_GENERATION_INITIAL;
		picture_cache_publish(p);
	}

	return p;
}

FvwmPicture *PCloneFvwmPicture(FvwmPicture *pic)
{
	if (pic != NULL)
	{
		pic->count++;
	}

	return pic;
}

void PicturePrintImageCache(int verbose)
{
	FvwmPicture *p;
	unsigned int count = 0;
	unsigned int hits = 0;
	unsigned int num_alpha = 0;
	unsigned int num_mask = 0;

	fflush(stderr);
	fflush(stdout);
	fvwm_debug(__func__, "fvwm info on Image cache:\n");

	for (p = FvwmPictureList; p != NULL; p = p->next)
	{
		int num_pixmaps = 1;
		if (p->mask != None)
		{
			num_mask++;
			num_pixmaps++;
		}
		if (p->alpha != None)
		{
			num_alpha++;
			num_pixmaps++;
		}
		if (verbose > 0)
		{
			fvwm_debug(__func__,
				   "Image: %s (%d pixmaps; used %d times)\n",
				   p->name, num_pixmaps, p->count);
		}
		count++;
		hits += p->count-1;
	}

	fvwm_debug(__func__, "%u images in cache (%d reuses) "
		   "(%u masks, %u alpha channels => %u pixmaps)\n",
		   count, hits, num_mask, num_alpha,
		   count + num_mask + num_alpha);
	fflush(stderr);
}
