/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "config.h"
#include "modules/FvwmScript/types.h"
#include "modules/FvwmScript/Widgets/Tools.h"
#include "libs/flocale_font_types.h"
#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

extern X11base *x11base;
void LoadIcon(struct XObj *object);
void __real_free(void *pointer);
static Pixel *owned_pixels;
static unsigned int vectors, color_releases, pixmap_releases;
static unsigned int serial = 10;
static bool released[4096];
static Bool decode_ok = True;
static int color_count = 2;

void __wrap_free(void *pointer)
{
	if (pointer != NULL && pointer == owned_pixels)
	{
		assert(vectors == 1);
		vectors--;
		owned_pixels = NULL;
	}
	__real_free(pointer);
}

int __wrap_XFreePixmap(Display *display, Pixmap pixmap)
{
	(void)display;
	assert(pixmap >= 10 && pixmap < serial);
	assert(!released[pixmap]);
	released[pixmap] = true;
	pixmap_releases++;
	return 1;
}

void __wrap_PictureFreeColors(Display *display, Colormap cmap,
	Pixel *pixels, int count, unsigned long planes, Bool no_limit)
{
	(void)display;
	(void)cmap;
	(void)planes;
	(void)no_limit;
	assert(pixels == owned_pixels && count == color_count && count > 0);
	color_releases++;
}

char *__wrap_PictureFindImageFile(const char *name, const char *path,
	int access_mode)
{
	(void)path;
	(void)access_mode;
	return strcmp(name, "missing") == 0 ? NULL : strdup(name);
}

Bool __wrap_PImageLoadPixmapFromFile(Display *display, Window window,
	char *file, Pixmap *pixmap, Pixmap *mask, Pixmap *alpha, int *width,
	int *height, int *depth, int *count, Pixel **pixels, int *no_limit,
	FvwmPictureAttributes attributes)
{
	(void)display;
	(void)window;
	(void)file;
	(void)no_limit;
	(void)attributes;
	assert(vectors == 0);
	assert(serial + 3 < sizeof(released) / sizeof(released[0]));
	owned_pixels = malloc(2 * sizeof(*owned_pixels));
	assert(owned_pixels != NULL);
	vectors++;
	*pixels = owned_pixels;
	*count = color_count;
	*pixmap = serial++;
	*mask = serial++;
	*alpha = serial++;
	*width = 17;
	*height = 23;
	*depth = 24;
	return decode_ok;
}

#ifndef SCRIPT_ICON_BASELINE_CONTROL
static void assert_empty(const struct XObj *object)
{
	assert(object->iconPixmap == None && object->icon_maskPixmap == None &&
		object->icon_alphaPixmap == None);
	assert(object->alloc_pixels == NULL && object->nalloc_pixels == 0);
	assert(object->icon_w == 0 && object->icon_h == 0 && vectors == 0);
}

static void exercise_owner(void)
{
	FlocaleFont borrowed_font = {0};
	struct XObj object = {.colorset = -1, .win = 7,
		.gc = (GC)&borrowed_font, .Ffont = &borrowed_font};
	script_layout_geometry geometry;
	struct XObj layout = {.TypeWidget = ItemDraw, .title = "",
		.width = 1, .height = 1, .icon = "missing",
		.icon_w = 17, .icon_h = 23};

	/* Requested names and old dimensions cannot establish a loaded icon. */
	assert(script_widget_layout(&layout, NULL, &geometry));
	assert(geometry.width == 1 && geometry.height == 1);
	layout.iconPixmap = 9;
	assert(script_widget_layout(&layout, NULL, &geometry));
	assert(geometry.width == 17 && geometry.height == 23);
	layout.icon_h = 0;
	assert(script_widget_layout(&layout, NULL, &geometry));
	assert(geometry.width == 1 && geometry.height == 1);
	script_icon_clear(&object);
	assert_empty(&object);
	for (unsigned int repeat = 0; repeat < 100; repeat++)
	{
		unsigned int pixmaps = pixmap_releases;
		unsigned int colors = color_releases;

		object.icon = "first";
		LoadIcon(&object);
		object.icon = "replacement";
		LoadIcon(&object);
		assert(pixmap_releases == pixmaps + 3);
		assert(color_releases == colors + 1);
		script_icon_clear(&object);
		script_icon_clear(&object);
		assert_empty(&object);
		assert(pixmap_releases == pixmaps + 6);
		assert(color_releases == colors + 2);

		color_count = 0;
		LoadIcon(&object);
		assert(object.alloc_pixels != NULL && vectors == 1);
		object.icon = "missing";
		LoadIcon(&object);
		assert_empty(&object);
		assert(pixmap_releases == pixmaps + 9);
		assert(color_releases == colors + 2);
		color_count = 2;

		/* Refusal may leave partial loader outputs; the widget owns them. */
		decode_ok = False;
		object.icon = "refused";
		LoadIcon(&object);
		assert_empty(&object);
		assert(pixmap_releases == pixmaps + 12);
		assert(color_releases == colors + 3);
		decode_ok = True;
		LoadIcon(&object);
		assert(object.icon_w == 17 && object.icon_h == 23);
		object.icon = "missing";
		LoadIcon(&object);
		assert_empty(&object);
		assert(pixmap_releases == pixmaps + 15);
		assert(color_releases == colors + 4);
		assert(object.win == 7 && object.gc == (GC)&borrowed_font &&
			object.Ffont == &borrowed_font);
	}
}
#endif

int __wrap_main(int argc, char **argv)
{
	X11base base = {0};
	struct XObj object = {.colorset = -1, .icon = "loaded"};
	int failed;

	(void)argc;
	(void)argv;
	x11base = &base;
	ScriptName = "icon-owner";
	LoadIcon(&object);
	assert(object.icon_w == 17 && object.icon_h == 23 && vectors == 1);
	object.icon = NULL;
	LoadIcon(&object);
	failed = vectors != 0 || object.alloc_pixels != NULL ||
		object.nalloc_pixels != 0 || object.icon_w != 0 || object.icon_h != 0;
	fprintf(stderr, "actual LoadIcon null replacement: vectors=%u "
		"dimensions=%dx%d pixmap_releases=%u color_releases=%u\n",
		vectors, object.icon_w, object.icon_h,
		pixmap_releases, color_releases);
	/* Keep this phase independent of new symbols so an unchanged module
	 * object can demonstrate both baseline failures in one execution. */
	assert(!failed);
	assert(pixmap_releases == 3 && color_releases == 1);
#ifndef SCRIPT_ICON_BASELINE_CONTROL
	exercise_owner();
#endif
	puts("actual Script icon owners release once and reset layout on refusal");
	return 0;
}
