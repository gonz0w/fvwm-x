/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include "config.h"
#include "libs/FRenderInit.h"
#include "libs/Fcursor.h"
#include "libs/Fpng.h"
#include "libs/Fsvg.h"
#include "libs/Fxpm.h"
#include "libs/Picture.h"
#include "libs/System.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static const char *swap_path;
static struct stat swap_metadata;
static int swapped;
static int animation_checked;
static int image_hotspot = -1;
#ifdef HAVE_XCURSOR
static int image_checked;
#endif

ssize_t __real_read(int fd, void *bytes, size_t length);

/* Replace an owned fixture after acquisition. The loader must continue from
 * the opened inode and use that inode's metadata for the cache stamp. */
ssize_t __wrap_read(int fd, void *bytes, size_t length)
{
	struct stat metadata;

	if (swap_path && !swapped && fstat(fd, &metadata) == 0 &&
	    metadata.st_dev == swap_metadata.st_dev &&
	    metadata.st_ino == swap_metadata.st_ino)
	{
		assert(unlink(swap_path) == 0);
		assert(mkfifo(swap_path, 0600) == 0);
		swapped = 1;
	}
	return __real_read(fd, bytes, length);
}

#ifdef HAVE_XCURSOR
Cursor __real_XcursorImagesLoadCursor(Display *, const XcursorImages *);
Cursor __real_XcursorImageLoadCursor(Display *, const XcursorImage *);

Cursor __wrap_XcursorImageLoadCursor(Display *display,
	const XcursorImage *image)
{
	if (image_hotspot >= 0)
	{
		assert(image->width == 2 && image->height == 2);
		assert(image->xhot == (unsigned)image_hotspot);
		assert(image->yhot == 1 && image->pixels[0] == 0xffff0000U);
		image_checked++;
	}
	return __real_XcursorImageLoadCursor(display, image);
}

Cursor __wrap_XcursorImagesLoadCursor(Display *display,
	const XcursorImages *images)
{
	assert(images->nimage == 2);
	for (int index = 0; index < 2; index++)
	{
		XcursorImage *image = images->images[index];

		assert(image->width == 2 && image->height == 2);
		assert(image->xhot == 0 && image->yhot == 1);
		assert(image->delay == (unsigned)(100 * (index + 1)));
		assert(image->pixels[0] ==
		    (index == 0 ? 0xffff0000U : 0xff0000ffU));
	}
	animation_checked++;
	return __real_XcursorImagesLoadCursor(display, images);
}
#endif

static int descriptor_count(void)
{
	int count = 0;

	for (int fd = 0; fd < 256; fd++)
	{
		if (fcntl(fd, F_GETFD) >= 0 || errno != EBADF)
		{
			count++;
		}
	}
	return count;
}

static void refuse(Display *display, char *path, int all_apis)
{
	FvwmPictureAttributes attributes = {0};
	monitor_scale_factor factor = {4};
	Pixmap picture = 123, mask = 123, alpha = 123;
	int width = 123, height = 123, depth = 123;
	int nalloc = 123, no_limit = 123;
	Pixel *pixels = (Pixel *)&nalloc;
	char *owned_path = strdup(path);
	int before = descriptor_count();

	assert(owned_path);
	assert(!PImageLoadPixmapFromFile(display, DefaultRootWindow(display),
	    path, &picture, &mask, &alpha, &width, &height, &depth,
	    &nalloc, &pixels, &no_limit, attributes));
	assert(picture == None && mask == None && alpha == None);
	assert(width == 0 && height == 0 && depth == 0);
	assert(nalloc == 0 && pixels == NULL && no_limit == False);
	if (all_apis)
	{
		assert(!PImageLoadFvwmPictureFromFile(display,
		    DefaultRootWindow(display), owned_path, attributes,
		    factor, 0, 0));
		assert(!PImageLoadCursorFromFile(display,
		    DefaultRootWindow(display), path, -1, -1, factor));
	}
	free(owned_path);
	XSync(display, False);
	assert(descriptor_count() == before);
}

static unsigned long pixel(Display *display, Pixmap pixmap)
{
	XImage *image = XGetImage(display, pixmap, 0, 0, 1, 1,
	    AllPlanes, ZPixmap);
	unsigned long value;

	assert(image);
	value = XGetPixel(image, 0, 0);
	XDestroyImage(image);
	return value;
}

static void load(Display *display, char *path, const char *kind,
	unsigned expected)
{
	FvwmPictureAttributes attributes = {0};
	monitor_scale_factor factor = {4};
	char *owned_path = strdup(path);
	FvwmPicture *picture;
	int before = descriptor_count();

	assert(owned_path);
	picture = PImageLoadFvwmPictureFromFile(display,
	    DefaultRootWindow(display), owned_path, attributes, factor, 0, 0);
	assert(picture && picture->width == 2 && picture->height == 2);
	if (strcmp(kind, "bitmap") == 0)
	{
		assert(picture->depth == 1);
		assert(pixel(display, picture->picture) == 1);
	}
	else
	{
		unsigned alpha = expected >> 24;

		if (picture->alpha != None)
		{
			assert(pixel(display, picture->alpha) == alpha);
		}
		else if (picture->mask != None)
		{
			assert(pixel(display, picture->mask) == (alpha > 126));
		}
		if (alpha == 255 || picture->alpha != None || alpha > 126)
		{
			XColor color = {
			    .pixel = pixel(display, picture->picture)};
			unsigned wanted[3] = {(expected >> 16) & 255,
			    (expected >> 8) & 255, expected & 255};
			unsigned actual[3];

			XQueryColor(display, Pcmap, &color);
			actual[0] = color.red;
			actual[1] = color.green;
			actual[2] = color.blue;
			for (int index = 0; index < 3; index++)
			{
				assert(abs((int)actual[index] -
				    (int)(wanted[index] * 257)) <= 8 * 257);
			}
		}
	}
	if (swap_path)
	{
		assert(swapped);
		assert(picture->stamp ==
		    ((FileStamp)swap_metadata.st_mtime << 13) +
		    (FileStamp)swap_metadata.st_size);
	}
	PDestroyFvwmPicture(display, picture);
	XSync(display, False);
	assert(descriptor_count() == before);
}

int main(int argc, char **argv)
{
	Display *display;

	if (argc == 2 && strcmp(argv[1], "--features") == 0)
	{
		printf("{\"png\":%d,\"svg\":%d,\"xpm\":%d,"
		    "\"xcursor\":%d,\"limit\":%zu,\"expanded_limit\":%zu,"
		    "\"gzip\":%d,\"svg_timeout_ms\":%d,\"svg_memory_limit\":%llu}\n",
		    !!PngSupport, !!USE_SVG, !!XpmSupport,
#ifdef HAVE_XCURSOR
		    1,
#else
		    0,
#endif
		    (size_t)FVWM_IMAGE_INPUT_LIMIT,
		    (size_t)FVWM_IMAGE_XPM_EXPANDED_LIMIT,
		    FVWM_IMAGE_GZIP[0] != 0, FVWM_IMAGE_SVG_TIMEOUT_MS,
		    (unsigned long long)FVWM_IMAGE_SVG_MEMORY_LIMIT);
		return 0;
	}
	assert(argc >= 3);
	display = XOpenDisplay(NULL);
	assert(display);
	PictureInitCMapRoot(display, True, NULL, False, True);
	FRenderInit(display);
	if (strcmp(argv[1], "refuse") == 0 ||
	    strcmp(argv[1], "refuse-pixmap") == 0)
	{
		for (int index = 2; index < argc; index++)
		{
			refuse(display, argv[index],
			    strcmp(argv[1], "refuse") == 0);
		}
	}
	else if (strcmp(argv[1], "cursor") == 0)
	{
		monitor_scale_factor factor = {4};
		int before = descriptor_count();

		if (argc == 4 && strcmp(argv[3], "hotspot") == 0)
		{
			image_hotspot = 0;
		}

		for (int index = 0; index < 8; index++)
		{
			Cursor cursor = PImageLoadCursorFromFile(display,
			    DefaultRootWindow(display), argv[2],
			    -1, -1, factor);

			assert(cursor != None);
			XFreeCursor(display, cursor);
		}
		if (argc == 4 && strcmp(argv[3], "animated") == 0)
		{
			assert(animation_checked == 8);
		}
#ifdef HAVE_XCURSOR
		if (image_hotspot >= 0)
		{
			assert(image_checked == 8);
		}
#endif
		XSync(display, False);
		assert(descriptor_count() == before);
	}
	else
	{
		assert(argc == 5);
		if (strcmp(argv[1], "swap") == 0)
		{
			swap_path = argv[2];
			assert(stat(swap_path, &swap_metadata) == 0);
		}
		else
		{
			assert(strcmp(argv[1], "load") == 0);
		}
		load(display, argv[2], argv[3], strtoul(argv[4], NULL, 16));
		if (swap_path)
		{
			swap_path = NULL;
			refuse(display, argv[2], 1);
		}
	}
	XCloseDisplay(display);
	puts("checked image handoff passed");
	return 0;
}
