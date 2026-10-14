/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include "config.h"
#include "libs/image_effect_client.h"
#include "libs/picture_argb.h"
#include "libs/picture_async.h"
#include "libs/Picture.h"
#include "libs/PictureGraphics.h"
#include "libs/PictureImageLoader.h"
#include "libs/PictureUtils.h"
#include "libs/FRenderInit.h"
#include "libs/fvwmsignal.h"

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static Bool force_software = True;
Bool __real_FRenderGetExtensionSupported(void);

/* Force the public legacy renderer's software branch in this probe. */
Bool __wrap_FRenderGetExtensionSupported(void)
{
	return force_software ? False : __real_FRenderGetExtensionSupported();
}

static image_process_status pump(image_process *process)
{
	for (unsigned turns = 0; ; turns++)
	{
		assert(turns < 10000);
		image_process_status status = image_process_step(process);

		if (status != IMAGE_PROCESS_RUNNING)
		{
			return status;
		}
		struct pollfd endpoints[2];

		image_process_descriptors(process, endpoints);
		int ready = poll(endpoints, 2, 10);

		assert(ready >= 0 || errno == EINTR);
	}
}

static void test_legacy_pixels(Display *dpy, unsigned tint_percent,
	unsigned alpha_percent, unsigned flags)
{
	const unsigned width = 513, height = 3;
	CARD32 pixels[513 * 3];

	for (unsigned i = 0; i < width * height; i++)
	{
		unsigned alpha[] = {0, 1, 127, 128, 255};

		pixels[i] = alpha[i % 5] << 24 | ((i * 17) & 255) << 16 |
		    ((i * 31) & 255) << 8 | ((i * 7) & 255);
	}
	FvwmPicture *source = calloc(1, sizeof(*source));

	assert(source != NULL);
	source->count = 1;
	source->width = width;
	source->height = height;
	source->depth = Pdepth;
	source->color_cmap = Pcmap;
	assert(PImageCreatePixmapFromArgbData(dpy, DefaultRootWindow(dpy),
	    pixels, width * height, 0, width, height, &source->picture, &source->mask,
	    &source->alpha, &source->nalloc_pixels, &source->alloc_pixels,
	    &source->no_limit, (FvwmPictureAttributes){.mask = flags}));
	XColor bg = {.blue = 65535, .flags = DoRed | DoGreen | DoBlue};
	XColor tint = {.green = 65535, .flags = DoRed | DoGreen | DoBlue};

	assert(PictureAllocColor(dpy, Pcmap, &bg, True));
	assert(PictureAllocColor(dpy, Pcmap, &tint, True));
	Pixmap destination = XCreatePixmap(dpy, DefaultRootWindow(dpy),
	    width, height, Pdepth);
	XGCValues values = {.foreground = bg.pixel};
	GC gc = XCreateGC(dpy, destination, GCForeground, &values);

	XFillRectangle(dpy, destination, gc, 0, 0, width, height);
	XSync(dpy, False); /* Independent test setup, not production service. */
	image_effect_request request = {.width = width, .height = height,
	    .pixmap = source->picture, .mask = source->mask,
	    .alpha = source->alpha, .visual = XVisualIDFromVisual(Pvisual),
	    .depth = Pdepth, .alpha_depth = FRenderGetAlphaDepth(),
	    .colormap = Pcmap, .background = bg.pixel, .tint = tint.pixel,
	    .tint_percent = tint_percent, .alpha_percent = alpha_percent};
	image_input input = {0};
	image_effect_result result = {0};
	image_process *process = image_effect_start(DisplayString(dpy),
	    &request, &input, &result);

	assert(process != NULL && pump(process) == IMAGE_PROCESS_SUCCEEDED);
	assert(image_process_destroy(process));
	assert(image_effect_result_complete(&result));
	assert(result.count == width * height);
	FvwmRenderAttributes attributes = {
	    .mask = FRAM_HAVE_ADDED_ALPHA | FRAM_HAVE_TINT,
	    .added_alpha_percent = alpha_percent, .tint = tint.pixel,
	    .tint_percent = tint_percent};

	PGraphicsRenderPixmaps(dpy, DefaultRootWindow(dpy), source->picture,
	    source->mask, source->alpha, Pdepth, &attributes, destination,
	    gc, None, None, 0, 0, width, height, 0, 0, width, height, False);
	picture_argb_builder *builder = picture_argb_start_effects(dpy,
	    DefaultRootWindow(dpy), result.pixels, result.count, result.width,
	    result.height, bg.pixel);

	assert(builder != NULL);
	for (unsigned turns = 0; !picture_argb_step(builder); turns++)
	{
		assert(turns < 100);
	}
	FvwmPicture *actual = picture_argb_take(builder);

	assert(actual != NULL && picture_argb_destroy(builder));
	assert(!actual->average_valid && actual->mask == None &&
	    actual->alpha == None);
	XImage *before = XGetImage(dpy, destination, 0, 0, width, height,
	    AllPlanes, ZPixmap);
	XImage *after = XGetImage(dpy, actual->picture, 0, 0, width, height,
	    AllPlanes, ZPixmap);

	assert(before != NULL && after != NULL);
	for (unsigned y = 0; y < height; y++)
	{
		for (unsigned x = 0; x < width; x++)
		{
			if (XGetPixel(before, x, y) != XGetPixel(after, x, y))
			{
				fprintf(stderr, "effect mismatch depth=%u x=%u y=%u "
				    "tint=%u alpha=%u\n", Pdepth, x, y,
				    tint_percent, alpha_percent);
				abort();
			}
		}
	}
	XDestroyImage(before);
	XDestroyImage(after);
	PDestroyFvwmPicture(dpy, actual);
	PDestroyFvwmPicture(dpy, source);
	XFreePixmap(dpy, destination);
	XFreeGC(dpy, gc);
	PictureFreeColors(dpy, Pcmap, &bg.pixel, 1, 0, True);
	PictureFreeColors(dpy, Pcmap, &tint.pixel, 1, 0, True);
	image_input_destroy(&input);
	image_effect_result_destroy(&result);
}

static void test_bad_resource(Display *dpy)
{
	image_effect_request request = {.width = 1, .height = 1,
	    .pixmap = 0xffffffffU, .visual = XVisualIDFromVisual(Pvisual),
	    .depth = Pdepth, .colormap = Pcmap, .alpha_percent = 100};
	image_input input = {0};
	image_effect_result result = {0};
	image_process *process = image_effect_start(DisplayString(dpy),
	    &request, &input, &result);

	assert(process != NULL && pump(process) == IMAGE_PROCESS_FAILED);
	assert(!image_effect_result_complete(&result));
	assert(image_process_destroy(process));
	image_input_destroy(&input);
	image_effect_result_destroy(&result);
}

static picture_async *active_manager;
static unsigned unexpected_errors;

static int effect_error(Display *dpy, XErrorEvent *error)
{
	(void)dpy;
	if (!picture_async_handle_error(active_manager, error))
	{
		unexpected_errors++;
	}
	return 0;
}

static void drain(Display *dpy)
{
	while (XPending(dpy))
	{
		XEvent event;

		XNextEvent(dpy, &event);
		picture_async_handle_event(active_manager, &event);
	}
}

static void service(Display *dpy)
{
	picture_async_service(active_manager, True);
	drain(dpy);
	picture_async_commit_ready(active_manager, NULL, NULL, NULL);
	struct pollfd endpoints[3];

	picture_async_descriptors(active_manager, endpoints);
	endpoints[2] = (struct pollfd){ConnectionNumber(dpy), POLLIN, 0};
	int ready = poll(endpoints, 3, 1);

	assert(ready >= 0 || errno == EINTR);
}

static void test_queue_lifetime(Display *dpy, Bool hardware)
{
	force_software = !hardware;
	const uint32_t pixels[] = {0xffff0000U, 0, 0x80123456U, 0xffffffffU};
	picture_argb_builder *builder = picture_argb_start(dpy,
	    DefaultRootWindow(dpy), pixels, 4, 2, 2,
	    (FvwmPictureAttributes){0});

	assert(builder != NULL);
	while (!picture_argb_step(builder))
	{
	}
	FvwmPicture *source = picture_argb_take(builder);

	assert(source != NULL && picture_argb_destroy(builder));
	XColor bg = {.blue = 65535, .flags = DoRed | DoGreen | DoBlue};
	XColor tint = {.green = 65535, .flags = DoRed | DoGreen | DoBlue};

	assert(PictureAllocColor(dpy, Pcmap, &bg, True));
	assert(PictureAllocColor(dpy, Pcmap, &tint, True));
	picture_color *background = picture_color_adopt(Pcmap, bg.pixel, True);
	picture_color *tint_color = picture_color_adopt(Pcmap, tint.pixel, True);

	Pixmap expected = XCreatePixmap(dpy, DefaultRootWindow(dpy), 2, 2, Pdepth);
	XGCValues values = {.foreground = bg.pixel};
	GC gc = XCreateGC(dpy, expected, GCForeground, &values);
	FvwmRenderAttributes attributes = {
	    .mask = FRAM_HAVE_ADDED_ALPHA | FRAM_HAVE_TINT,
	    .added_alpha_percent = 50, .tint = tint.pixel, .tint_percent = 50};

	XFillRectangle(dpy, expected, gc, 0, 0, 2, 2);
	PGraphicsRenderPixmaps(dpy, DefaultRootWindow(dpy), source->picture,
	    source->mask, source->alpha, Pdepth, &attributes, expected, gc,
	    None, None, 0, 0, 2, 2, 0, 0, 2, 2, False);
	XSync(dpy, False); /* Independently acknowledged source setup. */
	active_manager = picture_async_create(dpy, DefaultRootWindow(dpy));
	assert(active_manager != NULL);
	int (*prior_error)(Display *, XErrorEvent *) = XSetErrorHandler(effect_error);
	FvwmPicture *view = picture_async_effect(active_manager, source,
	    background, tint_color, 50, 50);
	FvwmPicture *cancelled = picture_async_effect(active_manager, source,
	    background, tint_color, 25, 75);

	assert(view != NULL && cancelled != NULL && source->count == 3);
	picture_view_set_backing(dpy, view, source);
	assert(view->picture == source->picture && view->pending);
	PDestroyFvwmPicture(dpy, cancelled); /* Never start the queued job. */
	assert(source->count == 3); /* Original, active job, retained old view. */
	picture_color_release(dpy, background);
	picture_color_release(dpy, tint_color);
	PDestroyFvwmPicture(dpy, source); /* Only retained owners remain. */
	picture_async_service(active_manager, False);
	struct pollfd endpoints[2];

	picture_async_descriptors(active_manager, endpoints);
	assert(endpoints[0].fd == -1 && endpoints[1].fd == -1);
	for (unsigned turns = 0; view->pending; turns++)
	{
		assert(turns < 10000);
		service(dpy);
	}
	assert(view->picture != None && view->width == 2 && view->height == 2);
	assert(view->backing->effect_background != NULL);
	XImage *before = XGetImage(dpy, expected, 0, 0, 2, 2, AllPlanes, ZPixmap);
	XImage *after = XGetImage(dpy, view->picture, 0, 0, 2, 2,
	    AllPlanes, ZPixmap);

	assert(before != NULL && after != NULL);
	for (unsigned y = 0; y < 2; y++)
	{
		for (unsigned x = 0; x < 2; x++)
		{
			assert(XGetPixel(before, x, y) == XGetPixel(after, x, y));
		}
	}
	XDestroyImage(before);
	XDestroyImage(after);
	XFreePixmap(dpy, expected);
	XFreeGC(dpy, gc);

	FvwmPicture *snapshot = picture_snapshot(view);

	PDestroyFvwmPicture(dpy, view);
	assert(picture_async_destroy(active_manager));
	active_manager = NULL;
	/* Published backing still pins the exact background cell after the
	 * manager, source owner and requesting palette owners retire. */
	assert(picture_color_pixel(snapshot->effect_background) == bg.pixel);
	XColor preserved = {.pixel = bg.pixel};

	assert(XQueryColor(dpy, Pcmap, &preserved) && preserved.blue == bg.blue);
	PDestroyFvwmPicture(dpy, snapshot);
	XSync(dpy, False);
	assert(unexpected_errors == 0);
	XSetErrorHandler(prior_error);
	force_software = True;
}

int main(int argc, char **argv)
{
	assert(argc == 2 && setenv("FVWM_IMAGE_SVG_WORKER", argv[1], 1) == 0);
	assert(setenv("FVWM_IMAGE_EFFECT_WORKER", argv[1], 1) == 0);
	Display *dpy = XOpenDisplay(NULL);

	assert(dpy != NULL);
	PictureInitCMapRoot(dpy, True, NULL, False, True);
	FRenderInit(dpy);
	test_legacy_pixels(dpy, 0, 100, 0);
	test_legacy_pixels(dpy, 50, 100, 0);
	test_legacy_pixels(dpy, 50, 50, 0);
	test_legacy_pixels(dpy, 100, 0, 0);
	test_legacy_pixels(dpy, 37, 71, FPAM_NO_ALPHA | FPAM_DITHER);
	test_bad_resource(dpy);
	test_queue_lifetime(dpy, False);
	test_queue_lifetime(dpy, True);
	XSync(dpy, False);
	XCloseDisplay(dpy);
	printf("Software effect worker matches legacy pixels at depth %u\n", Pdepth);
	return 0;
}
