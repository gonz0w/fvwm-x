/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include "config.h"
#include "picture_color.h"
#include "PictureUtils.h"
#include "safemalloc.h"

#include <assert.h>
#include <stdlib.h>

struct picture_color
{
	Colormap cmap;
	Pixel pixel;
	Bool no_limit;
	Bool allocated;
	size_t references;
};

picture_color *picture_color_adopt(Colormap cmap, Pixel pixel, Bool no_limit)
{
	picture_color *color = fxmalloc(sizeof(*color));

	*color = (picture_color){cmap, pixel, no_limit, True, 1};
	return color;
}

picture_color *picture_color_borrow(Colormap cmap, Pixel pixel)
{
	picture_color *color = picture_color_adopt(cmap, pixel, False);

	color->allocated = False;
	return color;
}

picture_color *picture_color_retain(picture_color *color)
{
	if (color != NULL)
	{
		assert(color->references > 0 && color->references < SIZE_MAX);
		color->references++;
	}
	return color;
}

void picture_color_release(Display *dpy, picture_color *color)
{
	if (color == NULL)
	{
		return;
	}
	assert(color->references > 0);
	if (--color->references == 0)
	{
		if (color->allocated)
		{
			PictureFreeColors(dpy, color->cmap, &color->pixel, 1, 0,
			    color->no_limit);
		}
		free(color);
	}
}

Pixel picture_color_pixel(const picture_color *color)
{
	return color->pixel;
}
