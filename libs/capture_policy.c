/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include "config.h"
#include "capture_policy.h"
#include "ImageSafety.h"

#include <limits.h>

bool capture_geometry(int width, int height, int xoffset, int stride,
	int bits_per_pixel, size_t *row_bytes)
{
	size_t pixels;
	uint64_t minimum;

	if (row_bytes != NULL)
		*row_bytes = 0;
	if (row_bytes == NULL || width <= 0 || height <= 0 || xoffset < 0 ||
	    stride <= 0 || (bits_per_pixel != 1 && bits_per_pixel != 4 &&
	    bits_per_pixel != 8 && bits_per_pixel != 16 &&
	    bits_per_pixel != 24 && bits_per_pixel != 32) ||
	    !FImageSizeIsSafe(width, height, INT_MAX,
	    FVWM_CAPTURE_MAX_PIXELS, &pixels))
		return false;
	minimum = ((uint64_t)width + xoffset) * bits_per_pixel;
	if ((minimum + 7) / 8 > (uint64_t)stride ||
	    (uint64_t)height * stride > (uint64_t)FVWM_CAPTURE_MAX_PIXELS * 8 ||
	    (size_t)width > SIZE_MAX / 3)
		return false;
	*row_bytes = (size_t)width * 3;
	return true;
}

bool capture_channels(const unsigned long masks[3], int bits_per_pixel,
	capture_channel channels[3])
{
	uint64_t used = 0;
	int i;

	if (masks == NULL || channels == NULL || bits_per_pixel <= 0 ||
	    bits_per_pixel > 32)
		return false;
	for (i = 0; i < 3; i++)
	{
		uint64_t value = masks[i];
		unsigned shift = 0;

		if (value == 0 || value >= (UINT64_C(1) << bits_per_pixel) ||
		    (used & value) != 0)
			return false;
		used |= value;
		while ((value & 1) == 0)
		{
			value >>= 1;
			shift++;
		}
		/* A contiguous component is 2^n - 1 after removing low zeroes. */
		if ((value & (value + 1)) != 0)
			return false;
		channels[i].mask = (uint32_t)masks[i];
		channels[i].shift = shift;
		channels[i].maximum = (uint32_t)value;
	}
	return true;
}

void capture_rgb(unsigned long pixel, const capture_channel channels[3],
	unsigned char rgb[3])
{
	int i;

	for (i = 0; i < 3; i++)
	{
		uint64_t value = (pixel & channels[i].mask) >> channels[i].shift;

		rgb[i] = (unsigned char)(value * 255 / channels[i].maximum);
	}
}
