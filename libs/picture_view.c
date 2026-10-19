/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include "config.h"
#include "picture_view.h"
#include "safemalloc.h"

#include <assert.h>
#include "libs/picture_types.h"
Bool picture_is_usable(const FvwmPicture *picture)
{
	return picture != NULL && picture->picture != None &&
	    picture->width != 0 && picture->height != 0;
}

FvwmPicture *picture_snapshot(FvwmPicture *picture)
{
	if (picture != NULL && picture->is_view)
	{
		picture = picture->backing;
	}
	return picture_is_usable(picture) ? PCloneFvwmPicture(picture) : NULL;
}

FvwmPicture *picture_view_create(char *name, FvwmPictureAttributes attributes,
	monitor_scale_factor factor, unsigned target_width,
	unsigned target_height,
	unsigned generation, picture_view_release_fn release, void *context)
{
	FvwmPicture *view = fxcalloc(1, sizeof(*view));

	view->count = 1;
	view->name = name;
	view->fpa_mask = attributes.mask;
	view->scale_quarters = factor.quarters;
	view->target_width = target_width;
	view->target_height = target_height;
	view->generation = generation;
	view->is_view = view->pending = True;
	view->view_release = release;
	view->view_context = context;
	return view;
}

void picture_view_set_backing(Display *dpy, FvwmPicture *view,
	FvwmPicture *picture)
{
	assert(view != NULL && view->is_view && view->count != 0);
	if (picture != NULL && picture->is_view)
	{
		picture = picture->backing;
	}
	assert(picture == NULL || !picture->is_view);
	if (!picture_is_usable(picture))
	{
		picture = NULL;
	}
	FvwmPicture *old = view->backing;

	view->backing = PCloneFvwmPicture(picture);
	view->picture = picture != NULL ? picture->picture : None;
	view->mask = picture != NULL ? picture->mask : None;
	view->alpha = picture != NULL ? picture->alpha : None;
	view->width = picture != NULL ? picture->width : 0;
	view->height = picture != NULL ? picture->height : 0;
	view->depth = picture != NULL ? picture->depth : 0;
	view->alloc_pixels = picture != NULL ? picture->alloc_pixels : NULL;
	view->nalloc_pixels = picture != NULL ? picture->nalloc_pixels : 0;
	view->no_limit = picture != NULL ? picture->no_limit : False;
	view->color_cmap = picture != NULL ? picture->color_cmap : None;
	view->average_valid = picture != NULL && picture->average_valid;
	view->average_has_pixels = picture != NULL &&
	    picture->average_has_pixels;
	for (unsigned i = 0; i < 3; i++)
	{
		view->average_rgb[i] = picture != NULL ?
		    picture->average_rgb[i] : 0;
	}
	PDestroyFvwmPicture(dpy, old);
}
