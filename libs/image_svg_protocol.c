/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include "config.h"
#include "image_svg_protocol.h"
#include "ImageSafety.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

static void
image_svg_word(unsigned char *bytes, uint32_t word)
{
	for (unsigned i = 0; i < 4; i++)
	{
		bytes[i] = (unsigned char)(word >> (i * 8));
	}
}

static uint32_t
image_svg_read_word(const unsigned char *bytes)
{
	uint32_t word = 0;

	for (unsigned i = 0; i < 4; i++)
	{
		word |= (uint32_t)bytes[i] << (i * 8);
	}
	return word;
}

void
image_svg_header(unsigned char header[IMAGE_SVG_HEADER_SIZE],
	unsigned width, unsigned height)
{
	memcpy(header, "FSVG", 4);
	/* Version plus native pixel byte order; header integers are little endian. */
	uint32_t native = 1;

	image_svg_word(header + 4, 1U | (*(unsigned char *)&native << 8));
	image_svg_word(header + 8, width);
	image_svg_word(header + 12, height);
}

void
image_svg_result_destroy(image_svg_result *result)
{
	if (result != NULL)
	{
		free(result->pixels);
		memset(result, 0, sizeof(*result));
	}
}

bool
image_svg_result_consume(const unsigned char *bytes, size_t length,
	void *context)
{
	image_svg_result *result = context;

	if (result == NULL || (length != 0 && bytes == NULL))
	{
		errno = EINVAL;
		return false;
	}
	if (result->header_used < IMAGE_SVG_HEADER_SIZE)
	{
		size_t needed = IMAGE_SVG_HEADER_SIZE - result->header_used;
		size_t take = length < needed ? length : needed;

		if (take != 0)
		{
			memcpy(result->header + result->header_used, bytes, take);
			bytes += take;
			length -= take;
			result->header_used += take;
		}
		if (result->header_used < IMAGE_SVG_HEADER_SIZE)
		{
			return true;
		}
		unsigned width = image_svg_read_word(result->header + 8);
		unsigned height = image_svg_read_word(result->header + 12);
		unsigned char expected[IMAGE_SVG_HEADER_SIZE];
		size_t count;

		image_svg_header(expected, width, height);
		if (memcmp(result->header, expected, IMAGE_SVG_HEADER_SIZE) != 0 ||
		    !FImageSizeIsSafe(width, height, IMAGE_SVG_MAX_DIMENSION,
		    IMAGE_SVG_MAX_PIXELS, &count))
		{
			errno = EINVAL;
			return false;
		}
		result->width = (int)width;
		result->height = (int)height;
		result->pixel_bytes = count * sizeof(uint32_t);
		result->pixels = malloc(result->pixel_bytes);
		if (result->pixels == NULL)
		{
			return false;
		}
	}
	if (result->received > result->pixel_bytes ||
	    length > result->pixel_bytes - result->received)
	{
		errno = EINVAL;
		return false;
	}
	if (length != 0)
	{
		memcpy((unsigned char *)result->pixels + result->received,
		    bytes, length);
		result->received += length;
	}
	return true;
}

bool
image_svg_result_complete(const image_svg_result *result)
{
	return result != NULL && result->header_used == IMAGE_SVG_HEADER_SIZE &&
	    result->pixels != NULL && result->received == result->pixel_bytes;
}

