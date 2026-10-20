/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include "config.h"
#include "libs/PictureBase.h"
#include "libs/PictureUtils.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned queries, replies, releases, allocations, freed_colors;
static unsigned fail_color;
static int reply_count = 1;
static Bool null_reply, live;
static XVisualInfo *owned_reply;
static Visual selected_visual;
static unsigned long selected_id = 0x42, selected_cmap = 0x43;

XVisualInfo *__real_XGetVisualInfo(Display *, long, XVisualInfo *, int *);
int __real_XFree(void *);
Status __real_XAllocColor(Display *, Colormap, XColor *);
int __real_XFreeColors(Display *, Colormap, unsigned long *, int, unsigned long);
int __real_PictureInitColors(int, Bool, PictureColorLimitOption *, Bool, Bool);

XVisualInfo *__wrap_XGetVisualInfo(Display *display, long mask,
	XVisualInfo *query, int *count)
{
	queries++;
	assert((mask & (VisualIDMask | VisualScreenMask)) ==
		(VisualIDMask | VisualScreenMask));
	assert(query->screen == DefaultScreen(display));
	if (live)
		owned_reply = __real_XGetVisualInfo(display, mask, query, count);
	else
	{
		assert(query->visualid == selected_id);
		*count = reply_count;
		owned_reply = null_reply ? NULL : malloc(sizeof(*owned_reply));
		if (owned_reply != NULL)
			*owned_reply = (XVisualInfo) {
				.visual = &selected_visual, .visualid = selected_id,
				.screen = DefaultScreen(display), .depth = 24
			};
	}
	if (owned_reply != NULL)
		replies++;
	return owned_reply;
}

int __wrap_XFree(void *data)
{
	if (data != NULL && data == owned_reply)
	{
		releases++;
		owned_reply = NULL;
	}
	return __real_XFree(data);
}

Status __wrap_XAllocColor(Display *display, Colormap cmap, XColor *color)
{
	allocations++;
	assert(cmap == selected_cmap);
	assert(color->flags == (DoRed | DoGreen | DoBlue));
	assert(color->red == (allocations == 1 ? 65535 : 0));
	assert(color->red == color->green && color->green == color->blue);
	/* A refusal must not publish anything the server left in the output. */
	color->pixel = 0xdead;
	if (allocations == fail_color)
		return 0;
	if (live)
		return __real_XAllocColor(display, cmap, color);
	color->pixel = allocations == 1 ? 19 : 23;
	return 1;
}

int __wrap_XFreeColors(Display *display, Colormap cmap, unsigned long *pixels,
	int count, unsigned long planes)
{
	assert(count == 1 && planes == 0 && cmap == selected_cmap);
	if (!live)
		assert(pixels[0] == 19);
	freed_colors++;
	return live ? __real_XFreeColors(display, cmap, pixels, count, planes) : 1;
}

int __wrap_PictureInitColors(int caller, Bool initialize,
	PictureColorLimitOption *options, Bool own_limit, Bool dither)
{
	return live ? __real_PictureInitColors(caller, initialize, options,
		own_limit, dither) : 0;
}

static void environment(const char *visual, const char *cmap)
{
	assert(owned_reply == NULL);
	if (visual == NULL)
		unsetenv("FVWM_VISUALID");
	else
		assert(setenv("FVWM_VISUALID", visual, 1) == 0);
	if (cmap == NULL)
		unsetenv("FVWM_COLORMAP");
	else
		assert(setenv("FVWM_COLORMAP", cmap, 1) == 0);
	queries = replies = releases = allocations = freed_colors = 0;
	fail_color = 0;
	reply_count = 1;
	null_reply = False;
}

static void defaults(Display *display)
{
	assert(Pdefault && Pdpy == display);
	assert(Pvisual == DefaultVisual(display, DefaultScreen(display)));
	assert(Pdepth == DefaultDepth(display, DefaultScreen(display)));
	assert(Pcmap == DefaultColormap(display, DefaultScreen(display)));
	assert(PictureWhitePixel() == WhitePixel(display, DefaultScreen(display)));
	assert(PictureBlackPixel() == BlackPixel(display, DefaultScreen(display)));
	assert(replies == releases && owned_reply == NULL);
}

static void policy_tests(Display *display)
{
	const char *bad[] = {NULL, "", "0", "0x", "-1", "+42", " 42", "42 ",
		"42junk", "100000000", "000000042", "0x100000000"};
	unsigned cases = 0;

	environment(NULL, NULL);
	PictureInitCMap(display);
	defaults(display);
	assert(queries == 0 && allocations == 0);
	for (unsigned i = 0; i < sizeof(bad) / sizeof(bad[0]); i++)
	{
		for (unsigned field = 0; field < 2; field++)
		{
			environment(field == 0 ? bad[i] : "42",
				field == 1 ? bad[i] : "43");
			PictureInitCMap(display);
			defaults(display);
			assert(queries == 0 && allocations == 0);
			cases++;
		}
	}
	for (unsigned mode = 0; mode < 5; mode++)
	{
		environment("0X42", "0x43");
		null_reply = mode == 0;
		reply_count = mode == 1 ? 0 : 1;
		fail_color = mode == 2 ? 1 : mode == 3 ? 2 : 0;
		PictureInitCMap(display);
		assert(queries == 1 && replies == releases);
		if (mode != 4)
			defaults(display);
		else
		{
			assert(!Pdefault && Pvisual == &selected_visual);
			assert(Pdepth == 24 && Pcmap == selected_cmap);
			assert(PictureWhitePixel() == 19 && PictureBlackPixel() == 23);
		}
		assert(freed_colors == (mode == 3 ? 1 : 0));
		cases++;
	}
	/* A failed pair must not replace either already published pixel. */
	for (unsigned failure = 1; failure <= 2; failure++)
	{
		allocations = freed_colors = 0;
		fail_color = failure;
		assert(!PictureSetupWhiteAndBlack());
		assert(PictureWhitePixel() == 19 && PictureBlackPixel() == 23);
		assert(freed_colors == (failure == 2 ? 1 : 0));
	}
	printf("picture visual: %u admission/reply/color cases passed\n", cases + 3);
}

static void live_tests(Display *display)
{
	char visual[32], cmap[32];
	Window root = DefaultRootWindow(display);
	selected_cmap = XCreateColormap(display, root,
		DefaultVisual(display, DefaultScreen(display)), AllocNone);

	snprintf(visual, sizeof(visual), "%lx", XVisualIDFromVisual(
		DefaultVisual(display, DefaultScreen(display))));
	snprintf(cmap, sizeof(cmap), "%lx", selected_cmap);
	for (unsigned mode = 0; mode < 3; mode++)
	{
		environment(visual, cmap);
		fail_color = mode;
		PictureInitCMap(display);
		assert(queries == 1 && replies == 1 && releases == 1);
		if (mode == 0)
		{
			XSetWindowAttributes attrs = {.colormap = Pcmap,
				.background_pixel = PictureWhitePixel(),
				.border_pixel = PictureBlackPixel()};
			Window window = XCreateWindow(display, root, 0, 0, 32, 32,
				0, Pdepth, InputOutput, Pvisual,
				CWColormap | CWBackPixel | CWBorderPixel, &attrs);
			assert(!Pdefault && Pcmap == selected_cmap);
			XMapWindow(display, window);
			XDestroyWindow(display, window);
			unsigned long pixels[] = {
				PictureWhitePixel(), PictureBlackPixel()};
			__real_XFreeColors(display, Pcmap, pixels, 2, 0);
		}
		else
			defaults(display);
		XSync(display, False);
	}
	environment("ffffffff", cmap);
	PictureInitCMap(display);
	defaults(display);
	assert(queries == 1 && allocations == 0);
	XFreeColormap(display, selected_cmap);
	puts("picture visual: 4 real-X initialization cases passed");
}

int main(int argc, char **argv)
{
	live = argc == 2 && strcmp(argv[1], "--live") == 0;
	if (live)
	{
		Display *display = XOpenDisplay(NULL);
		assert(display != NULL);
		live_tests(display);
		XCloseDisplay(display);
	}
	else
	{
		/* Xlib's default accessors read these fields without a connection. */
		_XPrivDisplay display = calloc(1, sizeof(*display));
		Screen screen = {0};
		Visual default_visual = {0};
		assert(display != NULL);
		display->screens = &screen;
		screen.root_visual = &default_visual;
		screen.root_depth = 16;
		screen.cmap = 7;
		screen.white_pixel = 255;
		screen.black_pixel = 0;
		policy_tests((Display *)display);
		free(display);
	}
	return 0;
}
