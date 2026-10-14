/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "config.h"
#include "libs/FEvent.h"
#include <assert.h>
#include <limits.h>

static void test_coordinate_bounds(void)
{
	const int input[] = {INT_MIN, -32769, -32768, -42, 0, 123,
	    32767, 32768, INT_MAX};
	const int expected[] = {-32768, -32768, -32768, -42, 0, 123,
	    32767, 32767, 32767};

	for (size_t i = 0; i < sizeof(input) / sizeof(input[0]); i++)
	{
		XSizeHints hints = {.x = input[i], .y = input[i]};
		fev_sanitize_size_hints(&hints);
		assert(hints.x == expected[i] && hints.y == expected[i]);
	}
}

int main(void)
{
	test_coordinate_bounds();
	return 0;
}
