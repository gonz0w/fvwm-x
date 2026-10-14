/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include "config.h"
#include "libs/Colorset.h"
#include "libs/XError.h"
#include "libs/fvwmlib.h"

#include <assert.h>
#include <stdio.h>

static int handled_errors;
static int delegated_errors;
static Pixmap expiring_pixmap;

GC __real_XCreateGC(Display *, Drawable, unsigned long, XGCValues *);

GC __wrap_XCreateGC(Display *display, Drawable drawable,
		   unsigned long mask, XGCValues *values)
{
	if (expiring_pixmap != None && (mask & GCTile) &&
	    values->tile == expiring_pixmap)
	{
		/* Reproduce a wallpaper replacement after geometry validation but
		 * before the buffered transparent draw creates its tiled GC. */
		XFreePixmap(display, expiring_pixmap);
		XSync(display, False);
		expiring_pixmap = None;
	}
	return __real_XCreateGC(display, drawable, mask, values);
}

static int original_handler(Display *display, XErrorEvent *error)
{
	(void)display;
	(void)error;
	delegated_errors++;
	return 0;
}

static int temporary_handler(Display *display, XErrorEvent *error)
{
	if (error->error_code == BadDrawable)
	{
		handled_errors++;
		return 0;
	}
	return ferror_call_next_error_handler(display, error);
}

static void invalid_geometry(Display *display)
{
	Window root;
	int x, y;
	unsigned int width, height, border, depth;

	/* None cannot name a drawable. Exercise a real server error rather
	 * than invoking the callback directly. */
	assert(!XGetGeometry(display, None, &root, &x, &y, &width, &height,
			     &border, &depth));
	XSync(display, False);
}

static void test_replaced_root_pixmap(Display *display)
{
	Window root = DefaultRootWindow(display);
	colorset_t colorset = {0};
	int previous_errors = delegated_errors;

	flib_init_graphics(display);
	expiring_pixmap = XCreatePixmap(display, root, 32, 32, Pdepth);
	colorset.pixmap = expiring_pixmap;
	colorset.pixmap_type = PIXMAP_ROOT_PIXMAP_PURE;
	colorset.width = colorset.height = 32;
	assert(CreateBackgroundPixmap(display, root, 32, 32, &colorset, Pdepth,
				      DefaultGC(display, DefaultScreen(display)),
				      False) == None);
	assert(expiring_pixmap == None);
	/* Only the invalid tile from GC creation is delegated. BadGC from
	 * drawing and GC cleanup belongs to the temporary handler. */
	assert(delegated_errors == previous_errors + 1);
	invalid_geometry(display);
	assert(delegated_errors == previous_errors + 2);
}

int main(void)
{
	Display *display = XOpenDisplay(NULL);
	assert(display != NULL);
	XErrorHandler previous = XSetErrorHandler(original_handler);

	ferror_set_temp_error_handler(temporary_handler);
	invalid_geometry(display);
	assert(handled_errors == 1 && delegated_errors == 0);

	/* Other errors must reach the original handler while the temporary
	 * handler is installed. */
	XDestroyWindow(display, None);
	XSync(display, False);
	assert(handled_errors == 1 && delegated_errors == 1);

	ferror_reset_temp_error_handler();
	invalid_geometry(display);
	assert(handled_errors == 1 && delegated_errors == 2);
	test_replaced_root_pixmap(display);
	assert(XSetErrorHandler(previous) == original_handler);
	XCloseDisplay(display);
	puts("temporary handler, delegation, restoration, and pixmap cleanup passed");
	return 0;
}
