/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or any later version.
 */
#include "config.h"
#include "libs/PictureUtils.h"

#include <stdio.h>
#include <stdlib.h>

/* Link only a separately named test core with
 * -Wl,--wrap=PictureAllocColor. Normal allocations pass through until the
 * live fixture creates its private marker after startup. This wrapper does
 * not create successful colors, packets, workers or resource owners. */
int __real_PictureAllocColor(Display *dpy, Colormap cmap, XColor *color,
	int no_limit);
int __wrap_PictureAllocColor(Display *dpy, Colormap cmap, XColor *color,
	int no_limit);

int __wrap_PictureAllocColor(Display *dpy, Colormap cmap, XColor *color,
	int no_limit)
{
	static unsigned int attempts;
	const char *path = getenv("FVWM_TEST_CONTRAST_ALLOC_FAIL");
	FILE *marker = path != NULL ? fopen(path, "r") : NULL;

	if (marker != NULL)
	{
		unsigned int fail_at = 0;
		int armed = fscanf(marker, "%u", &fail_at) == 1;

		fclose(marker);
		if (armed && fail_at != 0 && ++attempts == fail_at)
		{
			fprintf(stderr, "contrast allocation failure at %u\n",
				attempts);
			return 0;
		}
	}
	else
		attempts = 0;
	return __real_PictureAllocColor(dpy, cmap, color, no_limit);
}
