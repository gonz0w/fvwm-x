/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include "config.h"
#include "image_effect.h"

#include <stddef.h>

bool image_effect_blend(const uint16_t source[3], const uint16_t tint[3],
	const uint16_t background[3], unsigned alpha, bool masked,
	unsigned tint_percent, unsigned alpha_percent, image_effect_pixel *result)
{
	if (result == NULL)
	{
		return false;
	}
	*result = (image_effect_pixel){0};
	if (source == NULL || tint == NULL || background == NULL ||
	    alpha > 255 || tint_percent > 100 || alpha_percent > 100)
	{
		return false;
	}
	alpha = masked ? 0 : alpha * alpha_percent / 100;
	if (alpha == 0)
	{
		return true;
	}
	uint16_t channels[3];

	for (unsigned i = 0; i < 3; i++)
	{
		uint32_t channel = ((100 - tint_percent) * source[i] +
		    tint_percent * tint[i]) / 100;

		channels[i] = ((255 - alpha) * background[i] +
		    alpha * channel) / 255;
	}
	*result = (image_effect_pixel){channels[0], channels[1], channels[2],
	    IMAGE_EFFECT_RGB};
	return true;
}
