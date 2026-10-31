/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or any later version.
 */
#include "config.h"
#include "image_background.h"
#include "image_png.h"
#include "ImageSafety.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

bool image_background_render(const struct background_plan *plan,
	unsigned width, unsigned height, uint32_t **pixels)
{
	image_input input = {0};
	uint32_t *source = NULL, *output = NULL;
	int sw = 0, sh = 0;
	size_t count;
	bool success = false;
	char checked[BACKGROUND_SUFFIX_MAX + 1];

	if (!pixels)
		return false;
	*pixels = NULL;
	if (!background_plan_format(checked, sizeof(checked), plan) ||
	    !FImageSizeIsSafe(width, height,
		IMAGE_SVG_MAX_DIMENSION, IMAGE_SVG_MAX_PIXELS, &count))
		return false;
	unsigned mode = (plan->flags & BACKGROUND_MODE_MASK) >> 1;

	if (mode != BACKGROUND_SOLID && mode != BACKGROUND_PNG)
		return false;
	if (mode == BACKGROUND_PNG &&
	    (!image_input_load(plan->path, FVWM_IMAGE_INPUT_LIMIT, &input, NULL) ||
	     !image_png_decode(&input, &source, &sw, &sh)))
		goto done;
	output = malloc(count * sizeof(*output));
	if (!output)
		goto done;
	/* Centered nearest-neighbour aspect cover. All products are uint64_t;
	 * both source and destination have already passed common pixel limits. */
	uint64_t scaled_w = width, scaled_h = height;

	if (source)
	{
		if ((uint64_t)width * sh >= (uint64_t)height * sw)
			scaled_h = ((uint64_t)sh * width + sw - 1) / sw;
		else
			scaled_w = ((uint64_t)sw * height + sh - 1) / sh;
	}
	for (unsigned y = 0; y < height; y++)
	{
		for (unsigned x = 0; x < width; x++)
		{
			uint32_t rgb = plan->rgb;

			if (source)
			{
				size_t sx = ((uint64_t)x + (scaled_w - width) / 2) *
					sw / scaled_w;
				size_t sy = ((uint64_t)y + (scaled_h - height) / 2) *
					sh / scaled_h;
				uint32_t argb = source[sy * sw + sx];
				unsigned alpha = argb >> 24;

				rgb = 0;
				for (unsigned shift = 0; shift < 24; shift += 8)
					rgb |= ((((argb >> shift) & 255) * alpha +
					    ((plan->rgb >> shift) & 255) * (255 - alpha)) /
					    255) << shift;
			}
			output[(size_t)y * width + x] = 0xff000000U | rgb;
		}
	}
	*pixels = output;
	output = NULL;
	success = true;
done:
	free(output);
	free(source);
	image_input_destroy(&input);
	return success;
}
