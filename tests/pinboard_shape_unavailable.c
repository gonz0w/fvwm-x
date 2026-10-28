/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include "config.h"
#include <X11/Xlib.h>
#include <stdio.h>

/* The noninstalled module fixture qualifies its real unavailable branch on
 * a SHAPE-capable server.  This is a controlled query result, not evidence
 * of an actual server without SHAPE. */
Bool __wrap_XShapeQueryExtension(Display *display, int *event_base,
				int *error_base)
{
	(void)display;
	if (event_base)
		*event_base = 0;
	if (error_base)
		*error_base = 0;
	fputs("FVWM_PINBOARD_TEST_SHAPE_QUERY_FALSE\n", stderr);
	fflush(stderr);
	return False;
}
