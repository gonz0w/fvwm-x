/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include "config.h"
#include "libs/picture_average.h"

#include <assert.h>
#include <stddef.h>
#include <string.h>

static void test_transparency_and_fractional_mean(void)
{
	picture_average average = {0};
	uint16_t rgb[3] = {1, 2, 3};

	assert(!picture_average_finish(&average, rgb));
	assert(rgb[0] == 0 && rgb[1] == 0 && rgb[2] == 0);
	assert(picture_average_add(&average, 0x00ffffff));
	assert(average.count == 0);
	assert(picture_average_add(&average, 0xff000000));
	assert(picture_average_add(&average, 0x01ffffff));
	assert(picture_average_finish(&average, rgb));
	assert(rgb[0] == 32767 && rgb[1] == 32767 && rgb[2] == 32767);
	/* Partial alpha contributes one straight-RGB sample, not its
	 * premultiplied color or an alpha-weighted fraction of a sample. */
	average = (picture_average){0};
	assert(picture_average_add(&average, 0x8080c040));
	assert(picture_average_finish(&average, rgb));
	assert(rgb[0] == 32896 && rgb[1] == 49344 && rgb[2] == 16448);
}

static void test_bounds_and_invalid_state(void)
{
	uint64_t limit = UINT64_MAX / UINT16_MAX;
	picture_average average = {255 * (limit - 1), 255 * (limit - 1),
	    255 * (limit - 1), limit - 1};
	uint16_t rgb[3];

	assert(picture_average_add(&average, 0xffffffff));
	assert(picture_average_finish(&average, rgb));
	assert(rgb[0] == UINT16_MAX && rgb[1] == UINT16_MAX &&
	    rgb[2] == UINT16_MAX);
	picture_average before = average;

	assert(!picture_average_add(&average, 0xff000000));
	assert(memcmp(&average, &before, sizeof(average)) == 0);
	average.count = UINT64_MAX;
	assert(!picture_average_finish(&average, rgb));
	assert(rgb[0] == 0 && rgb[1] == 0 && rgb[2] == 0);
	average = (picture_average){256, 0, 0, 1};
	before = average;
	assert(!picture_average_add(&average, 0xff000000));
	assert(memcmp(&average, &before, sizeof(average)) == 0);
	assert(!picture_average_finish(&average, rgb));
	assert(!picture_average_add(NULL, 0));
	assert(!picture_average_finish(NULL, rgb));
	assert(!picture_average_finish(&before, NULL));
}

int main(void)
{
	test_transparency_and_fractional_mean();
	test_bounds_and_invalid_state();
	return 0;
}
