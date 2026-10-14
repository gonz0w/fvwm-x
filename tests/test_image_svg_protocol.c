/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include "config.h"
#include "libs/image_svg_protocol.h"

#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

static unsigned allocations;
static bool fail_allocate;
void *__real_malloc(size_t size);

void *
__wrap_malloc(size_t size)
{
	allocations++;
	if (fail_allocate)
	{
		errno = ENOMEM;
		return NULL;
	}
	return __real_malloc(size);
}

static void
test_split_result(void)
{
	unsigned char message[IMAGE_SVG_HEADER_SIZE + 16];
	uint32_t expected[] = {0xffff0000U, 0x7f00ff00U, 0xff0000ffU, 0};

	image_svg_header(message, 2, 2);
	memcpy(message + IMAGE_SVG_HEADER_SIZE, expected, sizeof(expected));
	for (size_t split = 0; split <= sizeof(message); split++)
	{
		image_svg_result result = {0};

		allocations = 0;
		assert(image_svg_result_consume(message, split, &result));
		assert(allocations == (split >= IMAGE_SVG_HEADER_SIZE));
		assert(image_svg_result_consume(message + split,
		    sizeof(message) - split, &result));
		assert(image_svg_result_complete(&result));
		assert(result.width == 2 && result.height == 2);
		assert(memcmp(result.pixels, expected, sizeof(expected)) == 0);
		assert(allocations == 1);
		assert(!image_svg_result_consume(message, 1, &result));
		image_svg_result_destroy(&result);
		assert(result.pixels == NULL && result.header_used == 0);
	}
}

static void
test_refusal_before_allocation(void)
{
	unsigned char header[IMAGE_SVG_HEADER_SIZE];
	const unsigned dimensions[][2] = {
		{0, 1}, {1, 0}, {32769, 1}, {1, 32769},
		{32768, 2049}, {0xffffffffU, 0xffffffffU},
	};

	for (size_t i = 0; i < sizeof(dimensions) / sizeof(dimensions[0]); i++)
	{
		image_svg_result result = {0};

		image_svg_header(header, dimensions[i][0], dimensions[i][1]);
		allocations = 0;
		assert(!image_svg_result_consume(header, sizeof(header), &result));
		assert(allocations == 0 && result.pixels == NULL);
		image_svg_result_destroy(&result);
	}
	for (unsigned field = 0; field < 8; field++)
	{
		image_svg_result result = {0};

		image_svg_header(header, 2, 2);
		header[field] ^= 0x80;
		allocations = 0;
		assert(!image_svg_result_consume(header, sizeof(header), &result));
		assert(allocations == 0 && result.pixels == NULL);
		image_svg_result_destroy(&result);
	}
}

static void
test_partial_and_failed_results(void)
{
	unsigned char header[IMAGE_SVG_HEADER_SIZE];
	image_svg_result result = {0};

	image_svg_header(header, 2, 2);
	assert(!image_svg_result_complete(&result));
	assert(image_svg_result_consume(header, sizeof(header) - 1, &result));
	assert(!image_svg_result_complete(&result) && result.pixels == NULL);
	image_svg_result_destroy(&result);
	fail_allocate = true;
	assert(!image_svg_result_consume(header, sizeof(header), &result));
	assert(errno == ENOMEM && result.pixels == NULL);
	fail_allocate = false;
	image_svg_result_destroy(&result);
	assert(image_svg_result_consume(header, sizeof(header), &result));
	assert(!image_svg_result_complete(&result));
	assert(image_svg_result_consume(header, 15, &result));
	assert(!image_svg_result_complete(&result));
	image_svg_result_destroy(&result);
	assert(!image_svg_result_consume(NULL, 1, &result));
	assert(!image_svg_result_consume(header, 1, NULL));
	image_svg_result_destroy(NULL);
}

int
main(void)
{
	test_split_result();
	test_refusal_before_allocation();
	test_partial_and_failed_results();
	return 0;
}
