/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include "config.h"
#include "image_effect_protocol.h"
#include "ImageSafety.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

_Static_assert(sizeof(image_effect_pixel) == IMAGE_EFFECT_RECORD_SIZE,
	"Effect output bound must include the in-memory record size");

static void effect_word(unsigned char *bytes, uint32_t value)
{
	for (unsigned i = 0; i < 4; i++)
	{
		bytes[i] = value >> (i * 8);
	}
}

static uint32_t effect_read_word(const unsigned char *bytes)
{
	return (uint32_t)bytes[0] | (uint32_t)bytes[1] << 8 |
	    (uint32_t)bytes[2] << 16 | (uint32_t)bytes[3] << 24;
}

static bool effect_request_valid(const image_effect_request *request)
{
	size_t count;

	return request != NULL && request->pixmap != 0 &&
	    request->visual != 0 && request->colormap != 0 &&
	    request->depth > 1 && request->depth <= 32 &&
	    (request->alpha == 0 ||
	    (request->alpha_depth >= 8 && request->alpha_depth <= 32)) &&
	    request->tint_percent <= 100 && request->alpha_percent <= 100 &&
	    FImageSizeIsSafe(request->width, request->height,
	    IMAGE_SVG_MAX_DIMENSION, IMAGE_SVG_MAX_PIXELS, &count);
}

bool image_effect_request_encode(unsigned char bytes[IMAGE_EFFECT_REQUEST_SIZE],
	const image_effect_request *request)
{
	if (bytes == NULL || !effect_request_valid(request))
	{
		errno = EINVAL;
		return false;
	}
	uint32_t words[] = {1, request->width, request->height, request->pixmap,
	    request->mask, request->alpha, request->visual, request->depth,
	    request->alpha_depth, request->colormap, request->background,
	    request->tint, request->tint_percent, request->alpha_percent};

	memcpy(bytes, "FVER", 4);
	for (unsigned i = 0; i < sizeof(words) / sizeof(words[0]); i++)
	{
		effect_word(bytes + 4 + i * 4, words[i]);
	}
	return true;
}

bool image_effect_request_decode(const unsigned char *bytes, size_t length,
	image_effect_request *request)
{
	if (request == NULL)
	{
		errno = EINVAL;
		return false;
	}
	*request = (image_effect_request){0};
	if (bytes == NULL || length != IMAGE_EFFECT_REQUEST_SIZE ||
	    memcmp(bytes, "FVER", 4) != 0 || effect_read_word(bytes + 4) != 1)
	{
		errno = EINVAL;
		return false;
	}
	image_effect_request parsed = {
	    effect_read_word(bytes + 8), effect_read_word(bytes + 12),
	    effect_read_word(bytes + 16), effect_read_word(bytes + 20),
	    effect_read_word(bytes + 24), effect_read_word(bytes + 28),
	    effect_read_word(bytes + 32), effect_read_word(bytes + 36),
	    effect_read_word(bytes + 40), effect_read_word(bytes + 44),
	    effect_read_word(bytes + 48), effect_read_word(bytes + 52),
	    effect_read_word(bytes + 56)};

	if (!effect_request_valid(&parsed))
	{
		errno = EINVAL;
		return false;
	}
	*request = parsed;
	return true;
}

void image_effect_header(unsigned char bytes[IMAGE_EFFECT_HEADER_SIZE],
	unsigned width, unsigned height)
{
	memcpy(bytes, "FEFX", 4);
	effect_word(bytes + 4, 1);
	effect_word(bytes + 8, width);
	effect_word(bytes + 12, height);
}

void image_effect_record(unsigned char bytes[IMAGE_EFFECT_RECORD_SIZE],
	const image_effect_pixel *pixel)
{
	uint16_t words[] = {pixel->red, pixel->green, pixel->blue, pixel->kind};

	for (unsigned i = 0; i < 4; i++)
	{
		bytes[i * 2] = words[i];
		bytes[i * 2 + 1] = words[i] >> 8;
	}
}

static bool effect_refuse(image_effect_result *result)
{
	if (result != NULL)
	{
		result->failed = true;
	}
	errno = EPROTO;
	return false;
}

bool image_effect_result_consume(const unsigned char *bytes, size_t length,
	void *context)
{
	image_effect_result *result = context;

	if (result == NULL || result->failed || (length != 0 && bytes == NULL))
	{
		return effect_refuse(result);
	}
	if (result->header_used < IMAGE_EFFECT_HEADER_SIZE)
	{
		size_t needed = IMAGE_EFFECT_HEADER_SIZE - result->header_used;
		size_t take = length < needed ? length : needed;

		if (take != 0)
		{
			memcpy(result->header + result->header_used, bytes, take);
			bytes += take;
			length -= take;
			result->header_used += take;
		}
		if (result->header_used < IMAGE_EFFECT_HEADER_SIZE)
		{
			return true;
		}
		unsigned width = effect_read_word(result->header + 8);
		unsigned height = effect_read_word(result->header + 12);
		unsigned char expected[IMAGE_EFFECT_HEADER_SIZE];
		size_t count;

		image_effect_header(expected, width, height);
		if (memcmp(result->header, expected, sizeof(expected)) != 0 ||
		    (result->expected_width && result->expected_width != width) ||
		    (result->expected_height && result->expected_height != height) ||
		    !FImageSizeIsSafe(width, height, IMAGE_SVG_MAX_DIMENSION,
		    IMAGE_SVG_MAX_PIXELS, &count) ||
		    count > SIZE_MAX / sizeof(*result->pixels))
		{
			return effect_refuse(result);
		}
		result->pixels = malloc(count * sizeof(*result->pixels));
		if (result->pixels == NULL)
		{
			result->failed = true;
			return false;
		}
		result->width = width;
		result->height = height;
		result->count = count;
	}
	size_t room = (result->count - result->received) *
	    IMAGE_EFFECT_RECORD_SIZE - result->partial_used;

	if (length > room)
	{
		return effect_refuse(result);
	}
	while (length != 0)
	{
		size_t take = IMAGE_EFFECT_RECORD_SIZE - result->partial_used;

		if (take > length)
		{
			take = length;
		}
		memcpy(result->partial + result->partial_used, bytes, take);
		result->partial_used += take;
		bytes += take;
		length -= take;
		if (result->partial_used == IMAGE_EFFECT_RECORD_SIZE)
		{
			uint16_t words[4];

			for (unsigned i = 0; i < 4; i++)
			{
				words[i] = result->partial[i * 2] |
				    (uint16_t)result->partial[i * 2 + 1] << 8;
			}
			if (words[3] > IMAGE_EFFECT_RGB ||
			    (words[3] == IMAGE_EFFECT_BACKGROUND &&
			    (words[0] || words[1] || words[2])))
			{
				return effect_refuse(result);
			}
			result->pixels[result->received++] = (image_effect_pixel){
			    words[0], words[1], words[2], words[3]};
			result->partial_used = 0;
		}
	}
	return true;
}

bool image_effect_result_complete(const image_effect_result *result)
{
	return result != NULL && !result->failed && result->pixels != NULL &&
	    result->header_used == IMAGE_EFFECT_HEADER_SIZE &&
	    result->received == result->count && result->partial_used == 0;
}

void image_effect_result_destroy(image_effect_result *result)
{
	if (result != NULL)
	{
		free(result->pixels);
		memset(result, 0, sizeof(*result));
	}
}
