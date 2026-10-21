/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or any later version.
 */
#include "config.h"

#include <assert.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include "fvwm/ewmh_desktop_policy.h"

int main(void)
{
	unsigned count = 99;

	assert(fvwm_ewmh_desktop_count(0, 0, -1, 0, 65535, &count));
	assert(count == 1);
	assert(fvwm_ewmh_desktop_count(2, 300, 400, 0, 65535, &count));
	assert(count == 401);
	assert(fvwm_ewmh_desktop_count(2, 300, 400, 10, 65535, &count));
	assert(count == 2);
	assert(!fvwm_ewmh_desktop_count(UINT_MAX, 0, -1, 0, 65535, &count));
	assert(count == 0);
	assert(!fvwm_ewmh_desktop_count(1, INT_MAX, -1, 0, 65535, &count));
	assert(!fvwm_ewmh_desktop_count(1, 0, INT_MAX, 0, 65535, &count));
	assert(!fvwm_ewmh_desktop_count(1, 0, -1, 0, 5, &count));
	assert(!fvwm_ewmh_desktop_count(1, 0, -1, 0, 9, &count));
	assert(fvwm_ewmh_desktop_count(1, 0, -1, 0, 10, &count));
	assert(count == 1);
	assert(fvwm_ewmh_desktop_count(1024, 0, -1, 0, 65535, &count));
	assert(count == 1024);
	assert(!fvwm_ewmh_desktop_count(1025, 0, -1, 0, 65535, &count));
	assert(!fvwm_ewmh_desktop_count(1, 0, -1, 0, 65535, NULL));
	for (unsigned words = 6; words < 64; words++)
	{
		unsigned capacity = (words - 6) / 4;

		assert(fvwm_ewmh_desktop_count(capacity + 1, 0, -1, 0,
			words, &count) == false);
		assert(count == 0);
		if (capacity)
		{
			assert(fvwm_ewmh_desktop_count(capacity, 0, -1, 0,
				words, &count));
			assert(count == capacity);
		}
	}
	assert(fvwm_ewmh_desktop_request(3, 0, 2, 10, 65535, &count));
	assert(count == 3);
	assert(!fvwm_ewmh_desktop_request(2, 2, -1, 10, 65535, &count));
	assert(count == 0);
	assert(!fvwm_ewmh_desktop_request(2, 0, 2, 10, 65535, &count));
	assert(!fvwm_ewmh_desktop_request(2, 0, 20, 10, 65535, &count));
	assert(!fvwm_ewmh_desktop_request(11, 0, -1, 10, 65535, &count));
	assert(!fvwm_ewmh_desktop_request(0, 0, -1, 0, 65535, &count));
	assert(!fvwm_ewmh_desktop_request(-1, 0, -1, 0, 65535, &count));
	assert(!fvwm_ewmh_desktop_request(UINT32_MAX, 0, -1, 0,
		65535, &count));
	assert(!fvwm_ewmh_desktop_request(1025, 0, -1, 0, 65535, &count));
	assert(!fvwm_ewmh_desktop_request(1, 0, -1, 0, 65535, NULL));
	assert(fvwm_ewmh_desktop_index(0, 1));
	assert(!fvwm_ewmh_desktop_index(-1, 1));
	assert(!fvwm_ewmh_desktop_index(1, 1));
	assert(!fvwm_ewmh_desktop_index(UINT32_MAX, 1024));
	assert(!fvwm_ewmh_desktop_index(0, 0));
	puts("Bounded desktop publication and request policy passed");
	return 0;
}
