/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include "config.h"
#include "libs/picture_argb.h"
#include "libs/picture_view.h"
#include "libs/Picture.h"
#include "libs/PictureUtils.h"
#include "libs/PictureGraphics.h"
#include "libs/FRenderInit.h"
#include "libs/Fxpm.h"

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static Bool bounded_step;
static unsigned upload_calls;
static size_t uploaded_pixels;
static unsigned released_colors;

int __real_XPutImage(Display *, Drawable, GC, XImage *, int, int,
	int, int, unsigned, unsigned);
int __real_XFreeColors(Display *, Colormap, unsigned long *, int,
	unsigned long);

int __wrap_XPutImage(Display *dpy, Drawable drawable, GC gc, XImage *image,
	int sx, int sy, int dx, int dy, unsigned width, unsigned height)
{
	if (bounded_step)
	{
		assert((size_t)width * height <= PICTURE_ARGB_STEP_PIXELS);
		upload_calls++;
		uploaded_pixels += (size_t)width * height;
	}
	return __real_XPutImage(dpy, drawable, gc, image,
	    sx, sy, dx, dy, width, height);
}

int __wrap_XFreeColors(Display *dpy, Colormap cmap, unsigned long *pixels,
	int count, unsigned long planes)
{
	if (bounded_step)
	{
		assert(count >= 0);
		released_colors += count;
	}
	return __real_XFreeColors(dpy, cmap, pixels, count, planes);
}

static Bool step(picture_argb_builder *builder)
{
	upload_calls = released_colors = 0;
	uploaded_pixels = 0;
	bounded_step = True;
	Bool complete = picture_argb_step(builder);

	bounded_step = False;
	assert(upload_calls <= 3);
	assert(uploaded_pixels <= 3 * PICTURE_ARGB_STEP_PIXELS);
	/* Final unique-color transfer has at most 256 recorded entries. */
	assert(released_colors <= PICTURE_ARGB_STEP_PIXELS + 256);
	return complete;
}

static XImage *read_pixmap(Display *dpy, FvwmPicture *picture, Pixmap pixmap)
{
	return XGetImage(dpy, pixmap, 0, 0, picture->width, picture->height,
	    AllPlanes, ZPixmap);
}

static void compare(Display *dpy, FvwmPicture *expected,
	FvwmPicture *actual, const uint32_t *pixels, unsigned flags)
{
	assert(expected->width == actual->width);
	assert(expected->height == actual->height);
	assert(expected->depth == actual->depth);
	assert((expected->mask == None) == (actual->mask == None));
	assert((expected->alpha == None) == (actual->alpha == None));
	Pixmap originals[] = {
	    expected->picture, expected->mask, expected->alpha};
	Pixmap candidates[] = {actual->picture, actual->mask, actual->alpha};

	for (unsigned kind = 0; kind < 3; kind++)
	{
		if (originals[kind] == None)
		{
			continue;
		}
		XImage *before = read_pixmap(dpy, expected, originals[kind]);
		XImage *after = read_pixmap(dpy, actual, candidates[kind]);

		assert(before != NULL && after != NULL);
		for (unsigned y = 0; y < actual->height; y++)
		{
			for (unsigned x = 0; x < actual->width; x++)
			{
				unsigned alpha = pixels[
				    (size_t)y * actual->width + x] >> 24;
				unsigned threshold = expected->alpha != None &&
				    !(flags & FPAM_NO_ALPHA) ? 0 :
				    FRenderGetAlphaDepth() != 0 &&
				    !(flags & FPAM_NO_ALPHA) ? 0 :
				    PICTURE_ALPHA_LIMIT;

				/* Old transparent RGB is unspecified; mask and
				 * alpha must still match exactly. */
				if (kind != 0 || alpha > threshold)
				{
					assert(XGetPixel(before, x, y) ==
					    XGetPixel(after, x, y));
				}
			}
		}
		XDestroyImage(before);
		XDestroyImage(after);
	}
}

static void test_pixels(Display *dpy, unsigned width, unsigned height,
	unsigned flags, unsigned pattern)
{
	fprintf(stderr, "depth=%u pixels=%ux%u flags=%u pattern=%u\n",
	    Pdepth, width, height, flags, pattern);
	size_t count = (size_t)width * height;
	uint32_t *pixels = malloc(count * sizeof(*pixels));
	FvwmPictureAttributes attributes = {.mask = flags};
	FvwmPicture *expected = calloc(1, sizeof(*expected));

	assert(pixels != NULL && expected != NULL);
	for (size_t i = 0; i < count; i++)
	{
		unsigned alpha = pattern == 0 ? 255 :
		    pattern == 1 ?
		    (i / PICTURE_ARGB_STEP_PIXELS == 0 ? 0 : 128) :
		    pattern == 2 ? (i % 3 == 0 ? 0 : 255) : i % 256;

		pixels[i] = (alpha << 24) | ((i % 256) << 16) |
		    (((i / 256) % 256) << 8) | (255 - i % 256);
	}
	expected->count = 1;
	expected->width = width;
	expected->height = height;
	expected->depth = flags & FPAM_MONOCHROME ? 1 : Pdepth;
	assert(PImageCreatePixmapFromArgbData(dpy, DefaultRootWindow(dpy),
	    pixels, count, 0, width, height, &expected->picture,
	    &expected->mask,
	    &expected->alpha, &expected->nalloc_pixels, &expected->alloc_pixels,
	    &expected->no_limit, attributes));
	XSync(dpy, False);
	picture_argb_builder *builder = picture_argb_start(dpy,
	    DefaultRootWindow(dpy), pixels, count, width, height, attributes);

	assert(builder != NULL);
	assert(picture_argb_take(builder) == NULL && errno == EBUSY);
	assert(!picture_argb_destroy(builder) && errno == EBUSY);
	unsigned turns = 1;

	while (!step(builder))
	{
		assert(++turns <= count + 10);
	}
	assert(turns >= (count + PICTURE_ARGB_STEP_PIXELS - 1) /
	    PICTURE_ARGB_STEP_PIXELS);
	FvwmPicture *actual = picture_argb_take(builder);

	assert(actual != NULL);
	assert(actual->average_valid == !(flags & FPAM_MONOCHROME));
	compare(dpy, expected, actual, pixels, flags);
	assert(picture_argb_destroy(builder));
	PDestroyFvwmPicture(dpy, expected);
	PDestroyFvwmPicture(dpy, actual);
	free(pixels);
}

static void test_source_average(Display *dpy)
{
	/* Alpha-zero white is excluded, alpha-one white is included even
	 * when NO_ALPHA's binary renderer makes it invisible. */
	uint32_t pixels[] = {0x00ffffff, 0xff000000, 0x01ffffff, 0xffff0000};
	picture_argb_builder *builder = picture_argb_start(dpy,
	    DefaultRootWindow(dpy), pixels, 4, 2, 2,
	    (FvwmPictureAttributes){.mask = FPAM_NO_ALPHA});

	assert(builder != NULL);
	while (!step(builder))
	{
	}
	FvwmPicture *picture = picture_argb_take(builder);

	assert(picture != NULL && picture->average_valid &&
	    picture->average_has_pixels);
	assert(picture->average_rgb[0] == 43690 &&
	    picture->average_rgb[1] == 21845 &&
	    picture->average_rgb[2] == 21845);
	FvwmPicture *view = picture_view_create(strdup("average-view"),
	    (FvwmPictureAttributes){0}, (monitor_scale_factor){4},
	    0, 0, 0, NULL, NULL);

	picture_view_set_backing(dpy, view, picture);
	assert(view->average_valid && view->average_has_pixels);
	assert(memcmp(view->average_rgb, picture->average_rgb,
	    sizeof(view->average_rgb)) == 0);
	PDestroyFvwmPicture(dpy, picture);
	picture = picture_snapshot(view);
	PDestroyFvwmPicture(dpy, view);
	assert(picture != NULL && picture->average_rgb[0] == 43690);
	PDestroyFvwmPicture(dpy, picture);
	assert(picture_argb_destroy(builder));
#if XpmSupport
	/* Ordinary file loading uses the same pre-palette summary policy. */
	char path[] = "/tmp/fvwm-average-XXXXXX";
	const char source[] = "/* XPM */\nstatic char *data[] = {\n"
	    "\"3 1 3 1\",\"a c None\",\"b c black\",\"c c white\","
	    "\"abc\"};\n";
	int fd = mkstemp(path);

	assert(fd >= 0 && write(fd, source, sizeof(source) - 1) ==
	    sizeof(source) - 1);
	assert(close(fd) == 0);
	picture = PImageLoadFvwmPictureFromFile(dpy, DefaultRootWindow(dpy),
	    strdup(path), (FvwmPictureAttributes){0},
	    (monitor_scale_factor){4}, 0, 0);
	assert(picture != NULL && picture->average_valid &&
	    picture->average_has_pixels);
	assert(picture->average_rgb[0] == 32767 &&
	    picture->average_rgb[1] == 32767 &&
	    picture->average_rgb[2] == 32767);
	PDestroyFvwmPicture(dpy, picture);
	assert(unlink(path) == 0);
#endif
}

static void test_cancel(Display *dpy)
{
	uint32_t pixels[32768];

	for (unsigned i = 0; i < 32768; i++)
	{
		pixels[i] = 0xff123456;
	}
	for (unsigned painted = 0; painted < 2; painted++)
	{
		picture_argb_builder *builder = picture_argb_start(dpy,
		    DefaultRootWindow(dpy), pixels, 32768, 256, 128,
		    (FvwmPictureAttributes){.mask = FPAM_NO_COLOR_LIMIT});

		assert(builder != NULL);
		if (painted)
		{
			assert(!step(builder));
		}
		picture_argb_cancel(builder);
		while (!step(builder))
		{
			assert(upload_calls == 0);
		}
		assert(upload_calls == 0);
		assert(picture_argb_take(builder) == NULL &&
		    errno == ECANCELED);
		assert(picture_argb_destroy(builder));
	}
	assert(picture_argb_start(dpy, DefaultRootWindow(dpy), pixels,
	    1, 0, 1, (FvwmPictureAttributes){0}) == NULL && errno == EINVAL);
	assert(picture_argb_start(dpy, DefaultRootWindow(dpy), pixels,
	    1, 32769, 1, (FvwmPictureAttributes){0}) == NULL &&
	    errno == EINVAL);
	assert(picture_argb_start(dpy, DefaultRootWindow(dpy), pixels,
	    1, 2, 1, (FvwmPictureAttributes){0}) == NULL && errno == EINVAL);
}

static void release_view(Display *dpy, FvwmPicture *view, void *context)
{
	unsigned *released = context;

	assert(dpy != NULL && view->is_view && view->count == 0);
	assert(view->backing == NULL || picture_is_usable(view->backing));
	(*released)++;
}

static FvwmPicture *small_picture(Display *dpy, unsigned extent)
{
	uint32_t pixels[16];

	for (unsigned i = 0; i < 16; i++)
	{
		pixels[i] = 0xff123456;
	}
	picture_argb_builder *builder = picture_argb_start(dpy,
	    DefaultRootWindow(dpy), pixels, 16, extent, extent,
	    (FvwmPictureAttributes){0});

	assert(builder != NULL);
	while (!step(builder))
	{
	}
	FvwmPicture *picture = picture_argb_take(builder);

	assert(picture != NULL && picture_argb_destroy(builder));
	return picture;
}

static void test_views(Display *dpy)
{
	unsigned released = 0;
	FvwmPicture *old = small_picture(dpy, 2);
	FvwmPicture *replacement = small_picture(dpy, 4);
	FvwmPicture *views[3];

	for (unsigned i = 0; i < 3; i++)
	{
		views[i] = picture_view_create(strdup("new.svg"),
		    (FvwmPictureAttributes){0}, (monitor_scale_factor){8},
		    16, 16, 11, release_view, &released);
		assert(!picture_is_usable(views[i]) && views[i]->pending);
		assert(views[i]->picture == None && views[i]->width == 0);
		assert(picture_snapshot(views[i]) == NULL);
	}
	assert(picture_snapshot(NULL) == NULL);
	picture_view_set_backing(dpy, views[0], old);
	picture_view_set_backing(dpy, views[1], views[0]);
	assert(old->count == 3 && views[1]->backing == old);
	FvwmPicture *snapshot = picture_snapshot(views[0]);

	assert(snapshot == old && !snapshot->is_view && old->count == 4);
	PDestroyFvwmPicture(dpy, old);
	picture_view_set_backing(dpy, views[0], replacement);
	assert(views[0]->width == 4 && views[1]->width == 2);
	assert(views[1]->picture != views[0]->picture);
	assert(strcmp(views[0]->name, "new.svg") == 0);
	assert(views[0]->generation == 11 && views[0]->scale_quarters == 8);
	picture_view_set_backing(dpy, views[2], views[1]);
	picture_view_set_backing(dpy, views[1], replacement);
	assert(views[2]->backing == old && old->count == 2);
	XImage *retained = read_pixmap(dpy, views[2], views[2]->picture);

	assert(retained != NULL);
	XDestroyImage(retained);
	picture_view_set_backing(dpy, views[2], NULL);
	assert(!picture_is_usable(views[2]) && views[2]->backing == NULL);
	assert(snapshot->width == 2 && snapshot->count == 1);
	retained = read_pixmap(dpy, snapshot, snapshot->picture);
	assert(retained != NULL);
	XDestroyImage(retained);
	PDestroyFvwmPicture(dpy, snapshot);
	PCloneFvwmPicture(views[0]);
	PDestroyFvwmPicture(dpy, views[0]);
	assert(released == 0);
	for (unsigned i = 0; i < 3; i++)
	{
		PDestroyFvwmPicture(dpy, views[i]);
	}
	assert(released == 3 && replacement->count == 1);
	PDestroyFvwmPicture(dpy, replacement);
}

static void test_absent_render(Display *dpy)
{
	FvwmPicture absent = {0};
	unsigned long serial = NextRequest(dpy);

	/* Invalid destination/GC make an accidental render visible as an X
	 * error. Absence must issue no request and must tolerate NULL. */
	PGraphicsRenderPicture(dpy, None, NULL, NULL, None, None, None,
	    None, 0, 0, 1, 1, 0, 0, 1, 1, False);
	PGraphicsRenderPicture(dpy, None, &absent, NULL, None, None, None,
	    None, 0, 0, 1, 1, 0, 0, 1, 1, False);
	assert(NextRequest(dpy) == serial);
}

int main(int argc, char **argv)
{
	Display *dpy = XOpenDisplay(NULL);

	assert(dpy != NULL);
	PictureColorLimitOption options = {
		.color_limit = 68, .strict = 1, .allocate = 0,
		.not_dynamic = 0, .use_named_table = 0};
	Bool strict = argc == 2 && strcmp(argv[1], "--strict") == 0;

	PictureInitCMapRoot(dpy, True, strict ? &options : NULL, strict, True);
	PictureSaveFvwmVisual();
	FRenderInit(dpy);
	for (unsigned pattern = 0; pattern < 4; pattern++)
	{
		test_pixels(dpy, 257, 129, 0, pattern);
		test_pixels(dpy, 16385, 2, FPAM_DITHER, pattern);
		test_pixels(dpy, 32767, 1, FPAM_NO_ALPHA, pattern);
		test_pixels(dpy, 256, 129, FPAM_MONOCHROME, pattern);
		test_pixels(dpy, 257, 3, FPAM_NO_COLOR_LIMIT, pattern);
	}
	XSync(dpy, False);
	fprintf(stderr, "cancellation depth=%u\n", Pdepth);
	test_source_average(dpy);
	test_cancel(dpy);
	test_views(dpy);
	test_absent_render(dpy);
	XSync(dpy, False);
	printf("bounded ARGB conversion matches legacy pixels at depth %u\n",
	    Pdepth);
	XCloseDisplay(dpy);
	return 0;
}
