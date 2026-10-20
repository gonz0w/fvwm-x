/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include "config.h"
#include "ImageSafety.h"
#include <limits.h>

bool FEwmhIconSizeIsSafe(uint64_t width, uint64_t height,
	size_t retained_words, size_t max_bytes, size_t *pixels, size_t *words)
{
	size_t count;
	size_t limit = max_bytes / sizeof(uint32_t);

	if (pixels != NULL)
		*pixels = 0;
	if (words != NULL)
		*words = 0;
	if (pixels == NULL || words == NULL)
		return false;
	if (limit > INT_MAX / sizeof(uint32_t))
		limit = INT_MAX / sizeof(uint32_t);
	if (!FImageSizeIsSafe(width, height, F_EWMH_ICON_MAX_DIMENSION,
	    F_EWMH_ICON_MAX_PIXELS, &count) || limit < 2 ||
	    retained_words > limit - 2 || count > limit - 2 - retained_words)
		return false;
	*pixels = count;
	*words = count + retained_words + 2;
	return true;
}

FEwmhIconResult FEwmhIconFindExact(
	const uint32_t *data, size_t data_len, uint32_t wanted_width,
	uint32_t wanted_height, size_t *record_start, size_t *record_words)
{
	size_t offset = 0;

	if (record_start != NULL)
		*record_start = 0;
	if (record_words != NULL)
		*record_words = 0;
	if (record_start == NULL || record_words == NULL ||
	    (data == NULL && data_len != 0))
		return F_EWMH_ICON_INVALID;
	while (offset < data_len)
	{
		uint64_t pixels;
		size_t words;

		if (data_len - offset < 2)
			return F_EWMH_ICON_INVALID;
		pixels = (uint64_t)data[offset] * data[offset + 1];
		if (data[offset] == 0 || data[offset + 1] == 0 ||
		    pixels > data_len - offset - 2)
			return F_EWMH_ICON_INVALID;
		words = 2 + (size_t)pixels;
		if (data[offset] == wanted_width &&
		    data[offset + 1] == wanted_height)
		{
			*record_start = offset;
			*record_words = words;
			return F_EWMH_ICON_FOUND;
		}
		offset += words;
	}
	return F_EWMH_ICON_NOT_FOUND;
}

bool FImageSizeIsSafe(
	uint64_t width, uint64_t height, uint64_t max_dimension,
	size_t max_pixels, size_t *pixel_count)
{
	if (pixel_count == NULL || width == 0 || height == 0 ||
	    width > max_dimension || height > max_dimension ||
	    height > max_pixels || width > max_pixels / height)
	{
		return false;
	}

	*pixel_count = (size_t)(width * height);

	return true;
}

bool FEwmhIconFindBest(
	const uint32_t *data, size_t data_len, uint32_t wanted_width,
	uint32_t wanted_height, uint32_t max_dimension, size_t max_pixels,
	size_t *pixel_start, uint32_t *best_width, uint32_t *best_height)
{
	size_t i;
	uint64_t best_distance = UINT64_MAX;

	if (pixel_start == NULL || best_width == NULL || best_height == NULL)
	{
		return false;
	}
	*pixel_start = 0;
	*best_width = 0;
	*best_height = 0;

	if (data == NULL)
	{
		return false;
	}

	for (i = 0; data_len - i >= 2; )
	{
		uint64_t width = data[i];
		uint64_t height = data[i + 1];
		uint64_t pixels = width * height;

		if (width == 0 || height == 0 || pixels > data_len - i - 2)
		{
			*pixel_start = 0;
			*best_width = 0;
			*best_height = 0;
			return false;
		}
		if (width <= max_dimension && height <= max_dimension &&
		    pixels <= max_pixels)
		{
			uint64_t dw = width > wanted_width ?
				width - wanted_width : wanted_width - width;
			uint64_t dh = height > wanted_height ?
				height - wanted_height : wanted_height - height;
			uint64_t dw_squared = dw * dw;
			uint64_t dh_squared = dh * dh;
			uint64_t distance = dw_squared > UINT64_MAX - dh_squared ?
				UINT64_MAX : dw_squared + dh_squared;

			if (distance < best_distance)
			{
				*pixel_start = i + 2;
				*best_width = (uint32_t)width;
				*best_height = (uint32_t)height;
				best_distance = distance;
			}
		}
		i += 2 + (size_t)pixels;
	}
	if (i != data_len)
	{
		*pixel_start = 0;
		*best_width = 0;
		*best_height = 0;
		return false;
	}

	return *best_width != 0 && *best_height != 0;
}
