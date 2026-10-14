/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include "config.h"
#include "libs/image_effect_protocol.h"

#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

static unsigned allocations;
static bool fail_allocate;
void *__real_malloc(size_t size);

void *__wrap_malloc(size_t size)
{
	allocations++;
	if (fail_allocate)
	{
		errno = ENOMEM;
		return NULL;
	}
	return __real_malloc(size);
}

static void test_request_contract(void)
{
	image_effect_request request = {.width = 32768, .height = 2048,
	    .pixmap = 1, .mask = 2, .alpha = 3, .visual = 4, .depth = 24,
	    .alpha_depth = 8, .colormap = 5, .background = 0,
	    .tint = UINT32_MAX, .tint_percent = 100, .alpha_percent = 0};
	unsigned char bytes[IMAGE_EFFECT_REQUEST_SIZE];
	image_effect_request decoded;

	assert(image_effect_request_encode(bytes, &request));
	assert(image_effect_request_decode(bytes, sizeof(bytes), &decoded));
	assert(memcmp(&decoded, &request, sizeof(request)) == 0);
	for (unsigned i = 0; i < 8; i++)
	{
		bytes[i] ^= 0x80;
		assert(!image_effect_request_decode(bytes, sizeof(bytes),
		    &decoded));
		assert(decoded.width == 0 && decoded.pixmap == 0);
		bytes[i] ^= 0x80;
	}
	assert(!image_effect_request_decode(bytes, sizeof(bytes) - 1, &decoded));
	assert(!image_effect_request_decode(bytes, sizeof(bytes) + 1, &decoded));
	request.height++;
	assert(!image_effect_request_encode(bytes, &request));
	request.height--;
	request.alpha_depth = 4;
	assert(!image_effect_request_encode(bytes, &request));
	request.alpha = 0;
	assert(image_effect_request_encode(bytes, &request));
	request.tint_percent = 101;
	assert(!image_effect_request_encode(bytes, &request));
}

static void test_split_output(void)
{
	image_effect_pixel expected[] = {{65535, 32767, 1, IMAGE_EFFECT_RGB},
	    {0, 0, 0, IMAGE_EFFECT_BACKGROUND}};
	unsigned char message[IMAGE_EFFECT_HEADER_SIZE + sizeof(expected)];

	image_effect_header(message, 2, 1);
	for (unsigned i = 0; i < 2; i++)
	{
		image_effect_record(message + IMAGE_EFFECT_HEADER_SIZE +
		    i * IMAGE_EFFECT_RECORD_SIZE, &expected[i]);
	}
	for (size_t split = 0; split <= sizeof(message); split++)
	{
		image_effect_result result = {.expected_width = 2,
		    .expected_height = 1};

		allocations = 0;
		assert(image_effect_result_consume(message, split, &result));
		assert(!image_effect_result_complete(&result) ||
		    split == sizeof(message));
		assert(image_effect_result_consume(message + split,
		    sizeof(message) - split, &result));
		assert(image_effect_result_complete(&result));
		assert(allocations == 1 && result.count == 2);
		assert(memcmp(result.pixels, expected, sizeof(expected)) == 0);
		assert(!image_effect_result_consume(message, 1, &result));
		assert(!image_effect_result_complete(&result));
		image_effect_result_destroy(&result);
		assert(result.pixels == NULL && !result.failed);
	}
	image_effect_result result = {0};

	for (size_t i = 0; i < sizeof(message); i++)
	{
		assert(image_effect_result_consume(message + i, 1, &result));
	}
	assert(image_effect_result_complete(&result));
	image_effect_result_destroy(&result);
}

static void test_refusal_and_partial(void)
{
	unsigned char header[IMAGE_EFFECT_HEADER_SIZE];
	const unsigned dimensions[][2] = {{0, 1}, {1, 0}, {32769, 1},
	    {1, 32769}, {32768, 2049}, {UINT32_MAX, UINT32_MAX}};

	for (unsigned i = 0; i < sizeof(dimensions) / sizeof(dimensions[0]); i++)
	{
		image_effect_result result = {0};

		image_effect_header(header, dimensions[i][0], dimensions[i][1]);
		allocations = 0;
		assert(!image_effect_result_consume(header, sizeof(header), &result));
		assert(allocations == 0 && result.failed && result.pixels == NULL);
		assert(!image_effect_result_consume(NULL, 0, &result));
		image_effect_result_destroy(&result);
	}
	image_effect_header(header, 2, 1);
	image_effect_result result = {.expected_width = 1, .expected_height = 1};

	allocations = 0;
	assert(!image_effect_result_consume(header, sizeof(header), &result));
	assert(allocations == 0);
	image_effect_result_destroy(&result);
	fail_allocate = true;
	assert(!image_effect_result_consume(header, sizeof(header), &result));
	assert(errno == ENOMEM);
	fail_allocate = false;
	image_effect_result_destroy(&result);
	assert(image_effect_result_consume(header, sizeof(header), &result));
	assert(!image_effect_result_complete(&result));
	unsigned char record[IMAGE_EFFECT_RECORD_SIZE] = {0};

	assert(image_effect_result_consume(record, 7, &result));
	assert(result.received == 0 && result.partial_used == 7);
	record[0] = 2; /* Invalid kind in the last byte of the partial record. */
	assert(!image_effect_result_consume(record, 1, &result));
	image_effect_result_destroy(&result);
	assert(image_effect_result_consume(header, sizeof(header), &result));
	record[0] = 1; /* Background record must not carry unused RGB. */
	assert(!image_effect_result_consume(record, sizeof(record), &result));
	image_effect_result_destroy(&result);
	assert(!image_effect_result_consume(NULL, 1, &result));
	image_effect_result_destroy(&result);
	image_effect_result_destroy(NULL);
}

int main(void)
{
	test_request_contract();
	test_split_output();
	test_refusal_and_partial();
	return 0;
}
