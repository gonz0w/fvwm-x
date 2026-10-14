/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include "config.h"
#include "libs/picture_color.h"

#include <assert.h>

static unsigned releases;

void __wrap_PictureFreeColors(Display *dpy, Colormap cmap, Pixel *pixels,
    int count, unsigned long planes, Bool no_limit)
{
	assert(dpy == NULL && cmap == 19 && count == 1 && planes == 0);
	assert(pixels[0] == 37 && no_limit);
	releases++;
}

int main(void)
{
	picture_color *owner = picture_color_adopt(19, 37, True);
	picture_color *worker = picture_color_retain(owner);
	picture_color *published = picture_color_retain(owner);

	assert(picture_color_pixel(worker) == 37);
	picture_color_release(NULL, owner);
	picture_color_release(NULL, worker);
	assert(releases == 0);
	picture_color_release(NULL, published);
	assert(releases == 1);
	/* Restoring the legacy initial saved pixel must never release an
	 * allocation which belongs to a different foreground/image owner. */
	owner = picture_color_borrow(19, 0);
	published = picture_color_retain(owner);
	picture_color_release(NULL, owner);
	assert(picture_color_pixel(published) == 0);
	picture_color_release(NULL, published);
	picture_color_release(NULL, NULL);
	assert(releases == 1);
	return 0;
}
