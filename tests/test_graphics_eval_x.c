/* This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License, version 2 or later. */
#include "config.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "libs/graphics_eval.h"

static unsigned int x_errors;

static int error_handler(Display *display, XErrorEvent *error)
{
	(void)display;
	x_errors++;
	fprintf(stderr, "graphics comparison X error %u request %u\n",
		error->error_code, error->request_code);
	return 0;
}

static unsigned int observe(Display *display, Window window,
	graphics_eval_rect clip, unsigned long background)
{
	XImage *image = XGetImage(display, window, 0, 0, 96, 64,
		AllPlanes, ZPixmap);
	unsigned int colored = 0;

	assert(image);
	for (int y = 0; y < 64; y++)
		for (int x = 0; x < 96; x++)
		{
			unsigned long pixel = XGetPixel(image, x, y);

			if (pixel == background)
				continue;
			assert(x >= clip.x && y >= clip.y &&
				x < clip.x + (int)clip.width &&
				y < clip.y + (int)clip.height);
			colored++;
		}
	XDestroyImage(image);
	return colored;
}

static unsigned int shape_case(Display *display, Window window, Visual *visual,
	unsigned int depth, enum graphics_eval_kind kind, Bool antialias,
	graphics_eval_stats *stats)
{
	GC gc = XCreateGC(display, window, 0, NULL);
	graphics_eval_rect group = {2, 2, 60, 48};
	graphics_eval_operation op = {.kind = kind,
		.rectangle = {12, 12, 24, 24},
		.foreground = 0xf03010, .background = 0x1040e0};
	unsigned int changed = 0;
	unsigned int blended = 0;

	assert(gc);
	/* Nonuniform pixels catch a missing preservation copy, including
	 * places inside the group that this operation has no permission to
	 * touch. */
	for (int y = 0; y < 64; y++)
	{
		XSetForeground(display, gc, y % 2 ? 0x192c43 : 0x475a6d);
		XDrawLine(display, window, gc, 0, y, 95, y);
	}
	XImage *before = XGetImage(display, window, 0, 0, 96, 64,
		AllPlanes, ZPixmap);

	assert(before);
	assert(graphics_eval_paint(display, window, visual, depth,
		96, 64, group, group, &op, 1, antialias, False, stats));
	XImage *after = XGetImage(display, window, 0, 0, 96, 64,
		AllPlanes, ZPixmap);

	assert(after);
	for (int y = 0; y < 64; y++)
		for (int x = 0; x < 96; x++)
		{
			unsigned long a = XGetPixel(after, x, y);
			unsigned long b = XGetPixel(before, x, y);

			if (a == b)
				continue;
			assert(x >= 12 && x < 36 && y >= 12 && y < 36);
			changed++;
			if (a != op.foreground && a != op.background)
				blended++;
		}
	assert(changed > 0);
	if (kind == GRAPHICS_EVAL_RECTANGLE || kind == GRAPHICS_EVAL_ELLIPSE)
		assert(XGetPixel(after, 24, 24) == XGetPixel(before, 24, 24));
	if (kind == GRAPHICS_EVAL_LINE && antialias)
		assert(blended > 0);
	if (kind == GRAPHICS_EVAL_GRADIENT)
		assert(XGetPixel(after, 20, 14) != XGetPixel(after, 20, 33));
	XDestroyImage(before);
	XDestroyImage(after);
	XFreeGC(display, gc);
	return changed;
}

int main(void)
{
	Display *display = XOpenDisplay(NULL);
	graphics_eval_stats stats = {0};
	graphics_eval_rect bounds = {10, 10, 20, 20};
	graphics_eval_rect clip = {10, 10, 20, 10};
	graphics_eval_operation ops[2] = {{
		.kind = GRAPHICS_EVAL_RECTANGLE,
		.rectangle = {10, 10, 20, 20}, .foreground = 0xff0000}};
	unsigned long background = 0x152535;
	unsigned long serial;

	if (!display)
		return 77;
	XSetErrorHandler(error_handler);
	int screen = DefaultScreen(display);
	Visual *visual = DefaultVisual(display, screen);
	unsigned int depth = (unsigned int)DefaultDepth(display, screen);
	assert(graphics_eval_cairo_backend());
	assert(graphics_eval_supported(display, visual, depth));
	Window window = XCreateSimpleWindow(display, RootWindow(display,
		screen),
		0, 0, 96, 64, 0, background, background);
	XMapWindow(display, window);
	XSync(display, False);

	serial = NextRequest(display);
	Visual unsupported = *visual;
	unsupported.class = PseudoColor;
	assert(!graphics_eval_paint(display, window, &unsupported, depth,
		96, 64, bounds, clip, ops, 1, False, False, &stats));
	assert(!graphics_eval_paint(display, window, visual, depth,
		96, 64, bounds, clip, ops, SIZE_MAX, False, False, &stats));
	ops[1] = ops[0];
	ops[1].kind = (enum graphics_eval_kind)999;
	assert(!graphics_eval_paint(display, window, visual, depth,
		96, 64, bounds, clip, ops, 2, False, False, &stats));
	assert(!graphics_eval_paint(display, window, visual, depth,
		96, 64, bounds, clip, ops, 1, False, True, &stats));
	assert(NextRequest(display) == serial);
	assert(observe(display, window, (graphics_eval_rect){0}, background)
		== 0);
	assert(stats.pixmap_requests == 0 && stats.committed == 0);

	assert(graphics_eval_paint(display, window, visual, depth,
		96, 64, bounds, clip, ops, 1, False, False, &stats));
	unsigned int ink = observe(display, window, clip, background);
	assert(ink > 0);
	assert(stats.pixmap_requests == 1 && stats.pixmap_retires == 1);
	assert(stats.gc_requests == 1 && stats.gc_retires == 1);
	assert(stats.committed == 1 && stats.peak_scratch_pixels == 200);
	serial = NextRequest(display);
	assert(graphics_eval_paint(display, window, visual, depth,
		96, 64, bounds, clip, NULL, 0, True, False, &stats));
	assert(NextRequest(display) == serial && stats.empty == 1);

	/* Preparation refusal leaves the ordinary native draw available. */
	XClearWindow(display, window);
	assert(!graphics_eval_paint(display, window, visual, depth,
		96, 64, bounds, bounds, ops, 1, True, True, &stats));
	GC gc = XCreateGC(display, window, 0, NULL);
	assert(gc);
	XSetForeground(display, gc, 0xff0000);
	XDrawRectangle(display, window, gc, 10, 10, 19, 19);
	assert(observe(display, window, bounds, background) > 0);
	XFreeGC(display, gc);
	unsigned int shape_ink = 0;
	for (int kind = GRAPHICS_EVAL_LINE; kind <= GRAPHICS_EVAL_GRADIENT;
		kind++)
		for (int aa = 0; aa < 2; aa++)
			shape_ink += shape_case(display, window, visual, depth,
				(enum graphics_eval_kind)kind, aa, &stats);
	assert(stats.committed == 9);
	assert(stats.preserve_copies == 9 && stats.commit_copies == 9);
	assert(stats.pixels_copied == 2 * (200 + 8 * 60 * 48));
	assert(stats.pixmap_requests == stats.pixmap_retires);
	assert(stats.gc_requests == stats.gc_retires);
	assert(stats.surface_requests == 9 && stats.surface_retires == 9);
	assert(stats.context_requests == 9 && stats.context_retires == 9);
	assert(stats.pattern_requests == 2 && stats.pattern_retires == 2);
	assert(shape_ink > 0);
	XDestroyWindow(display, window);
	XSync(display, False);
	assert(!x_errors);
	XCloseDisplay(display);
	printf("{\"cairo\":true,\"clipped_ink\":%u,"
		"\"precommit_refusals_unchanged\":true,"
		"\"native_fallback\":true,\"own_resources_retired\":true,"
		"\"x_errors\":0}\n", ink);
	return 0;
}
