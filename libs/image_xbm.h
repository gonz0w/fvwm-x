#ifndef FVWMLIB_IMAGE_XBM_H
#define FVWMLIB_IMAGE_XBM_H

#include "image_input.h"

/* Xlib-compatible X10/X11 bitmap data from the checked snapshot. Validate
 * dimensions before allocation. Failure clears data/dimensions/hotspots;
 * success returns caller-owned packed rows without X10 source padding. */
bool image_xbm_read(const image_input *source, unsigned max_dimension,
	size_t max_pixels, unsigned char **bits, unsigned *width,
	unsigned *height, int *x_hot, int *y_hot);

#endif
