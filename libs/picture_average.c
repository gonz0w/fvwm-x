/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include "config.h"
#include "picture_average.h"

#include <stddef.h>

#define PICTURE_AVERAGE_MAX_COUNT (UINT64_MAX / UINT16_MAX)

bool picture_average_add(picture_average *average, uint32_t pixel)
{
	if (average == NULL)
	{
		return false;
	}
	if ((pixel >> 24) == 0)
	{
		return true;
	}
	if (average->count >= PICTURE_AVERAGE_MAX_COUNT ||
	    average->red > average->count * 255 ||
	    average->green > average->count * 255 ||
	    average->blue > average->count * 255)
	{
		return false;
	}
	average->red += (pixel >> 16) & 255;
	average->green += (pixel >> 8) & 255;
	average->blue += pixel & 255;
	average->count++;
	return true;
}

bool picture_average_finish(const picture_average *average, uint16_t rgb[3])
{
	if (rgb == NULL)
	{
		return false;
	}
	rgb[0] = rgb[1] = rgb[2] = 0;
	if (average == NULL || average->count == 0 ||
	    average->count > PICTURE_AVERAGE_MAX_COUNT ||
	    average->red > average->count * 255 ||
	    average->green > average->count * 255 ||
	    average->blue > average->count * 255)
	{
		return false;
	}
	rgb[0] = average->red * 257 / average->count;
	rgb[1] = average->green * 257 / average->count;
	rgb[2] = average->blue * 257 / average->count;
	return true;
}
