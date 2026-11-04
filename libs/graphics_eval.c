/* This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License, version 2 or later. */
#include "config.h"
#include "graphics_eval.h"
#include <time.h>

#ifndef FVWM_GRAPHICS_EVAL_CAIRO
#define FVWM_GRAPHICS_EVAL_CAIRO 0
#endif
#if FVWM_GRAPHICS_EVAL_CAIRO
#include <cairo.h>
#include <cairo-xlib.h>
#endif

static void count(uint64_t *value, uint64_t amount)
{
	*value = UINT64_MAX - *value < amount ? UINT64_MAX : *value + amount;
}

Bool graphics_eval_cairo_backend(void)
{
	return FVWM_GRAPHICS_EVAL_CAIRO ? True : False;
}

Bool graphics_eval_supported(
	Display *display, Visual *visual, unsigned int depth)
{
	if (!display || !visual || depth != 24)
		return False;
	int screen = DefaultScreen(display);

	return visual == DefaultVisual(display, screen) &&
		DefaultDepth(display, screen) == 24 &&
		visual->class == TrueColor && visual->red_mask == 0xff0000 &&
		visual->green_mask == 0x00ff00 && visual->blue_mask == 0x0000ff;
}

Bool graphics_eval_clock(uint64_t *milliseconds)
{
	struct timespec stamp;

	if (!milliseconds)
		return False;
	*milliseconds = 0;
	if (clock_gettime(CLOCK_MONOTONIC, &stamp) < 0 || stamp.tv_sec < 0 ||
	    stamp.tv_nsec < 0 || stamp.tv_nsec >= 1000000000)
		return False;
	uint64_t fraction = (uint64_t)stamp.tv_nsec / 1000000;

	if ((uint64_t)stamp.tv_sec > (UINT64_MAX - fraction) / 1000)
		return False;
	*milliseconds = (uint64_t)stamp.tv_sec * 1000 + fraction;
	return True;
}

static Bool operations_valid(graphics_eval_rect bounds,
	const graphics_eval_operation *operations, size_t length)
{
	int64_t right = (int64_t)bounds.x + bounds.width;
	int64_t bottom = (int64_t)bounds.y + bounds.height;

	if (length > GRAPHICS_EVAL_MAX_OPERATIONS || (length && !operations))
		return False;
	for (size_t i = 0; i < length; i++)
	{
		const graphics_eval_operation *op = operations + i;
		graphics_eval_rect rect = op->rectangle;
		unsigned int allowed;

		switch (op->kind)
		{
		case GRAPHICS_EVAL_LINE:
			allowed = GRAPHICS_EVAL_REVERSE_LINE;
			break;
		case GRAPHICS_EVAL_GRADIENT:
			allowed = GRAPHICS_EVAL_HORIZONTAL_GRADIENT;
			break;
		case GRAPHICS_EVAL_RECTANGLE:
		case GRAPHICS_EVAL_ELLIPSE:
			allowed = 0;
			if (rect.width < 2 || rect.height < 2)
				return False;
			break;
		default:
			return False;
		}
		if ((op->flags & ~allowed) || !rect.width || !rect.height ||
		    op->foreground > 0xffffff || op->background > 0xffffff ||
		    rect.x < bounds.x || rect.y < bounds.y ||
		    (int64_t)rect.x + rect.width > right ||
		    (int64_t)rect.y + rect.height > bottom)
			return False;
	}
	return True;
}

#if FVWM_GRAPHICS_EVAL_CAIRO
static void color(cairo_t *context, uint32_t rgb)
{
	cairo_set_source_rgb(context, ((rgb >> 16) & 255u) / 255.0,
		((rgb >> 8) & 255u) / 255.0, (rgb & 255u) / 255.0);
}

static void stop(cairo_pattern_t *pattern, double position, uint32_t rgb)
{
	cairo_pattern_add_color_stop_rgb(pattern, position,
		((rgb >> 16) & 255u) / 255.0,
		((rgb >> 8) & 255u) / 255.0, (rgb & 255u) / 255.0);
}

static void paint_operation(cairo_t *context, const graphics_eval_operation *op,
	graphics_eval_stats *stats)
{
	double x = op->rectangle.x, y = op->rectangle.y;
	double width = op->rectangle.width, height = op->rectangle.height;

	color(context, op->foreground);
	/* A square cap can antialias outside a diagonal's declared footprint.
	 * The aggregate group clip alone cannot enforce each operation's
	 * bounds. */
	cairo_save(context);
	cairo_rectangle(context, x, y, width, height);
	cairo_clip(context);
	cairo_new_path(context);
	switch (op->kind)
	{
	case GRAPHICS_EVAL_LINE:
		if (width == 1 && height == 1)
		{
			cairo_rectangle(context, x, y, 1, 1);
			cairo_fill(context);
			break;
		}
		cairo_move_to(context, x + .5,
			y + ((op->flags & GRAPHICS_EVAL_REVERSE_LINE) ?
			height - .5 : .5));
		cairo_line_to(context, x + width - .5,
			y + ((op->flags & GRAPHICS_EVAL_REVERSE_LINE) ?
			.5 : height - .5));
		cairo_stroke(context);
		break;
	case GRAPHICS_EVAL_RECTANGLE:
		cairo_rectangle(context, x + .5, y + .5, width - 1, height - 1);
		cairo_stroke(context);
		break;
	case GRAPHICS_EVAL_ELLIPSE:
		cairo_save(context);
		cairo_translate(context, x + width / 2, y + height / 2);
		cairo_scale(context, (width - 1) / 2, (height - 1) / 2);
		cairo_arc(context, 0, 0, 1, 0, 6.283185307179586);
		cairo_restore(context);
		cairo_stroke(context);
		break;
	case GRAPHICS_EVAL_GRADIENT:
	{
		Bool horizontal = (op->flags &
			GRAPHICS_EVAL_HORIZONTAL_GRADIENT);
		if (stats)
			count(&stats->pattern_requests, 1);
		cairo_pattern_t *pattern = cairo_pattern_create_linear(x, y,
			x + (horizontal ? width : 0),
			y + (horizontal ? 0 : height));

		stop(pattern, 0, op->foreground);
		stop(pattern, 1, op->background);
		cairo_set_source(context, pattern);
		cairo_pattern_destroy(pattern);
		if (stats)
			count(&stats->pattern_retires, 1);
		cairo_rectangle(context, x, y, width, height);
		cairo_fill(context);
		break;
	}
	}
	cairo_restore(context);
}
#endif

Bool graphics_eval_paint(
	Display *display, Drawable destination, Visual *visual, unsigned int
		depth,
	uint32_t window_width, uint32_t window_height, graphics_eval_rect
		bounds,
	graphics_eval_rect clip, const graphics_eval_operation *operations,
	size_t length, Bool antialias, Bool refuse_preparation,
	graphics_eval_stats *stats)
{
	graphics_eval_rect paint;
	enum graphics_eval_preparation prepared;
	Bool committed = False;
#if FVWM_GRAPHICS_EVAL_CAIRO
	Pixmap scratch = None;
	GC gc = NULL;
	cairo_surface_t *surface = NULL;
	cairo_t *context = NULL;
#endif

	if (stats)
		count(&stats->attempts, 1);
	if (!destination || !graphics_eval_supported(display, visual, depth))
		goto refused;
	prepared = graphics_eval_prepare(bounds, clip, window_width,
		window_height, &paint);
	if (prepared == GRAPHICS_EVAL_REFUSED ||
	    !operations_valid(bounds, operations, length) || refuse_preparation)
		goto refused;
	if (!length || prepared == GRAPHICS_EVAL_EMPTY)
	{
		if (stats)
			count(&stats->empty, 1);
		return True;
	}
#if FVWM_GRAPHICS_EVAL_CAIRO
	/* This is an operation-local staging surface, never a retained image
	 * or backing lease. The owner regenerated this frame's background
	 * first. */
	if (stats)
		count(&stats->pixmap_requests, 1);
	scratch = XCreatePixmap(display, destination, paint.width, paint.height,
		depth);
	if (scratch == None)
		goto cleanup;
	if (stats)
	{
		uint64_t pixels = (uint64_t)paint.width * paint.height;

		count(&stats->scratch_pixels_requested, pixels);
		if (pixels > stats->peak_scratch_pixels)
			stats->peak_scratch_pixels = pixels;
		count(&stats->gc_requests, 1);
	}
	XGCValues values = {.graphics_exposures = False};

	gc = XCreateGC(display, scratch, GCGraphicsExposures, &values);
	if (!gc)
		goto cleanup;
	XCopyArea(display, destination, scratch, gc, paint.x, paint.y,
		paint.width, paint.height, 0, 0);
	if (stats)
	{
		count(&stats->preserve_copies, 1);
		count(&stats->pixels_copied, (uint64_t)paint.width *
			paint.height);
		count(&stats->surface_requests, 1);
	}
	surface = cairo_xlib_surface_create(display, scratch, visual,
		(int)paint.width, (int)paint.height);
	if (cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS)
		goto cleanup;
	if (stats)
		count(&stats->context_requests, 1);
	context = cairo_create(surface);
	if (cairo_status(context) != CAIRO_STATUS_SUCCESS)
		goto cleanup;
	cairo_set_antialias(context, antialias ? CAIRO_ANTIALIAS_DEFAULT :
		CAIRO_ANTIALIAS_NONE);
	cairo_set_line_width(context, 1);
	cairo_set_line_cap(context, CAIRO_LINE_CAP_SQUARE);
	cairo_set_line_join(context, CAIRO_LINE_JOIN_MITER);
	cairo_translate(context, -paint.x, -paint.y);
	cairo_rectangle(context, paint.x, paint.y, paint.width, paint.height);
	cairo_clip(context);
	for (size_t i = 0; i < length; i++)
		paint_operation(context, operations + i, stats);
	if (cairo_status(context) != CAIRO_STATUS_SUCCESS)
		goto cleanup;
	cairo_destroy(context);
	if (stats)
		count(&stats->context_retires, 1);
	context = NULL;
	/* Flush before the native read; finish drops Cairo's drawable access.
	 * Neither status check acknowledges asynchronous server errors. */
	cairo_surface_flush(surface);
	cairo_surface_finish(surface);
	if (cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS)
		goto cleanup;
	XCopyArea(display, scratch, destination, gc, 0, 0,
		paint.width, paint.height, paint.x, paint.y);
	committed = True;
	if (stats)
	{
		count(&stats->committed, 1);
		count(&stats->commit_copies, 1);
		count(&stats->pixels_copied, (uint64_t)paint.width *
			paint.height);
	}
cleanup:
	if (context)
	{
		cairo_destroy(context);
		if (stats)
			count(&stats->context_retires, 1);
	}
	if (surface)
	{
		cairo_surface_finish(surface);
		cairo_surface_destroy(surface);
		if (stats)
			count(&stats->surface_retires, 1);
	}
	if (gc)
	{
		XFreeGC(display, gc);
		if (stats)
			count(&stats->gc_retires, 1);
	}
	if (scratch != None)
	{
		XFreePixmap(display, scratch);
		if (stats)
			count(&stats->pixmap_retires, 1);
	}
#else
	(void)antialias;
#endif
	if (committed)
		return True;
refused:
	if (stats)
		count(&stats->refused, 1);
	return False;
}
