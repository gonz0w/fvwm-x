#ifndef FVWMLIB_IMAGE_SVG_H
#define FVWMLIB_IMAGE_SVG_H

#include "image_input.h"
#include "monitor_scale.h"
#include <stdint.h>

/* Decoder implementation for the private exec'd worker only. No X calls. */
bool image_svg_render(const image_input *source, const char *path,
	monitor_scale_factor factor, unsigned target_width, unsigned target_height,
	uint32_t **pixels, int *width, int *height);

#endif
